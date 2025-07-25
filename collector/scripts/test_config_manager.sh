#!/bin/bash

# =============================================================================
# ConfigManager 테스트 실행 스크립트
# collector/scripts/test_config_manager.sh
# =============================================================================

set -e  # 에러 발생 시 스크립트 중단

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
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
    echo -e "${PURPLE}🔧 $1${NC}"
    echo -e "${PURPLE}$(printf '=%.0s' $(seq 1 ${#1}))${NC}"
}

# 스크립트 시작
echo -e "${CYAN}"
echo "🧪 ConfigManager 테스트 실행 스크립트"
echo "====================================="
echo -e "${NC}"

# 현재 디렉토리 확인
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

log_info "스크립트 위치: $SCRIPT_DIR"
log_info "프로젝트 루트: $PROJECT_ROOT"

# collector 디렉토리로 이동
cd "$PROJECT_ROOT"

if [ ! -f "include/Utils/ConfigManager.h" ]; then
    log_error "ConfigManager.h를 찾을 수 없습니다. collector 디렉토리에서 실행하세요."
    exit 1
fi

log_success "ConfigManager 헤더 파일 확인됨"

# =============================================================================
# 1. 환경 준비
# =============================================================================

log_section "환경 준비"

# 테스트 디렉토리 생성
TEST_DIR="./tests"
mkdir -p "$TEST_DIR"
log_success "테스트 디렉토리 생성: $TEST_DIR"

# 빌드 디렉토리 생성
BUILD_DIR="./build/tests"
mkdir -p "$BUILD_DIR"
log_success "빌드 디렉토리 생성: $BUILD_DIR"

# 기존 테스트 바이너리 정리
if [ -f "$TEST_DIR/test_config_manager" ]; then
    rm -f "$TEST_DIR/test_config_manager"
    log_info "기존 테스트 바이너리 제거"
fi

# =============================================================================
# 2. 의존성 확인
# =============================================================================

log_section "의존성 확인"

# 필수 소스 파일들 확인
REQUIRED_FILES=(
    "include/Utils/ConfigManager.h"
    "src/Utils/ConfigManager.cpp"
    "include/Utils/LogManager.h"
    "src/Utils/LogManager.cpp"
)

for file in "${REQUIRED_FILES[@]}"; do
    if [ -f "$file" ]; then
        log_success "✓ $file"
    else
        log_error "✗ $file (필수 파일 누락)"
        exit 1
    fi
done

# 컴파일러 확인
if command -v g++ &> /dev/null; then
    GCC_VERSION=$(g++ --version | head -n1)
    log_success "컴파일러 확인: $GCC_VERSION"
else
    log_error "g++ 컴파일러를 찾을 수 없습니다."
    exit 1
fi

# =============================================================================
# 3. 테스트 코드 생성 (아직 없다면)
# =============================================================================

log_section "테스트 코드 확인"

TEST_SOURCE="$TEST_DIR/test_config_manager.cpp"

if [ ! -f "$TEST_SOURCE" ]; then
    log_warning "테스트 소스 파일이 없습니다. 생성하시겠습니까? (y/n)"
    read -r response
    if [[ "$response" =~ ^[Yy]$ ]]; then
        log_info "테스트 소스 파일을 생성합니다..."
        # 여기서 위에서 만든 테스트 코드를 파일로 저장
        cat > "$TEST_SOURCE" << 'EOF'
// 테스트 코드가 여기에 들어갑니다
// (실제로는 위의 artifacts에서 생성한 코드를 복사)
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include <iostream>

int main() {
    std::cout << "🧪 ConfigManager 기본 테스트" << std::endl;
    
    try {
        PulseOne::LogManager::getInstance().initialize();
        
        auto& config = ConfigManager::getInstance();
        config.initialize();
        
        std::cout << "✅ ConfigManager 초기화 성공" << std::endl;
        std::cout << "✅ 기본 테스트 완료" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cout << "❌ 테스트 실패: " << e.what() << std::endl;
        return 1;
    }
}
EOF
        log_success "기본 테스트 파일 생성됨: $TEST_SOURCE"
    else
        log_error "테스트를 진행할 수 없습니다."
        exit 1
    fi
else
    log_success "테스트 소스 파일 확인: $TEST_SOURCE"
fi

# =============================================================================
# 4. 컴파일
# =============================================================================

log_section "컴파일"

# 컴파일 플래그 설정
CXXFLAGS="-std=c++17 -Wall -Wextra -g -O0"
INCLUDES="-I./include"
LIBS="-lpthread"

# 필요한 소스 파일들
SOURCES=(
    "$TEST_SOURCE"
    "src/Utils/ConfigManager.cpp"
    "src/Utils/LogManager.cpp"
)

# 데이터베이스 관련 라이브러리가 있다면 추가
if pkg-config --exists libpqxx; then
    LIBS="$LIBS -lpqxx -lpq"
    log_info "PostgreSQL 라이브러리 추가"
fi

if [ -f "/usr/include/sqlite3.h" ]; then
    LIBS="$LIBS -lsqlite3"
    log_info "SQLite 라이브러리 추가"
fi

# 컴파일 명령어 생성
COMPILE_CMD="g++ $CXXFLAGS $INCLUDES ${SOURCES[*]} $LIBS -o $TEST_DIR/test_config_manager"

log_info "컴파일 명령어:"
echo "  $COMPILE_CMD"
echo

# 컴파일 실행
log_info "컴파일 중..."
if eval "$COMPILE_CMD" 2>&1; then
    log_success "컴파일 성공!"
else
    log_error "컴파일 실패. 위의 오류 메시지를 확인하세요."
    exit 1
fi

# =============================================================================
# 5. 테스트 실행
# =============================================================================

log_section "테스트 실행"

# 테스트 바이너리 존재 확인
if [ ! -f "$TEST_DIR/test_config_manager" ]; then
    log_error "테스트 바이너리를 찾을 수 없습니다."
    exit 1
fi

# 테스트 바이너리 권한 확인
chmod +x "$TEST_DIR/test_config_manager"

# 환경 정리 (이전 테스트의 영향 제거)
unset PULSEONE_CONFIG_DIR
unset PULSEONE_DATA_DIR

log_info "테스트 실행 중..."
echo

# 테스트 실행 및 결과 캡처
if "$TEST_DIR/test_config_manager"; then
    TEST_RESULT=$?
    echo
    if [ $TEST_RESULT -eq 0 ]; then
        log_success "모든 테스트 통과!"
    else
        log_warning "일부 테스트 실패 (종료 코드: $TEST_RESULT)"
    fi
else
    TEST_RESULT=$?
    echo
    log_error "테스트 실행 실패 (종료 코드: $TEST_RESULT)"
fi

# =============================================================================
# 6. 결과 분석 및 정리
# =============================================================================

log_section "결과 분석"

# 생성된 테스트 파일들 확인
echo "📁 생성된 파일들:"
find . -name "test_config_*" -type d 2>/dev/null | head -5 | while read -r dir; do
    echo "  🗂️  $dir (테스트용 임시 디렉토리)"
done

# 바이너리 크기 확인
if [ -f "$TEST_DIR/test_config_manager" ]; then
    BINARY_SIZE=$(du -h "$TEST_DIR/test_config_manager" | cut -f1)
    log_info "테스트 바이너리 크기: $BINARY_SIZE"
fi

# 메모리 사용량 체크 (valgrind가 있다면)
if command -v valgrind &> /dev/null; then
    log_info "Valgrind를 사용한 메모리 검사를 실행하시겠습니까? (y/n)"
    read -r -t 10 response || response="n"
    if [[ "$response" =~ ^[Yy]$ ]]; then
        log_info "메모리 검사 실행 중..."
        valgrind --leak-check=brief --error-exitcode=1 "$TEST_DIR/test_config_manager" 2>&1 | tail -10
        if [ $? -eq 0 ]; then
            log_success "메모리 누수 없음"
        else
            log_warning "메모리 누수 가능성 있음 (자세한 내용은 위 출력 참조)"
        fi
    fi
fi

# =============================================================================
# 7. 정리 및 권장사항
# =============================================================================

log_section "정리 및 권장사항"

# 임시 파일 정리 여부 확인
log_info "테스트 임시 파일들을 정리하시겠습니까? (y/n)"
read -r -t 10 cleanup_response || cleanup_response="y"

if [[ "$cleanup_response" =~ ^[Yy]$ ]]; then
    # 임시 테스트 디렉토리들 정리
    find . -maxdepth 1 -name "test_config_*" -type d -exec rm -rf {} \; 2>/dev/null || true
    log_success "임시 파일 정리 완료"
else
    log_info "임시 파일들이 보존되었습니다. 수동으로 정리하세요."
fi

# 최종 결과 및 권장사항
echo
echo -e "${CYAN}🎯 최종 결과${NC}"
echo "============="

if [ $TEST_RESULT -eq 0 ]; then
    echo -e "${GREEN}✅ ConfigManager 테스트 성공!${NC}"
    echo
    echo "📋 권장 다음 단계:"
    echo "  1. 프로덕션 환경에서 실제 설정 파일 생성 테스트"
    echo "  2. 다른 모듈들과의 통합 테스트"
    echo "  3. 성능 및 동시성 테스트 실행"
    echo "  4. 실제 환경 변수들로 테스트"
else
    echo -e "${RED}❌ ConfigManager 테스트 실패${NC}"
    echo
    echo "🔧 문제 해결 방법:"
    echo "  1. 컴파일 오류: 누락된 의존성 설치"
    echo "  2. 런타임 오류: 소스 코드의 구현 확인"
    echo "  3. 테스트 실패: 로직 검토 및 디버깅"
    echo "  4. 환경 문제: 권한 및 경로 설정 확인"
fi

echo
echo -e "${BLUE}💡 추가 정보:${NC}"
echo "  📄 테스트 로그는 위의 출력에서 확인하세요"
echo "  🔧 문제가 지속되면 개발팀에 문의하세요"
echo "  📚 ConfigManager 사용법은 헤더 파일의 주석을 참조하세요"

exit $TEST_RESULT