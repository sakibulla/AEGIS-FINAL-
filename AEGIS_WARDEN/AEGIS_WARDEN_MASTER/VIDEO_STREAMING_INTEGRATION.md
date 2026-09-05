# Warden Camera Streaming & Backend Integration Guide

## Overview

Your Warden now has **dual camera capabilities**:
1. **Local HTTP Stream** - Direct MJPEG stream on port 81
2. **Backend Integration** - Telemetry with camera URL registration

## Architecture

```
┌─────────────────────────────────────────┐
│   WARDEN ESP32-S3 (10.75.11.XXX)       │
│                                         │
│  Port 81: HTTP Camera Server            │
│  ├─ /stream        (MJPEG video)        │
│  ├─ /capture       (JPEG snapshot)      │
│  ├─ /status        (Camera JSON)        │
│  └─ /              (HTML viewer)        │
│                                         │
│  Main Task (Core 1):                    │
│  └─ Edge Impulse AI inference           │
│     └─ Fire/Smoke detection             │
│        └─ Queue telemetry               │
│                                         │
│  Telemetry Task (Core 0):               │
│  └─ Send to Backend (10.75.11.83:8000) │
└─────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────┐
│  AEGIS Backend (10.75.11.83:8000)       │
│                                         │
│  Endpoints:                             │
│  ├─ POST /api/v1/telemetry/ingest       │
│  ├─ POST /api/v1/incidents/report       │
│  ├─ GET  /api/v1/bots/Warden            │
│  ├─ GET  /api/v1/video/stream/Warden    │
│  └─ GET  /api/v1/video/snapshot/Warden  │
│                                         │
│  Frontend Dashboard:                    │
│  └─ Live Warden camera view             │
│  └─ Fire/Smoke detection overlays       │
│  └─ Real-time telemetry display         │
└─────────────────────────────────────────┘
```

---

## How to Access Warden Camera

### Method 1: Direct Stream (Fastest)

**Access the Warden camera directly without going through backend:**

1. **Find Warden's IP Address** from serial monitor:
   ```
   [BACKEND] Connected to Wi-Fi, IP: 10.75.11.120
   [CAM_STREAM] Stream URL: http://10.75.11.120:81/stream
   ```

2. **Open in browser:**
   ```
   http://10.75.11.120:81/
   ```
   This shows a simple HTML page with embedded video.

3. **Direct stream URL:**
   ```
   http://10.75.11.120:81/stream
   ```
   Use this for VLC, OpenCV, or custom players.

4. **Snapshot URL:**
   ```
   http://10.75.11.120:81/capture
   ```
   Returns a single JPEG image.

5. **Camera status:**
   ```
   http://10.75.11.120:81/status
   ```
   Returns JSON with camera settings.

### Method 2: Through Backend (Integrated)

**Access camera through AEGIS backend dashboard:**

The backend will automatically discover Warden's IP from telemetry and proxy the video stream:

```
http://10.75.11.83:8000/api/v1/video/stream/Warden
```

Your frontend can display all three robots:
- Guardian camera: `/api/v1/video/stream/Guardian`
- Pathfinder camera: `/api/v1/video/stream/Pathfinder`
- Warden camera: `/api/v1/video/stream/Warden`

---

## Testing Camera Streaming

### 1. Test Direct Camera Access

After flashing Warden, open serial monitor and look for:

```
[CAM_STREAM] Starting camera stream server on port 81
[CAM_STREAM] Camera stream available at: http://10.75.11.XXX:81/stream
[CAM_STREAM] Snapshot endpoint: http://10.75.11.XXX:81/capture
[CAM_STREAM] Status endpoint: http://10.75.11.XXX:81/status
```

**Test in browser:**
```
http://10.75.11.XXX:81/
```

You should see live video from the OV2640 camera.

### 2. Test with VLC Media Player

1. Open VLC
2. Media → Open Network Stream
3. Enter: `http://10.75.11.XXX:81/stream`
4. Click Play

### 3. Test with Python/OpenCV

```python
import cv2

stream_url = "http://10.75.11.120:81/stream"
cap = cv2.VideoCapture(stream_url)

while True:
    ret, frame = cap.read()
    if ret:
        cv2.imshow('Warden Camera', frame)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
```

### 4. Test with curl (Snapshot)

```bash
curl http://10.75.11.120:81/capture -o snapshot.jpg
```

---

## Monitoring Backend Data

### Check What Warden Sends to Backend

**Method 1: Backend Serial Output**

When you run the backend (`start_backend.bat`), you'll see:

```
[AEGIS Telemetry] Ingest: Warden - telemetry
[AEGIS Telemetry] Ingest: Warden - vision_detection
[AEGIS] Emergency dispatch simulated: {...}
```

**Method 2: Backend API Queries**

Open a browser or use curl:

```bash
# Get Warden status
curl http://10.75.11.83:8000/api/v1/bots/Warden

# Get all bots
curl http://10.75.11.83:8000/api/v1/bots

# Get Warden detections
curl http://10.75.11.83:8000/api/v1/bots/Warden/detections

# Get incidents
curl http://10.75.11.83:8000/api/v1/incidents
```

**Method 3: Live WebSocket Monitoring**

Your backend has a WebSocket endpoint:
```
ws://10.75.11.83:8000/api/v1/ws/telemetry
```

Use a WebSocket client to see real-time data.

---

## What Data is Sent to Backend

### 1. Status Telemetry (Every 3 seconds)

```json
{
  "bot_id": "Warden",
  "kind": "telemetry",
  "payload": {
    "status": "CLEAR",
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

**Backend receives this as:**
```
POST http://10.75.11.83:8000/api/v1/telemetry/ingest
```

### 2. Vision Detection (When fire/smoke detected)

```json
{
  "bot_id": "Warden",
  "kind": "vision_detection",
  "payload": {
    "label": "fire",
    "confidence": 0.92,
    "is_threat": true,
    "bbox": {
      "x": 120,
      "y": 80,
      "w": 150,
      "h": 200
    },
    "timing": {
      "total_ms": 245
    }
  }
}
```

### 3. Incident Reports (State transitions)

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

**Backend receives this as:**
```
POST http://10.75.11.83:8000/api/v1/incidents/report
```

---

## Backend API Endpoints for Warden

### Get Warden Status
```bash
curl http://10.75.11.83:8000/api/v1/bots/Warden
```

**Response:**
```json
{
  "bot": {
    "bot_id": "Warden",
    "status": "CLEAR",
    "battery_pct": 76,
    "system_info": {
      "free_heap": 150000,
      "free_psram": 4000000
    },
    "wifi_rssi": -45,
    "ip_address": "10.75.11.120",
    "vision_detections": [],
    "hazard_data": {
      "fire_detected": false,
      "smoke_detected": false
    }
  },
  "video": {
    "stream_url": "http://10.75.11.120:81/stream",
    "snapshot_url": "http://10.75.11.120:81/capture",
    "status": "online"
  },
  "timestamp": "2026-08-15T12:34:56Z"
}
```

### Get Warden Camera Stream
```bash
# Through backend (proxied)
curl http://10.75.11.83:8000/api/v1/video/stream/Warden

# Or direct (faster)
curl http://10.75.11.120:81/stream
```

### Get Warden Snapshot
```bash
# Through backend
curl http://10.75.11.83:8000/api/v1/video/snapshot/Warden -o warden_snapshot.jpg

# Or direct
curl http://10.75.11.120:81/capture -o warden_snapshot.jpg
```

### Get All Incidents
```bash
curl http://10.75.11.83:8000/api/v1/incidents
```

**Response includes Warden incidents:**
```json
[
  {
    "id": "INC-WAR-20260815123456",
    "title": "Fire Detected Alert from Warden",
    "severity": "CRITICAL",
    "type": "FIRE_DETECTED",
    "message": "Fire detected by vision system",
    "bot_id": "Warden",
    "timestamp": "2026-08-15T12:34:56Z",
    "active": true
  }
]
```

---

## Creating a Test/Monitoring Script

Create `E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Backend\test_warden.py`:

```python
#!/usr/bin/env python3
"""Test script for Warden backend integration."""

import requests
import json
import time
from datetime import datetime

BACKEND_URL = "http://10.75.11.83:8000"
WARDEN_DIRECT_IP = "10.75.11.120"  # Update after getting IP from serial

def print_header(title):
    print(f"\n{'='*60}")
    print(f"  {title}")
    print(f"{'='*60}\n")

def test_backend_health():
    """Check if backend is online."""
    print_header("Testing Backend Health")
    try:
        response = requests.get(f"{BACKEND_URL}/", timeout=5)
        print(f"✅ Backend Status: {response.status_code}")
        print(f"Response: {json.dumps(response.json(), indent=2)}")
        return True
    except Exception as e:
        print(f"❌ Backend Error: {e}")
        return False

def test_warden_camera_direct():
    """Test direct access to Warden camera."""
    print_header("Testing Warden Camera (Direct)")
    
    # Test stream endpoint
    try:
        response = requests.get(f"http://{WARDEN_DIRECT_IP}:81/status", timeout=5)
        print(f"✅ Camera Status: {response.status_code}")
        print(f"Response: {json.dumps(response.json(), indent=2)}")
    except Exception as e:
        print(f"❌ Camera Error: {e}")
        return False
    
    # Test snapshot
    try:
        response = requests.get(f"http://{WARDEN_DIRECT_IP}:81/capture", timeout=5)
        print(f"✅ Snapshot Size: {len(response.content)} bytes")
        
        # Save snapshot
        filename = f"warden_snapshot_{datetime.now().strftime('%H%M%S')}.jpg"
        with open(filename, 'wb') as f:
            f.write(response.content)
        print(f"📸 Snapshot saved: {filename}")
        return True
    except Exception as e:
        print(f"❌ Snapshot Error: {e}")
        return False

def test_warden_status():
    """Get Warden status from backend."""
    print_header("Testing Warden Backend Status")
    try:
        response = requests.get(f"{BACKEND_URL}/api/v1/bots/Warden", timeout=5)
        print(f"✅ Status Code: {response.status_code}")
        print(f"Response:\n{json.dumps(response.json(), indent=2)}")
        return True
    except Exception as e:
        print(f"❌ Status Error: {e}")
        return False

def test_warden_detections():
    """Get Warden vision detections from backend."""
    print_header("Testing Warden Detections")
    try:
        response = requests.get(f"{BACKEND_URL}/api/v1/bots/Warden/detections", timeout=5)
        print(f"✅ Detections Code: {response.status_code}")
        print(f"Response:\n{json.dumps(response.json(), indent=2)}")
        return True
    except Exception as e:
        print(f"❌ Detections Error: {e}")
        return False

def test_incidents():
    """Get all incidents from backend."""
    print_header("Testing Incidents API")
    try:
        response = requests.get(f"{BACKEND_URL}/api/v1/incidents", timeout=5)
        print(f"✅ Incidents Code: {response.status_code}")
        incidents = response.json()
        
        # Filter Warden incidents
        warden_incidents = [inc for inc in incidents if inc.get('bot_id') == 'Warden']
        print(f"\nTotal Incidents: {len(incidents)}")
        print(f"Warden Incidents: {len(warden_incidents)}")
        
        if warden_incidents:
            print(f"\nLatest Warden Incident:")
            print(json.dumps(warden_incidents[-1], indent=2))
        return True
    except Exception as e:
        print(f"❌ Incidents Error: {e}")
        return False

def monitor_telemetry(duration=30):
    """Monitor Warden telemetry for specified duration."""
    print_header(f"Monitoring Warden Telemetry ({duration}s)")
    print("Watching for status updates and detections...")
    print("(In real deployment, use WebSocket endpoint)\n")
    
    start_time = time.time()
    while time.time() - start_time < duration:
        try:
            response = requests.get(f"{BACKEND_URL}/api/v1/bots/Warden", timeout=5)
            if response.status_code == 200:
                data = response.json()
                bot = data.get('bot', {})
                timestamp = datetime.now().strftime('%H:%M:%S')
                
                status = bot.get('status', 'UNKNOWN')
                detections = bot.get('vision_detections', [])
                
                print(f"[{timestamp}] Status: {status} | Detections: {len(detections)}")
                
                if detections:
                    for det in detections:
                        label = det.get('label', 'unknown')
                        conf = det.get('confidence', 0)
                        threat = det.get('is_threat', False)
                        print(f"  🔍 {label} ({conf:.1f}%) - Threat: {threat}")
                
            time.sleep(3)  # Match Warden's 3-second telemetry interval
        except KeyboardInterrupt:
            print("\n\n⏹️  Monitoring stopped by user")
            break
        except Exception as e:
            print(f"❌ Monitor Error: {e}")
            time.sleep(3)

def main():
    """Run all tests."""
    print("\n" + "="*60)
    print("  AEGIS WARDEN BACKEND INTEGRATION TEST SUITE")
    print("="*60)
    
    tests = [
        ("Backend Health", test_backend_health),
        ("Warden Camera Direct", test_warden_camera_direct),
        ("Warden Backend Status", test_warden_status),
        ("Warden Detections", test_warden_detections),
        ("Incidents API", test_incidents),
    ]
    
    results = []
    for test_name, test_func in tests:
        try:
            result = test_func()
            results.append((test_name, result))
        except Exception as e:
            print(f"❌ Test '{test_name}' crashed: {e}")
            results.append((test_name, False))
    
    # Summary
    print_header("Test Summary")
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for test_name, result in results:
        status = "✅ PASS" if result else "❌ FAIL"
        print(f"{status} - {test_name}")
    
    print(f"\nTotal: {passed}/{total} tests passed")
    
    # Optional: Live monitoring
    if passed == total:
        print("\n" + "="*60)
        response = input("All tests passed! Monitor live telemetry? (y/n): ")
        if response.lower() == 'y':
            duration = input("Duration in seconds (default 30): ")
            duration = int(duration) if duration.isdigit() else 30
            monitor_telemetry(duration)

if __name__ == "__main__":
    main()
```

---

## How to Use

### Step 1: Flash Warden
```bash
idf.py build flash monitor
```

### Step 2: Note the IP Address

From serial monitor:
```
[BACKEND] Connected to Wi-Fi, IP: 10.75.11.120
[CAM_STREAM] Stream URL: http://10.75.11.120:81/stream
```

Update `WARDEN_DIRECT_IP` in test script with this IP.

### Step 3: Start Backend
```bash
cd E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Backend
start_backend.bat
```

### Step 4: Run Test Script
```bash
python test_warden.py
```

### Step 5: View Camera in Browser

**Option A - Direct:**
```
http://10.75.11.120:81/
```

**Option B - Through Backend:**
```
http://10.75.11.83:8000/api/v1/video/stream/Warden
```

**Option C - Backend Video Viewer:**
```
http://10.75.11.83:8000/video
```

---

## Frontend Integration

Your React/Expo frontend can display Warden camera:

```javascript
// Warden camera component
const WardenCamera = () => {
  const streamUrl = `http://10.75.11.83:8000/api/v1/video/stream/Warden`;
  
  return (
    <View style={styles.cameraContainer}>
      <Image
        source={{ uri: streamUrl }}
        style={styles.camera}
        resizeMode="contain"
      />
      <Text style={styles.label}>Warden - Fire/Smoke Detection</Text>
    </View>
  );
};

// Or use HTML img tag in web
<img src="http://10.75.11.83:8000/api/v1/video/stream/Warden" 
     alt="Warden Camera" />
```

---

## Troubleshooting

### Camera Stream Not Working

1. **Check Warden is online:**
   ```bash
   ping 10.75.11.120
   ```

2. **Check port 81 is accessible:**
   ```bash
   curl http://10.75.11.120:81/status
   ```

3. **Check serial monitor for errors:**
   ```
   [CAM_STREAM] Failed to start stream server
   ```

4. **Try rebooting Warden** (press reset button)

### Backend Not Receiving Data

1. **Check Wi-Fi connection:**
   ```
   [BACKEND] Connected to Wi-Fi, IP: 10.75.11.XXX
   ```

2. **Check backend health:**
   ```bash
   curl http://10.75.11.83:8000/
   ```

3. **Check backend logs** for incoming POST requests

4. **Verify network** - Warden and backend must be on same network

### No Fire/Smoke Detections

1. **Check detection threshold** in `warden_config.h`:
   ```cpp
   #define WARDEN_DETECTION_THRESHOLD 0.50f
   ```

2. **Test with fire source** (lighter, candle, LED flashlight)

3. **Check inference time** in serial monitor:
   ```
   [WARDEN] Inference: 245ms
   ```

4. **Verify model is loaded**:
   ```
   [WARDEN] Project: Fire and Smoke Detection
   ```

---

## Performance Metrics

### Expected Values

- **Camera FPS**: ~10 FPS (100ms delay between frames)
- **AI Inference**: 200-300ms per frame
- **Total Loop Time**: ~320ms (3 FPS effective for AI)
- **Telemetry Rate**: Every 3 seconds
- **HTTP Response**: 50-200ms
- **Stream Latency**: 300-500ms

### Memory Usage

- **Free Heap**: ~150 KB
- **Free PSRAM**: ~4 MB
- **HTTP Server**: ~8 KB
- **Camera Buffers**: Managed by esp32-camera

---

## Summary

✅ **Camera Streaming**: Direct MJPEG on port 81  
✅ **Backend Integration**: Telemetry + incident reporting  
✅ **Video Access**: Direct or through backend proxy  
✅ **Data Monitoring**: API endpoints + test script  
✅ **Frontend Ready**: Stream URLs available  

**Next Steps:**
1. Flash Warden with new code
2. Note the IP address from serial monitor
3. Test camera stream in browser
4. Run test_warden.py to verify backend integration
5. Integrate stream URL into your frontend dashboard

---

**Document Version**: 1.0  
**Date**: 2026-08-15  
**Project**: AEGIS Multi-Robot System
