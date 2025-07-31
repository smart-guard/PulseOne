#!/bin/bash
# =============================================================================
# PulseOne Collector 전체 플로우 테스트 스크립트
# DB → Entity → WorkerFactory → Protocol Workers → Drivers 검증
# =============================================================================

set -e

echo "🔬 PulseOne Collector 전체 플로우 테스트"
echo "======================================="

# 색상 정의
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'

# 테스트 결과 저장
TEST_RESULTS=()
DEEP_TEST_RESULTS=()

# 로그 파일 경로
COLLECTOR_LOG="logs/deep_test.log"
TEST_LOG="logs/test_output.log"

# 테스트 함수
run_test() {
    local test_name="$1"
    local test_command="$2"
    echo -e "\n${BLUE}🔍 ${test_name}${NC}"
    if eval "$test_command" >> "$TEST_LOG" 2>&1; then
        echo -e "${GREEN}✅ 성공: ${test_name}${NC}"
        TEST_RESULTS+=("✅ $test_name")
        return 0
    else
        echo -e "${RED}❌ 실패: ${test_name}${NC}"
        TEST_RESULTS+=("❌ $test_name")
        echo "에러 내용:"
        tail -5 "$TEST_LOG"
        return 1
    fi
}

# 깊이 있는 테스트 함수
run_deep_test() {
    local test_name="$1"
    local expected_log="$2"
    local timeout_sec="${3:-10}"
    
    echo -e "\n${PURPLE}🏗️ 깊이 테스트: ${test_name}${NC}"
    
    # 로그 파일 초기화
    > "$COLLECTOR_LOG"
    > "$TEST_LOG"
    
    # Collector 실행 (백그라운드)
    timeout ${timeout_sec}s ./bin/pulseone_collector > "$TEST_LOG" 2>&1 || true
    
    # 로그 분석
    if [ -f "$COLLECTOR_LOG" ]; then
        echo -e "${CYAN}📋 로그 분석 중...${NC}"
        
        if grep -q "$expected_log" "$COLLECTOR_LOG"; then
            echo -e "${GREEN}✅ 성공: ${test_name}${NC}"
            echo -e "   ${GREEN}└─ 예상 로그 발견: ${expected_log}${NC}"
            DEEP_TEST_RESULTS+=("✅ $test_name")
            return 0
        else
            echo -e "${RED}❌ 실패: ${test_name}${NC}"
            echo -e "   ${RED}└─ 예상 로그 없음: ${expected_log}${NC}"
            DEEP_TEST_RESULTS+=("❌ $test_name")
            
            echo -e "${YELLOW}📝 실제 로그 내용:${NC}"
            tail -10 "$COLLECTOR_LOG" 2>/dev/null || echo "로그 파일 없음"
            return 1
        fi
    else
        echo -e "${RED}❌ 실패: ${test_name} (로그 파일 없음)${NC}"
        DEEP_TEST_RESULTS+=("❌ $test_name - 로그 없음")
        echo -e "${YELLOW}📝 stdout/stderr:${NC}"
        tail -10 "$TEST_LOG"
        return 1
    fi
}

# =============================================================================
# 1단계: 기본 환경 준비
# =============================================================================

echo -e "\n${CYAN}=== 1단계: 기본 환경 준비 ===${NC}"

# 디렉토리 생성
mkdir -p config data logs
echo "✅ 디렉토리 생성 완료"

# 설정 파일 생성 (디버그 로그 활성화)
cat > config/.env << 'EOF'
# PulseOne Collector 전체 플로우 테스트 설정
DB_TYPE=sqlite
SQLITE_PATH=../data/db/pulseone.db
LOG_LEVEL=debug

# Database 설정
DATABASE_TYPE=SQLITE
SQLITE_DB_PATH=../data/db/pulseone.db

# 로그 설정 (더 상세하게)
LOG_FILE_PATH=./logs/deep_test.log
LOG_CONSOLE=true
LOG_DETAIL=true

# WorkerFactory 디버그
WORKER_FACTORY_DEBUG=true
PROTOCOL_DRIVER_DEBUG=true
EOF

echo "✅ 설정 파일 생성 완료"

# =============================================================================
# 2단계: 데이터베이스 스키마 분석
# =============================================================================

echo -e "\n${CYAN}=== 2단계: 데이터베이스 스키마 분석 ===${NC}"

if [ ! -f "../data/db/pulseone.db" ]; then
    echo -e "${RED}❌ 데이터베이스 파일이 없습니다: ../data/db/pulseone.db${NC}"
    exit 1
fi

echo -e "${BLUE}📊 데이터베이스 기본 정보:${NC}"
echo "파일 크기: $(ls -lh ../data/db/pulseone.db | awk '{print $5}')"
echo "파일 타입: $(file ../data/db/pulseone.db)"

echo -e "\n${BLUE}📋 전체 테이블 목록:${NC}"
sqlite3 ../data/db/pulseone.db ".tables"

echo -e "\n${BLUE}🔍 핵심 테이블 구조 분석:${NC}"

# Devices 테이블 분석
if sqlite3 ../data/db/pulseone.db "SELECT name FROM sqlite_master WHERE type='table' AND name='devices';" | grep -q devices; then
    echo -e "${GREEN}✅ devices 테이블 존재${NC}"
    echo "스키마:"
    sqlite3 ../data/db/pulseone.db ".schema devices"
    echo -e "\n데이터 샘플 (처음 3개):"
    sqlite3 ../data/db/pulseone.db "SELECT id, name, protocol_type, endpoint, is_enabled FROM devices LIMIT 3;"
    
    echo -e "\n프로토콜별 통계:"
    sqlite3 ../data/db/pulseone.db "SELECT protocol_type, COUNT(*) as count FROM devices GROUP BY protocol_type;"
else
    echo -e "${RED}❌ devices 테이블 없음${NC}"
    echo "사용 가능한 테이블들:"
    sqlite3 ../data/db/pulseone.db ".schema" | grep "CREATE TABLE" | head -10
fi

# DataPoints 테이블 분석
if sqlite3 ../data/db/pulseone.db "SELECT name FROM sqlite_master WHERE type='table' AND name='data_points';" | grep -q data_points; then
    echo -e "\n${GREEN}✅ data_points 테이블 존재${NC}"
    echo "스키마:"
    sqlite3 ../data/db/pulseone.db ".schema data_points"
    
    echo -e "\n데이터포인트 통계:"
    sqlite3 ../data/db/pulseone.db "SELECT COUNT(*) as total_datapoints FROM data_points;"
    
    echo -e "\n디바이스별 데이터포인트 개수:"
    sqlite3 ../data/db/pulseone.db "SELECT device_id, COUNT(*) as points FROM data_points GROUP BY device_id LIMIT 5;"
else
    echo -e "${RED}❌ data_points 테이블 없음${NC}"
fi

# =============================================================================
# 3단계: ConfigManager 초기화 테스트
# =============================================================================

echo -e "\n${CYAN}=== 3단계: ConfigManager 초기화 테스트 ===${NC}"

run_deep_test "ConfigManager 초기화" "ConfigManager.*initialized" 5
run_deep_test "환경 변수 로딩" "Loading.*config.*file" 5

# =============================================================================
# 4단계: DatabaseManager 연결 테스트
# =============================================================================

echo -e "\n${CYAN}=== 4단계: DatabaseManager 연결 테스트 ===${NC}"

run_deep_test "DatabaseManager 초기화" "DatabaseManager.*initialized" 5
run_deep_test "SQLite 연결 성공" "SQLite.*connection.*successful" 5
run_deep_test "데이터베이스 스키마 확인" "Database.*schema.*loaded" 5

# =============================================================================  
# 5단계: RepositoryFactory 초기화 테스트
# =============================================================================

echo -e "\n${CYAN}=== 5단계: RepositoryFactory 초기화 테스트 ===${NC}"

run_deep_test "RepositoryFactory 초기화" "RepositoryFactory.*initialized" 5
run_deep_test "DeviceRepository 생성" "DeviceRepository.*created" 5
run_deep_test "DataPointRepository 생성" "DataPointRepository.*created" 5

# =============================================================================
# 6단계: WorkerFactory 초기화 테스트  
# =============================================================================

echo -e "\n${CYAN}=== 6단계: WorkerFactory 초기화 테스트 ===${NC}"

run_deep_test "WorkerFactory 싱글톤 생성" "WorkerFactory.*getInstance" 8
run_deep_test "WorkerFactory 초기화" "WorkerFactory.*initialized" 8
run_deep_test "의존성 주입 완료" "WorkerFactory.*dependencies.*injected" 8

# =============================================================================
# 7단계: Entity 로딩 테스트
# =============================================================================

echo -e "\n${CYAN}=== 7단계: Entity 로딩 테스트 ===${NC}"

run_deep_test "DeviceEntity 로딩" "Loading.*devices.*from.*database" 10
run_deep_test "활성 디바이스 필터링" "Found.*active.*devices" 10
run_deep_test "Entity→DeviceInfo 변환" "Converting.*DeviceEntity.*to.*DeviceInfo" 10

# =============================================================================
# 8단계: Protocol Worker 생성 테스트
# =============================================================================

echo -e "\n${CYAN}=== 8단계: Protocol Worker 생성 테스트 ===${NC}"

run_deep_test "ModbusTcp Worker 생성" "Creating.*ModbusTcpWorker" 15
run_deep_test "MQTT Worker 생성" "Creating.*MQTTWorker" 15
run_deep_test "BACnet Worker 생성" "Creating.*BACnetWorker" 15

# =============================================================================
# 9단계: Driver 초기화 테스트
# =============================================================================

echo -e "\n${CYAN}=== 9단계: Driver 초기화 테스트 ===${NC}"

run_deep_test "Modbus Driver 초기화" "ModbusDriver.*initialized" 20
run_deep_test "MQTT Driver 초기화" "MqttDriver.*initialized" 20
run_deep_test "BACnet Driver 초기화" "BACnetDriver.*initialized" 20

# =============================================================================
# 10단계: DataPoint 로딩 및 매핑 테스트
# =============================================================================

echo -e "\n${CYAN}=== 10단계: DataPoint 로딩 및 매핑 테스트 ===${NC}"

run_deep_test "DataPoint 로딩" "Loading.*data.*points.*for.*device" 15
run_deep_test "DataPoint→Worker 매핑" "Adding.*data.*point.*to.*worker" 15
run_deep_test "Worker 설정 완료" "Worker.*configuration.*completed" 15

# =============================================================================
# 11단계: 전체 통합 테스트 (긴 실행)
# =============================================================================

echo -e "\n${CYAN}=== 11단계: 전체 통합 테스트 ===${NC}"

echo -e "${PURPLE}🚀 30초간 완전한 실행 테스트...${NC}"
timeout 30s ./bin/pulseone_collector > logs/full_test.log 2>&1 || true

if [ -f "logs/full_test.log" ]; then
    echo -e "\n${BLUE}📊 30초 실행 결과 분석:${NC}"
    
    # 핵심 키워드들 검색
    echo "초기화 단계별 확인:"
    grep -i "initialized" logs/full_test.log | head -5 || echo "  초기화 로그 없음"
    
    echo -e "\nWorker 생성 확인:"
    grep -i "worker.*created\|creating.*worker" logs/full_test.log | head -5 || echo "  Worker 생성 로그 없음"
    
    echo -e "\n에러 발생 확인:"
    grep -i "error\|fail\|exception" logs/full_test.log | head -5 || echo "  에러 없음 ✅"
    
    echo -e "\n메모리 누수 확인:"
    grep -i "leak\|memory.*error" logs/full_test.log || echo "  메모리 누수 없음 ✅"
    
    echo -e "\n최종 상태:"
    tail -10 logs/full_test.log
else
    echo -e "${RED}❌ 전체 테스트 로그 없음${NC}"
fi

# =============================================================================
# 결과 요약 및 리포트
# =============================================================================

echo -e "\n${CYAN}======================================${NC}"
echo -e "${CYAN}📋 전체 테스트 결과 요약${NC}"
echo -e "${CYAN}======================================${NC}"

echo -e "\n${BLUE}기본 테스트 결과:${NC}"
for result in "${TEST_RESULTS[@]}"; do
    echo -e "  $result"
done

echo -e "\n${PURPLE}깊이 있는 테스트 결과:${NC}"
for result in "${DEEP_TEST_RESULTS[@]}"; do
    echo -e "  $result"
done

# 성공/실패 통계
SUCCESS_COUNT=$(printf '%s\n' "${TEST_RESULTS[@]}" "${DEEP_TEST_RESULTS[@]}" | grep -c "✅" || true)
FAIL_COUNT=$(printf '%s\n' "${TEST_RESULTS[@]}" "${DEEP_TEST_RESULTS[@]}" | grep -c "❌" || true)
TOTAL_COUNT=$((SUCCESS_COUNT + FAIL_COUNT))

echo -e "\n${CYAN}📊 통계:${NC}"
echo -e "  총 테스트: $TOTAL_COUNT"
echo -e "  ${GREEN}성공: $SUCCESS_COUNT${NC}"
echo -e "  ${RED}실패: $FAIL_COUNT${NC}"

if [ $FAIL_COUNT -eq 0 ]; then
    echo -e "\n${GREEN}🎉 모든 테스트 통과! PulseOne Collector가 정상 동작합니다.${NC}"
else
    echo -e "\n${YELLOW}⚠️ $FAIL_COUNT 개 테스트 실패. 로그를 확인하세요:${NC}"
    echo -e "  - logs/deep_test.log (Collector 로그)"
    echo -e "  - logs/test_output.log (테스트 출력)"
    echo -e "  - logs/full_test.log (30초 완전 실행 로그)"
fi

# 다음 단계 안내
echo -e "\n${YELLOW}📌 다음 단계 제안:${NC}"
if [ $SUCCESS_COUNT -gt $((TOTAL_COUNT / 2)) ]; then
    echo "1. 🌐 Backend API 서버 연동 테스트"
    echo "2. 📡 실제 디바이스 연결 테스트"
    echo "3. 📊 데이터 수집 및 저장 테스트"
    echo "4. 🔄 메시지큐 통신 테스트"
else
    echo "1. 🐛 실패한 테스트들의 로그 분석"
    echo "2. 🔧 누락된 헤더 파일이나 라이브러리 확인"
    echo "3. 📝 설정 파일 점검"
    echo "4. 🗃️ 데이터베이스 스키마 검증"
fi

echo -e "\n${GREEN}🎯 전체 플로우 테스트 완료!${NC}"