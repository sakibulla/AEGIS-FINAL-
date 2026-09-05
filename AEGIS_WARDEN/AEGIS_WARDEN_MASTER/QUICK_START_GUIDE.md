# Warden Quick Start Guide

## What's New? 🎉

Your Warden now has:
✅ Live camera streaming on port 81
✅ Backend telemetry integration (10.75.11.83:8000)
✅ Fire/Smoke AI detection with alerts
✅ Real-time data monitoring

---

## Flash and Deploy

### 1. Build and Flash
```bash
cd E:\AEGIS_GUARDIAN_FOMO\AEGIS_WARDEN\AEGIS_WARDEN_MASTER
idf.py build flash monitor
```

### 2. Get Warden's IP Address

From serial monitor, look for:
```
[BACKEND] Connected to Wi-Fi, IP: 10.75.11.120
[CAM_STREAM] Stream URL: http://10.75.11.120:81/stream
```

**Write down this IP address!**

---

## View Live Camera

### Option 1: Browser (Easiest)

Open in any browser:
```
http://10.75.11.120:81/
```

### Option 2: VLC Player

1. Open VLC
2. Media → Open Network Stream
3. Enter: `http://10.75.11.120:81/stream`
4. Play

### Option 3: Backend Dashboard

```
http://10.75.11.83:8000/api/v1/video/stream/Warden
```

---

## Check Backend Integration

### 1. Start Backend
```bash
cd E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Backend
start_backend.bat
```

### 2. Update Test Script

Open `E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Backend\test_warden.py`

Change line 8:
```python
WARDEN_DIRECT_IP = "10.75.11.120"  # <- Update with your Warden's IP
```

### 3. Run Tests
```bash
python test_warden.py
```

This will:
- ✅ Check backend health
- ✅ Test camera streaming
- ✅ Verify telemetry data
- ✅ Check fire/smoke detections
- ✅ Show incident reports
- 🎥 Capture snapshot

---

## What Data Goes to Backend?

### Every 3 Seconds: Status Update
```json
{
  "bot_id": "Warden",
  "status": "CLEAR",
  "system_info": {
    "free_heap": 150000,
    "free_psram": 4000000
  },
  "wifi_rssi": -45,
  "ip_address": "10.75.11.120"
}
```

### When Fire/Smoke Detected: Vision Alert
```json
{
  "bot_id": "Warden",
  "label": "fire",
  "confidence": 0.92,
  "is_threat": true
}
```

### On State Change: Incident Report
```json
{
  "bot_id": "Warden",
  "type": "FIRE_DETECTED",
  "severity": "CRITICAL",
  "message": "Fire detected by vision system"
}
```

---

## Monitor Live Data

### Check Warden Status
```bash
curl http://10.75.11.83:8000/api/v1/bots/Warden
```

### Get Vision Detections
```bash
curl http://10.75.11.83:8000/api/v1/bots/Warden/detections
```

### Get All Incidents
```bash
curl http://10.75.11.83:8000/api/v1/incidents
```

### Capture Snapshot
```bash
curl http://10.75.11.120:81/capture -o snapshot.jpg
```

---

## Troubleshooting

### ❌ Camera Not Working

1. Check serial monitor for:
   ```
   [CAM_STREAM] Starting camera stream server on port 81
   ```

2. Ping Warden:
   ```bash
   ping 10.75.11.120
   ```

3. Test status endpoint:
   ```bash
   curl http://10.75.11.120:81/status
   ```

### ❌ Backend Not Receiving Data

1. Check Wi-Fi connection in serial:
   ```
   [BACKEND] Connected to Wi-Fi, IP: 10.75.11.XXX
   [BACKEND] Backend health check: ONLINE
   ```

2. Check backend is running:
   ```bash
   curl http://10.75.11.83:8000/
   ```

3. Verify same network

### ❌ No Detections

1. Check threshold in `main/warden_config.h`:
   ```cpp
   #define WARDEN_DETECTION_THRESHOLD 0.50f
   ```

2. Test with lighter/candle/LED flashlight

3. Watch serial monitor:
   ```
   [WARDEN] FIRE detected (conf=0.92)
   ```

---

## Architecture Summary

```
Warden ESP32-S3
  ├─ Camera Server (Port 81)
  │   ├─ /stream      (MJPEG video)
  │   ├─ /capture     (JPEG snapshot)
  │   └─ /status      (JSON info)
  │
  ├─ AI Inference (Core 1)
  │   ├─ Edge Impulse FOMO
  │   ├─ Fire detection
  │   └─ Smoke detection
  │
  └─ Backend Client (Core 0)
      ├─ Status telemetry (3s)
      ├─ Vision detections
      └─ Incident reports
          ↓
    Backend (10.75.11.83:8000)
      ├─ /api/v1/telemetry/ingest
      ├─ /api/v1/incidents/report
      └─ /api/v1/video/stream/Warden
          ↓
    Frontend Dashboard
      └─ Live camera + detections
```

---

## Files Created/Modified

### New Files:
- `main/warden_camera_stream.h` - Camera streaming header
- `main/warden_camera_stream.cpp` - Camera streaming implementation
- `VIDEO_STREAMING_INTEGRATION.md` - Detailed integration guide
- `BACKEND_READINESS_REPORT.md` - Backend status report
- `QUICK_START_GUIDE.md` - This file

### Modified Files:
- `main/warden_main.cpp` - Added camera stream init
- `main/CMakeLists.txt` - Added streaming module & dependencies
- `sdkconfig` - Custom partition table enabled
- `main/warden_backend.h` - Wi-Fi credentials updated

### Test Files:
- `../AEGIS-Backend/test_warden.py` - Comprehensive test script

---

## Next Steps

1. ✅ Flash Warden firmware
2. ✅ Note IP address from serial
3. ✅ Test camera in browser
4. ✅ Run backend test script
5. ⏭️ Integrate stream into frontend
6. ⏭️ Add detection overlays to UI
7. ⏭️ Deploy multi-robot system

---

## Support URLs

- **Documentation**: `VIDEO_STREAMING_INTEGRATION.md`
- **Backend Report**: `BACKEND_READINESS_REPORT.md`
- **Test Script**: `../AEGIS-Backend/test_warden.py`

---

**Ready to Deploy! 🚀**

Flash, test, and your Warden will be streaming live video with AI fire/smoke detection to the backend!
