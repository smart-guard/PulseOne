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
