#!/bin/bash
# =============================================================================
# collector/quick_build.sh
# PulseOne Collector 빠른 빌드 스크립트
# =============================================================================

set -e

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 로그 함수들
log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# 현재 디렉토리가 collector인지 확인
if [ ! -f "Makefile" ] && [ ! -f "CMakeLists.txt" ]; then
    log_error "Please run this script from the collector directory"
    exit 1
fi

# 명령행 옵션 파싱
BUILD_TYPE="release"
BUILD_SYSTEM="make"
INSTALL_DEPS=false
RUN_TESTS=false
CLEAN_BUILD=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="debug"
            shift
            ;;
        -r|--release)
            BUILD_TYPE="release"
            shift
            ;;
        -c|--cmake)
            BUILD_SYSTEM="cmake"
            shift
            ;;
        -m|--make)
            BUILD_SYSTEM="make"
            shift
            ;;
        --install-deps)
            INSTALL_DEPS=true
            shift
            ;;
        -t|--test)
            RUN_TESTS=true
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  -d, --debug      Build in debug mode"
            echo "  -r, --release    Build in release mode (default)"
            echo "  -c, --cmake      Use CMake build system"
            echo "  -m, --make       Use Make build system (default)"
            echo "  --install-deps   Install dependencies first"
            echo "  -t, --test       Run tests after build"
            echo "  --clean          Clean build before building"
            echo "  -h, --help       Show this help"
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

log_info "PulseOne Collector Quick Build"
log_info "Build Type: $BUILD_TYPE"
log_info "Build System: $BUILD_SYSTEM"

# 의존성 설치
if [ "$INSTALL_DEPS" = true ]; then
    log_info "Installing dependencies..."
    if [ -f "install_dependencies.sh" ]; then
        chmod +x install_dependencies.sh
        ./install_dependencies.sh
    else
        log_error "install_dependencies.sh not found"
        exit 1
    fi
fi

# 빌드 디렉토리 정리
if [ "$CLEAN_BUILD" = true ]; then
    log_info "Cleaning previous build..."
    if [ "$BUILD_SYSTEM" = "cmake" ]; then
        rm -rf build CMakeCache.txt CMakeFiles
    else
        make clean 2>/dev/null || true
    fi
fi

# 환경 변수 설정
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH"
export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"

# 헤더 파일 확인
log_info "Checking required headers..."
check_header() {
    local header="$1"
    if echo "#include <$header>" | g++ -x c++ -c - -o /dev/null 2>/dev/null;