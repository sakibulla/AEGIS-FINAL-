# AEGIS Multi-Robot System - Backend Configuration Summary

**Date**: August 15, 2026  
**Backend Server**: FastAPI at http://192.168.1.100:8000  
**Wi-Fi Network**: SSID "Mateen"

---

## 🎯 Overview

All three AEGIS robots are now configured to communicate with your centralized backend server:

| Robot | Bot ID | Location | Status |
|-------|--------|----------|--------|
| **Guardian** | Guardian | `AEGIS_GURDIAN\AEGIS_GURDIAN_MASTER_FOMO` | ✅ Updated |
| **Pathfinder** | Pathfinder | `AEGIS_PATHFINDER\aegis_pathfinder_master` | ✅ Updated |
| **Warden** | Warden | `AEGIS_WARDEN\AEGIS_WARDEN_MASTER` | ✅ Updated |

---

## 📡 Network Configuration

### Common Settings (All Bots)

```cpp
Backend URL:    "http://192.168.1.100:8000"
Wi-Fi SSID:     "Mateen"
Wi-Fi Password: "12345678"
Max Retries:    10
```

### Individual Bot IDs

Each bot identifies itself to the backend:
- Guardian: `"Guardian"`
- Pathfinder: `"Pathfinder"`
- Warden: `"Warden"`

---

## 🤖 Robot Capabilities

### 1. Guardian (Person Detection & Security)

**Hardware**: ESP32-S3-EYE + OV2640 Camera  
**AI Model**: Edge Impulse FOMO (Person Detection)  
**Primary Function**: Security monitoring, person detection, voice activation

**Backend Communications**:
- ✅ Status Telemetry (Periodic)
- ✅ Vision Detection Events (Person detected)
- ✅ Incident Reports (Intruder alerts)
- ✅ Scenario Fetching
- ✅ ESP-NOW Master (Commands to slaves)

**Configuration File**: 
```
e:\AEGIS_GUARDIAN_FOMO\AEGIS_GURDIAN\AEGIS_GURDIAN_MASTER_FOMO\main\main.cpp
Lines 93-107 (Backend & Wi-Fi Configuration)
```

---

### 2. Pathfinder (Mapping & Navigation)

**Hardware**: ESP32-S3 + VL53L5CX ToF Sensor + Camera  
**AI Model**: Edge Impulse FOMO (Object Detection)  
**Primary Function**: Autonomous mapping, obstacle detection, door detection

**Backend Communications**:
- ✅ Status Telemetry (Periodic with battery, position)
- ✅ Map Packets (2D spatial data with ToF readings)
- ✅ Vision Detection Events (Objects, doors)
- ✅ Incident Reports (Blocked path, navigation issues)
- ✅ Scenario Fetching
- ✅ ESP-NOW Mesh Communication

**Configuration File**:
```
e:\AEGIS_GUARDIAN_FOMO\AEGIS_PATHFINDER\aegis_pathfinder_master\main\main.cpp
Lines 34-46 (Backend & Wi-Fi Configuration)
```

---

### 3. Warden (Fire & Smoke Detection)

**Hardware**: ESP32-S3-EYE + OV2640 Camera  
**AI Model**: Edge Impulse FOMO (Fire & Smoke Detection)  
**Primary Function**: Fire safety monitoring, smoke detection

**Backend Communications**:
- ✅ Status Telemetry (Periodic with detection state)
- ✅ Vision Detection Events (Fire/smoke with confidence)
- ✅ Incident Reports (Fire, smoke, system clear)
- ✅ Scenario Fetching
- ✅ ESP-NOW Transmitter (Alert commands to slave)

**Configuration Files**:
```
e:\AEGIS_GUARDIAN_FOMO\AEGIS_WARDEN\AEGIS_WARDEN_MASTER\main\warden_backend.h
Lines 10-23 (Backend & Wi-Fi Configuration)
```

---

## 📊 Backend API Endpoints

All robots use these standardized endpoints:

| Endpoint | Method | Purpose | Usage |
|----------|--------|---------|-------|
| `/` | GET | Health check | Startup validation |
| `/api/v1/telemetry/ingest` | POST | Send telemetry | Status, vision, map data |
| `/api/v1/incidents/report` | POST | Report incidents | Security/safety alerts |
| `/api/v1/test/current-scenario` | GET | Fetch scenario | Swarm coordination |

---

## 📦 Data Formats

### Status Telemetry (All Bots)

```json
{
  "bot_id": "Guardian|Pathfinder|Warden",
  "kind": "telemetry",
  "payload": {
    "status": "IDLE|PATROLLING|MAPPING|FIRE|CLEAR",
    "system_info": {
      "free_heap": 123456,
      "free_psram": 456789
    },
    "wifi_rssi": -52,
    "ip_address": "192.168.1.xxx",
    "timestamp": "uptime_120s",
    "battery_pct": 85  // Pathfinder only
  }
}
```

### Vision Detection (Guardian, Pathfinder, Warden)

```json
{
  "bot_id": "Guardian|Pathfinder|Warden",
  "kind": "vision_detection",
  "payload": {
    "label": "person|door|fire|smoke",
    "confidence": 0.94,
    "is_threat": true,
    "bbox": {
      "x": 100,
      "y": 80,
      "w": 120,
      "h": 200
    }
  }
}
```

### Map Packet (Pathfinder Only)

```json
{
  "bot_id": "Pathfinder",
  "kind": "map_packet",
  "payload": {
    "total_snaps": 20,
    "snap_index": 5,
    "position": {"x": 1.5, "y": 2.3},
    "distances_mm": [500, 520, 540, ...],
    "has_door": true
  }
}
```

### Incident Report (All Bots)

```json
{
  "bot_id": "Guardian|Pathfinder|Warden",
  "type": "INTRUDER_DETECTED|FIRE_DETECTED|PATH_BLOCKED",
  "message": "Person detected in restricted area",
  "severity": "CRITICAL|HIGH|MEDIUM|LOW",
  "context": {
    "confidence": 0.94,
    "location": "Zone A"
  }
}
```

---

## 🏗️ System Architecture

```
                     AEGIS BACKEND SERVER
                   (192.168.1.100:8000)
                  ┌─────────────────────┐
                  │   FastAPI + Uvicorn │
                  │   WebSocket Server  │
                  │   SQLite Database   │
                  └──────────┬──────────┘
                             │
         ┌───────────────────┼───────────────────┐
         │                   │                   │
    ┌────▼────┐         ┌────▼────┐         ┌───▼─────┐
    │ Guardian│         │Pathfinder│        │ Warden  │
    │         │         │          │        │         │
    │ Person  │         │ Mapping  │        │  Fire   │
    │Detection│         │Navigation│        │ Detection│
    └────┬────┘         └────┬─────┘        └────┬────┘
         │                   │                    │
         │ ESP-NOW           │ ESP-NOW            │ ESP-NOW
         ▼                   ▼                    ▼
    [Slave(s)]          [Mesh Nodes]         [Slave]
```

### Communication Flow

1. **All Bots → Backend**: HTTP POST/GET (Telemetry, Incidents)
2. **Backend → Dashboard**: WebSocket (Real-time updates)
3. **Guardian ↔ Slaves**: ESP-NOW (Commands)
4. **Pathfinder ↔ Mesh**: ESP-NOW (Coordination)
5. **Warden → Slave**: ESP-NOW (Alert commands)

---

## 🔨 Build & Flash Instructions

### Guardian

```bash
cd e:\AEGIS_GUARDIAN_FOMO\AEGIS_GURDIAN\AEGIS_GURDIAN_MASTER_FOMO
idf.py build
idf.py flash monitor
```

### Pathfinder

```bash
cd e:\AEGIS_GUARDIAN_FOMO\AEGIS_PATHFINDER\aegis_pathfinder_master
idf.py build
idf.py flash monitor
```

### Warden

```bash
cd e:\AEGIS_GUARDIAN_FOMO\AEGIS_WARDEN\AEGIS_WARDEN_MASTER
idf.py build
idf.py flash monitor
```

---

## 🚀 Backend Startup

### Start the Backend Server

```bash
cd e:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Backend
start_backend.bat
```

**Server will run on**:
- HTTP API: http://192.168.1.100:8000
- WebSocket: ws://192.168.1.100:8000/api/v1/ws/telemetry
- API Docs: http://192.168.1.100:8000/docs
- ReDoc: http://192.168.1.100:8000/redoc

---

## 📋 Verification Checklist

### Pre-Flight Checks

- [ ] Backend server running and accessible
- [ ] Wi-Fi router "Mateen" is online
- [ ] All ESP32-S3 devices have firmware flashed
- [ ] USB cables connected for serial monitoring

### Guardian Checks

- [ ] Wi-Fi connected (SSID: Mateen)
- [ ] Backend health check: ONLINE
- [ ] Person detection working
- [ ] ESP-NOW slave communication working
- [ ] Telemetry appearing in backend logs

### Pathfinder Checks

- [ ] Wi-Fi connected (SSID: Mateen)
- [ ] Backend health check: ONLINE
- [ ] ToF sensor readings valid
- [ ] Vision detection working
- [ ] Map packets sending
- [ ] ESP-NOW mesh communication working

### Warden Checks

- [ ] Wi-Fi connected (SSID: Mateen)
- [ ] Backend health check: ONLINE
- [ ] Camera capturing frames
- [ ] Fire/smoke detection working
- [ ] ESP-NOW slave communication working
- [ ] Incidents reporting properly

---

## 🔍 Monitoring

### Expected Serial Monitor Output

**Guardian**:
```
[GUARDIAN] Connected to Wi-Fi, IP: 192.168.1.101
[GUARDIAN] Backend health check: ONLINE
[GUARDIAN] Active Backend Swarm Scenario: patrol_mode
[GUARDIAN] Telemetry background task started CPU0
[GUARDIAN] Guardian Master READY
```

**Pathfinder**:
```
[PATHFINDER] Connected to Wi-Fi, IP: 192.168.1.102
[PATHFINDER] Backend health check: ONLINE
[PATHFINDER] Telemetry Task started (Core 0)
[PATHFINDER] Pathfinder Master READY - MAPPING
```

**Warden**:
```
[BACKEND] Connected to Wi-Fi, IP: 192.168.1.103
[BACKEND] Backend health check: ONLINE
[BACKEND] Active scenario: fire_drill_scenario
[BACKEND] Telemetry task started
[WARDEN] Warden Master READY
[WARDEN] Watching for FIRE / SMOKE...
```

### Backend Server Logs

```
INFO:     Started server process
INFO:     Uvicorn running on http://0.0.0.0:8000
POST /api/v1/telemetry/ingest - 200 OK (Guardian status update)
POST /api/v1/telemetry/ingest - 200 OK (Pathfinder map packet)
POST /api/v1/telemetry/ingest - 200 OK (Warden vision detection)
POST /api/v1/incidents/report - 200 OK (Guardian intruder alert)
```

---

## 🛠️ Troubleshooting

### Wi-Fi Connection Failed

**Problem**: `Failed to connect to SSID: Mateen`

**Solutions**:
1. Verify Wi-Fi password: `12345678`
2. Check router is on 2.4 GHz (ESP32 doesn't support 5 GHz)
3. Ensure router is broadcasting SSID
4. Check signal strength (move closer to router)
5. Verify router isn't at max device limit

### Backend Unreachable

**Problem**: `Backend health check: OFFLINE`

**Solutions**:
1. Verify backend is running:
   ```bash
   curl http://192.168.1.100:8000/
   ```
2. Check IP address is correct (use `ipconfig` on Windows)
3. Ensure firewall allows port 8000
4. Verify backend virtual environment is activated
5. Check backend logs for errors

### No Telemetry Appearing

**Problem**: Bots connected but no data in backend

**Solutions**:
1. Check backend logs for POST requests
2. Verify bot is calling telemetry functions
3. Check telemetry queue status in serial monitor
4. Ensure backend endpoints are correct
5. Verify JSON payload format is valid

### ESP-NOW Communication Failed

**Problem**: `ESP-NOW send failed`

**Solutions**:
1. Verify slave MAC addresses are correct
2. Check Wi-Fi channel compatibility
3. Ensure ESP-NOW is initialized after Wi-Fi
4. Check distance between master and slave
5. Verify slave devices are powered on

---

## 📊 Performance Expectations

### Network Performance

| Metric | Expected Value |
|--------|---------------|
| Wi-Fi Connection Time | < 5 seconds |
| Backend Health Check | < 500 ms |
| Telemetry POST | < 1 second |
| Status Update Interval | 3 seconds (Guardian/Warden), 5 seconds (Pathfinder) |
| ESP-NOW Latency | < 10 ms |

### Resource Usage (Per Bot)

| Resource | Guardian | Pathfinder | Warden |
|----------|----------|------------|--------|
| Free Heap | ~100 KB | ~150 KB | ~120 KB |
| Free PSRAM | ~6 MB | ~6 MB | ~6 MB |
| HTTP Buffers | ~4 KB | ~4 KB | ~4 KB |
| Telemetry Queue | ~4 KB | ~4 KB | ~4 KB |
| Task Stack (AI) | 16 KB | 16 KB | 16 KB |
| Task Stack (Telemetry) | 4 KB | 4 KB | 4 KB |

---

## 🎯 Testing Scenarios

### Scenario 1: Individual Bot Testing

1. Start backend server
2. Flash and power one bot at a time
3. Monitor serial output for connection
4. Verify telemetry in backend logs
5. Test specific bot functionality (detection/mapping)

### Scenario 2: Multi-Bot Coordination

1. Start backend server
2. Power all three bots simultaneously
3. Verify all bots connect to Wi-Fi
4. Check backend receives telemetry from all bots
5. Monitor WebSocket for real-time updates
6. Test scenario commands from backend

### Scenario 3: Network Recovery

1. Start all bots with backend running
2. Stop backend server
3. Verify bots continue operating (ESP-NOW, AI)
4. Restart backend server
5. Verify bots reconnect automatically
6. Check telemetry resumes

### Scenario 4: Incident Response

1. Trigger person detection on Guardian
2. Verify incident report sent to backend
3. Trigger fire detection on Warden
4. Verify critical incident reported
5. Check incident timeline in backend
6. Verify ESP-NOW commands sent to slaves

---

## 🔐 Security Considerations

### Current Implementation

- ✅ WPA2-PSK Wi-Fi encryption
- ✅ Local network only (no internet exposure)
- ✅ HTTP (not HTTPS - suitable for local testing)
- ✅ No authentication tokens (suitable for development)
- ✅ ESP-NOW unencrypted (fast, low-latency)

### Future Production Recommendations

- 🔲 HTTPS with SSL/TLS certificates
- 🔲 JWT authentication tokens for backend API
- 🔲 ESP-NOW encryption enabled
- 🔲 Network isolation/VLAN for robot network
- 🔲 Rate limiting on backend endpoints
- 🔲 Input validation and sanitization
- 🔲 Secure OTA firmware updates

---

## 📈 Future Enhancements

### Planned Features

1. **Dashboard Integration**
   - Real-time 3D map visualization (Pathfinder data)
   - Live camera streams from all bots
   - Incident timeline and alerts
   - Bot health monitoring

2. **Swarm Coordination**
   - Multi-bot patrol patterns
   - Coordinated threat response
   - Dynamic task allocation
   - Collision avoidance

3. **Advanced AI**
   - Improved person identification
   - Gesture recognition
   - Fire prediction (before ignition)
   - Autonomous decision-making

4. **Data Persistence**
   - Historical telemetry storage
   - Incident reports database
   - Map data accumulation
   - Performance analytics

---

## 📞 Support & Documentation

### Configuration Files

- **Guardian**: `AEGIS_GURDIAN_MASTER_FOMO/main/main.cpp` (Lines 93-107)
- **Pathfinder**: `aegis_pathfinder_master/main/main.cpp` (Lines 34-46)
- **Warden**: `AEGIS_WARDEN_MASTER/main/warden_backend.h` (Lines 10-23)

### Additional Documentation

- **Warden Integration**: `AEGIS_WARDEN_MASTER/BACKEND_INTEGRATION.md`
- **Warden Update Summary**: `AEGIS_WARDEN_MASTER/BACKEND_UPDATE_SUMMARY.md`
- **Backend API**: Visit http://192.168.1.100:8000/docs when running

---

## ✅ Configuration Status

| Component | Status | Notes |
|-----------|--------|-------|
| Guardian Backend Config | ✅ Complete | Updated to 192.168.1.100:8000 |
| Pathfinder Backend Config | ✅ Complete | Updated to 192.168.1.100:8000 |
| Warden Backend Config | ✅ Complete | Updated to 192.168.1.100:8000 |
| Wi-Fi Credentials | ✅ Complete | All set to SSID "Mateen" |
| Backend Server | ✅ Ready | Located in AEGIS-Backend folder |
| Documentation | ✅ Complete | This file + individual docs |

---

**Ready to Deploy**: All three robots are now configured and ready to connect to your backend server! 🚀

**Next Step**: Build, flash, and power on the robots, then start the backend server to see them all communicate in real-time.

---

**Author**: AEGIS Development Team  
**Version**: 1.0  
**Last Updated**: August 15, 2026

