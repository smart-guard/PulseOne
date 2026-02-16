#!/bin/bash
set -e

# =============================================================================
# PulseOne Master Release Orchestrator
# =============================================================================

PROJECT_ROOT=$(pwd)
VERSION=$(grep '"version"' "$PROJECT_ROOT/version.json" | cut -d'"' -f4 || echo "6.1.0")

echo "================================================================="
echo "🚀 PulseOne Unified Release v$VERSION"
echo "================================================================="

# Command line arguments
TARGET_OS=""
SKIP_FRONTEND=false

usage() {
    echo "Usage: ./release.sh [options]"
    echo "Options:"
    echo "  --windows     Build Windows deployment package"
    echo "  --linux       Build Linux deployment package"
    echo "  --docker      Build Docker container images and export tars"
    echo "  --all         Build all platform packages (Windows, Linux, Docker)"
    echo "  --skip-ui     Skip frontend build (use existing dist)"
    exit 1
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --windows) TARGET_OS="windows"; shift ;;
        --linux) TARGET_OS="linux"; shift ;;
        --docker) TARGET_OS="docker"; shift ;;
        --all) TARGET_OS="all"; shift ;;
        --skip-ui) SKIP_FRONTEND=true; shift ;;
        *) usage ;;
    esac
done

if [ -z "$TARGET_OS" ]; then usage; fi

# 1. Frontend Build (Dockerized)
if [ "$SKIP_FRONTEND" = "true" ]; then
    echo "🎨 Skipping frontend build..."
else
    echo "🎨 Building frontend inside Docker (node:22-alpine)..."
    # Ensure dist is clean before Docker build
    rm -rf "$PROJECT_ROOT/frontend/dist"
    
    docker run --rm \
        -v "$PROJECT_ROOT/frontend:/app" \
        -w /app \
        node:22-alpine sh -c "npm install --silent && npm run build"
    
    if [ ! -d "$PROJECT_ROOT/frontend/dist" ]; then
        echo "❌ Frontend build failed (dist/ not found)"
        exit 1
    fi
    echo "✅ Frontend build completed in Docker"
fi

# 2. Platform Dispatch
case $TARGET_OS in
    windows)
        echo "🪟 Starting Windows Deployment..."
        SKIP_FRONTEND=true ./deploy-windows.sh
        ;;
    linux)
        echo "🐧 Starting Linux Deployment..."
        SKIP_FRONTEND=true ./deploy-linux.sh
        ;;
    docker)
        echo "🐳 Starting Docker Container Deployment..."
        ./deploy-docker.sh
        ;;
    all)
        echo "🌐 Starting Full Platform Release (Win, Linux, Docker)..."
        echo "1/3 🪟 Windows..."
        SKIP_FRONTEND=true ./deploy-windows.sh
        echo "2/3 🐧 Linux..."
        SKIP_FRONTEND=true ./deploy-linux.sh
        echo "3/3 🐳 Docker..."
        ./deploy-docker.sh
        ;;
esac

echo "================================================================="
echo "✅ Unified Release Process Completed for $TARGET_OS"
echo "================================================================="
