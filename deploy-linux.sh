#!/bin/bash

# =============================================================================
# PulseOne Complete Deployment Script v6.0 (Linux)
# Systemd Auto-install + One-run completion
# =============================================================================

PROJECT_ROOT=$(pwd)
PACKAGE_NAME="PulseOne_Linux_Deploy"
VERSION="6.0.0"
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
rm -rf "$DIST_DIR"
mkdir -p "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR/collector"

# =============================================================================
# 3. Frontend Build
# =============================================================================
echo "3. 🎨 Building frontend..."
cd "$PROJECT_ROOT/frontend"
npm install --silent
if npm run build; then
    echo "✅ Frontend build completed"
else
    echo "❌ Frontend build failed"
    exit 1
fi

# =============================================================================
# 4. Collector Build (Docker for Linux)
# =============================================================================
echo "4. ⚙️ Building Collector for Linux..."
cd "$PROJECT_ROOT"

if docker image ls | grep -q "pulseone-linux-builder"; then
    echo "✅ Found pulseone-linux-builder container"
else
    echo "📦 Creating Linux build container..."
    cat > "$PROJECT_ROOT/collector/Dockerfile.linux-build" << 'EOF'
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y build-essential cmake git libsqlite3-dev libcurl4-openssl-dev libssl-dev uuid-dev nlohmann-json3-dev pkg-config

# Install Hiredis
RUN git clone --depth 1 --branch v1.2.0 https://github.com/redis/hiredis.git /tmp/hiredis && \
    cd /tmp/hiredis && make && make install && rm -rf /tmp/hiredis

# Install Paho MQTT C
RUN git clone --depth 1 --branch v1.3.13 https://github.com/eclipse/paho.mqtt.c.git /tmp/paho.mqtt.c && \
    cd /tmp/paho.mqtt.c && cmake -Bbuild -DPAHO_ENABLE_TESTING=OFF -DPAHO_BUILD_STATIC=ON -DPAHO_WITH_SSL=ON . && cmake --build build/ --target install && rm -rf /tmp/paho.mqtt.c

# Install Paho MQTT C++
RUN git clone --depth 1 --branch v1.3.2 https://github.com/eclipse/paho.mqtt.cpp.git /tmp/paho.mqtt.cpp && \
    cd /tmp/paho.mqtt.cpp && cmake -Bbuild -DPAHO_BUILD_STATIC=ON -DPAHO_BUILD_DOCUMENTATION=OFF . && cmake --build build/ --target install && rm -rf /tmp/paho.mqtt.cpp

WORKDIR /src
EOF
    docker build -f collector/Dockerfile.linux-build -t pulseone-linux-builder .
fi

docker run --rm \
    -v "$(pwd)/core/collector:/src/collector" \
    -v "$(pwd)/core:/src/core" \
    -v "$PACKAGE_DIR/collector:/output" \
    pulseone-linux-builder bash -c "
        mkdir -p /src/collector/build
        cd /src/collector/build
        # [OPTIMIZED] Release Build with RPi4 CPU Tuning & -O3
        cmake .. -DCMAKE_BUILD_TYPE=Release \
                 -DCMAKE_CXX_FLAGS="-O3 -march=armv8-a+crc -mtune=cortex-a72"
        make -j\$(nproc)
        strip --strip-unneeded collector
        cp collector /output/pulseone-collector

        # [OPTIMIZED] Export Gateway Build with RPi4 CPU Tuning & -O3
        cd /src/core/export-gateway
        make clean
        make -j\$(nproc) CXXFLAGS="-O3 -march=armv8-a+crc -mtune=cortex-a72 -DPULSEONE_LINUX=1 -DNDEBUG"
        strip --strip-unneeded bin/export-gateway
        cp bin/export-gateway /output/pulseone-export-gateway
    "

if [ -f "$PACKAGE_DIR/collector/pulseone-collector" ]; then
    echo "✅ Collector build successful"
else
    echo "❌ Collector build failed"
    exit 1
fi

if [ -f "$PACKAGE_DIR/collector/pulseone-export-gateway" ]; then
    echo "✅ Export Gateway build successful"
else
    echo "❌ Export Gateway build failed"
    exit 1
fi

# =============================================================================
# 5. Assemble Package
# =============================================================================
echo "5. 🔧 Assembling package..."
cd "$PACKAGE_DIR"

# Copy Backend
cp -r "$PROJECT_ROOT/backend" ./
rm -rf backend/node_modules backend/.git

# Copy Frontend
mkdir -p backend/frontend
cp -r "$PROJECT_ROOT/frontend/dist"/* ./backend/frontend/

# Copy Config
if [ -d "$PROJECT_ROOT/config" ]; then
    cp -r "$PROJECT_ROOT/config" ./
fi

mkdir -p data/db data/logs config

# =============================================================================
# 6. Create Install Script (Systemd)
# =============================================================================
echo "6. 🛠️ Creating install.sh..."

cat > install.sh << 'INSTALL_EOF'
#!/bin/bash
if [ "$EUID" -ne 0 ]; then
  echo "❌ Please run as root"
  exit 1
fi

INSTALL_DIR="/opt/pulseone"
echo "Installing to $INSTALL_DIR..."

# Create User
id -u pulseone &>/dev/null || useradd -r -s /bin/false pulseone

# Create Directories
mkdir -p "$INSTALL_DIR"
cp -r * "$INSTALL_DIR/"
chown -R pulseone:pulseone "$INSTALL_DIR"

# Install Dependencies (Ubuntu/Debian)
if command -v apt-get &> /dev/null; then
    apt-get update
    apt-get install -y nodejs npm redis-server libsqlite3-0 libcurl4
elif command -v yum &> /dev/null; then
    yum install -y nodejs redis libsqlite3 libcurl
fi

# Install Node Modules
cd "$INSTALL_DIR/backend"
npm install --production --unsafe-perm

# Generate Systemd Service
cat > /etc/systemd/system/pulseone-backend.service << SERVICE_EOF
[Unit]
Description=PulseOne Backend
After=network.target redis.service

[Service]
ExecStart=/usr/bin/node $INSTALL_DIR/backend/app.js
WorkingDirectory=$INSTALL_DIR/backend
Environment=NODE_ENV=production
Restart=always
User=pulseone
Group=pulseone
# [OPTIMIZED] Resource Limits for Raspberry Pi
MemoryHigh=512M
MemoryMax=768M
CPUWeight=100
TasksMax=100

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
Environment=LD_LIBRARY_PATH=$INSTALL_DIR/collector:/usr/local/lib
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
Environment=LD_LIBRARY_PATH=$INSTALL_DIR/collector:/usr/local/lib
Restart=always
User=pulseone
Group=pulseone
# [OPTIMIZED] Resource Limits and HA
MemoryHigh=256M
MemoryMax=512M
CPUWeight=200
KillMode=process

[Install]
WantedBy=multi-user.target
SERVICE_EOF

# Enable Services
systemctl daemon-reload
systemctl enable pulseone-backend
systemctl enable pulseone-collector
systemctl enable pulseone-export-gateway
systemctl start pulseone-backend
systemctl start pulseone-collector
systemctl start pulseone-export-gateway

echo "✅ Installation Complete! Web UI: http://localhost:3000"
INSTALL_EOF

chmod +x install.sh

# Generate uninstall.sh
cat > uninstall.sh << 'UNINSTALL_EOF'
#!/bin/bash
if [ "$EUID" -ne 0 ]; then
  echo "❌ Please run as root"
  exit 1
fi

INSTALL_DIR="/opt/pulseone"
echo "Cleaning up PulseOne services..."

# Stop and Disable Services
SERVICES=("pulseone-backend" "pulseone-collector" "pulseone-export-gateway")

for service in "${SERVICES[@]}"; do
    if systemctl is-active --quiet "$service"; then
        echo "Stopping $service..."
        systemctl stop "$service"
    fi
    if [ -f "/etc/systemd/system/$service.service" ]; then
        echo "Removing $service..."
        systemctl disable "$service"
        rm "/etc/systemd/system/$service.service"
    fi
done

systemctl daemon-reload

echo "Services removed successfully."
echo "Note: The installation directory ($INSTALL_DIR) was not removed."
echo "To remove all data, run: rm -rf $INSTALL_DIR"
UNINSTALL_EOF

chmod +x uninstall.sh

echo "✅ Build Complete: dist_linux/$PACKAGE_NAME"
# [OPTIMIZED] Native deployment package with full lifecycle support (install/uninstall)
