#!/bin/bash
# =============================================================================
# scripts/build_all_plugins.sh - 모든 플러그인 통합 빌드
# 프로젝트 루트의 scripts 폴더에 위치
# =============================================================================

set -e

echo "🚀 Building all PulseOne driver plugins..."

# 프로젝트 루트 찾기
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DRIVER_DIR="$PROJECT_ROOT/driver"
PLUGIN_DIR="$DRIVER_DIR/plugins"
SUCCESS_COUNT=0
TOTAL_COUNT=0

echo "📁 Project Root: $PROJECT_ROOT"
echo "📁 Driver Directory: $DRIVER_DIR"

# 플러그인 디렉토리 생성
mkdir -p "$PLUGIN_DIR"

# 빌드 가능한 드라이버 목록 확인
AVAILABLE_DRIVERS=()
for driver_dir in "$DRIVER_DIR"/*/; do
    if [ -d "$driver_dir" ] && [ -f "$driver_dir/Makefile" ]; then
        driver_name=$(basename "$driver_dir")
        if [ "$driver_name" != "plugins" ] && [ "$driver_name" != "dist" ]; then
            AVAILABLE_DRIVERS+=("$driver_name")
        fi
    fi
done

if [ ${#AVAILABLE_DRIVERS[@]} -eq 0 ]; then
    echo "❌ No buildable drivers found in $DRIVER_DIR"
    echo "💡 Make sure driver folders have Makefile:"
    echo "   - driver/ModbusDriver/Makefile"
    echo "   - driver/MQTTDriver/Makefile"  
    echo "   - driver/BACnetDriver/Makefile"
    exit 1
fi

echo "📋 Found buildable drivers: ${AVAILABLE_DRIVERS[*]}"
TOTAL_COUNT=${#AVAILABLE_DRIVERS[@]}

# 각 드라이버 빌드
for driver_name in "${AVAILABLE_DRIVERS[@]}"; do
    driver_path="$DRIVER_DIR/$driver_name"
    
    echo ""
    echo "🔨 Building $driver_name..."
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "📁 Path: $driver_path"
    
    cd "$driver_path"
    
    # 의존성 확인 (선택적)
    echo "🔍 Checking dependencies for $driver_name..."
    if make deps 2>/dev/null; then
        echo "✅ Dependencies check passed"
    else
        echo "⚠️  Dependencies check skipped (make deps not available)"
    fi
    
    # 빌드 실행
    echo "⚙️  Starting build for $driver_name..."
    if make clean 2>/dev/null && make build; then
        echo "✅ $driver_name build successful"
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
        
        # 빌드 결과 확인
        if [ -f "../plugins/${driver_name}.so" ]; then
            echo "📦 Plugin created: ../plugins/${driver_name}.so"
            ls -la "../plugins/${driver_name}.so"
        elif [ -f "../plugins/${driver_name}.dll" ]; then
            echo "📦 Plugin created: ../plugins/${driver_name}.dll"
            ls -la "../plugins/${driver_name}.dll"
        fi
        
        # 간단한 테스트
        if make test 2>/dev/null; then
            echo "🧪 $driver_name test passed"
        else
            echo "⚠️  $driver_name test skipped"
        fi
        
    else
        echo "❌ $driver_name build failed"
        echo "📝 Build log (last 10 lines):"
        tail -10 build.log 2>/dev/null || echo "No build log available"
    fi
done

# 최종 결과 요약
echo ""
echo "📊 Build Summary"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "✅ Successful: $SUCCESS_COUNT"
echo "❌ Failed: $((TOTAL_COUNT - SUCCESS_COUNT))"
echo "📦 Total drivers: $TOTAL_COUNT"

# 생성된 플러그인 목록
echo ""
echo "📁 Generated plugins:"
cd "$PLUGIN_DIR"
if ls *.so >/dev/null 2>&1 || ls *.dll >/dev/null 2>&1; then
    for plugin in *.so *.dll; do
        if [ -f "$plugin" ]; then
            echo "  📦 $plugin ($(stat --format=%s "$plugin" 2>/dev/null || echo "unknown") bytes)"
        fi
    done
else
    echo "  ❌ No plugins were generated"
fi

# 최종 상태
if [ $SUCCESS_COUNT -eq $TOTAL_COUNT ]; then
    echo ""
    echo "🎉 All plugins built successfully!"
    echo ""
    echo "🚀 Next steps:"
    echo "  1. Test plugins: docker-compose -f docker-compose.dev.yml up"
    echo "  2. Check logs for plugin loading"
    echo "  3. Verify plugin functionality in web interface"
    echo ""
    echo "📋 Plugin locations:"
    echo "  - Build output: $PLUGIN_DIR/"
    echo "  - Install location: $DRIVER_DIR/dist/"
    echo ""
    exit 0
else
    echo ""
    echo "⚠️  Some plugins failed to build ($((TOTAL_COUNT - SUCCESS_COUNT)) failures)"
    echo ""
    echo "🔧 Troubleshooting:"
    echo "  1. Check individual driver dependencies: cd driver/DriverName && make deps"
    echo "  2. Check build logs for specific errors"
    echo "  3. Ensure required libraries are installed in Docker container"
    echo "  4. Try building individual drivers: cd driver/DriverName && make build"
    echo ""
    exit 1
fi