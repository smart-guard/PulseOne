#!/bin/bash
# =============================================================================
# PulseOne 데이터 수집 시뮬레이션 테스트
# Worker → Driver → 실제 데이터 수집 동작 검증
# =============================================================================

set -e

echo "📡 PulseOne 데이터 수집 시뮬레이션 테스트"
echo "========================================="

# 색상 정의
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'

# 로그 파일
SIMULATION_LOG="logs/data_collection_test.log"
PERFORMANCE_LOG="logs/performance_test.log"

# =============================================================================
# 1. 시뮬레이션 환경 설정
# =============================================================================

echo -e "\n${CYAN}=== 시뮬레이션 환경 설정 ===${NC}"

# 고급 설정 파일 생성
cat > config/.env << 'EOF'
# PulseOne 데이터 수집 시뮬레이션 설정
DB_TYPE=sqlite
SQLITE_PATH=../data/db/pulseone.db
LOG_LEVEL=info

# 성능 테스트 설정
PERFORMANCE_TEST=true
DATA_COLLECTION_INTERVAL=1000
MAX_RETRY_COUNT=3
CONNECTION_TIMEOUT=5000

# 프로토콜별 설정
MODBUS_SIMULATION=true
MQTT_SIMULATION=true
BACNET_SIMULATION=true

# 로그 설정
LOG_FILE_PATH=./logs/data_collection_test.log
LOG_PERFORMANCE=true
EOF

echo "✅ 시뮬레이션 설정 완료"

# =============================================================================
# 2. 테스트 데이터베이스 검증
# =============================================================================

echo -e "\n${CYAN}=== 테스트 데이터베이스 검증 ===${NC}"

# 활성 디바이스 확인
echo -e "${BLUE}📊 활성 디바이스 분석:${NC}"
if sqlite3 ../data/db/pulseone.db "SELECT COUNT(*) FROM devices WHERE is_enabled = 1;" > /dev/null 2>&1; then
    ACTIVE_DEVICES=$(sqlite3 ../data/db/pulseone.db "SELECT COUNT(*) FROM devices WHERE is_enabled = 1;")
    echo "활성 디바이스 수: $ACTIVE_DEVICES"
    
    if [ "$ACTIVE_DEVICES" -gt 0 ]; then
        echo -e "${GREEN}✅ 테스트할 활성 디바이스 존재${NC}"
        
        echo -e "\n프로토콜별 활성 디바이스:"
        sqlite3 ../data/db/pulseone.db "
        SELECT 
            protocol_type, 
            COUNT(*) as active_count,
            GROUP_CONCAT(name, ', ') as device_names
        FROM devices 
        WHERE is_enabled = 1 
        GROUP BY protocol_type;"
        
    else
        echo -e "${YELLOW}⚠️ 활성 디바이스가 없습니다. 테스트 데이터를 생성합니다.${NC}"
        
        # 테스트 디바이스 생성
        sqlite3 ../data/db/pulseone.db "
        INSERT OR REPLACE INTO devices (id, name, protocol_type, endpoint, is_enabled, created_at) VALUES
        (1, 'Test_Modbus_Device', 'ModbusTcp', '192.168.1.100:502', 1, datetime('now')),
        (2, 'Test_MQTT_Device', 'MQTT', 'mqtt://localhost:1883', 1, datetime('now')),
        (3, 'Test_BACnet_Device', 'BACnet', '192.168.1.200:47808', 1, datetime('now'));"
        
        # 테스트 데이터포인트 생성
        sqlite3 ../data/db/pulseone.db "
        INSERT OR REPLACE INTO data_points (id, device_id, name, address, data_type, is_active) VALUES
        (1, 1, 'Temperature', '1', 'FLOAT', 1),
        (2, 1, 'Pressure', '2', 'FLOAT', 1),
        (3, 2, 'Status', 'device/status', 'BOOLEAN', 1),
        (4, 3, 'AirFlow', 'AI1', 'FLOAT', 1);"
        
        echo "✅ 테스트 데이터 생성 완료"
    fi
else
    echo -e "${RED}❌ 데이터베이스 스키마 문제${NC}"
    echo "사용 가능한 테이블:"
    sqlite3 ../data/db/pulseone.db ".tables"
fi

# =============================================================================
# 3. 데이터 수집 시뮬레이션 실행
# =============================================================================

echo -e "\n${CYAN}=== 데이터 수집 시뮬레이션 실행 ===${NC}"

# 로그 파일 초기화
> "$SIMULATION_LOG"
> "$PERFORMANCE_LOG"

echo -e "${PURPLE}🚀 데이터 수집 시뮬레이션 시작 (60초)...${NC}"

# 백그라운드에서 성능 모니터링
monitor_performance() {
    local duration=60
    local interval=5
    local count=0
    
    while [ $count -lt $((duration / interval)) ]; do
        echo "$(date '+%Y-%m-%d %H:%M:%S') - Memory: $(ps -o pid,vsz,rss,comm -p $1 2>/dev/null | tail -1 || echo 'Process not found')" >> "$PERFORMANCE_LOG"
        sleep $interval
        count=$((count + 1))
    done
} &

# Collector 실행
timeout 60s ./bin/pulseone_collector > "$SIMULATION_LOG" 2>&1 &
COLLECTOR_PID=$!

# 성능 모니터링 시작
monitor_performance $COLLECTOR_PID &
MONITOR_PID=$!

# 프로세스 대기
wait $COLLECTOR_PID 2>/dev/null || true
kill $MONITOR_PID 2>/dev/null || true

echo -e "${GREEN}✅ 시뮬레이션 완료${NC}"

# =============================================================================
# 4. 결과 분석
# =============================================================================

echo -e "\n${CYAN}=== 시뮬레이션 결과 분석 ===${NC}"

if [ -f "$SIMULATION_LOG" ]; then
    echo -e "${BLUE}📊 데이터 수집 통계:${NC}"
    
    # 초기화 성공 확인
    echo "1. 초기화 단계:"
    grep -i "initialized\|setup\|connected" "$SIMULATION_LOG" | head -5 || echo "   초기화 로그 없음"
    
    # Worker 생성 확인
    echo -e "\n2. Worker 생성:"
    grep -i "worker.*created\|creating.*worker" "$SIMULATION_LOG" | head -5 || echo "   Worker 생성 로그 없음"
    
    # 데이터 수집 시도 확인
    echo -e "\n3. 데이터 수집 시도:"
    grep -i "reading\|collecting\|polling\|data.*point" "$SIMULATION_LOG" | head -5 || echo "   데이터 수집 로그 없음"
    
    # 연결 상태 확인
    echo -e "\n4. 연결 상태:"
    grep -i "connection\|connect\|disconnect" "$SIMULATION_LOG" | head -5 || echo "   연결 로그 없음"
    
    # 에러 분석
    echo -e "\n5. 에러 분석:"
    ERROR_COUNT=$(grep -i "error\|fail\|exception" "$SIMULATION_LOG" | wc -l)
    if [ "$ERROR_COUNT" -gt 0 ]; then
        echo -e "${YELLOW}   발견된 에러: $ERROR_COUNT 개${NC}"
        grep -i "error\|fail\|exception" "$SIMULATION_LOG" | head -3
    else
        echo -e "${GREEN}   에러 없음 ✅${NC}"
    fi
    
    # 성능 분석
    echo -e "\n6. 성능 분석:"
    if [ -f "$PERFORMANCE_LOG" ]; then
        echo "   메모리 사용량 변화:"
        head -3 "$PERFORMANCE_LOG"
        echo "   ..."
        tail -3 "$PERFORMANCE_LOG"
    else
        echo "   성능 로그 없음"
    fi
    
else
    echo -e "${RED}❌ 시뮬레이션 로그 없음${NC}"
fi

# =============================================================================
# 5. 프로토콜별 상세 분석
# =============================================================================

echo -e "\n${CYAN}=== 프로토콜별 상세 분석 ===${NC}"

if [ -f "$SIMULATION_LOG" ]; then
    
    # Modbus 분석
    echo -e "${BLUE}📡 Modbus 프로토콜:${NC}"
    MODBUS_LOGS=$(grep -i "modbus" "$SIMULATION_LOG" | wc -l)
    if [ "$MODBUS_LOGS" -gt 0 ]; then
        echo "   로그 라인 수: $MODBUS_LOGS"
        grep -i "modbus" "$SIMULATION_LOG" | head -3 || true
    else
        echo "   Modbus 관련 로그 없음"
    fi
    
    # MQTT 분석
    echo -e "\n📡 MQTT 프로토콜:${NC}"
    MQTT_LOGS=$(grep -i "mqtt" "$SIMULATION_LOG" | wc -l)
    if [ "$MQTT_LOGS" -gt 0 ]; then
        echo "   로그 라인 수: $MQTT_LOGS"
        grep -i "mqtt" "$SIMULATION_LOG" | head -3 || true
    else
        echo "   MQTT 관련 로그 없음"
    fi
    
    # BACnet 분석
    echo -e "\n📡 BACnet 프로토콜:${NC}"
    BACNET_LOGS=$(grep -i "bacnet" "$SIMULATION_LOG" | wc -l)
    if [ "$BACNET_LOGS" -gt 0 ]; then
        echo "   로그 라인 수: $BACNET_LOGS"
        grep -i "bacnet" "$SIMULATION_LOG" | head -3 || true
    else
        echo "   BACnet 관련 로그 없음"
    fi
    
fi

# =============================================================================
# 6. 데이터베이스 변화 확인
# =============================================================================

echo -e "\n${CYAN}=== 데이터베이스 변화 확인 ===${NC}"

# 최근 데이터 확인 (만약 current_values 테이블이 있다면)
if sqlite3 ../data/db/pulseone.db "SELECT name FROM sqlite_master WHERE type='table' AND name='current_values';" | grep -q current_values; then
    echo -e "${BLUE}📊 수집된 데이터:${NC}"
    RECENT_DATA=$(sqlite3 ../data/db/pulseone.db "SELECT COUNT(*) FROM current_values WHERE updated_at > datetime('now', '-2 minutes');")
    echo "   최근 2분간 업데이트된 값: $RECENT_DATA 개"
    
    if [ "$RECENT_DATA" -gt 0 ]; then
        echo "   샘플 데이터:"
        sqlite3 ../data/db/pulseone.db "SELECT device_id, data_point_id, value, updated_at FROM current_values ORDER BY updated_at DESC LIMIT 3;"
    fi
else
    echo "   current_values 테이블 없음"
fi

# =============================================================================
# 7. 최종 평가 및 권장사항
# =============================================================================

echo -e "\n${CYAN}======================================${NC}"
echo -e "${CYAN}📋 데이터 수집 시뮬레이션 결과${NC}"
echo -e "${CYAN}======================================${NC}"

# 점수 계산
SCORE=0

if [ -f "$SIMULATION_LOG" ]; then
    # 기본 실행: 20점
    SCORE=$((SCORE + 20))
    
    # 초기화 성공: 20점
    if grep -q -i "initialized" "$SIMULATION_LOG"; then
        SCORE=$((SCORE + 20))
    fi
    
    # Worker 생성: 20점
    if grep -q -i "worker.*created" "$SIMULATION_LOG"; then
        SCORE=$((SCORE + 20))
    fi
    
    # 데이터 수집 시도: 20점
    if grep -q -i "reading\|collecting\|polling" "$SIMULATION_LOG"; then
        SCORE=$((SCORE + 20))
    fi
    
    # 에러 없음: 20점
    ERROR_COUNT=$(grep -i "error\|fail\|exception" "$SIMULATION_LOG" | wc -l)
    if [ "$ERROR_COUNT" -eq 0 ]; then
        SCORE=$((SCORE + 20))
    fi
fi

echo -e "${BLUE}📊 시뮬레이션 점수: ${SCORE}/100${NC}"

if [ "$SCORE" -ge 80 ]; then
    echo -e "${GREEN}🎉 우수: PulseOne Collector가 정상적으로 동작합니다!${NC}"
    echo -e "\n${YELLOW}📌 다음 단계:${NC}"
    echo "1. 🌐 실제 디바이스와 연결 테스트"
    echo "2. 📊 백엔드 API와 데이터 연동"
    echo "3. 🔄 메시지큐 시스템 통합"
    echo "4. 📈 대용량 데이터 처리 테스트"
    
elif [ "$SCORE" -ge 60 ]; then
    echo -e "${YELLOW}⚠️ 양호: 기본 동작하지만 개선이 필요합니다.${NC}"
    echo -e "\n${YELLOW}📌 개선사항:${NC}"
    echo "1. 🐛 에러 로그 분석 및 수정"
    echo "2. 🔧 프로토콜 드라이버 안정성 향상"
    echo "3. 📝 설정 파일 최적화"
    
elif [ "$SCORE" -ge 40 ]; then
    echo -e "${RED}⚠️ 문제 있음: 부분적으로만 동작합니다.${NC}"
    echo -e "\n${RED}📌 수정 필요:${NC}"
    echo "1. 🔧 Worker 생성 로직 점검"
    echo "2. 🗃️ 데이터베이스 연결 문제 해결"
    echo "3. 📚 라이브러리 의존성 확인"
    
else
    echo -e "${RED}❌ 심각: 시스템이 제대로 동작하지 않습니다.${NC}"
    echo -e "\n${RED}📌 긴급 수정:${NC}"
    echo "1. 🔧 컴파일 에러 재확인"
    echo "2. 📁 파일 경로 및 권한 점검"
    echo "3. 🗃️ 데이터베이스 스키마 검증"
fi

echo -e "\n${CYAN}📁 로그 파일 위치:${NC}"
echo "  - $SIMULATION_LOG (시뮬레이션 로그)"
echo "  - $PERFORMANCE_LOG (성능 로그)"

echo -e "\n${GREEN}🎯 데이터 수집 시뮬레이션 완료!${NC}"