#!/bin/bash

# =============================================================================
# ConfigManager 테스트 실행 스크립트
# collector/scripts/run_config_test.sh
# =============================================================================

set -e  # 에러 발생 시 스크립트 중단

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# 로그 함수들
log_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

log_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

log_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

log_error() {
    echo -e "${RED}❌ $1${NC}"
}

log_section() {
    echo
    echo -e "${PURPLE}${BOLD}🔧 $1${NC}"
    echo -e "${PURPLE}$(printf '=%.0s' $(seq 1 ${#1}))${NC}"
}

# 스크립트 시작
echo -e "${CYAN}${BOLD}"
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                ConfigManager 테스트 실행기                  ║"
echo "║                   자동 빌드 및 검증                         ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo -e "${NC}"

# =============================================================================
# 1. 환경 확인
# =============================================================================

log_section "환경 확인"

# 현재 디렉토리 확인
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

log_info "스크립트 위치: $SCRIPT_DIR"
log_info "프로젝트 루트: $PROJECT_ROOT"

# collector 디렉토리로 이동
cd "$PROJECT_ROOT"

# 필수 파일들 확인
REQUIRED_FILES=(
    "include/Utils/ConfigManager.h"
    "src/Utils/ConfigManager.cpp"
    "include/Utils/LogManager.h"
    "src/Utils/LogManager.cpp"
    "include/Common/UnifiedCommonTypes.h"
)

log_info "필수 파일 확인 중..."
for file in "${REQUIRED_FILES[@]}"; do
    if [ -f "$file" ]; then
        log_success "✓ $file"
    else
        log_error "✗ $file (필수 파일 누락)"
        echo -e "${RED}누락된 파일이 있습니다. collector 디렉토리에서 실행하고 있는지 확인하세요.${NC}"
        exit 1
    fi
done

# 컴파일러 확인
if command -v g++ &> /dev/null; then
    GCC_VERSION=$(g++ --version | head -n1)
    log_success "컴파일러 확인: $GCC_VERSION"
else
    log_error "g++ 컴파일러를 찾을 수 없습니다."
    echo -e "${RED}C++ 컴파일러를 설치해주세요: sudo apt-get install g++${NC}"
    exit 1
fi

# C++17 지원 확인
log_info "C++17 지원 확인 중..."
echo '#include <filesystem>' > /tmp/cpp17_test.cpp
echo 'int main() { return 0; }' >> /tmp/cpp17_test.cpp
if g++ -std=c++17 /tmp/cpp17_test.cpp -o /tmp/cpp17_test 2>/dev/null; then
    log_success "C++17 지원 확인됨"
    rm -f /tmp/cpp17_test.cpp /tmp/cpp17_test
else
    log_error "C++17을 지원하지 않는 컴파일러입니다."
    exit 1
fi

# =============================================================================
# 2. 테스트 환경 준비
# =============================================================================

log_section "테스트 환경 준비"

# 테스트 디렉토리 생성
TEST_DIR="./tests"
BUILD_DIR="./build/tests"

mkdir -p "$TEST_DIR"
mkdir -p "$BUILD_DIR"

log_success "테스트 디렉토리 생성: $TEST_DIR"
log_success "빌드 디렉토리 생성: $BUILD_DIR"

# 기존 테스트 바이너리 정리
if [ -f "$BUILD_DIR/test_config_manager" ]; then
    rm -f "$BUILD_DIR/test_config_manager"
    log_info "기존 테스트 바이너리 제거"
fi

# 테스트 소스 파일 확인
TEST_SOURCE="$TEST_DIR/test_config_manager.cpp"

if [ ! -f "$TEST_SOURCE" ]; then
    log_warning "테스트 소스 파일이 없습니다."
    log_info "기본 테스트 코드를 생성하시겠습니까? (y/N)"
    read -r response
    if [[ "$response" =~ ^[Yy]$ ]]; then
        log_info "기본 테스트 파일을 작성해주세요..."
        log_warning "테스트 코드가 artifacts에 준비되어 있습니다."
        exit 1
    else
        log_error "테스트 소스 파일이 필요합니다."
        exit 1
    fi
fi

log_success "테스트 소스 파일 확인: $TEST_SOURCE"

# =============================================================================
# 3. 컴파일
# =============================================================================

log_section "컴파일"

# 컴파일 설정
CXX="g++"
CXXFLAGS="-std=c++17 -Wall -Wextra -g -O0"
INCLUDES="-Iinclude"
LIBS="-lpthread"

# 소스 파일들
SOURCES=(
    "src/Utils/ConfigManager.cpp"
    "src/Utils/LogManager.cpp"
    "tests/test_config_manager.cpp"
)

# 존재하는 소스 파일만 포함
EXISTING_SOURCES=()
for src in "${SOURCES[@]}"; do
    if [ -f "$src" ]; then
        EXISTING_SOURCES+=("$src")
        log_success "소스 파일 포함: $src"
    else
        log_warning "소스 파일 없음: $src (건너뜀)"
    fi
done

if [ ${#EXISTING_SOURCES[@]} -eq 0 ]; then
    log_error "컴파일할 소스 파일이 없습니다."
    exit 1
fi

# 컴파일 실행
log_info "컴파일 중..."
COMPILE_CMD="$CXX $CXXFLAGS $INCLUDES ${EXISTING_SOURCES[*]} $LIBS -o $BUILD_DIR/test_config_manager"

echo -e "${YELLOW}실행 명령어: $COMPILE_CMD${NC}"

if $COMPILE_CMD; then
    log_success "컴파일 성공!"
else
    log_error "컴파일 실패!"
    echo -e "${RED}컴파일 명령어를 확인하고 오류를 수정해주세요.${NC}"
    exit 1
fi

# 실행 파일 확인
if [ -f "$BUILD_DIR/test_config_manager" ]; then
    FILE_SIZE=$(stat -f%z "$BUILD_DIR/test_config_manager" 2>/dev/null || stat -c%s "$BUILD_DIR/test_config_manager" 2>/dev/null || echo "unknown")
    log_success "실행 파일 생성: $BUILD_DIR/test_config_manager ($FILE_SIZE bytes)"
else
    log_error "실행 파일이 생성되지 않았습니다."
    exit 1
fi

# =============================================================================
# 4. 테스트 실행
# =============================================================================

log_section "테스트 실행"

log_info "ConfigManager 테스트를 시작합니다..."
echo

# 테스트 실행
if "$BUILD_DIR/test_config_manager"; then
    echo
    log_success "테스트 완료!"
else
    echo
    log_error "테스트 실행 중 오류가 발생했습니다."
    exit 1
fi

# =============================================================================
# 5. 결과 정리
# =============================================================================

log_section "정리"

# 임시 파일 정리
if [ -d "./test_config" ]; then
    rm -rf "./test_config"
    log_info "임시 테스트 설정 디렉토리 정리"
fi

# 성공 메시지
echo
echo -e "${GREEN}${BOLD}🎉 ConfigManager 테스트가 성공적으로 완료되었습니다!${NC}"
echo
echo -e "${CYAN}추가 테스트 옵션:${NC}"
echo "  • 메모리 누수 체크: valgrind $BUILD_DIR/test_config_manager"
echo "  • 성능 프로파일링: time $BUILD_DIR/test_config_manager"
echo "  • 정적 분석: cppcheck --enable=all src/Utils/ConfigManager.cpp"
echo

# 실행 파일 유지 여부 확인
echo -e "${YELLOW}테스트 실행 파일을 유지하시겠습니까? (y/N)${NC}"
read -r keep_binary
if [[ ! "$keep_binary" =~ ^[Yy]$ ]]; then
    rm -f "$BUILD_DIR/test_config_manager"
    log_info "테스트 실행 파일 제거"
fi

log_success "스크립트 실행 완료!"