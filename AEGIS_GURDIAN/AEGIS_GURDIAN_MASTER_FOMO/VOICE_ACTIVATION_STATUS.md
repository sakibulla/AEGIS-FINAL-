# Voice Activation Integration Status

## ✅ COMPLETED - Build Successful!

**Date:** August 8, 2026  
**Build Status:** SUCCESS  
**Binary Size:** 4.3 MB (31% free space)  
**ESP-SR Version:** v2.4.7

---

## Implementation Summary

Voice activation ("Hi ESP" wake word detection) has been successfully integrated from AEGIS_Guardian-main into Gurdian_Master_old project.

### Key Changes

1. **Voice Activation Module Created**
   - `main/voice_activation.h` - Header with API definitions
   - `main/voice_activation.cpp` - Implementation with ESP-SR integration
   - Modular design for easy enable/disable

2. **ESP-SR API Integration** ✅
   - Fixed API compatibility issues with ESP-SR v2.4.7
   - Used `afe_config_init()` for proper configuration initialization
   - Used `esp_afe_handle_from_config()` for AFE handle creation
   - Properly configured for ESP32-S3-EYE hardware

3. **I2S Microphone Configuration** ✅
   - Configured for ESP32-S3-EYE built-in MSM261S4030H0 MEMS microphone
   - Pin Configuration:
     - I2S_MIC_SERIAL_CLOCK: GPIO_41
     - I2S_MIC_LEFT_RIGHT_CLOCK: GPIO_42
     - I2S_MIC_SERIAL_DATA: GPIO_2
   - Sample Rate: 16000 Hz
   - Bits per Sample: 16
   - Channels: 1 (mono)

4. **Dependencies Added**
   - ESP-SR (Speech Recognition) library - v2.4.7
   - ESP Codec Dev library
   - Updated `main/idf_component.yml`

5. **Build System Updates**
   - Added `voice_activation.cpp` to `main/CMakeLists.txt`
   - Enabled voice activation: `ENABLE_VOICE_ACTIVATION 1`

---

## Architecture

### Voice Activation Flow

```
Microphone (GPIO_2) 
    ↓
I2S Driver (16kHz, 16-bit, mono)
    ↓
AFE (Audio Front End)
    ├── Voice Activity Detection (VAD)
    ├── Speech Enhancement (SE)
    └── Wake Word Detection (WakeNet)
        ↓
"Hi ESP" or "乐鑫" detected
        ↓
Event signaled to main application
```

### Task Architecture

- **voice_feed_task** (CPU 0, Priority 5)
  - Reads audio from I2S microphone
  - Feeds data to AFE pipeline
  - Chunk-based processing

- **voice_detect_task** (CPU 1, Priority 5)
  - Fetches AFE results
  - Detects wake word events
  - Signals activation via FreeRTOS event group

---

## API Functions

### `esp_err_t voice_activation_init()`
Initializes I2S microphone, loads wake word models, and configures AFE.

**Returns:** 
- `ESP_OK` - Success
- `ESP_ERR_NOT_FOUND` - No wake word models found in "model" partition
- `ESP_FAIL` - Initialization failed

### `esp_err_t voice_activation_start()`
Starts voice recognition tasks on both CPU cores.

### `bool voice_activation_wait(uint32_t timeout_ms)`
Waits for wake word detection with optional timeout.

**Parameters:**
- `timeout_ms` - Timeout in milliseconds (`portMAX_DELAY` for infinite)

**Returns:** `true` if wake word detected, `false` on timeout

### `void voice_activation_stop()`
Stops voice recognition tasks.

### `voice_state_t voice_activation_get_state()`
Returns current voice activation state.

---

## Configuration Options

### AFE Configuration (in `voice_activation.cpp`)

```cpp
afe_config_t *afe_config = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
afe_config->pcm_config.sample_rate = 16000;
afe_config->afe_perferred_core = 0;
afe_config->afe_perferred_priority = 5;
afe_config->afe_ringbuf_size = 50;
afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
afe_config->wakenet_init = true;
afe_config->wakenet_mode = DET_MODE_90;  // 90% detection mode
afe_config->vad_init = true;
afe_config->vad_mode = VAD_MODE_3;
afe_config->se_init = true;
afe_config->aec_init = false;  // No echo cancellation
```

### Wake Word Threshold

```cpp
// Set detection thresholds (0.0 - 1.0, default: 0.6)
afe_handle->set_wakenet_threshold(afe_data, 1, 0.6f);
afe_handle->set_wakenet_threshold(afe_data, 2, 0.6f);
```

---

## ⚠️ IMPORTANT: Wake Word Models Required

Voice activation requires wake word models to be flashed to the ESP32-S3-EYE. The models should be in a flash partition labeled **"model"**.

### Without Models
- `voice_activation_init()` will return `ESP_ERR_NOT_FOUND`
- System will continue without voice activation
- Only camera/face detection features will work

### With Models
- Wake words: "Hi ESP" (English) or "乐鑫" (Chinese)
- Full voice activation available
- System waits for wake word before activating camera

---

## Testing Voice Activation

### On Device (when models are available)

1. **Flash the firmware:**
   ```bash
   idf.py flash monitor
   ```

2. **Look for initialization logs:**
   ```
   I (xxxx) VOICE: Initializing voice activation for ESP32-S3-EYE...
   I (xxxx) VOICE: I2S microphone initialized successfully
   I (xxxx) VOICE: Found N model(s):
   I (xxxx) VOICE:   [0] model_name
   I (xxxx) VOICE: Voice activation initialized successfully!
   ```

3. **Say the wake word:**
   - English: "Hi ESP"
   - Chinese: "乐鑫" (Lè Xīn)

4. **Confirmation:**
   ```
   I (xxxx) VOICE: ========================================
   I (xxxx) VOICE:   WAKE WORD DETECTED!
   I (xxxx) VOICE: ========================================
   ```

### Troubleshooting

**No models found:**
- Verify "model" partition exists in `partitions.csv`
- Flash wake word model binary to "model" partition
- Check model compatibility with ESP-SR v2.4.7

**I2S initialization fails:**
- Check GPIO pin configuration matches ESP32-S3-EYE
- Verify no GPIO conflicts with camera pins

**Poor detection:**
- Adjust wake word threshold (0.5-0.7 recommended)
- Check microphone distance (30-50cm optimal)
- Reduce background noise

---

## Build Information

```
Project build complete. To flash, run:
  idf.py flash
or
  idf.py -p PORT flash

Binary Information:
  esp32s3_vision_project.bin size: 0x41d960 bytes (4.3 MB)
  Smallest app partition: 0x600000 bytes (6 MB)
  Free space: 0x1e26a0 bytes (31%)
```

---

## Next Steps

1. **Flash Wake Word Models** (if not already done)
   - Obtain wake word model binary from Edge Impulse or ESP-SR
   - Flash to "model" partition
   - Restart device

2. **Test Voice Activation**
   - Monitor serial output
   - Say "Hi ESP" or "乐鑫"
   - Verify wake word detection

3. **Fine-tune Detection**
   - Adjust threshold based on environment
   - Modify VAD mode if needed
   - Test in various noise conditions

4. **Integration with Camera System**
   - System currently activates camera after wake word
   - Can be modified to trigger specific actions
   - See `main/main.cpp` for integration example

---

## Files Modified/Created

### Created
- `main/voice_activation.h`
- `main/voice_activation.cpp`
- `VOICE_ACTIVATION_GUIDE.md`
- `VOICE_ACTIVATION_API.md`
- `VOICE_ACTIVATION_TROUBLESHOOTING.md`
- `VOICE_ACTIVATION_HARDWARE.md`
- `ESP_SR_INTEGRATION_NOTES.md`
- `WAKE_WORD_MODELS.md`
- `NEXT_STEPS_VOICE.md`
- This status file

### Modified
- `main/main.cpp` - Added voice activation toggle and integration
- `main/CMakeLists.txt` - Added voice_activation.cpp to sources
- `main/idf_component.yml` - Added esp-sr and esp_codec_dev dependencies

---

## Success Criteria ✅

- [x] Code compiles without errors
- [x] Voice activation module is modular
- [x] I2S microphone properly configured
- [x] ESP-SR API correctly integrated
- [x] AFE configuration matches ESP32-S3-EYE hardware
- [x] Build size within acceptable limits (31% free)
- [x] Voice activation can be enabled/disabled via compile flag
- [ ] Wake word models flashed (requires user action)
- [ ] Wake word detection tested on device (requires models)

---

## Documentation Available

All documentation files have been created in the project root:
- Complete API reference
- Hardware configuration guide  
- Troubleshooting guide
- ESP-SR integration notes
- Wake word model information
- Next steps guide

**The voice activation integration is complete and ready for testing!**
