# Complete System Workflow - Guardian with Voice Activation

## 🎯 How Everything Works Now

Your ESP32-S3-EYE Guardian system now has **two modes** of operation:

---

## Mode 1: VOICE ACTIVATION ENABLED (Current Setting)

### 📋 Boot Sequence

```
1. System Powers On
   ↓
2. Initialize NVS Flash
   ↓
3. ✨ VOICE ACTIVATION PHASE ✨
   ├── Initialize I2S Microphone (GPIO 2, 41, 42)
   ├── Load Wake Word Models from "model" partition
   ├── Configure AFE (Audio Front End)
   │   ├── VAD (Voice Activity Detection)
   │   ├── Speech Enhancement
   │   └── WakeNet (Wake Word Detection)
   ├── Start voice_feed_task (CPU 0)
   └── Start voice_detect_task (CPU 1)
   ↓
4. 🎤 LISTENING FOR WAKE WORD 🎤
   ├── System waits here (blocking)
   ├── Microphone continuously listening
   ├── Display message: "Say 'Hi ESP' to activate camera"
   └── [User must say wake word to continue]
   ↓
5. 🎉 WAKE WORD DETECTED! 🎉
   ├── Log: "Wake word detected! Activating system..."
   └── System continues boot...
   ↓
6. Initialize Camera (ESP32-S3-EYE configuration)
   ↓
7. Initialize SPIFFS File System
   ↓
8. Initialize Wi-Fi Connection
   ├── Connect to SSID: "Sakibmob"
   └── Wait for IP address
   ↓
9. Start HTTP Server
   ├── Stream endpoint: http://<IP>/stream
   └── Root redirects to /stream
   ↓
10. Start AI Tasks (Dual Core)
    ├── Face Recognition Task (CPU 0)
    │   ├── Face detection every 1 second
    │   ├── Auto-enroll first face as OWNER
    │   ├── Compare subsequent faces to owner
    │   └── Set security state:
    │       ├── OWNER (recognized)
    │       ├── INTRUDER (unknown face)
    │       └── NO_FACE (no face detected)
    │
    └── Object Detection Task (CPU 1)
        ├── Edge Impulse inference every 1.5 seconds
        ├── Detect: person, chair, light, screen
        └── Log bounding boxes + confidence
   ↓
11. Start Status Task
    └── Log every 5 seconds:
        ├── Security State (OWNER/INTRUDER/NO_FACE)
        ├── Owner enrollment status
        ├── Free heap memory
        └── PSRAM usage
   ↓
12. 🟢 SYSTEM FULLY OPERATIONAL 🟢
```

---

## Mode 2: VOICE ACTIVATION DISABLED

### If you set `ENABLE_VOICE_ACTIVATION 0`

```
1. System Powers On
   ↓
2. Initialize NVS Flash
   ↓
3. Skip voice activation entirely
   ↓
4. Immediately initialize Camera
   ↓
5. [Rest of the flow same as above from step 7]
```

---

## 🎙️ Voice Activation Details

### What Happens During Voice Phase

```
┌─────────────────────────────────────────────────────────┐
│  MICROPHONE LISTENING (Continuous Loop)                 │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  voice_feed_task (CPU 0):                               │
│  ┌────────────────────────────────────────────┐        │
│  │ 1. Read audio from I2S microphone          │        │
│  │ 2. Buffer: 16-bit, 16kHz, mono             │        │
│  │ 3. Feed to AFE pipeline                    │        │
│  │ 4. Repeat continuously                     │        │
│  └────────────────────────────────────────────┘        │
│                           ↓                              │
│  ┌────────────────────────────────────────────┐        │
│  │ AFE Processing Pipeline                    │        │
│  │ ├─ VAD: Detect voice activity              │        │
│  │ ├─ Speech Enhancement: Reduce noise        │        │
│  │ └─ WakeNet: Match wake word patterns       │        │
│  └────────────────────────────────────────────┘        │
│                           ↓                              │
│  voice_detect_task (CPU 1):                             │
│  ┌────────────────────────────────────────────┐        │
│  │ 1. Fetch AFE results                       │        │
│  │ 2. Check for wake word detection           │        │
│  │ 3. If detected:                             │        │
│  │    - Log: "WAKE WORD DETECTED!"            │        │
│  │    - Set event bit                         │        │
│  │    - Signal main thread                    │        │
│  │ 4. Repeat continuously                     │        │
│  └────────────────────────────────────────────┘        │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

### Supported Wake Words

- **English:** "Hi ESP"
- **Chinese:** "乐鑫" (Lè Xīn)

### Detection Settings

```cpp
Detection Mode: DET_MODE_90 (90% accuracy mode)
Threshold: 0.6 (60% confidence)
VAD Mode: VAD_MODE_3 (moderate sensitivity)
Sample Rate: 16000 Hz
Channels: 1 (mono)
```

---

## 👤 Face Recognition System

### First Boot Behavior

```
Face Database: EMPTY (cleared on boot if RESET_FACE_DB_ON_BOOT = 1)
                ↓
First Face Detected → Auto-enroll as OWNER (ID 0)
                ↓
Subsequent Faces → Compare to Owner
                ↓
              Match?
        ┌───────┴────────┐
       YES               NO
        ↓                 ↓
  SECURITY_OWNER   SECURITY_INTRUDER
  (similarity ≥ 40%)    (Log alert)
```

### Face Recognition Flow

```
Every 1 second:
  1. Capture camera frame
  2. Convert JPEG → RGB888
  3. Run face detection (HumanFaceDetect)
  4. If face(s) found:
     ├─ If no owner enrolled yet:
     │  └─ Enroll first face as OWNER
     │     └─ Save to /spiflash/face_db
     └─ If owner enrolled:
        └─ Run face recognition (HumanFaceRecognizer)
           ├─ Compare to database
           ├─ Get similarity score
           └─ Update security state
```

### Security States

| State | Meaning | Trigger |
|-------|---------|---------|
| `SECURITY_OWNER` | Owner recognized | Face matched with similarity ≥ 40% |
| `SECURITY_INTRUDER` | Unknown person | Face detected but not matched |
| `SECURITY_NO_FACE` | No face visible | Face detector found nothing |
| `SECURITY_UNKNOWN` | Initial state | Before any detection |

---

## 🎯 Object Detection System

### Edge Impulse Integration

```
Every 1.5 seconds:
  1. Capture camera frame
  2. Convert JPEG → RGB888
  3. Crop & resize to model input size (96x96)
  4. Run Edge Impulse classifier
  5. Parse bounding boxes
  6. Log detections with confidence > 50%
```

### Detected Classes

- Person
- Chair  
- Light
- Screen
- (Other custom classes from your Edge Impulse model)

### Detection Output

```
[OBJ] person 87.3% x=45 y=67 w=120 h=180
[OBJ] chair 65.1% x=200 y=150 w=80 h=90
```

---

## 🌐 HTTP Streaming Server

### Endpoints

| URL | Description |
|-----|-------------|
| `http://<ESP32-IP>/` | Redirects to /stream |
| `http://<ESP32-IP>/stream` | Live MJPEG video stream |

### Stream Format

```
Content-Type: multipart/x-mixed-replace;boundary=frame

--frame
Content-Type: image/jpeg
Content-Length: [size]

[JPEG data]

--frame
Content-Type: image/jpeg
Content-Length: [size]

[JPEG data]

... (continuous)
```

### Frame Rate

- Target: ~15 FPS (66ms delay between frames)
- Actual: Depends on network and camera capture speed

---

## 🔄 System Integration

### Task Distribution

```
CPU 0 (Protocol CPU):
  ├── voice_feed_task (Priority 5) - Feeds microphone data
  ├── face_task (Priority depends on FreeRTOS) - Face recognition
  └── HTTP server tasks

CPU 1 (Application CPU):
  ├── voice_detect_task (Priority 5) - Wake word detection
  ├── ai_task - Object detection
  └── status_task - System monitoring
```

### Memory Usage

```
PSRAM (External RAM):
  ├── Camera framebuffers (2 buffers)
  ├── Face recognition RGB buffer (~230 KB)
  ├── Object detection buffers (~230 KB for full, ~27 KB for model)
  └── AFE processing buffers

Internal RAM:
  ├── FreeRTOS tasks
  ├── Wi-Fi stack
  ├── HTTP server
  └── Control structures
```

---

## 📊 Status Monitoring

### Console Logs (Every 5 seconds)

```
Security=OWNER | OwnerID=0 | Enrolled=YES | FreeHeap=45632 | PSRAM=3145728
Security=INTRUDER | OwnerID=0 | Enrolled=YES | FreeHeap=45440 | PSRAM=3145600
Security=NO_FACE | OwnerID=0 | Enrolled=YES | FreeHeap=45632 | PSRAM=3145728
```

---

## 🎬 Complete Usage Scenario

### Scenario 1: First Time Boot with Voice Activation

```
1. Flash firmware → Power on ESP32-S3-EYE
   
2. Serial Monitor shows:
   I (xxx) VOICE: Initializing voice activation...
   I (xxx) VOICE: I2S microphone initialized successfully
   I (xxx) VOICE: Loading wake word models...
   I (xxx) VOICE: Found 1 model(s):
   I (xxx) VOICE:   [0] wn9_hilexin
   I (xxx) VOICE: Voice activation initialized successfully!
   I (xxx) VOICE: 🎤 Listening... Say 'Hi ESP' to activate!
   I (xxx) AEGIS: ====================================
   I (xxx) AEGIS:   Waiting for wake word...
   I (xxx) AEGIS:   Say 'Hi ESP' to activate camera
   I (xxx) AEGIS: ====================================

3. [System is waiting... LED might be blinking]

4. You say: "Hi ESP"

5. Serial Monitor shows:
   I (xxx) VOICE: ========================================
   I (xxx) VOICE:   WAKE WORD DETECTED!
   I (xxx) VOICE:   Model: 1  Word: 1
   I (xxx) VOICE: ========================================
   I (xxx) VOICE: ✅ Wake word detected! Activating system...
   I (xxx) AEGIS: Wake word detected! Activating system...

6. Camera initializes:
   I (xxx) AEGIS: Initializing camera...
   I (xxx) AEGIS: Camera OK

7. Wi-Fi connects:
   I (xxx) AEGIS: Wi-Fi connecting...
   I (xxx) AEGIS: IP: 192.168.1.100

8. HTTP server starts:
   I (xxx) AEGIS: HTTP server started
   I (xxx) AEGIS: Stream: http://192.168.1.100/stream

9. Face detection starts:
   I (xxx) AEGIS: Face task started on CPU0
   I (xxx) AEGIS: First face detected
   I (xxx) AEGIS: Enrolling owner as ID=0...
   I (xxx) AEGIS: ====================================
   I (xxx) AEGIS: OWNER ENROLLED
   I (xxx) AEGIS: OWNER ID = 0
   I (xxx) AEGIS: ====================================

10. System runs continuously:
    I (xxx) AEGIS: Security=OWNER | OwnerID=0 | Enrolled=YES
    I (xxx) AEGIS: [OBJ] person 92.5% x=100 y=50 w=150 h=200
```

---

### Scenario 2: Boot WITHOUT Voice Models

```
1. Power on ESP32-S3-EYE (no models in flash)

2. Serial Monitor shows:
   I (xxx) VOICE: Initializing voice activation...
   I (xxx) VOICE: I2S microphone initialized successfully
   I (xxx) VOICE: Loading wake word models...
   W (xxx) VOICE: No models found in 'model' partition
   W (xxx) VOICE: Voice activation requires wake word models
   W (xxx) VOICE: Continuing without voice activation...
   W (xxx) AEGIS: Voice init failed, using normal mode

3. Camera immediately initializes:
   I (xxx) AEGIS: Initializing camera...
   
4. [System continues normally without voice activation]
```

---

### Scenario 3: Owner Recognition Working

```
Boot → Voice detected → Camera ON → Face enrolled

Loop every 1 second:
┌──────────────────────────────────────────────┐
│ Camera captures frame                        │
│ Face detected                                │
│ Run recognition                              │
│                                              │
│ Case 1: It's YOU (owner)                    │
│   → Recognize -> id=0 sim=0.652             │
│   → >>> OWNER CONFIRMED                     │
│   → Security=OWNER                          │
│                                              │
│ Case 2: It's SOMEONE ELSE                   │
│   → Recognize -> id=1 sim=0.234             │
│   → >>> INTRUDER id=1 sim=0.23             │
│   → Security=INTRUDER                       │
│   → SECURITY ALERT: INTRUDER                │
│                                              │
│ Case 3: NO FACE                             │
│   → No faces detected                       │
│   → Security=NO_FACE                        │
└──────────────────────────────────────────────┘
```

---

## 🔧 Configuration Options

### Voice Activation

```cpp
// main/main.cpp line 37
#define ENABLE_VOICE_ACTIVATION 1  // 1=Voice ON, 0=Voice OFF
```

### Face Database

```cpp
// main/main.cpp line 104
#define RESET_FACE_DB_ON_BOOT 1    // 1=Clear on boot, 0=Keep database
#define AUTO_ENROLL_FIRST_FACE 1   // 1=Auto-enroll, 0=Manual
```

### Thresholds

```cpp
// main/main.cpp line 108
#define OWNER_SIMILARITY_THRESHOLD 0.40f  // 40% similarity for owner match
```

### Timing

```cpp
// main/main.cpp lines 79-81
#define STREAM_DELAY_MS       66     // ~15 FPS streaming
#define FACE_INTERVAL_MS      1000   // Face check every 1 second
#define OBJECT_INTERVAL_MS    1500   // Object detection every 1.5 seconds
```

### Wi-Fi

```cpp
// main/main.cpp lines 48-49
#define WIFI_SSID "Sakibmob"
#define WIFI_PASS "12345678"
```

---

## 🎯 Summary: What Happens Now

### WITH Voice Activation (Current):

1. ✅ System boots
2. 🎤 Voice system initializes
3. ⏸️ **System WAITS for "Hi ESP"**
4. 🎉 Wake word detected
5. 📷 Camera starts
6. 🌐 Wi-Fi connects
7. 🤖 AI starts (face + object detection)
8. 🌐 HTTP stream available
9. 🔄 Continuous monitoring

### WITHOUT Voice Activation:

1. ✅ System boots
2. 📷 Camera starts immediately (no waiting)
3. 🌐 Wi-Fi connects
4. 🤖 AI starts
5. 🌐 HTTP stream available
6. 🔄 Continuous monitoring

---

## 🚀 Key Benefits

1. **Security**: System only activates when you say the wake word
2. **Privacy**: Camera doesn't start until authorized
3. **Energy Saving**: Microphone uses less power than camera
4. **Hands-free**: No button press needed
5. **Flexibility**: Can disable voice and use instant-on mode

---

## 📱 Access Your System

Once running, open your browser:
```
http://192.168.1.100/stream
```
(Replace with your ESP32's actual IP address from serial monitor)

You'll see:
- Live video stream
- Face detection working in background
- Object detection working in background
- Security state logged to serial console

---

**Your Guardian System is Now Complete and Ready! 🎉**
