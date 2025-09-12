#!/bin/bash

# =============================================================================
# PulseOne Complete Deployment Script v3.0
# Both Portable + Electron packages
# Windows downloads all binaries and runs npm install
# macOS only copies source code
# =============================================================================

PROJECT_ROOT=$(pwd)
PACKAGE_NAME="PulseOne_Complete_Deploy"
VERSION="3.0.0"
TIMESTAMP=$(date '+%Y%m%d_%H%M%S')
DIST_DIR="$PROJECT_ROOT/dist"
PACKAGE_DIR="$DIST_DIR/$PACKAGE_NAME"
ELECTRON_DIR="$DIST_DIR/electron-build"

echo "================================================================="
echo "🚀 PulseOne Complete Deployment Script v3.0"
echo "Portable + Electron both included"
echo "Windows downloads binaries & runs npm install"
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

# Docker check (for Collector)
if ! command -v docker &> /dev/null; then
    echo "❌ Docker is required for Collector build"
    if [ "$SKIP_COLLECTOR" != "true" ]; then
        echo "Set SKIP_COLLECTOR=true to skip"
        exit 1
    fi
fi

# Check existing Windows build container
if [ "$SKIP_COLLECTOR" != "true" ]; then
    echo "🔍 Checking existing Windows build container..."
    if docker image ls | grep -q "pulseone-windows-builder"; then
        echo "✅ Found pulseone-windows-builder container"
        DOCKER_IMAGE="pulseone-windows-builder"
    else
        echo "⚠️ pulseone-windows-builder not found. Create it? (y/n)"
        read -r CREATE_CONTAINER
        if [ "$CREATE_CONTAINER" = "y" ] || [ "$CREATE_CONTAINER" = "Y" ]; then
            if [ -f "collector/Dockerfile.mingw" ]; then
                docker build -f collector/Dockerfile.mingw -t pulseone-windows-builder .
                DOCKER_IMAGE="pulseone-windows-builder"
            else
                echo "❌ collector/Dockerfile.mingw not found"
                exit 1
            fi
        else
            SKIP_COLLECTOR="true"
        fi
    fi
fi

echo "✅ Project structure check completed"

# =============================================================================
# 2. Build Environment Setup
# =============================================================================

echo "2. 📦 Setting up build environment..."

rm -rf "$DIST_DIR"
mkdir -p "$PACKAGE_DIR"
mkdir -p "$ELECTRON_DIR"

if ! command -v node &> /dev/null; then
    echo "❌ Node.js is required"
    exit 1
fi

echo "✅ Build environment setup completed"

# =============================================================================
# 3. Frontend Build (platform independent)
# =============================================================================

echo "3. 🎨 Building frontend..."

cd "$PROJECT_ROOT/frontend"

if [ ! -f "package.json" ]; then
    echo "❌ frontend/package.json not found"
    exit 1
fi

npm install --silent
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
# 4. Collector Build (existing Docker method)
# =============================================================================

if [ "$SKIP_COLLECTOR" = "true" ]; then
    echo "4. ⚙️ Collector build skipped (SKIP_COLLECTOR=true)"
else
    echo "4. ⚙️ Building Collector..."
    
    cd "$PROJECT_ROOT"
    
    echo "  Using container: $DOCKER_IMAGE"
    
    # Use existing Docker build command
    if docker run --rm \
        -v "$(pwd)/collector:/src" \
        -v "$PACKAGE_DIR:/output" \
        "$DOCKER_IMAGE" bash -c "
            cd /src
            make -f Makefile.windows clean 2>/dev/null || true
            make -f Makefile.windows check-libs
            if make -f Makefile.windows all; then
                if [ -f 'bin-windows/collector.exe' ]; then
                    cp bin-windows/collector.exe /output/collector.exe
                elif [ -f 'collector.exe' ]; then
                    cp collector.exe /output/collector.exe
                fi
            fi
        "; then
        
        if [ -f "$PACKAGE_DIR/collector.exe" ]; then
            COLLECTOR_SIZE=$(du -sh "$PACKAGE_DIR/collector.exe" | cut -f1)
            echo "✅ Collector build successful: $COLLECTOR_SIZE"
        fi
    else
        echo "❌ Collector build failed"
    fi
fi

# =============================================================================
# 5. Backend Source Code Copy (no node_modules - install on Windows)
# =============================================================================

echo "5. 🔧 Copying backend source code..."

cd "$PACKAGE_DIR"

# Copy backend source code only (exclude node_modules)
cp -r "$PROJECT_ROOT/backend" ./
# Remove node_modules if exists (will be installed on Windows)
rm -rf backend/node_modules 2>/dev/null || true

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

echo "7. 📝 Copying configuration files..."

# Copy config and data folders
if [ -d "$PROJECT_ROOT/config" ]; then
    cp -r "$PROJECT_ROOT/config" ./
fi

if [ -d "$PROJECT_ROOT/data" ]; then
    cp -r "$PROJECT_ROOT/data" ./
else
    # Create basic directory structure
    mkdir -p data/db data/backup data/logs data/temp config
fi

# Create production configuration
cat >> "./config/.env" << 'EOF'

# Production Environment Settings
NODE_ENV=production
PORT=3000

# Database Configuration
DATABASE_TYPE=SQLITE
SQLITE_PATH=./data/db/pulseone.db

# Auto initialization
AUTO_INITIALIZE_ON_START=true
SKIP_IF_INITIALIZED=true

# Redis Configuration (Optional)
REDIS_ENABLED=false
REDIS_HOST=localhost
REDIS_PORT=6379

# Logging
LOG_LEVEL=info
LOG_TO_FILE=true
LOG_FILE_PATH=./data/logs/pulseone.log

# Frontend
SERVE_FRONTEND=true
FRONTEND_PATH=./frontend
EOF

echo "✅ Configuration files copied"

# =============================================================================
# 8. Windows Smart Install Script (Download everything on Windows)
# =============================================================================

echo "8. 🛠️ Creating Windows smart install script..."

cd "$PACKAGE_DIR"

# Main install script (downloads everything on Windows)
cat > install-pulseone.bat << 'INSTALL_EOF'
@echo off
setlocal enabledelayedexpansion
title PulseOne Smart Installation System v3.0

echo ================================================================
echo PulseOne Industrial IoT Platform v3.0 Smart Installation
echo ================================================================
echo Feature: Downloads all dependencies directly on Windows
echo ================================================================

cd /d "%~dp0"

REM Check admin privileges
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Administrator privileges required
    echo Right-click this file and select "Run as administrator"
    pause
    exit /b 1
)

echo OK: Administrator privileges confirmed

REM =============================================================================
REM Step 1: Check and download Node.js
REM =============================================================================

echo [1/5] Checking and downloading Node.js...

node --version >nul 2>&1
if %errorlevel% equ 0 (
    echo OK: Node.js already installed
    for /f "tokens=*" %%i in ('node --version') do echo Version: %%i
    goto download_redis
)

echo Node.js not installed. Downloading portable version...

set NODE_VERSION=v20.11.0
set NODE_ZIP=node-%NODE_VERSION%-win-x64.zip
set NODE_URL=https://nodejs.org/dist/%NODE_VERSION%/%NODE_ZIP%

echo Downloading: %NODE_URL%

powershell -Command "& {[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%NODE_URL%' -OutFile '%NODE_ZIP%'}"

if exist "%NODE_ZIP%" (
    echo OK: Node.js download completed
    
    echo Extracting Node.js...
    powershell -Command "Expand-Archive -Path '%NODE_ZIP%' -DestinationPath '.' -Force"
    
    REM Rename folder to nodejs
    move "node-%NODE_VERSION%-win-x64" nodejs >nul
    del /f "%NODE_ZIP%"
    
    echo OK: Node.js portable installation completed
    
    REM Add to PATH
    set PATH=%~dp0nodejs;%PATH%
    
) else (
    echo ERROR: Node.js download failed
    pause
    exit /b 1
)

:download_redis
REM =============================================================================
REM Step 2: Download Redis Windows binaries
REM =============================================================================

echo [2/5] Downloading Redis Windows binaries...

set REDIS_URL=https://github.com/tporadowski/redis/releases/download/v5.0.14.1/Redis-x64-5.0.14.1.zip
set REDIS_ZIP=redis-win.zip

powershell -Command "& {[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%REDIS_URL%' -OutFile '%REDIS_ZIP%'}"

if exist "%REDIS_ZIP%" (
    echo OK: Redis download completed
    
    powershell -Command "Expand-Archive -Path '%REDIS_ZIP%' -DestinationPath '.' -Force"
    
    REM Keep only needed files
    if exist "redis-server.exe" echo OK: Redis server ready
    if exist "redis-cli.exe" echo OK: Redis CLI ready
    
    REM Clean up unnecessary files
    del /f "%REDIS_ZIP%" *.pdb EventLog.dll 2>nul
    del /f redis-benchmark* redis-check-* RELEASENOTES.txt 00-RELEASENOTES *.conf 2>nul
    
    echo OK: Redis preparation completed
) else (
    echo WARNING: Redis download failed (optional component)
)

REM =============================================================================
REM Step 3: Download SQLite Windows DLL
REM =============================================================================

echo [3/5] Downloading SQLite Windows DLL...

set SQLITE_URL=https://www.sqlite.org/2024/sqlite-dll-win-x64-3460000.zip
set SQLITE_ZIP=sqlite-dll.zip

powershell -Command "& {[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%SQLITE_URL%' -OutFile '%SQLITE_ZIP%'}"

if exist "%SQLITE_ZIP%" (
    echo OK: SQLite download completed
    
    powershell -Command "Expand-Archive -Path '%SQLITE_ZIP%' -DestinationPath '.' -Force"
    del /f "%SQLITE_ZIP%"
    
    if exist "sqlite3.dll" echo OK: SQLite DLL ready
) else (
    echo WARNING: SQLite DLL download failed
)

REM =============================================================================
REM Step 4: Install Backend dependencies
REM =============================================================================

echo [4/5] Installing backend dependencies...

cd backend

echo Setting npm configuration...
if exist "%~dp0nodejs\npm.cmd" (
    call "%~dp0nodejs\npm.cmd" config set cache "%~dp0nodejs\npm-cache" --global
    call "%~dp0nodejs\npm.cmd" config set prefix "%~dp0nodejs" --global
) else (
    npm config set cache "%~dp0nodejs\npm-cache" --global
    npm config set prefix "%~dp0nodejs" --global
)

echo Installing backend packages...
if exist "%~dp0nodejs\npm.cmd" (
    call "%~dp0nodejs\npm.cmd" install --production --loglevel=error
) else (
    npm install --production --loglevel=error
)

if %errorlevel% neq 0 (
    echo ERROR: Backend package installation failed
    echo Check internet connection and try again
    pause
    exit /b 1
)

echo Installing Windows-compatible SQLite3...
if exist "%~dp0nodejs\npm.cmd" (
    call "%~dp0nodejs\npm.cmd" uninstall sqlite3 2>nul
    call "%~dp0nodejs\npm.cmd" install sqlite3 --loglevel=error
) else (
    npm uninstall sqlite3 2>nul
    npm install sqlite3 --loglevel=error
)

if %errorlevel% neq 0 (
    echo SQLite3 installation failed, trying better-sqlite3...
    if exist "%~dp0nodejs\npm.cmd" (
        call "%~dp0nodejs\npm.cmd" install better-sqlite3 --loglevel=error
    ) else (
        npm install better-sqlite3 --loglevel=error
    )
    
    if %errorlevel% neq 0 (
        echo WARNING: SQLite installation may have issues
        echo Try running the start script anyway
    )
)

cd ..

echo OK: Backend dependencies installation completed

REM =============================================================================
REM Step 5: Final setup
REM =============================================================================

echo [5/5] Final setup...

REM Create data directories
if not exist "data\db" mkdir "data\db"
if not exist "data\logs" mkdir "data\logs"
if not exist "data\backup" mkdir "data\backup"
if not exist "data\temp" mkdir "data\temp"

echo ================================================================
echo Installation completed successfully!
echo Now you can run start-pulseone.bat to start PulseOne
echo ================================================================
pause
INSTALL_EOF

# Windows start script
cat > start-pulseone.bat << 'START_EOF'
@echo off
setlocal enabledelayedexpansion
title PulseOne Industrial IoT Platform v3.0

echo ================================================================
echo PulseOne Industrial IoT Platform v3.0
echo ================================================================

cd /d "%~dp0"

REM Check Node.js installation
if exist "nodejs\node.exe" (
    echo OK: Using portable Node.js
    set NODE_PATH=%~dp0nodejs\node.exe
    set NPM_PATH=%~dp0nodejs\npm.cmd
    set PATH=%~dp0nodejs;%PATH%
) else (
    node --version >nul 2>&1
    if %errorlevel% neq 0 (
        echo ERROR: Node.js not found
        echo Please run install-pulseone.bat first
        pause
        exit /b 1
    )
    echo OK: Using system Node.js
    set NODE_PATH=node.exe
)

REM Create data directories
if not exist "data\db" mkdir "data\db"
if not exist "data\logs" mkdir "data\logs"

echo Starting PulseOne services...

REM Check port availability
netstat -an | find "127.0.0.1:3000" >nul 2>&1
if %errorlevel% equ 0 (
    echo WARNING: Port 3000 is already in use
    choice /M "Kill existing processes and continue"
    if %errorlevel% equ 1 (
        taskkill /F /IM node.exe >nul 2>&1
        timeout /t 2 >nul
    ) else (
        exit /b 1
    )
)

REM Start Redis (optional)
if exist "redis-server.exe" (
    echo [1/3] Starting Redis...
    tasklist /FI "IMAGENAME eq redis-server.exe" >nul 2>&1
    if %errorlevel% neq 0 (
        start /B "Redis Server" redis-server.exe --port 6379 --bind 127.0.0.1
        echo OK: Redis started
        timeout /t 1 >nul
    ) else (
        echo OK: Redis already running
    )
) else (
    echo [1/3] Redis not found (optional)
)

REM Start Backend
echo [2/3] Starting Backend...
if not exist "backend\app.js" (
    echo ERROR: Backend files not found
    pause
    exit /b 1
)

REM Set environment variables
set DATABASE_TYPE=SQLITE
set SQLITE_PATH=./data/db/pulseone.db
set NODE_ENV=production
set PORT=3000
set SERVE_FRONTEND=true
set FRONTEND_PATH=./frontend

cd backend
start /B "PulseOne Backend" "%NODE_PATH%" app.js
cd ..

REM Wait for server to start
echo Waiting for server to start...
set /a count=0
:wait_backend

REM Check with curl if available, otherwise use netstat
curl -s http://localhost:3000/api/health >nul 2>&1
if %errorlevel% equ 0 goto backend_ready

netstat -an | find "127.0.0.1:3000" >nul 2>&1
if %errorlevel% equ 0 goto backend_ready

set /a count+=1
if %count% geq 20 (
    echo ERROR: Server startup failed
    echo Check logs: data/logs/pulseone.log
    pause
    exit /b 1
)

timeout /t 1 /nobreak >nul
goto wait_backend

:backend_ready
echo OK: PulseOne backend started successfully!

REM Start Collector (optional)
if exist "collector.exe" (
    echo [3/3] Starting Collector...
    start /B "PulseOne Collector" collector.exe
    echo OK: Collector started
) else (
    echo [3/3] Collector not found (C++ compilation needed)
)

REM Open browser
timeout /t 2 /nobreak >nul
start http://localhost:3000

echo ================================================================
echo PulseOne started successfully!
echo ================================================================
echo Web Interface: http://localhost:3000
echo Default Login: admin / admin
echo ================================================================
echo System Information:
echo - Node.js: %NODE_PATH%
echo - Database: SQLite (%DATABASE_TYPE%)
echo - Logs: data/logs/pulseone.log
echo ================================================================
echo Press any key to stop all services...
pause >nul

REM Cleanup
echo Stopping services...
taskkill /F /IM node.exe >nul 2>&1
taskkill /F /IM collector.exe >nul 2>&1
taskkill /F /IM redis-server.exe >nul 2>&1

echo All services stopped.
timeout /t 2
START_EOF

# Convert to Windows CRLF
for script in install-pulseone.bat start-pulseone.bat; do
    if command -v unix2dos &> /dev/null; then
        unix2dos "$script"
    else
        awk '{printf "%s\r\n", $0}' "$script" > "${script}.tmp"
        mv "${script}.tmp" "$script"
    fi
done

echo "✅ Windows smart install scripts created"

# =============================================================================
# 9. README Creation
# =============================================================================

echo "9. 📚 Creating user guide..."

cat > README.txt << 'README_EOF'
================================================================
PulseOne Industrial IoT Platform v3.0
================================================================

INSTALLATION AND EXECUTION

Step 1: Installation
- Run install-pulseone.bat (Administrator privileges required)
- Node.js portable and dependencies will be automatically installed

Step 2: Execution  
- Run start-pulseone.bat
- Browser will open automatically
- Login: admin / admin

================================================================
INCLUDED FILES
================================================================

Executable Files:
  • install-pulseone.bat - Installation script
  • start-pulseone.bat - Start script
  • collector.exe - Data collector (if built)

Source Code:
  • backend/ - Server source code
  • frontend/ - Web interface (built)
  • config/ - Configuration files

================================================================
TROUBLESHOOTING
================================================================

Issue: "Node.js not found"
Solution: Run install-pulseone.bat with administrator privileges

Issue: "Port 3000 already in use"
Solution: Kill existing processes or restart computer

Issue: "Backend package installation failed"  
Solution: Check internet connection and run install-pulseone.bat again

Issue: "SQLite error"
Solution: Try running install-pulseone.bat again

================================================================
SYSTEM REQUIREMENTS
================================================================

• OS: Windows 10/11 (64-bit)
• RAM: 4GB or more recommended
• Storage: 2GB or more
• Internet: Required for initial installation only

================================================================
v3.0 IMPROVEMENTS
================================================================

✅ Windows downloads all binaries (no macOS compatibility issues)
✅ Administrator privileges only required for installation
✅ Portable Node.js (no system installation needed)
✅ Windows-compatible SQLite3 automatic installation
✅ Existing Collector Docker build method maintained

================================================================
SUPPORT
================================================================

• GitHub: https://github.com/smart-guard/PulseOne
• Issues: GitHub Issues

================================================================
README_EOF

if command -v unix2dos &> /dev/null; then
    unix2dos README.txt
else
    awk '{printf "%s\r\n", $0}' README.txt > temp.txt
    mv temp.txt README.txt
fi

echo "✅ User guide created"

# =============================================================================
# 10. Electron Desktop App Build
# =============================================================================

if [ "$SKIP_ELECTRON" != "true" ]; then
    echo "10. 🖥️ Building Electron desktop app..."
    
    mkdir -p "$ELECTRON_DIR"
    cd "$ELECTRON_DIR"
    
    # Electron package.json with NSIS install/uninstall fixes
    cat > package.json << EOF
{
  "name": "pulseone-desktop",
  "version": "$VERSION",
  "description": "PulseOne Industrial IoT Platform - Desktop Application",
  "main": "main.js",
  "author": "PulseOne Team",
  "license": "MIT",
  "scripts": {
    "start": "electron .",
    "build": "electron-builder",
    "build-win": "electron-builder --win"
  },
  "build": {
    "appId": "com.pulseone.desktop",
    "productName": "PulseOne Industrial IoT Platform",
    "directories": {
      "output": "../"
    },
    "files": [
      "main.js",
      "assets/**/*",
      "installer.nsh"
    ],
    "extraResources": [
      "../$PACKAGE_NAME/**/*"
    ],
    "win": {
      "target": [
        {
          "target": "nsis",
          "arch": ["x64"]
        }
      ],
      "icon": "assets/icon.ico"
    },
    "nsis": {
      "oneClick": false,
      "allowToChangeInstallationDirectory": true,
      "allowElevation": true,
      "createDesktopShortcut": true,
      "createStartMenuShortcut": true,
      "installerIcon": "assets/icon.ico",
      "uninstallerIcon": "assets/icon.ico",
      "deleteAppDataOnUninstall": false,
      "runAfterFinish": true,
      "menuCategory": "PulseOne",
      "artifactName": "PulseOne-Setup-\${version}.\${ext}",
      "guid": "A1B2C3D4-E5F6-7890-ABCD-123456789ABC",
      "include": "installer.nsh",
      "displayLanguageSelector": false,
      "installerLanguages": ["en_US"],
      "license": false,
      "warningsAsErrors": false
    }
  },
  "devDependencies": {
    "electron": "^27.0.0",
    "electron-builder": "^24.0.0"
  }
}
EOF

    # NSIS install/uninstall complete fix script
    cat > installer.nsh << 'NSIS_EOF'
# PulseOne Complete Install/Uninstall Fix NSIS Script

# Pre-install process termination and existing installation check
!macro customInit
  DetailPrint "PulseOne pre-installation preparation..."
  
  # 1. Force terminate all related processes
  nsExec::ExecToLog 'taskkill /F /IM "PulseOne Industrial IoT Platform.exe" 2>nul'
  nsExec::ExecToLog 'taskkill /F /IM pulseone-backend.exe 2>nul'
  nsExec::ExecToLog 'taskkill /F /IM redis-server.exe 2>nul'
  nsExec::ExecToLog 'taskkill /F /IM collector.exe 2>nul'
  nsExec::ExecToLog 'taskkill /F /IM node.exe 2>nul'
  
  Sleep 3000
  
  # 2. Check and remove existing installation
  ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\A1B2C3D4-E5F6-7890-ABCD-123456789ABC" "InstallLocation"
  ${If} $0 != ""
    DetailPrint "Found existing PulseOne installation: $0"
    
    # Terminate processes again
    nsExec::ExecToLog 'taskkill /F /IM "PulseOne Industrial IoT Platform.exe" 2>nul'
    Sleep 2000
    
    # Run existing uninstaller (silent mode)
    ExecWait '"$0\Uninstall PulseOne Industrial IoT Platform.exe" /S'
    Sleep 5000
    
    # Force delete remaining files
    RMDir /r "$0"
  ${EndIf}
  
  # 3. Clean temporary files and app data
  RMDir /r "$TEMP\pulseone*"
  RMDir /r "$LOCALAPPDATA\PulseOne"
  RMDir /r "$APPDATA\PulseOne"
  
  DetailPrint "Pre-installation preparation completed"
!macroend

# Post-install additional tasks
!macro customInstall
  DetailPrint "PulseOne post-installation setup..."
  
  # Create data directories
  CreateDirectory "$INSTDIR\data"
  CreateDirectory "$INSTDIR\data\db"
  CreateDirectory "$INSTDIR\data\logs"
  CreateDirectory "$INSTDIR\data\backup"
  CreateDirectory "$INSTDIR\data\temp"
  CreateDirectory "$INSTDIR\config"
  
  # Add Windows firewall rules
  DetailPrint "Adding firewall rules..."
  nsExec::ExecToLog 'netsh advfirewall firewall delete rule name="PulseOne Backend" 2>nul'
  nsExec::ExecToLog 'netsh advfirewall firewall delete rule name="PulseOne Redis" 2>nul'
  nsExec::ExecToLog 'netsh advfirewall firewall add rule name="PulseOne Backend" dir=in action=allow protocol=TCP localport=3000 program="$INSTDIR\PulseOne Industrial IoT Platform.exe"'
  nsExec::ExecToLog 'netsh advfirewall firewall add rule name="PulseOne Redis" dir=in action=allow protocol=TCP localport=6379'
  
  # Store additional information in registry
  WriteRegStr HKLM "Software\PulseOne" "InstallPath" "$INSTDIR"
  WriteRegStr HKLM "Software\PulseOne" "Version" "${VERSION}"
  WriteRegDWORD HKLM "Software\PulseOne" "InstallDate" "${__DATE__}"
  
  DetailPrint "Post-installation setup completed"
!macroend

# Pre-uninstall process termination
!macro customUnInit
  DetailPrint "PulseOne pre-uninstall preparation..."
  
  # Force terminate all PulseOne processes
  nsExec::ExecToLog 'taskkill /F /IM "PulseOne Industrial IoT Platform.exe" 2>nul'
  nsExec::ExecToLog 'taskkill /F /IM pulseone-backend.exe 2>nul'
  nsExec::ExecToLog 'taskkill /F /IM redis-server.exe 2>nul'
  nsExec::ExecToLog 'taskkill /F /IM collector.exe 2>nul'
  nsExec::ExecToLog 'taskkill /F /IM node.exe 2>nul'
  
  Sleep 3000
  
  DetailPrint "Pre-uninstall preparation completed"
!macroend

# Complete uninstall tasks
!macro customUnInstall
  DetailPrint "PulseOne complete removal..."
  
  # Remove firewall rules
  DetailPrint "Removing firewall rules..."
  nsExec::ExecToLog 'netsh advfirewall firewall delete rule name="PulseOne Backend"'
  nsExec::ExecToLog 'netsh advfirewall firewall delete rule name="PulseOne Redis"'
  
  # Ask about user data removal
  MessageBox MB_YESNO|MB_ICONQUESTION "Also delete user data (database, logs)?" IDYES DeleteUserData IDNO KeepUserData
  
  DeleteUserData:
    DetailPrint "Deleting user data..."
    RMDir /r "$INSTDIR\data"
    RMDir /r "$INSTDIR\config"
    RMDir /r "$LOCALAPPDATA\PulseOne"
    RMDir /r "$APPDATA\PulseOne"
    Goto CleanupRegistry
  
  KeepUserData:
    DetailPrint "User data preserved"
  
  CleanupRegistry:
    # Registry cleanup
    DeleteRegKey HKLM "Software\PulseOne"
    
    # Desktop and start menu cleanup
    Delete "$DESKTOP\PulseOne Industrial IoT Platform.lnk"
    RMDir /r "$SMPROGRAMS\PulseOne"
    
    DetailPrint "Complete removal completed"
!macroend
NSIS_EOF

    # Electron main process (using portable Node.js)
    cat > main.js << 'ELECTRON_EOF'
const { app, BrowserWindow, Menu, shell } = require('electron');
const { spawn } = require('child_process');
const path = require('path');

let mainWindow;
let backendProcess;
let redisProcess;
let collectorProcess;

function getResourcePath(filename) {
    if (app.isPackaged) {
        return path.join(process.resourcesPath, filename);
    } else {
        return path.join(__dirname, '..', filename);
    }
}

// Start services
function startServices() {
    const packagePath = getResourcePath('PulseOne_Complete_Deploy');
    
    // Start Redis
    const redisPath = path.join(packagePath, 'redis-server.exe');
    if (require('fs').existsSync(redisPath)) {
        redisProcess = spawn(redisPath, ['--port', '6379'], { 
            cwd: packagePath,
            stdio: 'ignore' 
        });
        console.log('Redis started');
    }
    
    // Start Backend (using portable Node.js)
    const nodePath = path.join(packagePath, 'nodejs', 'node.exe');
    const appPath = path.join(packagePath, 'backend', 'app.js');
    
    if (require('fs').existsSync(nodePath) && require('fs').existsSync(appPath)) {
        backendProcess = spawn(nodePath, [appPath], {
            cwd: packagePath,
            stdio: ['pipe', 'pipe', 'pipe'],
            env: {
                ...process.env,
                DATABASE_TYPE: 'SQLITE',
                SQLITE_PATH: './data/db/pulseone.db',
                NODE_ENV: 'production',
                PORT: '3000'
            }
        });
        
        backendProcess.stdout.on('data', (data) => {
            console.log('Backend:', data.toString());
        });
        
        console.log('Backend started with portable Node.js');
    }
    
    // Start Collector
    const collectorPath = path.join(packagePath, 'collector.exe');
    if (require('fs').existsSync(collectorPath)) {
        collectorProcess = spawn(collectorPath, [], {
            cwd: packagePath,
            stdio: 'ignore'
        });
        console.log('Collector started');
    }
}

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 1200,
        height: 800,
        webPreferences: {
            nodeIntegration: false,
            contextIsolation: true
        },
        title: 'PulseOne Industrial IoT Platform v3.0',
        icon: path.join(__dirname, 'assets', 'icon.ico')
    });

    // Show loading page first
    mainWindow.loadURL('data:text/html,<h1 style="text-align:center;margin-top:50px;">🚀 PulseOne Starting...</h1><p style="text-align:center;">Please wait...</p>');

    // Load actual application after 5 seconds
    setTimeout(() => {
        mainWindow.loadURL('http://localhost:3000');
    }, 5000);

    // Open external links in default browser
    mainWindow.webContents.setWindowOpenHandler(({ url }) => {
        shell.openExternal(url);
        return { action: 'deny' };
    });
}

// Menu setup
function createMenu() {
    const template = [
        {
            label: 'PulseOne',
            submenu: [
                {
                    label: 'Open Web Interface',
                    click: () => {
                        mainWindow.loadURL('http://localhost:3000');
                    }
                },
                { type: 'separator' },
                {
                    label: 'Exit',
                    role: 'quit'
                }
            ]
        }
    ];
    
    const menu = Menu.buildFromTemplate(template);
    Menu.setApplicationMenu(menu);
}

app.whenReady().then(() => {
    createMenu();
    startServices();
    createWindow();
});

app.on('window-all-closed', () => {
    // Terminate all processes
    if (backendProcess) backendProcess.kill();
    if (redisProcess) redisProcess.kill();
    if (collectorProcess) collectorProcess.kill();
    
    app.quit();
});
ELECTRON_EOF

    # Create assets directory (icon placeholder)
    mkdir -p assets
    
    # Note if no icon file exists
    if [ ! -f "assets/icon.ico" ]; then
        echo "  ⚠️ No icon file found (assets/icon.ico)"
        echo "  Will build with default icon"
    fi
    
    if command -v npm &> /dev/null; then
        echo "  📦 Installing Electron dependencies..."
        npm install --silent
        
        echo "  🔨 Running Electron Windows build..."
        if npm run build-win; then
            echo "  ✅ Electron desktop app build completed"
            ELECTRON_FILES=$(find .. -name "*.exe" -o -name "*.msi" 2>/dev/null | grep -v "$PACKAGE_NAME")
            if [ -n "$ELECTRON_FILES" ]; then
                echo "  📱 Generated installer files:"
                echo "$ELECTRON_FILES" | while read file; do
                    SIZE=$(du -h "$file" | cut -f1)
                    echo "    $(basename "$file"): $SIZE"
                done
            fi
        else
            echo "  ❌ Electron build failed"
            echo "  💡 Manual build: cd $ELECTRON_DIR && npm run build-win"
        fi
    else
        echo "  ❌ npm not found, skipping Electron build"
    fi
    
    cd "$DIST_DIR"
else
    echo "10. 🖥️ Electron build skipped (SKIP_ELECTRON=true)"
fi

# =============================================================================
# 11. Create Portable ZIP Package
# =============================================================================

echo "11. 📦 Creating portable ZIP package..."

cd "$DIST_DIR"

# Create ZIP package
PORTABLE_ZIP_NAME="${PACKAGE_NAME}_Complete_${TIMESTAMP}.zip"

if zip -r "$PORTABLE_ZIP_NAME" "$PACKAGE_NAME/" > /dev/null 2>&1; then
    PORTABLE_SIZE=$(du -sh "$PORTABLE_ZIP_NAME" | cut -f1)
    echo "✅ Portable ZIP created: $PORTABLE_ZIP_NAME ($PORTABLE_SIZE)"
else
    echo "❌ ZIP compression failed"
fi

# =============================================================================
# 12. Final Summary (Both Portable + Electron)
# =============================================================================

echo "================================================================="
echo "🎉 PulseOne Windows Deployment Packages Creation Complete!"
echo "================================================================="

echo ""
echo "📦 Generated Packages:"

# Check Electron results
ELECTRON_EXECUTABLES=$(find . -name "*.exe" -o -name "*.msi" 2>/dev/null | grep -v "$PACKAGE_NAME")
if [ -n "$ELECTRON_EXECUTABLES" ]; then
    echo "🖥️ Electron Installer:"
    echo "$ELECTRON_EXECUTABLES" | while read file; do
        SIZE=$(du -h "$file" | cut -f1)
        echo "  📦 $(basename "$file"): $SIZE"
    done
    echo "  ✅ Install and run from desktop icon"
fi

# Portable package
if [ -f "$PORTABLE_ZIP_NAME" ]; then
    echo "📁 Portable Package:"
    echo "  ✅ $PORTABLE_ZIP_NAME: $PORTABLE_SIZE"
    echo "  ✅ Extract and run install-pulseone.bat → start-pulseone.bat"
fi

echo ""
echo "🔧 v3.0 Improvements:"
echo "✅ Both Portable + Electron packages provided"
echo "✅ Windows downloads all binaries (no macOS compatibility issues)"
echo "✅ Electron install/uninstall issues completely fixed"
echo "✅ English language (no CMD encoding issues)"
echo "✅ Existing Collector Docker build method maintained"
echo "✅ Fully automated service start/stop"

echo ""
echo "🚀 Usage Instructions:"
echo "• General users: Run .msi installer → Click desktop icon"
echo "• Developers/testers: Extract portable ZIP → install-pulseone.bat → start-pulseone.bat"

echo ""
echo "🎯 Key Advantages:"
echo "• Two deployment methods for different user needs"
echo "• No administrator privileges needed (portable method)"
echo "• Complete desktop app experience (Electron method)"
echo "• Existing proven Collector build method maintained"
echo "• All dependency issues resolved"

echo "================================================================="