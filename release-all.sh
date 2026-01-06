#!/bin/bash

# =============================================================================
# PulseOne Master Release & Packaging Script v1.0
# Multi-platform: Linux (Bare-metal/RPi), Windows (Native), Docker
# =============================================================================

set -e

PROJECT_ROOT=$(pwd)
RELEASE_DIR="$PROJECT_ROOT/release"
VERSION=$(date '+%Y.%m.%d')
TIMESTAMP=$(date '+%H%M%S')
PACKAGE_VERSION="${VERSION}-${TIMESTAMP}"

echo "================================================================="
echo "🚀 PulseOne Master Release Script v1.0"
echo "Target: Linux Native, Windows Native, Docker"
echo "Version: $PACKAGE_VERSION"
echo "================================================================="

# 1. Prepare Release Directory
rm -rf "$RELEASE_DIR"
mkdir -p "$RELEASE_DIR/linux-baremetal"
mkdir -p "$RELEASE_DIR/windows-native"
mkdir -p "$RELEASE_DIR/docker-compose"

# =============================================================================
# 2. Build Phase
# =============================================================================

echo "🎨 Step 2: Building Frontend..."
cd "$PROJECT_ROOT/frontend"
npm install --silent
npm run build --silent
echo "✅ Frontend build completed"

echo "⚙️ Step 2.1: Building Collector for Linux..."
cd "$PROJECT_ROOT/core/collector"
make clean > /dev/null
make release > /dev/null
echo "✅ Collector Linux binary ready"

echo "⚙️ Step 2.2: Building Collector for Windows (via Docker)..."
cd "$PROJECT_ROOT"
./deploy-windows.sh # Utilize existing script for Windows build logic
echo "✅ Collector Windows binary ready"

# =============================================================================
# 3. Packaging Phase - Linux Bare-metal (Raspberry Pi optimized)
# =============================================================================
echo "📦 Step 3: Packaging Linux Bare-metal Track..."
LINUX_PKG="$RELEASE_DIR/linux-baremetal"
mkdir -p "$LINUX_PKG/bin" "$LINUX_PKG/scripts" "$LINUX_PKG/config" "$LINUX_PKG/frontend"

cp "$PROJECT_ROOT/core/collector/bin/pulseone-collector" "$LINUX_PKG/bin/"
cp "$PROJECT_ROOT/core/collector/scripts/install-service.sh" "$LINUX_PKG/scripts/"
cp "$PROJECT_ROOT/core/collector/scripts/pulseone-collector.service" "$LINUX_PKG/scripts/"
cp "$PROJECT_ROOT/core/collector/scripts/DEPLOYMENT.md" "$LINUX_PKG/"
cp -r "$PROJECT_ROOT/frontend/dist"/* "$LINUX_PKG/frontend/"
cp -r "$PROJECT_ROOT/config"/* "$LINUX_PKG/config/"
cp -r "$PROJECT_ROOT/backend" "$LINUX_PKG/"
rm -rf "$LINUX_PKG/backend/node_modules"

cd "$RELEASE_DIR"
tar -czf "pulseone-linux-baremetal-${PACKAGE_VERSION}.tar.gz" linux-baremetal/
echo "✅ Linux Bare-metal package created"

# =============================================================================
# 4. Packaging Phase - Windows Native
# =============================================================================
echo "📦 Step 4: Packaging Windows Native Track..."
WIN_PKG="$RELEASE_DIR/windows-native"
mkdir -p "$WIN_PKG/bin" "$WIN_PKG/scripts" "$WIN_PKG/config" "$WIN_PKG/frontend"

# Assuming deploy-windows.sh put output in dist/PulseOne_Complete_Deploy
if [ -f "$PROJECT_ROOT/dist/PulseOne_Complete_Deploy/collector.exe" ]; then
    cp "$PROJECT_ROOT/dist/PulseOne_Complete_Deploy/collector.exe" "$WIN_PKG/bin/"
fi
cp "$PROJECT_ROOT/core/collector/scripts/register-windows-service.ps1" "$WIN_PKG/scripts/"
cp "$PROJECT_ROOT/core/collector/scripts/DEPLOYMENT.md" "$WIN_PKG/"
cp -r "$PROJECT_ROOT/frontend/dist"/* "$WIN_PKG/frontend/"
cp -r "$PROJECT_ROOT/config"/* "$WIN_PKG/config/"
cp -r "$PROJECT_ROOT/backend" "$WIN_PKG/"

# Include installation helpers from deploy-windows.sh if available
if [ -f "$PROJECT_ROOT/dist/PulseOne_Complete_Deploy/install.bat" ]; then
    cp "$PROJECT_ROOT/dist/PulseOne_Complete_Deploy/install.bat" "$WIN_PKG/"
    cp "$PROJECT_ROOT/dist/PulseOne_Complete_Deploy/start.bat" "$WIN_PKG/"
fi

cd "$RELEASE_DIR"
zip -r "pulseone-windows-native-${PACKAGE_VERSION}.zip" windows-native/ > /dev/null
echo "✅ Windows Native package created"

# =============================================================================
# 5. Packaging Phase - Docker Compose Ready
# =============================================================================
echo "📦 Step 5: Packaging Docker Track..."
DOCKER_PKG="$RELEASE_DIR/docker-compose"
cp "$PROJECT_ROOT/docker-compose.dev.yml" "$DOCKER_PKG/docker-compose.yml"
cp "$PROJECT_ROOT/config/.env.dev" "$DOCKER_PKG/.env.template"
# Update docker-compose for production if needed here

cd "$RELEASE_DIR"
tar -czf "pulseone-docker-ready-${PACKAGE_VERSION}.tar.gz" docker-compose/
echo "✅ Docker package created"

echo "================================================================="
echo "🎉 All packages are ready in $RELEASE_DIR"
ls -lh "$RELEASE_DIR"/*.tar.gz "$RELEASE_DIR"/*.zip
echo "================================================================="
