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
TIMESTAMP=${HOST_TIMESTAMP:-$(date '+%Y%m%d_%H%M%S')}

# 중앙 빌드 결과물 폴더
BIN_DIR="$PROJECT_ROOT/bin-windows"

SKIP_SHARED=false
SKIP_COLLECTOR=false
SKIP_GATEWAY=false
SKIP_BACKEND=false
SKIP_FRONTEND=false
NO_PACKAGE=false
DB_BUNDLE="${DB_BUNDLE:-sqlite}"

for arg in "$@"; do
    case "$arg" in
        --skip-shared)     SKIP_SHARED=true ;;
        --skip-collector)  SKIP_COLLECTOR=true ;;
        --skip-gateway)    SKIP_GATEWAY=true ;;
        --skip-backend)    SKIP_BACKEND=true ;;
        --skip-frontend)   SKIP_FRONTEND=true ;;
        --skip-cpp)        SKIP_SHARED=true; SKIP_COLLECTOR=true; SKIP_GATEWAY=true ;;
        --no-package)      NO_PACKAGE=true ;;
        --db=sqlite)       DB_BUNDLE="sqlite" ;;
        --db=mariadb)      DB_BUNDLE="mariadb" ;;
        --db=postgresql)   DB_BUNDLE="postgresql" ;;
        --db=all)          DB_BUNDLE="all" ;;
        --db=mssql)        DB_BUNDLE="mssql" ;;
    esac
done

WIN_BUILDER="pulseone-windows-builder"

echo "================================================================="
echo "🪟 PulseOne Windows Deploy v$VERSION"
echo "   skip: shared=$SKIP_SHARED  collector=$SKIP_COLLECTOR  gateway=$SKIP_GATEWAY"
echo "         backend=$SKIP_BACKEND  frontend=$SKIP_FRONTEND"
echo "   DB bundle: $DB_BUNDLE"
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
    (
        cd "$PROJECT_ROOT/core/shared"
        rm -rf lib/Windows-Cross && mkdir -p lib/Windows-Cross
        make -j4 CROSS_COMPILE_WINDOWS=1
    )
    echo "✅ Shared libs 완료"
else
    echo "⏭️  [1/5] Shared Libraries 스킵"
fi

# =============================================================================
# [2] Collector + Drivers
# =============================================================================
COLLECTOR_EXE="$PROJECT_ROOT/core/collector/bin-windows/pulseone-collector.exe"
if [ "$SKIP_COLLECTOR" = "false" ] && [ -f "$COLLECTOR_EXE" ]; then
    echo "⚡ [2/5] Collector: 이미 빌드됨 → 스킵"
    SKIP_COLLECTOR=true
fi

if [ "$SKIP_COLLECTOR" = "false" ]; then
    echo "🔨 [2/5] Collector + Drivers 빌드 중..."
    (
        cd "$PROJECT_ROOT/core/collector"
        rm -rf build-windows bin-windows/*.exe 2>/dev/null || true
        make -f Makefile.windows -j2
        x86_64-w64-mingw32-strip --strip-unneeded bin-windows/pulseone-collector.exe
    )
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
    for dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
        f=$(find /usr -name "$dll" | head -n 1)
        [ -n "$f" ] && [ -f "$f" ] && cp "$f" "$BIN_DIR/"
    done
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
    (
        cd "$PROJECT_ROOT/core/export-gateway"
        rm -rf build-win bin-windows
        make -j4 CROSS_COMPILE_WINDOWS=1
        x86_64-w64-mingw32-strip --strip-unneeded bin/export-gateway.exe
        mkdir -p bin-windows
        cp bin/export-gateway.exe bin-windows/export-gateway.exe
    )
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
    (
        cd "$PROJECT_ROOT/backend"
        npm install --silent 2>/dev/null || true
        npx pkg . --targets node18-win-x64 --output "$BIN_DIR/pulseone-backend.exe"
    )
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

# Windows 전용 변수만 .env.production에 추가
# (redis, timeseries, messaging은 소스 파일에서 이미 올바른 값으로 수정됨)
WIN_ENV="$BIN_DIR/config/.env.production"
# 중복 키 제거: ALLOW_NO_AUTH, INFLUX_TOKEN은 소스 .env.production에 이미 존재
# Windows 전용 추가 설정이 필요한 경우만 여기에 추가
echo "✅ Windows config 준비 완료 (.env.production)"

# C++ Collector용 config/.env 생성
# (.gitignore에 .env가 포함되어 있어 빌드 환경에 없으므로 스크립트에서 직접 생성)
cat > "$BIN_DIR/config/.env" << 'EOF'
# PulseOne Collector 메인 설정 (deploy-windows.sh 자동 생성)
# C++ ConfigManager가 이 파일을 최초로 로드합니다.
NODE_ENV=production
LOG_LEVEL=INFO
DATA_DIR=./data
LOGS_DIR=./logs
AUTO_INITIALIZE_ON_START=true
SKIP_IF_INITIALIZED=true
# 추가 설정 파일 목록 - 아래 파일들을 순서대로 로드합니다
CONFIG_FILES=database.env,redis.env,timeseries.env,messaging.env,collector.env
EOF
echo "✅ C++ Collector용 config/.env 생성 완료 (CONFIG_FILES 포함)"


# 샘플 데이터가 들어간 SQLite DB 사전 생성 (첫 실행부터 데이터 표시)
echo "🗄️  사전 시드 SQLite DB 생성 중..."
SEED_DB="$BIN_DIR/data/db/pulseone.db"
rm -f "$SEED_DB"
sqlite3 "$SEED_DB" < "$PROJECT_ROOT/data/sql/schema.sql" && \
sqlite3 "$SEED_DB" < "$PROJECT_ROOT/data/sql/seed.sql" && \
echo "✅ 사전 시드 DB 생성 완료 (devices: $(sqlite3 "$SEED_DB" 'SELECT count(*) FROM devices;'), roles: $(sqlite3 "$SEED_DB" 'SELECT count(*) FROM roles;'))" || \
echo "⚠️  사전 시드 DB 생성 실패 - 첫 실행 시 자동 초기화됨"


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

    VCREDIST_EXE="vc_redist.x64.exe"
    if [ ! -f "$VCREDIST_EXE" ]; then
        echo "   Downloading MSVC Redistributable (vc_redist.x64.exe)..."
        curl -fsSL -o "$VCREDIST_EXE" \
            "https://aka.ms/vs/17/release/vc_redist.x64.exe" || \
            echo "   ⚠️  vc_redist.x64.exe 다운로드 실패"
    else
        echo "   ✅ vc_redist.x64.exe (cached)"
    fi

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

    INFLUX_ZIP="influxdb2-client-2.7.1-windows-amd64.zip"
    INFLUXD_ZIP="influxdb2-2.7.1-windows-amd64.zip"
    if [ ! -f "$INFLUXD_ZIP" ]; then
        echo "   Downloading InfluxDB 2.7 for Windows..."
        curl -fsSL -o "$INFLUXD_ZIP" \
            "https://download.influxdata.com/influxdb/releases/influxdb2-2.7.1-windows-amd64.zip" || \
            echo "   ⚠️  InfluxDB 다운로드 실패"
    else
        echo "   ✅ InfluxDB (cached)"
    fi

    ZLIB_DLL="zlib1.dll"
    if [ ! -f "$ZLIB_DLL" ]; then
        echo "   Downloading zlib1.dll..."
        curl -fsSL -o "msys2_zlib.tar.zst" "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-zlib-1.3.1-1-any.pkg.tar.zst" && \
        (apt-get update -qq && apt-get install -y -qq zstd tar || true) && \
        tar -I zstd -xf msys2_zlib.tar.zst mingw64/bin/zlib1.dll && mv mingw64/bin/zlib1.dll . && rm -rf mingw64 msys2_zlib.tar.zst || \
        echo "   ⚠️  zlib1.dll 다운로드 실패"
    else
        echo "   ✅ zlib1.dll (cached)"
    fi

    OPENS_DLL="libcrypto-3-x64.dll"
    if [ ! -f "$OPENS_DLL" ]; then
        echo "   Downloading OpenSSL 3 DLLs..."
        curl -fsSL -o "msys2_libcrypto.tar.zst" "https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-openssl-3.3.2-1-any.pkg.tar.zst" && \
        tar -I zstd -xf msys2_libcrypto.tar.zst mingw64/bin/libcrypto-3-x64.dll mingw64/bin/libssl-3-x64.dll && mv mingw64/bin/*.dll . && rm -rf mingw64 msys2_libcrypto.tar.zst || \
        echo "   ⚠️  OpenSSL DLL 다운로드 실패"
    else
        echo "   ✅ OpenSSL DLLs (cached)"
    fi

    mkdir -p "$PACKAGE_DIR/setup_assets"
    cp "$SETUP_CACHE/"* "$PACKAGE_DIR/setup_assets/" 2>/dev/null || true

    # ==========================================================================
    # DB 번들 다운로드 (MariaDB / PostgreSQL)
    # ==========================================================================
    MARIA_ZIP="mariadb-11.4.5-winx64.zip"
    PG_ZIP="postgresql-17.4-2-windows-x64-binaries.zip"

    if [ "$DB_BUNDLE" = "mariadb" ] || [ "$DB_BUNDLE" = "all" ]; then
        if [ ! -f "$SETUP_CACHE/$MARIA_ZIP" ]; then
            echo "   Downloading MariaDB 11.4 portable ZIP (~270MB)..."
            curl -fsSL -L -o "$SETUP_CACHE/$MARIA_ZIP" \
                "https://downloads.mariadb.org/rest-api/mariadb/11.4.5/mariadb-11.4.5-winx64.zip" || \
            curl -fsSL -L -o "$SETUP_CACHE/$MARIA_ZIP" \
                "https://archive.mariadb.org/mariadb-11.4.5/winx64-packages/mariadb-11.4.5-winx64.zip" || \
                echo "   ⚠️  MariaDB 다운로드 실패"
        else
            echo "   ✅ MariaDB (cached)"
        fi
        [ -f "$SETUP_CACHE/$MARIA_ZIP" ] && cp "$SETUP_CACHE/$MARIA_ZIP" "$PACKAGE_DIR/setup_assets/"
    fi

    if [ "$DB_BUNDLE" = "postgresql" ] || [ "$DB_BUNDLE" = "all" ]; then
        if [ ! -f "$SETUP_CACHE/$PG_ZIP" ]; then
            echo "   Downloading PostgreSQL 17 portable ZIP (~300MB)..."
            curl -fsSL -L -o "$SETUP_CACHE/$PG_ZIP" \
                "https://get.enterprisedb.com/postgresql/postgresql-17.4-2-windows-x64-binaries.zip" || \
                echo "   ⚠️  PostgreSQL 다운로드 실패"
        else
            echo "   ✅ PostgreSQL (cached)"
        fi
        [ -f "$SETUP_CACHE/$PG_ZIP" ] && cp "$SETUP_CACHE/$PG_ZIP" "$PACKAGE_DIR/setup_assets/"
    fi

    cd "$PROJECT_ROOT"
    echo "✅ setup_assets ready (DB bundle: $DB_BUNDLE)"

    # ==========================================================================
    # install.bat — DB_BUNDLE=$DB_BUNDLE 에 따라 DB 선택 메뉴 동적 삽입
    # ==========================================================================
    # 히어독을 분리하여 shell 변수($DB_BUNDLE)와 bat 변수(%%VAR%%)를 혼용
    # ==========================================================================

    # ① 공통 헤더 (관리자 권한 체크 + 디렉토리 생성)
    cat > "$PACKAGE_DIR/install.bat" << 'PART1'
@echo off
chcp 65001 >nul
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
    powershell -Command "Start-Process '%~f0' -Verb RunAs -Wait"
    exit /b 0
)

echo.
echo [시작] 설치 디렉토리: %ROOT%
echo.

:: 데이터 디렉토리 사전 생성
if not exist "data\db"       mkdir "data\db"
if not exist "data\logs"     mkdir "data\logs"
if not exist "data\backup"   mkdir "data\backup"
if not exist "data\temp"     mkdir "data\temp"
if not exist "data\influxdb" mkdir "data\influxdb"

PART1

    # ② DB 선택 메뉴 (번들에 포함된 DB만 표시)
    if [ "$DB_BUNDLE" = "sqlite" ]; then
        # SQLite 전용 패키지 — 메뉴 없이 바로 SQLite 설정
        cat >> "$PACKAGE_DIR/install.bat" << 'PART2'
:: ============================================================
:: 데이터베이스: SQLite (기본)
:: ============================================================
set "DB_TYPE=SQLITE"
echo [DB] SQLite 모드로 설치합니다.
PART2
    else
        # MariaDB / PostgreSQL / all 패키지 — 메뉴 표시
        # 메뉴 옵션을 DB_BUNDLE에 따라 동적으로 구성
        cat >> "$PACKAGE_DIR/install.bat" << 'PART2A'
:: ============================================================
:: [0/?] 데이터베이스 유형 선택
:: ============================================================
echo.
echo ============================================================
echo   PulseOne 데이터베이스 선택
echo.
echo   1. SQLite   (소규모, 기본값, 별도 서버 불필요)
PART2A

        if [ "$DB_BUNDLE" = "mariadb" ] || [ "$DB_BUNDLE" = "all" ]; then
            cat >> "$PACKAGE_DIR/install.bat" << 'PART2B'
echo   2. MariaDB  (중대규모, 자동 설치 포함)
PART2B
        fi
        if [ "$DB_BUNDLE" = "postgresql" ] || [ "$DB_BUNDLE" = "all" ]; then
            cat >> "$PACKAGE_DIR/install.bat" << 'PART2C'
echo   3. PostgreSQL (대규모, 자동 설치 포함)
PART2C
        fi
        if [ "$DB_BUNDLE" = "mssql" ] || [ "$DB_BUNDLE" = "all" ]; then
            cat >> "$PACKAGE_DIR/install.bat" << 'PART2MSSQL'
echo   4. MSSQL    (SQL Server - 기존 서버 연결, database.env만 설정)
PART2MSSQL
        fi

        # DB 선택 프롬프트 — shell에서 문자열 완성 후 한 줄에 기록
        DB_PROMPT='선택 [1=SQLite'
        if [ "$DB_BUNDLE" = "mariadb" ]; then
            DB_PROMPT="${DB_PROMPT}/2=MariaDB"
        elif [ "$DB_BUNDLE" = "postgresql" ]; then
            DB_PROMPT="${DB_PROMPT}/3=PostgreSQL"
        elif [ "$DB_BUNDLE" = "mssql" ]; then
            DB_PROMPT="${DB_PROMPT}/4=MSSQL"
        elif [ "$DB_BUNDLE" = "all" ]; then
            DB_PROMPT="${DB_PROMPT}/2=MariaDB/3=PostgreSQL/4=MSSQL"
        fi
        DB_PROMPT="${DB_PROMPT}, 기본값=1]: "

        cat >> "$PACKAGE_DIR/install.bat" << PART2D_EOF
echo.
echo ============================================================
set /p "DB_CHOICE=${DB_PROMPT}"
if "%DB_CHOICE%"=="" set DB_CHOICE=1
if "%DB_CHOICE%"=="1" set "DB_TYPE=SQLITE"
if "%DB_CHOICE%"=="2" set "DB_TYPE=MARIADB"
if "%DB_CHOICE%"=="3" set "DB_TYPE=POSTGRESQL"
if "%DB_CHOICE%"=="4" set "DB_TYPE=MSSQL"
if "%DB_TYPE%"=="" set "DB_TYPE=SQLITE"
echo [DB] 선택된 데이터베이스: %DB_TYPE%
echo.
PART2D_EOF
    fi

    # ③ 공통 설치 단계 (MSVC, Node.js, InfluxDB, Redis, Mosquitto)
    cat >> "$PACKAGE_DIR/install.bat" << 'PART3'

:: ============================================================
:: [1/8] MSVC Redistributable
:: ============================================================
echo [1/8] MSVC Redistributable 확인 중...
if exist "setup_assets\vc_redist.x64.exe" (
    start /wait "" "setup_assets\vc_redist.x64.exe" /install /quiet /norestart
    echo    MSVC Redistributable 설치 완료
) else (
    echo    설치 파일 없음, 건너뜁니다.
)

:: ============================================================
:: [2/8] Node.js
:: ============================================================
echo [2/8] Node.js 확인 중...
where node >nul 2>&1
if errorlevel 1 (
    echo    Node.js 설치 중...
    start /wait msiexec /i "setup_assets\node-v22.13.1-x64.msi" /quiet /qn /norestart ADDLOCAL=ALL
    set "PATH=!PATH!;C:\Program Files\nodejs"
    echo    Node.js 설치 완료
) else (
    for /f "tokens=*" %%v in ('node --version') do echo    Node.js %%v 확인
)

:: ============================================================
:: [3/8] InfluxDB
:: ============================================================
echo [3/8] InfluxDB 설정 중...
if not exist "influxdb\influxd.exe" (
    mkdir influxdb 2>nul
    tar -xf "setup_assets\influxdb2-2.7.1-windows-amd64.zip" -C influxdb --strip-components=1 >nul 2>&1
)
if exist "influxdb\influxd.exe" (
    if not exist "data\influxdb\.influxdbv2" (
        echo    InfluxDB 초기 설정 중 ^(최초 1회^)...
        start /b /min "" "influxdb\influxd.exe" --bolt-path "%ROOT%\data\influxdb\.influxdbv2\influxd.bolt" --engine-path "%ROOT%\data\influxdb\.influxdbv2\engine"
        timeout /t 8 /nobreak >nul
        powershell -Command "'{""username"":""admin"",""password"":""admin123456"",""org"":""pulseone"",""bucket"":""telemetry_data"",""token"":""pulseone-influx-token-windows-2026"",""retentionPeriodSeconds"":0}' | Set-Content -Encoding UTF8 -Path '%TEMP%\influx_setup.json'"
        curl -s -X POST http://localhost:8086/api/v2/setup -H "Content-Type: application/json" -d "@%TEMP%\influx_setup.json" >nul 2>&1
        del "%TEMP%\influx_setup.json" >nul 2>&1
        taskkill /f /im influxd.exe >nul 2>&1
        timeout /t 2 /nobreak >nul
        echo    InfluxDB 초기 설정 완료
    )
    sc query PulseOne-InfluxDB >nul 2>&1
    if errorlevel 1 (
        powershell -Command "New-Service -Name 'PulseOne-InfluxDB' -BinaryPathName ('%ROOT%\influxdb\influxd.exe --bolt-path %ROOT%\data\influxdb\.influxdbv2\influxd.bolt --engine-path %ROOT%\data\influxdb\.influxdbv2\engine') -DisplayName 'PulseOne InfluxDB' -StartupType Automatic" >nul 2>&1
    )
    sc start PulseOne-InfluxDB >nul 2>&1
    echo    InfluxDB 서비스 등록 완료
) else (
    echo    [WARNING] InfluxDB 바이너리 없음 - 시계열 데이터 저장 불가
)

:: ============================================================
:: [4/8] Redis
:: ============================================================
echo [4/8] Redis 설정 중...
if not exist "redis\redis-server.exe" (
    mkdir redis 2>nul
    tar -xf "setup_assets\Redis-x64-5.0.14.1.zip" -C redis >nul 2>&1
)
sc query PulseOne-Redis >nul 2>&1
if errorlevel 1 (
    sc create PulseOne-Redis binPath= "\"%ROOT%\redis\redis-server.exe\" --service-run" start= auto DisplayName= "PulseOne Redis" >nul 2>&1
)
sc start PulseOne-Redis >nul 2>&1
echo    Redis 서비스 등록 완료

:: ============================================================
:: [5/8] Mosquitto
:: ============================================================
echo [5/8] Mosquitto 설정 중...
if not exist "mosquitto\mosquitto.exe" (
    mkdir "%ROOT%\mosquitto" 2>nul
    if exist "setup_assets\zlib1.dll"          copy /y "setup_assets\zlib1.dll"          "%ROOT%\mosquitto\" >nul
    if exist "setup_assets\libcrypto-3-x64.dll" copy /y "setup_assets\libcrypto-3-x64.dll" "%ROOT%\mosquitto\" >nul
    if exist "setup_assets\libssl-3-x64.dll"   copy /y "setup_assets\libssl-3-x64.dll"   "%ROOT%\mosquitto\" >nul
    start /wait "" "setup_assets\mosquitto-2.0.21-install-windows-x64.exe" /S /D=%ROOT%\mosquitto
)
if not exist "%ROOT%\mosquitto\mosquitto.conf" (
    echo listener 1883 0.0.0.0 > "%ROOT%\mosquitto\mosquitto.conf"
    echo allow_anonymous true >> "%ROOT%\mosquitto\mosquitto.conf"
    echo log_type all >> "%ROOT%\mosquitto\mosquitto.conf"
    echo    mosquitto.conf 기본 설정 생성 완료
)
sc query PulseOne-MQTT >nul 2>&1
if errorlevel 1 (
    powershell -Command "New-Service -Name 'PulseOne-MQTT' -BinaryPathName ('%ROOT%\mosquitto\mosquitto.exe -c %ROOT%\mosquitto\mosquitto.conf') -DisplayName 'PulseOne MQTT' -StartupType Automatic" >nul 2>&1
)
sc start PulseOne-MQTT >nul 2>&1
echo    Mosquitto 서비스 등록 완료

PART3

    # ④ DB별 추가 설치 (MariaDB / PostgreSQL)
    if [ "$DB_BUNDLE" = "sqlite" ]; then
        cat >> "$PACKAGE_DIR/install.bat" << 'PART4_SQLITE'
:: ============================================================
:: [6/8] 데이터베이스 초기화 (SQLite)
:: ============================================================
echo [6/8] SQLite 데이터베이스 설정 완료 (별도 서버 불필요)
:: database.env 생성
(
    echo DATABASE_TYPE=SQLITE
    echo SQLITE_PATH=./data/db/pulseone.db
    echo SQLITE_JOURNAL_MODE=WAL
    echo SQLITE_SYNCHRONOUS=NORMAL
    echo SQLITE_CACHE_SIZE=2000
    echo SQLITE_BUSY_TIMEOUT_MS=5000
    echo SQLITE_FOREIGN_KEYS=true
) > "config\database.env"
echo    database.env 생성 완료 ^(SQLite^)
PART4_SQLITE
    else
        cat >> "$PACKAGE_DIR/install.bat" << 'PART4_DB'
:: ============================================================
:: [6/8] 데이터베이스 설치 및 초기화
:: ============================================================
echo [6/8] 데이터베이스 설치 중 ^(선택: %DB_TYPE%^)...

if "%DB_TYPE%"=="SQLITE"     goto DB_SQLITE
if "%DB_TYPE%"=="MARIADB"    goto DB_MARIADB
if "%DB_TYPE%"=="POSTGRESQL" goto DB_POSTGRESQL
if "%DB_TYPE%"=="MSSQL"      goto DB_MSSQL
goto DB_SQLITE

:DB_SQLITE
(
    echo DATABASE_TYPE=SQLITE
    echo SQLITE_PATH=./data/db/pulseone.db
    echo SQLITE_JOURNAL_MODE=WAL
    echo SQLITE_SYNCHRONOUS=NORMAL
    echo SQLITE_CACHE_SIZE=2000
    echo SQLITE_BUSY_TIMEOUT_MS=5000
    echo SQLITE_FOREIGN_KEYS=true
    echo SQLITE_ENABLED=true
) > "config\database.env"
echo    SQLite database.env 생성 완료
goto DB_DONE

:DB_MARIADB
echo    MariaDB 압축 해제 중...
if not exist "mariadb\bin\mysqld.exe" (
    mkdir mariadb 2>nul
    tar -xf "setup_assets\mariadb-11.4.5-winx64.zip" -C mariadb --strip-components=1 >nul 2>&1
)
if exist "mariadb\bin\mysqld.exe" (
    if not exist "data\db\mariadb" (
        mkdir "data\db\mariadb" 2>nul
        echo    MariaDB 데이터 디렉토리 초기화 중...
        "mariadb\bin\mysqld.exe" --initialize-insecure --basedir="%ROOT%\mariadb" --datadir="%ROOT%\data\db\mariadb" >nul 2>&1
        echo    MariaDB 초기화 완료
    )
    sc query PulseOne-MariaDB >nul 2>&1
    if errorlevel 1 (
        "mariadb\bin\mysqld.exe" --install PulseOne-MariaDB --basedir="%ROOT%\mariadb" --datadir="%ROOT%\data\db\mariadb" >nul 2>&1
    )
    sc start PulseOne-MariaDB >nul 2>&1
    timeout /t 5 /nobreak >nul
    rem DB/계정 생성
    "mariadb\bin\mysql.exe" -u root -e "CREATE DATABASE IF NOT EXISTS pulseone CHARACTER SET utf8mb4; CREATE USER IF NOT EXISTS 'pulseone'@'localhost' IDENTIFIED BY ''; GRANT ALL ON pulseone.* TO 'pulseone'@'localhost'; FLUSH PRIVILEGES;" >nul 2>&1
    echo    MariaDB PulseOne 데이터베이스/계정 생성 완료
    (
        echo DATABASE_TYPE=MARIADB
        echo MARIADB_ENABLED=true
        echo MARIADB_HOST=localhost
        echo MARIADB_PORT=3306
        echo MARIADB_DATABASE=pulseone
        echo MARIADB_USER=pulseone
        echo MARIADB_PASSWORD=
        echo MARIADB_POOL_SIZE=10
        echo MARIADB_CHARSET=utf8mb4
        echo.
        echo # C++ Collector 호환 (MYSQL_ 접두사)
        echo MYSQL_HOST=localhost
        echo MYSQL_PORT=3306
        echo MYSQL_DATABASE=pulseone
        echo MYSQL_USER=pulseone
        echo MYSQL_PASSWORD=
    ) > "config\database.env"
    echo    MariaDB database.env 생성 완료
) else (
    echo    [WARNING] MariaDB 바이너리 없음 - SQLite로 대체
    goto DB_SQLITE
)
goto DB_DONE

:DB_POSTGRESQL
echo    PostgreSQL 압축 해제 중...
if not exist "postgresql\bin\postgres.exe" (
    mkdir postgresql 2>nul
    tar -xf "setup_assets\postgresql-17.4-2-windows-x64-binaries.zip" -C postgresql --strip-components=1 >nul 2>&1
)
if exist "postgresql\bin\postgres.exe" (
    if not exist "data\db\postgresql" (
        mkdir "data\db\postgresql" 2>nul
        echo    PostgreSQL 데이터 디렉토리 초기화 중...
        "postgresql\bin\initdb.exe" -D "%ROOT%\data\db\postgresql" -U postgres -E UTF8 >nul 2>&1
        echo    PostgreSQL 초기화 완료
    )
    sc query PulseOne-PostgreSQL >nul 2>&1
    if errorlevel 1 (
        sc create PulseOne-PostgreSQL binPath= "\"%ROOT%\postgresql\bin\pg_ctl.exe\" runservice -N \"PulseOne-PostgreSQL\" -D \"%ROOT%\data\db\postgresql\"" start= auto DisplayName= "PulseOne PostgreSQL" >nul 2>&1
    )
    sc start PulseOne-PostgreSQL >nul 2>&1
    timeout /t 5 /nobreak >nul
    rem DB/계정 생성
    set "PGPASSWORD="
    "postgresql\bin\psql.exe" -U postgres -c "CREATE DATABASE pulseone;" >nul 2>&1
    "postgresql\bin\psql.exe" -U postgres -c "CREATE USER pulseone WITH PASSWORD '';" >nul 2>&1
    "postgresql\bin\psql.exe" -U postgres -c "GRANT ALL PRIVILEGES ON DATABASE pulseone TO pulseone;" >nul 2>&1
    echo    PostgreSQL PulseOne 데이터베이스/계정 생성 완료
    (
        echo DATABASE_TYPE=POSTGRESQL
        echo POSTGRES_ENABLED=true
        echo POSTGRES_HOST=localhost
        echo POSTGRES_PORT=5432
        echo POSTGRES_DATABASE=pulseone
        echo POSTGRES_USER=pulseone
        echo POSTGRES_PASSWORD=
        echo POSTGRES_POOL_SIZE=10
        echo POSTGRES_SSL=false
    ) > "config\database.env"
    echo    PostgreSQL database.env 생성 완료
) else (
    echo    [WARNING] PostgreSQL 바이너리 없음 - SQLite로 대체
    goto DB_SQLITE
)
    goto DB_DONE

:DB_MSSQL
echo    MSSQL (SQL Server) - 기존 서버 연결 설정
echo.
echo    [!] MSSQL은 별도 설치되지 않습니다.
echo    [!] 이미 설치된 SQL Server 인스턴스에 연결 설정합니다.
echo.
set /p "MSSQL_H=SQL Server 호스트 [기본값: localhost]: "
if "!MSSQL_H!"=="" set "MSSQL_H=localhost"
set /p "MSSQL_PT=포트 [기본값: 1433]: "
if "!MSSQL_PT!"=="" set "MSSQL_PT=1433"
set /p "MSSQL_DB=데이터베이스명 [기본값: pulseone]: "
if "!MSSQL_DB!"=="" set "MSSQL_DB=pulseone"
set /p "MSSQL_U=사용자 [기본값: sa]: "
if "!MSSQL_U!"=="" set "MSSQL_U=sa"
set /p "MSSQL_PW=비밀번호: "
set /p "MSSQL_INST=인스턴스명 (없으면 Enter): "

(
    echo DATABASE_TYPE=MSSQL
    echo MSSQL_ENABLED=true
    echo MSSQL_HOST=!MSSQL_H!
    echo MSSQL_PORT=!MSSQL_PT!
    echo MSSQL_DATABASE=!MSSQL_DB!
    echo MSSQL_USER=!MSSQL_U!
    echo MSSQL_PASSWORD=!MSSQL_PW!
    echo MSSQL_INSTANCE=!MSSQL_INST!
    echo MSSQL_ENCRYPT=false
    echo MSSQL_TRUST_SERVER_CERTIFICATE=true
    echo MSSQL_POOL_SIZE=10
) > "config\database.env"
echo    MSSQL database.env 생성 완료

if "!MSSQL_INST!"=="" (
    sqlcmd -S !MSSQL_H! -U !MSSQL_U! -P !MSSQL_PW! -Q "IF NOT EXISTS (SELECT name FROM sys.databases WHERE name='!MSSQL_DB!') CREATE DATABASE [!MSSQL_DB!];" 2>nul
) else (
    sqlcmd -S !MSSQL_H!\!MSSQL_INST! -U !MSSQL_U! -P !MSSQL_PW! -Q "IF NOT EXISTS (SELECT name FROM sys.databases WHERE name='!MSSQL_DB!') CREATE DATABASE [!MSSQL_DB!];" 2>nul
)
if not errorlevel 1 echo    PulseOne 데이터베이스 생성 완료 (또는 이미 존재)
goto DB_DONE

:DB_DONE
echo    데이터베이스 설정 완료
PART4_DB
    fi

    # ⑤ WinSW 서비스 등록 (DATABASE_TYPE 변수화)
    cat >> "$PACKAGE_DIR/install.bat" << 'PART5'

:: ============================================================
:: [7/8] WinSW 서비스 등록 (Backend / Collector / Gateway)
:: ============================================================
echo [7/8] PulseOne 서비스 등록 중...
if exist "setup_assets\winsw.exe" (
    copy /y "setup_assets\winsw.exe" "%ROOT%\winsw.exe" >nul
)

if not exist "winsw.exe" (
    echo    WinSW 없음 - start.bat으로 수동 실행
    goto NO_WINSW
)

rem Backend XML
(
    echo ^<?xml version="1.0" encoding="UTF-8"?^>
    echo ^<service^>
    echo   ^<id^>PulseOne-Backend^</id^>
    echo   ^<name^>PulseOne Backend^</name^>
    echo   ^<description^>PulseOne Industrial IoT Backend^</description^>
    echo   ^<executable^>%ROOT%\pulseone-backend.exe^</executable^>
    echo   ^<workingdirectory^>%ROOT%^</workingdirectory^>
    echo   ^<env name="DATA_DIR" value="%ROOT%\data"/^>
    echo   ^<env name="COLLECTOR_LOG_DIR" value="%ROOT%\logs\packets"/^>
    echo   ^<startarguments^>--auto-init^</startarguments^>
    echo   ^<log mode="roll"^>^<sizeThreshold^>10240^</sizeThreshold^>^</log^>
    echo ^</service^>
) > pulseone-backend.xml

winsw stop     pulseone-backend.xml 2>nul
winsw uninstall pulseone-backend.xml 2>nul
winsw install  pulseone-backend.xml
winsw start    pulseone-backend.xml
if errorlevel 1 (
    echo    [WARNING] Backend 서비스 시작 실패. start-debug.bat으로 원인 확인.
) else (
    echo    Backend 서비스 등록 완료
)

rem Collector XML — DATABASE_TYPE을 선택한 DB로 주입
(
    echo ^<?xml version="1.0" encoding="UTF-8"?^>
    echo ^<service^>
    echo   ^<id^>PulseOne-Collector^</id^>
    echo   ^<name^>PulseOne Collector^</name^>
    echo   ^<description^>PulseOne Data Collection Service^</description^>
    echo   ^<executable^>%ROOT%\pulseone-collector.exe^</executable^>
    echo   ^<workingdirectory^>%ROOT%^</workingdirectory^>
    echo   ^<env name="DATA_DIR" value="%ROOT%\data"/^>
    echo   ^<env name="DATABASE_TYPE" value="%DB_TYPE%"/^>
    echo   ^<log mode="roll"/^>
    echo ^</service^>
) > pulseone-collector.xml

winsw stop     pulseone-collector.xml 2>nul
winsw uninstall pulseone-collector.xml 2>nul
winsw install  pulseone-collector.xml
winsw start    pulseone-collector.xml
echo    Collector 서비스 등록 완료

rem Gateway XML
(
    echo ^<?xml version="1.0" encoding="UTF-8"?^>
    echo ^<service^>
    echo   ^<id^>PulseOne-Gateway^</id^>
    echo   ^<name^>PulseOne Export Gateway^</name^>
    echo   ^<description^>PulseOne Data Export Gateway^</description^>
    echo   ^<executable^>%ROOT%\pulseone-export-gateway.exe^</executable^>
    echo   ^<workingdirectory^>%ROOT%^</workingdirectory^>
    echo   ^<env name="DATA_DIR" value="%ROOT%\data"/^>
    echo   ^<env name="DATABASE_TYPE" value="%DB_TYPE%"/^>
    echo   ^<arguments^>--config=%ROOT%\config^</arguments^>
    echo   ^<log mode="roll"/^>
    echo ^</service^>
) > pulseone-gateway.xml

winsw stop     pulseone-gateway.xml 2>nul
winsw uninstall pulseone-gateway.xml 2>nul
winsw install  pulseone-gateway.xml
winsw start    pulseone-gateway.xml
echo    Gateway 서비스 등록 완료

:NO_WINSW

:: ============================================================
:: [8/8] 바탕화면 바로가기
:: ============================================================
echo [8/8] 바탕화면 바로가기 생성 중...
echo [InternetShortcut] > "%USERPROFILE%\Desktop\PulseOne Web UI.url"
echo URL=http://localhost:3000 >> "%USERPROFILE%\Desktop\PulseOne Web UI.url"

echo.
echo ==========================================================
echo  PulseOne 설치가 모두 완료되었습니다!
echo  데이터베이스: %DB_TYPE%
echo  브라우저에서 http://localhost:3000 으로 접속하세요.
echo  (서비스 시작까지 10~30초 소요될 수 있습니다)
echo ==========================================================
echo.
echo 창을 닫으려면 아무 키나 누르세요...
pause >nul
popd
PART5


    # ==========================================================================
    # start.bat
    # ==========================================================================
    cat > "$PACKAGE_DIR/start.bat" << 'WIN_START'
@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
pushd "%~dp0"
set "ROOT=%CD%"

if not exist "runHidden.vbs" (
    echo Set objShell = WScript.CreateObject("WScript.Shell"^) > runHidden.vbs
    echo objShell.Run WScript.Arguments(0^), 0, False >> runHidden.vbs
)

echo Starting PulseOne...

if exist "influxdb\influxd.exe" (
    wscript.exe runHidden.vbs "cmd /c cd /d %ROOT% && influxdb\influxd.exe --bolt-path %ROOT%\data\influxdb\.influxdbv2\influxd.bolt --engine-path %ROOT%\data\influxdb\.influxdbv2\engine --http-bind-address 127.0.0.1:8086"
    timeout /t 4 /nobreak >nul
)

if exist "redis\redis-server.exe" (
    wscript.exe runHidden.vbs "cmd /c cd /d %ROOT% && redis\redis-server.exe --bind 127.0.0.1 --port 6379 --loglevel warning"
    timeout /t 2 /nobreak >nul
)

if exist "mosquitto\mosquitto.exe" (
    wscript.exe runHidden.vbs "cmd /c cd /d %ROOT%\mosquitto && mosquitto.exe -c %ROOT%\mosquitto\mosquitto.conf"
    timeout /t 2 /nobreak >nul
)

if not exist "%ROOT%\logs" mkdir "%ROOT%\logs"
if not exist "%ROOT%\logs\packets" mkdir "%ROOT%\logs\packets"

if not exist "pulseone-backend.exe" (
    echo [ERROR] pulseone-backend.exe not found!
    goto SKIP_BACKEND
)

wscript.exe runHidden.vbs "cmd /c cd /d %ROOT% && pulseone-backend.exe --config=%ROOT%\config >> %ROOT%\logs\backend-startup.log 2>&1"
echo [INFO] Backend 시작됨. DB 초기화 완료 대기 중 (최대 90초)...
echo [INFO] /api/health 의 db_initialized:true 응답을 기다립니다...

set WAIT_COUNT=0
:WAIT_DB_READY
timeout /t 3 /nobreak >nul
set /a WAIT_COUNT+=1

powershell -Command "try { $r = (Invoke-WebRequest -Uri 'http://localhost:3000/api/health' -TimeoutSec 3 -UseBasicParsing).Content | ConvertFrom-Json; if ($r.db_initialized -eq $true) { exit 0 } else { exit 2 } } catch { exit 1 }" >nul 2>&1
set HEALTH_CODE=%errorlevel%

if %HEALTH_CODE% EQU 0 (
    echo [OK] DB 초기화 완료 확인 ^(대기 !WAIT_COUNT!회 / 약 !WAIT_COUNT!x3초^)
    goto BACKEND_READY
)
if !WAIT_COUNT! GEQ 30 (
    echo [WARN] 90초 내 DB 초기화 미완료 - Collector 강제 시작 ^(logs\backend-startup.log 확인 필요^)
    goto BACKEND_READY
)
if %HEALTH_CODE% EQU 2 (
    echo [...] DB 초기화 진행 중... ^(!WAIT_COUNT!회^)
)
goto WAIT_DB_READY

:BACKEND_READY
:SKIP_BACKEND

if exist "pulseone-collector.exe" (
    wscript.exe runHidden.vbs "cmd /c cd /d %ROOT% && pulseone-collector.exe --config=%ROOT%\config >> %ROOT%\logs\collector-startup.log 2>&1"
    timeout /t 2 /nobreak >nul
    tasklist | findstr /i "pulseone-collector.exe" >nul 2>&1
    if errorlevel 1 (echo [WARN] Collector 시작 후 즉시 종료됨. logs\collector-startup.log 확인) else (echo [OK] Collector started)
) else (
    echo [INFO] pulseone-collector.exe 없음 - Collector 미포함 패키지
)

if exist "pulseone-export-gateway.exe" (
    wscript.exe runHidden.vbs "cmd /c cd /d %ROOT% && pulseone-export-gateway.exe --config=%ROOT%\config >> %ROOT%\logs\gateway-startup.log 2>&1"
    timeout /t 2 /nobreak >nul
    tasklist | findstr /i "pulseone-export-gateway.exe" >nul 2>&1
    if errorlevel 1 (echo [WARN] Export Gateway 시작 후 즉시 종료됨. logs\gateway-startup.log 확인) else (echo [OK] Export Gateway started)
) else (
    echo [INFO] pulseone-export-gateway.exe 없음 - Gateway 미포함 패키지
)

echo PulseOne started! Web UI: http://localhost:3000
popd
WIN_START

    # ==========================================================================
    # start-debug.bat (디버깅용)
    # ==========================================================================
    cat > "$PACKAGE_DIR/start-debug.bat" << 'WIN_DEBUG'
@echo off
chcp 65001 >nul
setlocal
pushd "%~dp0"
set "ROOT=%CD%"

echo ==========================================================
echo  PulseOne Debug Mode (Backend Troubleshooting)
echo ==========================================================

rem runHidden.vbs 없으면 생성
if not exist "runHidden.vbs" (
    echo Set objShell = WScript.CreateObject("WScript.Shell"^) > runHidden.vbs
    echo objShell.Run WScript.Arguments(0^), 0, False >> runHidden.vbs
)

echo Stopping any existing processes...
taskkill /F /IM redis-server.exe >nul 2>&1
taskkill /F /IM mosquitto.exe >nul 2>&1
taskkill /F /IM pulseone-collector.exe >nul 2>&1
taskkill /F /IM pulseone-export-gateway.exe >nul 2>&1
timeout /t 1 /nobreak >nul

echo Starting Redis on 127.0.0.1:6379...
if exist "redis\redis-server.exe" (
    wscript.exe runHidden.vbs "cmd /c cd /d %ROOT% && redis\redis-server.exe --bind 127.0.0.1 --port 6379 --loglevel warning"
    timeout /t 2 /nobreak >nul
    netstat -an | find "127.0.0.1:6379" >nul 2>&1
    if errorlevel 1 (
        echo [ERROR] Redis failed to start on 127.0.0.1:6379
    ) else (
        echo [OK] Redis listening on 127.0.0.1:6379
    )
) else (
    echo [WARN] redis\redis-server.exe not found. Run install.bat first.
)

echo Starting Mosquitto...
if exist "mosquitto\mosquitto.exe" (
    wscript.exe runHidden.vbs "cmd /c cd /d %ROOT%\mosquitto && mosquitto.exe -c %ROOT%\mosquitto\mosquitto.conf"
    timeout /t 1 /nobreak >nul
    echo [OK] Mosquitto started
)

echo Starting Collector in visible window (로그 확인 가능)...
if exist "pulseone-collector.exe" (
    start "PulseOne Collector" /min cmd /k "cd /d %ROOT% && pulseone-collector.exe"
    timeout /t 2 /nobreak >nul
    tasklist | findstr /i "pulseone-collector.exe" >nul 2>&1
    if errorlevel 1 (
        echo [WARN] Collector 시작 실패 또는 즉시 종료됨
    ) else (
        echo [OK] Collector running - 작업표시줄에서 창 확인 가능
    )
) else (
    echo [INFO] pulseone-collector.exe 없음 (정상 - Collector 미설치)
)

echo.
echo Starting Backend in foreground to capture errors...
echo If this window closes immediately, the backend crashed.
echo.
pulseone-backend.exe
echo.
echo Backend exit code: %errorlevel%
pause
popd
WIN_DEBUG

    # ==========================================================================
    # stop.bat
    # ==========================================================================
    cat > "$PACKAGE_DIR/stop.bat" << 'WIN_STOP'
@echo off
chcp 65001 >nul
echo Stopping PulseOne...

rem 1. WinSW 서비스 정지 (서비스로 등록된 경우)
if exist "pulseone-backend.xml"   winsw stop pulseone-backend.xml   2>nul
if exist "pulseone-collector.xml" winsw stop pulseone-collector.xml 2>nul
if exist "pulseone-gateway.xml"   winsw stop pulseone-gateway.xml   2>nul

rem 2. SC 서비스 정지 (New-Service로 등록된 경우)
sc stop PulseOne-Backend   2>nul
sc stop PulseOne-Collector 2>nul
sc stop PulseOne-Gateway   2>nul
sc stop PulseOne-InfluxDB  2>nul
sc stop PulseOne-Redis     2>nul
sc stop PulseOne-MQTT      2>nul

rem 3. 잔여 프로세스 강제 종료 (start.bat으로 실행된 경우)
taskkill /f /im "pulseone-backend.exe"        2>nul
taskkill /f /im "pulseone-collector.exe"      2>nul
taskkill /f /im "pulseone-export-gateway.exe" 2>nul
taskkill /f /im "influxd.exe"                 2>nul
taskkill /f /im "redis-server.exe"            2>nul
taskkill /f /im "mosquitto.exe"               2>nul

echo All services stopped.
WIN_STOP

    # ==========================================================================
    # reset.bat (데이터 초기화)
    # ==========================================================================
    cat > "$PACKAGE_DIR/reset.bat" << 'WIN_RESET'
@echo off
chcp 65001 >nul
pushd "%~dp0"
set "ROOT=%CD%"

echo ==========================================================
echo  ⚠️  PulseOne 데이터 초기화 경고
echo ==========================================================
echo.
echo  이 작업을 실행하면 다음이 삭제됩니다:
echo    - data\db\pulseone.db  (모든 설정 및 운영 데이터)
echo    - data\logs\           (로그 파일)
echo    - data\backup\         (자동 백업 파일)
echo.
echo  단, data\backup\에 수동 백업이 있다면 미리 복사해 두세요.
echo.
set /p CONFIRM="초기화하려면 Y를 입력하세요 (Y/N): "
if /i not "%CONFIRM%"=="Y" (
    echo 취소되었습니다.
    popd
    exit /b 0
)

echo.
echo 서비스 중지 중...
taskkill /f /im "pulseone-backend.exe"        2>nul
taskkill /f /im "pulseone-collector.exe"      2>nul
taskkill /f /im "pulseone-export-gateway.exe" 2>nul
taskkill /f /im "influxd.exe"                 2>nul
taskkill /f /im "redis-server.exe"            2>nul
taskkill /f /im "mosquitto.exe"               2>nul
timeout /t 2 /nobreak >nul

echo 기존 데이터베이스 삭제 중...
if exist "data\db\pulseone.db"             del /f /q "data\db\pulseone.db"
if exist "data\db\pulseone.db-wal"         del /f /q "data\db\pulseone.db-wal"
if exist "data\db\pulseone.db-shm"         del /f /q "data\db\pulseone.db-shm"

echo 로그 정리 중...
if exist "data\logs"  rd /s /q "data\logs"
mkdir "data\logs"

echo 기본 데이터베이스 복원 중...
if exist "data\db\pulseone_default.db" (
    copy /y "data\db\pulseone_default.db" "data\db\pulseone.db" >nul
    echo ✅ 기본 데이터베이스가 복원되었습니다.
) else (
    echo ⚠️  기본 DB 파일이 없습니다. 백엔드 재시작 시 자동 초기화됩니다.
)

echo.
echo ✅ 초기화 완료!
echo 이제 start.bat 또는 install.bat을 실행하세요.
echo.
pause
popd
WIN_RESET

    # ==========================================================================
    # uninstall.bat
    # ==========================================================================

    cat > "$PACKAGE_DIR/uninstall.bat" << 'WIN_UNINSTALL'
@echo off
chcp 65001 >nul
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
    powershell -Command "Start-Process '%~f0' -Verb RunAs -Wait"
    exit /b 0
)

echo PulseOne 제거 중...
for %%s in (Backend Collector Gateway) do (
    if exist "pulseone-%%s.xml" (
        winsw stop    pulseone-%%s.xml 2>nul
        winsw uninstall pulseone-%%s.xml 2>nul
    )
    sc stop   "PulseOne-%%s" 2>nul
    sc delete "PulseOne-%%s" 2>nul
)
sc stop   PulseOne-InfluxDB 2>nul
sc delete PulseOne-InfluxDB  2>nul
sc stop   PulseOne-Redis    2>nul
sc delete PulseOne-Redis     2>nul
sc stop   PulseOne-MQTT     2>nul
sc delete PulseOne-MQTT      2>nul
taskkill /f /im influxd.exe        2>nul
taskkill /f /im redis-server.exe   2>nul
taskkill /f /im mosquitto.exe      2>nul

echo.
echo PulseOne 제거 완료. 이 폴더를 삭제하면 완전히 제거됩니다.
echo (influxdb/, redis/, mosquitto/ 폴더도 함께 삭제하면 깨끗하게 정리됩니다.)
pause >nul
popd
WIN_UNINSTALL

    echo "✅ Windows scripts created (install/start/stop/uninstall)"

    # Convert line endings to CRLF for Windows CMD compatibility
    perl -pi -e 's/\r?\n/\r\n/g' "$PACKAGE_DIR/"*.bat

    # ==========================================================================
    # ZIP
    # ==========================================================================
    echo "📦 ZIP 패키징 중..."
    cd "$DIST_DIR"
    zip -r "${PACKAGE_NAME}.zip" "$PACKAGE_NAME/" > /dev/null
    echo "✅ Windows ZIP: $DIST_DIR/${PACKAGE_NAME}.zip ($(du -sh "${PACKAGE_NAME}.zip" | cut -f1))"
fi