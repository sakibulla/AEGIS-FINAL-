# AEGIS Warden Backend Integration - Current Status

**Date**: 2026-08-15  
**Issue**: Frontend shows all bots offline, backend only receives connections from `127.0.0.1`

## ✅ What's Been Fixed

### 1. Frontend Backend URL
- **File**: `E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Frontend\src\services\AegisService.js`
- **Change**: Hardcoded backend URL to `http://10.75.11.83:8000`
- **Before**: Dynamic URL that defaulted to `localhost`
- **After**: Fixed URL pointing to actual backend server

### 2. Warden Backend URL
- **File**: `E:\AEGIS_GUARDIAN_FOMO\AEGIS_WARDEN\AEGIS_WARDEN_MASTER\main\warden_main.cpp` (line 43)
- **Status**: Already correct (`http://10.75.11.83:8000`)

### 3. Camera Port Registration
- **File**: `E:\AEGIS_GUARDIAN_FOMO\AEGIS_WARDEN\AEGIS_WARDEN_MASTER\main\warden_main.cpp`
- **Change**: Added `"camera_port": 81` to status telemetry JSON
- **Backend**: `E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Backend\app\api\routes.py` correctly extracts and registers camera port

### 4. Bot Online/Offline Tracking
- **File**: `E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Frontend\src\hooks\useTelemetry.js`
- **Features**:
  - Track `activeBots` Set to know which bots are sending telemetry
  - Timeout mechanism: Mark bot offline if no telemetry for 10 seconds
  - Console logging for debugging connection state
  - Initialize all bots as offline on startup

### 5. Click-to-Start Video Stream
- **Files**: 
  - `E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Frontend\src\components\StreamViewer.js`
  - `E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Frontend\src\screens\FeedsScreen.js`
- **Feature**: Warden video requires explicit click to start (pauses AI detection while streaming)

### 6. Detection State Display
- **File**: `E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Frontend\src\screens\FeedsScreen.js`
- **Feature**: Color-coded chips showing fire/smoke/clear streak counters (🔥 Fire, 💨 Smoke, ✅ Clear)

### 7. Bot Detail Panel
- **File**: `E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Frontend\src\components\SwarmUI.js`
- **Feature**: Expandable panel showing all bot telemetry (IP, battery, WiFi RSSI, heap, PSRAM)

## 🔍 What Needs Verification

The backend logs show **ONLY** connections from `127.0.0.1` (localhost), which means:

### Probable Root Causes:

1. **Backend Not Listening on Network Interface**
   - Backend may have been started with: `uvicorn app.main:app` (defaults to 127.0.0.1)
   - Or: `uvicorn app.main:app --host 127.0.0.1`
   - **Should be**: `python -m uvicorn app.main:app --host 0.0.0.0 --port 8000`

2. **Warden Device Not Running or Not Connected**
   - Warden may not be powered on
   - Warden may not have connected to Wi-Fi (SSID: "A34")
   - Warden may have failed backend health check
   - Warden may be assigned different IP (not 10.75.11.50)

3. **Network/Firewall Issues**
   - Windows Firewall may be blocking port 8000
   - Network routing between 10.75.11.83 and 10.75.11.50 may be blocked
   - Backend and Warden may be on different subnets

## 📋 Verification Steps

### Step 1: Run Diagnostic Script
```powershell
cd E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE
.\diagnose_connection.ps1
```

This will check:
- ✓ Backend listening on 0.0.0.0 vs 127.0.0.1
- ✓ IP address configuration
- ✓ Firewall status
- ✓ Backend accessibility on localhost
- ✓ Backend accessibility on network IP
- ✓ Warden device reachability

### Step 2: Restart Backend with Correct Command
```powershell
cd E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Backend
python -m uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload
```

**Look for**:
```
INFO:     Uvicorn running on http://0.0.0.0:8000 (Press CTRL+C to quit)
```

NOT:
```
INFO:     Uvicorn running on http://127.0.0.1:8000
```

### Step 3: Check Warden Serial Monitor
```powershell
cd E:\AEGIS_GUARDIAN_FOMO\AEGIS_WARDEN\AEGIS_WARDEN_MASTER
idf.py monitor
```

**Look for**:
```
I (xxxx) WARDEN: Wi-Fi station started, connecting...
I (xxxx) WARDEN: Connected to Wi-Fi, IP: 10.75.11.50
I (xxxx) WARDEN: Backend health check: ONLINE
I (xxxx) WARDEN: Inference: fire 5%, smoke 3%, clear 92%
```

**If you see errors**:
- `Wi-Fi connection failed` → Check SSID/password
- `Backend health check: OFFLINE` → Backend not accessible
- No inference messages → AI not running

### Step 4: Verify Backend Receives Telemetry

**Backend console should show**:
```
INFO:     10.75.11.50:xxxxx - "POST /api/v1/telemetry/ingest HTTP/1.1" 200 OK
[AEGIS] Registered Warden video feed at 10.75.11.50:81
```

**If you only see**:
```
INFO:     127.0.0.1:xxxxx - "GET /api/v1/..." 200 OK
INFO:     127.0.0.1:xxxxx - "WebSocket /api/v1/ws/telemetry" [accepted]
```

→ Backend is NOT receiving telemetry from Warden!

### Step 5: Hard Refresh Frontend
```
Press: Ctrl + Shift + R
```

**Browser console (F12) should show**:
```
[AEGIS] Swarm telemetry socket connected to ws://10.75.11.83:8000/api/v1/ws/telemetry
[AEGIS] WebSocket message received: INGESTED_TELEMETRY Warden
[AEGIS] Received telemetry: Warden (telemetry) {...}
[AEGIS] Active bots: ["warden"]
```

**If you see**:
- Only `TELEMETRY_FRAME` messages → Backend serving mock data only
- No `INGESTED_TELEMETRY` → Warden not sending to backend
- Empty `Active bots: []` → No telemetry received

## 📊 Expected Results When Working

### Backend Logs:
```
INFO:     Uvicorn running on http://0.0.0.0:8000 (Press CTRL+C to quit)
INFO:     10.75.11.50:12345 - "POST /api/v1/telemetry/ingest HTTP/1.1" 200 OK
[AEGIS] Registered Warden video feed at 10.75.11.50:81
INFO:     127.0.0.1:54321 - "WebSocket /api/v1/ws/telemetry" [accepted]
INFO:     connection open
```

### Warden Serial Monitor:
```
I (12345) WARDEN: Connected to Wi-Fi, IP: 10.75.11.50
I (12346) WARDEN: Backend health check: ONLINE
I (12347) WARDEN: Camera stream server started on port 81
I (15000) WARDEN: Inference complete (2937ms): fire=5%, smoke=3%, clear=92%
I (15100) WARDEN: Status telemetry queued
```

### Frontend Console:
```
[AEGIS] Swarm telemetry socket connected to ws://10.75.11.83:8000/api/v1/ws/telemetry
[AEGIS] WebSocket message received: INGESTED_TELEMETRY Warden
[AEGIS] Received telemetry: Warden (telemetry) {
  status: "online",
  battery_pct: 95,
  ip_address: "10.75.11.50",
  camera_port: 81,
  detection_state: { fire_streak: 0, smoke_streak: 0, clear_streak: 5 }
}
[AEGIS] Active bots: ["warden"]
```

### Frontend UI:
- ✅ Warden card shows green "Online" badge
- ✅ IP address displayed: `10.75.11.50`
- ✅ Battery level: `95%`
- ✅ WiFi RSSI: `-64 dBm`
- ✅ Detection state chips visible
- ✅ "Click to Start Live Stream" button (doesn't auto-play)

## 📁 Modified Files Summary

| File | Purpose | Status |
|------|---------|--------|
| `AEGIS-Frontend/src/services/AegisService.js` | Backend URL configuration | ✅ Fixed |
| `AEGIS-Frontend/src/hooks/useTelemetry.js` | Bot tracking & timeout logic | ✅ Implemented |
| `AEGIS-Frontend/src/components/StreamViewer.js` | Click-to-start video | ✅ Implemented |
| `AEGIS-Frontend/src/screens/FeedsScreen.js` | Detection state display | ✅ Implemented |
| `AEGIS-Frontend/src/components/SwarmUI.js` | Bot detail panel | ✅ Implemented |
| `AEGIS_WARDEN/main/warden_main.cpp` | Telemetry sending & camera port | ✅ Implemented |
| `AEGIS-Backend/app/api/routes.py` | Telemetry ingestion | ✅ Already correct |

## 🎯 Next Action Required

**You need to**:
1. Run `diagnose_connection.ps1` to check backend configuration
2. Stop backend if running (Ctrl+C)
3. Restart backend with: `python -m uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload`
4. Verify Warden is powered on and check serial monitor
5. Hard refresh frontend (Ctrl+Shift+R)
6. Share results:
   - Backend startup message (which IP it's listening on)
   - Warden serial monitor (Wi-Fi connection section)
   - Backend logs (look for POST from 10.75.11.50)
   - Frontend console (look for INGESTED_TELEMETRY)

## 📚 Documentation Created

1. **VERIFY_WARDEN_CONNECTION.md** - Detailed troubleshooting guide
2. **diagnose_connection.ps1** - Automated diagnostic script
3. **CURRENT_STATUS.md** - This file (project status summary)
4. **TROUBLESHOOTING_OFFLINE_BOTS.md** - Original debugging guide
5. **FRONTEND_IMPROVEMENTS_SUMMARY.md** - Complete feature documentation
6. **WARDEN_STREAM_FEATURE.md** - Video streaming documentation

---

**Last Updated**: 2026-08-15  
**Status**: Awaiting backend restart verification
