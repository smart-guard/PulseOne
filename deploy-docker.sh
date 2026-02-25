#!/bin/bash
set -e

# =============================================================================
# PulseOne Docker Deploy Script v8.0
# 실행 환경: macOS/Linux 호스트 (docker 명령 필요)
# 운영용 Docker 이미지 빌드 → tar 내보내기 → 오프라인 배포 패키지
#
# 사용법:
#   ./deploy-docker.sh                    # 전체 빌드 + 패키징
#   ./deploy-docker.sh --skip-backend     # backend 이미지 스킵
#   ./deploy-docker.sh --skip-collector   # collector 이미지 스킵
#   ./deploy-docker.sh --skip-gateway     # gateway 이미지 스킵
#   ./deploy-docker.sh --skip-infra       # 인프라 이미지(redis/influx/mosquitto) 스킵
#   ./deploy-docker.sh --no-package       # tar.gz 패키징 없이 이미지만 빌드
# =============================================================================

PROJECT_ROOT=$(cd "$(dirname "$0")" && pwd)
VERSION=$(grep '"version"' "$PROJECT_ROOT/version.json" | cut -d'"' -f4 2>/dev/null || echo "6.1.0")
TIMESTAMP=$(TZ=KST-9 date '+%Y%m%d_%H%M%S')

SKIP_BACKEND=false
SKIP_COLLECTOR=false
SKIP_GATEWAY=false
SKIP_INFRA=false
NO_PACKAGE=false

for arg in "$@"; do
    case "$arg" in
        --skip-backend)    SKIP_BACKEND=true ;;
        --skip-collector)  SKIP_COLLECTOR=true ;;
        --skip-gateway)    SKIP_GATEWAY=true ;;
        --skip-infra)      SKIP_INFRA=true ;;
        --no-package)      NO_PACKAGE=true ;;
    esac
done

echo "================================================================="
echo "🐳 PulseOne Docker Deploy v$VERSION"
echo "   skip: backend=$SKIP_BACKEND  collector=$SKIP_COLLECTOR  gateway=$SKIP_GATEWAY"
echo "         infra=$SKIP_INFRA"
echo "================================================================="

# =============================================================================
# [1/3] Backend 이미지 (Frontend UI 포함)
# =============================================================================
if [ "$SKIP_BACKEND" = "false" ]; then
    echo ""
    echo "📦 [1/3] Backend 이미지 빌드 중..."

    # Frontend 빌드 (dist/ 없으면 자동 빌드)
    if [ ! -d "$PROJECT_ROOT/frontend/dist" ]; then
        echo "   🎨 Frontend 빌드 중..."
        (cd "$PROJECT_ROOT/frontend" && npm install --silent && npm run build)
    fi

    docker build --no-cache \
        -f "$PROJECT_ROOT/backend/Dockerfile.prod" \
        -t "pulseone-backend:${VERSION}" \
        -t "pulseone-backend:latest" \
        "$PROJECT_ROOT"
    echo "   ✅ Backend 이미지 완료"
else
    echo "⏭️  [1/3] Backend 스킵"
fi

# =============================================================================
# [2/3] Collector 이미지
# =============================================================================
if [ "$SKIP_COLLECTOR" = "false" ]; then
    echo ""
    echo "📦 [2/3] Collector 이미지 빌드 중..."
    if [ -f "$PROJECT_ROOT/core/collector/Dockerfile.prod" ]; then
        docker build --no-cache \
            -f "$PROJECT_ROOT/core/collector/Dockerfile.prod" \
            -t "pulseone-collector:${VERSION}" \
            -t "pulseone-collector:latest" \
            "$PROJECT_ROOT"
    elif [ -f "$PROJECT_ROOT/collector/Dockerfile.prod" ]; then
        docker build --no-cache \
            -f "$PROJECT_ROOT/collector/Dockerfile.prod" \
            -t "pulseone-collector:${VERSION}" \
            -t "pulseone-collector:latest" \
            "$PROJECT_ROOT"
    else
        echo "   ⚠️  Collector Dockerfile.prod 없음 - 스킵"
    fi
    echo "   ✅ Collector 이미지 완료"
else
    echo "⏭️  [2/3] Collector 스킵"
fi

# =============================================================================
# [3/3] Export Gateway 이미지
# =============================================================================
if [ "$SKIP_GATEWAY" = "false" ]; then
    echo ""
    echo "📦 [3/3] Export Gateway 이미지 빌드 중..."
    if [ -f "$PROJECT_ROOT/core/export-gateway/Dockerfile.prod" ]; then
        docker build --no-cache \
            -f "$PROJECT_ROOT/core/export-gateway/Dockerfile.prod" \
            -t "pulseone-gateway:${VERSION}" \
            -t "pulseone-gateway:latest" \
            "$PROJECT_ROOT"
    else
        echo "   ⚠️  Gateway Dockerfile.prod 없음 - 스킵"
    fi
    echo "   ✅ Gateway 이미지 완료"
else
    echo "⏭️  [3/3] Gateway 스킵"
fi

echo ""
echo "================================================================="
echo "✅ Docker 이미지 빌드 완료"
echo "================================================================="

# =============================================================================
# 패키징 (--no-package 없을 때만)
# =============================================================================
if [ "$NO_PACKAGE" = "false" ]; then
    PACKAGE_NAME="PulseOne_Docker-v${VERSION}_${TIMESTAMP}"
    DIST_DIR="$PROJECT_ROOT/dist_docker"
    PACKAGE_DIR="$DIST_DIR/$PACKAGE_NAME"
    IMAGE_DIR="$PACKAGE_DIR/images"
    mkdir -p "$IMAGE_DIR" "$PACKAGE_DIR/config" "$PACKAGE_DIR/data/db" \
             "$PACKAGE_DIR/data/logs" "$PACKAGE_DIR/data/sql"

    echo ""
    echo "💾 Docker 이미지 tar 내보내기 중..."

    # PulseOne 이미지 내보내기
    if docker image inspect "pulseone-backend:latest" > /dev/null 2>&1; then
        docker save "pulseone-backend:latest" > "$IMAGE_DIR/pulseone-backend.tar"
        echo "   ✅ pulseone-backend:latest"
    fi
    if docker image inspect "pulseone-collector:latest" > /dev/null 2>&1; then
        docker save "pulseone-collector:latest" > "$IMAGE_DIR/pulseone-collector.tar"
        echo "   ✅ pulseone-collector:latest"
    fi
    if docker image inspect "pulseone-gateway:latest" > /dev/null 2>&1; then
        docker save "pulseone-gateway:latest" > "$IMAGE_DIR/pulseone-gateway.tar"
        echo "   ✅ pulseone-gateway:latest"
    fi

    # 인프라 이미지 내보내기
    if [ "$SKIP_INFRA" = "false" ]; then
        echo "   인프라 이미지 pull & export 중..."
        for img in "redis:alpine" "influxdb:2.7" "eclipse-mosquitto:2" "rabbitmq:3-management"; do
            docker pull "$img" > /dev/null 2>&1 || true
            FNAME=$(echo "$img" | tr ':/' '-')
            docker save "$img" > "$IMAGE_DIR/${FNAME}.tar" 2>/dev/null && \
                echo "   ✅ $img" || echo "   ⚠️  $img 내보내기 실패"
        done
    fi

    # Compose 파일 복사
    echo ""
    echo "📝 설정 파일 복사 중..."
    if [ -f "$PROJECT_ROOT/docker/docker-compose.prod.yml" ]; then
        cp "$PROJECT_ROOT/docker/docker-compose.prod.yml" "$PACKAGE_DIR/docker-compose.yml"
    fi

    # Config 복사
    if [ -d "$PROJECT_ROOT/config" ]; then
        rsync -a --exclude='secrets' "$PROJECT_ROOT/config/" "$PACKAGE_DIR/config/" 2>/dev/null || \
            cp "$PROJECT_ROOT/config/"*.env "$PACKAGE_DIR/config/" 2>/dev/null || true
    fi

    # SQL 복사
    cp "$PROJECT_ROOT/data/sql/schema.sql" "$PACKAGE_DIR/data/sql/" 2>/dev/null || true
    cp "$PROJECT_ROOT/data/sql/seed.sql"   "$PACKAGE_DIR/data/sql/" 2>/dev/null || true

    # 사전 시드 DB 생성
    if command -v sqlite3 > /dev/null 2>&1; then
        echo "🗄️  사전 시드 DB 생성 중..."
        SEED_DB="$PACKAGE_DIR/data/db/pulseone.db"
        sqlite3 "$SEED_DB" < "$PROJECT_ROOT/data/sql/schema.sql" && \
        sqlite3 "$SEED_DB" < "$PROJECT_ROOT/data/sql/seed.sql" && \
        echo "   ✅ 시드 DB 완료" || echo "   ⚠️  시드 DB 실패"
    fi

    # ==========================================================================
    # setup.sh (Linux/Mac용)
    # ==========================================================================
    cat > "$PACKAGE_DIR/setup.sh" << 'SETUP_EOF'
#!/bin/bash
set -e
echo "==========================================="
echo " 🐳 PulseOne Docker 설치"
echo "==========================================="

command -v docker > /dev/null 2>&1 || { echo "❌ Docker가 설치되어 있지 않습니다."; exit 1; }

echo ""
echo "📥 Docker 이미지 로드 중..."
for img in images/*.tar; do
    [ -f "$img" ] || continue
    echo "   Loading $(basename $img)..."
    docker load < "$img"
done

echo ""
echo "🚀 서비스 시작 중..."
docker compose up -d

echo ""
echo "==========================================="
echo " ✅ PulseOne이 시작되었습니다!"
echo "    Web UI: http://localhost:3000"
echo "    로그:   docker compose logs -f"
echo "==========================================="
SETUP_EOF
    chmod +x "$PACKAGE_DIR/setup.sh"

    # ==========================================================================
    # stop.sh
    # ==========================================================================
    cat > "$PACKAGE_DIR/stop.sh" << 'STOP_EOF'
#!/bin/bash
echo "Stopping PulseOne..."
docker compose down
echo "✅ 모든 서비스가 중지되었습니다."
STOP_EOF
    chmod +x "$PACKAGE_DIR/stop.sh"

    # ==========================================================================
    # reset.sh (데이터 초기화)
    # ==========================================================================
    cat > "$PACKAGE_DIR/reset.sh" << 'RESET_EOF'
#!/bin/bash
echo "==========================================="
echo " ⚠️  PulseOne 데이터 초기화 경고"
echo "==========================================="
echo ""
echo " 이 작업을 실행하면 모든 데이터가 삭제됩니다:"
echo "   - data/db/pulseone.db  (설정 및 운영 데이터)"
echo "   - Docker volumes       (시계열, Redis 캐시)"
echo ""
read -p "초기화하려면 Y를 입력하세요 (Y/N): " CONFIRM
if [[ "$CONFIRM" != "Y" && "$CONFIRM" != "y" ]]; then
    echo "취소되었습니다."
    exit 0
fi

echo ""
echo "서비스 중지 중..."
docker compose down -v

echo "로컬 데이터 삭제 중..."
rm -f data/db/pulseone.db data/db/pulseone.db-wal data/db/pulseone.db-shm
rm -rf data/logs/*

echo "기본 DB 복원 중..."
if [ -f "data/db/pulseone_default.db" ]; then
    cp data/db/pulseone_default.db data/db/pulseone.db
    echo "✅ 기본 DB 복원 완료"
else
    echo "⚠️  기본 DB 없음 - 서비스 시작 시 자동 초기화됩니다"
fi

echo ""
echo "✅ 초기화 완료! setup.sh 또는 docker compose up -d 로 재시작하세요."
RESET_EOF
    chmod +x "$PACKAGE_DIR/reset.sh"

    # ==========================================================================
    # setup.bat (Windows Docker Desktop용)
    # ==========================================================================
    cat > "$PACKAGE_DIR/setup.bat" << 'BAT_EOF'
@echo off
chcp 65001 >nul
echo === PulseOne Docker Setup ===
echo.
echo Loading Docker images...
for %%f in (images\*.tar) do (
    echo   Loading %%f...
    docker load < "%%f"
)
echo.
echo Starting services...
docker compose up -d
echo.
echo Done. Open http://localhost:3000
pause
BAT_EOF

    echo "✅ 스크립트 생성 완료 (setup/stop/reset)"

    # ==========================================================================
    # TAR.GZ 패키징
    # ==========================================================================
    echo ""
    echo "📦 TAR.GZ 패키징 중..."
    cd "$DIST_DIR"
    tar -czf "${PACKAGE_NAME}.tar.gz" "$PACKAGE_NAME/"

    echo ""
    echo "================================================================="
    echo "✅ Docker 배포 패키지 완료"
    echo "   📦 $DIST_DIR/${PACKAGE_NAME}.tar.gz"
    echo "   📐 $(du -sh "${PACKAGE_NAME}.tar.gz" | cut -f1)"
    echo ""
    echo "   사용법:"
    echo "     1. 대상 서버에 tar.gz 복사 및 압축 해제"
    echo "     2. cd $PACKAGE_NAME && ./setup.sh"
    echo "================================================================="
fi
