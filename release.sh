#!/bin/bash
set -e

# =============================================================================
# PulseOne Master Release Orchestrator v7.0
# =============================================================================

PROJECT_ROOT=$(pwd)
VERSION=$(grep '"version"' "$PROJECT_ROOT/version.json" | cut -d'"' -f4 || echo "6.1.0")

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
    echo "  --linux       Build Linux deployment package"
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
    echo "🪟 Windows: running deploy-windows.sh inside pulseone-windows-builder..."
    docker run --rm \
        -v "$PROJECT_ROOT:/workspace" \
        -w /workspace \
        -e PROJECT_ROOT=/workspace \
        -e SKIP_FRONTEND=true \
        -e SKIP_BUILD="$SKIP_BUILD" \
        pulseone-windows-builder bash /workspace/deploy-windows.sh
}

run_linux() {
    echo "🐧 Linux: running deploy-linux.sh inside Docker (Alpine + docker CLI)..."
    docker run --rm \
        -v "$PROJECT_ROOT:/workspace" \
        -v /var/run/docker.sock:/var/run/docker.sock \
        -w /workspace \
        -e PROJECT_ROOT=/workspace \
        -e HOST_PROJECT_ROOT="$PROJECT_ROOT" \
        -e SKIP_FRONTEND=true \
        -e SKIP_BUILD="$SKIP_BUILD" \
        alpine:latest sh -c "apk add --no-cache bash rsync zip docker-cli >/dev/null 2>&1 && bash /workspace/deploy-linux.sh"
}

run_docker() {
    echo "🐳 Docker: running deploy-docker.sh..."
    bash "$PROJECT_ROOT/deploy-docker.sh"
}

case $TARGET_OS in
    windows) run_windows ;;
    linux)   run_linux ;;
    docker)  run_docker ;;
    all)
        run_windows
        run_linux
        run_docker
        ;;
esac

echo "================================================================="
echo "✅ Release complete: $TARGET_OS"
echo "================================================================="
