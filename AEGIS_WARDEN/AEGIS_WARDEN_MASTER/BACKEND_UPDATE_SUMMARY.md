# AEGIS Warden - Backend Integration Update Summary

**Date**: August 15, 2026  
**System**: AEGIS Warden Master (Fire/Smoke Detection Bot)  
**Backend**: FastAPI Server at http://192.168.1.100:8000

---

## Updates Made

### 1. Backend Server Configuration (`main/warden_backend.h`)

**Updated Settings:**
```cpp
#define BACKEND_SERVER_URL "http://192.168.1.100:8000"
#define WIFI_SSID "Mateen"
#define WIFI_PASSWORD "12345678"
```

**Previous Settings:**
```cpp
#define BACKEND_SERVER_URL "http://10.75.11.83:8000"
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
```

### 2. Documentation Updates (`BACKEND_INTEGRATION.md`)

Updated all references to reflect:
- New backend URL: `192.168.1.100:8000`
- Wi-Fi credentials: SSID "Mateen" with password
- Troubleshooting commands with correct IP

---

## What the Warden Bot Does

### Communication with Backend

The Warden bot is now configured to communicate with your local AEGIS backend server. It sends:

#### 1. **Status Telemetry** (Every 3 seconds)
- System status: CLEAR / FIRE / SMOKE / FIRE_SMOKE
- Memory info: Free heap, PSRAM
- Network info: Wi-Fi RSSI, IP address
- Detection streaks: Fire, smoke, clear frame counts
- Uptime

#### 2. **Vision Detection Events**
- Sent when fire or smoke is detected
- Includes: Label, confidence score, bounding box
- Inference timing information

#### 3. **Incident Reports**
- FIRE_DETECTED (Critical severity)
- SMOKE_DETECTED (High severity)
- FIRE_SMOKE_DETECTED (Critical severity)
- SYSTEM_CLEAR (Low severity)
- Only sent on state transitions to avoid spam

---

## Network Architecture

```
Wi-Fi Router (SSID: "Mateen")
        │
        ├─── Backend Server (192.168.1.100:8000)
        │       └─── FastAPI + WebSocket
        │
        └─── Warden Bot (ESP32-S3-EYE)
                ├─── Wi-Fi STA Mode
                ├─── Camera + AI Detection
                ├─── ESP-NOW → Slave Device
                └─── HTTP Client → Backend
```

---

## Next Steps

### 1. Build and Flash the Firmware

```bash
cd e:\AEGIS_GUARDIAN_FOMO\AEGIS_WARDEN\AEGIS_WARDEN_MASTER
idf.py build
idf.py flash monitor
```

### 2. Start the Backend Server

The backend is already configured in:
```
e:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Backend
```

To start it:
```bash
cd e:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Backend
start_backend.bat
```

This will start the FastAPI server on:
- HTTP API: http://192.168.1.100:8000
- WebSocket: ws://192.168.1.100:8000/api/v1/ws/telemetry
- Swagger Docs: http://192.168.1.100:8000/docs

### 3. Monitor the System

**Warden Serial Monitor Output:**
```
[BACKEND] Initializing backend communication...
[BACKEND] Backend URL: http://192.168.1.100:8000
[BACKEND] Bot ID: Warden
[BACKEND] Wi-Fi station started, connecting...
[BACKEND] Connected to Wi-Fi, IP: 192.168.1.xxx
[BACKEND] Backend health check: ONLINE
[BACKEND] Telemetry task started
[WARDEN] Warden Master READY
[WARDEN] Watching for FIRE / SMOKE...
```

**Backend Server Output:**
```
INFO:     Started server process
INFO:     Uvicorn running on http://0.0.0.0:8000
POST /api/v1/telemetry/ingest - 200 OK (Warden status update)
POST /api/v1/telemetry/ingest - 200 OK (Warden vision detection)
POST /api/v1/incidents/report - 200 OK (Warden incident)
```

---

## Features Enabled

### Non-Blocking Telemetry
- All backend communication runs on Core 0
- AI inference runs on Core 1
- Camera processing never blocked by network
- ESP-NOW continues working during network issues

### Fault Tolerance
- System continues if Wi-Fi fails
- System continues if backend is offline
- Telemetry queued and processed when available
- No crashes or reboots due to network issues

### Multi-Robot Integration
- Guardian, Pathfinder, and Warden all report to same backend
- Each bot identified by unique "bot_id"
- Centralized monitoring and coordination
- Unified incident management

---

## Configuration Summary

| Parameter | Value |
|-----------|-------|
| Backend URL | http://192.168.1.100:8000 |
| Bot ID | Warden |
| Wi-Fi SSID | Mateen |
| Wi-Fi Password | 12345678 |
| Status Interval | 3000 ms (3 seconds) |
| HTTP Timeout | 5000 ms (5 seconds) |
| Telemetry Queue | 20 messages |

---

## Verification Checklist

- [x] Backend URL updated to 192.168.1.100:8000
- [x] Wi-Fi credentials configured (SSID: Mateen)
- [x] Documentation updated
- [ ] Backend server running and accessible
- [ ] Warden firmware built successfully
- [ ] Warden firmware flashed to ESP32-S3
- [ ] Wi-Fi connection verified
- [ ] Backend health check passing
- [ ] Telemetry appearing in backend logs
- [ ] Fire/smoke detection working
- [ ] ESP-NOW communication to slave working

---

## Important Notes

### ESP-NOW + Wi-Fi Coexistence
- Warden uses Wi-Fi STA mode with AP connection for backend
- ESP-NOW runs on the same channel as the AP
- Slave device must be on compatible channel
- If ESP-NOW issues occur, check channel compatibility

### Detection Thresholds
```cpp
#define WARDEN_DETECTION_THRESHOLD 0.50f   // 50% confidence
#define WARDEN_DETECTION_FRAMES 3           // 3 consecutive frames
#define WARDEN_CLEAR_FRAMES 8               // 8 clear frames before CLEAR
```

### Memory Usage
- Telemetry adds ~10 KB RAM overhead
- HTTP buffers: ~4 KB
- JSON buffers: ~2 KB per message
- Queue: ~4 KB (20 messages)

---

## Troubleshooting

### Wi-Fi Connection Failed
```
[BACKEND] Failed to connect to SSID: Mateen
```
**Solution**: Check Wi-Fi password, router availability, signal strength

### Backend Health Check Failed
```
[BACKEND] Backend health check: OFFLINE
```
**Solution**: 
1. Verify backend is running: `curl http://192.168.1.100:8000/`
2. Check IP address is correct
3. Ensure firewall allows port 8000
4. Check backend logs for errors

### Telemetry Queue Full
```
[BACKEND] Telemetry queue full, dropping message
```
**Solution**: Backend may be slow or unreachable. Messages will be dropped but system continues.

### ESP-NOW Send Failed
```
[WARDEN] esp_now_send() failed: ESP_ERR_ESPNOW_NOT_INIT
```
**Solution**: Check slave MAC address in `warden_config.h`, verify channel compatibility

---

**Status**: ✅ Configuration Complete - Ready for Build & Flash

