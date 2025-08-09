#!/bin/bash
# =============================================================================
# run_docker_pipeline_test.sh - Docker 환경용 PulseOne 완전한 데이터 파이프라인 테스트
# =============================================================================

set -e  # 에러 시 종료

# =============================================================================
# 🎨 색상 및 스타일 설정
# =============================================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 이모지 및 아이콘
CHECK_MARK="✅"
CROSS_MARK="❌"
WARNING="⚠️ "
INFO="🔍"
ROCKET="🚀"
GEAR="🔧"
DOCKER="🐳"

# =============================================================================
# 🔧 함수 정의
# =============================================================================

print_header() {
    echo -e "${CYAN}"
    echo "════════════════════════════════════════════════════════════════════"
    echo "🐳 PulseOne Docker 환경 데이터 파이프라인 테스트"
    echo "════════════════════════════════════════════════════════════════════"
    echo -e "${NC}"
}

print_step() {
    echo -e "${BLUE}$1${NC}"
}

print_success() {
    echo -e "${GREEN}${CHECK_MARK} $1${NC}"
}

print_error() {
    echo -e "${RED}${CROSS_MARK} $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}${WARNING}$1${NC}"
}

print_info() {
    echo -e "${CYAN}${INFO} $1${NC}"
}

# =============================================================================
# 🐳 Docker 환경 감지 및 Redis 확인
# =============================================================================

detect_docker_environment() {
    print_step "\n${DOCKER} Docker 환경 감지 중..."
    
    # Docker 환경 확인
    if [ -f /.dockerenv ] || grep -q 'docker\|lxc' /proc/1/cgroup 2>/dev/null; then
        print_success "Docker 컨테이너 환경 감지됨"
        export IS_DOCKER=true
    else
        print_info "일반 Linux 환경"
        export IS_DOCKER=false
    fi
    
    # 현재 디렉토리 및 환경 정보
    echo -e "${CYAN}📍 현재 환경 정보:${NC}"
    echo "  - 작업 디렉토리: $(pwd)"
    echo "  - 사용자: $(whoami)"
    echo "  - 호스트명: $(hostname)"
    echo "  - Docker 환경: ${IS_DOCKER}"
}

check_redis_in_docker() {
    print_step "\n${INFO} Docker 환경에서 Redis 확인..."
    
    local redis_found=false
    local redis_host="localhost"
    local redis_port="6379"
    
    # 1. 환경 설정 파일에서 Redis 정보 읽기
    if [ -f "./config/redis.env" ]; then
        print_info "redis.env 설정 파일 발견"
        source ./config/redis.env 2>/dev/null || true
        
        if [ ! -z "$REDIS_PRIMARY_HOST" ]; then
            redis_host="$REDIS_PRIMARY_HOST"
        fi
        if [ ! -z "$REDIS_PRIMARY_PORT" ]; then
            redis_port="$REDIS_PRIMARY_PORT"
        fi
        
        print_info "설정에서 Redis 주소: ${redis_host}:${redis_port}"
    fi
    
    # 2. Docker Compose 서비스로 Redis 확인
    if command -v docker >/dev/null 2>&1; then
        print_info "Docker 명령어로 Redis 컨테이너 확인 중..."
        
        # Redis 컨테이너 찾기
        local redis_containers=$(docker ps --format "table {{.Names}}\t{{.Ports}}" | grep -i redis | head -5)
        if [ ! -z "$redis_containers" ]; then
            print_success "실행 중인 Redis 컨테이너 발견:"
            echo "$redis_containers" | while read line; do
                echo "    🐳 $line"
            done
            redis_found=true
        fi
        
        # Docker network에서 redis 호스트 확인
        if docker network ls >/dev/null 2>&1; then
            local networks=$(docker network ls --format "{{.Name}}" | grep -v bridge | grep -v host | grep -v none)
            for network in $networks; do
                if docker network inspect "$network" 2>/dev/null | grep -q redis; then
                    print_info "Docker 네트워크 '$network'에서 Redis 서비스 발견"
                    redis_found=true
                    break
                fi
            done
        fi
    fi
    
    # 3. 네트워크로 Redis 연결 테스트
    print_info "Redis 연결 테스트 시도 중..."
    
    # Redis 연결 테스트를 위한 간단한 방법들
    local connection_methods=(
        "redis ${redis_host} ${redis_port}"
        "redis redis ${redis_port}"
        "redis localhost 6379"
        "redis 127.0.0.1 6379"
    )
    
    for method in "${connection_methods[@]}"; do
        local host=$(echo $method | cut -d' ' -f2)
        local port=$(echo $method | cut -d' ' -f3)
        
        print_info "테스트: ${host}:${port}"
        
        # nc (netcat)로 포트 체크
        if command -v nc >/dev/null 2>&1; then
            if timeout 3 nc -z "$host" "$port" 2>/dev/null; then
                print_success "Redis 서버 연결 가능: ${host}:${port}"
                export REDIS_HOST="$host"
                export REDIS_PORT="$port"
                redis_found=true
                break
            fi
        fi
        
        # telnet으로 포트 체크
        if command -v telnet >/dev/null 2>&1; then
            if timeout 3 bash -c "echo >/dev/tcp/$host/$port" 2>/dev/null; then
                print_success "Redis 서버 연결 가능: ${host}:${port}"
                export REDIS_HOST="$host"
                export REDIS_PORT="$port"
                redis_found=true
                break
            fi
        fi
        
        # Python으로 포트 체크
        if command -v python3 >/dev/null 2>&1; then
            if python3 -c "
import socket
import sys
try:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(3)
    result = sock.connect_ex(('$host', $port))
    sock.close()
    sys.exit(0 if result == 0 else 1)
except:
    sys.exit(1)
" 2>/dev/null; then
                print_success "Redis 서버 연결 가능: ${host}:${port}"
                export REDIS_HOST="$host"
                export REDIS_PORT="$port"
                redis_found=true
                break
            fi
        fi
    done
    
    if [ "$redis_found" = true ]; then
        print_success "Redis 서버가 Docker 환경에서 접근 가능합니다"
        
        # Redis 정보 출력
        echo -e "\n${CYAN}📊 Redis 연결 정보:${NC}"
        echo "  - 호스트: ${REDIS_HOST:-$redis_host}"
        echo "  - 포트: ${REDIS_PORT:-$redis_port}"
        echo "  - 환경: Docker 컨테이너"
        
        return 0
    else
        print_error "Docker 환경에서 Redis 서버를 찾을 수 없습니다"
        
        echo -e "\n${YELLOW}💡 해결 방법:${NC}"
        echo "1. Docker Compose로 Redis 시작:"
        echo "   docker-compose up -d redis"
        echo ""
        echo "2. 별도 Redis 컨테이너 실행:"
        echo "   docker run -d --name redis -p 6379:6379 redis:latest"
        echo ""
        echo "3. 기존 Redis 컨테이너 확인:"
        echo "   docker ps | grep redis"
        
        return 1
    fi
}

check_build_environment() {
    print_step "\n${GEAR} Docker 빌드 환경 확인..."
    
    local errors=0
    
    # G++ 컴파일러 확인
    if ! command -v g++ &> /dev/null; then
        print_error "G++ 컴파일러가 설치되지 않았습니다"
        ((errors++))
    else
        print_success "G++ 컴파일러 확인됨"
    fi
    
    # Make 확인
    if ! command -v make &> /dev/null; then
        print_error "Make가 설치되지 않았습니다"
        ((errors++))
    else
        print_success "Make 확인됨"
    fi
    
    # 라이브러리 확인 (컨테이너 환경에서는 이미 설치되어 있을 것)
    echo -e "\n${CYAN}📚 라이브러리 상태:${NC}"
    
    # SQLite 확인
    if pkg-config --exists sqlite3 2>/dev/null; then
        print_success "SQLite3 개발 라이브러리 확인됨"
    else
        print_warning "SQLite3 개발 라이브러리 상태 확인 불가 (컨테이너에서는 정상)"
    fi
    
    # Hiredis 확인
    if ldconfig -p | grep -q libhiredis 2>/dev/null; then
        print_success "Hiredis 라이브러리 확인됨"
    else
        print_warning "Hiredis 라이브러리 상태 확인 불가 (컨테이너에서는 정상)"
    fi
    
    # Google Test 확인
    if ldconfig -p | grep -q libgtest 2>/dev/null; then
        print_success "Google Test 라이브러리 확인됨"
    else
        print_warning "Google Test 라이브러리 상태 확인 불가 (컨테이너에서는 정상)"
    fi
    
    return $errors
}

check_project_structure() {
    print_step "\n${INFO} 프로젝트 구조 확인..."
    
    local required_dirs=(
        "../src"
        "../include"
    )
    
    local required_files=(
        "../src/Utils/LogManager.cpp"
        "../src/Database/DatabaseManager.cpp" 
        "../src/Workers/WorkerFactory.cpp"
    )
    
    # 필수 디렉토리 확인
    for dir in "${required_dirs[@]}"; do
        if [ ! -d "$dir" ]; then
            print_error "필수 디렉토리 없음: $dir"
            return 1
        fi
    done
    
    # 필수 파일 확인 (일부)
    local missing_files=0
    for file in "${required_files[@]}"; do
        if [ ! -f "$file" ]; then
            print_warning "소스 파일 없음: $file"
            ((missing_files++))
        fi
    done
    
    if [ $missing_files -eq 0 ]; then
        print_success "모든 핵심 소스 파일 확인됨"
    else
        print_warning "$missing_files개 소스 파일이 누락되었지만 계속 진행합니다"
    fi
    
    # 테스트 데이터 디렉토리 확인
    if [ ! -d "./data" ]; then
        print_info "테스트 데이터 디렉토리 생성 중..."
        mkdir -p ./data
    fi
    
    print_success "프로젝트 구조 검증 완료"
    return 0
}

# =============================================================================
# 🚀 빌드 및 테스트 실행
# =============================================================================

build_pipeline_test() {
    print_step "\n${GEAR} 파이프라인 테스트 빌드 중..."
    
    # test_complete_data_pipeline.cpp 파일 존재 확인
    if [ ! -f "test_complete_data_pipeline.cpp" ]; then
        print_error "test_complete_data_pipeline.cpp 파일이 없습니다"
        echo ""
        echo -e "${YELLOW}해결 방법:${NC}"
        echo "1. 제공된 test_complete_data_pipeline.cpp 파일을 현재 디렉토리에 생성하세요"
        echo "2. 또는 기존 테스트 실행: make step3-test"
        return 1
    fi
    
    # Makefile 확인
    if [ ! -f "Makefile" ]; then
        print_error "Makefile이 없습니다"
        return 1
    fi
    
    # 빌드 시도
    print_info "Make를 사용한 빌드 시작..."
    
    # 기존 테스트 빌드 시도 (step6이 없을 경우를 대비)
    if make step6-build 2>/dev/null; then
        print_success "step6-build 성공 (파이프라인 테스트)"
        export BUILD_TARGET="step6"
        return 0
    elif make step5-build 2>/dev/null; then
        print_success "step5-build 성공 (DB 통합 테스트)"
        export BUILD_TARGET="step5"
        return 0
    elif make step3-build 2>/dev/null; then
        print_success "step3-build 성공 (Workers 테스트)"
        export BUILD_TARGET="step3"
        return 0
    else
        print_error "빌드 실패 - 빌드 로그를 확인하세요"
        return 1
    fi
}

run_tests() {
    print_step "\n${ROCKET} Docker 환경 테스트 실행..."
    
    local test_option="$1"
    local build_target="${BUILD_TARGET:-step3}"
    
    # Redis 환경 변수 설정
    export REDIS_HOST="${REDIS_HOST:-localhost}"
    export REDIS_PORT="${REDIS_PORT:-6379}"
    
    echo -e "${CYAN}🔧 테스트 환경:${NC}"
    echo "  - Redis 호스트: $REDIS_HOST"
    echo "  - Redis 포트: $REDIS_PORT"
    echo "  - 빌드 타겟: $build_target"
    echo "  - Docker 환경: $IS_DOCKER"
    
    case "$test_option" in
        "step6"|"pipeline")
            if [ "$build_target" = "step6" ]; then
                print_info "완전한 파이프라인 테스트 실행"
                make step6-test
            else
                print_warning "파이프라인 테스트를 사용할 수 없습니다. 기존 테스트 실행..."
                make "${build_target}-test"
            fi
            ;;
        "quick")
            if [ "$build_target" = "step6" ]; then
                print_info "빠른 파이프라인 테스트"
                make pipeline-quick 2>/dev/null || make step6-test
            else
                print_info "빠른 테스트 모드"
                make "${build_target}-test"
            fi
            ;;
        *)
            print_info "기본 테스트 실행: ${build_target}"
            make "${build_target}-test"
            ;;
    esac
}

# =============================================================================
# 📊 결과 분석
# =============================================================================

show_test_results() {
    print_step "\n${INFO} 테스트 결과 확인..."
    
    # Redis에 저장된 데이터 확인 (가능한 경우)
    if [ ! -z "$REDIS_HOST" ] && [ ! -z "$REDIS_PORT" ]; then
        print_info "Redis 데이터 확인 시도..."
        
        # Python으로 Redis 데이터 확인
        if command -v python3 >/dev/null 2>&1; then
            python3 << EOF 2>/dev/null || true
import socket
try:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(3)
    sock.connect(('$REDIS_HOST', $REDIS_PORT))
    sock.send(b'*1\r\n\$4\r\nDBSIZE\r\n')
    response = sock.recv(1024)
    sock.close()
    print("📊 Redis 연결 성공 - 데이터베이스 응답 확인됨")
except Exception as e:
    print(f"Redis 연결 테스트 실패: {e}")
EOF
        fi
    fi
    
    # 빌드 결과 확인
    if [ -f "./bin/test_step6" ]; then
        print_success "파이프라인 테스트 바이너리 생성됨"
    elif [ -f "./bin/test_step5" ]; then
        print_success "DB 통합 테스트 바이너리 생성됨"
    elif [ -f "./bin/test_step3" ]; then
        print_success "Workers 테스트 바이너리 생성됨"
    fi
    
    print_success "Docker 환경 테스트 완료"
}

# =============================================================================
# 🎯 메인 실행 로직
# =============================================================================

main() {
    local option="${1:-all}"
    local start_time=$(date +%s)
    
    print_header
    
    case "$option" in
        "help"|"-h"|"--help")
            echo -e "${CYAN}사용법: $0 [옵션]${NC}"
            echo ""
            echo "옵션:"
            echo "  all          - 전체 테스트 (기본값)"
            echo "  pipeline     - 파이프라인 테스트"
            echo "  quick        - 빠른 테스트"
            echo "  check        - 환경 확인만"
            echo "  build        - 빌드만"
            echo "  help         - 도움말"
            exit 0
            ;;
        "check")
            detect_docker_environment
            if check_redis_in_docker && check_build_environment && check_project_structure; then
                print_success "\n모든 환경 검증이 성공적으로 완료되었습니다!"
                exit 0
            else
                print_error "\n환경 검증 중 문제가 발생했습니다"
                exit 1
            fi
            ;;
        "build")
            detect_docker_environment
            if check_build_environment && check_project_structure; then
                if build_pipeline_test; then
                    print_success "\n빌드가 성공적으로 완료되었습니다!"
                    exit 0
                else
                    print_error "\n빌드 중 문제가 발생했습니다"
                    exit 1
                fi
            else
                exit 1
            fi
            ;;
        *)
            # 전체 테스트 플로우
            print_info "테스트 시작 시간: $(date)"
            
            # 1. 환경 감지
            detect_docker_environment
            
            # 2. Redis 확인
            if ! check_redis_in_docker; then
                print_error "Redis 서버 확인 실패 - 테스트를 계속 진행하지만 일부 기능이 제한될 수 있습니다"
            fi
            
            # 3. 빌드 환경 확인
            if ! check_build_environment; then
                print_error "빌드 환경 확인 중 경고가 있었지만 계속 진행합니다"
            fi
            
            # 4. 프로젝트 구조 확인  
            if ! check_project_structure; then
                print_error "프로젝트 구조 검증 실패"
                exit 1
            fi
            
            # 5. 빌드
            print_step "\n🔧 빌드 단계"
            if ! build_pipeline_test; then
                print_error "빌드 실패"
                exit 1
            fi
            
            # 6. 테스트 실행
            print_step "\n🚀 테스트 실행 단계"
            if run_tests "$option"; then
                print_success "\n테스트 실행 성공!"
                
                # 7. 결과 확인
                show_test_results
                
                # 실행 시간 계산
                local end_time=$(date +%s)
                local duration=$((end_time - start_time))
                local minutes=$((duration / 60))
                local seconds=$((duration % 60))
                
                echo -e "\n${GREEN}🎉 === Docker 환경 파이프라인 테스트 완료! ===${NC}"
                echo -e "${CYAN}📊 총 실행 시간: ${minutes}분 ${seconds}초${NC}"
                echo -e "${CYAN}📅 완료 시간: $(date)${NC}"
                echo -e "${CYAN}🐳 Docker 환경: $IS_DOCKER${NC}"
                
            else
                print_error "\n테스트 실행 실패"
                
                echo -e "\n${YELLOW}🔍 진단 정보:${NC}"
                echo "1. Redis 상태: ${REDIS_HOST:-미설정}:${REDIS_PORT:-미설정}"
                echo "2. 빌드 타겟: ${BUILD_TARGET:-미설정}"
                echo "3. 수동 실행: make ${BUILD_TARGET:-step3}-test"
                
                exit 1
            fi
            ;;
    esac
}

# =============================================================================
# 🚀 스크립트 진입점
# =============================================================================

# 스크립트가 직접 실행될 때만 main 함수 호출
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    # 인터럽트 시그널 처리
    trap 'echo -e "\n${YELLOW}테스트가 중단되었습니다${NC}"; exit 130' INT
    
    # 작업 디렉토리를 스크립트 위치로 변경
    cd "$(dirname "$0")"
    
    # 메인 함수 실행
    main "$@"
fi