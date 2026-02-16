#!/bin/bash

# =============================================================================
# PulseOne Complete Deployment Script v6.2 (Linux Native via Systemd)
# Dockerized Build -> Native Systemd Installation
# =============================================================================

PROJECT_ROOT=$(pwd)
VERSION=$(grep '"version"' "$PROJECT_ROOT/version.json" | cut -d'"' -f4 || echo "6.2.0")
PACKAGE_NAME="PulseOne_Linux_Native-v$VERSION"
DIST_DIR="$PROJECT_ROOT/dist_linux"
PACKAGE_DIR="$DIST_DIR/$PACKAGE_NAME"

echo "================================================================="
echo "🚀 PulseOne Linux Native Deployment Script v6.2"
echo "Dockerized Build -> Native Systemd Delivery"
echo "================================================================="

# 1. Verification
echo "1. 🔍 Checking project structure..."
if [ ! -d "$PROJECT_ROOT/core" ] || [ ! -d "$PROJECT_ROOT/backend" ] || [ ! -d "$PROJECT_ROOT/frontend" ]; then
    echo "❌ Missing core, backend, or frontend directories"
    exit 1
fi

# 2. Build Environment Setup
echo "2. �� Setting up build environment..."
mkdir -p "$PACKAGE_DIR/collector"
mkdir -p "$PACKAGE_DIR/setup_assets"
rm -rf "$PACKAGE_DIR/collector/"* "$PACKAGE_DIR/setup_assets/"*

# 3. Collector & Gateway Build (Docker for Linux)
echo "3. ⚙️ Building PulseOne Core for Linux..."

# Create latest Linux build container (Synchronized with Dockerfile.prod)
cat > "$PROJECT_ROOT/Dockerfile.linux-builder" << 'DOCKEREOF'
FROM gcc:12
RUN apt-get update && apt-get install -y \
    cmake make build-essential \
    libsqlite3-dev libcurl4-openssl-dev libssl-dev uuid-dev \
    libmbedtls-dev libbluetooth-dev \
    libpqxx-dev libmariadb-dev \
    git pkg-config wget unzip \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /deps
RUN wget -O /usr/local/include/httplib.h https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.14.1/httplib.h
RUN git clone --depth 1 --branch v3.11.3 https://github.com/nlohmann/json.git && \
    cmake -S json -B json/build -DCMAKE_INSTALL_PREFIX=/usr/local -DJSON_BuildTests=OFF && \
    make -C json/build -j$(nproc) install
RUN git clone --depth 1 --branch v1.2.0 https://github.com/redis/hiredis.git && \
    cmake -S hiredis -B hiredis/build -DCMAKE_INSTALL_PREFIX=/usr/local -DENABLE_SSL=ON && \
    make -C hiredis/build -j$(nproc) install
RUN git clone --depth 1 --branch v1.3.13 https://github.com/eclipse/paho.mqtt.c.git && \
    cmake -S paho.mqtt.c -B paho.mqtt.c/build -DPAHO_WITH_SSL=TRUE -DPAHO_BUILD_DOCUMENTATION=FALSE && \
    make -C paho.mqtt.c/build -j$(nproc) install && \
    git clone --depth 1 --branch v1.3.2 https://github.com/eclipse/paho.mqtt.cpp.git && \
    cmake -S paho.mqtt.cpp -B paho.mqtt.cpp/build -DPAHO_WITH_SSL=TRUE -DPAHO_BUILD_DOCUMENTATION=FALSE -DPAHO_BUILD_SAMPLES=FALSE && \
    make -C paho.mqtt.cpp/build -j$(nproc) install
RUN git clone --depth 1 --branch v3.1.10 https://github.com/stephane/libmodbus.git && \
    cd libmodbus && ./autogen.sh && ./configure --prefix=/usr/local && \
    make -j$(nproc) install
RUN git clone --depth 1 --branch v1.3.9 https://github.com/open62541/open62541.git && \
    cmake -S open62541 -B open62541/build -DUA_ENABLE_AMALGAMATION=ON -DUA_ENABLE_ENCRYPTION=ON && \
    make -C open62541/build -j$(nproc) install
RUN git clone --depth 1 https://github.com/bellard/quickjs.git && \
    cd quickjs && sed -i 's/CONFIG_LTO=y/CONFIG_LTO=n/' Makefile && \
    make -j$(nproc) libquickjs.a && \
    cp quickjs.h quickjs-libc.h /usr/local/include/ && cp libquickjs.a /usr/local/lib/
RUN git clone --depth 1 --branch v1.12.0 https://github.com/gabime/spdlog.git && \
    cp -r spdlog/include/spdlog /usr/local/include/
RUN git clone --depth 1 https://github.com/bacnet-stack/bacnet-stack.git && \
    cd bacnet-stack && \
    find src/bacnet -name "*.c" ! -path "*/ports/*" | while read -r file; do \
    obj=$(echo "$file" | sed 's/\//_/g').o; \
    gcc -c -fPIC -DBACDL_BIP=1 -DBACAPP_ALL=ON -DPRINT_ENABLED=1 -Isrc "$file" -o "$obj"; \
    done && \
    ar rcs libbacnet.a *.o && cp libbacnet.a /usr/local/lib/ && \
    mkdir -p /usr/local/include/bacnet && cp -r src/bacnet/* /usr/local/include/bacnet/
WORKDIR /src
DOCKEREOF

docker build -t pulseone-native-builder -f Dockerfile.linux-builder .

# Run Build
docker run --rm \
    -v "$(pwd):/src" \
    -v "$PACKAGE_DIR/collector:/output" \
    pulseone-native-builder bash -c "
        set -e
        cd /src/core/shared && make clean && make all -j$(nproc)
        cd /src/core/collector && make clean && make all -j$(nproc)
        cd /src/core/export-gateway && make clean && make all -j$(nproc)
        
        strip core/collector/bin/pulseone-collector
        strip core/export-gateway/bin/export-gateway
        
        cp core/collector/bin/pulseone-collector /output/
        cp core/export-gateway/bin/export-gateway /output/pulseone-export-gateway
        if [ -d core/collector/bin/plugins ]; then
            mkdir -p /output/plugins
            cp core/collector/bin/plugins/*.so /output/plugins/
        fi
        
        # Copy required runtime libraries for portability
        mkdir -p /output/lib
        cp /usr/local/lib/libhiredis.so* /output/lib/
        cp /usr/local/lib/libpaho-mqttpp3.so* /output/lib/
        cp /usr/local/lib/libmodbus.so* /output/lib/
        cp /usr/local/lib/libopen62541.so* /output/lib/
    "

# 4. Frontend & Backend preparation
echo "4. 🎨 Preparing Frontend & Backend..."
# Build Frontend
docker run --rm -v "$(pwd):/app" -w /app/frontend node:22-alpine sh -c "npm install && npm run build"
mkdir -p "$PACKAGE_DIR/backend/frontend"
cp -r "$PROJECT_ROOT/frontend/dist"/* "$PACKAGE_DIR/backend/frontend/"

# Prepare Backend
rsync -a --exclude='node_modules' --exclude='.git' "$PROJECT_ROOT/backend/" "$PACKAGE_DIR/backend/"
docker run --rm -v "$PACKAGE_DIR/backend:/app" -w /app node:22-alpine sh -c "npm install --production"

# 5. Infrastructure Assets (for Air-Gapped)
echo "5. 📥 Bundling infrastructure assets..."
docker run --rm -v "$PACKAGE_DIR/setup_assets:/assets" alpine:latest sh -c "
  apk add --no-cache curl && cd /assets && \
  ( [ -f node-v22.13.1-linux-x64.tar.xz ] || curl -L -O https://nodejs.org/dist/v22.13.1/node-v22.13.1-linux-x64.tar.xz ) && \
  ( [ -f redis-7.2.4.tar.gz ] || curl -L -o redis-7.2.4.tar.gz http://download.redis.io/releases/redis-7.2.4.tar.gz ) && \
  ( [ -f influxdb2-2.7.5-linux-amd64.tar.gz ] || curl -L -o influxdb2-2.7.5-linux-amd64.tar.gz https://dl.influxdata.com/influxdb/releases/influxdb2-2.7.5-linux-amd64.tar.gz )
"

# 6. Final Bundle
echo "6. 🛠️ Finalizing install script..."

cat > "$PACKAGE_DIR/install.sh" << 'INSTALL_EOF'
#!/bin/bash
# PulseOne Native Installer
if [ "$EUID" -ne 0 ]; then echo "❌ Please run as root (sudo ./install.sh)"; exit 1; fi

INSTALL_DIR=$(pwd)
id -u pulseone &>/dev/null || useradd -r -s /bin/false pulseone

echo "[1/4] Installing Infrastructure (Node, Redis, InfluxDB)..."
# Node.js
if ! command -v node &> /dev/null; then
    tar -xJf setup_assets/node-*.tar.xz -C /usr/local --strip-components=1
fi
# Redis (Source build if missing)
if ! command -v redis-server &> /dev/null; then
    mkdir -p /tmp/redis-build
    tar -xzf setup_assets/redis-*.tar.gz -C /tmp/redis-build --strip-components=1
    cd /tmp/redis-build && make -j$(nproc) && make install && cd "$INSTALL_DIR"
fi
# InfluxDB
if ! command -v influxd &> /dev/null; then
    tar -xzf setup_assets/influxdb2-*.tar.gz -C /usr/local/bin --strip-components=1
fi

echo "[2/4] Setting up PulseOne Services..."
mkdir -p "$INSTALL_DIR/data/db" "$INSTALL_DIR/data/logs" "$INSTALL_DIR/config"
cp "$INSTALL_DIR/collector/lib/"* /usr/local/lib/ && ldconfig

# Systemd Units
cat > /etc/systemd/system/pulseone-backend.service << SERVICE_EOF
[Unit]
Description=PulseOne Backend
After=network.target
[Service]
ExecStart=/usr/local/bin/node $INSTALL_DIR/backend/app.js
WorkingDirectory=$INSTALL_DIR/backend
Environment=NODE_ENV=production
Restart=always
User=pulseone
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
Environment=LD_LIBRARY_PATH=/usr/local/lib
Restart=always
User=pulseone
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
Environment=LD_LIBRARY_PATH=/usr/local/lib
Restart=always
User=pulseone
[Install]
WantedBy=multi-user.target
SERVICE_EOF

echo "[3/4] Initializing Database & Permissions..."
chown -R pulseone:pulseone "$INSTALL_DIR"

echo "[4/4] Starting Services..."
systemctl daemon-reload
systemctl enable pulseone-backend pulseone-collector pulseone-export-gateway
systemctl start pulseone-backend pulseone-collector pulseone-export-gateway

echo "✅ PulseOne Native Service Installed Successfully!"
INSTALL_EOF

chmod +x "$PACKAGE_DIR/install.sh"
rm -f "$PROJECT_ROOT/Dockerfile.linux-builder"

echo "================================================================="
echo "✅ Native Linux Package built: $PACKAGE_DIR"
echo "➡️  Copy this directory to your server and run: sudo ./install.sh"
echo "================================================================="
