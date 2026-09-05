# AEGIS Warden - Backend Communication Integration

## Overview

This document describes the backend communication integration for the Warden robot in the AEGIS multi-robot system. Warden now communicates with the central AEGIS backend at `http://10.75.11.83:8000` using the same protocol as Guardian and Pathfinder robots.

## Architecture

```
WARDEN BOT
    │
    ├─── Camera (OV2640)
    │       │
    │       ▼
    ├─── Edge Impulse AI (Fire/Smoke Detection)
    │       │
    │       ▼
    ├─── Warden Master Task (Core 1)
    │       │
    │       ├─── ESP-NOW → Slave Device
    │       │
    │       └─── Telemetry Queue (Non-blocking)
    │               │
    │               ▼
    └─── Telemetry Task (Core 0)
            │
            ├─── cJSON Serialization
            │
            └─── HTTP Client → AEGIS Backend
                    │
                    └─── Wi-Fi Station Mode
```

## Configuration

### 1. Wi-Fi Credentials

Edit `main/warden_backend.h` and set your Wi-Fi credentials:

```cpp
#define WIFI_SSID "Mateen"
#define WIFI_PASSWORD "12345678"
```

**IMPORTANT**: Ensure your Wi-Fi router is on the same channel (or compatible channel) as your ESP-NOW slave device for optimal performance.

### 2. Backend Server

The backend URL is configured for your local network:

```cpp
#define BACKEND_SERVER_URL "http://192.168.1.100:8000"
#define BOT_ID "Warden"
```

### 3. ESP-NOW Configuration

Update `main/warden_config.h` if needed:

```cpp
#define WARDEN_USE_BROADCAST 0  // Set to 1 for broadcast mode
#define WARDEN_SLAVE_MAC { 0x00, 0x70, 0x07, 0x7E, 0xD8, 0x14 }
```

**NOTE**: ESP-NOW now runs on the same Wi-Fi channel as your AP connection. The fixed channel configuration has been removed to ensure compatibility.

## Communication Channels

### 1. Status Telemetry (Every 3 seconds)

**Endpoint**: `POST /api/v1/telemetry/ingest`

**Payload**:
```json
{
  "bot_id": "Warden",
  "kind": "telemetry",
  "payload": {
    "status": "CLEAR|FIRE|SMOKE|FIRE_SMOKE",
    "system_info": {
      "free_heap": 123456,
      "free_psram": 456789
    },
    "wifi_rssi": -52,
    "ip_address": "10.75.11.120",
    "timestamp": "uptime_120s",
    "detection_state": {
      "fire_streak": 0,
      "smoke_streak": 0,
      "clear_streak": 8
    }
  }
}
```

### 2. Vision Detection Telemetry

**Endpoint**: `POST /api/v1/telemetry/ingest`

**Sent when**: Fire or smoke detected above threshold

**Payload**:
```json
{
  "bot_id": "Warden",
  "kind": "vision_detection",
  "payload": {
    "label": "fire",
    "confidence": 0.94,
    "is_threat": true,
    "bbox": {
      "x": 100,
      "y": 80,
      "w": 120,
      "h": 200
    },
    "timing": {
      "total_ms": 245
    }
  }
}
```

### 3. Incident Reports

**Endpoint**: `POST /api/v1/incidents/report`

**Sent when**: State transitions occur (clear → fire, etc.)

**Payload**:
```json
{
  "bot_id": "Warden",
  "type": "FIRE_DETECTED|SMOKE_DETECTED|FIRE_SMOKE_DETECTED|SYSTEM_CLEAR",
  "message": "Fire detected by vision system",
  "severity": "CRITICAL|HIGH|MEDIUM|LOW",
  "context": {
    "fire_confidence": 0.94,
    "smoke_confidence": 0.12
  }
}
```

### 4. Backend Health Check

**Endpoint**: `GET /`

**When**: On startup after Wi-Fi connection

### 5. Active Scenario Retrieval

**Endpoint**: `GET /api/v1/test/current-scenario`

**When**: On startup after backend health check

## Code Integration Points

### Telemetry Functions (Non-blocking)

All telemetry is sent through a FreeRTOS queue to avoid blocking the AI task:

```cpp
// Queue status telemetry
status_data_t status = { ... };
queue_status_telemetry(&status);

// Queue vision detection
vision_detection_t detection = { ... };
queue_vision_detection(&detection);

// Queue incident report
incident_data_t incident = { ... };
queue_incident_report(&incident);
```

### Integration in warden_master_task()

The backend integration has been added to the main AI task at these points:

1. **After inference**: Vision detections are queued for detections above threshold
2. **Command transitions**: Incidents are reported when state changes (clear → fire, etc.)
3. **Periodic status**: Status telemetry sent every 3 seconds
4. **System clear**: Clear incident reported after sustained no-detection

### Initialization in app_main()

```cpp
// 1. NVS initialization
nvs_flash_init();

// 2. Camera initialization
camera_init();

// 3. Backend communication (Wi-Fi + Telemetry task)
backend_init();

// 4. ESP-NOW initialization (reuses Wi-Fi from backend)
espnow_init();

// 5. Start Warden master task
xTaskCreatePinnedToCore(warden_master_task, ...);
```

## Non-Blocking Design

**CRITICAL**: The Warden master task (AI + detection) **NEVER** directly makes HTTP requests.

All backend communication flows through:
1. Main task calls `queue_*()` functions (immediate return)
2. Message placed in FreeRTOS queue
3. Telemetry task (separate core) processes queue
4. Telemetry task performs HTTP operations

This ensures:
- AI inference is never blocked by network operations
- Camera processing continues during network issues
- ESP-NOW communication remains real-time
- System continues operating if backend is unavailable

## Fault Tolerance

### Wi-Fi Connection Failure
- Maximum 10 retries with automatic reconnection
- System continues operating without backend
- ESP-NOW continues functioning
- AI detection continues functioning

### Backend Unavailable
- Health check performed on startup
- `backend_is_online()` tracks backend state
- Telemetry queuing continues (messages dropped if queue full)
- No crashes, no reboots, no service interruption

### Queue Full
- Telemetry messages are dropped with warning log
- System continues operating normally
- Older messages remain in queue for processing

## Dependencies

All required dependencies are part of ESP-IDF 5.x:

- `esp_wifi` - Wi-Fi station mode
- `esp_http_client` - HTTP client for REST API
- `esp_netif` - Network interface
- `json` (cJSON) - JSON serialization
- `nvs_flash` - Non-volatile storage
- `esp_timer` - Uptime tracking

No external dependencies required.

## Build Instructions

1. Update Wi-Fi credentials in `main/warden_backend.h`
2. Build the project:
   ```bash
   idf.py build
   ```
3. Flash to ESP32-S3:
   ```bash
   idf.py flash monitor
   ```

## Monitoring

### Serial Monitor Output

```
[BACKEND] Initializing backend communication...
[BACKEND] Backend URL: http://10.75.11.83:8000
[BACKEND] Bot ID: Warden
[BACKEND] Wi-Fi station started, connecting...
[BACKEND] Connected to Wi-Fi, IP: 10.75.11.120
[BACKEND] Backend health check: ONLINE
[BACKEND] Active scenario: fire_drill_scenario
[BACKEND] Telemetry task started
[WARDEN] Warden Master READY
[WARDEN] Watching for FIRE / SMOKE...
```

### Backend Dashboard

The backend can now track:
- Warden online/offline status
- Real-time fire/smoke detections
- System health (heap, PSRAM, Wi-Fi signal)
- Detection streaks and confidence levels
- Incident timeline (fire detected → clear)

## Multi-Robot System

All three robots now communicate with the same backend:

```
AEGIS BACKEND (192.168.1.100:8000)
        │
        ├─── Guardian (Bot ID: "Guardian")
        │       └─── Telemetry, Incidents, Vision
        │
        ├─── Pathfinder (Bot ID: "Pathfinder")
        │       └─── Telemetry, Incidents, Vision
        │
        └─── Warden (Bot ID: "Warden")
                └─── Telemetry, Incidents, Fire/Smoke Detection
```

Each robot is identified by its unique `bot_id` field in all messages.

## Important Notes

### ESP-NOW + Wi-Fi Coexistence

- **Previous**: Warden used WIFI_MODE_STA without AP connection for ESP-NOW
- **Current**: Warden uses WIFI_MODE_STA with AP connection for backend + ESP-NOW
- **Channel**: ESP-NOW uses the same channel as the AP connection (no fixed channel)
- **Impact**: ESP-NOW slave device must be on a compatible channel with your AP

### Task Allocation

- **Core 0**: Telemetry task (backend communication)
- **Core 1**: Warden master task (AI inference, camera, ESP-NOW)

### Memory Usage

- Telemetry queue: 20 messages × ~200 bytes = ~4 KB
- HTTP buffers: ~4 KB total
- cJSON temporary buffers: ~2 KB per message
- **Total additional RAM**: ~10 KB

### Detection State Machine

```
CLEAR ──(3 fire frames)──> FIRE ──(8 clear frames)──> CLEAR
  │                           │
  └──(3 smoke frames)──> SMOKE ──(8 clear frames)──> CLEAR
  │                           │
  └──(3 both frames)──> FIRE_SMOKE ──(8 clear)──> CLEAR
```

Incidents are reported only on state transitions to avoid spam.

## Troubleshooting

### Backend Not Online
1. Verify Wi-Fi credentials (SSID: "Mateen", Password: "12345678")
2. Ensure backend server is running: `curl http://192.168.1.100:8000/`
3. Check network connectivity from ESP32-S3
4. Review serial monitor for connection errors

### ESP-NOW Not Working
1. Ensure slave device is on compatible channel
2. Check MAC address configuration in `warden_config.h`
3. Verify Wi-Fi AP channel matches slave device expectations
4. Consider using broadcast mode for testing

### Telemetry Not Appearing
1. Check `backend_is_online()` status in logs
2. Verify queue is not full (increase `TELEMETRY_QUEUE_SIZE`)
3. Check HTTP response codes in serial monitor
4. Verify backend endpoints are correct

### High Memory Usage
1. Reduce `TELEMETRY_QUEUE_SIZE` if needed
2. Monitor heap with `esp_get_free_heap_size()`
3. Check for memory leaks in cJSON usage

## Future Enhancements

Potential improvements for future iterations:

- HTTPS support with SSL/TLS
- Authentication tokens for backend API
- Configurable telemetry intervals via backend
- OTA firmware updates from backend
- Multi-backend redundancy
- Local SD card logging when backend unavailable
- WebSocket for real-time bidirectional communication

---

**Author**: AEGIS Development Team  
**Version**: 1.0  
**Date**: 2026-08-15
