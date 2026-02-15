@echo off
setlocal enabledelayedexpansion

echo ================================================================
echo PulseOne Containerized Setup v1.0 (Windows)
echo ================================================================

:: Check Docker
where docker >nul 2>&1
if errorlevel 1 (
    echo [!] ERROR: Docker is not installed or not in PATH.
    echo Please install Docker Desktop for Windows first.
    pause & exit /b 1
)

:: Load Images
echo [1/2] Loading Docker images from local assets...
for %%f in (images\*.tar) do (
    echo     Loading %%f...
    docker load < "%%f"
)

:: Start Services
echo [2/2] Starting PulseOne services...
docker-compose up -d

echo.
echo ================================================================
echo ✅ PulseOne is running!
echo ➡️  Web UI: http://localhost:3000
echo ================================================================
pause
