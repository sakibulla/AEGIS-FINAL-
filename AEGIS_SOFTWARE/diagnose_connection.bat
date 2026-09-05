@echo off
echo.
echo === AEGIS WARDEN CONNECTION DIAGNOSTICS ===
echo.

echo [1/6] Checking if backend is listening on port 8000...
netstat -an | findstr ":8000" | findstr "LISTENING"
if errorlevel 1 (
    echo    Backend is NOT listening on port 8000
    echo    Fix: Start backend with: python -m uvicorn app.main:app --host 0.0.0.0 --port 8000
) else (
    echo    Backend is listening on port 8000
    echo    Check if it shows 0.0.0.0:8000 or 127.0.0.1:8000 above
)
echo.

echo [2/6] Checking network configuration...
ipconfig | findstr /i "IPv4"
echo.

echo [3/6] Testing backend on localhost...
curl -s http://localhost:8000/ 2>nul
if errorlevel 1 (
    echo    Backend NOT responding on localhost
) else (
    echo    Backend responds on localhost
)
echo.

echo [4/6] Testing backend on network IP...
curl -s http://10.75.11.83:8000/ 2>nul
if errorlevel 1 (
    echo    Backend NOT accessible on network IP
    echo    Backend may be listening on 127.0.0.1 only
    echo    Or firewall is blocking port 8000
) else (
    echo    Backend is accessible from network IP
)
echo.

echo [5/6] Testing connectivity to Warden...
ping -n 2 10.75.11.50 | findstr /i "reply"
if errorlevel 1 (
    echo    Warden device is NOT reachable
    echo    Is Warden powered on and connected to Wi-Fi?
) else (
    echo    Warden device is reachable
)
echo.

echo === SUMMARY ===
echo.
echo Expected backend startup command:
echo   python -m uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload
echo.
echo Expected in backend logs:
echo   10.75.11.50:xxxxx - "POST /api/v1/telemetry/ingest HTTP/1.1" 200 OK
echo.
echo If you only see 127.0.0.1 connections:
echo   1. Stop backend (Ctrl+C)
echo   2. Run: python -m uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload
echo   3. Check Warden serial monitor for Wi-Fi connection
echo   4. Hard refresh frontend (Ctrl+Shift+R)
echo.
echo For detailed troubleshooting, see:
echo   E:\AEGIS_GUARDIAN_FOMO\AEGIS_SOFTWARE\VERIFY_WARDEN_CONNECTION.md
echo.
pause
