# Voice Activation Integration Guide

## Quick Integration Steps

I've created a modular voice activation system that can be easily integrated into your existing code.

### Files Created:
1. `main/voice_activation.h` - Voice activation header
2. `main/voice_activation.cpp` - Voice activation implementation  
3. `VOICE_ACTIVATION_INTEGRATION_PLAN.md` - Detailed technical plan
4. `merge_voice_activation.py` - Helper script

### Dependencies Updated:
✓ `main/idf_component.yml` - Added esp-sr and esp_codec_dev

## Integration Method

### Option 1: Compile-Time Toggle (Recommended)

Add voice activation with an enable/disable flag:

#### Step 1: Add to main.cpp includes (after line 28)
```cpp
// Voice activation (optional)
#define ENABLE_VOICE_ACTIVATION 1

#if ENABLE_VOICE_ACTIVATION
#include "voice_activation.h"
#endif
```

#### Step 2: Modify app_main() function

Replace the existing app_main() with this modified version:

```cpp
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "          AEGIS START");
    ESP_LOGI(TAG, "====================================");

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

#if ENABLE_VOICE_ACTIVATION
    /* ===== VOICE ACTIVATION MODE ===== */
    ESP_LOGI(TAG, "Voice activation: ENABLED");
    
    // Initialize voice recognition BEFORE camera
    if (voice_activation_init() == ESP_OK) {
        // Start voice recognition tasks
        voice_activation_start();
        
        // Initialize WiFi and SPIFFS first
        s_camera_mutex = xSemaphoreCreateMutex();
        if (!s_camera_mutex) {
            ESP_LOGE(TAG, "Camera mutex creation failed");
            return;
        }
        
        if (!init_spiffs()) {
            ESP_LOGE(TAG, "SPIFFS initialization failed");
            return;
        }
        
        wifi_init();
        
        ESP_LOGI(TAG, "====================================");
        ESP_LOGI(TAG, "  Waiting for wake word...");
        ESP_LOGI(TAG, "  Say 'Hi ESP' to activate camera");
        ESP_LOGI(TAG, "====================================");
        
        // Wait for wake word (blocks until detected)
        voice_activation_wait(portMAX_DELAY);
        
        ESP_LOGI(TAG, "Wake word detected! Activating camera...");
    } else {
        ESP_LOGW(TAG, "Voice activation init failed, using normal mode");
    }
#else
    /* ===== NORMAL MODE (No voice activation) ===== */
    ESP_LOGI(TAG, "Voice activation: DISABLED");
#endif

    /* Camera mutex */
    if (!s_camera_mutex) {
        s_camera_mutex = xSemaphoreCreateMutex();
        if (!s_camera_mutex) {
            ESP_LOGE(TAG, "Camera mutex creation failed");
            return;
        }
    }

    /* Camera */
    if (init_camera() != ESP_OK) {
        ESP_LOGE(TAG, "Camera initialization failed");
        return;
    }

#if !ENABLE_VOICE_ACTIVATION
    /* SPIFFS (if not already initialized) */
    if (!init_spiffs()) {
        ESP_LOGE(TAG, "SPIFFS initialization failed");
        return;
    }

    /* Wi-Fi (if not already initialized) */
    wifi_init();
#endif

    /* HTTP stream */
    start_stream_server();

    /* Give camera + Wi-Fi time to settle */
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Face task */
    BaseType_t face_result = xTaskCreatePinnedToCore(
        face_task, "face_task", 16 * 1024, NULL, 5, NULL, 0);
    
    if (face_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create face task");
    } else {
        ESP_LOGI(TAG, "Face task created");
    }

    /* Object AI task */
    vTaskDelay(pdMS_TO_TICKS(500));
    
    BaseType_t ai_result = xTaskCreatePinnedToCore(
        ai_task, "ai_task", 16 * 1024, NULL, 3, NULL, 1);
    
    if (ai_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create AI task");
        ESP_LOGE(TAG, "Largest internal block: %u",
            (unsigned)heap_caps_get_largest_free_block(
                MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    } else {
        ESP_LOGI(TAG, "AI task created");
    }

    /* Status task */
    BaseType_t status_result = xTaskCreatePinnedToCore(
        status_task, "status_task", 4096, NULL, 2, NULL, 0);
    
    if (status_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create status task");
    }

    /* READY */
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "          AEGIS READY");
    ESP_LOGI(TAG, "====================================");
    
#if ENABLE_VOICE_ACTIVATION
    ESP_LOGI(TAG, "Mode: VOICE ACTIVATED");
    ESP_LOGI(TAG, "Wake word: Hi ESP");
#else
    ESP_LOGI(TAG, "Mode: ALWAYS ON");
#endif
    
    ESP_LOGI(TAG, "Camera: QVGA 320x240");
    ESP_LOGI(TAG, "Face AI: ENABLED");
    ESP_LOGI(TAG, "Object AI: ENABLED");
    ESP_LOGI(TAG, "====================================");
}
```

### Option 2: Always-On Voice Activation

If you want voice activation always enabled, simplify by removing the `#if ENABLE_VOICE_ACTIVATION` conditionals.

## Build and Test

### 1. Build with Voice Disabled (Test Existing Functionality)
```bash
# Set ENABLE_VOICE_ACTIVATION to 0 in main.cpp
idf.py build
idf.py flash monitor
```

### 2. Build with Voice Enabled
```bash
# Set ENABLE_VOICE_ACTIVATION to 1 in main.cpp
idf.py build
idf.py flash monitor
```

### 3. Expected Boot Sequence (Voice Enabled)
```
[AEGIS] AEGIS START
[AEGIS] Voice activation: ENABLED
[VOICE] Initializing voice activation...
[VOICE] Initializing I2S microphone...
[VOICE] Loading wake word models...
[VOICE]   Found wake word model: ...
[VOICE] Voice recognition tasks started
[AEGIS] ====================================
[AEGIS]   Waiting for wake word...
[AEGIS]   Say 'Hi ESP' to activate camera
[AEGIS] ====================================
```

### 4. After Saying "Hi ESP"
```
[VOICE] ========================================
[VOICE]   WAKE WORD DETECTED!
[VOICE] ========================================
[AEGIS] Wake word detected! Activating camera...
[AEGIS] Initializing camera...
[AEGIS] Camera OK
[AEGIS] HTTP server started
[AEGIS] Face task created
[AEGIS] AI task created
[AEGIS] AEGIS READY
```

## Hardware Requirements

### For Voice Activation:
- ESP32-S3 with PSRAM (you have this ✓)
- I2S Microphone (connected to board)
- Wake word model in flash partition named "model"

### Microphone Connection:
Check your board's schematic for I2S microphone pins. The `esp_board_init()` function should handle the pin configuration automatically if your board is supported.

## Troubleshooting

### Issue: "No models found in 'model' partition"
**Solution**: Flash wake word models to the "model" partition
```bash
# Download wake word models from Espressif
# Flash models to partition
esptool.py --port COM_PORT write_flash 0x<model_partition_address> wakenet_model.bin
```

### Issue: Voice activation init fails
**Fallback**: System automatically falls back to normal mode (camera starts immediately)

### Issue: I2S initialization fails
**Check**: 
- Board has I2S microphone
- Correct board selected in menuconfig
- I2S pins configured correctly

## Customization

### Change Wake Word Threshold
Edit `voice_activation.cpp` line 60-61:
```cpp
g_voice_ctx.afe_handle->set_wakenet_threshold(afe_data, 1, 0.6f);  // Adjust 0.6
```
- Lower value = more sensitive (more false positives)
- Higher value = less sensitive (might miss wake word)

### Add Custom Wake Word
Replace the wake word model in the "model" partition with your custom trained model.

## Files Modified

1. ✓ `main/idf_component.yml` - Added dependencies
2. ✓ `main/voice_activation.h` - Created new (modular interface)
3. ✓ `main/voice_activation.cpp` - Created new (implementation)
4. 🔄 `main/main.cpp` - Needs manual modification (see above)
5. ✓ `main/CMakeLists.txt` - May need to add voice_activation.cpp

## Next Step

Modify your `main/main.cpp` file following Step 1 and Step 2 above, then build and test!
