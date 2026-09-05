# Voice Activation Integration Plan

## Overview
Merging voice activation from AEGIS_Guardian-main into Gurdian_Master_old

## Key Features from AEGIS_Guardian-main
1. **ESP-SR (Speech Recognition)** - Wake word detection using "Hi ESP"
2. **AFE (Audio Front End)** - Audio processing pipeline
3. **I2S Microphone** - Audio input via esp_board_init
4. **State Management** - IDLE vs ACTIVE states
5. **Event-Driven Camera** - Camera only starts after wake word detection

## Integration Strategy

### Phase 1: Add Dependencies
- ✓ Add esp-sr to idf_component.yml
- ✓ Add esp_codec_dev to idf_component.yml

### Phase 2: Add Voice Recognition Headers
Add to includes:
```cpp
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_board_init.h"
#include "model_path.h"
```

### Phase 3: System State Management
Add system state enum and variables:
- System states: SYSTEM_IDLE, SYSTEM_ACTIVATING, SYSTEM_ACTIVE
- Wake word event group bit
- AFE handle and data structures
- Task flags for voice processing

### Phase 4: Voice Recognition Tasks
Add two new tasks:
1. **feed_task** - Feeds microphone data to AFE pipeline
2. **detect_task** - Detects wake word and triggers camera activation

### Phase 5: Modified Initialization Sequence
Change boot sequence to:
1. Initialize NVS
2. Initialize I2S microphone (esp_board_init)
3. Initialize WiFi
4. Initialize SPIFFS
5. Load voice recognition models
6. Initialize AFE
7. Start voice recognition tasks
8. Wait for wake word
9. When wake word detected -> initialize camera
10. Start HTTP server
11. Start face/object AI tasks

### Phase 6: Configuration Options
Add compile-time options:
- ENABLE_VOICE_ACTIVATION - Enable/disable voice activation
- WAKE_WORD - Configure wake word phrase
- VOICE_THRESHOLD - Set wake word detection threshold

## Modified Boot Flow

### Without Voice Activation (Current):
NVS → Camera → WiFi → SPIFFS → HTTP Server → AI Tasks → Ready

### With Voice Activation (New):
NVS → I2S Mic → WiFi → SPIFFS → Voice Models → AFE Tasks → [WAIT FOR WAKE WORD] → Camera → HTTP Server → AI Tasks → Ready

## File Changes Required

### main.cpp
- Add voice recognition includes
- Add system state variables
- Add voice_init() function
- Add feed_task() function
- Add detect_task() function
- Modify app_main() to initialize voice first
- Modify camera initialization to be event-driven
- Add camera_activation_task()

### idf_component.yml
- ✓ Already updated with esp-sr and esp_codec_dev dependencies

## Testing Steps
1. Build with voice activation disabled - verify existing functionality
2. Build with voice activation enabled - verify compilation
3. Flash and test wake word detection
4. Verify camera starts only after wake word
5. Verify all AI features work after activation
6. Test re-activation scenarios

## Rollback Plan
- Original main.cpp backed up to main.cpp.backup
- Can easily disable with #define ENABLE_VOICE_ACTIVATION 0

## Notes
- Voice activation requires ESP32-S3 with PSRAM
- Requires I2S microphone connected to board
- Wake word model stored in "model" partition
- Default wake word is "Hi ESP" (Chinese and English models available)
