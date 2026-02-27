#!/bin/bash
set -e

# =============================================================================
# PulseOne Linux Deploy Script
# 실행 환경: pulseone-linux-builder 컨테이너 내부 (release.sh가 마운트해서 실행)
#
# 사용법 (release.sh가 자동 호출):
#   ./release.sh --linux
#
# 직접 실행 (컨테이너 안에서):
#   docker run --rm -v $(pwd):/workspace -w /workspace pulseone-linux-builder bash deploy-linux.sh
#
# 옵션:
#   --skip-shared      shared 재빌드 스킵
#   --skip-collector   collector 재빌드 스킵
#   --skip-gateway     gateway 재빌드 스킵
#   --skip-backend     backend(pkg) 재빌드 스킵
#   --skip-frontend    frontend 복사 스킵
#   --skip-cpp         C++ 빌드 전체 스킵
#   --no-package       ZIP 패키징 없이 bin-linux/만 채움
# =============================================================================

PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$(dirname "$0")" && pwd)}"
VERSION=$(grep '"version"' "$PROJECT_ROOT/version.json" | cut -d'"' -f4 2>/dev/null || echo "6.1.0")
TIMESTAMP=${HOST_TIMESTAMP:-$(date '+%Y%m%d_%H%M%S')}

BIN_DIR="$PROJECT_ROOT/bin-linux"

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

# release.sh에서 환경변수로 스킵 전달 시 처리
[ "${SKIP_FRONTEND:-false}" = "true" ] && SKIP_FRONTEND=true

echo "================================================================="
echo "🐧 PulseOne Linux Deploy v$VERSION"
echo "   skip: shared=$SKIP_SHARED  collector=$SKIP_COLLECTOR  gateway=$SKIP_GATEWAY"
echo "         backend=$SKIP_BACKEND  frontend=$SKIP_FRONTEND"
echo "   output: $BIN_DIR"
echo "================================================================="

mkdir -p "$BIN_DIR/drivers" "$BIN_DIR/lib"

# =============================================================================
# [1] Shared Libraries — 컨테이너 안에서 직접 make
# =============================================================================
if [ "$SKIP_SHARED" = "false" ] && [ -f "$PROJECT_ROOT/core/shared/lib/libpulseone-common.a" ]; then
    echo "⚡ [1/5] Shared: 이미 빌드됨 → 스킵"
    SKIP_SHARED=true
fi

if [ "$SKIP_SHARED" = "false" ]; then
    echo "🔨 [1/5] Shared Libraries 빌드 중..."
    (
        cd "$PROJECT_ROOT/core/shared"
        make clean 2>/dev/null || true
        make -j$(nproc)
        
        # Deploy shared libraries to /usr/local/lib for plugin linkages
        cp lib/Linux/*.a /usr/local/lib/ 2>/dev/null || true
        cp lib/Linux/*.so* /usr/local/lib/ 2>/dev/null || true
        ldconfig
    )
    echo "✅ Shared libs 완료"
else
    echo "⏭️  [1/5] Shared Libraries 스킵"
fi

# =============================================================================
# [2] Collector + Drivers — 직접 make
# =============================================================================
# 목적지(bin-linux)에 이미 있으면 스킵 (소스 빌드 디렉토리가 아닌 목적지 기준)
if [ "$SKIP_COLLECTOR" = "false" ] && [ -f "$BIN_DIR/pulseone-collector" ]; then
    echo "⚡ [2/5] Collector: 이미 패키징됨 → 스킵"
    SKIP_COLLECTOR=true
fi

if [ "$SKIP_COLLECTOR" = "false" ]; then
    echo "🔨 [2/5] Collector + Drivers 빌드 중..."
    (
        cd "$PROJECT_ROOT/core/collector"
        make clean 2>/dev/null || true
        make all -j$(nproc)
        strip bin/pulseone-collector 2>/dev/null || true
    )
    echo "✅ Collector 빌드 완료"
else
    echo "⏭️  [2/5] Collector 스킵"
fi

COLLECTOR_BIN="$PROJECT_ROOT/core/collector/bin/pulseone-collector"
if [ -f "$COLLECTOR_BIN" ]; then
    cp "$COLLECTOR_BIN" "$BIN_DIR/"
    if [ -d "$PROJECT_ROOT/core/collector/bin/drivers" ] && \
       ls "$PROJECT_ROOT/core/collector/bin/drivers"/*.so 1>/dev/null 2>&1; then
        cp "$PROJECT_ROOT/core/collector/bin/drivers"/*.so "$BIN_DIR/drivers/"
        echo "✅ Driver .so copied"
    fi
    # 런타임 공유 라이브러리 복사
    for lib in libhiredis libpaho-mqttpp3 libpaho-mqtt3a libpaho-mqtt3c libpaho-mqtt3as libmodbus libopen62541; do
        find /usr/local/lib -name "${lib}.so*" -exec cp -n {} "$BIN_DIR/lib/" \; 2>/dev/null || true
    done
    echo "✅ Collector → $BIN_DIR/ ($(du -sh "$BIN_DIR/pulseone-collector" | cut -f1))"
fi

# =============================================================================
# [3] Export Gateway — 직접 make
# =============================================================================
# 목적지(bin-linux)에 이미 있으면 스킵
if [ "$SKIP_GATEWAY" = "false" ] && [ -f "$BIN_DIR/pulseone-export-gateway" ]; then
    echo "⚡ [3/5] Gateway: 이미 패키징됨 → 스킵"
    SKIP_GATEWAY=true
fi

if [ "$SKIP_GATEWAY" = "false" ]; then
    echo "🔨 [3/5] Export Gateway 빌드 중..."
    (
        cd "$PROJECT_ROOT/core/export-gateway"
        make clean 2>/dev/null || true
        make all -j$(nproc)
        strip bin/export-gateway 2>/dev/null || true
    )
    echo "✅ Gateway 빌드 완료"
else
    echo "⏭️  [3/5] Gateway 스킵"
fi

GATEWAY_BIN="$PROJECT_ROOT/core/export-gateway/bin/export-gateway"
if [ -f "$GATEWAY_BIN" ]; then
    cp "$GATEWAY_BIN" "$BIN_DIR/pulseone-export-gateway"
    echo "✅ Gateway → $BIN_DIR/ ($(du -sh "$BIN_DIR/pulseone-export-gateway" | cut -f1))"
fi

# =============================================================================
# [4] Backend (npx pkg → Linux x64 단일 실행파일)
# =============================================================================
if [ "$SKIP_BACKEND" = "false" ] && [ -f "$BIN_DIR/pulseone-backend" ]; then
    echo "⚡ [4/5] Backend: 이미 패키징됨 → 스킵"
    SKIP_BACKEND=true
fi

if [ "$SKIP_BACKEND" = "false" ]; then
    echo "📦 [4/5] Backend 빌드 중 (npx pkg)..."
    (
        cd "$PROJECT_ROOT/backend"
        npm install --silent 2>/dev/null || true
        npx pkg . --targets node18-linux-x64 --output "$BIN_DIR/pulseone-backend"
    )
    chmod +x "$BIN_DIR/pulseone-backend"
    echo "✅ Backend → $BIN_DIR/ ($(du -sh "$BIN_DIR/pulseone-backend" 2>/dev/null | cut -f1 || echo 'N/A'))"
else
    echo "⏭️  [4/5] Backend 스킵"
fi

# sqlite3 네이티브 바인딩 다운로드
echo "   Downloading Linux x64 sqlite3.node..."
curl -sL https://github.com/TryGhost/node-sqlite3/releases/download/v5.1.7/sqlite3-v5.1.7-napi-v6-linux-x64.tar.gz | \
    tar -xz -C "$BIN_DIR" 2>/dev/null || true
mv "$BIN_DIR/build/Release/node_sqlite3.node" "$BIN_DIR/node_sqlite3.node" 2>/dev/null || true
rm -rf "$BIN_DIR/build" 2>/dev/null || true

# =============================================================================
# [5] Frontend (이미 release.sh가 빌드해둔 dist/ 복사)
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

# =============================================================================
# Config & SQL 복사
# =============================================================================
mkdir -p "$BIN_DIR/data/db" "$BIN_DIR/data/logs" "$BIN_DIR/data/backup" \
         "$BIN_DIR/data/temp" "$BIN_DIR/data/influxdb" \
         "$BIN_DIR/config" "$BIN_DIR/data/sql"

[ -d "$PROJECT_ROOT/config" ] && \
    rsync -a --exclude='secrets' "$PROJECT_ROOT/config/" "$BIN_DIR/config/" 2>/dev/null || true
cp "$PROJECT_ROOT/data/sql/schema.sql" "$BIN_DIR/data/sql/" 2>/dev/null || true
cp "$PROJECT_ROOT/data/sql/seed.sql"   "$BIN_DIR/data/sql/" 2>/dev/null || true

# Linux Bare-Metal 환경 변수 오버라이드 주입
cat >> "$BIN_DIR/config/.env.production" << 'LINENV'

# =============================================================================
# Linux Bare-Metal 배포 전용 오버라이드 (deploy-linux.sh가 자동 주입)
# =============================================================================
SQLITE_PATH=./data/db/pulseone.db
INFLUX_TOKEN=pulseone-influx-token-linux-2026
LINENV
echo "✅ Linux 환경 변수 주입 완료 (.env.production)"

# C++ Collector용 config/.env
cat > "$BIN_DIR/config/.env" << 'EOF'
# PulseOne Collector 메인 설정 (deploy-linux.sh 자동 생성)
NODE_ENV=production
LOG_LEVEL=INFO
DATA_DIR=./data
LOGS_DIR=./logs
AUTO_INITIALIZE_ON_START=true
SKIP_IF_INITIALIZED=true
CONFIG_FILES=database.env,redis.env,timeseries.env,messaging.env,collector.env
EOF
echo "✅ C++ Collector용 config/.env 생성 완료"

# 사전 시드 SQLite DB 생성 (컨테이너 안에 sqlite3 있음)
echo "🗄️  사전 시드 SQLite DB 생성 중..."
SEED_DB="$BIN_DIR/data/db/pulseone.db"
DEFAULT_DB="$BIN_DIR/data/db/pulseone_default.db"
rm -f "$SEED_DB" "$DEFAULT_DB"
if command -v sqlite3 >/dev/null 2>&1; then
    sqlite3 "$SEED_DB" < "$PROJECT_ROOT/data/sql/schema.sql" && \
    sqlite3 "$SEED_DB" < "$PROJECT_ROOT/data/sql/seed.sql" && \
    cp "$SEED_DB" "$DEFAULT_DB" && \
    echo "✅ 시드 DB 생성 완료 (devices: $(sqlite3 "$SEED_DB" 'SELECT count(*) FROM devices;'), roles: $(sqlite3 "$SEED_DB" 'SELECT count(*) FROM roles;'))" || \
    echo "⚠️  시드 DB 생성 실패 - 첫 실행 시 자동 초기화됨"
else
    echo "⚠️  sqlite3 없음 - 첫 실행 시 자동 초기화됨"
fi

echo ""
echo "================================================================="
echo "✅ 빌드 완료: $BIN_DIR"
echo "   Collector: $(du -sh "$BIN_DIR/pulseone-collector" 2>/dev/null | cut -f1 || echo 'N/A')"
echo "   Gateway:   $(du -sh "$BIN_DIR/pulseone-export-gateway" 2>/dev/null | cut -f1 || echo 'N/A')"
echo "   Backend:   $(du -sh "$BIN_DIR/pulseone-backend" 2>/dev/null | cut -f1 || echo 'N/A')"
echo "   Drivers:   $(ls "$BIN_DIR/drivers/"*.so 2>/dev/null | wc -l | tr -d ' ') .so"
echo "================================================================="

# =============================================================================
# 패키징 (--no-package 없을 때만)
# =============================================================================
if [ "$NO_PACKAGE" = "false" ]; then
    PACKAGE_NAME="PulseOne_Linux-v${VERSION}_${TIMESTAMP}"
    DIST_DIR="$PROJECT_ROOT/dist_linux"
    PACKAGE_DIR="$DIST_DIR/$PACKAGE_NAME"
    SETUP_CACHE="$DIST_DIR/setup_assets_cache"
    mkdir -p "$DIST_DIR" "$SETUP_CACHE"

    echo ""
    echo "📦 패키징 중: $PACKAGE_DIR"
    cp -r "$BIN_DIR" "$PACKAGE_DIR"

    # setup_assets 다운로드 (캐시 재사용)
    echo "📥 setup_assets 다운로드 중 (오프라인/에어갭 지원)..."
    cd "$SETUP_CACHE"

    NODE_TGZ="node-v22.13.1-linux-x64.tar.xz"
    if [ ! -f "$NODE_TGZ" ]; then
        echo "   Downloading Node.js..."
        curl -fsSL -o "$NODE_TGZ" "https://nodejs.org/dist/v22.13.1/$NODE_TGZ" || \
            echo "   ⚠️  Node.js 다운로드 실패"
    else
        echo "   ✅ Node.js (cached)"
    fi

    # Redis .deb (컨테이너 안에서는 apt-get download 가능)
    REDIS_DEB_CACHE="$SETUP_CACHE/redis-server.deb"
    if [ ! -f "$REDIS_DEB_CACHE" ]; then
        echo "   Downloading Redis .deb..."
        apt-get update -qq 2>/dev/null && apt-get download redis-server 2>/dev/null && \
            mv redis-server*.deb "$REDIS_DEB_CACHE" || \
            echo "   ⚠️  Redis deb 다운로드 실패"
    else
        echo "   ✅ Redis .deb (cached)"
    fi

    # Mosquitto .deb
    MOSQUITTO_DEB_CACHE="$SETUP_CACHE/mosquitto.deb"
    if [ ! -f "$MOSQUITTO_DEB_CACHE" ]; then
        echo "   Downloading Mosquitto .deb..."
        apt-get update -qq 2>/dev/null && apt-get download mosquitto 2>/dev/null && \
            mv mosquitto_*.deb "$MOSQUITTO_DEB_CACHE" || \
            echo "   ⚠️  Mosquitto deb 다운로드 실패"
    else
        echo "   ✅ Mosquitto .deb (cached)"
    fi

    # InfluxDB
    INFLUXD_TGZ="influxdb2-2.7.1-linux-amd64.tar.gz"
    if [ ! -f "$INFLUXD_TGZ" ]; then
        echo "   Downloading InfluxDB 2.7..."
        curl -fsSL -o "$INFLUXD_TGZ" \
            "https://download.influxdata.com/influxdb/releases/influxdb2-2.7.1-linux-amd64.tar.gz" || \
            echo "   ⚠️  InfluxDB 다운로드 실패"
    else
        echo "   ✅ InfluxDB (cached)"
    fi

    mkdir -p "$PACKAGE_DIR/setup_assets"
    cp "$SETUP_CACHE/"* "$PACKAGE_DIR/setup_assets/" 2>/dev/null || true
    cd "$PROJECT_ROOT"
    echo "✅ setup_assets ready"

    # ==========================================================================
    # install.sh
    # ==========================================================================
    cat > "$PACKAGE_DIR/install.sh" << 'INSTALL_EOF'
#!/bin/bash
set -e
if [ "$EUID" -ne 0 ]; then
    exec sudo bash "$0" "$@"
fi

INSTALL_DIR=$(cd "$(dirname "$0")" && pwd)
export DEBIAN_FRONTEND=noninteractive

echo "=========================================="
echo " PulseOne 자동 설치 시작 (Ubuntu/Debian)"
echo "=========================================="

mkdir -p "$INSTALL_DIR/data/db" "$INSTALL_DIR/data/logs" \
         "$INSTALL_DIR/data/backup" "$INSTALL_DIR/data/temp" \
         "$INSTALL_DIR/data/influxdb" \
         "$INSTALL_DIR/logs/packets"

# [1/5] Redis
echo "[1/5] Redis 설치 중..."
if ! command -v redis-server >/dev/null 2>&1; then
    if ls "$INSTALL_DIR/setup_assets/"redis*.deb 1>/dev/null 2>&1; then
        dpkg -i "$INSTALL_DIR/setup_assets/"redis*.deb >/dev/null 2>&1 || apt-get install -f -y -q >/dev/null
    else
        apt-get install -y -q redis-server >/dev/null
    fi
fi
systemctl enable redis-server >/dev/null 2>&1 || true
systemctl start  redis-server 2>/dev/null || systemctl start redis 2>/dev/null || true
echo "   ✅ Redis 실행 중"

# [2/5] Mosquitto
echo "[2/5] Mosquitto 설치 중..."
if ! command -v mosquitto >/dev/null 2>&1; then
    if ls "$INSTALL_DIR/setup_assets/"mosquitto*.deb 1>/dev/null 2>&1; then
        dpkg -i "$INSTALL_DIR/setup_assets/"mosquitto*.deb >/dev/null 2>&1 || apt-get install -f -y -q >/dev/null
    else
        apt-get install -y -q mosquitto mosquitto-clients >/dev/null
    fi
fi
systemctl enable mosquitto >/dev/null 2>&1 || true
if [ ! -f "/etc/mosquitto/mosquitto.conf" ]; then
    mkdir -p /etc/mosquitto
    cat > /etc/mosquitto/mosquitto.conf << MQTTCONF
listener 1883 0.0.0.0
allow_anonymous true
log_type all
MQTTCONF
fi
systemctl start mosquitto >/dev/null 2>&1 || true
echo "   ✅ Mosquitto 실행 중"

# [3/5] InfluxDB
echo "[3/5] InfluxDB 설치 중..."
if ! command -v influxd >/dev/null 2>&1 && [ ! -f "$INSTALL_DIR/influxdb/influxd" ]; then
    mkdir -p "$INSTALL_DIR/influxdb"
    if [ -f "$INSTALL_DIR/setup_assets/influxdb2-2.7.1-linux-amd64.tar.gz" ]; then
        tar -xzf "$INSTALL_DIR/setup_assets/influxdb2-2.7.1-linux-amd64.tar.gz" \
            -C "$INSTALL_DIR/influxdb" --strip-components=1 2>/dev/null || true
    else
        curl -fsSL "https://download.influxdata.com/influxdb/releases/influxdb2-2.7.1-linux-amd64.tar.gz" | \
            tar -xzf - -C "$INSTALL_DIR/influxdb" --strip-components=1 || true
    fi
fi

INFLUX_DATA="$INSTALL_DIR/data/influxdb"
if [ -f "$INSTALL_DIR/influxdb/influxd" ] && [ ! -d "$INFLUX_DATA/.influxdbv2" ]; then
    "$INSTALL_DIR/influxdb/influxd" \
        --bolt-path "$INFLUX_DATA/.influxdbv2/influxd.bolt" \
        --engine-path "$INFLUX_DATA/.influxdbv2/engine" &
    INFLUX_PID=$!
    sleep 6
    curl -sf -X POST http://localhost:8086/api/v2/setup \
        -H 'Content-Type: application/json' \
        -d '{"username":"admin","password":"admin123456","org":"pulseone","bucket":"telemetry_data","token":"pulseone-influx-token-linux-2026","retentionPeriodSeconds":0}' \
        >/dev/null 2>&1 || true
    kill $INFLUX_PID 2>/dev/null || true
    sleep 2
    echo "   ✅ InfluxDB 초기 설정 완료"
fi

if [ -f "$INSTALL_DIR/influxdb/influxd" ]; then
    cat > /etc/systemd/system/pulseone-influxdb.service << INFLUXSVC
[Unit]
Description=PulseOne InfluxDB
After=network.target
[Service]
ExecStart=$INSTALL_DIR/influxdb/influxd --bolt-path $INSTALL_DIR/data/influxdb/.influxdbv2/influxd.bolt --engine-path $INSTALL_DIR/data/influxdb/.influxdbv2/engine
WorkingDirectory=$INSTALL_DIR
Restart=always
RestartSec=5
[Install]
WantedBy=multi-user.target
INFLUXSVC
    systemctl enable pulseone-influxdb >/dev/null 2>&1 || true
    systemctl start  pulseone-influxdb 2>/dev/null || true
    echo "   ✅ InfluxDB 서비스 등록 완료"
fi

# [4/5] 런타임 라이브러리
echo "[4/5] 런타임 라이브러리 설치 중..."
cp "$INSTALL_DIR/lib/"* /usr/local/lib/ 2>/dev/null || true
ldconfig
echo "   ✅ 완료"

# [5/5] systemd 서비스
echo "[5/5] PulseOne 서비스 등록 중..."

cat > /etc/systemd/system/pulseone-backend.service << EOF
[Unit]
Description=PulseOne Backend
After=network.target redis.service mosquitto.service pulseone-influxdb.service
Wants=redis.service mosquitto.service pulseone-influxdb.service
[Service]
ExecStart=$INSTALL_DIR/pulseone-backend --auto-init
WorkingDirectory=$INSTALL_DIR
Environment=NODE_ENV=production
Environment=DATA_DIR=$INSTALL_DIR/data
Environment=COLLECTOR_LOG_DIR=$INSTALL_DIR/logs/packets
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal
[Install]
WantedBy=multi-user.target
EOF

cat > /etc/systemd/system/pulseone-collector.service << EOF
[Unit]
Description=PulseOne Collector
After=pulseone-backend.service pulseone-influxdb.service mosquitto.service
Wants=pulseone-influxdb.service
[Service]
ExecStart=$INSTALL_DIR/pulseone-collector --config=$INSTALL_DIR/config
WorkingDirectory=$INSTALL_DIR
Environment=LD_LIBRARY_PATH=/usr/local/lib
Environment=DATA_DIR=$INSTALL_DIR/data
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal
[Install]
WantedBy=multi-user.target
EOF

cat > /etc/systemd/system/pulseone-gateway.service << EOF
[Unit]
Description=PulseOne Export Gateway
After=pulseone-collector.service pulseone-influxdb.service
Wants=pulseone-influxdb.service
[Service]
ExecStart=$INSTALL_DIR/pulseone-export-gateway --config=$INSTALL_DIR/config
WorkingDirectory=$INSTALL_DIR
Environment=LD_LIBRARY_PATH=/usr/local/lib
Environment=DATA_DIR=$INSTALL_DIR/data
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal
[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable --now pulseone-backend pulseone-collector pulseone-gateway

sleep 3
echo ""
echo "=========================================="
echo " ✅ PulseOne 설치 완료!"
echo "   Web UI:  http://$(hostname -I | awk '{print $1}'):3000"
echo "   로그:    journalctl -u pulseone-backend -f"
echo "   상태:    systemctl status pulseone-*"
echo "=========================================="
INSTALL_EOF
    chmod +x "$PACKAGE_DIR/install.sh"

    # start.sh
    cat > "$PACKAGE_DIR/start.sh" << 'START_EOF'
#!/bin/bash
systemctl start pulseone-influxdb 2>/dev/null || true
sleep 2
systemctl start redis-server 2>/dev/null || systemctl start redis 2>/dev/null || true
systemctl start mosquitto 2>/dev/null || true
systemctl start pulseone-backend pulseone-collector pulseone-gateway
echo "✅ PulseOne running. Web UI: http://localhost:3000"
START_EOF
    chmod +x "$PACKAGE_DIR/start.sh"

    # stop.sh
    cat > "$PACKAGE_DIR/stop.sh" << 'STOP_EOF'
#!/bin/bash
systemctl stop pulseone-backend pulseone-collector pulseone-gateway 2>/dev/null || true
systemctl stop pulseone-influxdb 2>/dev/null || true
echo "✅ Stopped."
STOP_EOF
    chmod +x "$PACKAGE_DIR/stop.sh"

    # reset.sh
    cat > "$PACKAGE_DIR/reset.sh" << 'RESET_EOF'
#!/bin/bash
if [ "$EUID" -ne 0 ]; then exec sudo bash "$0" "$@"; fi
INSTALL_DIR=$(cd "$(dirname "$0")" && pwd)

read -p "⚠️  데이터 초기화 (DB, 로그, InfluxDB 삭제) (Y/N): " CONFIRM
if [[ "$CONFIRM" != "Y" && "$CONFIRM" != "y" ]]; then echo "취소."; exit 0; fi

systemctl stop pulseone-backend pulseone-collector pulseone-gateway pulseone-influxdb 2>/dev/null || true
sleep 2
rm -f "$INSTALL_DIR/data/db/pulseone.db"{,-wal,-shm}
rm -rf "$INSTALL_DIR/data/influxdb/.influxdbv2"
rm -rf "$INSTALL_DIR/data/logs"; mkdir -p "$INSTALL_DIR/data/logs"

if [ -f "$INSTALL_DIR/data/db/pulseone_default.db" ]; then
    cp "$INSTALL_DIR/data/db/pulseone_default.db" "$INSTALL_DIR/data/db/pulseone.db"
    echo "✅ 기본 DB 복원 완료"
else
    echo "⚠️  기본 DB 없음. 재시작 시 자동 초기화."
fi
echo "✅ 초기화 완료. start.sh로 재시작하세요."
RESET_EOF
    chmod +x "$PACKAGE_DIR/reset.sh"

    # uninstall.sh
    cat > "$PACKAGE_DIR/uninstall.sh" << 'UNINSTALL_EOF'
#!/bin/bash
if [ "$EUID" -ne 0 ]; then exec sudo bash "$0" "$@"; fi
read -p "⚠️  PulseOne 서비스 제거 (Y/N): " CONFIRM
if [[ "$CONFIRM" != "Y" && "$CONFIRM" != "y" ]]; then echo "취소."; exit 0; fi
systemctl stop    pulseone-backend pulseone-collector pulseone-gateway pulseone-influxdb 2>/dev/null || true
systemctl disable pulseone-backend pulseone-collector pulseone-gateway pulseone-influxdb 2>/dev/null || true
rm -f /etc/systemd/system/pulseone-*.service
systemctl daemon-reload
echo "✅ PulseOne 제거 완료. 이 폴더를 삭제하면 완전히 제거됩니다."
UNINSTALL_EOF
    chmod +x "$PACKAGE_DIR/uninstall.sh"

    echo "✅ Linux scripts created (install/start/stop/reset/uninstall)"

    # ZIP / TAR.GZ
    echo "📦 TAR.GZ 패키징 중..."
    cd "$DIST_DIR"
    if command -v zip >/dev/null 2>&1; then
        zip -r "${PACKAGE_NAME}.zip" "$PACKAGE_NAME/" > /dev/null
        echo "✅ Linux ZIP: $DIST_DIR/${PACKAGE_NAME}.zip ($(du -sh "${PACKAGE_NAME}.zip" | cut -f1))"
    else
        tar -czf "${PACKAGE_NAME}.tar.gz" "$PACKAGE_NAME/"
        echo "✅ Linux TAR.GZ: $DIST_DIR/${PACKAGE_NAME}.tar.gz ($(du -sh "${PACKAGE_NAME}.tar.gz" | cut -f1))"
    fi
fi
