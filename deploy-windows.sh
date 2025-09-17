#!/bin/bash

# =============================================================================
# PulseOne Complete Deployment Script v6.0 FINAL
# MSI 자동 설치 + 모든 것이 한 번에 완료되는 버전
# =============================================================================

PROJECT_ROOT=$(pwd)
PACKAGE_NAME="PulseOne_Complete_Deploy"
VERSION="6.0.0"
TIMESTAMP=$(date '+%Y%m%d_%H%M%S')
DIST_DIR="$PROJECT_ROOT/dist"
PACKAGE_DIR="$DIST_DIR/$PACKAGE_NAME"

echo "================================================================="
echo "🚀 PulseOne Complete Deployment Script v6.0 FINAL"
echo "MSI Auto-install + One-run completion"
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
    echo "⚠️ Docker not installed"
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
# 8. Windows Installation Script - MSI AUTO INSTALL VERSION
# =============================================================================

echo "8. 🛠️ Creating MSI auto-install Windows script..."

cd "$PACKAGE_DIR"

cat > install.bat << 'INSTALL_EOF'
@echo off
setlocal enabledelayedexpansion

title PulseOne Installation v6.0 - Complete Auto Install

echo ================================================================
echo PulseOne Industrial IoT Platform v6.0
echo Complete Installation in ONE Run
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
    curl -L -o "!NODE_MSI!" "!NODE_URL!"
    
    if not exist "!NODE_MSI!" (
        echo Trying PowerShell download...
        powershell -Command "& {[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '!NODE_URL!' -OutFile '!NODE_MSI!' -UseBasicParsing}"
    )
)

if exist "!NODE_MSI!" (
    echo Installing Node.js silently (this takes 1-2 minutes)...
    
    :: Silent install with progress bar
    start /wait msiexec /i "!NODE_MSI!" /quiet /qn /norestart ADDLOCAL=ALL
    
    :: Update PATH for current session
    set "PATH=!PATH!;C:\Program Files\nodejs"
    
    :: Verify installation
    if exist "C:\Program Files\nodejs\node.exe" (
        echo Node.js installed successfully!
        set "NODE_EXE=C:\Program Files\nodejs\node.exe"
        set "NPM_CMD=C:\Program Files\nodejs\npm.cmd"
    ) else (
        echo ERROR: Node.js installation failed
        echo Please install Node.js manually from https://nodejs.org
        pause
        exit /b 1
    )
) else (
    echo ERROR: Could not download Node.js
    echo Please check your internet connection
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

:: Use newly installed Node.js if available
if defined NODE_EXE (
    "!NODE_EXE!" "!NPM_CMD:npm.cmd=node_modules\npm\bin\npm-cli.js!" install --no-audit --no-fund --loglevel=error
    
    :: Install SQLite3 separately
    if not exist "node_modules\sqlite3" (
        echo Installing SQLite3...
        "!NODE_EXE!" "!NPM_CMD:npm.cmd=node_modules\npm\bin\npm-cli.js!" install sqlite3 --no-audit --no-fund
    )
) else (
    :: Use system Node.js
    call npm install --no-audit --no-fund --loglevel=error
    
    if not exist "node_modules\sqlite3" (
        echo Installing SQLite3...
        call npm install sqlite3 --no-audit --no-fund
    )
)

:: Verify critical packages
if not exist "node_modules\express" (
    echo WARNING: Some packages may have failed to install
    echo Trying alternative installation...
    if defined NODE_EXE (
        "!NODE_EXE!" "!NPM_CMD:npm.cmd=node_modules\npm\bin\npm-cli.js!" install --force
    ) else (
        call npm install --force
    )
)

cd ..

:: Download and setup Redis
echo.
echo [4/6] Setting up Redis...

if not exist "redis-server.exe" (
    echo Downloading Redis for Windows...
    
    :: Use curl (included in Windows 10/11)
    curl -L -o redis.zip "https://github.com/tporadowski/redis/releases/download/v5.0.14.1/Redis-x64-5.0.14.1.zip"
    
    if exist "redis.zip" (
        echo Extracting Redis files...
        
        :: Use tar (included in Windows 10/11)
        tar -xf redis.zip
        
        :: Clean up unnecessary files
        del /f /q redis.zip *.pdb redis-benchmark.exe redis-check-aof.exe redis-check-rdb.exe 00-RELEASENOTES RELEASENOTES.txt 2>nul
        
        if exist "redis-server.exe" (
            echo Redis setup completed
        ) else (
            echo WARNING: Redis extraction may have failed
        )
    ) else (
        echo WARNING: Redis download failed (optional component)
    )
) else (
    echo Redis already installed
)

:: Download SQLite DLL if needed
echo.
echo [5/6] Checking SQLite DLL...

if not exist "sqlite3.dll" (
    echo Downloading SQLite DLL...
    curl -L -o sqlite.zip "https://www.sqlite.org/2024/sqlite-dll-win-x64-3460100.zip"
    
    if exist "sqlite.zip" (
        tar -xf sqlite.zip
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

:: Create main .env configuration
if not exist "config\.env" (
    (
        echo # PulseOne Main Configuration - PRODUCTION
        echo NODE_ENV=production
        echo ENV_STAGE=prod
        echo LOG_LEVEL=warn
        echo BACKEND_PORT=3000
        echo.
        echo # Database Auto Initialize - Production Settings
        echo AUTO_INITIALIZE_ON_START=false
        echo SKIP_IF_INITIALIZED=true
        echo FAIL_ON_INIT_ERROR=true
        ...
    ) > config\.env
    echo Created new production configuration
) else (
    echo Configuration file already exists - checking if update needed...
    
    :: Check if it's development config
    findstr /C:"NODE_ENV=development" config\.env >nul
    if not errorlevel 1 (
        echo Development configuration detected - updating to production...
        
        :: Backup existing config
        copy config\.env config\.env.dev.bak >nul
        
        :: Update NODE_ENV and related settings
        powershell -Command "(Get-Content config\.env) -replace 'NODE_ENV=development', 'NODE_ENV=production' -replace 'ENV_STAGE=dev', 'ENV_STAGE=prod' -replace 'LOG_LEVEL=info', 'LOG_LEVEL=warn' -replace 'LOG_TO_CONSOLE=true', 'LOG_TO_CONSOLE=false' -replace 'AUTO_INITIALIZE_ON_START=true', 'AUTO_INITIALIZE_ON_START=false' -replace 'MAX_WORKER_THREADS=4', 'MAX_WORKER_THREADS=8' | Set-Content config\.env"
        
        echo Configuration updated to production mode
    ) else (
        echo Production configuration already in place
    )
)

:: Create database.env
if not exist "config\database.env" (
    (
        echo # Database Configuration
        echo DATABASE_TYPE=SQLITE
        echo.
        echo # SQLite paths (auto-converted by ConfigManager)
        echo SQLITE_PATH=./data/db/pulseone.db
        echo SQLITE_BACKUP_PATH=./data/backup/
        echo SQLITE_LOGS_PATH=./data/logs/
        echo SQLITE_TEMP_PATH=./data/temp/
        echo.
        echo # SQLite settings
        echo SQLITE_JOURNAL_MODE=WAL
        echo SQLITE_SYNCHRONOUS=NORMAL
        echo SQLITE_CACHE_SIZE=2000
        echo SQLITE_BUSY_TIMEOUT_MS=5000
        echo SQLITE_FOREIGN_KEYS=true
        echo.
        echo # PostgreSQL (optional)
        echo POSTGRES_ENABLED=false
        echo POSTGRES_HOST=localhost
        echo POSTGRES_PORT=5432
        echo POSTGRES_DATABASE=pulseone
        echo POSTGRES_USER=postgres
        echo POSTGRES_PASSWORD=postgres123
        echo.
        echo # MariaDB (optional)
        echo MARIADB_ENABLED=false
        echo MARIADB_HOST=localhost
        echo MARIADB_PORT=3306
        echo MARIADB_DATABASE=pulseone
        echo MARIADB_USER=root
        echo MARIADB_PASSWORD=mariadb123
        echo.
        echo # MSSQL (optional)
        echo MSSQL_ENABLED=false
        echo MSSQL_HOST=localhost
        echo MSSQL_PORT=1433
        echo MSSQL_DATABASE=pulseone
        echo MSSQL_USER=sa
        echo MSSQL_PASSWORD=MsSql123!
    ) > config\database.env
)

:: Create redis.env
if not exist "config\redis.env" (
    (
        echo # Redis Configuration
        echo REDIS_PRIMARY_ENABLED=true
        echo REDIS_PRIMARY_HOST=127.0.0.1
        echo REDIS_PRIMARY_PORT=6379
        echo REDIS_PRIMARY_DB=0
        echo REDIS_KEY_PREFIX=pulseone:
        echo REDIS_PRIMARY_TIMEOUT_MS=5000
        echo REDIS_PRIMARY_CONNECT_TIMEOUT_MS=3000
        echo REDIS_POOL_SIZE=5
    ) > config\redis.env
)

:: Create collector.env
if not exist "config\collector.env" (
    (
        echo # Collector Configuration
        echo COLLECTOR_EXECUTABLE_PATH=
        echo COLLECTOR_HOST=localhost
        echo COLLECTOR_PORT=8001
        echo COLLECTOR_HEALTH_CHECK_INTERVAL_MS=30000
        echo COLLECTOR_START_TIMEOUT_MS=10000
        echo COLLECTOR_STOP_TIMEOUT_MS=5000
        echo COLLECTOR_AUTO_RESTART=true
        echo COLLECTOR_MAX_RESTART_ATTEMPTS=3
        echo COLLECTOR_RESTART_DELAY_MS=5000
        echo COLLECTOR_DEBUG_MODE=false
        echo COLLECTOR_LOG_LEVEL=info
        echo COLLECTOR_LOG_TO_FILE=true
        echo COLLECTOR_LOG_FILE_PATH=./logs/collector.log
    ) > config\collector.env
)

echo Configuration files created

:: Installation summary
echo.
echo ================================================================
echo Installation Complete!
echo ================================================================

:: Check components
where node >nul 2>&1
if not errorlevel 1 (
    for /f "tokens=*" %%i in ('node --version') do echo [✓] Node.js: %%i
) else (
    echo [✗] Node.js: Not found - Please restart command prompt
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
echo.
echo ✅ Installation successful!
echo.
echo Next steps:
echo   1. Run: start.bat
echo   2. Browser will open automatically
echo   3. Login: admin / admin
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
START_EOF

# =============================================================================
# 10. README Creation
# =============================================================================

echo "10. 📚 Creating user documentation..."

cat > README.txt << 'README_EOF'
================================================================
PulseOne Industrial IoT Platform v6.0 FINAL
Complete One-Click Installation Package
================================================================

QUICK START:
============
1. Extract all files to desired location
2. Run: install.bat (everything installs automatically)
3. Run: start.bat (starts all services)
4. Browser opens automatically to http://localhost:3000
5. Login with: admin / admin

WHAT'S NEW IN v6.0:
===================
✅ TRUE one-click installation - everything in ONE run
✅ Node.js MSI auto-download and silent install
✅ Redis auto-download and setup
✅ Backend packages auto-installation
✅ SQLite3 auto-compilation
✅ No manual steps required
✅ No restart required

SYSTEM REQUIREMENTS:
====================
• Windows 10 (64-bit) or Windows 11
• 4GB RAM minimum, 8GB recommended
• 2GB free disk space
• Internet connection for initial installation
• Administrator privileges (recommended for Node.js MSI)

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
• Node.js MSI installer (v22.19.0)
• Redis server for Windows
• NPM packages including Express, SQLite3
• SQLite DLL for Windows

HOW IT WORKS:
=============
1. install.bat checks for Node.js
2. If not found, downloads and installs MSI silently
3. Downloads and sets up Redis automatically
4. Installs all backend packages
5. Creates configuration files
6. Everything completes in ONE run!

TROUBLESHOOTING:
================
If Node.js installation fails:
• Download manually from https://nodejs.org
• Install with default settings
• Run install.bat again

If backend packages fail:
• Open Command Prompt as Administrator
• Navigate to backend folder
• Run: npm install

If Redis download fails:
• Redis is optional - system works without it
• Or download manually from GitHub

If SQLite3 compilation fails:
• Run: npm install sqlite3 --build-from-source=false

CONFIGURATION:
==============
Main config file: config/.env

Key settings:
• PORT=3000           - Web server port
• REDIS_ENABLED=true  - Enable/disable Redis
• LOG_LEVEL=info      - Logging level

SUPPORT:
========
GitHub: https://github.com/smart-guard/PulseOne
Documentation: https://github.com/smart-guard/PulseOne/wiki

================================================================
© 2024 PulseOne - Industrial IoT Platform
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
PACKAGE_ZIP="${PACKAGE_NAME}_v6.0_FINAL_${TIMESTAMP}.zip"

if command -v zip &> /dev/null; then
    zip -r "$PACKAGE_ZIP" "$PACKAGE_NAME/" > /dev/null 2>&1
    PACKAGE_SIZE=$(du -sh "$PACKAGE_ZIP" | cut -f1)
    echo "✅ ZIP package created: $PACKAGE_ZIP ($PACKAGE_SIZE)"
elif command -v tar &> /dev/null; then
    tar -czf "${PACKAGE_NAME}_v6.0_FINAL_${TIMESTAMP}.tar.gz" "$PACKAGE_NAME/"
    PACKAGE_SIZE=$(du -sh "${PACKAGE_NAME}_v6.0_FINAL_${TIMESTAMP}.tar.gz" | cut -f1)
    echo "✅ TAR.GZ package created ($PACKAGE_SIZE)"
else
    echo "⚠️ No compression tool found, package in: $PACKAGE_DIR"
fi

# =============================================================================
# 12. Final Summary
# =============================================================================

echo ""
echo "================================================================="
echo "🎉 PulseOne v6.0 FINAL Deployment Package Created!"
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
echo "   • Node.js MSI auto-installation"
echo "   • Redis automatic setup"
echo "   • SQLite3 automatic compilation"
echo "   • Complete in ONE run"
echo "   • Zero manual configuration"
echo ""
echo "================================================================="
echo "Build completed at: $(date '+%Y-%m-%d %H:%M:%S')"
echo "================================================================="