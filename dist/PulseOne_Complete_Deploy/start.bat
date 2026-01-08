@echo off
setlocal enabledelayedexpansion

title PulseOne Industrial IoT Platform

echo ================================================================
echo PulseOne Industrial IoT Platform v6.0
echo ================================================================

pushd "%~dp0"

:: Check Node.js
where node >nul 2>&1
if errorlevel 1 (
    echo ERROR: Node.js is not installed or not in PATH!
    echo Please run install.bat first
    echo Or restart this command prompt if you just installed Node.js
    pause
    exit /b 1
)

:: Environment setup
echo [1/5] Environment setup...

:: Create directories if missing
if not exist "data" mkdir data >nul 2>&1
if not exist "data\db" mkdir data\db >nul 2>&1
if not exist "data\logs" mkdir data\logs >nul 2>&1
if not exist "config" mkdir config >nul 2>&1

echo OK: Environment ready

:: Process cleanup
echo [2/5] Process cleanup...
taskkill /F /IM node.exe >nul 2>&1
taskkill /F /IM redis-server.exe >nul 2>&1
taskkill /F /IM collector.exe >nul 2>&1
timeout /t 2 /nobreak >nul
echo OK: Processes cleaned

:: Start Redis
echo [3/5] Starting Redis server...
if exist "redis-server.exe" (
    if exist "redis.windows.conf" (
        start /B redis-server.exe redis.windows.conf >nul 2>&1
    ) else (
        start /B redis-server.exe --port 6379 --maxmemory 512mb >nul 2>&1
    )
    timeout /t 2 /nobreak >nul
    echo OK: Redis started on port 6379
) else (
    echo INFO: Redis not found, continuing without cache
)

:: Start Backend
echo [4/5] Starting backend server...
cd backend
start /B node app.js >nul 2>&1
cd ..

:: Wait for startup
echo [5/5] Waiting for server startup...
timeout /t 5 /nobreak >nul

:: Start Collector if available
if exist "collector.exe" (
    echo Starting data collector...
    start /B collector.exe >nul 2>&1
    echo OK: Collector started
)

:: Open browser
echo.
echo Opening web browser...
start http://localhost:3000

:: Display status
echo.
echo ================================================================
echo ✅ PulseOne is running!
echo ================================================================
echo.
echo Web Interface: http://localhost:3000
echo Default Login: admin / admin
echo.
echo Active Services:
echo   • Backend API: Port 3000
if exist "redis-server.exe" echo   • Redis Cache: Port 6379
if exist "collector.exe" echo   • Data Collector: Running
echo.
echo Press any key to stop all services...
echo ================================================================
pause >nul

:: Stop all services
echo.
echo Stopping all services...
taskkill /F /IM node.exe >nul 2>&1
taskkill /F /IM redis-server.exe >nul 2>&1
taskkill /F /IM collector.exe >nul 2>&1

echo All services stopped.
timeout /t 2

popd
endlocal
