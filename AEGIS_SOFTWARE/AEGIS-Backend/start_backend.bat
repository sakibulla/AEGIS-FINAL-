@echo off
title AEGIS Backend Server (FastAPI + Uvicorn)
echo ============================================
echo   AEGIS CENTRAL COMMAND - Backend Server
echo ============================================
echo.
echo Starting FastAPI server on http://0.0.0.0:8000
echo WebSocket endpoint: ws://localhost:8000/api/v1/ws/telemetry
echo Video streams: http://localhost:8000/api/v1/video/stream/[Bot]
echo.
cd /d "%~dp0"
call venv\Scripts\activate.bat
if errorlevel 1 (
    echo ERROR: Virtual environment not found!
    echo Please run: python -m venv venv
    echo Then: .\venv\Scripts\activate
    echo Then: pip install -r requirements.txt
    pause
    exit /b 1
)
echo Virtual environment activated
python -m uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload
pause
