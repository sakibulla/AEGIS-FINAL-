# AEGIS Warden Master - Backend Readiness Report

**Generated**: 2026-08-15  
**Status**: ⚠️ **NEEDS FIXES BEFORE BUILD**

---

## Executive Summary

The AEGIS Warden Master has backend integration code in place, but there are **critical issues** that must be resolved before building and deployment:

### Issues Found:
1. ✅ **FIXED**: Partition table misconfiguration  
2. ✅ **FIXED**: Wi-Fi credentials updated to match network  
3. ✅ **FIXED**: Backend URL updated to correct server
4. ✅ **FIXED**: warden_backend.cpp added to CMakeLists.txt build  
5. ⚠️ **CRITICAL**: Duplicate backend code in warden_main.cpp (conflicts with warden_backend module)

---

## Fixed Issues

### 1. Partition Table Configuration ✅

**Problem**: sdkconfig was using default `partitions_singleapp.csv` (1 MB app partition), but the binary is 1.08 MB.

**Solution Applied**:
```
Changed: CONFIG_PARTITION_TABLE_SINGLE_APP=y
To:      CONFIG_PARTITION_TABLE_CUSTOM=y
         CONFIG_PARTITION_TABLE_FILENAME="partitions.csv"
```

The custom `partitions.csv` allocates 3 MB for the factory partition, which is sufficient.

### 2. Wi-Fi Credentials ✅

**Updated in**: `main/warden_backend.h`

```cpp
#define WIFI_SSID "A34"
#define WIFI_PASSWORD "01234567"
```

### 3. Backend Server URL ✅

**Updated in**: `main/warden_backend.h`

```cpp
#define BACKEND_SERVER_URL "http://10.75.11.83:8000"
```

### 4. CMakeLists.txt Build Configuration ✅

**Updated**: Added `warden_backend.cpp` to build sources

```cmake
idf_component_register(
    SRCS
        "warden_main.cpp"
        "warden_backend.cpp"  # <- ADDED
        ${EI_SOURCES}
    ...
)
```

---

## Critical Issue: Duplicate Backend Code

### Problem Description

The project has **two separate backend implementations**:

1. **Modular Implementation** (Correct):
   - `main/warden_backend.h` - Header file with clean API
   - `main/warden_backend.cpp` - Separate implementation module
   - Clean separation of concerns
   - Proper encapsulation

2. **Embedded Implementation** (Incorrect):
   - `main/warden_main.cpp` contains **duplicate** backend code
   - Lines 38-577 have all backend functionality embedded
   - Conflicts with the modular implementation
   - Same function names cause linker errors

### Current State

**warden_main.cpp** contains:
- Backend configuration macros (lines 38-60)
- Backend data type definitions (lines 62-127)
- Wi-Fi event handlers (lines 180-230)
- HTTP helper functions (lines 260-380)
- Telemetry senders (lines 390-520)
- Telemetry task (lines 510-540)
- Backend queue functions (lines 520-575)
- Backend initialization (lines 578-635)

**This duplicates everything in warden_backend.cpp!**

### Required Fix

You need to **remove the duplicate backend code** from `warden_main.cpp` and use the clean modular interface instead.

#### Step 1: Remove Lines from warden_main.cpp

Delete lines **38-635** (all backend implementation)

#### Step 2: Keep Only the Include

```cpp
#include "warden_backend.h"
```

#### Step 3: Use the Clean API

The existing calls in warden_main.cpp already use the correct API:
- `backend_init()` - Initialize backend
- `backend_wifi_connected()` - Check Wi-Fi status  
- `backend_is_online()` - Check backend health
- `queue_status_telemetry()` - Queue status updates
- `queue_vision_detection()` - Queue vision detections
- `queue_incident_report()` - Queue incident reports

#### Step 4: Remove Helper Function Declarations

Delete these forward declarations (lines 172-177):
```cpp
static void send_status_to_backend(...);
static void send_vision_to_backend(...);
static void send_incident_to_backend(...);
static bool backend_wifi_connected(void);
static esp_err_t queue_status_telemetry(...);
static esp_err_t queue_vision_detection(...);
static esp_err_t queue_incident_report(...);
```

These are all provided by `warden_backend.h` now.

---

## Backend Architecture

### Correct Implementation (warden_backend module)

```
┌─────────────────────────────────────┐
│      warden_main.cpp (Core 1)       │
│  ┌───────────────────────────────┐  │
│  │  • Camera capture             │  │
│  │  • Edge Impulse inference     │  │
│  │  • Fire/smoke detection       │  │
│  │  • ESP-NOW transmission       │  │
│  └───────────────────────────────┘  │
│            ▼ (non-blocking)          │
│     queue_status_telemetry()         │
│     queue_vision_detection()         │
│     queue_incident_report()          │
└─────────────────────────────────────┘
                 │
                 ▼ FreeRTOS Queue
┌─────────────────────────────────────┐
│    warden_backend.cpp (Core 0)      │
│  ┌───────────────────────────────┐  │
│  │  • Telemetry task             │  │
│  │  • Wi-Fi management           │  │
│  │  • HTTP client operations     │  │
│  │  • cJSON serialization        │  │
│  └───────────────────────────────┘  │
│            ▼                         │
│    POST /api/v1/telemetry/ingest     │
│    POST /api/v1/incidents/report     │
└─────────────────────────────────────┘
                 │
                 ▼
    ┌────────────────────────┐
    │  AEGIS Backend Server  │
    │   10.75.11.83:8000     │
    └────────────────────────┘
```

### Key Benefits

✅ **Non-blocking**: AI task never waits for network  
✅ **Fault-tolerant**: System continues if backend offline  
✅ **Multi-core**: Backend on Core 0, AI on Core 1  
✅ **Clean API**: Simple queue-based interface  
✅ **Maintainable**: Separate concerns  

---

## Backend Communication Endpoints

### 1. Status Telemetry (Every 3s)

**POST** `/api/v1/telemetry/ingest`

```json
{
  "bot_id": "Warden",
  "kind": "telemetry",
  "payload": {
    "status": "CLEAR|FIRE|SMOKE|FIRE_SMOKE",
    "system_info": {
      "free_heap": 150000,
      "free_psram": 4000000
    },
    "wifi_rssi": -45,
    "ip_address": "10.75.11.120",
    "timestamp": "uptime_125s",
    "detection_state": {
      "fire_streak": 0,
      "smoke_streak": 0,
      "clear_streak": 10
    }
  }
}
```

### 2. Vision Detection

**POST** `/api/v1/telemetry/ingest`

```json
{
  "bot_id": "Warden",
  "kind": "vision_detection",
  "payload": {
    "label": "fire",
    "confidence": 0.92,
    "is_threat": true,
    "bbox": { "x": 120, "y": 80, "w": 150, "h": 200 },
    "timing": { "total_ms": 245 }
  }
}
```

### 3. Incident Reports

**POST** `/api/v1/incidents/report`

```json
{
  "bot_id": "Warden",
  "type": "FIRE_DETECTED",
  "message": "Fire detected by vision system",
  "severity": "CRITICAL",
  "context": {
    "fire_confidence": 0.92,
    "smoke_confidence": 0.15
  }
}
```

---

## Build Instructions

### Before Building

⚠️ **YOU MUST FIX THE DUPLICATE CODE ISSUE FIRST!**

### After Fixing

1. **Clean previous build** (recommended):
   ```bash
   idf.py fullclean
   ```

2. **Build the project**:
   ```bash
   idf.py build
   ```

3. **Flash to ESP32-S3**:
   ```bash
   idf.py flash monitor
   ```

### Expected Build Output

✅ Successful compilation without errors  
✅ Binary size: ~1.08 MB (fits in 3 MB partition)  
✅ No linker conflicts  
✅ Clean build log  

---

## Dependencies

All dependencies are included in ESP-IDF 5.5+:

✅ `esp_wifi` - Wi-Fi station mode  
✅ `esp_http_client` - HTTP REST client  
✅ `esp_netif` - Network interface  
✅ `json` (cJSON) - JSON serialization  
✅ `nvs_flash` - Non-volatile storage  
✅ `esp_timer` - Uptime tracking  
✅ `esp32-camera` - OV2640 camera driver  
✅ `esp_now` - ESP-NOW protocol  

---

## Testing Checklist

After building and flashing:

### 1. Wi-Fi Connection
```
[BACKEND] Wi-Fi station started, connecting...
[BACKEND] Connected to Wi-Fi, IP: 10.75.11.XXX
[BACKEND] Connected to SSID: A34
```

### 2. Backend Health Check
```
[BACKEND] GET http://10.75.11.83:8000/ -> 200
[BACKEND] Backend health check: ONLINE
```

### 3. Telemetry Task Started
```
[BACKEND] Telemetry task started
```

### 4. Warden Ready
```
[WARDEN] Warden Master READY
[WARDEN] Watching for FIRE / SMOKE...
```

### 5. Periodic Status
```
[BACKEND] POST http://10.75.11.83:8000/api/v1/telemetry/ingest -> 200
```

### 6. Detection Events
```
[WARDEN] FIRE detected (conf=0.92)
[BACKEND] POST http://10.75.11.83:8000/api/v1/telemetry/ingest -> 200
[BACKEND] POST http://10.75.11.83:8000/api/v1/incidents/report -> 200
```

---

## Network Configuration

### Current Settings

- **SSID**: A34
- **Password**: 01234567
- **Backend**: http://10.75.11.83:8000
- **Bot ID**: Warden

### Wi-Fi Channel Compatibility

⚠️ **IMPORTANT**: ESP-NOW will use the same Wi-Fi channel as the AP connection.

- Warden connects to AP "A34"
- ESP-NOW peer (slave) must be on a compatible channel
- No fixed channel configuration needed
- Automatic channel selection based on AP

### ESP-NOW Slave Configuration

In `main/warden_config.h`:

```cpp
#define WARDEN_USE_BROADCAST 0
#define WARDEN_SLAVE_MAC { 0x00, 0x70, 0x07, 0x7E, 0xD8, 0x14 }
```

The slave device should be configured to work with your AP's channel.

---

## Backend Dashboard Integration

The AEGIS backend (http://10.75.11.83:8000) can now monitor:

✅ **Real-time Status**
- Warden online/offline
- Current detection state (CLEAR/FIRE/SMOKE)
- Detection confidence levels
- Fire/smoke/clear streaks

✅ **System Health**
- Free heap memory
- Free PSRAM
- Wi-Fi signal strength (RSSI)
- IP address
- Uptime

✅ **Vision Detections**
- Fire/smoke labels
- Confidence scores
- Bounding box coordinates
- Inference timing

✅ **Incident Timeline**
- Fire detected events
- Smoke detected events
- System clear events
- Severity levels

---

## Multi-Robot Coordination

All three robots now communicate with the same backend:

```
AEGIS Backend (10.75.11.83:8000)
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

Each robot is uniquely identified by its `bot_id` field.

---

## Known Limitations

### 1. Memory Constraints
- Telemetry queue: 20 messages max
- HTTP buffers: 2 KB response limit
- cJSON heap allocations during serialization

### 2. Network Reliability
- 10 retry attempts for Wi-Fi connection
- 5-second HTTP request timeout
- No persistent storage for failed sends

### 3. Detection State Machine
- 3 consecutive frames required for alert
- 8 consecutive clear frames required to clear
- Incidents reported only on state transitions

---

## Recommended Next Steps

### 1. Fix Duplicate Code (CRITICAL)
Remove embedded backend code from warden_main.cpp as described above.

### 2. Build and Test
```bash
idf.py fullclean
idf.py build
idf.py flash monitor
```

### 3. Verify Backend Connectivity
- Check Wi-Fi connection logs
- Verify backend health check succeeds
- Monitor telemetry POST requests

### 4. Test Detection Flow
- Trigger fire/smoke detections
- Verify vision telemetry is sent
- Confirm incidents are reported
- Check backend dashboard

### 5. Multi-Robot Integration
- Ensure all three robots are online
- Verify unique bot_id identification
- Test coordinated response scenarios

---

## Support Files

### Configuration Files
- `main/warden_config.h` - Detection thresholds, ESP-NOW config
- `main/warden_protocol.h` - ESP-NOW packet format
- `main/warden_backend.h` - Backend API and configuration
- `sdkconfig` - ESP-IDF project configuration
- `partitions.csv` - Flash partition table

### Implementation Files
- `main/warden_main.cpp` - AI inference and ESP-NOW (needs fix)
- `main/warden_backend.cpp` - Backend communication module
- `main/CMakeLists.txt` - Build configuration

### Documentation
- `BACKEND_INTEGRATION.md` - Detailed integration guide
- `BACKEND_UPDATE_SUMMARY.md` - Integration summary
- `README.md` - Project overview

---

## Summary

### ✅ Ready Components
- Partition table configured (3 MB app partition)
- Wi-Fi credentials updated (A34 network)
- Backend URL configured (10.75.11.83:8000)
- Modular backend implementation complete
- CMakeLists.txt updated with backend sources
- All dependencies available

### ⚠️ Blocking Issues
- **Duplicate backend code** in warden_main.cpp must be removed
- Potential linker conflicts due to duplicate symbols
- Code maintenance nightmare with two implementations

### 🎯 Action Required
1. Remove lines 38-635 from `main/warden_main.cpp`
2. Keep only `#include "warden_backend.h"`
3. Remove duplicate forward declarations
4. Clean build: `idf.py fullclean`
5. Build: `idf.py build`
6. Flash and test: `idf.py flash monitor`

---

**Once the duplicate code issue is resolved, the Warden Master will be fully ready for backend integration and deployment.**

**Report Generated**: 2026-08-15  
**Engineer**: Kiro AI Assistant  
**Project**: AEGIS Multi-Robot System
