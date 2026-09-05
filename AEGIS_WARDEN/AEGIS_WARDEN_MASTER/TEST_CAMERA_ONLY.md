# Camera Streaming Test (Without AI)

## Problem Diagnosis

Your logs show:
```
W (117405) httpd_txrx: httpd_sock_err: error in send : 104
I (117406) CAM_STREAM: Stream client disconnected
```

**Error 104 = Connection Reset** - The browser connects but gets no data and times out.

## Root Cause

The camera is configured as **RGB565** for AI inference, but HTTP streaming needs **JPEG** format. The conversion is happening but it's too slow, causing the browser to disconnect.

Additionally, the AI task is using the camera for ~3 seconds per frame, so the streaming task gets starved.

---

## Quick Test: Camera-Only Mode

Let's verify the camera hardware works by temporarily disabling AI:

### Option 1: Comment Out AI Loop (Fastest Test)

Edit `main/warden_main.cpp`, find line ~1505:

```cpp
while (true) {
    /*
     * Get frame.
     */
    g_frame = esp_camera_fb_get();
    // ... rest of AI code
```

**Comment out the entire AI loop:**

```cpp
while (true) {
    // TEMPORARY: Disable AI for camera test
    vTaskDelay(pdMS_TO_TICKS(1000));
    continue;
    
    /*
    g_frame = esp_camera_fb_get();
    // ... rest of AI code
    */
}
```

Then rebuild:
```bash
idf.py build flash monitor
```

Now try `http://10.75.11.50:81/stream` - it should work!

---

## Option 2: Use JPEG Camera Mode

Better solution: Configure camera for JPEG output.

Edit `main/warden_main.cpp` camera_init() function:

**Change:**
```cpp
config.pixel_format = PIXFORMAT_RGB565;
```

**To:**
```cpp
config.pixel_format = PIXFORMAT_JPEG;  // For streaming
```

**Problem**: This breaks AI inference since Edge Impulse expects RGB565.

---

## Option 3: Dual Mode Camera (Recommended)

The **real solution** is to switch camera modes:

1. **Streaming mode**: JPEG format, fast, low CPU
2. **AI mode**: RGB565 format, only when needed

This requires more code changes to switch formats dynamically.

---

## Option 4: Lower AI Frame Rate

Keep both working but make AI use camera less:

In the main AI loop (line ~1505), add a longer delay:

```cpp
while (true) {
    g_frame = esp_camera_fb_get();
    
    if (g_frame == nullptr) {
        ESP_LOGE(TAG, "Camera capture failed");
        vTaskDelay(pdMS_TO_TICKS(100));
        continue;
    }

    // ... inference code ...
    
    esp_camera_fb_return(g_frame);
    g_frame = nullptr;
    
    // LONGER DELAY: Let streaming have more time
    vTaskDelay(pdMS_TO_TICKS(5000));  // 5 seconds instead of 20ms
}
```

This gives the streaming task more time to grab frames.

---

## Quick Verification Steps

### 1. Test with VLC (More Tolerant)

VLC handles slow streams better than browsers:

1. Open VLC
2. Media → Open Network Stream
3. `http://10.75.11.50:81/stream`
4. Set caching to 3000ms (Tools → Preferences → Show All → Input/Codecs → Network Caching)

### 2. Check Camera Status

```bash
curl http://10.75.11.50:81/status
```

Should return camera info (even if stream doesn't work).

### 3. Try Snapshot (Single Frame)

```bash
curl http://10.75.11.50:81/capture -o test.jpg
```

If this works, camera hardware is fine - it's just the streaming loop timing.

---

## Recommended Fix (Best Approach)

Since your AI takes **2937ms per frame**, the streaming will always struggle. Here's the best solution:

### Reduce AI Frame Rate Significantly

Edit the main loop to run AI only every 5 seconds:

```cpp
while (true) {
    // Only run AI every 5 seconds
    static int64_t last_inference = 0;
    int64_t now = esp_timer_get_time() / 1000;
    
    if (now - last_inference < 5000) {
        vTaskDelay(pdMS_TO_TICKS(100));
        continue;
    }
    last_inference = now;
    
    // Now do AI inference
    g_frame = esp_camera_fb_get();
    // ... rest of AI code
}
```

This way:
- AI runs every 5 seconds (plenty for fire/smoke detection)
- Camera is free for streaming the other ~4.8 seconds
- Streaming gets smooth video at 5-10 FPS

---

## Alternative: Use ESP32-CAM's Built-in Camera Server

ESP-IDF has an example camera server that's optimized. You could:

1. Use that for streaming (proven to work)
2. Keep your AI code
3. They share the camera with proper semaphores

Location: `components/esp-who/examples/camera_web_server`

---

## Summary

**Root Cause**: AI holds camera for 3 seconds, starving the streaming task.

**Quick Tests**:
1. Disable AI loop temporarily → streaming should work
2. Try VLC instead of browser → more tolerant
3. Try `/capture` endpoint → single frame test

**Real Solution**: Reduce AI frame rate from 0.3 FPS to 0.2 FPS (every 5 seconds instead of 3)

**Best Solution**: Implement proper camera sharing with semaphores or use ESP-IDF's camera server example.

---

Let me know which approach you want to try!
