#!/bin/bash

# =============================================================================
# backend/test-runner.sh
# PulseOne Backend API 테스트 실행 스크립트
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

# 로고 출력
echo -e "${BLUE}
╔══════════════════════════════════════════════════════════════════════════════╗
║                         🎯 PulseOne Backend API 테스트                        ║
║                          Comprehensive Test Suite                            ║
╚══════════════════════════════════════════════════════════════════════════════╝
${NC}"

# 환경 확인 함수
check_environment() {
    echo -e "${CYAN}🔍 === 테스트 환경 확인 ===${NC}"
    
    # Node.js 버전 확인
    if command -v node &> /dev/null; then
        NODE_VERSION=$(node --version)
        echo -e "${GREEN}✅ Node.js: ${NODE_VERSION}${NC}"
    else
        echo -e "${RED}❌ Node.js가 설치되어 있지 않습니다.${NC}"
        exit 1
    fi
    
    # npm 버전 확인
    if command -v npm &> /dev/null; then
        NPM_VERSION=$(npm --version)
        echo -e "${GREEN}✅ npm: v${NPM_VERSION}${NC}"
    else
        echo -e "${RED}❌ npm이 설치되어 있지 않습니다.${NC}"
        exit 1
    fi
    
    # package.json 존재 확인
    if [ -f "package.json" ]; then
        echo -e "${GREEN}✅ package.json 발견${NC}"
    else
        echo -e "${RED}❌ package.json이 없습니다. backend 디렉토리에서 실행하세요.${NC}"
        exit 1
    fi
    
    echo ""
}

# 의존성 설치 함수
install_dependencies() {
    echo -e "${CYAN}📦 === 의존성 설치 ===${NC}"
    
    if [ ! -d "node_modules" ]; then
        echo -e "${YELLOW}⏳ node_modules가 없습니다. 의존성을 설치합니다...${NC}"
        npm install
        echo -e "${GREEN}✅ 의존성 설치 완료${NC}"
    else
        echo -e "${GREEN}✅ node_modules 존재함${NC}"
        echo -e "${YELLOW}⏳ 의존성 업데이트 확인 중...${NC}"
        npm outdated || true  # outdated가 있어도 계속 진행
    fi
    
    echo ""
}

# 테스트 파일 존재 확인
check_test_files() {
    echo -e "${CYAN}📋 === 테스트 파일 확인 ===${NC}"
    
    TEST_FILES=(
        "__tests__/app.test.js"
        "__tests__/backend-api-comprehensive.test.js"
        "__tests__/setup.js"
    )
    
    for file in "${TEST_FILES[@]}"; do
        if [ -f "$file" ]; then
            echo -e "${GREEN}✅ $file${NC}"
        else
            echo -e "${YELLOW}⚠️ $file (없음 - 건너뛸 예정)${NC}"
        fi
    done
    
    echo ""
}

# 개별 서비스 상태 확인
check_services() {
    echo -e "${CYAN}🔍 === 외부 서비스 상태 확인 ===${NC}"
    
    # Redis 확인
    if command -v redis-cli &> /dev/null; then
        if redis-cli ping &> /dev/null; then
            echo -e "${GREEN}✅ Redis: 연결 가능${NC}"
        else
            echo -e "${YELLOW}⚠️ Redis: 연결 불가 (테스트는 계속 진행)${NC}"
        fi
    else
        echo -e "${YELLOW}⚠️ Redis: redis-cli 없음 (테스트는 계속 진행)${NC}"
    fi
    
    # PostgreSQL 확인
    if command -v psql &> /dev/null; then
        if pg_isready -h localhost -p 5432 &> /dev/null; then
            echo -e "${GREEN}✅ PostgreSQL: 연결 가능${NC}"
        else
            echo -e "${YELLOW}⚠️ PostgreSQL: 연결 불가 (테스트는 계속 진행)${NC}"
        fi
    else
        echo -e "${YELLOW}⚠️ PostgreSQL: psql 없음 (테스트는 계속 진행)${NC}"
    fi
    
    # RabbitMQ 확인
    if command -v rabbitmqctl &> /dev/null; then
        if rabbitmqctl status &> /dev/null; then
            echo -e "${GREEN}✅ RabbitMQ: 연결 가능${NC}"
        else
            echo -e "${YELLOW}⚠️ RabbitMQ: 연결 불가 (테스트는 계속 진행)${NC}"
        fi
    else
        echo -e "${YELLOW}⚠️ RabbitMQ: rabbitmqctl 없음 (테스트는 계속 진행)${NC}"
    fi
    
    # InfluxDB 확인
    if command -v curl &> /dev/null; then
        if curl -s http://localhost:8086/ping &> /dev/null; then
            echo -e "${GREEN}✅ InfluxDB: 연결 가능${NC}"
        else
            echo -e "${YELLOW}⚠️ InfluxDB: 연결 불가 (테스트는 계속 진행)${NC}"
        fi
    else
        echo -e "${YELLOW}⚠️ curl 없음 - InfluxDB 확인 불가${NC}"
    fi
    
    echo ""
}

# 메인 테스트 실행 함수
run_tests() {
    echo -e "${CYAN}🧪 === 테스트 실행 ===${NC}"
    
    # 테스트 종류별 실행
    TEST_TYPES=(
        "기본 서버 테스트:test:basic"
        "통합 API 테스트:test:api"
        "전체 테스트:test"
    )
    
    for test_info in "${TEST_TYPES[@]}"; do
        IFS=':' read -r test_name test_command <<< "$test_info"
        
        echo -e "${PURPLE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
        echo -e "${PURPLE}🔍 ${test_name}${NC}"
        echo -e "${PURPLE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
        
        if npm run $test_command; then
            echo -e "${GREEN}✅ ${test_name} 성공${NC}"
        else
            echo -e "${RED}❌ ${test_name} 실패${NC}"
            FAILED_TESTS="${FAILED_TESTS}${test_name}\n"
        fi
        
        echo ""
        sleep 1
    done
}

# 테스트 커버리지 생성
generate_coverage() {
    echo -e "${CYAN}📊 === 테스트 커버리지 생성 ===${NC}"
    
    if npm run test:coverage; then
        echo -e "${GREEN}✅ 커버리지 리포트 생성 완료${NC}"
        echo -e "${BLUE}📁 커버리지 리포트: coverage/lcov-report/index.html${NC}"
    else
        echo -e "${YELLOW}⚠️ 커버리지 생성 실패 (계속 진행)${NC}"
    fi
    
    echo ""
}

# Docker 환경에서의 추가 테스트
run_docker_tests() {
    echo -e "${CYAN}🐳 === Docker 환경 테스트 ===${NC}"
    
    if command -v docker &> /dev/null; then
        if docker ps &> /dev/null; then
            echo -e "${GREEN}✅ Docker 사용 가능${NC}"
            
            # Docker Compose로 서비스 시작 시도
            if [ -f "../docker-compose.yml" ]; then
                echo -e "${YELLOW}⏳ Docker Compose 서비스 확인 중...${NC}"
                cd ..
                docker-compose ps || true
                cd backend
                echo -e "${GREEN}✅ Docker 환경 확인 완료${NC}"
            else
                echo -e "${YELLOW}⚠️ docker-compose.yml 없음${NC}"
            fi
        else
            echo -e "${YELLOW}⚠️ Docker 데몬 연결 불가${NC}"
        fi
    else
        echo -e "${YELLOW}⚠️ Docker 설치되지 않음${NC}"
    fi
    
    echo ""
}

# 결과 요약
print_summary() {
    echo -e "${BLUE}
╔══════════════════════════════════════════════════════════════════════════════╗
║                              📋 테스트 결과 요약                               ║
╚══════════════════════════════════════════════════════════════════════════════╝
${NC}"
    
    if [ -z "$FAILED_TESTS" ]; then
        echo -e "${GREEN}🎉 모든 테스트가 성공적으로 완료되었습니다!${NC}"
        echo ""
        echo -e "${CYAN}📝 다음 단계:${NC}"
        echo -e "${YELLOW}   1. 프로덕션 환경에서 추가 테스트${NC}"
        echo -e "${YELLOW}   2. 성능 테스트 실행${NC}"
        echo -e "${YELLOW}   3. 보안 테스트 실행${NC}"
        echo -e "${YELLOW}   4. 문서화 업데이트${NC}"
    else
        echo -e "${RED}❌ 일부 테스트가 실패했습니다:${NC}"
        echo -e "${RED}${FAILED_TESTS}${NC}"
        echo ""
        echo -e "${CYAN}📝 해결 방안:${NC}"
        echo -e "${YELLOW}   1. Docker 환경 설정 확인${NC}"
        echo -e "${YELLOW}   2. 데이터베이스 서비스 시작${NC}"
        echo -e "${YELLOW}   3. 환경 변수 설정 확인${NC}"
        echo -e "${YELLOW}   4. 로그 파일 확인${NC}"
    fi
    
    echo ""
    echo -e "${BLUE}📁 생성된 파일들:${NC}"
    echo -e "${YELLOW}   - coverage/: 테스트 커버리지 리포트${NC}"
    echo -e "${YELLOW}   - npm-debug.log: 오류 발생 시 디버그 로그${NC}"
    
    echo ""
    echo -e "${CYAN}🔗 유용한 명령어:${NC}"
    echo -e "${YELLOW}   npm run test:watch  # 테스트 감시 모드${NC}"
    echo -e "${YELLOW}   npm run test:api    # API 테스트만 실행${NC}"
    echo -e "${YELLOW}   npm run dev         # 개발 서버 시작${NC}"
    
    echo -e "${BLUE}
╔══════════════════════════════════════════════════════════════════════════════╗
║                        ✅ 테스트 스크립트 실행 완료                            ║
╚══════════════════════════════════════════════════════════════════════════════╝
${NC}"
}

# 메인 실행 함수
main() {
    local start_time=$(date +%s)
    
    # 모든 함수 실행
    check_environment
    install_dependencies
    check_test_files
    check_services
    run_tests
    generate_coverage
    run_docker_tests
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    echo -e "${BLUE}⏱️ 총 실행 시간: ${duration}초${NC}"
    print_summary
}

# 스크립트 인자 처리
case "${1:-all}" in
    "env")
        check_environment
        ;;
    "deps")
        install_dependencies
        ;;
    "services")
        check_services
        ;;
    "test")
        run_tests
        ;;
    "coverage")
        generate_coverage
        ;;
    "docker")
        run_docker_tests
        ;;
    "all"|*)
        main
        ;;
esac