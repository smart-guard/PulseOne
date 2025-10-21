#!/bin/bash
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}ℹ️  $1${NC}"; }
log_success() { echo -e "${GREEN}✅ $1${NC}"; }
log_error() { echo -e "${RED}❌ $1${NC}"; }

check_prerequisites() {
    log_info "사전 요구사항 체크..."
    if ! redis-cli -h pulseone-redis ping > /dev/null 2>&1; then
        log_error "Redis가 실행되지 않음!"
        log_info "Redis 확인: docker ps | grep redis"
        exit 1
    fi
    log_success "Redis 실행 중"
    
    if [ ! -f "bin/tests/test_integration" ]; then
        log_error "테스트 바이너리 없음!"
        log_info "먼저 빌드: make tests"
        exit 1
    fi
    log_success "테스트 바이너리 존재"
}

setup_test_environment() {
    log_info "테스트 환경 초기화..."
    redis-cli -h pulseone-redis DEL "alarm:test:1001" > /dev/null 2>&1 || true
    rm -f /tmp/test_export_gateway.db 2>/dev/null || true
    log_success "환경 초기화 완료"
}

run_integration_tests() {
    log_info "=========================================="
    log_info "통합 테스트 실행"
    log_info "=========================================="
    
    if ./bin/tests/test_integration; then
        log_success "=========================================="
        log_success "모든 테스트 통과! 🎉"
        log_success "=========================================="
        return 0
    else
        log_error "=========================================="
        log_error "테스트 실패!"
        log_error "=========================================="
        return 1
    fi
}

cleanup_test_environment() {
    log_info "정리 중..."
    redis-cli -h pulseone-redis DEL "alarm:test:1001" > /dev/null 2>&1 || true
    rm -f /tmp/test_export_gateway.db 2>/dev/null || true
    log_success "정리 완료"
}

main() {
    echo ""
    log_info "🚀 Export Gateway 통합 테스트 러너"
    echo ""
    
    check_prerequisites
    setup_test_environment
    
    if run_integration_tests; then
        TEST_RESULT=0
    else
        TEST_RESULT=1
    fi
    
    cleanup_test_environment
    
    echo ""
    if [ $TEST_RESULT -eq 0 ]; then
        log_success "✨ 테스트 완료 - 모든 테스트 통과!"
        exit 0
    else
        log_error "💥 테스트 실패"
        exit 1
    fi
}

main
