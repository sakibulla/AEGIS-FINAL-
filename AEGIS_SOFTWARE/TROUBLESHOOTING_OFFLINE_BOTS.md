# Troubleshooting: All Bots Showing Offline

## Quick Diagnosis Steps

### 1. **Check Browser Console Logs** (Most Important!)

Open your browser's Developer Tools (F12) and check the Console tab for these messages:

#### ✅ **Expected Logs (When Working):**
```
[AEGIS] WebSocket connected successfully
[AEGIS] WebSocket message received: INGESTED_TELEMETRY Warden
[AEGIS] Received telemetry: Warden (telemetry) {...}
[AEGIS] Active bots: ['warden']
[AEGIS] Updated bot warden: {status: 'online', ip: '10.75.11.50', battery: 95}
```

#### ❌ **Problem Indicators:**
```
[AEGIS] WebSocket error: Failed to connect
[AEGIS] WebSocket disconnected, will retry...
```

---

### 2. **Verify Backend is Running**

Check your backend terminal for:
```
INFO:     Application startup complete.
INFO:     10.75.11.50:55364 - "POST /api/v1/telemetry/ingest HTTP/1.1" 200 OK
[AEGIS Video] Bot 'Warden' IP registered: 10.75.11.50:81
```

If you DON'T see these logs, Warden isn't sending telemetry to backend.

---

### 3. **Check Backend URL in Frontend**

In the Feeds screen, check the "Backend Connected" pill at the top:
- ✅ Should be **green** with "Backend Connected"
- ❌ If **amber** "Connecting Backend..." - backend URL is wrong

**To Fix:**
1. Click the "Backend Connected" pill
2. Enter correct backend URL: `http://10.75.11.83:8000`
3. Click "Connect"

---

### 4. **Verify Warden is Sending Telemetry**

Check Warden's serial monitor logs:
```
I (12345) WARDEN: Connected to Wi-Fi, IP: 10.75.11.50
I (15000) WARDEN: Inference | DSP=5ms NN=2937ms POST=0ms | FIRE=0.00 | SMOKE=0.00
```

If Warden isn't logging inference, the firmware isn't running.

---

### 5. **Test WebSocket Connection Manually**

Open browser console and run:
```javascript
const ws = new WebSocket('ws://10.75.11.83:8000/api/v1/ws/telemetry');
ws.onopen = () => console.log('WebSocket OPEN');
ws.onmessage = (e) => console.log('Message:', e.data);
ws.onerror = (e) => console.error('Error:', e);
```

Expected output:
```
WebSocket OPEN
Message: {"type":"INGESTED_TELEMETRY","bot_id":"Warden",...}
```

---

## Common Issues & Fixes

### Issue 1: WebSocket Not Connecting

**Symptoms:**
- Console shows: `WebSocket error: Failed to connect`
- Backend pill shows "Connecting Backend..."

**Causes:**
- Backend not running
- Wrong backend URL
- CORS issue
- Firewall blocking WebSocket

**Fix:**
```bash
# 1. Verify backend is running
cd E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Backend
uvicorn app.main:app --host 0.0.0.0 --port 8000

# 2. Test backend is accessible
curl http://10.75.11.83:8000/api/v1/bots

# 3. Check frontend backend URL
# Go to Feeds → Click "Backend Connected" pill → Verify URL
```

---

### Issue 2: Warden Not Sending Telemetry

**Symptoms:**
- Backend running, WebSocket connected
- No `INGESTED_TELEMETRY` messages in console
- Backend logs don't show Warden POST requests

**Causes:**
- Warden ESP32 not powered on
- Warden not connected to Wi-Fi
- Warden backend URL misconfigured

**Fix:**
```cpp
// Check warden_main.cpp line 43:
#define BACKEND_SERVER_URL "http://10.75.11.83:8000"

// Rebuild and flash:
idf.py build flash monitor
```

---

### Issue 3: Telemetry Received but Bot Still Offline

**Symptoms:**
- Console shows: `[AEGIS] Received telemetry: Warden (telemetry)`
- Console shows: `[AEGIS] Active bots: ['warden']`
- But bot card still shows offline

**Causes:**
- Bot ID mismatch (case sensitivity)
- Status not being updated in state
- React not re-rendering

**Fix:**

Check console for:
```
[AEGIS] Updated bot warden: {status: 'online', ...}
```

If you DON'T see this log, the state update is failing.

**Debug Code to Add:**
```javascript
// In useTelemetry.js, after updating bot state:
setBots((current) => {
  const result = current.map((bot) => {
    if (bot.id !== normId) return bot;
    const updated = { ...bot, status: 'online', ... };
    console.log('Bot after update:', updated);
    return updated;
  });
  console.log('All bots after update:', result);
  return result;
});
```

---

### Issue 4: Bot Shows Online Then Goes Offline

**Symptoms:**
- Bot briefly shows online
- After 10-15 seconds, goes back to offline
- Console shows: `Bot warden timed out, marking offline`

**Causes:**
- Timeout too aggressive (10 seconds)
- Warden sending telemetry slower than 10 seconds
- Network latency

**Fix:**

Increase timeout in `useTelemetry.js`:
```javascript
const TIMEOUT_MS = 20000; // Changed from 10000 to 20000 (20 seconds)
```

Or check if Warden is actually sending telemetry every 3 seconds:
```cpp
// In warden_main.cpp, verify:
#define TELEMETRY_INTERVAL_MS 3000  // Should be 3 seconds
```

---

## Debug Checklist

Run through this checklist in order:

- [ ] **Backend Running**: Terminal shows "Application startup complete"
- [ ] **Warden Connected to Wi-Fi**: Serial monitor shows IP address
- [ ] **Warden Sending POST Requests**: Backend logs show POST /api/v1/telemetry/ingest
- [ ] **Frontend Backend URL Correct**: http://10.75.11.83:8000
- [ ] **WebSocket Connected**: Browser console shows "WebSocket connected successfully"
- [ ] **Telemetry Messages Received**: Console shows "Received telemetry: Warden"
- [ ] **Active Bots Updated**: Console shows "Active bots: ['warden']"
- [ ] **Bot State Updated**: Console shows "Updated bot warden: {status: 'online'}"
- [ ] **Bot Card Shows Online**: Dashboard shows "1/3 bots active"

---

## Manual Testing Commands

### Test Backend REST API:
```bash
# Get bots status
curl http://10.75.11.83:8000/api/v1/bots

# Expected response:
[{"bot_id":"Warden","status":"PATROL","ip_address":"10.75.11.50",...}]
```

### Test WebSocket in Browser Console:
```javascript
const testWS = () => {
  const ws = new WebSocket('ws://10.75.11.83:8000/api/v1/ws/telemetry');
  ws.onopen = () => console.log('✅ WebSocket OPEN');
  ws.onmessage = (e) => {
    const data = JSON.parse(e.data);
    console.log('📨 Message:', data.type, data.bot_id);
  };
  ws.onerror = (e) => console.error('❌ Error:', e);
  ws.onclose = () => console.log('🔌 Closed');
};
testWS();
```

### Test Warden Direct:
```bash
# Check Warden is accessible
ping 10.75.11.50

# Check camera stream endpoint
curl http://10.75.11.50:81/status
```

---

## Still Not Working?

### Collect Debug Info:

1. **Browser Console Logs** (full output)
2. **Backend Terminal Output** (last 50 lines)
3. **Warden Serial Monitor** (last 50 lines)
4. **Network Config**:
   - Frontend running on: `http://localhost:3000` ?
   - Backend running on: `http://10.75.11.83:8000` ?
   - Warden IP: `10.75.11.50` ?

### Quick Reset:

```bash
# 1. Stop everything
# Kill backend, frontend, and Warden

# 2. Restart backend
cd E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Backend
uvicorn app.main:app --host 0.0.0.0 --port 8000

# 3. Restart frontend
cd E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Frontend
npm start

# 4. Restart Warden (press reset button or reflash)
cd E:\AEGIS_GUARDIAN_FOMO\AEGIS_WARDEN\AEGIS_WARDEN_MASTER
idf.py flash monitor

# 5. Refresh browser (Ctrl+Shift+R to clear cache)
```

---

## Log Examples

### ✅ **Working System Logs:**

**Backend:**
```
INFO:     Application startup complete.
[AEGIS Video] Bot 'Warden' IP registered: 10.75.11.50:81
INFO:     10.75.11.50:55364 - "POST /api/v1/telemetry/ingest HTTP/1.1" 200 OK
```

**Warden Serial:**
```
I (2565) WARDEN: Connected to Wi-Fi, IP: 10.75.11.50
I (6602) WARDEN: Inference | DSP=5ms NN=2938ms POST=0ms | FIRE=0.00 | SMOKE=0.00
```

**Browser Console:**
```
[AEGIS] WebSocket connected successfully
[AEGIS] WebSocket message received: INGESTED_TELEMETRY Warden
[AEGIS] Received telemetry: Warden (telemetry) {status: 'PATROL', ip_address: '10.75.11.50', ...}
[AEGIS] Active bots: ['warden']
[AEGIS] Updated bot warden: {status: 'online', ip: '10.75.11.50', battery: 95}
```

### ❌ **Broken System Logs:**

**Browser Console (WebSocket Failed):**
```
[AEGIS] WebSocket error: Failed to connect to ws://10.75.11.83:8000/api/v1/ws/telemetry
[AEGIS] WebSocket disconnected, will retry...
```

**Browser Console (No Telemetry):**
```
[AEGIS] WebSocket connected successfully
(No further messages - Warden not sending data)
```

---

**Last Updated**: 2026-08-15  
**For Support**: Check console logs first, then backend/Warden serial output
