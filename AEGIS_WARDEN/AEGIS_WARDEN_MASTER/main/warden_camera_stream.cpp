#include "warden_camera_stream.h"
#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "img_converters.h"  // For frame2jpg RGB to JPEG conversion

static const char *TAG = "CAM_STREAM";

#define STREAM_PORT 81
#define PART_BOUNDARY "123456789000000000000987654321"

static httpd_handle_t stream_httpd = NULL;
static char stream_url[64] = {0};

// Global flag: true when someone is viewing the stream
static volatile bool stream_active = false;

// MJPEG stream content type
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

/* ============================================================
 * STREAM HANDLER
 * Serves MJPEG video stream
 * ============================================================ */

static esp_err_t stream_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    char part_buf[128];
    uint8_t *jpg_buf = NULL;
    size_t jpg_buf_len = 0;
    
    ESP_LOGI(TAG, "Stream requested from %s", req->uri);
    ESP_LOGI(TAG, "🎥 STREAM MODE: AI inference PAUSED");
    
    // Signal AI to stop
    stream_active = true;
    
    // Set response content type
    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set content type");
        stream_active = false;
        return res;
    }
    
    // Set headers
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "20");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    
    ESP_LOGI(TAG, "Stream started, camera available for streaming...");
    
    // Stream loop
    int frame_count = 0;
    while (true) {
        // Get frame
        fb = esp_camera_fb_get();
        
        if (fb == NULL) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        
        frame_count++;
        
        // Convert RGB565 to JPEG if needed
        if (fb->format != PIXFORMAT_JPEG) {
            bool jpeg_converted = frame2jpg(fb, 85, &jpg_buf, &jpg_buf_len);  // Quality 85 (faster)
            esp_camera_fb_return(fb);
            fb = NULL;
            
            if (!jpeg_converted) {
                ESP_LOGE(TAG, "JPEG compression failed");
                if (jpg_buf) {
                    free(jpg_buf);
                    jpg_buf = NULL;
                }
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }
            
            if (frame_count % 30 == 0) {
                ESP_LOGI(TAG, "Streaming frame %d, size: %d bytes (JPEG)", 
                         frame_count, jpg_buf_len);
            }
        } else {
            jpg_buf = fb->buf;
            jpg_buf_len = fb->len;
            
            if (frame_count % 30 == 0) {
                ESP_LOGI(TAG, "Streaming frame %d, size: %d bytes", 
                         frame_count, jpg_buf_len);
            }
        }
        
        // Send boundary
        res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        if (res != ESP_OK) {
            if (fb == NULL && jpg_buf) free(jpg_buf);
            else if (fb) esp_camera_fb_return(fb);
            break;
        }
        
        // Send JPEG header
        size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, jpg_buf_len);
        res = httpd_resp_send_chunk(req, part_buf, hlen);
        if (res != ESP_OK) {
            if (fb == NULL && jpg_buf) free(jpg_buf);
            else if (fb) esp_camera_fb_return(fb);
            break;
        }
        
        // Send JPEG data
        res = httpd_resp_send_chunk(req, (const char *)jpg_buf, jpg_buf_len);
        
        // Clean up current frame buffers
        if (fb == NULL && jpg_buf) {
            free(jpg_buf);
            jpg_buf = NULL;
        } else if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
        }
        
        if (res != ESP_OK) {
            break;
        }
        
        // Fast frame rate while streaming (~20 FPS)
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // Final cleanup (buffers already freed in loop, but check for safety)
    // Note: fb and jpg_buf are already NULL from loop cleanup
    if (fb) {
        esp_camera_fb_return(fb);
        fb = NULL;
    }
    if (jpg_buf) {
        free(jpg_buf);
        jpg_buf = NULL;
    }
    
    // Re-enable AI
    stream_active = false;
    
    ESP_LOGI(TAG, "Stream ended after %d frames", frame_count);
    ESP_LOGI(TAG, "🔥 AI MODE: Fire/smoke detection RESUMED");
    
    return res;
}

/* ============================================================
 * CAPTURE HANDLER
 * Serves single JPEG snapshot
 * ============================================================ */

static esp_err_t capture_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;
    int retry_count = 0;
    const int MAX_RETRIES = 60;
    
    ESP_LOGI(TAG, "Capture requested");
    
    // Signal AI to pause
    stream_active = true;
    
    // Wait a bit to ensure AI releases camera
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Try to get frame
    while (retry_count < MAX_RETRIES && fb == NULL) {
        fb = esp_camera_fb_get();
        if (!fb) {
            retry_count++;
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    
    if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed after %d retries", MAX_RETRIES);
        stream_active = false;
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Frame acquired: %u bytes, format: %d", fb->len, fb->format);
    
    // Convert to JPEG if needed
    if (fb->format != PIXFORMAT_JPEG) {
        ESP_LOGI(TAG, "Converting RGB565 to JPEG...");
        bool jpeg_converted = frame2jpg(fb, 80, &jpg_buf, &jpg_len);
        esp_camera_fb_return(fb);
        fb = NULL;
        
        if (!jpeg_converted || !jpg_buf) {
            ESP_LOGE(TAG, "JPEG conversion failed");
            stream_active = false;
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        
        ESP_LOGI(TAG, "JPEG conversion successful: %u bytes", jpg_len);
    } else {
        jpg_buf = fb->buf;
        jpg_len = fb->len;
    }
    
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    res = httpd_resp_send(req, (const char *)jpg_buf, jpg_len);
    
    // Cleanup
    if (fb == NULL && jpg_buf) {
        free(jpg_buf);
    } else if (fb) {
        esp_camera_fb_return(fb);
    }
    
    stream_active = false;
    
    if (res == ESP_OK) {
        ESP_LOGI(TAG, "Capture sent: %u bytes (JPEG)", jpg_len);
    }
    
    return res;
}

/* ============================================================
 * STATUS HANDLER
 * Returns camera status as JSON
 * ============================================================ */

static esp_err_t status_handler(httpd_req_t *req)
{
    sensor_t *s = esp_camera_sensor_get();
    
    char json_response[256];
    snprintf(json_response, sizeof(json_response),
        "{"
        "\"framesize\":\"%d\","
        "\"quality\":\"%d\","
        "\"brightness\":\"%d\","
        "\"contrast\":\"%d\","
        "\"saturation\":\"%d\","
        "\"stream_url\":\"%s\""
        "}",
        s->status.framesize,
        s->status.quality,
        s->status.brightness,
        s->status.contrast,
        s->status.saturation,
        stream_url
    );
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

/* ============================================================
 * ROOT HANDLER
 * Simple HTML page with stream embed
 * ============================================================ */

static esp_err_t root_handler(httpd_req_t *req)
{
    const char* html = 
        "<!DOCTYPE html>"
        "<html>"
        "<head><title>Warden Camera</title></head>"
        "<body>"
        "<h1>AEGIS Warden Camera Stream</h1>"
        "<img src=\"/stream\" style=\"max-width:100%;\">"
        "<br><br>"
        "<a href=\"/capture\">Capture Snapshot</a> | "
        "<a href=\"/status\">Status</a>"
        "</body>"
        "</html>";
    
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, strlen(html));
}

/* ============================================================
 * PUBLIC API
 * ============================================================ */

esp_err_t camera_stream_init(void)
{
    if (stream_httpd != NULL) {
        ESP_LOGW(TAG, "Stream server already running");
        return ESP_OK;
    }
    
    // Get IP address
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        snprintf(stream_url, sizeof(stream_url), 
            "http://" IPSTR ":%d/stream", 
            IP2STR(&ip_info.ip), STREAM_PORT);
    } else {
        snprintf(stream_url, sizeof(stream_url), 
            "http://[IP]:%d/stream", STREAM_PORT);
    }
    
    // Configure HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = STREAM_PORT;
    config.ctrl_port = STREAM_PORT + 1;
    config.max_open_sockets = 7;
    config.max_uri_handlers = 8;
    config.stack_size = 6144;  // Increased for better performance
    config.task_priority = 5;   // Higher priority
    config.core_id = 0;         // Run on core 0 (separate from AI)
    
    ESP_LOGI(TAG, "Starting camera stream server on port %d", STREAM_PORT);
    
    // Start HTTP server
    if (httpd_start(&stream_httpd, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start stream server");
        return ESP_FAIL;
    }
    
    // Register URI handlers
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(stream_httpd, &root_uri);
    
    // Live MJPEG /stream is intentionally NOT registered: stream_handler()
    // holds stream_active=true (pausing fire/smoke AI inference) for the
    // entire duration a viewer is connected, with no timeout — under real
    // memory/CPU pressure this bot can't afford to have detection paused
    // indefinitely just because someone opened the dashboard. Snapshot mode
    // (/capture, /snapshot below) grabs one frame and returns immediately,
    // so AI only pauses for the ~100ms of an actual capture, not open-ended.
    // The backend's video proxy already falls back to snapshot polling when
    // /stream isn't reachable, so this degrades cleanly on the dashboard side.

    httpd_uri_t capture_uri = {
        .uri = "/capture",
        .method = HTTP_GET,
        .handler = capture_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(stream_httpd, &capture_uri);
    
    // Also register /snapshot as an alias for /capture (for frontend compatibility)
    httpd_uri_t snapshot_uri = {
        .uri = "/snapshot",
        .method = HTTP_GET,
        .handler = capture_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(stream_httpd, &snapshot_uri);
    
    httpd_uri_t status_uri = {
        .uri = "/status",
        .method = HTTP_GET,
        .handler = status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(stream_httpd, &status_uri);
    
    ESP_LOGI(TAG, "Live /stream disabled (snapshot-only mode) — was: %s", stream_url);
    ESP_LOGI(TAG, "Snapshot endpoint: http://[IP]:%d/capture", STREAM_PORT);
    ESP_LOGI(TAG, "Snapshot endpoint: http://[IP]:%d/snapshot", STREAM_PORT);
    ESP_LOGI(TAG, "Status endpoint: http://[IP]:%d/status", STREAM_PORT);
    
    return ESP_OK;
}

void camera_stream_stop(void)
{
    if (stream_httpd != NULL) {
        httpd_stop(stream_httpd);
        stream_httpd = NULL;
        ESP_LOGI(TAG, "Camera stream server stopped");
    }
}

const char* camera_stream_get_url(void)
{
    return stream_url;
}

bool camera_stream_is_running(void)
{
    return (stream_httpd != NULL);
}

bool camera_stream_is_active(void)
{
    return stream_active;
}
