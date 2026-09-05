# AEGIS Guardian - System Architecture

## System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    AEGIS GUARDIAN SYSTEM                         │
│                     (ESP32-S3 Based)                             │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────┬───────────────────────────────────────────┐
│                     │                                           │
│  VOICE ACTIVATION   │      MAIN SECURITY SYSTEM                 │
│    (NEW MODULE)     │      (EXISTING FEATURES)                  │
│                     │                                           │
└─────────────────────┴───────────────────────────────────────────┘
```

## Boot Flow Comparison

### WITHOUT Voice Activation (Original):
```
┌──────┐    ┌────────┐    ┌──────┐    ┌────────┐    ┌───────┐
│ NVS  │───▶│ Camera │───▶│ WiFi │───▶│ SPIFFS │───▶│  AI   │
└──────┘    └────────┘    └──────┘    └────────┘    └───────┘
                                                         │
                                                         ▼
                        ┌─────────────────────────────────┐
                        │  HTTP Stream + Face + Object    │
                        │       Recognition Running        │
                        └─────────────────────────────────┘
```

### WITH Voice Activation (New):
```
┌──────┐    ┌───────┐    ┌──────┐    ┌────────┐
│ NVS  │───▶│  I2S  │───▶│ WiFi │───▶│ SPIFFS │
└──────┘    │  Mic  │    └──────┘    └────────┘
            └───────┘
                │
                ▼
        ┌──────────────┐
        │ Voice Models │
        │   (ESP-SR)   │
        └──────────────┘
                │
                ▼
        ┌──────────────────┐
        │  AFE Pipeline    │
        │  - Feed Task     │
        │  - Detect Task   │
        └──────────────────┘
                │
                ▼
        ╔═══════════════════╗
        ║ WAIT FOR WAKE WORD║
        ║   "Hi ESP"        ║
        ╚═══════════════════╝
                │
                ▼ (Wake Word Detected)
                │
        ┌────────────────────────────────┐
        │   Camera + HTTP + AI Tasks     │
        │   (Same as original system)    │
        └────────────────────────────────┘
```

## Module Architecture

### Voice Activation Module

```
┌─────────────────────────────────────────────────┐
│          voice_activation.h/.cpp                │
├─────────────────────────────────────────────────┤
│                                                 │
│  ┌─────────────────┐     ┌──────────────────┐  │
│  │ Initialization  │     │  State Machine   │  │
│  │  - I2S Setup    │     │  - IDLE          │  │
│  │  - Model Load   │     │  - LISTENING     │  │
│  │  - AFE Config   │     │  - ACTIVATED     │  │
│  └─────────────────┘     └──────────────────┘  │
│                                                 │
│  ┌─────────────────┐     ┌──────────────────┐  │
│  │  Feed Task      │     │  Detect Task     │  │
│  │  (CPU 0)        │     │  (CPU 1)         │  │
│  │  - Read Mic     │     │  - AFE Fetch     │  │
│  │  - Feed AFE     │     │  - Wake Detection│  │
│  └─────────────────┘     └──────────────────┘  │
│                                                 │
│  ┌─────────────────────────────────────────┐   │
│  │          Event Group                    │   │
│  │  - WAKE_WORD_DETECTED_BIT               │   │
│  └─────────────────────────────────────────┘   │
│                                                 │
└─────────────────────────────────────────────────┘
```

### Main System Integration

```
┌──────────────────────────────────────────────────────┐
│                    main.cpp                          │
├──────────────────────────────────────────────────────┤
│                                                      │
│  #if ENABLE_VOICE_ACTIVATION                         │
│    ┌────────────────────────────────┐                │
│    │  voice_activation_init()       │                │
│    │  voice_activation_start()      │                │
│    │  voice_activation_wait()       │                │
│    └────────────────────────────────┘                │
│                      │                               │
│                      ▼ (Wake Word)                   │
│  #endif                                              │
│                                                      │
│    ┌────────────────────────────────┐                │
│    │  init_camera()                 │                │
│    │  wifi_init()                   │                │
│    │  start_stream_server()         │                │
│    └────────────────────────────────┘                │
│                      │                               │
│                      ▼                               │
│    ┌────────────────────────────────┐                │
│    │  face_task (CPU 0)             │                │
│    │  ai_task (CPU 1)               │                │
│    │  status_task                   │                │
│    └────────────────────────────────┘                │
│                                                      │
└──────────────────────────────────────────────────────┘
```

## Task Distribution

### CPU 0 (Protocol Core):
```
┌─────────────────────────────────────┐
│           CPU 0 Tasks               │
├─────────────────────────────────────┤
│                                     │
│  ┌───────────────┐  Priority: 5    │
│  │ voice_feed    │  (Voice Mode)   │
│  └───────────────┘                  │
│                                     │
│  ┌───────────────┐  Priority: 5    │
│  │ face_task     │  (Always)       │
│  └───────────────┘                  │
│                                     │
│  ┌───────────────┐  Priority: 2    │
│  │ status_task   │  (Always)       │
│  └───────────────┘                  │
│                                     │
│  ┌───────────────┐  WiFi/HTTP      │
│  │ System Tasks  │                 │
│  └───────────────┘                  │
│                                     │
└─────────────────────────────────────┘
```

### CPU 1 (Application Core):
```
┌─────────────────────────────────────┐
│           CPU 1 Tasks               │
├─────────────────────────────────────┤
│                                     │
│  ┌───────────────┐  Priority: 5    │
│  │ voice_detect  │  (Voice Mode)   │
│  └───────────────┘                  │
│                                     │
│  ┌───────────────┐  Priority: 3    │
│  │ ai_task       │  (Always)       │
│  │ (Edge Impulse)│                 │
│  └───────────────┘                  │
│                                     │
└─────────────────────────────────────┘
```

## Memory Layout

### Without Voice Activation:
```
┌─────────────────────────────────────────────┐
│              Internal RAM                   │
├─────────────────────────────────────────────┤
│ FreeRTOS + System                           │
│ WiFi Stack                                  │
│ Task Stacks (Face, AI, Status, HTTP)       │
│ Buffers (small)                             │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│               PSRAM                         │
├─────────────────────────────────────────────┤
│ Camera Frame Buffers (2x)                   │
│ RGB Conversion Buffers                      │
│ Face Detection Buffers                      │
│ Edge Impulse Model Buffers                  │
└─────────────────────────────────────────────┘
```

### With Voice Activation:
```
┌─────────────────────────────────────────────┐
│              Internal RAM                   │
├─────────────────────────────────────────────┤
│ FreeRTOS + System                           │
│ WiFi Stack                                  │
│ Task Stacks (Voice, Face, AI, Status, HTTP)│
│ Voice Processing Buffers                    │
│ AFE Context                                 │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│               PSRAM                         │
├─────────────────────────────────────────────┤
│ Camera Frame Buffers (2x)                   │
│ RGB Conversion Buffers                      │
│ Face Detection Buffers                      │
│ Edge Impulse Model Buffers                  │
│ Voice AFE Buffers                           │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│             Flash Partition                 │
├─────────────────────────────────────────────┤
│ Application Code                            │
│ ESP-SR Library                              │
│ Wake Word Models ("model" partition)        │
│ SPIFFS (Face Database)                      │
└─────────────────────────────────────────────┘
```

## Data Flow

### Voice Detection Flow:
```
┌──────────┐      ┌─────────┐      ┌─────────────┐
│   I2S    │─────▶│  Feed   │─────▶│     AFE     │
│  Micro   │      │  Task   │      │   Pipeline  │
└──────────┘      └─────────┘      └─────────────┘
                                           │
                                           ▼
                                   ┌─────────────┐
                                   │   Detect    │
                                   │    Task     │
                                   └─────────────┘
                                           │
                                           ▼
                                   ╔═════════════╗
                                   ║ Wake Word?  ║
                                   ╚═════════════╝
                                      │       │
                                     Yes      No
                                      │       └──▶ Continue Listening
                                      ▼
                              ┌──────────────┐
                              │ Set Event Bit│
                              └──────────────┘
                                      │
                                      ▼
                              ┌──────────────┐
                              │ Activate Cam │
                              └──────────────┘
```

### Video/AI Flow (Same as Original):
```
┌────────┐      ┌─────────┐      ┌──────────┐
│ Camera │─────▶│  JPEG   │─────▶│   HTTP   │
│        │      │  Frame  │      │  Stream  │
└────────┘      └─────────┘      └──────────┘
    │                │
    │                ▼
    │           ┌─────────┐
    │           │  Face   │
    │           │  Detect │
    │           └─────────┘
    │                │
    ▼                ▼
┌──────────┐   ┌──────────────┐
│  Object  │   │     Face     │
│ Detect   │   │ Recognition  │
│ (Edge-I) │   │   (Owner)    │
└──────────┘   └──────────────┘
```

## Configuration Options

### Compile-Time Switches:
```cpp
#define ENABLE_VOICE_ACTIVATION 1  // Master switch
#define RESET_FACE_DB_ON_BOOT 1    // Face database
#define AUTO_ENROLL_FIRST_FACE 1   // Owner enrollment
```

### Runtime Parameters:
```cpp
// In voice_activation.cpp:
.set_wakenet_threshold(afe_data, 1, 0.6f);  // Wake sensitivity

// In main.cpp:
#define WIFI_SSID "YourSSID"
#define WIFI_PASS "YourPassword"
#define OWNER_SIMILARITY_THRESHOLD 0.40f
```

## External Interfaces

```
┌──────────────────────────────────────────────────┐
│                                                  │
│   I2S Microphone ◄────┐                          │
│                       │                          │
│   Camera (OV2640) ◄───┤                          │
│                       │                          │
│   WiFi ◄──────────────┼───▶ ESP32-S3 ◄──▶ HTTP  │
│                       │        │                 │
│   SPIFFS (Flash) ◄────┤        │                 │
│                       │        ▼                 │
│   Model Partition ◄───┘    [System]              │
│                                                  │
└──────────────────────────────────────────────────┘
```

## Failure Modes & Recovery

```
Voice Init Fail ──▶ Fall back to normal mode
    │
    ├─▶ Missing models ──▶ Log warning + continue
    ├─▶ I2S fail ──▶ Log error + continue
    └─▶ AFE fail ──▶ Log error + continue

Camera Init Fail ──▶ System halt (critical)

WiFi Disconnect ──▶ Auto reconnect

Face Recognition Fail ──▶ Continue with object detection

Object Detection Fail ──▶ Continue with face recognition
```

## Performance Characteristics

### Latency:
- Wake word detection: ~100-300ms
- Face recognition: ~1000ms intervals
- Object detection: ~1500ms intervals
- HTTP stream: ~66ms per frame (15 FPS)

### Resource Usage:
- Flash: ~3-4 MB (code + models)
- Internal RAM: ~200 KB
- PSRAM: ~2-3 MB
- CPU: 60-80% combined (both cores)

---

This architecture provides:
- ✓ Modular design
- ✓ Clean separation of concerns
- ✓ Resource efficiency
- ✓ Graceful degradation
- ✓ Easy to understand and maintain
