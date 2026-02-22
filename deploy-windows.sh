#!/bin/bash
set -e

# =============================================================================
# PulseOne Windows Deploy Script
# 실행 환경: Mac/Linux 호스트 (Docker 필요, pulseone-windows-builder 이미지)
#
# 사용법:
#   ./deploy-windows.sh                    # 전체 빌드 + 패키징
#   ./deploy-windows.sh --skip-shared      # shared 재빌드 스킵
#   ./deploy-windows.sh --skip-collector   # collector 재빌드 스킵
#   ./deploy-windows.sh --skip-gateway     # gateway 재빌드 스킵
#   ./deploy-windows.sh --skip-backend     # backend(pkg) 재빌드 스킵
#   ./deploy-windows.sh --skip-frontend    # frontend 재빌드 스킵
#   ./deploy-windows.sh --skip-cpp         # C++ 빌드 전체 스킵
#   ./deploy-windows.sh --no-package       # ZIP 패키징 없이 bin-windows/만 채움
#
# 이미 빌드된 바이너리가 bin-windows/에 있으면 자동으로 스킵됨
# =============================================================================

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
VERSION=$(grep '"version"' "$PROJECT_ROOT/version.json" | cut -d'"' -f4 2>/dev/null || echo "6.1.0")
TIMESTAMP=$(date '+%Y%m%d_%H%M%S')

# 중앙 빌드 결과물 폴더
BIN_DIR="$PROJECT_ROOT/bin-windows"

SKIP_SHARED=false
SKIP_COLLECTOR=false
SKIP_GATEWAY=false
SKIP_BACKEND=false
SKIP_FRONTEND=false
NO_PACKAGE=false

for arg in "$@"; do
    case "$arg" in
        --skip-shared)     SKIP_SHARED=true ;;
        --skip-collector)  SKIP_COLLECTOR=true ;;
        --skip-gateway)    SKIP_GATEWAY=true ;;
        --skip-backend)    SKIP_BACKEND=true ;;
        --skip-frontend)   SKIP_FRONTEND=true ;;
        --skip-cpp)        SKIP_SHARED=true; SKIP_COLLECTOR=true; SKIP_GATEWAY=true ;;
        --no-package)      NO_PACKAGE=true ;;
    esac
done

WIN_BUILDER="pulseone-windows-builder"

echo "================================================================="
echo "🪟 PulseOne Windows Deploy v$VERSION"
echo "   skip: shared=$SKIP_SHARED  collector=$SKIP_COLLECTOR  gateway=$SKIP_GATEWAY"
echo "         backend=$SKIP_BACKEND  frontend=$SKIP_FRONTEND"
echo "   output: $BIN_DIR"
echo "================================================================="

mkdir -p "$BIN_DIR/drivers"

# 필요한 패키징 도구
command -v zip   >/dev/null 2>&1 || brew install zip 2>/dev/null || apt-get install -y -qq zip 2>/dev/null || true
command -v rsync >/dev/null 2>&1 || brew install rsync 2>/dev/null || apt-get install -y -qq rsync 2>/dev/null || true

# =============================================================================
# [1] Shared Libraries
# =============================================================================
SHARED_LIB="$PROJECT_ROOT/core/shared/lib/Windows-Cross/libpulseone-common.a"
if [ "$SKIP_SHARED" = "false" ] && [ -f "$SHARED_LIB" ]; then
    echo "⚡ [1/5] Shared: 이미 빌드됨 → 스킵"
    SKIP_SHARED=true
fi

if [ "$SKIP_SHARED" = "false" ]; then
    echo "🔨 [1/5] Shared Libraries (Windows Cross) 빌드 중..."
    docker run --rm \
        -v "$PROJECT_ROOT/core":/src/core \
        $WIN_BUILDER bash -c "
            cd /src/core/shared
            rm -rf lib/Windows-Cross && mkdir -p lib/Windows-Cross
            make -j4 CROSS_COMPILE_WINDOWS=1
        "
    echo "✅ Shared libs 완료"
else
    echo "⏭️  [1/5] Shared Libraries 스킵"
fi

# =============================================================================
# [2] Collector + Drivers
# =============================================================================
COLLECTOR_EXE="$PROJECT_ROOT/core/collector/bin-windows/collector.exe"
if [ "$SKIP_COLLECTOR" = "false" ] && [ -f "$COLLECTOR_EXE" ]; then
    echo "⚡ [2/5] Collector: 이미 빌드됨 → 스킵"
    SKIP_COLLECTOR=true
fi

if [ "$SKIP_COLLECTOR" = "false" ]; then
    echo "🔨 [2/5] Collector + Drivers 빌드 중..."
    docker run --rm \
        -v "$PROJECT_ROOT/core":/src/core \
        $WIN_BUILDER bash -c "
            cd /src/core/collector
            rm -rf build-windows bin-windows/*.exe 2>/dev/null || true
            make -f Makefile.windows -j2
            x86_64-w64-mingw32-strip --strip-unneeded bin-windows/collector.exe
        "
    echo "✅ Collector 빌드 완료"
else
    echo "⏭️  [2/5] Collector 스킵"
fi

# bin-windows/로 복사
if [ -f "$COLLECTOR_EXE" ]; then
    cp "$COLLECTOR_EXE" "$BIN_DIR/pulseone-collector.exe"
    if [ -d "$PROJECT_ROOT/core/collector/bin-windows/drivers" ] && \
       ls "$PROJECT_ROOT/core/collector/bin-windows/drivers"/*.dll 1>/dev/null 2>&1; then
        cp "$PROJECT_ROOT/core/collector/bin-windows/drivers"/*.dll "$BIN_DIR/drivers/"
        echo "✅ Driver DLLs copied"
    fi
    # MinGW 런타임 DLLs
    docker run --rm \
        -v "$BIN_DIR":/output \
        $WIN_BUILDER bash -c "
            for dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
                f=\"/usr/x86_64-w64-mingw32/lib/\$dll\"
                [ -f \"\$f\" ] && cp \"\$f\" /output/
            done
        "
    echo "✅ Collector → $BIN_DIR/ ($(du -sh "$BIN_DIR/pulseone-collector.exe" | cut -f1))"
fi

# =============================================================================
# [3] Export Gateway
# =============================================================================
GATEWAY_EXE="$PROJECT_ROOT/core/export-gateway/bin-windows/export-gateway.exe"
if [ "$SKIP_GATEWAY" = "false" ] && [ -f "$GATEWAY_EXE" ]; then
    echo "⚡ [3/5] Gateway: 이미 빌드됨 → 스킵"
    SKIP_GATEWAY=true
fi

if [ "$SKIP_GATEWAY" = "false" ]; then
    echo "🔨 [3/5] Export Gateway 빌드 중..."
    docker run --rm \
        -v "$PROJECT_ROOT/core":/src/core \
        $WIN_BUILDER bash -c "
            cd /src/core/export-gateway
            rm -rf build-win bin-windows
            make -j4 CROSS_COMPILE_WINDOWS=1
            x86_64-w64-mingw32-strip --strip-unneeded bin-windows/export-gateway.exe
        "
    echo "✅ Gateway 빌드 완료"
else
    echo "⏭️  [3/5] Gateway 스킵"
fi

if [ -f "$GATEWAY_EXE" ]; then
    cp "$GATEWAY_EXE" "$BIN_DIR/pulseone-export-gateway.exe"
    echo "✅ Gateway → $BIN_DIR/ ($(du -sh "$BIN_DIR/pulseone-export-gateway.exe" | cut -f1))"
fi

# =============================================================================
# [4] Backend (pkg → .exe)
# =============================================================================
if [ "$SKIP_BACKEND" = "false" ] && [ -f "$BIN_DIR/pulseone-backend.exe" ]; then
    echo "⚡ [4/5] Backend: 이미 빌드됨 → 스킵"
    SKIP_BACKEND=true
fi

if [ "$SKIP_BACKEND" = "false" ]; then
    echo "📦 [4/5] Backend 빌드 중 (npx pkg)..."
    docker run --rm \
        -v "$PROJECT_ROOT/backend":/app/backend \
        -v "$BIN_DIR":/output \
        $WIN_BUILDER bash -c "
            cd /app/backend
            npm install --silent 2>/dev/null || true
            npx pkg . --targets node18-win-x64 --output /output/pulseone-backend.exe
        "
    echo "✅ Backend → $BIN_DIR/ ($(du -sh "$BIN_DIR/pulseone-backend.exe" 2>/dev/null | cut -f1 || echo 'N/A'))"
else
    echo "⏭️  [4/5] Backend 스킵"
fi

# sqlite3 네이티브 바인딩 (Windows x64 전용 다운로드 - 리눅스/ARM 환경 교차 빌드 버그 방지)
echo "   Downloading Windows x64 sqlite3.node..."
curl -sL https://github.com/TryGhost/node-sqlite3/releases/download/v5.1.7/sqlite3-v5.1.7-napi-v6-win32-x64.tar.gz | tar -xz -C "$BIN_DIR"
mv "$BIN_DIR/build/Release/node_sqlite3.node" "$BIN_DIR/node_sqlite3.node" 2>/dev/null || true
rm -rf "$BIN_DIR/build" 2>/dev/null || true

# =============================================================================
# [5] Frontend
# =============================================================================
if [ "$SKIP_FRONTEND" = "false" ] && [ -d "$PROJECT_ROOT/frontend/dist" ]; then
    echo "⚡ [5/5] Frontend: dist/ 이미 있음 → 복사만"
    mkdir -p "$BIN_DIR/frontend"
    cp -r "$PROJECT_ROOT/frontend/dist/." "$BIN_DIR/frontend/"
    echo "✅ Frontend → $BIN_DIR/frontend/"
elif [ "$SKIP_FRONTEND" = "false" ] && [ -d "$PROJECT_ROOT/frontend" ]; then
    echo "🎨 [5/5] Frontend 빌드 중..."
    cd "$PROJECT_ROOT/frontend"
    npm install --silent && npm run build
    mkdir -p "$BIN_DIR/frontend"
    cp -r dist/. "$BIN_DIR/frontend/"
    cd "$PROJECT_ROOT"
    echo "✅ Frontend → $BIN_DIR/frontend/"
else
    echo "⏭️  [5/5] Frontend 스킵"
fi

# Config & SQL
mkdir -p "$BIN_DIR/data/db" "$BIN_DIR/data/backup" "$BIN_DIR/data/logs" \
          "$BIN_DIR/data/temp" "$BIN_DIR/config" "$BIN_DIR/data/sql"
[ -d "$PROJECT_ROOT/config" ] && \
    rsync -a --exclude='secrets' "$PROJECT_ROOT/config/" "$BIN_DIR/config/" 2>/dev/null || true
cp "$PROJECT_ROOT/data/sql/schema.sql" "$BIN_DIR/data/sql/" 2>/dev/null || true
cp "$PROJECT_ROOT/data/sql/seed.sql"   "$BIN_DIR/data/sql/" 2>/dev/null || true

echo ""
echo "================================================================="
echo "✅ 빌드 완료: $BIN_DIR"
echo "   Collector: $(du -sh "$BIN_DIR/pulseone-collector.exe" 2>/dev/null | cut -f1 || echo 'N/A')"
echo "   Gateway:   $(du -sh "$BIN_DIR/pulseone-export-gateway.exe" 2>/dev/null | cut -f1 || echo 'N/A')"
echo "   Backend:   $(du -sh "$BIN_DIR/pulseone-backend.exe" 2>/dev/null | cut -f1 || echo 'N/A')"
echo "   Drivers:   $(ls "$BIN_DIR/drivers/"*.dll 2>/dev/null | wc -l | tr -d ' ') DLLs"
echo "================================================================="

# =============================================================================
# 패키징 (--no-package 없을 때만)
# =============================================================================
if [ "$NO_PACKAGE" = "false" ]; then
    PACKAGE_NAME="PulseOne_Windows-v${VERSION}_${TIMESTAMP}"
    DIST_DIR="$PROJECT_ROOT/dist_windows"
    PACKAGE_DIR="$DIST_DIR/$PACKAGE_NAME"
    SETUP_CACHE="$DIST_DIR/setup_assets_cache"
    mkdir -p "$DIST_DIR" "$SETUP_CACHE"

    echo ""
    echo "📦 패키징 중: $PACKAGE_DIR"
    cp -r "$BIN_DIR" "$PACKAGE_DIR"

    # ==========================================================================
    # setup_assets - 오프라인 설치용 파일 다운로드 (캐시 재사용)
    # ==========================================================================
    echo "📥 setup_assets 다운로드 중 (오프라인/에어갭 지원)..."
    cd "$SETUP_CACHE"

    NODE_MSI="node-v22.13.1-x64.msi"
    if [ ! -f "$NODE_MSI" ]; then
        echo "   Downloading Node.js..."
        curl -fsSL -o "$NODE_MSI" "https://nodejs.org/dist/v22.13.1/$NODE_MSI" || \
            echo "   ⚠️  Node.js 다운로드 실패"
    else
        echo "   ✅ Node.js (cached)"
    fi

    REDIS_ZIP="Redis-x64-5.0.14.1.zip"
    if [ ! -f "$REDIS_ZIP" ]; then
        echo "   Downloading Redis for Windows..."
        curl -fsSL -o "$REDIS_ZIP" \
            "https://github.com/tporadowski/redis/releases/download/v5.0.14.1/$REDIS_ZIP" || \
            echo "   ⚠️  Redis 다운로드 실패"
    else
        echo "   ✅ Redis (cached)"
    fi

    MOSQUITTO_EXE="mosquitto-2.0.21-install-windows-x64.exe"
    if [ ! -f "$MOSQUITTO_EXE" ]; then
        echo "   Downloading Mosquitto..."
        curl -fsSL -o "$MOSQUITTO_EXE" \
            "https://mosquitto.org/files/binary/win64/$MOSQUITTO_EXE" || \
            echo "   ⚠️  Mosquitto 다운로드 실패"
    else
        echo "   ✅ Mosquitto (cached)"
    fi

    WINSW_EXE="winsw.exe"
    if [ ! -f "$WINSW_EXE" ]; then
        echo "   Downloading WinSW..."
        curl -fsSL -o "$WINSW_EXE" \
            "https://github.com/winsw/winsw/releases/download/v2.11.0/WinSW.NET4.exe" || \
            echo "   ⚠️  WinSW 다운로드 실패"
    else
        echo "   ✅ WinSW (cached)"
    fi

    mkdir -p "$PACKAGE_DIR/setup_assets"
    cp "$SETUP_CACHE/"* "$PACKAGE_DIR/setup_assets/" 2>/dev/null || true
    cd "$PROJECT_ROOT"
    echo "✅ setup_assets ready"

    # ==========================================================================
    # install.bat
    # ==========================================================================
    cat > "$PACKAGE_DIR/install.bat" << 'WIN_INSTALL'
@echo off
setlocal enabledelayedexpansion
title PulseOne 자동 설치
echo ==========================================================
echo  PulseOne Industrial IoT Platform - 자동 설치 시작
echo ==========================================================
pushd "%~dp0"
set "ROOT=%CD%"

net session >nul 2>&1
if errorlevel 1 (
    echo 관리자 권한으로 다시 실행합니다...
    powershell -Command "Start-Process cmd -ArgumentList '/c cd /d %ROOT% && install.bat' -Verb RunAs -Wait"
    exit /b 0
)

:: [1/6] MSVC Redistributable
echo [1/6] MSVC Redistributable 설치 중...
if exist "setup_assets\vc_redist.x64.exe" (
    start /wait "" "setup_assets\vc_redist.x64.exe" /install /quiet /norestart
    echo    MSVC Redistributable 설치 완료
) else (
    echo    MSVC Redistributable 설치 파일 없음, 건너뜁니다.
)

:: [2/6] Node.js
echo [2/6] Node.js 확인 중...
where node >nul 2>&1
if errorlevel 1 (
    echo    Node.js 설치 중...
    start /wait msiexec /i "setup_assets\node-v22.13.1-x64.msi" /quiet /qn /norestart ADDLOCAL=ALL
    set "PATH=!PATH!;C:\Program Files\nodejs"
    echo    Node.js 설치 완료
) else (
    for /f "tokens=*" %%v in ('node --version') do echo    Node.js %%v 확인
)

:: [3/6] Redis
echo [3/6] Redis 설정 중...
if not exist "redis\redis-server.exe" (
    mkdir redis 2>nul
    tar -xf "setup_assets\Redis-x64-5.0.14.1.zip" -C redis --strip-components=1 >nul 2>&1
)
sc query PulseOne-Redis >nul 2>&1
if errorlevel 1 (
    sc create PulseOne-Redis binPath= "\"%ROOT%\redis\redis-server.exe\" --service-run" start= auto
    sc description PulseOne-Redis "PulseOne Redis Cache"
)
sc start PulseOne-Redis >nul 2>&1
echo    Redis 서비스 등록 완료

:: [4/6] Mosquitto
echo [4/6] Mosquitto 설정 중...
if not exist "mosquitto\mosquitto.exe" (
    start /wait "setup_assets\mosquitto-2.0.21-install-windows-x64.exe" /S /D="%ROOT%\mosquitto"
)
sc query PulseOne-MQTT >nul 2>&1
if errorlevel 1 (
    sc create PulseOne-MQTT binPath= "\"%ROOT%\mosquitto\mosquitto.exe\" -c \"%ROOT%\mosquitto\mosquitto.conf\"" start= auto
    sc description PulseOne-MQTT "PulseOne MQTT Broker"
)
sc start PulseOne-MQTT >nul 2>&1
echo    Mosquitto 서비스 등록 완료

:: [5/6] WinSW 서비스 등록
echo [5/6] PulseOne 서비스 등록 중...
if exist "setup_assets\winsw.exe" (
    copy /y "setup_assets\winsw.exe" "%ROOT%\winsw.exe" >nul
)

if exist "winsw.exe" (
    echo ^<service^>^<id^>PulseOne-Backend^</id^>^<name^>PulseOne Backend^</name^>^<executable^>%ROOT%\pulseone-backend.exe^</executable^>^<workingdirectory^>%ROOT%^</workingdirectory^>^<env name="NODE_ENV" value="production"/^>^<log mode="roll"^>^<sizeThreshold^>10240^</sizeThreshold^>^</log^>^</service^> > pulseone-backend.xml
    winsw install pulseone-backend.xml 2>nul
    winsw start  pulseone-backend.xml 2>nul

    echo ^<service^>^<id^>PulseOne-Collector^</id^>^<name^>PulseOne Collector^</name^>^<executable^>%ROOT%\pulseone-collector.exe^</executable^>^<workingdirectory^>%ROOT%^</workingdirectory^>^<log mode="roll"/^>^</service^> > pulseone-collector.xml
    winsw install pulseone-collector.xml 2>nul
    winsw start  pulseone-collector.xml 2>nul

    echo ^<service^>^<id^>PulseOne-Gateway^</id^>^<name^>PulseOne Export Gateway^</name^>^<executable^>%ROOT%\pulseone-export-gateway.exe^</executable^>^<workingdirectory^>%ROOT%^</workingdirectory^>^<log mode="roll"/^>^</service^> > pulseone-gateway.xml
    winsw install pulseone-gateway.xml 2>nul
    winsw start  pulseone-gateway.xml 2>nul

    echo    WinSW 서비스 등록 완료
) else (
    echo    WinSW 없음 - start.bat으로 수동 실행
    call start.bat
)

:: [6/6] 완료
echo.
echo ==========================================================
echo  PulseOne 설치 완료!
echo  Web UI: http://localhost:3000
echo ==========================================================
popd
WIN_INSTALL

    # ==========================================================================
    # start.bat
    # ==========================================================================
    cat > "$PACKAGE_DIR/start.bat" << 'WIN_START'
@echo off
setlocal
pushd "%~dp0"
set "ROOT=%CD%"
echo Starting PulseOne...
if exist "redis\redis-server.exe" (
    start "Redis" /min redis\redis-server.exe
    timeout /t 2 /nobreak >nul
)
if exist "mosquitto\mosquitto.exe" (
    start "Mosquitto" /min mosquitto\mosquitto.exe -c mosquitto\mosquitto.conf
    timeout /t 2 /nobreak >nul
)
start "PulseOne Backend"   /min cmd /c "cd /d "%ROOT%" && pulseone-backend.exe"
timeout /t 3 /nobreak >nul
start "PulseOne Collector" /min cmd /c "cd /d "%ROOT%" && pulseone-collector.exe"
timeout /t 2 /nobreak >nul
start "PulseOne Gateway"   /min cmd /c "cd /d "%ROOT%" && pulseone-export-gateway.exe"
echo PulseOne started! Web UI: http://localhost:3000
popd
WIN_START

    # ==========================================================================
    # stop.bat
    # ==========================================================================
    cat > "$PACKAGE_DIR/stop.bat" << 'WIN_STOP'
@echo off
echo Stopping PulseOne...
taskkill /f /im "pulseone-backend.exe"        2>nul
taskkill /f /im "pulseone-collector.exe"      2>nul
taskkill /f /im "pulseone-export-gateway.exe" 2>nul
taskkill /f /im "redis-server.exe"            2>nul
taskkill /f /im "mosquitto.exe"               2>nul
echo All services stopped.
WIN_STOP

    # ==========================================================================
    # uninstall.bat
    # ==========================================================================
    cat > "$PACKAGE_DIR/uninstall.bat" << 'WIN_UNINSTALL'
@echo off
pushd "%~dp0"
set "ROOT=%CD%"
echo ==========================================================
echo  ⚠️  PulseOne 제거 경고
echo ==========================================================
echo.
echo  이 작업을 실행하면 PulseOne의 모든 서비스가
echo  중지되고 시스템에서 제거됩니다.
echo.
echo  - Backend, Collector, Gateway 서비스 삭제
echo  - Redis, Mosquitto 서비스 삭제
echo  - 데이터는 data\ 폴더에 유지됩니다
echo.
set /p CONFIRM="계속하려면 Y를 입력하세요 (Y/N): "
if /i not "%CONFIRM%"=="Y" (
    echo 취소되었습니다.
    popd
    exit /b 0
)

net session >nul 2>&1
if errorlevel 1 (
    powershell -Command "Start-Process cmd -ArgumentList '/c cd /d %ROOT% && uninstall.bat' -Verb RunAs -Wait"
    exit /b 0
)

echo PulseOne 제거 중...
for %%s in (pulseone-backend pulseone-collector pulseone-gateway) do (
    if exist "%%s.xml" (
        winsw stop    %%s.xml 2>nul
        winsw uninstall %%s.xml 2>nul
    )
    sc stop   "PulseOne-%%s" 2>nul
    sc delete "PulseOne-%%s" 2>nul
)
sc stop   PulseOne-Redis 2>nul
sc delete PulseOne-Redis  2>nul
sc stop   PulseOne-MQTT  2>nul
sc delete PulseOne-MQTT   2>nul

echo.
echo PulseOne 제거 완료. 이 폴더를 삭제하면 완전히 제거됩니다.
popd
WIN_UNINSTALL

    echo "✅ Windows scripts created (install/start/stop/uninstall)"

    # ==========================================================================
    # ZIP
    # ==========================================================================
    echo "📦 ZIP 패키징 중..."
    cd "$DIST_DIR"
    zip -r "${PACKAGE_NAME}.zip" "$PACKAGE_NAME/" > /dev/null
    echo "✅ Windows ZIP: $DIST_DIR/${PACKAGE_NAME}.zip ($(du -sh "${PACKAGE_NAME}.zip" | cut -f1))"
fi