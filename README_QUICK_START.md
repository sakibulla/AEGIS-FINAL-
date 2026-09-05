# AEGIS Guardian System - Quick Start Guide

## 🚀 Quick Start (3 Steps)

### Step 1: Start Backend Server
```bash
cd AEGIS_SOFTWARE\AEGIS-Backend
START_BACKEND.bat
```
**Wait for**: `"Application startup complete"` message

### Step 2: Flash ESP32 Bots (if needed)
```bash
# Warden (Fire/Smoke Detection)
cd AEGIS_WARDEN\AEGIS_WARDEN_MASTER
idf.py build flash monitor

# Guardian (Face Recognition + Object Detection)
cd AEGIS_GURDIAN\AEGIS_GURDIAN_MASTER_FOMO
idf.py build flash monitor

# Pathfinder (SLAM Mapping)
cd AEGIS_PATHFINDER\aegis_pathfinder_master
idf.py build flash monitor
```

### Step 3: Start Frontend Dashboard
```bash
cd AEGIS_SOFTWARE\AEGIS-Frontend
npm start
# or for web
npx expo start --web
```

---

## ⚙️ Configuration

### Backend IP Address Setup

**IMPORTANT**: All ESP32 bots must know your backend server IP address!

1. **Find your computer's IP address**:
   ```bash
   ipconfig
   # Look for: IPv4 Address (e.g., 192.168.1.100)
   ```

2. **Update each bot's code** (before flashing):

   **Warden** - `AEGIS_WARDEN\AEGIS_WARDEN_MASTER\main\warden_main.cpp`:
   ```cpp
   #define BACKEND_SERVER_URL "http://YOUR_IP:8000"  // Line 42
   #define WIFI_SSID          "YourWiFi"             // Line 54
   #define WIFI_PASSWORD      "YourPassword"          // Line 55
   ```

   **Guardian** - `AEGIS_GURDIAN\AEGIS_GURDIAN_MASTER_FOMO\main\main.cpp`:
   ```cpp
   #define BACKEND_SERVER_URL "http://YOUR_IP:8000"  // Line 98
   #define WIFI_SSID          "YourWiFi"             // Line 88
   #define WIFI_PASSWORD      "YourPassword"          // Line 89
   ```

   **Pathfinder** - `AEGIS_PATHFINDER\aegis_pathfinder_master\main\main.cpp`:
   ```cpp
   #define BACKEND_SERVER_URL "http://YOUR_IP:8000"  // Line 38
   #define WIFI_SSID          "YourWiFi"             // Line 28
   #define WIFI_PASSWORD      "YourPassword"          // Line 29
   ```

3. **Rebuild and reflash** all bots after updating

---

## 🎥 Video Stream URLs

### Via Backend Proxy (Recommended)
- **Guardian**: http://localhost:8000/api/v1/video/stream/Guardian
- **Pathfinder**: http://localhost:8000/api/v1/video/stream/Pathfinder
- **Warden**: http://localhost:8000/api/v1/video/stream/Warden

### Direct ESP32 (if on same network)
- **Guardian**: http://192.168.0.102/stream
- **Pathfinder**: http://192.168.0.101/stream
- **Warden**: http://192.168.0.103/stream

### Web Viewer
- http://localhost:8000/video

---

## 🔍 Troubleshooting

### Frontend shows "Socket closed 1006"
**Cause**: Backend server not running or wrong URL
**Fix**: 
1. Start backend: `cd AEGIS_SOFTWARE\AEGIS-Backend && START_BACKEND.bat`
2. Verify it's running: http://localhost:8000

### Warden not sending telemetry
**Cause**: Wrong backend IP address or WiFi not connected
**Fix**:
1. Check Warden serial monitor for WiFi connection status
2. Update `BACKEND_SERVER_URL` to your computer's IP address
3. Ensure Warden and computer are on same WiFi network
4. Check IP with: `ipconfig` (Windows) or `ifconfig` (Mac/Linux)

### Video stream shows standby HUD instead of camera
**Cause**: Backend can't reach ESP32 camera
**Fix**:
1. Verify ESP32 is on same WiFi network
2. Check ESP32 IP address in serial monitor
3. Test direct access: http://[ESP32_IP]/stream
4. Update backend's bot IP if needed

### All bots show "Offline"
**Cause**: Bots not flashed or backend unreachable
**Fix**:
1. Flash ESP32 bots with `idf.py flash monitor`
2. Verify WiFi credentials match your network
3. Check backend IP is correct
4. Monitor serial output for connection errors

---

## 📊 System Architecture

```
┌─────────────────┐      HTTP/POST      ┌──────────────────┐
│   Warden ESP32  │ ─────telemetry─────▶│  FastAPI Backend │
│  (Fire/Smoke)   │ ◀────video proxy────│  (Port 8000)     │
└─────────────────┘                      │                  │
                                         │  - Video Service │
┌─────────────────┐                      │  - Telemetry WS  │      ┌─────────────────┐
│ Guardian ESP32  │ ─────telemetry─────▶│  - Incident Log  │◀─WS──│  React Native   │
│ (Vision AI)     │ ◀────video proxy────│  - Test Scenarios│      │  Frontend       │
└─────────────────┘                      └──────────────────┘      │  (Expo)         │
                                                                    └─────────────────┘
┌─────────────────┐
│Pathfinder ESP32 │ ─────telemetry─────▶
│  (SLAM Mapper)  │ ◀────video proxy────
└─────────────────┘
```

---

## 📡 ESP-NOW Mesh Network

```
┌─────────────┐                        ┌──────────────┐
│  Guardian   │──── ESP-NOW Alert ────▶│ Robot Slave  │
│  (Master)   │       (Channel 0)      │ (Receiver)   │
└─────────────┘                        └──────────────┘

┌─────────────┐                        ┌──────────────┐
│   Warden    │── ESP-NOW Fire/Smoke ─▶│ Alarm Slave  │
│  (Master)   │      (Channel 1)       │ (Actuator)   │
└─────────────┘                        └──────────────┘
```

- Guardian broadcasts intruder/weapon alerts to slave robots
- Warden broadcasts fire/smoke detection packets with confidence scores
- Slaves can trigger sirens, lights, or mechanical responses

---

## 🛠️ Development Commands

### Backend
```bash
cd AEGIS_SOFTWARE\AEGIS-Backend

# Install dependencies
pip install -r requirements.txt

# Start dev server
python -m uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload

# Test endpoints
python test_endpoints.py
```

### Frontend
```bash
cd AEGIS_SOFTWARE\AEGIS-Frontend

# Install dependencies
npm install

# Start web dev
npm start
# or
npx expo start --web

# Start mobile (Android/iOS)
npx expo start
```

### ESP32 (ESP-IDF)
```bash
# Build only
idf.py build

# Flash to ESP32
idf.py flash

# Monitor serial output
idf.py monitor

# Full build + flash + monitor
idf.py build flash monitor

# Clean build
idf.py fullclean
```

---

## 📝 Default Credentials

### WiFi
- **SSID**: A34
- **Password**: 01234567

### Backend
- **URL**: http://localhost:8000
- **WebSocket**: ws://localhost:8000/api/v1/ws/telemetry

### Bot IPs (Default)
- **Guardian**: 192.168.0.102
- **Pathfinder**: 192.168.0.101
- **Warden**: 192.168.0.103

*(Update these in code before flashing)*

---

## ✅ System Health Checks

### 1. Backend Running?
```bash
curl http://localhost:8000
# Should return: {"status":"online","system":"A.E.G.I.S. Backend active",...}
```

### 2. Bot Connected?
```bash
curl http://localhost:8000/api/v1/bots
# Should return: array of bot objects with status/battery/etc
```

### 3. Video Stream Working?
Open in browser:
- http://localhost:8000/api/v1/video/stream/Warden

### 4. WebSocket Connected?
Check browser console in frontend - should see:
```
[AEGIS] Swarm telemetry socket connected to ws://localhost:8000/api/v1/ws/telemetry
```

---

## 🎯 Success Criteria

When everything is working, you should see:

✅ Backend console: `"Application startup complete"`
✅ Backend console: `"[AEGIS Video] Background Video Streaming Service started"`
✅ ESP32 monitor: `"Connected to WiFi! IP: X.X.X.X"`
✅ ESP32 monitor: `"Backend POST /api/v1/telemetry/ingest OK (HTTP 200)"`
✅ Frontend: Green dot "Live Swarm Telemetry Online"
✅ Frontend: Bot cards showing battery, IP, status
✅ Video streams: Live camera feed (not standby HUD)

---

## 📚 Documentation

- **Architecture**: `AEGIS_GURDIAN\AEGIS_GURDIAN_MASTER_FOMO\ARCHITECTURE.md`
- **API Routes**: Check backend code at `AEGIS_SOFTWARE\AEGIS-Backend\app\api\routes.py`
- **Frontend Services**: `AEGIS_SOFTWARE\AEGIS-Frontend\src\services\AegisService.js`
- **Video Service**: `AEGIS_SOFTWARE\AEGIS-Backend\app\services\video_service.py`

---

Built with ESP32-S3, Edge Impulse FOMO, FastAPI, React Native (Expo)
