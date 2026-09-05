# Warden Connection Diagnostic Guide

## Problem
Frontend shows all bots as offline despite backend running. Backend logs show only `127.0.0.1` connections, no POST requests from Warden at `10.75.11.50`.

## Root Cause Analysis

### ✅ Fixed Issues:
1. **Frontend Backend URL**: Changed from dynamic `localhost` to hardcoded `http://10.75.11.83:8000` in `AegisService.js`
2. **Warden Backend URL**: Correctly set to `http://10.75.11.83:8000` in `warden_main.cpp` line 43
3. **Camera Port Registration**: Backend correctly extracts `camera_port: 81` from telemetry and registers Warden's video feed

### 🔍 Remaining Issues to Verify:

1. **Backend Not Listening on 0.0.0.0**
   - Backend MUST be started with: `uvicorn app.main:app --host 0.0.0.0 --port 8000`
   - NOT: `uvicorn app.main:app --host 127.0.0.1 --port 8000` (only accepts localhost)
   - NOT: `uvicorn app.main:app` (defaults to 127.0.0.1)

2. **Warden Not Running or Not Connected to Wi-Fi**
   - Check if Warden device is powered on
   - Check if Warden is connected to "A34" Wi-Fi network
   - Verify Warden got IP address `10.75.11.50` (or different DHCP assignment)

3. **Network Routing Issues**
   - Backend machine at `10.75.11.83` must be on same network as Warden
   - Check firewall rules on backend machine (Windows Firewall may block port 8000)

## Step-by-Step Verification

### Step 1: Verify Backend is Accessible from Network

On backend machine (10.75.11.83):

```bash
# Check if port 8000 is listening on all interfaces
netstat -an | findstr :8000

# Should show:
# TCP    0.0.0.0:8000    0.0.0.0:0    LISTENING
# 
# If it shows 127.0.0.1:8000, backend is only accepting localhost connections!
```

**Fix if needed**: Restart backend with correct command:
```bash
cd E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Backend
python -m uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload
```

### Step 2: Test Backend from Another Machine

From any device on the network (NOT localhost):

```bash
# Test backend health endpoint
curl http://10.75.11.83:8000/

# Should return: {"message":"AEGIS Backend is operational."}
```

If this fails, check:
- Windows Firewall settings (allow port 8000)
- Network adapter settings
- IP address is correct (`ipconfig` to verify)

### Step 3: Verify Warden Wi-Fi Connection

Connect to Warden's serial monitor:

```bash
cd E:\AEGIS_GUARDIAN_FOMO\AEGIS_WARDEN\AEGIS_WARDEN_MASTER
idf.py monitor
```

Look for these log messages:
```
I (xxxx) WARDEN: Wi-Fi station started, connecting...
I (xxxx) WARDEN: Connected to Wi-Fi, IP: 10.75.11.50
I (xxxx) WARDEN: Backend health check: ONLINE
```

If you see:
- `Wi-Fi connection failed` → Check SSID/password in `warden_main.cpp` (lines 55-56)
- `Backend health check: OFFLINE` → Backend not reachable from Warden's network

### Step 4: Verify Telemetry Sending

In Warden serial monitor, look for:
```
I (xxxx) WARDEN: Sending status telemetry...
I (xxxx) WARDEN: POST http://10.75.11.83:8000/api/v1/telemetry/ingest → 200 OK
```

If you see errors:
- `HTTP 503` → Backend not ready
- `Connection timeout` → Network routing issue
- `DNS resolution failed` → Use IP address instead of hostname

### Step 5: Verify Backend Receives Telemetry

In backend console, you should see:
```
INFO:     10.75.11.50:xxxxx - "POST /api/v1/telemetry/ingest HTTP/1.1" 200 OK
[AEGIS] Registered Warden video feed at 10.75.11.50:81
```

**If you only see `127.0.0.1` connections**, backend is not accessible from network!

### Step 6: Verify Frontend Connection

1. **Hard refresh frontend**: Press `Ctrl + Shift + R` to clear cache
2. **Check browser console** (F12 → Console tab):

Look for:
```
[AEGIS] Swarm telemetry socket connected to ws://10.75.11.83:8000/api/v1/ws/telemetry
[AEGIS] WebSocket message received: INGESTED_TELEMETRY Warden
[AEGIS] Received telemetry: Warden (telemetry) {...}
[AEGIS] Active bots: ["warden"]
```

If you see:
- `WebSocket connection failed` → Backend WebSocket not working
- Only `TELEMETRY_FRAME` messages → Backend serving mock data, not real hardware
- No `INGESTED_TELEMETRY` messages → Warden not sending to backend

## Quick Diagnostic Commands

### On Backend Machine (10.75.11.83):

```bash
# 1. Check backend is listening on all interfaces
netstat -an | findstr :8000

# 2. Check firewall status
netsh advfirewall show allprofiles state

# 3. Add firewall rule if needed
netsh advfirewall firewall add rule name="AEGIS Backend" dir=in action=allow protocol=TCP localport=8000

# 4. Test backend locally
curl http://localhost:8000/
curl http://10.75.11.83:8000/

# 5. Check backend logs for connections
# Should see connections from 10.75.11.50, not just 127.0.0.1
```

### From Another Network Device:

```bash
# Test backend is reachable
ping 10.75.11.83
curl http://10.75.11.83:8000/
```

### Warden Serial Monitor Commands:

```bash
# Connect to Warden
cd E:\AEGIS_GUARDIAN_FOMO\AEGIS_WARDEN\AEGIS_WARDEN_MASTER
idf.py monitor

# Watch for:
# - Wi-Fi connection success
# - IP address assignment
# - Backend health check
# - Telemetry POST requests
```

## Expected Behavior When Working

1. **Backend logs**:
   ```
   INFO:     10.75.11.50:xxxxx - "POST /api/v1/telemetry/ingest HTTP/1.1" 200 OK
   [AEGIS] Registered Warden video feed at 10.75.11.50:81
   INFO:     127.0.0.1:xxxxx - "WebSocket /api/v1/ws/telemetry" [accepted]
   ```

2. **Warden logs**:
   ```
   I (xxxx) WARDEN: Connected to Wi-Fi, IP: 10.75.11.50
   I (xxxx) WARDEN: Backend health check: ONLINE
   I (xxxx) WARDEN: Inference: fire 5%, smoke 3%, clear 92%
   I (xxxx) WARDEN: Status telemetry queued
   ```

3. **Frontend console**:
   ```
   [AEGIS] WebSocket connected successfully
   [AEGIS] WebSocket message received: INGESTED_TELEMETRY Warden
   [AEGIS] Received telemetry: Warden (telemetry) {status: "online", battery: 95, ...}
   [AEGIS] Active bots: ["warden"]
   ```

4. **Frontend UI**:
   - Warden card shows "Online" badge (green)
   - IP address: `10.75.11.50`
   - Battery: 95%
   - WiFi RSSI: -64 dBm
   - Detection state chips visible (🔥 Fire, 💨 Smoke, ✅ Clear)

## Common Issues and Solutions

### Issue: All bots show offline
**Cause**: Frontend connecting to wrong backend URL
**Solution**: Verify `AegisService.js` has `return "http://10.75.11.83:8000";` (already fixed)

### Issue: Backend only shows 127.0.0.1 connections
**Cause**: Backend started with `--host 127.0.0.1` or default
**Solution**: Restart backend with `--host 0.0.0.0`

### Issue: Warden can't connect to backend
**Cause**: Firewall blocking port 8000 or network routing
**Solution**: Add firewall rule, verify network connectivity

### Issue: Frontend receives only mock data (TELEMETRY_FRAME)
**Cause**: No hardware sending telemetry, backend falls back to test scenarios
**Solution**: Ensure Warden is running and sending POST requests to backend

### Issue: Warden video stream doesn't work
**Cause**: Camera port not registered or incorrect IP
**Solution**: Verify telemetry includes `camera_port: 81` field (already implemented)

## Files Modified

1. **Frontend**: `E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Frontend\src\services\AegisService.js`
   - Changed backend URL to `http://10.75.11.83:8000`

2. **Warden**: `E:\AEGIS_GUARDIAN_FOMO\AEGIS_WARDEN\AEGIS_WARDEN_MASTER\main\warden_main.cpp`
   - Backend URL already correct: `http://10.75.11.83:8000`
   - Camera port field added to telemetry: `"camera_port": 81`

3. **Backend**: `E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\AEGIS-Backend\app\api\routes.py`
   - Telemetry ingestion correctly extracts `camera_port` and registers bot IP

## Next Steps

**You should now**:
1. Stop the backend (Ctrl+C)
2. Restart backend with: `python -m uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload`
3. Verify Warden is powered on and running
4. Check Warden serial monitor for Wi-Fi connection and backend POST requests
5. Hard refresh frontend (Ctrl+Shift+R)
6. Check frontend console for `INGESTED_TELEMETRY` messages
7. Verify Warden shows as "Online" in frontend

If still not working, share:
- Backend startup command you used
- Warden serial monitor output (Wi-Fi connection section)
- Frontend browser console logs
- Backend console logs (look for POST requests from 10.75.11.50)
