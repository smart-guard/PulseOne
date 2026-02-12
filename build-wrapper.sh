#!/bin/bash
set -euo pipefail

# =============================================================================
# PulseOne Simple Deployment Wrapper
# =============================================================================

PROJECT_ROOT=$(pwd)
DIST_DIR="$PROJECT_ROOT/dist"
PACKAGE_DIR="$DIST_DIR/deploy"

echo "🔨 Preparing directory: $PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR"

echo "🚀 Running deployment script..."
./deploy-windows.sh "$@"
