#include "web_streamer.h"
#include "vision_task.h"
#include "protocol_defs.h"
#include "espnow_mesh.h"

#include <esp_http_server.h>
#include <esp_camera.h>
#include <img_converters.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

static const char *TAG = "WEB_STREAMER";

// External mode toggle defined in main.cpp
extern void set_auto_mode(bool enable);

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY     = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART         = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// ── Built-In Web Dashboard Interface ──────────────────────────────────────
static const char* INDEX_HTML = R"rawhtml(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>AEGIS Vision Dashboard</title>
<style>
  body { margin:0; background:#111; color:#0f0; display:flex; flex-direction:column; align-items:center; min-height:100vh; font-family:monospace; }
  h1 { color:#0f0; margin:12px 0 8px; font-size:1.1em; letter-spacing:2px; }
  #wrap { position:relative; display:inline-block; }
  #stream { display:block; width:640px; height:480px; image-rendering:pixelated; background:#222; }
  #overlay { position:absolute; top:0; left:0; width:640px; height:480px; pointer-events:none; }
  #status { font-size:0.8em; margin:8px 0; }
  .controls { display:grid; grid-template-columns:repeat(3, 60px); gap:6px; margin-top:10px; }
  button { background:#222; color:#0f0; border:1px solid #0f0; padding:10px; font-family:monospace; cursor:pointer; font-weight:bold; }
  button:active { background:#0f0; color:#000; }
  .btn-mode { grid-column:span 3; background:#333; margin-bottom:6px; }
</style>
</head>
<body>
<h1>&#9632; AEGIS PATHFINDER CONTROL</h1>
<div id="wrap">
  <img id="stream" src="/snapshot">
  <canvas id="overlay" width="640" height="480"></canvas>
</div>
<div id="status">Connecting...</div>

<div class="controls">
  <button class="btn-mode" onclick="toggleMode('auto')">AUTO MAPPING MODE</button>
  <div></div>
  <button onclick="sendCmd('FWD')">&#9650;</button>
  <div></div>
  <button onclick="sendCmd('LEFT')">&#9664;</button>
  <button onclick="sendCmd('STOP')">&#9632;</button>
  <button onclick="sendCmd('RIGHT')">&#9654;</button>
  <div></div>
  <button onclick="sendCmd('REV')">&#9660;</button>
  <div></div>
</div>

<script>
const canvas = document.getElementById('overlay');
const ctx    = canvas.getContext('2d');
const status = document.getElementById('status');
const MODEL_W = 96, MODEL_H = 96, DISP_W = 640, DISP_H = 480;
const COLORS  = ['#00ff00','#ff3300','#00aaff','#ffaa00','#ff00ff','#00ffcc'];

function colorFor(label) {
  let h = 0;
  for (let i = 0; i < label.length; i++) h = (h * 31 + label.charCodeAt(i)) & 0xffff;
  return COLORS[h % COLORS.length];
}

async function sendCmd(cmd) {
  try { await fetch(`/control?cmd=${cmd}`); } catch(e){}
}

async function toggleMode(mode) {
  try { await fetch(`/control?mode=${mode}`); } catch(e){}
}

async function fetchDetections() {
  try {
    const r = await fetch('/detections');
    if (!r.ok) return;
    const data = await r.json();
    ctx.clearRect(0, 0, DISP_W, DISP_H);
    if (!data.detections || data.detections.length === 0) {
      status.textContent = `No detections | ${data.ms}ms inference`;
      return;
    }
    const scaleX = DISP_W / MODEL_W, scaleY = DISP_H / MODEL_H;
    data.detections.forEach(d => {
      const x = d.x * scaleX, y = d.y * scaleY, w = d.w * scaleX, h = d.h * scaleY;
      const col = colorFor(d.label);
      ctx.strokeStyle = col; ctx.lineWidth = 2;
      ctx.strokeRect(x, y, w, h);
      const label = `${d.label} ${(d.score*100).toFixed(0)}%`;
      ctx.font = 'bold 13px monospace';
      ctx.fillStyle = col;
      ctx.fillRect(x, y - 18, ctx.measureText(label).width + 6, 18);
      ctx.fillStyle = '#000';
      ctx.fillText(label, x + 3, y - 4);
    });
    status.textContent = `${data.detections.length} detection(s) | ${data.ms}ms inference`;
  } catch(e) { status.textContent = 'Connection error'; }
}
setInterval(fetchDetections, 150);

// No live /stream anymore (snapshot-only, matches Guardian/Warden) — refresh
// the still image periodically instead.
const streamImg = document.getElementById('stream');
setInterval(() => { streamImg.src = '/snapshot?t=' + Date.now(); }, 1000);
</script>
</body>
</html>)rawhtml";

// ── HTTP Endpoint Handlers ────────────────────────────────────────────────
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t detections_handler(httpd_req_t *req) {
    char json[1024];
    int pos = 0;
    detection_t dets[MAX_DETECTIONS];
    int count = 0, ms = 0;

    get_current_detections(dets, &count, &ms);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    pos += snprintf(json + pos, sizeof(json) - pos, "{\"count\":%d,\"ms\":%d,\"detections\":[", count, ms);
    for (int i = 0; i < count && pos < (int)sizeof(json) - 80; i++) {
        pos += snprintf(json + pos, sizeof(json) - pos,
                        "%s{\"label\":\"%s\",\"score\":%.3f,\"x\":%lu,\"y\":%lu,\"w\":%lu,\"h\":%lu}",
                        (i > 0 ? "," : ""), dets[i].label, dets[i].score,
                        (unsigned long)dets[i].x, (unsigned long)dets[i].y,
                        (unsigned long)dets[i].w, (unsigned long)dets[i].h);
    }
    pos += snprintf(json + pos, sizeof(json) - pos, "]}");

    return httpd_resp_send(req, json, pos);
}

static esp_err_t control_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_get_url_query_str(req, buf, sizeof(buf));
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");

    if (ret == ESP_OK) {
        char param[32];
        if (httpd_query_key_value(buf, "mode", param, sizeof(param)) == ESP_OK) {
            if (strcmp(param, "auto") == 0) {
                set_auto_mode(true);
                return httpd_resp_sendstr(req, "{\"status\":\"MODE_AUTO\"}");
            } else if (strcmp(param, "manual") == 0) {
                set_auto_mode(false);
                return httpd_resp_sendstr(req, "{\"status\":\"MODE_MANUAL\"}");
            }
        }

        if (httpd_query_key_value(buf, "cmd", param, sizeof(param)) == ESP_OK) {
            // Manual commands pause the autonomous state machine
            set_auto_mode(false);

            if (strcmp(param, "FWD") == 0) send_cmd_to_slave(CMD_FWD, 400);
            else if (strcmp(param, "REV") == 0) send_cmd_to_slave(CMD_REV, 400);
            else if (strcmp(param, "LEFT") == 0) send_cmd_to_slave(CMD_LEFT, 200);
            else if (strcmp(param, "RIGHT") == 0) send_cmd_to_slave(CMD_RIGHT, 200);
            else if (strcmp(param, "STOP") == 0) send_cmd_to_slave(CMD_STOP, 0);

            return httpd_resp_sendstr(req, "{\"status\":\"CMD_EXECUTED\"}");
        }
    }

    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Query Parameters");
    return ESP_FAIL;
}

static esp_err_t snapshot_handler(httpd_req_t *req) {
    camera_fb_t *fb = safe_camera_fb_get();
    if (!fb) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "image/jpeg");

    if (fb->format == PIXFORMAT_JPEG) {
        esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
        safe_camera_fb_return(fb);
        return res;
    }

    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;
    bool converted = frame2jpg(fb, 80, &jpg_buf, &jpg_len);
    safe_camera_fb_return(fb);

    if (!converted || !jpg_buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    esp_err_t res = httpd_resp_send(req, (const char *)jpg_buf, jpg_len);
    free(jpg_buf);
    return res;
}

static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    char part_buf[128];
    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);

    while (true) {
        int64_t start_time = esp_timer_get_time();
        
        fb = safe_camera_fb_get();
        if (!fb) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (fb->format == PIXFORMAT_JPEG) {
            jpg_buf = fb->buf;
            jpg_len = fb->len;
        } else {
            bool converted = frame2jpg(fb, 75, &jpg_buf, &jpg_len);
            safe_camera_fb_return(fb);
            fb = NULL;
            if (!converted || !jpg_buf) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
        }

        size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, (unsigned int)jpg_len);
        res = httpd_resp_send_chunk(req, part_buf, hlen);
        if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)jpg_buf, jpg_len);
        if (res == ESP_OK) res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));

        if (fb) {
            safe_camera_fb_return(fb);
            fb = NULL;
        } else if (jpg_buf) {
            free(jpg_buf);
            jpg_buf = NULL;
        }

        if (res != ESP_OK) {
            break;
        }

        int64_t elapsed_ms = (esp_timer_get_time() - start_time) / 1000;
        int64_t delay_ms = 40 - elapsed_ms; // ~25 FPS target
        vTaskDelay(pdMS_TO_TICKS(delay_ms > 5 ? delay_ms : 5));
    }
    return res;
}

// ── Public Server Allocator ───────────────────────────────────────────────
esp_err_t start_web_streamer(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 8192;          // Safe stack size for JPEG conversion & JSON
    config.max_uri_handlers = 8;
    config.max_open_sockets = 5;
    config.lru_purge_enable = true;    // Automatically drops stale disconnected sockets
    config.core_id = 0;                // Pin HTTP server to Core 0

    httpd_handle_t server = NULL;
    esp_err_t err = httpd_start(&server, &config);
    if (err == ESP_OK) {
        // Live MJPEG /stream is intentionally not registered — snapshot-only,
        // same as Guardian and Warden.
        httpd_uri_t uris[] = {
            { .uri = "/",          .method = HTTP_GET, .handler = index_handler,      .user_ctx = NULL },
            { .uri = "/snapshot",  .method = HTTP_GET, .handler = snapshot_handler,   .user_ctx = NULL },
            { .uri = "/detections",.method = HTTP_GET, .handler = detections_handler, .user_ctx = NULL },
            { .uri = "/control",   .method = HTTP_GET, .handler = control_handler,    .user_ctx = NULL },
        };
        for (int i = 0; i < 4; i++) {
            httpd_register_uri_handler(server, &uris[i]);
        }
        ESP_LOGI(TAG, "Web Streamer active on Core 0 with /, /snapshot, /detections, and /control routes (no live /stream)");
    }
    return err;
}