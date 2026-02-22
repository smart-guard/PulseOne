#!/bin/bash
set -e

# =============================================================================
# PulseOne Docker Deployment Script v7.0
# 실행 환경: macOS 호스트 (docker 명령 직접 사용)
# 운영용 Docker 이미지 빌드 및 tar 내보내기
# =============================================================================

PROJECT_ROOT=$(cd "$(dirname "$0")" && pwd)
VERSION=$(grep '"version"' "$PROJECT_ROOT/version.json" | cut -d'"' -f4 || echo "6.1.0")
TIMESTAMP=$(date '+%Y%m%d_%H%M%S')
PACKAGE_NAME="PulseOne_Docker_Deploy-v${VERSION}_${TIMESTAMP}"
DIST_DIR="$PROJECT_ROOT/dist_docker"
IMAGE_DIR="$DIST_DIR/$PACKAGE_NAME/images"

echo "================================================================="
echo "� PulseOne Docker Deployment v$VERSION"
echo "================================================================="

mkdir -p "$IMAGE_DIR"

# =============================================================================
# 1. 운영용 Docker 이미지 빌드
# =============================================================================
echo "📦 Building production images..."

echo "   [1/3] Backend (with Frontend UI)..."
docker build --no-cache -f backend/Dockerfile.prod -t "pulseone-backend:${VERSION}" .
docker tag "pulseone-backend:${VERSION}" pulseone-backend:latest

echo "   [2/3] Collector (with all drivers)..."
docker build --no-cache -f core/collector/Dockerfile.prod -t "pulseone-collector:${VERSION}" .
docker tag "pulseone-collector:${VERSION}" pulseone-collector:latest

echo "   [3/3] Export Gateway..."
docker build --no-cache -f core/export-gateway/Dockerfile.prod -t "pulseone-gateway:${VERSION}" .
docker tag "pulseone-gateway:${VERSION}" pulseone-gateway:latest

echo "✅ All images built"

# =============================================================================
# 2. 이미지 tar 내보내기 (오프라인 배포용)
# =============================================================================
echo "💾 Exporting images to tarballs..."
docker save "pulseone-backend:${VERSION}"   > "$IMAGE_DIR/pulseone-backend.tar"
docker save "pulseone-collector:${VERSION}" > "$IMAGE_DIR/pulseone-collector.tar"
docker save "pulseone-gateway:${VERSION}"   > "$IMAGE_DIR/pulseone-gateway.tar"

echo "   Exporting infrastructure images..."
docker pull redis:alpine
docker save redis:alpine > "$IMAGE_DIR/redis-alpine.tar"

docker pull influxdb:2.7
docker save influxdb:2.7 > "$IMAGE_DIR/influxdb-2.7.tar"

echo "✅ Images exported"

# =============================================================================
# 3. Compose 파일 및 설정 복사
# =============================================================================
echo "📝 Copying orchestration files..."
PACKAGE_DIR="$DIST_DIR/$PACKAGE_NAME"
mkdir -p "$PACKAGE_DIR/config"

if [ -f "$PROJECT_ROOT/docker/docker-compose.prod.yml" ]; then
    cp "$PROJECT_ROOT/docker/docker-compose.prod.yml" "$PACKAGE_DIR/docker-compose.yml"
fi

if [ -d "$PROJECT_ROOT/deploy/config" ]; then
    cp "$PROJECT_ROOT/deploy/config/"*.env "$PACKAGE_DIR/config/" 2>/dev/null || true
elif [ -d "$PROJECT_ROOT/config" ]; then
    cp "$PROJECT_ROOT/config/"*.env "$PACKAGE_DIR/config/" 2>/dev/null || true
fi

# =============================================================================
# 4. 설치 스크립트
# =============================================================================
cat > "$PACKAGE_DIR/setup.sh" << 'EOF'
#!/bin/bash
echo "=== PulseOne Docker Setup ==="
command -v docker >/dev/null 2>&1 || { echo "❌ Docker not installed"; exit 1; }
echo "Loading images..."
for img in images/*.tar; do
    echo "  Loading $img..."
    docker load < "$img"
done
echo "Starting services..."
docker-compose up -d
echo "✅ PulseOne running at http://localhost:3000"
EOF
chmod +x "$PACKAGE_DIR/setup.sh"

cat > "$PACKAGE_DIR/setup.bat" << 'EOF'
@echo off
echo === PulseOne Docker Setup ===
for %%f in (images\*.tar) do (
    echo Loading %%f...
    docker load < "%%f"
)
docker-compose up -d
echo Done. Open http://localhost:3000
pause
EOF

# =============================================================================
# 5. 최종 패키지
# =============================================================================
echo "📦 Creating final package..."
cd "$DIST_DIR"
tar -czf "${PACKAGE_NAME}.tar.gz" "$PACKAGE_NAME/"

echo "================================================================="
echo "✅ Docker Package: $DIST_DIR/${PACKAGE_NAME}.tar.gz"
echo "   Size: $(du -sh "${PACKAGE_NAME}.tar.gz" | cut -f1)"
echo "================================================================="
