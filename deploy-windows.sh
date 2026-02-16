#!/bin/bash
set -euo pipefail

# =============================================================================
# PulseOne Complete Deployment Script v6.1
# Cross-compile on Mac/Linux → produce a self-contained Windows package
# =============================================================================

PROJECT_ROOT=$(pwd)
VERSION=$(grep '"version"' "$PROJECT_ROOT/version.json" | cut -d'"' -f4 || echo "6.1.0")
PACKAGE_NAME="deploy-v$VERSION"
TIMESTAMP=$(date '+%Y%m%d_%H%M%S')
DIST_DIR="$PROJECT_ROOT/dist"
PACKAGE_DIR="$DIST_DIR/$PACKAGE_NAME"

echo "================================================================="
echo "🚀 PulseOne Complete Deployment Script v${VERSION}"
echo "Cross-compile on Mac → Windows deployment package"
echo "================================================================="

# =============================================================================
# 1. Project Structure Check
# =============================================================================

echo "1. 🔍 Checking project structure..."

REQUIRED_DIRS=("backend" "frontend" "core/collector" "core/export-gateway")
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

# Docker check for Collector cross-compilation
if ! command -v docker &> /dev/null; then
    echo "⚠️ Docker not installed"
    if [ "${SKIP_COLLECTOR:-false}" != "true" ]; then
        echo "Set SKIP_COLLECTOR=true to skip native builds"
        echo "Or install Docker for MinGW cross-compilation"
        exit 1
    fi
fi

echo "✅ Project structure check completed"

# =============================================================================
# 2. Build Environment Setup
# =============================================================================

echo "2. 📦 Setting up build environment..."

# Selective cleanup to preserve setup_assets
mkdir -p "$PACKAGE_DIR"
DEPS_DIR="$PACKAGE_DIR/setup_assets"
mkdir -p "$DEPS_DIR"
find "$PACKAGE_DIR" -mindepth 1 -maxdepth 1 -not -name "setup_assets" -exec rm -rf {} +

if ! command -v node &> /dev/null; then
    echo "⚠️ Node.js not found on build system — frontend build may fail"
fi

# =============================================================================
# 2.1 📥 Pre-downloading dependencies (Dockerized to protect host)
# =============================================================================
echo "2.1 📥 Pre-downloading dependencies for offline support..."

# Use a lightweight container to download assets to the DEPS_DIR
docker run --rm -v "$DEPS_DIR:/assets" alpine:latest sh -c "
  apk add --no-cache curl && \
  cd /assets && \
  ( [ -f node-v22.13.1-x64.msi ] || curl -L -O https://nodejs.org/dist/v22.13.1/node-v22.13.1-x64.msi ) && \
  ( [ -f redis.zip ] || curl -L -o redis.zip https://github.com/tporadowski/redis/releases/download/v5.0.14.1/Redis-x64-5.0.14.1.zip ) && \
  ( [ -f influxdb.zip ] || curl -L -o influxdb.zip https://download.influxdata.com/influxdb/releases/influxdb2-2.7.11-windows-amd64.zip ) && \
  ( [ -f WinSW-x64.exe ] || curl -L -o WinSW-x64.exe https://github.com/winsw/winsw/releases/download/v2.11.0/WinSW-x64.exe ) && \
  ( [ -f sqlite.zip ] || curl -L -o sqlite.zip https://www.sqlite.org/2024/sqlite-dll-win-x64-3460100.zip )
"

echo "✅ All dependencies verified/downloaded via Docker"

echo "✅ Build environment setup completed"

# =============================================================================
# 3. Frontend Build
# =============================================================================

if [ "${SKIP_FRONTEND:-false}" = "true" ]; then
    echo "3. 🎨 Skipping frontend build (SKIP_FRONTEND=true)"
else
    echo "3. 🎨 Building frontend..."
    cd "$PROJECT_ROOT/frontend"

    if [ -d "dist" ]; then
        echo "✅ Frontend already built (dist found), skipping..."
    else
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
    fi
fi

cd "$PROJECT_ROOT"

# =============================================================================
# 4. Collector & Export Gateway Build (Docker MinGW Cross-compile)
# =============================================================================

# 4. ⚙️ Cross-compile native components (Collector & Gateway)
# =============================================================================
echo "4. ⚙️ Cross-compiling native components for Windows..."

if [ "${SKIP_NATIVE:-false}" = "true" ]; then
    echo "⏭️  Skipping native component compilation (SKIP_NATIVE=true)"
else
    # Check for rebuild flag
REBUILD_IMAGE=${REBUILD_IMAGE:-false}
for arg in "$@"; do
    if [ "$arg" == "--rebuild-image" ]; then
        REBUILD_IMAGE=true
    fi
done

# =============================================================================
# 1. Build Container Check
# =============================================================================
echo "1. 🐳 Checking build container..."

IMAGE_NAME="pulseone-windows-builder"
IMAGE_EXISTS=$(docker image ls -q "$IMAGE_NAME")

# Perform a quick sanity check if image exists
if [ -n "$IMAGE_EXISTS" ] && [ "$REBUILD_IMAGE" != "true" ]; then
    echo "🔍 Verifying image integrity..."
    # Check for a critical file that should be in the image
    if ! docker run --rm "$IMAGE_NAME" ls /usr/x86_64-w64-mingw32/include/bacnet/bacdef.h >/dev/null 2>&1; then
        echo "⚠️  Image appears to be stale or incomplete. Forcing rebuild."
        REBUILD_IMAGE=true
    fi
fi

if [ -z "$IMAGE_EXISTS" ] || [ "$REBUILD_IMAGE" == "true" ]; then
    echo "📦 Building Windows build container from Dockerfile.windows..."
    docker build -f "$PROJECT_ROOT/core/collector/Dockerfile.windows" -t "$IMAGE_NAME" "$PROJECT_ROOT"
else
    echo "✅ Found pulseone-windows-builder image"
fi

    # Use a more stable temporary directory for Docker mount (Bypass Docker for Mac sync issues)
    TMP_OUTPUT="/tmp/pulseone-dist-$(date +%s)"
    mkdir -p "$TMP_OUTPUT"
    chmod 777 "$TMP_OUTPUT"
    sync
    sleep 2

    docker run --rm \
        -v "$PROJECT_ROOT/core:/src/core" \
        -v "$TMP_OUTPUT:/output" \
        pulseone-windows-builder bash -c "
            set -e
            
            # Use POSIX thread model for C++11/14/17 features (std::thread, std::mutex)
            export CC=x86_64-w64-mingw32-gcc-posix
            export CXX=x86_64-w64-mingw32-g++-posix
            export AR=x86_64-w64-mingw32-ar
            export RANLIB=x86_64-w64-mingw32-ranlib
            
            # 0. Build Shared Libraries (CRITICAL for linking)
            cd /src/core/shared
            echo 'Building Shared Libraries (Windows Cross-compile)...'
            rm -rf build lib && mkdir lib
            make -j2 CROSS_COMPILE_WINDOWS=1
            
            # 1. Build Collector
            cd /src/core/collector
            echo 'Building Collector (Release) with ALL Drivers...'
            rm -rf build-windows bin-windows/*.exe
            make -f Makefile.windows -j2
            
            if [ -f bin-windows/collector.exe ]; then
                x86_64-w64-mingw32-strip --strip-unneeded bin-windows/collector.exe
                cp bin-windows/collector.exe /output/pulseone-collector.exe
                echo '✅ Collector binary copied to output'
                
                if [ -d bin-windows/plugins ]; then
                    echo '📦 Copying driver plugins...'
                    mkdir -p /output/plugins
                    cp bin-windows/plugins/*.dll /output/plugins/
                    echo '✅ Driver plugins copied'
                fi
            else
                echo '❌ Collector binary NOT found after build'
                exit 1
            fi
            
            # 2. Build Export Gateway
            cd /src/core/export-gateway
            echo 'Building Export Gateway (Release)...'
            if [ -f Makefile ]; then
                rm -rf build bin/*.exe
                make -j2 CROSS_COMPILE_WINDOWS=1
                
                if [ -f bin/export-gateway.exe ]; then
                    x86_64-w64-mingw32-strip --strip-unneeded bin/export-gateway.exe
                    cp bin/export-gateway.exe /output/pulseone-export-gateway.exe
                    echo '✅ Export Gateway built'
                else
                    echo '❌ Export Gateway binary NOT found'
                    exit 1
                fi
            else
                echo '⚠️ Makefile not found for export-gateway'
                exit 1
            fi
        "

    # Copy from TMP_OUTPUT to PACKAGE_DIR
    echo "📦 Copying artifacts from temporary build directory..."
    cp -r "$TMP_OUTPUT/"* "$PACKAGE_DIR/"
    rm -rf "$TMP_OUTPUT"

    # Verify binaries
    if [ -f "$PACKAGE_DIR/pulseone-collector.exe" ]; then
        COLLECTOR_SIZE=$(du -sh "$PACKAGE_DIR/pulseone-collector.exe" | cut -f1)
        echo "✅ Collector: $COLLECTOR_SIZE"
    else
        echo "❌ Collector build failed"
        exit 1
    fi

    if [ -f "$PACKAGE_DIR/pulseone-export-gateway.exe" ]; then
        GATEWAY_SIZE=$(du -sh "$PACKAGE_DIR/pulseone-export-gateway.exe" | cut -f1)
        echo "✅ Export Gateway: $GATEWAY_SIZE"
    else
        echo "❌ Export Gateway build failed"
        exit 1
    fi
    fi
# =============================================================================
# 5. Backend Source Code Copy
# =============================================================================

if [ "${SKIP_BACKEND:-false}" = "true" ]; then
    echo "5. ⏭️  Skipping backend source code copy (SKIP_BACKEND=true)"
else
    echo "5. 🔧 Copying backend source code..."
    cd "$PACKAGE_DIR"
    mkdir -p backend

    # Copy backend source (exclude dev artifacts)
    rsync -a \
        --exclude='nnode_modules' \
        --exclude='.git' \
        --exclude='__tests__' \
        --exclude='__mocks__' \
        --exclude='*.test.js' \
        --exclude='*.spec.js' \
        "$PROJECT_ROOT/backend/" ./backend/

    echo "✅ Backend source code copied"
fi

# =============================================================================
# 6. Copy Frontend Build Results
# =============================================================================

if [ "${SKIP_FRONTEND:-false}" = "true" ]; then
    echo "6. 🎨 Skipping frontend build results copy (SKIP_FRONTEND=true)"
else
    echo "6. 🎨 Copying frontend build results..."
    cd "$PACKAGE_DIR"
    mkdir -p frontend
    cp -r "$PROJECT_ROOT/frontend/dist/"* ./frontend/
    echo "✅ Frontend copy completed"
fi

# =============================================================================
# 7. Copy Config and Data Structure
# =============================================================================

echo "7. 📝 Copying configuration and creating structure..."

# Copy existing config (minus secrets)
if [ -d "$PROJECT_ROOT/config" ]; then
    rsync -a --exclude='secrets' "$PROJECT_ROOT/config/" ./config/
fi

# Create directory structure
mkdir -p data/db data/backup data/logs data/temp config

echo "✅ Configuration and structure created"

# 8. Windows Installation Script (install.bat)
# =============================================================================

echo "8. 🛠️ Creating install.bat (Air-Gapped Support)..."

cd "$PACKAGE_DIR"

cat > install.bat << 'INSTALL_EOF'
@echo off
setlocal enabledelayedexpansion

title PulseOne Installation v6.1 - Complete Offline Setup

echo ================================================================
echo PulseOne Industrial IoT Platform v6.1
echo Complete Installation (Air-Gapped Support)
echo ================================================================

pushd "%~dp0"
set "INSTALL_DIR=%CD%"
set "ASSETS_DIR=%INSTALL_DIR%\setup_assets"

:: Check administrator privileges
net session >nul 2>&1
if errorlevel 1 (
    echo [WARNING] Running without administrator privileges.
    echo Node.js installation may require admin rights.
    timeout /t 3
)

echo.
echo [1/9] Checking Node.js...

:: Check if Node.js is installed
where node >nul 2>&1
if not errorlevel 1 (
    for /f "tokens=*" %%i in ('node --version') do set "NODE_VERSION=%%i"
    echo Found Node.js !NODE_VERSION!
    goto install_redis
)

:: Install Node.js from bundled asset
echo Node.js not found. Installing from bundled package...
set "NODE_MSI=node-v22.13.1-x64.msi"

if exist "!ASSETS_DIR!\!NODE_MSI!" (
    echo Installing Node.js silently (this takes 1-2 minutes)...
    start /wait msiexec /i "!ASSETS_DIR!\!NODE_MSI!" /quiet /qn /norestart ADDLOCAL=ALL
    
    :: Update PATH for current session
    set "PATH=!PATH!;C:\Program Files\nodejs"
    
    if exist "C:\Program Files\nodejs\node.exe" (
        echo Node.js installed successfully!
    ) else (
        echo ERROR: Node.js installation failed.
        pause & exit /b 1
    )
) else (
    echo ERROR: Bundled Node.js installer missing in setup_assets\
    echo Please ensure you are using the complete deployment package.
    pause & exit /b 1
)

:install_redis
echo.
echo [2/9] Setting up Redis...

if not exist "redis-server.exe" (
    if exist "!ASSETS_DIR!\redis.zip" (
        echo Extracting Redis from bundled asset...
        tar -xf "!ASSETS_DIR!\redis.zip"
        del /f /q *.pdb redis-benchmark.exe redis-check-* 2>nul
        echo Redis setup completed
    ) else (
        echo WARNING: Bundled Redis missing. Skipping optional component.
    )
) else (
    echo Redis already installed
)

:install_influx
echo.
echo [3/9] Setting up InfluxDB v2...

if not exist "influxd.exe" (
    if exist "!ASSETS_DIR!\influxdb.zip" (
        echo Extracting InfluxDB from bundled asset...
        tar -xf "!ASSETS_DIR!\influxdb.zip" --strip-components=1
        echo InfluxDB setup completed
    ) else (
        echo WARNING: Bundled InfluxDB missing.
    )
) else (
    echo InfluxDB already installed
)

:install_sqlite
echo.
echo [4/9] Checking SQLite DLL...

if not exist "sqlite3.dll" (
    if exist "!ASSETS_DIR!\sqlite.zip" (
        tar -xf "!ASSETS_DIR!\sqlite.zip"
        echo SQLite DLL extracted
    )
)

:install_winsw
echo.
echo [5/9] Setting up Service Support (WinSW)...

if exist "!ASSETS_DIR!\WinSW-x64.exe" (
    copy /y "!ASSETS_DIR!\WinSW-x64.exe" "WinSW-x64.exe" >nul
    copy /y "WinSW-x64.exe" "pulseone-backend-svc.exe" >nul
    copy /y "WinSW-x64.exe" "pulseone-collector-svc.exe" >nul
    copy /y "WinSW-x64.exe" "pulseone-export-gateway-svc.exe" >nul
    copy /y "WinSW-x64.exe" "pulseone-redis-svc.exe" >nul
    copy /y "WinSW-x64.exe" "pulseone-influxdb-svc.exe" >nul
)

:install_backend
echo.
echo [6/9] Installing backend packages...
pushd backend
if exist "nnode_modules" rd /s /q nnode_modules
echo Running npm install...
call npm install --no-audit --no-fund --loglevel=error
popd

:setup_config
echo.
echo [7/9] Creating configuration files (Portable Relative Paths)...

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
        echo # Database Auto Initialize
        echo AUTO_INITIALIZE_ON_START=false
        echo SKIP_IF_INITIALIZED=true
        echo FAIL_ON_INIT_ERROR=true
        echo.
        echo # Directories (Relative to app root)
        echo DATA_DIR=./data
        echo LOGS_DIR=./data/logs
        echo CONFIG_DIR=./config
        echo.
        echo # Config Files
        echo CONFIG_FILES=database.env,redis.env,timeseries.env,messaging.env,collector.env,csp.env,security.env
        echo.
        echo # Logging
        echo LOG_TO_CONSOLE=false
        echo LOG_TO_FILE=true
        echo LOG_FILE_PATH=./data/logs/
        echo LOG_MAX_SIZE_MB=100
        echo LOG_MAX_FILES=30
        echo LOG_USE_DATE_FOLDERS=true
        echo LOG_USE_HOURLY_FILES=false
        echo LOG_RETENTION_DAYS=30
        echo.
        echo # System
        echo MAX_WORKER_THREADS=8
        echo DEFAULT_TIMEOUT_MS=5000
        echo MAINTENANCE_MODE=false
        echo.
        echo # Gateways
        echo CSP_GATEWAY_ENABLED=true
        echo EXPORT_GATEWAY_ENABLED=true
        echo.
        echo # Security
        echo SECRETS_DIR=./config/secrets
        echo ENCRYPTION_ENABLED=true
        echo.
        echo # Service Ports
        echo EXPORT_GATEWAY_PORT=8080
        echo CSP_GATEWAY_HEALTH_CHECK_PORT=8081
        echo CSP_USE_DYNAMIC_TARGETS=true
    ) > config\.env
    echo Created new production configuration
)
 else (
    echo Configuration file already exists - checking if update needed...

    :: Check if it's development config
    findstr /C:"NODE_ENV=development" config\.env >nul
    if not errorlevel 1 (
        echo Development configuration detected - updating to production...

        :: Backup existing config
        copy config\.env config\.env.dev.bak >nul

        :: Update to production settings
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
        echo # SQLite paths
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
        echo # PostgreSQL (optional - set POSTGRES_ENABLED=true to use)
        echo POSTGRES_ENABLED=false
        echo POSTGRES_HOST=localhost
        echo POSTGRES_PORT=5432
        echo POSTGRES_DATABASE=pulseone
        echo POSTGRES_USER=postgres
        echo POSTGRES_PASSWORD=CHANGE_ME
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

:: Create Service XML Configurations (use -svc suffix to avoid name collision)
echo.
echo Creating Service Configurations...

(
    echo ^<service^>
    echo   ^<id^>pulseone-backend^</id^>
    echo   ^<name^>PulseOne Backend^</name^>
    echo   ^<description^>PulseOne Industrial IoT Backend Service^</description^>
    echo   ^<executable^>node^</executable^>
    echo   ^<arguments^>backend\app.js^</arguments^>
    echo   ^<workingdirectory^>%INSTALL_DIR%^</workingdirectory^>
    echo   ^<logmode^>roll^</logmode^>
    echo   ^<onfailure action="restart" delay="10 sec"/^>
    echo   ^<env name="NODE_ENV" value="production"/^>
    echo ^</service^>
) > pulseone-backend-svc.xml

(
    echo ^<service^>
    echo   ^<id^>pulseone-collector^</id^>
    echo   ^<name^>PulseOne Collector^</name^>
    echo   ^<description^>PulseOne Data Collector Service^</description^>
    echo   ^<executable^>%INSTALL_DIR%\pulseone-collector.exe^</executable^>
    echo   ^<workingdirectory^>%INSTALL_DIR%^</workingdirectory^>
    echo   ^<logmode^>roll^</logmode^>
    echo   ^<onfailure action="restart" delay="10 sec"/^>
    echo ^</service^>
) > pulseone-collector-svc.xml

(
    echo ^<service^>
    echo   ^<id^>pulseone-export-gateway^</id^>
    echo   ^<name^>PulseOne Export Gateway^</name^>
    echo   ^<description^>PulseOne Export Gateway Service^</description^>
    echo   ^<executable^>%INSTALL_DIR%\pulseone-export-gateway.exe^</executable^>
    echo   ^<workingdirectory^>%INSTALL_DIR%^</workingdirectory^>
    echo   ^<logmode^>roll^</logmode^>
    echo   ^<onfailure action="restart" delay="10 sec"/^>
    echo ^</service^>
) > pulseone-export-gateway-svc.xml

(
    echo ^<service^>
    echo   ^<id^>pulseone-redis^</id^>
    echo   ^<name^>PulseOne Redis^</name^>
    echo   ^<description^>PulseOne Redis Cache Service^</description^>
    echo   ^<executable^>%INSTALL_DIR%\redis-server.exe^</executable^>
    echo   ^<arguments^>--port 6379^</arguments^>
    echo   ^<workingdirectory^>%INSTALL_DIR%^</workingdirectory^>
    echo   ^<logmode^>roll^</logmode^>
    echo   ^<onfailure action="restart" delay="10 sec"/^>
    echo ^</service^>
) > pulseone-redis-svc.xml

(
    echo ^<service^>
    echo   ^<id^>pulseone-influxdb^</id^>
    echo   ^<name^>PulseOne InfluxDB^</name^>
    echo   ^<description^>PulseOne InfluxDB Service^</description^>
    echo   ^<executable^>%INSTALL_DIR%\influxd.exe^</executable^>
    echo   ^<arguments^>run^</arguments^>
    echo   ^<workingdirectory^>%INSTALL_DIR%^</workingdirectory^>
    echo   ^<logmode^>roll^</logmode^>
    echo   ^<onfailure action="restart" delay="10 sec"/^>
    echo ^</service^>
) > pulseone-influxdb-svc.xml

:: Create Service Installer Script (actual commands, not echo)
(
    echo @echo off
    echo setlocal enabledelayedexpansion
    echo title PulseOne Service Installer
    echo.
    echo :: Check administrator privileges
    echo net session ^>nul 2^>^&1
    echo if errorlevel 1 ^(
    echo     echo [!] ERROR: Please run this script AS ADMINISTRATOR to register services.
    echo     pause ^& exit /b 1
    echo ^)
    echo.
    echo pushd "%%~dp0"
    echo.
    echo echo [1/3] Registering Backend Service...
    echo pulseone-backend-svc.exe install
    echo pulseone-backend-svc.exe start
    echo.
    echo echo [2/3] Registering Collector Service...
    echo if exist "pulseone-collector.exe" ^(
    echo     pulseone-collector-svc.exe install
    echo     pulseone-collector-svc.exe start
    echo ^) else ^(
    echo     echo [-] pulseone-collector.exe not found. Skipping.
    echo ^)
    echo.
    echo echo [3/3] Registering Export Gateway Service...
    echo if exist "pulseone-export-gateway.exe" ^(
    echo     pulseone-export-gateway-svc.exe install
    echo     pulseone-export-gateway-svc.exe start
    echo ^) else ^(
    echo     echo [-] pulseone-export-gateway.exe not found. Skipping.
    echo ^)
    echo.
    echo echo [4/5] Registering Redis Service...
    echo if exist "redis-server.exe" ^(
    echo     copy WinSW-x64.exe pulseone-redis-svc.exe ^>nul
    echo     pulseone-redis-svc.exe install
    echo     pulseone-redis-svc.exe start
    echo ^)
    echo.
    echo echo [5/5] Registering InfluxDB Service...
    echo if exist "influxd.exe" ^(
    echo     copy WinSW-x64.exe pulseone-influxdb-svc.exe ^>nul
    echo     pulseone-influxdb-svc.exe install
    echo     pulseone-influxdb-svc.exe start
    echo ^)
    echo.
    echo echo.
    echo echo All services registered and started!
    echo popd
    echo pause
) > install_service.bat

:: Create Service Uninstaller Script
(
    echo @echo off
    echo setlocal enabledelayedexpansion
    echo title PulseOne Service Uninstaller
    echo.
    echo :: Check administrator privileges
    echo net session ^>nul 2^>^&1
    echo if errorlevel 1 ^(
    echo     echo [!] ERROR: Please run this script AS ADMINISTRATOR.
    echo     pause ^& exit /b 1
    echo ^)
    echo.
    echo pushd "%%~dp0"
    echo.
    echo echo [1/3] Removing Backend Service...
    echo pulseone-backend-svc.exe stop
    echo pulseone-backend-svc.exe uninstall
    echo.
    echo echo [2/3] Removing Collector Service...
    echo if exist "pulseone-collector-svc.exe" ^(
    echo     pulseone-collector-svc.exe stop
    echo     pulseone-collector-svc.exe uninstall
    echo ^)
    echo.
    echo echo [3/3] Removing Export Gateway Service...
    echo if exist "pulseone-export-gateway-svc.exe" ^(
    echo     pulseone-export-gateway-svc.exe stop
    echo     pulseone-export-gateway-svc.exe uninstall
    echo ^)
    echo.
    echo echo [4/5] Removing Redis Service...
    echo if exist "pulseone-redis-svc.exe" ^(
    echo     pulseone-redis-svc.exe stop
    echo     pulseone-redis-svc.exe uninstall
    echo ^)
    echo.
    echo echo [5/5] Removing InfluxDB Service...
    echo if exist "pulseone-influxdb-svc.exe" ^(
    echo     pulseone-influxdb-svc.exe stop
    echo     pulseone-influxdb-svc.exe uninstall
    echo ^)
    echo.
    echo echo All services stopped and unregistered!
    echo popd
    echo pause
) > uninstall_service.bat

:: Installation summary
echo.
echo ================================================================
echo Installation Complete!
echo ================================================================

:: Check components
where node >nul 2>&1
if not errorlevel 1 (
    for /f "tokens=*" %%i in ('node --version') do echo [OK] Node.js: %%i
) else (
    echo [!!] Node.js: Not found - Please restart command prompt
)

if exist "redis-server.exe" (
    echo [OK] Redis: Server installed
) else (
    echo [--] Redis: Not installed (optional)
)

if exist "backend\nnode_modules\express" (
    echo [OK] Backend: Packages installed
) else (
    echo [!!] Backend: Packages missing
)

if exist "backend\nnode_modules\sqlite3" (
    echo [OK] Database: SQLite3 module installed
) else (
    echo [--] Database: SQLite3 module missing
)

if exist "frontend\index.html" (
    echo [OK] Frontend: Files ready
) else (
    echo [--] Frontend: Files missing
)

if exist "pulseone-collector.exe" (
    echo [OK] Collector: Ready
) else (
    echo [--] Collector: Not included
)

if exist "pulseone-export-gateway.exe" (
    echo [OK] Export Gateway: Ready
) else (
    echo [--] Export Gateway: Not included
)

if exist "config\.env" (
    echo [OK] Configuration: Created
) else (
    echo [!!] Configuration: Missing
)

echo ================================================================
echo.
echo Installation successful!
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
echo PulseOne Industrial IoT Platform v6.1
echo ================================================================

pushd "%~dp0"

:: Check Node.js
where node >nul 2>&1
if errorlevel 1 (
    echo ERROR: Node.js is not installed or not in PATH!
    echo Please run install.bat first
    pause
    exit /b 1
)

:: Environment setup
echo [1/5] Environment setup...

if not exist "data" mkdir data >nul 2>&1
if not exist "data\db" mkdir data\db >nul 2>&1
if not exist "data\logs" mkdir data\logs >nul 2>&1
if not exist "config" mkdir config >nul 2>&1

echo OK: Environment ready

:: Process cleanup (PulseOne processes only via window title)
echo [2/5] Process cleanup...
for /f "tokens=2" %%p in ('tasklist /fi "WINDOWTITLE eq PulseOne*" /fo csv /nh 2^>nul ^| findstr /r "[0-9]"') do (
    taskkill /pid %%~p /f >nul 2>&1
)
taskkill /F /IM redis-server.exe >nul 2>&1
taskkill /F /IM pulseone-collector.exe >nul 2>&1
taskkill /F /IM pulseone-export-gateway.exe >nul 2>&1
timeout /t 2 /nobreak >nul
echo OK: Processes cleaned

:: Start Redis
echo [3/5] Starting Redis server...
if exist "redis-server.exe" (
    if exist "redis.windows.conf" (
        start "PulseOne-Redis" /B redis-server.exe redis.windows.conf >nul 2>&1
    ) else (
        start "PulseOne-Redis" /B redis-server.exe --port 6379 --maxmemory 512mb >nul 2>&1
    )
    timeout /t 2 /nobreak >nul
    echo OK: Redis started on port 6379
) else (
    echo INFO: Redis not found, continuing without cache
)

:: Start InfluxDB
echo [4/7] Starting InfluxDB server...
if exist "influxd.exe" (
    start "PulseOne-InfluxDB" /B influxd.exe run >nul 2>&1
    timeout /t 3 /nobreak >nul
    echo OK: InfluxDB started on port 8086
)

:: Start Backend
echo [5/7] Starting backend server...
pushd backend
start "PulseOne-Backend" /B node app.js >nul 2>&1
popd

:: Wait for startup
echo [5/5] Waiting for server startup...
timeout /t 5 /nobreak >nul

:: Start Collector if available
if exist "pulseone-collector.exe" (
    echo Starting data collector...
    start "PulseOne-Collector" /B pulseone-collector.exe >nul 2>&1
    echo OK: Collector started
)

:: Start Export Gateway if available
if exist "pulseone-export-gateway.exe" (
    echo Starting export gateway...
    start "PulseOne-ExportGW" /B pulseone-export-gateway.exe >nul 2>&1
    echo OK: Export Gateway started
)

:: Open browser
echo.
echo Opening web browser...
start http://localhost:3000

:: Display status
echo.
echo ================================================================
echo PulseOne is running!
echo ================================================================
echo.
echo Web Interface: http://localhost:3000
echo Default Login: admin / admin
echo.
echo Active Services:
echo   * Backend API: Port 3000
if exist "redis-server.exe" echo   * Redis Cache: Port 6379
if exist "influxd.exe" echo   * InfluxDB: Port 8086
if exist "pulseone-collector.exe" echo   * Data Collector: Running
if exist "pulseone-export-gateway.exe" echo   * Export Gateway: Running
echo.
echo Press any key to stop all services...
echo ================================================================
pause >nul

:: Stop all services gracefully
echo.
echo Stopping all services...
for /f "tokens=2" %%p in ('tasklist /fi "WINDOWTITLE eq PulseOne*" /fo csv /nh 2^>nul ^| findstr /r "[0-9]"') do (
    taskkill /pid %%~p /f >nul 2>&1
)
taskkill /F /IM redis-server.exe >nul 2>&1
taskkill /F /IM pulseone-collector.exe >nul 2>&1
taskkill /F /IM pulseone-export-gateway.exe >nul 2>&1

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
PulseOne Industrial IoT Platform v6.1
Complete One-Click Installation Package
================================================================

QUICK START:
============
1. Extract all files to desired location
2. Run: install.bat (everything installs automatically)
3. Run: start.bat (starts all services)
4. Browser opens automatically to http://localhost:3000
5. Login with: admin / admin

SERVICE MODE (optional):
========================
To run PulseOne as Windows background services:
1. Run: install_service.bat AS ADMINISTRATOR
2. Services will auto-start on boot

To remove services:
1. Run: uninstall_service.bat AS ADMINISTRATOR

SYSTEM REQUIREMENTS:
====================
- Windows 10 (64-bit) or Windows 11
- 4GB RAM minimum, 8GB recommended
- 2GB free disk space
- Internet connection for initial installation
- Administrator privileges (recommended for Node.js MSI)

INCLUDED COMPONENTS:
====================
Core Files:
  install.bat               - Automated installer
  start.bat                 - Service launcher (console mode)
  install_service.bat       - Register as Windows services
  uninstall_service.bat     - Unregister Windows services
  backend/                  - Server application (Node.js)
  frontend/                 - Web interface (pre-built)
  pulseone-collector.exe    - Data collector (native)
  pulseone-export-gateway.exe - Export Gateway (native)
  config/                   - Configuration files
  data/                     - Database and logs directory

Auto-Downloaded During Installation:
  Node.js MSI installer (LTS)
  Redis server for Windows
  InfluxDB v2.x server for Windows
  NPM packages including Express, SQLite3
  SQLite DLL for Windows
  WinSW (Windows Service Wrapper)

TROUBLESHOOTING:
================
If Node.js installation fails:
  Download manually from https://nodejs.org
  Install with default settings
  Run install.bat again

If backend packages fail:
  Open Command Prompt as Administrator
  Navigate to backend folder
  Run: npm install

If Redis download fails:
  Redis is optional - system works without it

CONFIGURATION:
==============
Main config file: config\.env

Key settings:
  BACKEND_PORT=3000    - Web server port
  LOG_LEVEL=warn       - Logging level (debug/info/warn/error)

================================================================
PulseOne - Industrial IoT Platform
================================================================
README_EOF

# =============================================================================
# 11. Convert line endings to CRLF for Windows
# =============================================================================

echo "11. 🔄 Converting line endings for Windows..."

WINDOWS_FILES=(
    install.bat start.bat README.txt
    install_service.bat uninstall_service.bat
    pulseone-backend-svc.xml pulseone-collector-svc.xml pulseone-export-gateway-svc.xml
)

for script in "${WINDOWS_FILES[@]}"; do
    if [ -f "$script" ]; then
        if command -v unix2dos &> /dev/null; then
            unix2dos "$script" 2>/dev/null
        else
            awk '{printf "%s\r\n", $0}' "$script" > "${script}.tmp"
            mv "${script}.tmp" "$script"
        fi
    fi
done

echo "✅ Windows scripts created with proper line endings"

# =============================================================================
# 12. Create Deployment Package
# =============================================================================

echo "12. 📦 Creating final deployment package..."

cd "$DIST_DIR"

PACKAGE_ZIP="${PACKAGE_NAME}_v${VERSION}_${TIMESTAMP}.zip"

if command -v zip &> /dev/null; then
    zip -r "$PACKAGE_ZIP" "$PACKAGE_NAME/" > /dev/null 2>&1
    PACKAGE_SIZE=$(du -sh "$PACKAGE_ZIP" | cut -f1)
    echo "✅ ZIP package created: $PACKAGE_ZIP ($PACKAGE_SIZE)"
elif command -v tar &> /dev/null; then
    tar -czf "${PACKAGE_NAME}_v${VERSION}_${TIMESTAMP}.tar.gz" "$PACKAGE_NAME/"
    PACKAGE_SIZE=$(du -sh "${PACKAGE_NAME}_v${VERSION}_${TIMESTAMP}.tar.gz" | cut -f1)
    echo "✅ TAR.GZ package created ($PACKAGE_SIZE)"
else
    echo "⚠️ No compression tool found, package in: $PACKAGE_DIR"
fi

# =============================================================================
# 13. Final Summary
# =============================================================================

echo ""
echo "================================================================="
echo "🎉 PulseOne v${VERSION} Deployment Package Created!"
echo "================================================================="
echo ""
echo "📦 Package Details:"
echo "   Location: $DIST_DIR"
if [ -f "$DIST_DIR/$PACKAGE_ZIP" ]; then
    echo "   File: $PACKAGE_ZIP"
    echo "   Size: $PACKAGE_SIZE"
fi
echo ""
echo "📋 Package Contents:"
echo "   ✅ Automated installer (install.bat)"
echo "   ✅ Service launcher (start.bat)"
echo "   ✅ Service installer/uninstaller"
echo "   ✅ Backend source code"
echo "   ✅ Frontend compiled files"
if [ -f "$PACKAGE_DIR/pulseone-collector.exe" ]; then
    echo "   ✅ Collector executable"
else
    echo "   ⚠️ Collector not included"
fi
if [ -f "$PACKAGE_DIR/pulseone-export-gateway.exe" ]; then
    echo "   ✅ Export Gateway executable"
else
    echo "   ⚠️ Export Gateway not included"
fi
echo "   ✅ Configuration templates"
echo "   ✅ Documentation (README.txt)"
echo ""
echo "🚀 Deployment:"
echo "   1. Copy ZIP to Windows machine"
echo "   2. Extract"
echo "   3. Run install.bat"
echo "   4. Run start.bat"
echo "   5. Access http://localhost:3000"
echo ""
echo "================================================================="
echo "Build completed at: $(date '+%Y-%m-%d %H:%M:%S')"
echo "================================================================="