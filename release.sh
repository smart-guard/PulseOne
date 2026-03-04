#!/bin/bash
set -e

# =============================================================================
# PulseOne Master Release Orchestrator v8.0
# =============================================================================

PROJECT_ROOT=$(pwd)
VERSION=$(grep '"version"' "$PROJECT_ROOT/version.json" | cut -d'"' -f4 || echo "6.1.0")
HOST_TIMESTAMP=$(date '+%Y%m%d_%H%M%S')

echo "================================================================="
echo "🚀 PulseOne Unified Release v$VERSION"
echo "================================================================="

TARGET_OS=""
SKIP_FRONTEND=false
SKIP_BUILD=false

usage() {
    echo "Usage: ./release.sh [options]"
    echo "Options:"
    echo "  --windows     Build Windows deployment package"
    echo "  --linux       Build Linux (x64) deployment package"
    echo "  --rpi         Build Raspberry Pi (ARM64) deployment package"
    echo "  --docker      Build Docker container images"
    echo "  --all         Build all platforms"
    echo "  --skip-ui     Skip frontend build"
    echo "  --skip-build  Skip C++ compilation (use existing binaries)"
    exit 1
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --windows)    TARGET_OS="windows";  shift ;;
        --linux)      TARGET_OS="linux";    shift ;;
        --rpi)        TARGET_OS="rpi";      shift ;;
        --docker)     TARGET_OS="docker";   shift ;;
        --all)        TARGET_OS="all";      shift ;;
        --skip-ui)    SKIP_FRONTEND=true;   shift ;;
        --skip-build) SKIP_BUILD=true;      shift ;;
        *) usage ;;
    esac
done

if [ -z "$TARGET_OS" ]; then usage; fi

# =============================================================================
# Frontend build (Docker, node:22-alpine)
# =============================================================================
if [ "$SKIP_FRONTEND" = "true" ]; then
    echo "🎨 Skipping frontend build..."
else
    echo "🎨 Building frontend (Docker: node:22-alpine)..."
    rm -rf "$PROJECT_ROOT/frontend/dist"
    docker run --rm \
        -v "$PROJECT_ROOT/frontend:/app" \
        -w /app \
        node:22-alpine sh -c "npm install --silent && npm run build"
    if [ ! -d "$PROJECT_ROOT/frontend/dist" ]; then
        echo "❌ Frontend build failed"
        exit 1
    fi
    echo "✅ Frontend built"
fi

# =============================================================================
# Platform dispatch
# =============================================================================
run_windows() {
    echo "🪟 Windows: running deploy-windows.sh inside pulseone-win-builder..."
    # 이미지 없으면 자동 빌드 (최초 1회)
    if ! docker image inspect pulseone-win-builder > /dev/null 2>&1; then
        echo "   Building pulseone-win-builder image (최초 1회)..."
        docker build -t pulseone-win-builder -f "$PROJECT_ROOT/Dockerfile.windows-builder" "$PROJECT_ROOT"
    fi
    docker run --rm \
        -v "$PROJECT_ROOT:/workspace" \
        -w /workspace \
        -e PROJECT_ROOT=/workspace \
        -e SKIP_FRONTEND=true \
        -e SKIP_BUILD="$SKIP_BUILD" \
        -e HOST_TIMESTAMP="$HOST_TIMESTAMP" \
        pulseone-win-builder bash /workspace/deploy-windows.sh
}

run_linux() {
    echo "🐧 Linux: running deploy-linux.sh inside pulseone-linux-builder..."
    # 이미지 없으면 자동 빌드 (최초 1회)
    if ! docker image inspect pulseone-linux-builder > /dev/null 2>&1; then
        echo "   Building pulseone-linux-builder image (최초 1회)..."
        docker build -t pulseone-linux-builder -f "$PROJECT_ROOT/Dockerfile.linux-builder" "$PROJECT_ROOT"
    fi
    docker run --rm \
        -v "$PROJECT_ROOT:/workspace" \
        -w /workspace \
        -e PROJECT_ROOT=/workspace \
        -e SKIP_FRONTEND=true \
        -e SKIP_BUILD="$SKIP_BUILD" \
        -e HOST_TIMESTAMP="$HOST_TIMESTAMP" \
        pulseone-linux-builder bash /workspace/deploy-linux.sh
}

run_rpi() {
    echo "🍓 RPi: running deploy-rpi.sh inside pulseone-rpi-builder (linux/arm64)..."
    # 이미지 없으면 자동 빌드 (최초 1회 — QEMU 에뮬레이션으로 ARM64 빌드)
    if ! docker image inspect pulseone-rpi-builder > /dev/null 2>&1; then
        echo "   Building pulseone-rpi-builder image (최초 1회, ARM64 — 시간이 걸립니다)..."
        docker build --platform linux/arm64 -t pulseone-rpi-builder -f "$PROJECT_ROOT/Dockerfile.rpi-builder" "$PROJECT_ROOT"
    fi
    docker run --rm --platform linux/arm64 \
        -v "$PROJECT_ROOT:/workspace" \
        -w /workspace \
        -e PROJECT_ROOT=/workspace \
        -e SKIP_FRONTEND=true \
        -e SKIP_BUILD="$SKIP_BUILD" \
        -e HOST_TIMESTAMP="$HOST_TIMESTAMP" \
        pulseone-rpi-builder bash /workspace/deploy-rpi.sh
}

run_docker() {
    echo "🐳 Docker: running deploy-docker.sh..."
    bash "$PROJECT_ROOT/deploy-docker.sh"
}

case $TARGET_OS in
    windows) run_windows ;;
    linux)   run_linux ;;
    rpi)     run_rpi ;;
    docker)  run_docker ;;
    all)
        run_windows
        run_linux
        run_rpi
        run_docker
        ;;
esac

echo "================================================================="
echo "✅ Release complete: $TARGET_OS"
echo "================================================================="
