# Quick Start - Voice Activation Integration

## 5-Minute Integration

### Step 1: Backup Check
```bash
✓ Backup already created: main/main.cpp.backup
```

### Step 2: Run Integration Script
```bash
cd E:\Gurdian\Gurdian_Master_old
python apply_voice_activation.py
```

**Expected Output:**
```
======================================================================
Voice Activation Integration Script
======================================================================

✓ Backup created: backups/main.cpp.20260808_XXXXXX
✓ Reading main.cpp...
✓ Adding voice activation toggle...
✓ Modifying app_main() function...
✓ Adding voice status to boot messages...
✓ Writing modified main.cpp...

======================================================================
✓ Voice activation integrated successfully!
======================================================================
```

### Step 3: Build
```bash
idf.py build
```

**Watch for:**
- ✓ Compilation successful
- ✓ No errors about missing files
- ✓ esp-sr library found

### Step 4: Flash
```bash
idf.py flash monitor
```

### Step 5: Test
1. Watch boot sequence
2. Wait for "Say 'Hi ESP' to activate camera"
3. Say "Hi ESP" clearly
4. Watch for "WAKE WORD DETECTED!"
5. Verify camera activates
6. Test face recognition
7. Test object detection
8. Access HTTP stream

## Troubleshooting Quick Fixes

### Build Fails with "voice_activation.h not found"
```bash
# Check file exists:
dir main\voice_activation.h

# If missing, files are in the current directory
# They should be in main/ subdirectory
```

### Build Fails with "esp-sr not found"
```bash
# Clean and rebuild:
idf.py fullclean
idf.py build
```

### Voice Init Fails at Runtime
**Don't worry!** System automatically falls back to normal mode.

To disable voice permanently:
1. Edit `main/main.cpp`
2. Find `#define ENABLE_VOICE_ACTIVATION 1`
3. Change to `#define ENABLE_VOICE_ACTIVATION 0`
4. Rebuild and flash

### Wake Word Not Detected
1. Speak louder and clearer
2. Try "Hey, ESP" (more distinct pronunciation)
3. Adjust threshold in `voice_activation.cpp`:
   ```cpp
   set_wakenet_threshold(afe_data, 1, 0.5f);  // Lower = more sensitive
   ```
4. Rebuild and flash

### Camera Doesn't Start
Check logs for:
- "Voice init failed" → Falls back to normal mode automatically
- "Camera initialization failed" → Hardware issue

## Manual Integration (If Script Fails)

### 1. Edit main.cpp - Add Include
After line 28 (after all includes), add:
```cpp
#define ENABLE_VOICE_ACTIVATION 1

#if ENABLE_VOICE_ACTIVATION
#include "voice_activation.h"
#endif
```

### 2. Edit app_main() - Add Voice Init
Find this in app_main():
```cpp
    ESP_ERROR_CHECK(ret);
```

After it, add:
```cpp
#if ENABLE_VOICE_ACTIVATION
    ESP_LOGI(TAG, "Voice activation: ENABLED");
    
    if (voice_activation_init() == ESP_OK) {
        voice_activation_start();
        
        ESP_LOGI(TAG, "====================================");
        ESP_LOGI(TAG, "  Waiting for wake word...");
        ESP_LOGI(TAG, "  Say 'Hi ESP' to activate camera");
        ESP_LOGI(TAG, "====================================");
        
        voice_activation_wait(portMAX_DELAY);
        
        ESP_LOGI(TAG, "Wake word detected! Activating system...");
    } else {
        ESP_LOGW(TAG, "Voice init failed, using normal mode");
    }
#else
    ESP_LOGI(TAG, "Voice activation: DISABLED");
#endif
```

### 3. Build and Test
```bash
idf.py build flash monitor
```

## Verification Checklist

- [ ] Python script ran successfully
- [ ] Build completed without errors
- [ ] Flash completed successfully
- [ ] Boot shows "Voice activation: ENABLED"
- [ ] System shows "Waiting for wake word..."
- [ ] Wake word detection works
- [ ] Camera starts after wake word
- [ ] HTTP stream accessible
- [ ] Face recognition works
- [ ] Object detection works

## Configuration

### Enable/Disable Voice
Edit `main/main.cpp`:
```cpp
#define ENABLE_VOICE_ACTIVATION 1  // 1=enabled, 0=disabled
```

### Adjust Sensitivity
Edit `main/voice_activation.cpp` (around line 60):
```cpp
set_wakenet_threshold(afe_data, 1, 0.6f);
// Lower (0.3) = more sensitive
// Higher (0.8) = less sensitive
// Default: 0.6
```

## Restore Original

### Quick Restore:
```bash
copy main\main.cpp.backup main\main.cpp
idf.py build flash monitor
```

### Or Just Disable:
```cpp
#define ENABLE_VOICE_ACTIVATION 0
```

## Support

### Documentation:
1. **README_VOICE_ACTIVATION.md** - Complete guide
2. **INTEGRATION_GUIDE.md** - Detailed steps
3. **ARCHITECTURE.md** - System design
4. **VOICE_ACTIVATION_INTEGRATION_PLAN.md** - Technical details

### Files:
- `main/voice_activation.h` - API
- `main/voice_activation.cpp` - Implementation
- `apply_voice_activation.py` - Integration script

## Common Commands

```bash
# Build
idf.py build

# Flash and monitor
idf.py flash monitor

# Clean build
idf.py fullclean
idf.py build

# Just monitor (after flash)
idf.py monitor

# Exit monitor
Ctrl + ]
```

## Expected Behavior

### With Voice ENABLED (Default):
```
Boot → Listen for wake word → "Hi ESP" → Activate camera → Full system running
```

### With Voice DISABLED:
```
Boot → Activate camera immediately → Full system running
```

## Success Indicators

**Build Success:**
```
Project build complete. To flash, run:
 idf.py flash
```

**Flash Success:**
```
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
```

**Voice Active:**
```
[VOICE] Voice activation initialized successfully
[VOICE] Voice recognition tasks started
[AEGIS] Waiting for wake word...
```

**Wake Word Detected:**
```
[VOICE] ========================================
[VOICE]   WAKE WORD DETECTED!
[VOICE] ========================================
```

**System Ready:**
```
[AEGIS] ====================================
[AEGIS]           AEGIS READY
[AEGIS] ====================================
[AEGIS] Mode: VOICE ACTIVATED
```

---

## Ready? Let's Go!

```bash
cd E:\Gurdian\Gurdian_Master_old
python apply_voice_activation.py
idf.py build flash monitor
```

**Then say "Hi ESP" and watch the magic happen! 🎤✨**
