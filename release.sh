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
    echo "  --all         Build all platform packages"
    echo "  --skip-ui     Skip frontend build (use existing dist)"
    exit 1
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --windows) TARGET_OS="windows"; shift ;;
        --linux) TARGET_OS="linux"; shift ;;
        --all) TARGET_OS="all"; shift ;;
        --skip-ui) SKIP_FRONTEND=true; shift ;;
        *) usage ;;
    esac
done

if [ -z "$TARGET_OS" ]; then usage; fi

# 1. Frontend Build (Shared across all platforms)
if [ "$SKIP_FRONTEND" = "true" ]; then
    echo "🎨 Skipping frontend build..."
else
    echo "🎨 Building frontend once for all platforms..."
    cd "$PROJECT_ROOT/frontend"
    npm install --silent
    npm run build
    cd "$PROJECT_ROOT"
fi

# 2. Platform Dispatch
case $TARGET_OS in
    windows)
        echo "🪟 Starting Windows Deployment..."
        export SKIP_FRONTEND=true # Already built
        ./deploy-windows.sh
        ;;
    linux)
        echo "🐧 Starting Linux Deployment..."
        export SKIP_FRONTEND=true # Already built
        ./deploy-linux.sh
        ;;
    all)
        echo "🪟 Starting Windows Deployment..."
        SKIP_FRONTEND=true ./deploy-windows.sh
        echo "🐧 Starting Linux Deployment..."
        SKIP_FRONTEND=true ./deploy-linux.sh
        ;;
esac

echo "================================================================="
echo "✅ Unified Release Process Completed for $TARGET_OS"
echo "================================================================="
