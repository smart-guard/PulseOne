#!/bin/bash

# =============================================================================
# PulseOne Containerized Deployment Script v1.0
# Unified Docker-Native Packaging with Air-Gapped Support
# =============================================================================

PROJECT_ROOT=$(pwd)
DIST_DIR="$PROJECT_ROOT/dist_docker"
PACKAGE_NAME="PulseOne_Docker_Deploy"
PACKAGE_DIR="$DIST_DIR/$PACKAGE_NAME"
IMAGE_DIR="$PACKAGE_DIR/images"

VERSION="1.0.0"
TIMESTAMP=$(date '+%Y%m%d_%H%M%S')

echo "================================================================="
echo "🚀 PulseOne Containerized Deployment v$VERSION"
echo "Creating Production Docker Package..."
echo "================================================================="

# 1. Prepare Structure
rm -rf "$DIST_DIR"
mkdir -p "$IMAGE_DIR"
mkdir -p "$PACKAGE_DIR/config"
mkdir -p "$PACKAGE_DIR/data"

# 2. Build Production Images
echo "📦 Building Production Images..."

# Backend (includes Frontend)
echo "   - Building Backend (with Frontend UI)..."
docker build -f backend/Dockerfile.prod -t pulseone-backend:prod .

# Collector (with all drivers)
echo "   - Building Collector (Native Core)..."
docker build -f core/collector/Dockerfile.prod -t pulseone-collector:prod .

# Export Gateway
echo "   - Building Export Gateway..."
docker build -f core/export-gateway/Dockerfile.prod -t pulseone-export-gateway:prod .

# 3. Export Images for Offline Support
echo "💾 Exporting Images to tarballs (Offline Mode Support)..."
docker save pulseone-backend:prod > "$IMAGE_DIR/pulseone-backend.tar"
docker save pulseone-collector:prod > "$IMAGE_DIR/pulseone-collector.tar"
docker save pulseone-export-gateway:prod > "$IMAGE_DIR/pulseone-export-gateway.tar"

# Export third-party images
echo "   - Exporting Infrastructure Images..."
docker pull redis:alpine
docker save redis:alpine > "$IMAGE_DIR/redis-alpine.tar"
docker pull influxdb:2.7
docker save influxdb:2.7 > "$IMAGE_DIR/influxdb-2.7.tar"
docker pull rabbitmq:3-management
docker save rabbitmq:3-management > "$IMAGE_DIR/rabbitmq-3-mgmt.tar"

# 4. Copy Orchestration Files
echo "📝 Copying orchestration and config files..."
cp docker/docker-compose.prod.yml "$PACKAGE_DIR/docker-compose.yml"
cp config/*.env "$PACKAGE_DIR/config/" 2>/dev/null || true

# 5. Create Setup Scripts (Linux & Windows)
echo "🛠️ Creating setup scripts..."

# Linux/macOS setup.sh
cat > "$PACKAGE_DIR/setup.sh" << 'EOF'
#!/bin/bash
echo "================================================================"
echo "PulseOne Containerized Setup v1.0 (Linux/macOS)"
echo "================================================================"

if ! command -v docker &> /dev/null; then
    echo "❌ ERROR: Docker is not installed. Please install Docker first."
    exit 1
fi

echo "📦 Loading Docker images from local assets..."
for img in images/*.tar; do
    echo "   Loading $img..."
    docker load < "$img"
done

echo "🚀 Starting PulseOne services..."
docker-compose up -d

echo "✅ PulseOne is running!"
echo "➡️  Web UI: http://localhost:3000"
EOF
chmod +x "$PACKAGE_DIR/setup.sh"

# Windows setup.bat
cat > "$PACKAGE_DIR/setup.bat" << 'EOF'
@echo off
setlocal enabledelayedexpansion

echo ================================================================
echo PulseOne Containerized Setup v1.0 (Windows)
echo ================================================================

:: Check Docker
where docker >nul 2>&1
if errorlevel 1 (
    echo [!] ERROR: Docker is not installed or not in PATH.
    echo Please install Docker Desktop for Windows first.
    pause & exit /b 1
)

:: Load Images
echo [1/2] Loading Docker images from local assets...
for %%f in (images\*.tar) do (
    echo     Loading %%f...
    docker load < "%%f"
)

:: Start Services
echo [2/2] Starting PulseOne services...
docker-compose up -d

echo.
echo ================================================================
echo ✅ PulseOne is running!
echo ➡️  Web UI: http://localhost:3000
echo ================================================================
pause
EOF

# 6. Final Summary
echo "✅ Build Complete: $PACKAGE_DIR"
echo "📦 Package size: $(du -sh "$PACKAGE_DIR" | cut -f1)"
echo "🚀 Deployment: Copy $PACKAGE_NAME to target, then run ./setup.sh"
