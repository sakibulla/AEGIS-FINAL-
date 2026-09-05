# AEGIS Guardian - Voice Activation Integration

## Overview

This package integrates voice activation from AEGIS_Guardian-main into your Guardian_Master_old project. The system now supports wake word detection using ESP-SR (Speech Recognition) library.

## What's New

### Voice Activation Feature
- **Wake Word**: "Hi ESP" (customizable)
- **Lazy Loading**: Camera and AI systems only start after wake word detection
- **Resource Efficient**: Mic runs in low-power mode until activated
- **Toggle**: Can be enabled/disabled with a simple #define

### Files Added/Modified

#### New Files:
1. **`main/voice_activation.h`** - Voice activation module interface
2. **`main/voice_activation.cpp`** - Voice activation implementation
3. **`INTEGRATION_GUIDE.md`** - Detailed integration instructions
4. **`VOICE_ACTIVATION_INTEGRATION_PLAN.md`** - Technical design document
5. **`apply_voice_activation.py`** - Automated integration script
6. **`README_VOICE_ACTIVATION.md`** - This file

#### Modified Files:
1. **`main/idf_component.yml`** - Added esp-sr and esp_codec_dev dependencies
2. **`main/CMakeLists.txt`** - Added voice_activation.cpp and new requirements
3. **`main/main.cpp`** - Will be modified to integrate voice activation

#### Backups:
- **`main/main.cpp.backup`** - Original main.cpp before any changes

## Quick Start

### Option 1: Automatic Integration (Recommended)

Run the Python script to automatically integrate voice activation:

```bash
cd E:\Gurdian\Gurdian_Master_old
python apply_voice_activation.py
```

This will:
- Create a timestamped backup
- Modify main.cpp to add voice activation
- Preserve all existing functionality

### Option 2: Manual Integration

Follow the step-by-step guide in `INTEGRATION_GUIDE.md`.

## Building and Flashing

### 1. Build the Project

```bash
cd E:\Gurdian\Gurdian_Master_old
idf.py build
```

### 2. Flash to ESP32-S3

```bash
idf.py flash monitor
```

### 3. Test Voice Activation

Once flashed, the system will:
1. Initialize microphone
2. Load wake word models
3. Wait for "Hi ESP"
4. Activate camera and AI systems after wake word

## Expected Boot Sequence

### With Voice Activation Enabled:
```
[AEGIS] ====================================
[AEGIS]           AEGIS START
[AEGIS] ====================================
[AEGIS] Voice activation: ENABLED
[VOICE] Initializing voice activation...
[VOICE] Initializing I2S microphone...
[VOICE] Loading wake word models...
[VOICE]   Found wake word model: hilexin5_wn5
[VOICE]   Using wake word model: hilexin5_wn5
[VOICE] Voice activation initialized successfully
[VOICE] Starting voice recognition tasks...
[VOICE] Feed task started (chunksize=480, channels=1)
[VOICE] Detect task started - listening for wake word...
[VOICE] Voice recognition tasks started
[VOICE] Say the wake word to activate the system!
[AEGIS] ====================================
[AEGIS]   Waiting for wake word...
[AEGIS]   Say 'Hi ESP' to activate camera
[AEGIS] ====================================

[... Say "Hi ESP" ...]

[VOICE] ========================================
[VOICE]   WAKE WORD DETECTED!
[VOICE]   Model: 1  Word: 1
[VOICE] ========================================
[AEGIS] Wake word detected! Activating system...
[AEGIS] Initializing camera...
[AEGIS] Camera OK
[AEGIS] HTTP server started
[AEGIS] Face task created
[AEGIS] AI task created
[AEGIS] ====================================
[AEGIS]           AEGIS READY
[AEGIS] ====================================
[AEGIS] Mode: VOICE ACTIVATED
[AEGIS] Wake word: Hi ESP
```

### With Voice Activation Disabled:
```
[AEGIS] ====================================
[AEGIS]           AEGIS START
[AEGIS] ====================================
[AEGIS] Voice activation: DISABLED (normal mode)
[AEGIS] Initializing camera...
[AEGIS] Camera OK
...
[AEGIS] Mode: ALWAYS ON
```

## Configuration

### Enable/Disable Voice Activation

Edit `main/main.cpp`:

```cpp
// Set to 1 to enable, 0 to disable
#define ENABLE_VOICE_ACTIVATION 1
```

### Adjust Wake Word Sensitivity

Edit `main/voice_activation.cpp`, around line 60:

```cpp
// Lower = more sensitive (more false positives)
// Higher = less sensitive (might miss wake word)
g_voice_ctx.afe_handle->set_wakenet_threshold(afe_data, 1, 0.6f);
```

## Hardware Requirements

### Required:
- ESP32-S3 with PSRAM ✓ (you have this)
- I2S Microphone (check your board)
- Flash partition named "model" with wake word models

### Wake Word Model Installation

The wake word models should be in a flash partition named "model". If you don't have this:

1. Check your `partitions.csv`
2. Add a model partition:
```csv
model, data, 0x06, 0x400000, 0x100000,
```
3. Flash the wake word model to that partition

## Troubleshooting

### Issue: "No models found in 'model' partition"

**Cause**: Wake word models not flashed to device

**Solution**: 
1. Download wake word models from Espressif
2. Flash to the "model" partition
3. Or disable voice activation temporarily

### Issue: "Failed to initialize I2S"

**Cause**: Board doesn't have I2S microphone or wrong board selected

**Solution**:
1. Check if your board has an I2S microphone
2. Verify correct board selected in `idf.py menuconfig`
3. Or disable voice activation

### Issue: Voice activation fails, but system doesn't start

**Current Behavior**: If voice init fails, system falls back to normal mode

**To Force Normal Mode**: Set `ENABLE_VOICE_ACTIVATION` to 0

## Project Structure

```
Gurdian_Master_old/
├── main/
│   ├── main.cpp                    # Main application (modified)
│   ├── main.cpp.backup             # Original backup
│   ├── voice_activation.h          # Voice module interface (NEW)
│   ├── voice_activation.cpp        # Voice module implementation (NEW)
│   ├── CMakeLists.txt              # Modified to include voice files
│   └── idf_component.yml           # Modified dependencies
├── backups/                        # Timestamped backups (created by script)
├── INTEGRATION_GUIDE.md            # Step-by-step integration guide
├── VOICE_ACTIVATION_INTEGRATION_PLAN.md  # Technical design doc
├── apply_voice_activation.py       # Automated integration script
└── README_VOICE_ACTIVATION.md      # This file
```

## Features Preserved

All original features remain intact:
- ✓ Face recognition with owner enrollment
- ✓ Object detection using Edge Impulse
- ✓ HTTP video streaming
- ✓ Security state management
- ✓ SPIFFS file system
- ✓ WiFi connectivity

## Advantages of Voice Activation

1. **Power Saving**: Camera/AI only run when needed
2. **Privacy**: Video stream only active after voice command
3. **Security**: Additional activation barrier
4. **Resource Management**: Reduced load until activated

## Testing Checklist

- [ ] Build project successfully
- [ ] Flash to device
- [ ] Voice initialization succeeds
- [ ] Say wake word and verify detection
- [ ] Camera starts after wake word
- [ ] HTTP stream accessible
- [ ] Face recognition works
- [ ] Object detection works
- [ ] Test with voice disabled (set to 0)
- [ ] Verify fallback to normal mode if voice fails

## Support

### Documentation:
- `INTEGRATION_GUIDE.md` - Integration steps
- `VOICE_ACTIVATION_INTEGRATION_PLAN.md` - Technical details

### Recovery:
If something goes wrong, restore from backup:
```bash
copy main\main.cpp.backup main\main.cpp
```

Or use timestamped backups in `backups/` directory.

## Future Enhancements

Possible improvements:
- Multiple wake words
- Custom wake word training
- Voice commands for system control
- Continuous voice interaction
- Voice feedback/responses

## Credits

- Voice activation code adapted from AEGIS_Guardian-main
- Uses Espressif ESP-SR library
- Integration created for Guardian_Master_old project

## License

Same license as the original project.

---

**Ready to integrate? Run `python apply_voice_activation.py` to get started!**
