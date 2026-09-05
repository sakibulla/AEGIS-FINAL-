#include "voice_activation.h"

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_afe_sr_models.h"
#include "model_path.h"

static const char *TAG = "VOICE";

/* ============================================================
 * ESP32-S3-EYE Microphone Pins
 * ============================================================ */
#define I2S_BCLK   GPIO_NUM_41
#define I2S_WS     GPIO_NUM_42
#define I2S_DIN    GPIO_NUM_2

#define SAMPLE_RATE 16000
#define WAKE_WORD_BIT BIT0

static EventGroupHandle_t s_event_group = NULL;
static const esp_afe_sr_iface_t *afe_handle = NULL;
static esp_afe_sr_data_t *afe_data = NULL;
static i2s_chan_handle_t rx_handle = NULL;
static volatile int g_task_flag = 0;

/* ============================================================
 * I2S Init
 * ============================================================ */
static esp_err_t init_i2s(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK,
            .ws   = I2S_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = I2S_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    // Important for many MEMS mics on ESP32-S3-EYE
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

    ESP_LOGI(TAG, "Microphone ready (GPIO 41/42/2)");
    return ESP_OK;
}

/* ============================================================
 * Feed Task
 * ============================================================ */
static void feed_task(void *arg)
{
    esp_afe_sr_data_t *data = (esp_afe_sr_data_t *)arg;

    int chunksize = afe_handle->get_feed_chunksize(data);
    int nch = afe_handle->get_feed_channel_num(data);

    size_t bytes_needed = chunksize * nch * sizeof(int16_t);
    int16_t *buf = (int16_t *)malloc(bytes_needed);

    if (!buf) {
        ESP_LOGE(TAG, "Feed buffer allocation failed");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Voice feed task started");

    while (g_task_flag) {
        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(rx_handle, buf, bytes_needed, &bytes_read, portMAX_DELAY);

        if (err == ESP_OK && bytes_read == bytes_needed) {
            afe_handle->feed(data, buf);
        }
    }

    free(buf);
    vTaskDelete(NULL);
}

/* ============================================================
 * Detect Task
 * ============================================================ */
static void detect_task(void *arg)
{
    esp_afe_sr_data_t *data = (esp_afe_sr_data_t *)arg;

    // More sensitive threshold
    afe_handle->set_wakenet_threshold(data, 1, 0.40f);
    afe_handle->set_wakenet_threshold(data, 2, 0.40f);

    ESP_LOGI(TAG, "Listening for: Hi ESP");

    while (g_task_flag) {
        afe_fetch_result_t *res = afe_handle->fetch(data);

        if (!res || res->ret_value == ESP_FAIL) {
            continue;
        }

        if (res->wakeup_state == WAKENET_DETECTED) {
            ESP_LOGI(TAG, ">>> HI ESP DETECTED <<<");
            if (s_event_group) {
                xEventGroupSetBits(s_event_group, WAKE_WORD_BIT);
            }
        }
    }

    vTaskDelete(NULL);
}

/* ============================================================
 * Public API
 * ============================================================ */
extern "C" {

esp_err_t voice_activation_init(void)
{
    if (s_event_group) {
        return ESP_OK;
    }

    s_event_group = xEventGroupCreate();
    if (!s_event_group) {
        return ESP_FAIL;
    }

    if (init_i2s() != ESP_OK) {
        return ESP_FAIL;
    }

    srmodel_list_t *models = esp_srmodel_init("model");
    if (!models) {
        ESP_LOGE(TAG, "Model load failed");
        return ESP_FAIL;
    }

    afe_config_t *cfg = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    if (!cfg) {
        ESP_LOGE(TAG, "AFE config failed");
        return ESP_FAIL;
    }

    cfg->pcm_config.sample_rate = SAMPLE_RATE;
    cfg->wakenet_init = true;
    cfg->vad_init = false;
    cfg->aec_init = false;
    cfg->se_init = false;

    afe_handle = esp_afe_handle_from_config(cfg);
    afe_data   = afe_handle->create_from_config(cfg);
    afe_config_free(cfg);

    if (!afe_handle || !afe_data) {
        ESP_LOGE(TAG, "AFE creation failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Voice system ready");
    return ESP_OK;
}

esp_err_t voice_activation_start(void)
{
    if (!afe_handle || !afe_data) {
        return ESP_FAIL;
    }

    g_task_flag = 1;

    xTaskCreatePinnedToCore(feed_task,   "voice_feed",   8192, afe_data, 5, NULL, 0);
    xTaskCreatePinnedToCore(detect_task, "voice_detect", 4096, afe_data, 5, NULL, 1);

    ESP_LOGI(TAG, "Voice activation started");
    return ESP_OK;
}

bool voice_activation_wait(uint32_t timeout_ms)
{
    if (!s_event_group) return false;

    TickType_t ticks = (timeout_ms == portMAX_DELAY) ?
                       portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

    EventBits_t bits = xEventGroupWaitBits(
        s_event_group,
        WAKE_WORD_BIT,
        pdTRUE,
        pdFALSE,
        ticks
    );

    return (bits & WAKE_WORD_BIT) != 0;
}

void voice_activation_stop(void)
{
    g_task_flag = 0;
    if (rx_handle) {
        i2s_channel_disable(rx_handle);
    }
}

} // extern "C"