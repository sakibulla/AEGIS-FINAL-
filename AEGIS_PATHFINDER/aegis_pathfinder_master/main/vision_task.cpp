#include "vision_task.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

// ── ESP32-S3 SIMD / ESP-NN ACCELERATION MACROS ────────────────────────────
#define EI_CLASSIFIER_TFLITE_ENABLE_ESP_NN 1
#define EI_CLASSIFIER_TFLITE_ENABLE_ESP_NN_S3 1
#define EI_CLASSIFIER_USE_ESP_NN 1
#define EI_CLASSIFIER_USE_FULL_TFLITE 0

#ifndef CONFIG_IDF_TARGET_ESP32S3
#define CONFIG_IDF_TARGET_ESP32S3
#endif

#include "ei_run_classifier.h"

static const char *TAG = "AI_VISION_FAST";

#define CAM_WIDTH           320
#define CAM_HEIGHT          240
#define DETECTION_THRESHOLD 0.30f
#define BUF_ALIGN           16

static detection_t        s_detections[MAX_DETECTIONS];
static int                s_detection_count = 0;
static int                s_last_inference_ms = 0;
static bool               s_door_detected = false;
static SemaphoreHandle_t  s_det_mutex = NULL;
static SemaphoreHandle_t  s_cam_mutex = NULL;

static uint8_t  *s_model_rgb_buffer = NULL; // PSRAM 16B-aligned input tensor buffer
static uint16_t *s_src_x_lut = NULL;
static uint16_t *s_src_y_lut = NULL;

// ── THREAD-SAFE CAMERA ACCESSORS ──────────────────────────────────────────
camera_fb_t* safe_camera_fb_get(void) {
    if (!s_cam_mutex) return NULL;
    if (xSemaphoreTake(s_cam_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            xSemaphoreGive(s_cam_mutex);
            return NULL;
        }
        return fb; // Hold mutex until safe_camera_fb_return is called
    }
    return NULL;
}

void safe_camera_fb_return(camera_fb_t *fb) {
    if (!fb) return;
    esp_camera_fb_return(fb);
    if (s_cam_mutex) {
        xSemaphoreGive(s_cam_mutex);
    }
}

// ── EDGE IMPULSE PACKED SIGNAL CALLBACK ────────────────────────────────────
static int get_model_tensor_data(size_t offset, size_t length, float *out_ptr) {
    if (!s_model_rgb_buffer) return -1;

    size_t pixel_ix = offset * 3;
    for (size_t i = 0; i < length; i++) {
        out_ptr[i] = (float)(
            ((uint32_t)s_model_rgb_buffer[pixel_ix]     << 16) |
            ((uint32_t)s_model_rgb_buffer[pixel_ix + 1] <<  8) |
             (uint32_t)s_model_rgb_buffer[pixel_ix + 2]
        );
        pixel_ix += 3;
    }
    return 0;
}

esp_err_t init_hardware_camera(void) {
    if (!s_cam_mutex) {
        s_cam_mutex = xSemaphoreCreateMutex();
    }

    camera_config_t config = {
        .pin_pwdn  = -1, .pin_reset = -1,
        .pin_xclk  = 15, .pin_sccb_sda = 4, .pin_sccb_scl = 5,
        .pin_d7 = 16, .pin_d6 = 17, .pin_d5 = 18, .pin_d4 = 12,
        .pin_d3 = 10, .pin_d2 = 8,  .pin_d1 = 9,  .pin_d0 = 11,
        .pin_vsync = 6,  .pin_href = 7,  .pin_pclk = 13,
        .xclk_freq_hz = 10000000,         // Reduced to 10MHz for DMA clock stability
        .ledc_timer   = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_RGB565, // Direct RGB565 DMA
        .frame_size   = FRAMESIZE_QVGA,   // 320x240
        .fb_count     = 2,
        .fb_location  = CAMERA_FB_IN_PSRAM,
        .grab_mode    = CAMERA_GRAB_LATEST,
    };
    return esp_camera_init(&config);
}

// ── ENDIAN-AWARE RGB565 UNPACKING ──────────────────────────────────────────
static inline void unpack_rgb565(uint16_t px_raw, uint8_t *out) {
    // OV2640 outputs Big-Endian RGB565 over DMA. Swap bytes for Xtensa Little-Endian CPU
    uint16_t px = __builtin_bswap16(px_raw);

    uint8_t r5 = (px >> 11) & 0x1F;
    uint8_t g6 = (px >> 5)  & 0x3F;
    uint8_t b5 =  px        & 0x1F;

    out[0] = (r5 << 3) | (r5 >> 2); // R
    out[1] = (g6 << 2) | (g6 >> 4); // G
    out[2] = (b5 << 3) | (b5 >> 2); // B
}

// ── SINGLE-PASS DOWNSCALE & COLOR CONVERT DIRECTLY INTO MODEL BUFFER ───────
static void downscale_rgb565_to_model_rgb888(const uint16_t *src, uint8_t *dst) {
    const int dst_w = EI_CLASSIFIER_INPUT_WIDTH;
    const int dst_h = EI_CLASSIFIER_INPUT_HEIGHT;

    for (int y = 0; y < dst_h; y++) {
        uint16_t sy = s_src_y_lut[y];
        const uint16_t *src_row = src + (size_t)sy * CAM_WIDTH;
        uint8_t *dst_row = dst + (size_t)y * dst_w * 3;

        for (int x = 0; x < dst_w; x++) {
            uint16_t sx = s_src_x_lut[x];
            unpack_rgb565(src_row[sx], &dst_row[x * 3]);
        }
    }
}

static void build_downscale_luts(void) {
    const int dst_w = EI_CLASSIFIER_INPUT_WIDTH;
    const int dst_h = EI_CLASSIFIER_INPUT_HEIGHT;

    s_src_x_lut = (uint16_t *)malloc(sizeof(uint16_t) * dst_w);
    s_src_y_lut = (uint16_t *)malloc(sizeof(uint16_t) * dst_h);

    for (int x = 0; x < dst_w; x++) {
        s_src_x_lut[x] = (uint16_t)((x * CAM_WIDTH) / dst_w);
    }
    for (int y = 0; y < dst_h; y++) {
        s_src_y_lut[y] = (uint16_t)((y * CAM_HEIGHT) / dst_h);
    }
}

static void inference_worker_task(void *pvParameters) {
    size_t model_rgb_size = (size_t)EI_CLASSIFIER_INPUT_WIDTH * (size_t)EI_CLASSIFIER_INPUT_HEIGHT * 3;

    // 16-byte aligned allocation in PSRAM
    s_model_rgb_buffer = (uint8_t *)heap_caps_aligned_alloc(
        BUF_ALIGN, model_rgb_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    build_downscale_luts();

    if (!s_model_rgb_buffer || !s_src_x_lut || !s_src_y_lut) {
        ESP_LOGE(TAG, "FATAL: Buffer/LUT Allocation Failed");
        vTaskDelete(NULL);
        return;
    }

    signal_t signal = {
        .get_data = &get_model_tensor_data,
        .total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE
    };

    // Camera Warmup
    for (int i = 0; i < 5; i++) {
        camera_fb_t *f = safe_camera_fb_get();
        if (f) safe_camera_fb_return(f);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Vision pipeline active (ESP-NN SIMD + Endian-Correct RGB565) on Core 1.");

    uint32_t frame_count = 0;

    while (1) {
        frame_count++;

        camera_fb_t *frame = safe_camera_fb_get();
        if (!frame) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        // Single pass 320x240 RGB565 -> 96x96 Model RGB888
        downscale_rgb565_to_model_rgb888((const uint16_t *)frame->buf, s_model_rgb_buffer);
        safe_camera_fb_return(frame);

        int64_t t0 = esp_timer_get_time();
        ei_impulse_result_t result = {0};
        EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
        int infer_ms = (int)((esp_timer_get_time() - t0) / 1000);

        if (err == EI_IMPULSE_OK) {
            ESP_LOGI(TAG, "[FRAME %lu] Inference complete in %d ms | Detections: %u",
                     (unsigned long)frame_count, infer_ms, (unsigned)result.bounding_boxes_count);

            if (xSemaphoreTake(s_det_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                s_detection_count   = 0;
                s_last_inference_ms = infer_ms;
                s_door_detected     = false;

                for (size_t ix = 0; ix < result.bounding_boxes_count && s_detection_count < MAX_DETECTIONS; ix++) {
                    auto &bb = result.bounding_boxes[ix];
                    if (bb.value < DETECTION_THRESHOLD) continue;

                    ESP_LOGI(TAG, "  └─ DETECTED: '%s' | Confidence: %.1f%% | BBox: [x:%u, y:%u, w:%u, h:%u]",
                             bb.label, bb.value * 100.0f, bb.x, bb.y, bb.width, bb.height);

                    detection_t *d = &s_detections[s_detection_count++];
                    strncpy(d->label, bb.label, sizeof(d->label) - 1);
                    d->label[sizeof(d->label) - 1] = '\0';
                    d->score = bb.value;
                    d->x = bb.x; d->y = bb.y; d->w = bb.width; d->h = bb.height;

                    if (strstr(bb.label, "door") != NULL) {
                        s_door_detected = true;
                    }
                }
                xSemaphoreGive(s_det_mutex);
            }
        } else {
            ESP_LOGE(TAG, "[FRAME %lu] Classifier error: %d", (unsigned long)frame_count, (int)err);
        }

        // Pacing delay to prevent Core 1 bus starvation
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void start_vision_task(void) {
    if (!s_det_mutex) {
        s_det_mutex = xSemaphoreCreateMutex();
    }
    esp_err_t err = init_hardware_camera();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Initialization Failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "OV2640 / S3 Camera Hardware successfully initialized!");
    }
    xTaskCreatePinnedToCore(inference_worker_task, "vision_core", 16384, NULL, 5, NULL, 1);
}

void get_current_detections(detection_t *out_dets, int *out_count, int *out_ms) {
    if (s_det_mutex && xSemaphoreTake(s_det_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        memcpy(out_dets, s_detections, sizeof(detection_t) * s_detection_count);
        *out_count = s_detection_count;
        *out_ms    = s_last_inference_ms;
        xSemaphoreGive(s_det_mutex);
    }
}

bool is_door_currently_detected(void) {
    bool detected = false;
    if (s_det_mutex && xSemaphoreTake(s_det_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        detected = s_door_detected;
        xSemaphoreGive(s_det_mutex);
    }
    return detected;
}