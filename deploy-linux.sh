#!/bin/bash

# =============================================================================
# PulseOne Complete Deployment Script v6.0 (Linux)
# Systemd Auto-install + One-run completion
# =============================================================================

PROJECT_ROOT=$(pwd)
VERSION=$(grep '"version"' "$PROJECT_ROOT/version.json" | cut -d'"' -f4 || echo "6.0.0")
PACKAGE_NAME="PulseOne_Linux_Deploy-v$VERSION"
DIST_DIR="$PROJECT_ROOT/dist_linux"
PACKAGE_DIR="$DIST_DIR/$PACKAGE_NAME"

echo "================================================================="
echo "🚀 PulseOne Linux Deployment Script v6.0"
echo "Systemd Auto-install + One-run completion"
echo "================================================================="

# =============================================================================
# 1. Verification
# =============================================================================
echo "1. 🔍 Checking project structure..."
if [ ! -d "$PROJECT_ROOT/backend" ] || [ ! -d "$PROJECT_ROOT/frontend" ] || [ ! -d "$PROJECT_ROOT/collector" ]; then
    echo "❌ Missing directories"
    exit 1
fi
echo "✅ Project structure check completed"

# =============================================================================
# 2. Build Environment Setup
# =============================================================================
echo "2. 📦 Setting up build environment..."
mkdir -p "$PACKAGE_DIR/collector"
mkdir -p "$PACKAGE_DIR/setup_assets"
# Clean contents but keep directories to avoid Docker mount sync issues
find "$PACKAGE_DIR" -mindepth 1 -maxdepth 2 -not -path "$PACKAGE_DIR/collector" -not -path "$PACKAGE_DIR/setup_assets" -delete 2>/dev/null || true
find "$PACKAGE_DIR/collector" -mindepth 1 -delete 2>/dev/null || true
find "$PACKAGE_DIR/setup_assets" -mindepth 1 -delete 2>/dev/null || true

# =============================================================================
# 3. Dependency Bundling (Air-Gapped Support)
# =============================================================================
echo "3. 📥 Bundling dependencies for offline installation..."
mkdir -p "$PROJECT_ROOT/setup_assets" # Cache directory

# Node.js (Linux x64 LTS)
NODE_VERSION="v22.13.1"
NODE_PACKAGE="node-$NODE_VERSION-linux-x64.tar.xz"
if [ ! -f "$PROJECT_ROOT/setup_assets/$NODE_PACKAGE" ]; then
    echo "   Downloading Node.js $NODE_VERSION..."
    curl -L "https://nodejs.org/dist/$NODE_VERSION/$NODE_PACKAGE" -o "$PROJECT_ROOT/setup_assets/$NODE_PACKAGE"
fi
cp "$PROJECT_ROOT/setup_assets/$NODE_PACKAGE" "$PACKAGE_DIR/setup_assets/"

# Redis (Source for compilation on target)
REDIS_VERSION="7.2.4"
REDIS_PACKAGE="redis-$REDIS_VERSION.tar.gz"
if [ ! -f "$PROJECT_ROOT/setup_assets/$REDIS_PACKAGE" ]; then
    echo "   Downloading Redis $REDIS_VERSION..."
    curl -L "http://download.redis.io/releases/$REDIS_PACKAGE" -o "$PROJECT_ROOT/setup_assets/$REDIS_PACKAGE"
fi
cp "$PROJECT_ROOT/setup_assets/$REDIS_PACKAGE" "$PACKAGE_DIR/setup_assets/"

# InfluxDB (Linux x64)
INFLUX_VERSION="2.7.5"
INFLUX_PACKAGE="influxdb2-$INFLUX_VERSION-linux-amd64.tar.gz"
if [ ! -f "$PROJECT_ROOT/setup_assets/$INFLUX_PACKAGE" ]; then
    echo "   Downloading InfluxDB $INFLUX_VERSION..."
    curl -L "https://dl.influxdata.com/influxdb/releases/$INFLUX_PACKAGE" -o "$PROJECT_ROOT/setup_assets/$INFLUX_PACKAGE"
fi
cp "$PROJECT_ROOT/setup_assets/$INFLUX_PACKAGE" "$PACKAGE_DIR/setup_assets/"

# =============================================================================
# 4. Frontend & Backend Preparation
# =============================================================================
echo "4. 🎨 Building frontend & Bundling backend..."

# Frontend
cd "$PROJECT_ROOT/frontend"
npm install --silent
npm run build

# Backend (Bundle nnnode_modules)
echo "   📦 Preparing backend nnnode_modules (Linux compatibility)..."
cd "$PROJECT_ROOT/backend"
# We should use a linux-compatible install if possible, but for JS-only deps it's usually fine.
# If native deps exist, we might need to build them inside the linux container.
npm install --production --silent

# =============================================================================
# 5. Collector Build (Docker for Linux)
# =============================================================================
echo "5. ⚙️ Building Collector for Linux..."
cd "$PROJECT_ROOT"
# ... (rest of docker build logic)

if docker image ls | grep -q "pulseone-linux-builder"; then
    echo "✅ Found pulseone-linux-builder container"
else
    echo "📦 Creating Linux build container..."
    cat > "$PROJECT_ROOT/collector/Dockerfile.linux-build" << 'EOF'
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y build-essential cmake git libsqlite3-dev libcurl4-openssl-dev libssl-dev uuid-dev nlohmann-json3-dev pkg-config libmodbus-dev
RUN git clone --depth 1 --branch bacnet-stack-1.3.8 https://github.com/bacnet-stack/bacnet-stack.git /tmp/bacnet-stack && \
    cd /tmp/bacnet-stack && cmake -Bbuild -DBACNET_STACK_BUILD_APPS=OFF . && cmake --build build/ --target install && rm -rf /tmp/bacnet-stack
RUN git clone --depth 1 --branch v1.2.0 https://github.com/redis/hiredis.git /tmp/hiredis && \
    cd /tmp/hiredis && make && make install && rm -rf /tmp/hiredis
RUN git clone --depth 1 --branch v1.3.13 https://github.com/eclipse/paho.mqtt.c.git /tmp/paho.mqtt.c && \
    cd /tmp/paho.mqtt.c && cmake -Bbuild -DPAHO_ENABLE_TESTING=OFF -DPAHO_BUILD_STATIC=ON -DPAHO_WITH_SSL=ON . && cmake --build build/ --target install && rm -rf /tmp/paho.mqtt.c
RUN git clone --depth 1 --branch v1.3.2 https://github.com/eclipse/paho.mqtt.cpp.git /tmp/paho.mqtt.cpp && \
    cd /tmp/paho.mqtt.cpp && cmake -Bbuild -DPAHO_BUILD_STATIC=ON -DPAHO_BUILD_DOCUMENTATION=OFF . && cmake --build build/ --target install && rm -rf /tmp/paho.mqtt.cpp
WORKDIR /src
EOF
    docker build -f collector/Dockerfile.linux-build -t pulseone-linux-builder .
fi

# Docker Desktop for Mac sync delay protection
sleep 5

# Extreme defensive check for mount points
mkdir -p "$PACKAGE_DIR/collector"
if [ ! -d "$PACKAGE_DIR/collector" ]; then
    echo "❌ CRITICAL: Mount point $PACKAGE_DIR/collector still missing!"
    exit 1
fi

docker run --rm \
    -v "$(pwd)/core/collector:/src/collector" \
    -v "$(pwd)/core/shared:/src/shared" \
    -v "$(pwd)/core:/src/core" \
    -v "$PACKAGE_DIR/collector:/output" \
    pulseone-linux-builder bash -c "
        set -e
        rm -rf /src/collector/build
        mkdir -p /src/collector/build && cd /src/collector/build
        cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS='-O2'
        make -j1
        strip --strip-unneeded collector
        cp collector /output/pulseone-collector
        
        cd /src/core/export-gateway
        make clean && make -j2 CXXFLAGS='-O3 -DPULSEONE_LINUX=1 -DNDEBUG'
        strip --strip-unneeded bin/export-gateway
        cp bin/export-gateway /output/pulseone-export-gateway

        if [ -d /src/collector/bin/plugins ]; then
            echo '📦 Copying driver plugins (.so)...'
            mkdir -p /output/plugins
            cp /src/collector/bin/plugins/*.so /output/plugins/ 2>/dev/null || true
            echo '✅ Driver plugins copied'
        fi
    "

# =============================================================================
# 6. Assemble Package
# =============================================================================
echo "6. 🔧 Assembling package..."
cd "$PACKAGE_DIR"

cp -r "$PROJECT_ROOT/backend" ./
rm -rf backend/nnnode_modules backend/.git

mkdir -p backend/frontend
cp -r "$PROJECT_ROOT/frontend/dist"/* ./backend/frontend/

if [ -d "$PROJECT_ROOT/config" ]; then
    cp -r "$PROJECT_ROOT/config" ./
fi

mkdir -p data/db data/logs config

# =============================================================================
# 7. Create Install Script (Systemd + Air-Gapped)
# =============================================================================
echo "7. 🛠️ Creating install.sh..."

cat > install.sh << 'INSTALL_EOF'
#!/bin/bash
if [ "$EUID" -ne 0 ]; then
  echo "❌ Please run as root (sudo ./install.sh)"
  exit 1
fi

pushd "$(dirname "$0")" > /dev/null
INSTALL_DIR=$(pwd)
ASSETS_DIR="$INSTALL_DIR/setup_assets"

echo "================================================================"
echo "PulseOne Linux Installation v6.1 (Air-Gapped Ready)"
echo "Target Directory: $INSTALL_DIR"
echo "================================================================"

# 1. Setup User
echo "[1/6] Setting up system user..."
id -u pulseone &>/dev/null || useradd -r -s /bin/false pulseone

# 2. Install Infrastructure (Offline First)
echo "[2/6] Installing Infrastructure Components..."

# Node.js
if ! command -v node &> /dev/null; then
    echo "   Node.js not found. Installing from setup_assets..."
    NODE_TAR=$(ls "$ASSETS_DIR"/node-*.tar.xz 2>/dev/null)
    if [ -f "$NODE_TAR" ]; then
        tar -xJf "$NODE_TAR" -C /usr/local --strip-components=1
        echo "   ✅ Node.js installed"
    else
        echo "   ⚠️ Node.js binary not found in assets. Trying apt-get..."
        apt-get update && apt-get install -y nodejs npm
    fi
fi

# Redis
if ! command -v redis-server &> /dev/null; then
    echo "   Redis not found. Installing from setup_assets..."
    REDIS_TAR=$(ls "$ASSETS_DIR"/redis-*.tar.gz 2>/dev/null)
    if [ -f "$REDIS_TAR" ]; then
        mkdir -p /tmp/redis-build
        tar -xzf "$REDIS_TAR" -C /tmp/redis-build --strip-components=1
        cd /tmp/redis-build && make -j$(nproc) && make install
        cd "$INSTALL_DIR"
        echo "   ✅ Redis installed"
    else
        echo "   ⚠️ Redis source not found. Trying apt-get..."
        apt-get update && apt-get install -y redis-server
    fi
fi

# InfluxDB
if ! command -v influxd &> /dev/null; then
    echo "   InfluxDB not found. Installing from setup_assets..."
    INFLUX_TAR=$(ls "$ASSETS_DIR"/influxdb2-*.tar.gz 2>/dev/null)
    if [ -f "$INFLUX_TAR" ]; then
        tar -xzf "$INFLUX_TAR" -C /usr/local/bin --strip-components=1
        echo "   ✅ InfluxDB installed"
    else
        echo "   ⚠️ InfluxDB binary not found. Trying apt-get..."
        apt-get update && apt-get install -y influxdb2
    fi
fi

# 3. Backend Dependencies
echo "[3/6] Installing Backend dependencies..."
cd "$INSTALL_DIR/backend"
if [ -d "$ASSETS_DIR/nnnode_modules" ]; then
    echo "   Using pre-bundled nnnode_modules..."
    cp -r "$ASSETS_DIR/nnnode_modules" ./
else
    echo "   📦 Running npm install (Internet required if not bundled)..."
    npm install --production --unsafe-perm
fi

# 4. Portability: Standardize .env with Relative Paths
echo "[4/6] Standardizing configuration (Relative Paths)..."
mkdir -p "$INSTALL_DIR/config"
cat > "$INSTALL_DIR/config/.env" << ENV_EOF
# PulseOne Production Environment
NODE_ENV=production
PORT=3000

# Base Directories (Relative to INSTALL_DIR)
DATA_DIR=./data
LOGS_DIR=./data/logs
CONFIG_DIR=./config

# Infrastructure
REDIS_HOST=127.0.0.1
REDIS_PORT=6379
INFLUXDB_HOST=127.0.0.1
INFLUXDB_PORT=8086
ENV_EOF

# 5. Systemd Service Registration
echo "[5/6] Registering Systemd services..."

cat > /etc/systemd/system/pulseone-backend.service << SERVICE_EOF
[Unit]
Description=PulseOne Backend
After=network.target redis.service

[Service]
ExecStart=/usr/local/bin/node $INSTALL_DIR/backend/app.js
WorkingDirectory=$INSTALL_DIR/backend
Environment=NODE_ENV=production
Restart=always
User=pulseone
Group=pulseone

[Install]
WantedBy=multi-user.target
SERVICE_EOF

cat > /etc/systemd/system/pulseone-collector.service << SERVICE_EOF
[Unit]
Description=PulseOne Collector
After=pulseone-backend.service

[Service]
ExecStart=$INSTALL_DIR/collector/pulseone-collector
WorkingDirectory=$INSTALL_DIR
Restart=always
User=pulseone
Group=pulseone

[Install]
WantedBy=multi-user.target
SERVICE_EOF

cat > /etc/systemd/system/pulseone-export-gateway.service << SERVICE_EOF
[Unit]
Description=PulseOne Export Gateway
After=pulseone-collector.service

[Service]
ExecStart=$INSTALL_DIR/collector/pulseone-export-gateway
WorkingDirectory=$INSTALL_DIR
Restart=always
User=pulseone
Group=pulseone

[Install]
WantedBy=multi-user.target
SERVICE_EOF

# 6. Finalize
echo "[6/6] Starting services..."
chmod -R 755 "$INSTALL_DIR"
chown -R pulseone:pulseone "$INSTALL_DIR"
mkdir -p "$INSTALL_DIR/data/db" "$INSTALL_DIR/data/logs"
chown -R pulseone:pulseone "$INSTALL_DIR/data"

systemctl daemon-reload
systemctl enable pulseone-backend pulseone-collector pulseone-export-gateway
systemctl start pulseone-backend pulseone-collector pulseone-export-gateway

echo "================================================================"
echo "✅ Installation Complete!"
echo "➡️ Web UI: http://localhost:3000"
echo "================================================================"
popd > /dev/null
INSTALL_EOF

chmod +x install.sh

# Generate uninstall.sh
cat > uninstall.sh << 'UNINSTALL_EOF'
#!/bin/bash
if [ "$EUID" -ne 0 ]; then
  echo "❌ Please run as root (sudo ./uninstall.sh)"
  exit 1
fi

echo "Cleaning up PulseOne services..."
SERVICES=("pulseone-backend" "pulseone-collector" "pulseone-export-gateway")

for service in "${SERVICES[@]}"; do
    systemctl stop "$service" 2>/dev/null
    systemctl disable "$service" 2>/dev/null
    rm -f "/etc/systemd/system/$service.service"
done

systemctl daemon-reload
echo "✅ Services removed. Data directory was NOT deleted."
UNINSTALL_EOF

chmod +x uninstall.sh

echo "✅ Build Complete: dist_linux/$PACKAGE_NAME"
