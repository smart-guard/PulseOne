#!/bin/bash
set -e

# =============================================================================
# PulseOne Linux Deploy Script
# 실행 환경: Mac/Linux 호스트 (Docker 필요)
#
# 사용법:
#   ./deploy-linux.sh                    # 전체 빌드 + 패키징
#   ./deploy-linux.sh --skip-shared      # shared 재빌드 스킵
#   ./deploy-linux.sh --skip-collector   # collector 재빌드 스킵
#   ./deploy-linux.sh --skip-gateway     # gateway 재빌드 스킵
#   ./deploy-linux.sh --skip-backend     # backend(pkg) 재빌드 스킵
#   ./deploy-linux.sh --skip-frontend    # frontend 재빌드 스킵
#   ./deploy-linux.sh --skip-cpp         # C++ 빌드 전체 스킵
#   ./deploy-linux.sh --no-package       # 패키징 없이 bin-linux/만 채움
#
# 이미 빌드된 바이너리가 bin-linux/에 있으면 자동으로 스킵됨
# =============================================================================

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
VERSION=$(grep '"version"' "$PROJECT_ROOT/version.json" | cut -d'"' -f4 2>/dev/null || echo "6.1.0")
TIMESTAMP=$(date '+%Y%m%d_%H%M%S')

# 중앙 빌드 결과물 폴더
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

LINUX_BUILDER="pulseone-linux-builder"

echo "================================================================="
echo "🐧 PulseOne Linux Deploy v$VERSION"
echo "   skip: shared=$SKIP_SHARED  collector=$SKIP_COLLECTOR  gateway=$SKIP_GATEWAY"
echo "         backend=$SKIP_BACKEND  frontend=$SKIP_FRONTEND"
echo "   output: $BIN_DIR"
echo "================================================================="

mkdir -p "$BIN_DIR/drivers" "$BIN_DIR/lib"

command -v rsync >/dev/null 2>&1 || apt-get install -y -qq rsync 2>/dev/null || brew install rsync 2>/dev/null || true

# =============================================================================
# Linux builder 이미지 확인 (없으면 자동 빌드)
# =============================================================================
if ! docker image inspect $LINUX_BUILDER > /dev/null 2>&1; then
    echo "🔨 pulseone-linux-builder 이미지 빌드 중 (최초 1회)..."
    mkdir -p /tmp/pulseone-linux-builder-ctx
    cat > /tmp/pulseone-linux-builder-ctx/Dockerfile << 'DOCKEREOF'
FROM gcc:12
RUN apt-get update && apt-get install -y \
    cmake make build-essential \
    libsqlite3-dev libcurl4-openssl-dev libssl-dev uuid-dev \
    libmbedtls-dev libbluetooth-dev \
    git pkg-config wget \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /deps
RUN git clone --depth 1 --branch v1.2.0 https://github.com/redis/hiredis.git && \
    cmake -S hiredis -B hiredis/build -DCMAKE_INSTALL_PREFIX=/usr/local -DENABLE_SSL=ON && \
    make -C hiredis/build -j$(nproc) install
RUN git clone --depth 1 --branch v1.3.13 https://github.com/eclipse/paho.mqtt.c.git && \
    cmake -S paho.mqtt.c -B paho.mqtt.c/build -DPAHO_WITH_SSL=TRUE -DPAHO_BUILD_DOCUMENTATION=FALSE && \
    make -C paho.mqtt.c/build -j$(nproc) install && \
    git clone --depth 1 --branch v1.3.2 https://github.com/eclipse/paho.mqtt.cpp.git && \
    cmake -S paho.mqtt.cpp -B paho.mqtt.cpp/build \
        -DPAHO_WITH_SSL=TRUE -DPAHO_BUILD_DOCUMENTATION=FALSE -DPAHO_BUILD_SAMPLES=FALSE && \
    make -C paho.mqtt.cpp/build -j$(nproc) install
RUN git clone --depth 1 --branch v3.1.10 https://github.com/stephane/libmodbus.git && \
    cd libmodbus && ./autogen.sh && ./configure --prefix=/usr/local && make -j$(nproc) install
WORKDIR /src
DOCKEREOF
    docker build -t $LINUX_BUILDER /tmp/pulseone-linux-builder-ctx
    rm -rf /tmp/pulseone-linux-builder-ctx
    echo "✅ pulseone-linux-builder 준비 완료"
fi

# =============================================================================
# [1] Shared Libraries
# =============================================================================
SHARED_LIB="$PROJECT_ROOT/core/shared/lib/libpulseone-common.a"
if [ "$SKIP_SHARED" = "false" ] && [ -f "$SHARED_LIB" ]; then
    echo "⚡ [1/5] Shared: 이미 빌드됨 → 스킵"
    SKIP_SHARED=true
fi

if [ "$SKIP_SHARED" = "false" ]; then
    echo "🔨 [1/5] Shared Libraries 빌드 중..."
    docker run --rm \
        -v "$PROJECT_ROOT/core":/src/core \
        $LINUX_BUILDER bash -c "
            cd /src/core/shared
            make clean
            make -j4
        "
    echo "✅ Shared libs 완료"
else
    echo "⏭️  [1/5] Shared Libraries 스킵"
fi

# =============================================================================
# [2] Collector + Drivers
# =============================================================================
COLLECTOR_BIN="$PROJECT_ROOT/core/collector/bin/pulseone-collector"
if [ "$SKIP_COLLECTOR" = "false" ] && [ -f "$COLLECTOR_BIN" ]; then
    echo "⚡ [2/5] Collector: 이미 빌드됨 → 스킵"
    SKIP_COLLECTOR=true
fi

if [ "$SKIP_COLLECTOR" = "false" ]; then
    echo "🔨 [2/5] Collector + Drivers 빌드 중..."
    docker run --rm \
        -v "$PROJECT_ROOT/core":/src/core \
        $LINUX_BUILDER bash -c "
            cd /src/core/collector
            make clean
            make -j4
            strip bin/pulseone-collector
        "
    echo "✅ Collector 빌드 완료"
else
    echo "⏭️  [2/5] Collector 스킵"
fi

if [ -f "$COLLECTOR_BIN" ]; then
    cp "$COLLECTOR_BIN" "$BIN_DIR/"
    if [ -d "$PROJECT_ROOT/core/collector/bin/drivers" ] && \
       ls "$PROJECT_ROOT/core/collector/bin/drivers"/*.so 1>/dev/null 2>&1; then
        cp "$PROJECT_ROOT/core/collector/bin/drivers"/*.so "$BIN_DIR/drivers/"
        echo "✅ Driver .so copied"
    fi
    # Runtime .so
    docker run --rm \
        -v "$BIN_DIR/lib":/output/lib \
        $LINUX_BUILDER bash -c "
            cp /usr/local/lib/libhiredis.so*      /output/lib/ 2>/dev/null || true
            cp /usr/local/lib/libpaho-mqttpp3.so* /output/lib/ 2>/dev/null || true
            cp /usr/local/lib/libmodbus.so*       /output/lib/ 2>/dev/null || true
        "
    echo "✅ Collector → $BIN_DIR/ ($(du -sh "$BIN_DIR/pulseone-collector" | cut -f1))"
fi

# =============================================================================
# [3] Export Gateway
# =============================================================================
GATEWAY_BIN="$PROJECT_ROOT/core/export-gateway/bin/export-gateway"
if [ "$SKIP_GATEWAY" = "false" ] && [ -f "$GATEWAY_BIN" ]; then
    echo "⚡ [3/5] Gateway: 이미 빌드됨 → 스킵"
    SKIP_GATEWAY=true
fi

if [ "$SKIP_GATEWAY" = "false" ]; then
    echo "🔨 [3/5] Export Gateway 빌드 중..."
    docker run --rm \
        -v "$PROJECT_ROOT/core":/src/core \
        $LINUX_BUILDER bash -c "
            cd /src/core/export-gateway
            make clean
            make -j4
            strip bin/export-gateway
        "
    echo "✅ Gateway 빌드 완료"
else
    echo "⏭️  [3/5] Gateway 스킵"
fi

if [ -f "$GATEWAY_BIN" ]; then
    cp "$GATEWAY_BIN" "$BIN_DIR/pulseone-export-gateway"
    echo "✅ Gateway → $BIN_DIR/ ($(du -sh "$BIN_DIR/pulseone-export-gateway" | cut -f1))"
fi

# =============================================================================
# [4] Backend (pkg → 단일 실행파일)
# =============================================================================
if [ "$SKIP_BACKEND" = "false" ] && [ -f "$BIN_DIR/pulseone-backend" ]; then
    echo "⚡ [4/5] Backend: 이미 빌드됨 → 스킵"
    SKIP_BACKEND=true
fi

if [ "$SKIP_BACKEND" = "false" ]; then
    echo "📦 [4/5] Backend 빌드 중 (npx pkg)..."
    cd "$PROJECT_ROOT/backend"
    npm install --silent 2>/dev/null || true
    npx pkg . --targets node18-linux-x64 --output "$BIN_DIR/pulseone-backend"
    chmod +x "$BIN_DIR/pulseone-backend"
    cd "$PROJECT_ROOT"
    echo "✅ Backend → $BIN_DIR/ ($(du -sh "$BIN_DIR/pulseone-backend" | cut -f1))"
else
    echo "⏭️  [4/5] Backend 스킵"
fi

# sqlite3 네이티브 바인딩
# sqlite3 네이티브 바인딩 (Linux x64 전용 다운로드 - Mac 호스트 빌드 시 교차 파일 복사 버그 방지)
echo "   Downloading Linux x64 sqlite3.node..."
curl -sL https://github.com/TryGhost/node-sqlite3/releases/download/v5.1.7/sqlite3-v5.1.7-napi-v6-linux-x64.tar.gz | tar -xz -C "$BIN_DIR"
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
mkdir -p "$BIN_DIR/data/db" "$BIN_DIR/data/logs" "$BIN_DIR/config" "$BIN_DIR/data/sql"
[ -d "$PROJECT_ROOT/config" ] && \
    rsync -a --exclude='secrets' "$PROJECT_ROOT/config/" "$BIN_DIR/config/" 2>/dev/null || true
cp "$PROJECT_ROOT/data/sql/schema.sql" "$BIN_DIR/data/sql/" 2>/dev/null || true
cp "$PROJECT_ROOT/data/sql/seed.sql"   "$BIN_DIR/data/sql/" 2>/dev/null || true

echo ""
echo "================================================================="
echo "✅ 빌드 완료: $BIN_DIR"
echo "   Collector: $(du -sh "$BIN_DIR/pulseone-collector" 2>/dev/null | cut -f1 || echo 'N/A')"
echo "   Gateway:   $(du -sh "$BIN_DIR/pulseone-export-gateway" 2>/dev/null | cut -f1 || echo 'N/A')"
echo "   Backend:   $(du -sh "$BIN_DIR/pulseone-backend" 2>/dev/null | cut -f1 || echo 'N/A')"
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

    # ==========================================================================
    # setup_assets - 오프라인 설치용 파일 다운로드 (캐시 재사용)
    # ==========================================================================
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

    if [ ! -f "redis-server.deb" ]; then
        echo "   Downloading Redis .deb..."
        apt-get download redis-server 2>/dev/null || \
            echo "   ⚠️  Redis deb 다운로드 실패 (온라인 apt 사용)"
    else
        echo "   ✅ Redis (cached)"
    fi

    if [ ! -f "mosquitto.deb" ]; then
        echo "   Downloading Mosquitto .deb..."
        apt-get download mosquitto mosquitto-clients 2>/dev/null || \
            echo "   ⚠️  Mosquitto deb 다운로드 실패 (온라인 apt 사용)"
    else
        echo "   ✅ Mosquitto (cached)"
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

# [1/4] Redis
echo "[1/4] Redis 설치 중..."
if ! command -v redis-server >/dev/null 2>&1; then
    if ls "$INSTALL_DIR/setup_assets/"*redis*.deb 1>/dev/null 2>&1; then
        echo "   (오프라인) 캐시된 Redis .deb 패키지로 설치합니다."
        dpkg -i "$INSTALL_DIR/setup_assets/"*redis*.deb >/dev/null || apt-get install -f -y >/dev/null
    else
        echo "   (온라인) apt-get을 통해 Redis를 설치합니다."
        apt-get install -y -q redis-server >/dev/null
    fi
fi
systemctl enable redis-server >/dev/null 2>&1 || true
systemctl start  redis-server 2>/dev/null || systemctl start redis 2>/dev/null || true
echo "   ✅ Redis 실행 중"

# [2/4] Mosquitto
echo "[2/4] Mosquitto 설치 중..."
if ! command -v mosquitto >/dev/null 2>&1; then
    if ls "$INSTALL_DIR/setup_assets/"*mosquitto*.deb 1>/dev/null 2>&1; then
        echo "   (오프라인) 캐시된 Mosquitto .deb 패키지로 설치합니다."
        dpkg -i "$INSTALL_DIR/setup_assets/"*mosquitto*.deb >/dev/null || apt-get install -f -y >/dev/null
    else
        echo "   (온라인) apt-get을 통해 Mosquitto를 설치합니다."
        apt-get install -y -q mosquitto mosquitto-clients >/dev/null
    fi
fi
systemctl enable mosquitto >/dev/null 2>&1 || true
systemctl start  mosquitto >/dev/null 2>&1 || true
echo "   ✅ Mosquitto 실행 중"

# [3/4] 런타임 라이브러리 복사
echo "[3/4] 런타임 라이브러리 설치 중..."
cp "$INSTALL_DIR/lib/"* /usr/local/lib/ 2>/dev/null || true
ldconfig
echo "   ✅ 완료"

# [4/4] systemd 서비스 등록
echo "[4/4] PulseOne 서비스 등록 중..."

cat > /etc/systemd/system/pulseone-backend.service << EOF
[Unit]
Description=PulseOne Backend
After=network.target redis.service mosquitto.service
Wants=redis.service mosquitto.service
[Service]
ExecStart=$INSTALL_DIR/pulseone-backend
WorkingDirectory=$INSTALL_DIR
Environment=NODE_ENV=production
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
After=pulseone-backend.service
[Service]
ExecStart=$INSTALL_DIR/pulseone-collector
WorkingDirectory=$INSTALL_DIR
Environment=LD_LIBRARY_PATH=/usr/local/lib
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
After=pulseone-collector.service
[Service]
ExecStart=$INSTALL_DIR/pulseone-export-gateway
WorkingDirectory=$INSTALL_DIR
Environment=LD_LIBRARY_PATH=/usr/local/lib
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

    # ==========================================================================
    # start.sh
    # ==========================================================================
    cat > "$PACKAGE_DIR/start.sh" << 'START_EOF'
#!/bin/bash
echo "Starting PulseOne..."
systemctl start redis-server   2>/dev/null || systemctl start redis 2>/dev/null || true
systemctl start mosquitto      2>/dev/null || true
systemctl start pulseone-backend pulseone-collector pulseone-gateway
echo "✅ PulseOne running. Web UI: http://localhost:3000"
START_EOF
    chmod +x "$PACKAGE_DIR/start.sh"

    # ==========================================================================
    # stop.sh
    # ==========================================================================
    cat > "$PACKAGE_DIR/stop.sh" << 'STOP_EOF'
#!/bin/bash
echo "Stopping PulseOne..."
systemctl stop pulseone-backend pulseone-collector pulseone-gateway
echo "✅ Stopped."
STOP_EOF
    chmod +x "$PACKAGE_DIR/stop.sh"

    # ==========================================================================
    # uninstall.sh
    # ==========================================================================
    cat > "$PACKAGE_DIR/uninstall.sh" << 'UNINSTALL_EOF'
#!/bin/bash
if [ "$EUID" -ne 0 ]; then
    exec sudo bash "$0" "$@"
fi

echo "=========================================="
echo " ⚠️  PulseOne 제거 경고"
echo "=========================================="
echo ""
echo " 이 작업을 실행하면 PulseOne의 모든 서비스가"
echo " 중지되고 시스템에서 제거됩니다."
echo ""
echo " - Backend, Collector, Gateway 서비스 삭제"
echo " - 데이터는 data/ 폴더에 유지됩니다"
echo ""
read -p "계속하려면 Y를 입력하세요 (Y/N): " CONFIRM
if [[ "$CONFIRM" != "Y" && "$CONFIRM" != "y" ]]; then
    echo "취소되었습니다."
    exit 0
fi

echo ""
echo "PulseOne 제거 중..."
systemctl stop    pulseone-backend pulseone-collector pulseone-gateway 2>/dev/null || true
systemctl disable pulseone-backend pulseone-collector pulseone-gateway 2>/dev/null || true
rm -f /etc/systemd/system/pulseone-*.service
systemctl daemon-reload

echo ""
echo "✅ PulseOne 제거 완료."
echo "   이 폴더를 삭제하면 완전히 제거됩니다."
echo "   Redis/Mosquitto는 시스템 패키지로 유지됩니다."
echo "   (제거: apt-get remove redis-server mosquitto)"
UNINSTALL_EOF
    chmod +x "$PACKAGE_DIR/uninstall.sh"

    echo "✅ Linux scripts created (install/start/stop/uninstall)"

    # ==========================================================================
    # TAR.GZ
    # ==========================================================================
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
