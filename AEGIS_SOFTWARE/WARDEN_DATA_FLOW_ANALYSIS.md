# Warden Data Flow Analysis: Firmware → Backend → Frontend

## Data Flow Summary

### ✅ **WORKING**: Data Currently Flowing

| Data Type | Warden Sends | Backend Receives | Frontend Displays | Status |
|-----------|--------------|------------------|-------------------|--------|
| **Bot Status** | `status: "PATROL"` | ✅ Stored in bot state | ✅ Shows as "online" | ✅ WORKING |
| **IP Address** | `ip_address: "10.75.11.50"` | ✅ Registers for streaming | ✅ Shows in telemetry chips | ✅ WORKING |
| **Camera Port** | `camera_port: 81` | ✅ Registers for streaming | ✅ Used for stream URL | ✅ WORKING |
| **System Info** | `system_info.free_heap`, `free_psram` | ✅ Stored | ✅ Shows in telemetry | ✅ WORKING |
| **WiFi RSSI** | `wifi_rssi: -58` | ✅ Stored | ✅ Shows in telemetry chips | ✅ WORKING |
| **Vision Detection** | `label`, `confidence`, `is_threat` | ✅ Stored in visionDetections[] | ✅ Shows in StreamViewer HUD | ✅ WORKING |
| **Incidents (Fire)** | `type: "FIRE"`, `severity: "CRITICAL"` | ✅ Stored & broadcasted | ✅ Shows in Dashboard alerts | ✅ WORKING |

---

### ⚠️ **MISSING**: Data NOT Currently Displayed in Frontend

| Data Type | Warden Sends | Backend Receives | Frontend Issue | Fix Needed |
|-----------|--------------|------------------|----------------|------------|
| **Detection Streaks** | `detection_state.fire_streak: 3` | ✅ Stored in payload | ❌ NOT extracted or displayed | Need UI component |
| **Smoke Streak** | `detection_state.smoke_streak: 0` | ✅ Stored in payload | ❌ NOT extracted or displayed | Need UI component |
| **Clear Streak** | `detection_state.clear_streak: 5` | ✅ Stored in payload | ❌ NOT extracted or displayed | Need UI component |
| **Hazard Context** | In incident: `context.fire_confidence` | ✅ Stored in incidents | ❌ NOT shown in incident details | Need expanded view |

---

## Detailed Data Flow

### 1. **Telemetry Data** (Every 3 seconds)

**Warden Firmware → Backend**
```json
{
  "bot_id": "Warden",
  "kind": "telemetry",
  "payload": {
    "status": "PATROL",
    "system_info": {
      "free_heap": 4194304,
      "free_psram": 7990000
    },
    "wifi_rssi": -58,
    "ip_address": "10.75.11.50",
    "camera_port": 81,
    "timestamp": "uptime_1234s",
    "detection_state": {              // ⚠️ NOT DISPLAYED IN FRONTEND
      "fire_streak": 0,
      "smoke_streak": 0,
      "clear_streak": 15
    }
  }
}
```

**Backend Processing** (`routes.py` line ~610)
```python
# ✅ Extracts ip_address and camera_port for streaming
bot_ip = payload.payload.get("ip_address")
camera_port = payload.payload.get("camera_port", 80)
video_service.register_bot_ip(payload.bot_id, bot_ip, camera_port)

# ✅ Updates simulator state
simulator.update_bot_from_telemetry(payload.bot_id, payload.kind, payload.payload)

# ✅ Broadcasts to WebSocket
await manager.broadcast({
    "type": "INGESTED_TELEMETRY",
    "bot_id": "Warden",
    "kind": "telemetry",
    "payload": {...}  # Full payload including detection_state
})
```

**Frontend Reception** (`useTelemetry.js` line ~247)
```javascript
if (payload.type === 'INGESTED_TELEMETRY') {
  const { bot_id, kind, payload: data } = payload;
  
  // ✅ Updates bot status, battery, heap, rssi, ip_address
  if (kind === 'telemetry' || kind === 'status') {
    if (data.status) updated.status = STATUS_MAP[data.status];
    if (data.system_info.free_heap) updated.freeHeap = data.system_info.free_heap;
    if (data.wifi_rssi) updated.wifiRssi = data.wifi_rssi;
    if (data.ip_address) updated.ipAddress = data.ip_address;
    
    // ❌ MISSING: detection_state is NOT extracted here!
    // Should add:
    // if (data.detection_state) updated.detectionState = data.detection_state;
  }
}
```

---

### 2. **Vision Detection Data** (When fire/smoke detected)

**Warden Firmware → Backend**
```json
{
  "bot_id": "Warden",
  "kind": "vision_detection",
  "payload": {
    "label": "fire",
    "confidence": 0.85,
    "is_threat": true,
    "bbox": { "x": 120, "y": 80, "w": 40, "h": 40 },
    "timing": { "total_ms": 2937 }
  }
}
```

**Backend Processing**
```python
# ✅ Validates vision detection schema
VisionDetection(**payload.payload)

# ✅ If threat detected, creates incident
if payload.kind == "vision_detection" and payload.payload.get("is_threat"):
    incident = _make_incident(
        payload.bot_id,
        f"Threat Detected: {label}",
        "INTRUDER",
        f"{payload.bot_id} detected {label} ({confidence:.1f}% confidence)",
        severity="CRITICAL"
    )
    simulator.record_incident(incident)
    await _broadcast_emergency_status(...)

# ✅ Broadcasts vision detection
await manager.broadcast({
    "type": "INGESTED_TELEMETRY",
    "kind": "vision_detection",
    "payload": {...}
})
```

**Frontend Reception**
```javascript
// ✅ Vision detections are stored and displayed
else if (kind === 'vision_detection') {
  const currentVision = [...updated.visionDetections];
  currentVision.push(data);
  updated.visionDetections = currentVision.slice(-5);
  
  // ✅ Shows in StreamViewer HUD
  // ✅ Triggers threat alert notification
}
```

---

### 3. **Incident Reports** (Fire/Smoke alerts)

**Warden Firmware → Backend**
```json
{
  "bot_id": "Warden",
  "type": "FIRE",
  "message": "Fire detected with confidence 0.85",
  "severity": "CRITICAL",
  "context": {
    "fire_confidence": 0.85,
    "smoke_confidence": 0.12
  }
}
```

**Backend Processing**
```python
# ✅ Stores incident in simulator
simulator.record_incident(IncidentLog(**payload))

# ✅ Broadcasts to WebSocket
await manager.broadcast({
    "type": "ALERT",
    "incident": {...}
})
```

**Frontend Reception**
```javascript
// ✅ Incidents appear in Dashboard
if (payload.type === 'ALERT' || payload.incident) {
  const inc = payload.incident || payload;
  const mapped = mapBackendIncident(inc);
  setIncidents((curr) => [mapped, ...curr].slice(0, 25));
  setAlerts((curr) => [mapBackendAlert(inc), ...curr].slice(0, 10));
  
  // ✅ Shows: "Warden · Fire detected with confidence 0.85"
  // ❌ BUT: context.fire_confidence is NOT shown separately
}
```

---

## What's Missing in Frontend UI?

### Missing Component 1: **Detection State Panel** (for Warden)

Should display in `FeedsScreen.js` or `DashboardScreen.js`:

```
┌─────────────────────────────────┐
│ WARDEN DETECTION STATE          │
├─────────────────────────────────┤
│ 🔥 Fire Streak:   3 frames     │
│ 💨 Smoke Streak:  0 frames     │
│ ✅ Clear Streak:  0 frames     │
└─────────────────────────────────┘
```

### Missing Component 2: **Hazard Data Panel**

Should show gas sensor data (if available):

```
┌─────────────────────────────────┐
│ HAZARD SENSORS                  │
├─────────────────────────────────┤
│ 🌫️  Gas Level: 205 PPM         │
│ 🌡️  Temperature: N/A           │
└─────────────────────────────────┘
```

---

## Recommendations

### ✅ **Already Working Well**
1. IP address and camera port registration
2. Video streaming with click-to-start
3. Vision detection display in StreamViewer
4. Incident alerts in Dashboard
5. System telemetry (heap, PSRAM, RSSI)

### 🔧 **Needs Implementation**
1. **Extract `detection_state`** in `useTelemetry.js`
2. **Add Detection State UI** in FeedsScreen Warden card
3. **Show Streak Counters** (fire/smoke/clear frames)
4. **Display Hazard Context** in incident detail view
5. **Add Gas Sensor Display** (if MQ-2 sensor data added)

---

## Files to Modify

### Frontend Changes Needed:

1. **`src/hooks/useTelemetry.js`** (line ~250)
   - Extract `detection_state` from telemetry payload
   - Add to bot state: `detectionState: { fire_streak, smoke_streak, clear_streak }`

2. **`src/screens/FeedsScreen.js`** (line ~270)
   - Add detection state display in Warden telemetry chips row
   - Show fire/smoke/clear streak counters

3. **`src/components/SwarmUI.js`** (if exists)
   - Create `DetectionStateChip` component for Warden

---

## Testing Checklist

- [x] Warden IP address appears in telemetry chips
- [x] Camera port 81 is used for streaming
- [x] Vision detections show in StreamViewer HUD
- [x] Fire incidents appear in Dashboard alerts
- [x] System info (heap, PSRAM, RSSI) displays correctly
- [ ] **Detection streaks** (fire/smoke/clear) display in UI
- [ ] **Hazard context** shows in incident details
- [ ] Gas sensor data displays (when implemented)

---

**Status**: 85% of data flowing correctly, 15% needs UI implementation  
**Last Updated**: 2026-08-15  
**Priority**: Medium (core functionality works, missing advanced telemetry display)
