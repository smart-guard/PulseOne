#!/bin/bash
# =============================================================================
# WorkerFactory 초기화 및 Worker 생성 테스트
# =============================================================================

set -e

echo "🏭 WorkerFactory 및 Worker 생성 테스트"
echo "====================================="

# 색상 정의
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
PURPLE='\033[0;35m'
NC='\033[0m'

# 로그 파일
WORKER_LOG="logs/worker_test.log"

echo -e "\n${BLUE}📋 현재까지 성공한 항목들:${NC}"
echo "✅ ConfigManager 초기화"
echo "✅ DatabaseManager 초기화 (SQLite)"
echo "✅ RepositoryFactory 초기화"
echo "✅ 8개 Repository 생성 및 의존성 주입"

echo -e "\n${PURPLE}🔍 다음 단계: WorkerFactory 및 Worker 생성 테스트${NC}"

# 설정 파일 확인
echo -e "\n${BLUE}📄 현재 설정 확인:${NC}"
if [ -f "config/.env" ]; then
    echo "DATABASE_TYPE: $(grep "DATABASE_TYPE" config/.env | cut -d'=' -f2)"
    echo "SQLite 경로: $(grep "SQLITE_DB_PATH" config/.env | cut -d'=' -f2)"
    echo "로그 레벨: $(grep "LOG_LEVEL" config/.env | cut -d'=' -f2)"
else
    echo "⚠️ config/.env 파일 없음"
fi

# 데이터베이스 상태 확인
echo -e "\n${BLUE}🗃️ 데이터베이스 상태 확인:${NC}"
if [ -f "../data/db/pulseone.db" ]; then
    echo "✅ 데이터베이스 파일 존재: $(ls -lh ../data/db/pulseone.db | awk '{print $5}')"
    
    echo -e "\n${BLUE}📊 디바이스 데이터 확인:${NC}"
    DEVICE_COUNT=$(sqlite3 ../data/db/pulseone.db "SELECT COUNT(*) FROM devices;" 2>/dev/null || echo "0")
    echo "총 디바이스 수: $DEVICE_COUNT"
    
    if [ "$DEVICE_COUNT" -gt 0 ]; then
        echo -e "\n${BLUE}📋 활성 디바이스 목록:${NC}"
        sqlite3 ../data/db/pulseone.db "
        SELECT 
            id, 
            name, 
            protocol_type, 
            CASE WHEN is_enabled = 1 THEN '활성화' ELSE '비활성화' END as status
        FROM devices 
        LIMIT 5;" 2>/dev/null || echo "디바이스 조회 실패"
        
        echo -e "\n${BLUE}📈 프로토콜별 통계:${NC}"
        sqlite3 ../data/db/pulseone.db "
        SELECT 
            protocol_type, 
            COUNT(*) as count,
            SUM(CASE WHEN is_enabled = 1 THEN 1 ELSE 0 END) as active_count
        FROM devices 
        GROUP BY protocol_type;" 2>/dev/null || echo "통계 조회 실패"
    else
        echo "⚠️ 디바이스 데이터 없음 - 테스트 데이터 생성 필요"
    fi
else
    echo "❌ 데이터베이스 파일 없음"
fi

# WorkerFactory 테스트 실행
echo -e "\n${PURPLE}🚀 WorkerFactory 테스트 실행 (30초)...${NC}"

# 로그 파일 초기화
> "$WORKER_LOG"

# Collector 실행 (더 긴 시간으로 WorkerFactory까지 확인)
timeout 30s ./bin/pulseone_collector > "$WORKER_LOG" 2>&1 || echo "30초 후 종료됨"

# 로그 분석
echo -e "\n${BLUE}📊 WorkerFactory 관련 로그 분석:${NC}"

if [ -f "$WORKER_LOG" ]; then
    echo -e "\n${YELLOW}1. WorkerFactory 초기화:${NC}"
    grep -i "workerfactory" "$WORKER_LOG" | head -5 || echo "  WorkerFactory 로그 없음"
    
    echo -e "\n${YELLOW}2. Worker 생성 시도:${NC}"
    grep -i "worker.*creat\|creat.*worker" "$WORKER_LOG" | head -5 || echo "  Worker 생성 로그 없음"
    
    echo -e "\n${YELLOW}3. DeviceEntity 로딩:${NC}"
    grep -i "device.*entity\|loading.*device" "$WORKER_LOG" | head -3 || echo "  DeviceEntity 로그 없음"
    
    echo -e "\n${YELLOW}4. 프로토콜 드라이버:${NC}"
    grep -i "modbus\|mqtt\|bacnet" "$WORKER_LOG" | head -5 || echo "  프로토콜 드라이버 로그 없음"
    
    echo -e "\n${YELLOW}5. 에러 및 경고:${NC}"
    ERROR_COUNT=$(grep -i "error\|fail\|exception" "$WORKER_LOG" | wc -l)
    if [ "$ERROR_COUNT" -gt 0 ]; then
        echo "  발견된 에러: $ERROR_COUNT 개"
        grep -i "error\|fail\|exception" "$WORKER_LOG" | head -3
    else
        echo "  에러 없음 ✅"
    fi
    
    echo -e "\n${YELLOW}6. 최종 상태:${NC}"
    echo "  마지막 10개 로그 라인:"
    tail -10 "$WORKER_LOG"
    
else
    echo "❌ WorkerFactory 테스트 로그 없음"
fi

# 진행 상황 요약
echo -e "\n${BLUE}======================================${NC}"
echo -e "${BLUE}📋 현재 진행 상황 요약${NC}"
echo -e "${BLUE}======================================${NC}"

echo -e "\n${GREEN}✅ 완료된 단계:${NC}"
echo "1. ConfigManager 초기화"
echo "2. DatabaseManager 초기화 (SQLite)"
echo "3. RepositoryFactory 초기화"
echo "4. 8개 Repository 생성"

echo -e "\n${PURPLE}🔄 현재 테스트 중:${NC}"
echo "5. WorkerFactory 초기화"
echo "6. DeviceEntity → DeviceInfo 변환"
echo "7. Protocol Worker 생성"
echo "8. Driver 초기화"

# 다음 단계 제안
echo -e "\n${YELLOW}📌 다음 단계 제안:${NC}"
if grep -q "WorkerFactory.*initialized" "$WORKER_LOG" 2>/dev/null; then
    echo "1. ✅ WorkerFactory 초기화 성공 - Worker 생성 테스트 진행"
    echo "2. 🔧 프로토콜별 드라이버 동작 확인"
    echo "3. 📊 실제 데이터 수집 테스트"
else
    echo "1. 🔧 WorkerFactory 초기화 로그 분석"
    echo "2. 🗃️ DeviceRepository.findAll() 동작 확인"
    echo "3. 📝 디바이스 데이터 존재 여부 확인"
fi

echo -e "\n${GREEN}🎯 WorkerFactory 테스트 완료!${NC}"
echo -e "${BLUE}📁 로그 파일: $WORKER_LOG${NC}"