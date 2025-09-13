#!/bin/bash

# =============================================================================
# PulseOne Complete Deployment Script v4.0 FINAL
# Collector 빌드 + 자동화된 설치 포함 완전판
# =============================================================================

PROJECT_ROOT=$(pwd)
PACKAGE_NAME="PulseOne_Complete_Deploy"
VERSION="4.0.0"
TIMESTAMP=$(date '+%Y%m%d_%H%M%S')
DIST_DIR="$PROJECT_ROOT/dist"
PACKAGE_DIR="$DIST_DIR/$PACKAGE_NAME"

echo "================================================================="
echo "🚀 PulseOne Complete Deployment Script v4.0 FINAL"
echo "Collector build + Automated installation included"
echo "================================================================="

# =============================================================================
# 1. Project Structure Check
# =============================================================================

echo "1. 🔍 Checking project structure..."

REQUIRED_DIRS=("backend" "frontend" "collector")
MISSING_DIRS=()

for dir in "${REQUIRED_DIRS[@]}"; do
    if [ ! -d "$PROJECT_ROOT/$dir" ]; then
        MISSING_DIRS+=("$dir")
    fi
done

if [ ${#MISSING_DIRS[@]} -gt 0 ]; then
    echo "❌ Missing directories: ${MISSING_DIRS[*]}"
    exit 1
fi

# Docker check for Collector
if ! command -v docker &> /dev/null; then
    echo "⚠️ Docker is not installed"
    if [ "$SKIP_COLLECTOR" != "true" ]; then
        echo "Set SKIP_COLLECTOR=true to skip collector build"
        echo "Or install Docker to build collector"
        exit 1
    fi
fi

echo "✅ Project structure check completed"

# =============================================================================
# 2. Build Environment Setup
# =============================================================================

echo "2. 📦 Setting up build environment..."

rm -rf "$DIST_DIR"
mkdir -p "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR/collector"

if ! command -v node &> /dev/null; then
    echo "⚠️ Node.js not found on build system"
    echo "Frontend build may fail"
fi

echo "✅ Build environment setup completed"

# =============================================================================
# 3. Frontend Build
# =============================================================================

echo "3. 🎨 Building frontend..."

cd "$PROJECT_ROOT/frontend"

if [ ! -f "package.json" ]; then
    echo "❌ frontend/package.json not found"
    exit 1
fi

echo "Installing frontend dependencies..."
npm install --silent

echo "Building frontend..."
if npm run build; then
    if [ -d "dist" ]; then
        FRONTEND_SIZE=$(du -sh dist | cut -f1)
        echo "✅ Frontend build completed: $FRONTEND_SIZE"
    else
        echo "❌ Frontend dist folder not created"
        exit 1
    fi
else
    echo "❌ Frontend build failed"
    exit 1
fi

# =============================================================================
# 4. Collector Build (Docker Cross-compile for Windows)
# =============================================================================

if [ "$SKIP_COLLECTOR" = "true" ]; then
    echo "4. ⚙️ Collector build skipped (SKIP_COLLECTOR=true)"
else
    echo "4. ⚙️ Building Collector for Windows..."
    
    cd "$PROJECT_ROOT"
    
    # Check for existing Windows build container
    echo "🔍 Checking for Windows build container..."
    if docker image ls | grep -q "pulseone-windows-builder"; then
        echo "✅ Found pulseone-windows-builder container"
        DOCKER_IMAGE="pulseone-windows-builder"
    else
        echo "📦 Creating Windows build container..."
        
        # Create Dockerfile for MinGW cross-compilation
        cat > "$PROJECT_ROOT/collector/Dockerfile.mingw" << 'DOCKERFILE_EOF'
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    gcc-mingw-w64-x86-64 \
    g++-mingw-w64-x86-64 \
    make cmake git \
    pkg-config \
    wget unzip

# Install Windows libraries
RUN mkdir -p /usr/x86_64-w64-mingw32/include && \
    mkdir -p /usr/x86_64-w64-mingw32/lib

WORKDIR /src
DOCKERFILE_EOF
        
        docker build -f collector/Dockerfile.mingw -t pulseone-windows-builder .
        DOCKER_IMAGE="pulseone-windows-builder"
    fi
    
    echo "📦 Building Collector with $DOCKER_IMAGE..."
    
    # Run Windows cross-compilation
    docker run --rm \
        -v "$(pwd)/collector:/src" \
        -v "$PACKAGE_DIR:/output" \
        "$DOCKER_IMAGE" bash -c "
            cd /src
            echo 'Cleaning previous builds...'
            make -f Makefile.windows clean 2>/dev/null || true
            
            echo 'Building for Windows...'
            if make -f Makefile.windows all; then
                echo 'Build successful'
                if [ -f 'bin-windows/collector.exe' ]; then
                    cp bin-windows/collector.exe /output/collector.exe
                    echo 'Collector.exe copied to output'
                elif [ -f 'collector.exe' ]; then
                    cp collector.exe /output/collector.exe
                    echo 'Collector.exe copied to output'
                else
                    echo 'WARNING: collector.exe not found after build'
                fi
            else
                echo 'WARNING: Collector build failed'
            fi
        "
    
    if [ -f "$PACKAGE_DIR/collector.exe" ]; then
        COLLECTOR_SIZE=$(du -sh "$PACKAGE_DIR/collector.exe" | cut -f1)
        echo "✅ Collector build successful: $COLLECTOR_SIZE"
    else
        echo "⚠️ Collector build failed or not found"
    fi
fi

# =============================================================================
# 5. Backend Source Code Copy
# =============================================================================

echo "5. 🔧 Copying backend source code..."

cd "$PACKAGE_DIR"

# Copy backend source (exclude node_modules for Windows installation)
cp -r "$PROJECT_ROOT/backend" ./
rm -rf backend/node_modules 2>/dev/null || true
rm -rf backend/.git 2>/dev/null || true

echo "✅ Backend source code copied"

# =============================================================================
# 6. Copy Frontend Build Results
# =============================================================================

echo "6. 🎨 Copying frontend build results..."

mkdir -p frontend
cp -r "$PROJECT_ROOT/frontend/dist"/* ./frontend/

echo "✅ Frontend copy completed"

# =============================================================================
# 7. Copy Config and Data Structure
# =============================================================================

echo "7. 📝 Copying configuration and creating structure..."

# Copy existing config if available
if [ -d "$PROJECT_ROOT/config" ]; then
    cp -r "$PROJECT_ROOT/config" ./
fi

# Create directory structure
mkdir -p data/db data/backup data/logs data/temp config

echo "✅ Configuration and structure created"

# =============================================================================
# 8. Windows Installation Script - FULLY AUTOMATED VERSION
# =============================================================================

echo "8. 🛠️ Creating fully automated Windows installation script..."

cd "$PACKAGE_DIR"

cat > install.bat << 'INSTALL_EOF'
@echo off
setlocal enabledelayedexpansion

title PulseOne Installation v4.0 - Fully Automated

echo ================================================================
echo PulseOne Industrial IoT Platform v4.0
echo Fully Automated Installation
echo ================================================================

pushd "%~dp0"
set "INSTALL_DIR=%CD%"

:: Check administrator privileges
net session >nul 2>&1
if errorlevel 1 (
    echo INFO: Running without administrator privileges
    echo Some features may be limited
    timeout /t 3
)

echo.
echo [1/6] Checking Node.js...

:: Check if Node.js is installed
where node >nul 2>&1
if not errorlevel 1 (
    for /f "tokens=*" %%i in ('node --version') do set "NODE_VERSION=%%i"
    echo Found Node.js !NODE_VERSION!
    goto install_backend
)

echo Node.js not found. Installing automatically...

:: Download Node.js MSI
echo.
echo [2/6] Downloading Node.js installer...
set "NODE_VERSION=v22.19.0"
set "NODE_MSI=node-!NODE_VERSION!-x64.msi"
set "NODE_URL=https://nodejs.org/dist/!NODE_VERSION!/!NODE_MSI!"

if not exist "!NODE_MSI!" (
    echo Downloading from: !NODE_URL!
    powershell -Command "& {[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; try { $ProgressPreference='SilentlyContinue'; Invoke-WebRequest -Uri '!NODE_URL!' -OutFile '!NODE_MSI!' -UseBasicParsing; Write-Host 'Download completed' } catch { Write-Host 'Download failed'; exit 1 }}"
    
    if not exist "!NODE_MSI!" (
        echo Trying alternative download with curl...
        curl -L -o "!NODE_MSI!" "!NODE_URL!"
    )
)

if exist "!NODE_MSI!" (
    echo Installing Node.js silently...
    echo This may take a few minutes...
    
    :: Silent install with all features
    msiexec /i "!NODE_MSI!" /quiet /qn /norestart ADDLOCAL=ALL
    
    :: Wait for installation to complete
    timeout /t 20 /nobreak >nul
    
    :: Refresh environment variables
    echo Refreshing environment variables...
    call refreshenv 2>nul || (
        :: Manual PATH refresh for current session
        for /f "tokens=2*" %%a in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v Path 2^>nul') do set "SystemPath=%%b"
        for /f "tokens=2*" %%a in ('reg query "HKCU\Environment" /v Path 2^>nul') do set "UserPath=%%b"
        set "PATH=!SystemPath!;!UserPath!;C:\Program Files\nodejs"
    )
    
    :: Verify installation
    where node >nul 2>&1
    if errorlevel 1 (
        echo.
        echo ================================================================
        echo Node.js installed but PATH not updated
        echo Please close this window and run install.bat again
        echo ================================================================
        pause
        exit /b 0
    )
    
    echo Node.js installed successfully!
) else (
    echo ERROR: Could not download Node.js
    echo Please download manually from: https://nodejs.org
    pause
    exit /b 1
)

:install_backend
:: Install backend dependencies
echo.
echo [3/6] Installing backend packages...

cd backend

:: Clean install
if exist "node_modules" (
    echo Cleaning previous installation...
    rd /s /q node_modules 2>nul
    timeout /t 2 /nobreak >nul
)

echo Installing packages (this may take 2-3 minutes)...
call npm install --no-audit --no-fund --loglevel=error

:: Check if SQLite3 is installed
if not exist "node_modules\sqlite3" (
    echo.
    echo Installing SQLite3 separately...
    call npm install sqlite3 --no-audit --no-fund
)

:: Verify critical packages
if not exist "node_modules\express" (
    echo WARNING: Some packages may have failed to install
    echo Trying alternative installation...
    call npm install --force
)

cd ..

:: Download and setup Redis
echo.
echo [4/6] Setting up Redis...

if not exist "redis-server.exe" (
    echo Downloading Redis for Windows...
    set "REDIS_URL=https://github.com/tporadowski/redis/releases/download/v5.0.14.1/Redis-x64-5.0.14.1.zip"
    
    powershell -Command "& {[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; $ProgressPreference='SilentlyContinue'; try { Invoke-WebRequest -Uri '!REDIS_URL!' -OutFile 'redis.zip' -UseBasicParsing; Write-Host 'Download completed' } catch { Write-Host 'Download failed'; exit 1 }}"
    
    if exist "redis.zip" (
        echo Extracting Redis files...
        powershell -Command "try { Expand-Archive -Path 'redis.zip' -DestinationPath '.' -Force; exit 0 } catch { exit 1 }"
        
        echo Cleaning up unnecessary files...
        del /f /q redis.zip 2>nul
        del /f /q *.pdb 2>nul
        del /f /q redis-benchmark.exe 2>nul
        del /f /q redis-check-aof.exe 2>nul
        del /f /q redis-check-rdb.exe 2>nul
        del /f /q 00-RELEASENOTES 2>nul
        del /f /q RELEASENOTES.txt 2>nul
        
        if exist "redis-server.exe" (
            echo Redis setup completed successfully
        ) else (
            echo WARNING: Redis extraction may have failed
        )
    ) else (
        echo WARNING: Redis download failed, continuing without cache
    )
) else (
    echo Redis already installed
)

:: Download SQLite DLL if needed
echo.
echo [5/6] Checking SQLite DLL...

if not exist "sqlite3.dll" (
    echo Downloading SQLite DLL...
    set "SQLITE_URL=https://www.sqlite.org/2024/sqlite-dll-win-x64-3460100.zip"
    
    powershell -Command "& {[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; try { Invoke-WebRequest -Uri '!SQLITE_URL!' -OutFile 'sqlite.zip' -TimeoutSec 60 -UseBasicParsing; exit 0 } catch { exit 1 }}"
    
    if exist "sqlite.zip" (
        powershell -Command "Expand-Archive -Path 'sqlite.zip' -DestinationPath '.' -Force" 2>nul
        del /f sqlite.zip 2>nul
        
        if exist "sqlite3.dll" (
            echo SQLite DLL downloaded
        )
    )
)

:: Create configuration
echo.
echo [6/6] Creating configuration files...

:: Create directories
if not exist "data" mkdir data
if not exist "data\db" mkdir data\db
if not exist "data\logs" mkdir data\logs
if not exist "data\backup" mkdir data\backup
if not exist "data\temp" mkdir data\temp
if not exist "config" mkdir config

:: Create main configuration
if not exist "config\.env" (
    (
        echo # PulseOne Configuration
        echo NODE_ENV=production
        echo PORT=3000
        echo DATABASE_TYPE=SQLITE
        echo SQLITE_PATH=./data/db/pulseone.db
        echo AUTO_INITIALIZE_ON_START=true
        echo SKIP_IF_INITIALIZED=true
        echo SERVE_FRONTEND=true
        echo FRONTEND_PATH=./frontend
        echo LOG_LEVEL=info
        echo LOG_TO_FILE=true
        echo LOG_FILE_PATH=./data/logs/pulseone.log
        echo REDIS_ENABLED=true
        echo REDIS_HOST=127.0.0.1
        echo REDIS_PORT=6379
    ) > config\.env
    echo Configuration file created
)

:: Create Redis configuration
if not exist "redis.windows.conf" (
    if exist "redis.windows.conf" (
        echo Redis configuration exists
    ) else if exist "redis.windows-service.conf" (
        copy redis.windows-service.conf redis.windows.conf >nul
    )
)

:: Installation summary
echo.
echo ================================================================
echo Installation Summary
echo ================================================================

:: Check components
set "INSTALL_SUCCESS=1"

where node >nul 2>&1
if not errorlevel 1 (
    for /f "tokens=*" %%i in ('node --version') do echo [✓] Node.js: %%i
) else (
    echo [✗] Node.js: Not found
    set "INSTALL_SUCCESS=0"
)

if exist "redis-server.exe" (
    echo [✓] Redis: Server installed
) else (
    echo [⚠] Redis: Not installed (optional)
)

if exist "backend\node_modules\express" (
    echo [✓] Backend: Packages installed
) else (
    echo [✗] Backend: Packages missing
    set "INSTALL_SUCCESS=0"
)

if exist "backend\node_modules\sqlite3" (
    echo [✓] Database: SQLite3 module installed
) else (
    echo [⚠] Database: SQLite3 module missing
)

if exist "frontend\index.html" (
    echo [✓] Frontend: Files ready
) else (
    echo [⚠] Frontend: Files missing
)

if exist "collector.exe" (
    echo [✓] Collector: Ready
) else (
    echo [⚠] Collector: Not included
)

if exist "config\.env" (
    echo [✓] Configuration: Created
) else (
    echo [✗] Configuration: Missing
)

echo ================================================================

if "!INSTALL_SUCCESS!"=="1" (
    echo.
    echo ✅ Installation completed successfully!
    echo.
    echo Next steps:
    echo   1. Run: start.bat
    echo   2. Browser will open automatically
    echo   3. Login: admin / admin
) else (
    echo.
    echo ⚠️ Installation completed with warnings
    echo.
    echo Some components may need manual installation
    echo Check the status above for details
)

echo.
pause

popd
endlocal
INSTALL_EOF

# =============================================================================
# 9. Windows Start Script
# =============================================================================

echo "9. 🚀 Creating Windows start script..."

cat > start.bat << 'START_EOF'
@echo off
setlocal enabledelayedexpansion

title PulseOne Industrial IoT Platform

echo ================================================================
echo PulseOne Industrial IoT Platform v4.0
echo ================================================================

pushd "%~dp0"

:: Check Node.js
where node >nul 2>&1
if errorlevel 1 (
    echo ERROR: Node.js is not installed!
    echo Please run install.bat first
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
START_EOF

# =============================================================================
# 10. README Creation
# =============================================================================

echo "10. 📚 Creating user documentation..."

cat > README.txt << 'README_EOF'
================================================================
PulseOne Industrial IoT Platform v4.0 FINAL
Complete Deployment Package
================================================================

QUICK START GUIDE:
==================
1. Extract all files to desired location
2. Run: install.bat (fully automated installation)
3. Run: start.bat (starts all services)
4. Browser opens automatically to http://localhost:3000
5. Login with: admin / admin

WHAT'S AUTOMATED:
=================
✅ Node.js automatic download and silent installation
✅ Redis automatic download and configuration
✅ Backend packages automatic installation
✅ SQLite3 automatic compilation/installation
✅ Directory structure automatic creation
✅ Configuration files automatic generation
✅ Browser automatic opening

SYSTEM REQUIREMENTS:
====================
• Windows 10 (64-bit) or Windows 11
• 4GB RAM minimum, 8GB recommended
• 2GB free disk space
• Internet connection for initial installation
• Administrator privileges (recommended)

INCLUDED COMPONENTS:
====================
Core Files:
• install.bat     - Automated installer
• start.bat       - Service launcher
• backend/        - Server application (Node.js)
• frontend/       - Web interface (pre-built)
• collector.exe   - Data collector (if included)
• config/         - Configuration files
• data/           - Database and logs directory

Auto-Downloaded During Installation:
• Node.js runtime (if not installed)
• Redis server for Windows
• NPM packages including SQLite3
• SQLite DLL for Windows

CONFIGURATION:
==============
Main configuration file: config/.env

To change settings, edit config/.env:
• PORT=3000           - Web server port
• REDIS_ENABLED=true  - Enable/disable Redis cache
• LOG_LEVEL=info      - Logging level

TROUBLESHOOTING:
================
If installation fails:
1. Check internet connection
2. Temporarily disable antivirus/firewall
3. Run as Administrator
4. Check data/logs/ for error messages

Node.js installation issues:
• If Node.js MSI fails, download manually from https://nodejs.org
• After manual installation, run install.bat again

Backend package issues:
• Open Command Prompt as Administrator
• Navigate to backend folder
• Run: npm install --force

SQLite3 compilation issues:
• Install Visual Studio Build Tools
• Or use: npm install sqlite3 --build-from-source=false

Redis issues:
• Check if port 6379 is already in use
• Redis is optional, system works without it

MANUAL INSTALLATION:
====================
If automated installation fails:

1. Install Node.js:
   - Download from https://nodejs.org
   - Install with default settings

2. Install backend packages:
   cd backend
   npm install

3. Download Redis (optional):
   - https://github.com/tporadowski/redis/releases
   - Extract redis-server.exe to main folder

4. Run start.bat

SUPPORT:
========
GitHub: https://github.com/smart-guard/PulseOne
Documentation: https://github.com/smart-guard/PulseOne/wiki

VERSION HISTORY:
================
v4.0 - Fully automated installation
v3.0 - Redis integration
v2.0 - Frontend integration
v1.0 - Initial release

================================================================
Thank you for using PulseOne Industrial IoT Platform!
================================================================
README_EOF

# Convert to Windows line endings
for script in install.bat start.bat README.txt; do
    if command -v unix2dos &> /dev/null; then
        unix2dos "$script" 2>/dev/null
    else
        awk '{printf "%s\r\n", $0}' "$script" > "${script}.tmp"
        mv "${script}.tmp" "$script"
    fi
done

echo "✅ Windows scripts created with proper line endings"

# =============================================================================
# 11. Create Deployment Package
# =============================================================================

echo "11. 📦 Creating final deployment package..."

cd "$DIST_DIR"

# Create ZIP package
PACKAGE_ZIP="${PACKAGE_NAME}_v4.0_FINAL_${TIMESTAMP}.zip"

if command -v zip &> /dev/null; then
    zip -r "$PACKAGE_ZIP" "$PACKAGE_NAME/" > /dev/null 2>&1
    PACKAGE_SIZE=$(du -sh "$PACKAGE_ZIP" | cut -f1)
    echo "✅ ZIP package created: $PACKAGE_ZIP ($PACKAGE_SIZE)"
elif command -v tar &> /dev/null; then
    tar -czf "${PACKAGE_NAME}_v4.0_FINAL_${TIMESTAMP}.tar.gz" "$PACKAGE_NAME/"
    PACKAGE_SIZE=$(du -sh "${PACKAGE_NAME}_v4.0_FINAL_${TIMESTAMP}.tar.gz" | cut -f1)
    echo "✅ TAR.GZ package created ($PACKAGE_SIZE)"
else
    echo "⚠️ No compression tool found, package in: $PACKAGE_DIR"
fi

# =============================================================================
# 12. Final Summary
# =============================================================================

echo ""
echo "================================================================="
echo "🎉 PulseOne v4.0 FINAL Deployment Package Created!"
echo "================================================================="
echo ""
echo "📦 Package Details:"
echo "   Location: $DIST_DIR"
if [ -f "$PACKAGE_ZIP" ]; then
    echo "   File: $PACKAGE_ZIP"
    echo "   Size: $PACKAGE_SIZE"
fi
echo ""
echo "📋 Package Contents:"
echo "   ✅ Automated installer (install.bat)"
echo "   ✅ Service launcher (start.bat)"
echo "   ✅ Backend source code"
echo "   ✅ Frontend compiled files"
if [ -f "$PACKAGE_DIR/collector.exe" ]; then
    echo "   ✅ Collector executable"
else
    echo "   ⚠️ Collector not included (build failed or skipped)"
fi
echo "   ✅ Configuration templates"
echo "   ✅ Documentation (README.txt)"
echo ""
echo "🚀 Deployment Instructions:"
echo "   1. Copy package to Windows machine"
echo "   2. Extract ZIP file"
echo "   3. Run install.bat (fully automated)"
echo "   4. Run start.bat"
echo "   5. Access http://localhost:3000"
echo ""
echo "✨ Key Features:"
echo "   • Node.js automatic installation"
echo "   • Redis automatic setup"
echo "   • SQLite3 automatic compilation"
echo "   • Zero manual configuration"
echo "   • One-click startup"
echo ""
echo "================================================================="
echo "Build completed at: $(date '+%Y-%m-%d %H:%M:%S')"
echo "================================================================="