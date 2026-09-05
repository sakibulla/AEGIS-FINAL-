# Warden Click-to-Start Stream Feature

## Overview
The Warden bot's video stream now requires a manual click to start. This prevents the AI fire detection from being interrupted unnecessarily.

## Behavior

### **Warden Bot (Fire Detection)**
- **Default State**: Stream is OFF, AI fire detection is ACTIVE
- **User Action Required**: Click "Start Video Stream" button to view live camera feed
- **While Stream Active**: AI fire detection is PAUSED (ESP32 gives 100% camera bandwidth to streaming)
- **Stop Stream**: Click the stop button (🛑) to resume AI fire detection

### **Guardian & Pathfinder Bots**
- **Default State**: Stream auto-plays on page load (normal behavior)
- **No Click Required**: These bots don't have the same camera/AI conflict

## Technical Implementation

### Frontend Changes

**File: `src/components/StreamViewer.js`**
- Added `requireClickToStart` prop (boolean)
- New state: `streamActive` - controls whether stream loads
- New UI: "Click to Start" overlay with explanation
- New button: Stop stream button (only shown if `requireClickToStart={true}`)

**File: `src/screens/FeedsScreen.js`**
- Passes `requireClickToStart={botId === 'warden'}` to StreamViewer
- Only Warden gets click-to-start behavior

### Backend/Firmware Integration

**Warden Firmware (ESP32-S3-EYE)**
- `warden_camera_stream.cpp` sets `stream_active` flag when client connects
- `warden_main.cpp` checks `camera_stream_is_active()` before running AI inference
- When stream is active: AI loop pauses completely
- When stream disconnects: AI inference resumes automatically

## User Experience Flow

```
1. User opens Feeds screen
   └─> Warden shows: "Click to Start Live Stream"
   └─> Message: "AI fire detection is currently running"

2. User clicks "Start Video Stream"
   └─> HTTP request sent to http://10.75.11.50:81/stream
   └─> ESP32 sets stream_active = true
   └─> AI inference pauses
   └─> Video stream loads in browser

3. User watches video feed
   └─> Stream controls available: snapshot, refresh, fullscreen, STOP
   
4. User clicks STOP button (🛑)
   └─> Stream disconnects
   └─> ESP32 sets stream_active = false
   └─> AI fire detection resumes
   └─> "Click to Start" screen returns
```

## Configuration

To enable click-to-start for other bots, modify `FeedsScreen.js`:

```javascript
<StreamViewer
  streamUrl={streamUrl}
  botId={robot.name}
  requireClickToStart={botId === 'warden' || botId === 'guardian'} // Enable for multiple bots
/>
```

## Benefits

1. **AI Always Running**: Fire detection doesn't stop unless user actively wants video
2. **Explicit User Choice**: Clear messaging about AI vs streaming tradeoff
3. **No Camera Conflicts**: ESP32 camera buffer issues eliminated
4. **Better UX**: User understands why they need to click (shown in message)
5. **Easy to Stop**: One-click stop button returns to AI mode

## Files Modified

- `src/components/StreamViewer.js` - Core streaming component
- `src/screens/FeedsScreen.js` - Feeds display screen
- This documentation file

## Testing Checklist

- [ ] Warden feed shows "Click to Start" on page load
- [ ] Clicking "Start Video Stream" loads the MJPEG stream
- [ ] AI fire detection logs stop when stream connects
- [ ] Stop button (🛑) appears in HUD controls
- [ ] Clicking stop button returns to "Click to Start" screen
- [ ] AI fire detection logs resume when stream stops
- [ ] Guardian and Pathfinder streams auto-play normally
- [ ] Fullscreen mode works with click-to-start streams
- [ ] Multiple start/stop cycles work correctly

---

**Last Updated**: 2026-08-15  
**Version**: 1.0  
**Author**: AEGIS Development Team
