@echo off
title AEGIS System Health Check
color 0B
echo ===============================================
echo   AEGIS GUARDIAN SYSTEM - Health Check
echo ===============================================
echo.

echo [1/5] Checking Backend Server...
curl -s -o NUL -w "Backend HTTP Status: %%{http_code}\n" http://localhost:8000 2>NUL
if errorlevel 1 (
    echo   [X] Backend NOT RUNNING
    echo   Fix: Run AEGIS_SOFTWARE\AEGIS-Backend\START_BACKEND.bat
) else (
    echo   [OK] Backend is running
)
echo.

echo [2/5] Checking Your Network IP...
for /f "tokens=2 delims=:" %%a in ('ipconfig ^| findstr /C:"IPv4"') do (
    echo   Your IP: %%a
)
echo   Backend configured for: 10.75.11.83:8000
echo.

echo [3/5] Checking Bot Status API...
curl -s http://localhost:8000/api/v1/bots 2>NUL | findstr /C:"bot_id" >NUL
if errorlevel 1 (
    echo   [X] Bot API not responding or empty
) else (
    echo   [OK] Bot API returning data
)
echo.

echo [4/5] Checking Video Service...
curl -s http://localhost:8000/api/v1/video/stream/Warden --max-time 2 >NUL 2>&1
if errorlevel 1 (
    echo   [!] Video stream may need ESP32 hardware
) else (
    echo   [OK] Video service responding
)
echo.

echo [5/5] Expected ESP32 Bot IPs:
echo   - Guardian:   192.168.0.102 or 10.75.11.x
echo   - Pathfinder: 192.168.0.101 or 10.75.11.x  
echo   - Warden:     192.168.0.103 or 10.75.11.x
echo.

echo ===============================================
echo   Quick Fix Guide
echo ===============================================
echo.
echo   If Backend is DOWN:
echo   1. cd AEGIS_SOFTWARE\AEGIS-Backend
echo   2. Run START_BACKEND.bat
echo.
echo   If Warden not sending data:
echo   1. Check ESP32 serial monitor for WiFi connection
echo   2. Verify WiFi SSID/Password in warden_main.cpp
echo   3. Ensure BACKEND_SERVER_URL matches: http://10.75.11.83:8000
echo   4. Reflash: idf.py build flash monitor
echo.
echo   If Frontend crashing:
echo   1. Make sure Backend is running first
echo   2. Refresh the page or restart expo
echo.
pause
