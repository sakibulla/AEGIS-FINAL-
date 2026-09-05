# 🚀 Quick Start Guide - Guardian System

## What You Have Now

Your ESP32-S3-EYE is now a **Voice-Activated AI Security Camera** with:

✅ **Voice Activation** - Say "Hi ESP" to turn it on  
✅ **Face Recognition** - Knows owner from intruders  
✅ **Object Detection** - Detects people, chairs, lights, screens  
✅ **HTTP Streaming** - Watch live video from browser  
✅ **Dual AI Processing** - Face + Object detection simultaneously  

---

## 🎬 How to Use

### Step 1: Flash the Firmware

```bash
cd E:\Gurdian\Gurdian_Master_old
cmd /c "E:\Espressif\idf_cmd_init.bat & cd /d E:\Gurdian\Gurdian_Master_old & idf.py flash monitor"
```

### Step 2: Watch the Boot Process

You'll see on serial monitor:

```
I (xxx) VOICE: Initializing voice activation...
I (xxx) VOICE: 🎤 Listening... Say 'Hi ESP' to activate!
I (xxx) AEGIS: ====================================
I (xxx) AEGIS:   Waiting for wake word...
I (xxx) AEGIS:   Say 'Hi ESP' to activate camera
I (xxx) AEGIS: ====================================
```

### Step 3: Activate with Your Voice

Say clearly: **"Hi ESP"** (or **"乐鑫"** in Chinese)

You'll see:

```
I (xxx) VOICE: ========================================
I (xxx) VOICE:   WAKE WORD DETECTED!
I (xxx) VOICE: ========================================
I (xxx) AEGIS: Wake word detected! Activating system...
I (xxx) AEGIS: Initializing camera...
I (xxx) AEGIS: Camera OK
```

### Step 4: Wait for Wi-Fi Connection

```
I (xxx) AEGIS: Wi-Fi connecting...
I (xxx) AEGIS: IP: 192.168.1.XXX  ← COPY THIS IP!
I (xxx) AEGIS: HTTP server started
I (xxx) AEGIS: Stream: http://192.168.1.XXX/stream
```

### Step 5: Open the Stream in Browser

Navigate to: `http://192.168.1.XXX/stream`

You'll see:
- 📹 Live video from the camera
- 👤 First face gets auto-enrolled as OWNER
- 🔍 Object detection running in background

### Step 6: Monitor Security Status

Watch the serial console:

```
I (xxx) AEGIS: Security=OWNER | OwnerID=0 | Enrolled=YES
I (xxx) AEGIS: >>> OWNER CONFIRMED
I (xxx) AEGIS: [OBJ] person 92.5% x=100 y=50 w=150 h=200
```

---

## 🎯 Simple Flow

```
Power ON
   ↓
🎤 Listening for wake word...
   ↓
Say "Hi ESP"
   ↓
🎉 Wake word detected!
   ↓
📷 Camera starts
   ↓
🌐 Wi-Fi connects
   ↓
👤 First face = OWNER
   ↓
🔄 Continuous monitoring
   ├─ Face recognition (1 sec intervals)
   └─ Object detection (1.5 sec intervals)
```

---

## 🔧 Common Scenarios

### Scenario 1: "It's not detecting the wake word!"

**Possible causes:**
- No wake word models flashed (check serial: "No models found")
- Microphone not working (check GPIO connections)
- Too much background noise
- Speaking too far from microphone (optimal: 30-50cm)

**Solution:**
- Check if you see: `Found 1 model(s)` in serial log
- If "No models found", you need to flash wake word models
- Speak clearly and directly toward the device

### Scenario 2: "I don't want voice activation"

Edit `main/main.cpp` line 37:

```cpp
#define ENABLE_VOICE_ACTIVATION 0  // Disable voice
```

Rebuild and flash. Camera will start immediately.

### Scenario 3: "Face recognition not working"

**First boot:**
- Stand in front of camera after wake word
- First face detected becomes OWNER automatically
- Check serial: "OWNER ENROLLED - OWNER ID = 0"

**Subsequent boots:**
- If `RESET_FACE_DB_ON_BOOT = 1`, database clears each boot
- Change to `0` to keep owner enrolled permanently

### Scenario 4: "How do I view the stream?"

1. Note IP address from serial: `IP: 192.168.1.XXX`
2. Open browser: `http://192.168.1.XXX/stream`
3. You should see live MJPEG video

**If stream doesn't work:**
- Verify device and computer on same Wi-Fi network
- Check firewall settings
- Try different browser (Chrome/Firefox work best)

---

## 📊 What the System Does

### Face Recognition (Every 1 second)
- Detects faces in camera view
- Compares to owner face
- Updates security state:
  - `OWNER` - Recognized owner (similarity ≥ 40%)
  - `INTRUDER` - Unknown person detected
  - `NO_FACE` - No face visible

### Object Detection (Every 1.5 seconds)
- Runs Edge Impulse ML model
- Detects: person, chair, light, screen
- Shows bounding boxes and confidence

### HTTP Streaming (Continuous)
- Serves MJPEG stream at ~15 FPS
- Accessible from any browser
- Multiple clients can connect

---

## 🎤 Voice Activation Details

### Supported Wake Words
- **English:** "Hi ESP"
- **Chinese:** "乐鑫" (Lè Xīn - pronounced "Luh Shin")

### How It Works
```
Microphone (MSM261S4030H0) on GPIO 2
    ↓
I2S Interface (16kHz, 16-bit, mono)
    ↓
AFE Processing Pipeline
    ├─ Voice Activity Detection
    ├─ Noise Reduction
    └─ Wake Word Matching
        ↓
"Hi ESP" detected → Activate camera
```

### Requirements
- Wake word models must be in flash partition labeled "model"
- Without models: system falls back to normal mode (no voice)
- Models are loaded at boot automatically

---

## 🛠️ Configuration Cheat Sheet

### Enable/Disable Voice
```cpp
// main/main.cpp line 37
#define ENABLE_VOICE_ACTIVATION 1  // 1=ON, 0=OFF
```

### Wi-Fi Credentials
```cpp
// main/main.cpp lines 48-49
#define WIFI_SSID "YourWiFiName"
#define WIFI_PASS "YourPassword"
```

### Face Database Behavior
```cpp
// main/main.cpp line 104
#define RESET_FACE_DB_ON_BOOT 1  // 1=Clear each boot, 0=Keep forever
```

### Face Recognition Threshold
```cpp
// main/main.cpp line 108
#define OWNER_SIMILARITY_THRESHOLD 0.40f  // 0.40 = 40% match required
```

---

## 📈 System Status

Check status every 5 seconds in serial monitor:

```
Security=OWNER | OwnerID=0 | Enrolled=YES | FreeHeap=45632 | PSRAM=3145728
```

**Meaning:**
- `Security=OWNER` - Currently showing owner's face
- `OwnerID=0` - Owner is face ID 0 in database
- `Enrolled=YES` - Owner face is saved
- `FreeHeap=45632` - Free internal RAM (bytes)
- `PSRAM=3145728` - Free PSRAM (bytes)

---

## 🎯 Success Checklist

After flashing, you should see:

- [x] Boot messages in serial monitor
- [x] "Waiting for wake word..." message
- [x] Wake word detection when you say "Hi ESP"
- [x] Camera initialization
- [x] Wi-Fi connection with IP address
- [x] "OWNER ENROLLED" when first face detected
- [x] HTTP stream accessible from browser
- [x] Face recognition working (OWNER/INTRUDER detection)
- [x] Object detection results in serial log

---

## 🚨 Troubleshooting

### Voice not working
```
W (xxx) VOICE: No models found in 'model' partition
```
**Solution:** Wake word models need to be flashed. System will work in normal mode (no voice).

### Camera fails
```
E (xxx) AEGIS: Camera initialization failed: 0x105
```
**Solution:** Check GPIO connections, verify ESP32-S3-EYE board

### Wi-Fi fails
```
W (xxx) AEGIS: Wi-Fi disconnected
```
**Solution:** Check SSID/password in code, verify 2.4GHz network

### Stream not accessible
**Solution:** 
- Verify IP address from serial log
- Check firewall/network settings
- Ensure device and computer on same network

---

## 📚 Documentation Files

Full details available in project root:

- `SYSTEM_WORKFLOW.md` - Complete system flow diagram
- `VOICE_ACTIVATION_STATUS.md` - Voice integration status
- `VOICE_ACTIVATION_GUIDE.md` - Detailed voice guide
- `VOICE_ACTIVATION_API.md` - Voice API reference
- `VOICE_ACTIVATION_TROUBLESHOOTING.md` - Voice issues
- `ARCHITECTURE.md` - System architecture
- `START_HERE.md` - Original start guide

---

## 🎉 That's It!

Your Guardian system is complete and ready to use!

**Next time you power on:**
1. Say "Hi ESP"
2. Wait for camera to start
3. Open `http://<IP>/stream` in browser
4. System monitors for owner vs intruder

**Questions?** Check the detailed documentation files above!

---

**Built with ESP32-S3-EYE + ESP-IDF v5.5.5 + ESP-SR v2.4.7 + Edge Impulse** 🚀
