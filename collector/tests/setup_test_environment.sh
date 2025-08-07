#!/bin/bash
# =============================================================================
# setup_test_environment.sh
# PulseOne Collector 테스트 환경 자동 구성 스크립트 (tests 디렉토리 내에서 실행)
# =============================================================================

set -e

# 색상 정의
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}🔧 PulseOne Collector 테스트 환경 구성${NC}"
echo -e "${YELLOW}=======================================${NC}"
echo -e "${BLUE}📍 현재 위치: $(pwd)${NC}"

# collector/tests 디렉토리인지 확인
if [ ! -f "../include/Utils/ConfigManager.h" ]; then
    echo -e "${RED}❌ collector/tests 디렉토리에서 실행해주세요!${NC}"
    echo -e "${YELLOW}   현재 위치: $(pwd)${NC}"
    echo -e "${YELLOW}   예상 위치: /path/to/PulseOne/collector/tests${NC}"
    exit 1
fi

echo -e "${GREEN}✅ collector/tests 디렉토리에서 실행 중${NC}"

# =============================================================================
# 1. 테스트 디렉토리 구조 생성
# =============================================================================

echo -e "${BLUE}📁 테스트 디렉토리 구조 생성${NC}"

# 필요한 디렉토리들 생성
mkdir -p config
mkdir -p db
mkdir -p logs
mkdir -p backup
mkdir -p bin

echo -e "${GREEN}✅ 디렉토리 구조 생성 완료${NC}"
echo "   ./config/   - 테스트 전용 설정 파일"
echo "   ./db/       - 테스트 SQLite DB"
echo "   ./logs/     - 테스트 로그 파일"
echo "   ./backup/   - DB 백업"
echo "   ./bin/      - 컴파일된 테스트 실행 파일"

# =============================================================================
# 2. SQLite 테스트 DB 생성
# =============================================================================

echo -e "${BLUE}🗄️  SQLite 테스트 DB 생성${NC}"

if [ -f "db/pulseone_test.db" ]; then
    echo -e "${YELLOW}⚠️  기존 DB 발견 - 백업 생성${NC}"
    timestamp=$(date +%Y%m%d_%H%M%S)
    mv db/pulseone_test.db backup/pulseone_test_backup_$timestamp.db
fi

# setup_test_db.sql이 있는지 확인
if [ ! -f "setup_test_db.sql" ]; then
    echo -e "${YELLOW}⚠️  setup_test_db.sql이 없습니다. 생성 중...${NC}"
    
    # 간단한 테스트 스키마 생성
    cat > setup_test_db.sql << 'SQLEOF'
-- =============================================================================
-- PulseOne 테스트 DB 스키마 + 데이터
-- collector/tests/setup_test_db.sql
-- =============================================================================

-- 테넌트 테이블
CREATE TABLE IF NOT EXISTS tenants (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    company_name VARCHAR(100) NOT NULL,
    company_code VARCHAR(20) NOT NULL UNIQUE,
    is_active INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 사이트 테이블
CREATE TABLE IF NOT EXISTS sites (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    location VARCHAR(200),
    is_active INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id)
);

-- 디바이스 테이블
CREATE TABLE IF NOT EXISTS devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    site_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    protocol_type VARCHAR(50) NOT NULL,
    endpoint VARCHAR(255) NOT NULL,
    config TEXT,
    is_enabled INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id),
    FOREIGN KEY (site_id) REFERENCES sites(id)
);

-- 데이터 포인트 테이블
CREATE TABLE IF NOT EXISTS data_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    description TEXT,
    address INTEGER NOT NULL,
    data_type VARCHAR(20) NOT NULL,
    unit VARCHAR(20),
    scaling_factor DECIMAL(10,4) DEFAULT 1.0,
    is_enabled INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (device_id) REFERENCES devices(id)
);

-- 현재값 테이블
CREATE TABLE IF NOT EXISTS current_values (
    point_id INTEGER PRIMARY KEY,
    value DECIMAL(15,4),
    quality VARCHAR(20) DEFAULT 'good',
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (point_id) REFERENCES data_points(id)
);

-- =============================================================================
-- 테스트 데이터 삽입
-- =============================================================================

-- 테넌트 데이터
INSERT OR REPLACE INTO tenants (id, company_name, company_code) VALUES 
    (1, 'Test Factory Inc', 'TEST001'),
    (2, 'Demo Manufacturing', 'DEMO002');

-- 사이트 데이터
INSERT OR REPLACE INTO sites (id, tenant_id, name, location) VALUES 
    (1, 1, 'Main Test Site', 'Seoul Test Factory'),
    (2, 1, 'Secondary Site', 'Busan Test Plant'),
    (3, 2, 'Demo Site', 'Demo Location');

-- 디바이스 데이터 (5개)
INSERT OR REPLACE INTO devices (id, tenant_id, site_id, name, description, protocol_type, endpoint, config) VALUES 
    (1, 1, 1, 'Test-PLC-001', 'Main Production PLC', 'MODBUS_TCP', '192.168.1.10:502', '{"slave_id": 1, "timeout": 3000}'),
    (2, 1, 1, 'Test-HMI-001', 'Operator Interface', 'MODBUS_TCP', '192.168.1.11:502', '{"slave_id": 2, "timeout": 3000}'),
    (3, 1, 1, 'Test-HVAC-001', 'HVAC Controller', 'BACNET', '192.168.1.20', '{"device_id": 20, "network": 1}'),
    (4, 1, 2, 'Test-METER-001', 'Power Meter', 'MODBUS_TCP', '192.168.2.30:502', '{"slave_id": 10, "timeout": 5000}'),
    (5, 2, 3, 'Demo-SENSOR-001', 'IoT Sensor Array', 'MQTT', 'mqtt://192.168.3.50:1883', '{"topic": "sensors/data", "qos": 1}');

-- 데이터 포인트 데이터 (16개)
INSERT OR REPLACE INTO data_points (id, device_id, name, description, address, data_type, unit) VALUES 
    -- PLC-001 포인트들 (4개)
    (1, 1, 'Production_Count', 'Total production count', 40001, 'uint32', 'pieces'),
    (2, 1, 'Line_Speed', 'Production line speed', 40003, 'uint16', 'ppm'),
    (3, 1, 'Motor_Status', 'Main motor status', 10001, 'bool', ''),
    (4, 1, 'Temperature', 'Process temperature', 30001, 'int16', '°C'),
    
    -- HMI-001 포인트들 (3개)
    (5, 2, 'Alarm_Count', 'Active alarms', 40010, 'uint16', 'count'),
    (6, 2, 'Operator_Present', 'Operator presence', 10010, 'bool', ''),
    (7, 2, 'Screen_Brightness', 'Display brightness', 40012, 'uint16', '%'),
    
    -- HVAC-001 포인트들 (3개)
    (8, 3, 'Room_Temperature', 'Current temperature', 1, 'float', '°C'),
    (9, 3, 'Setpoint_Temp', 'Temperature setpoint', 2, 'float', '°C'),
    (10, 3, 'Fan_Speed', 'Fan speed', 3, 'uint16', 'rpm'),
    
    -- METER-001 포인트들 (3개)
    (11, 4, 'Voltage_L1', 'Phase 1 voltage', 40100, 'uint16', 'V'),
    (12, 4, 'Current_L1', 'Phase 1 current', 40102, 'uint16', 'A'),
    (13, 4, 'Power_Total', 'Total power', 40104, 'uint32', 'kW'),
    
    -- SENSOR-001 포인트들 (3개)
    (14, 5, 'Temp_Zone_1', 'Temperature zone 1', 1, 'float', '°C'),
    (15, 5, 'Temp_Zone_2', 'Temperature zone 2', 2, 'float', '°C'),
    (16, 5, 'Humidity', 'Relative humidity', 3, 'float', '%');

-- 현재값 데이터 (시뮬레이션)
INSERT OR REPLACE INTO current_values (point_id, value, quality) VALUES 
    (1, 12543, 'good'), (2, 85.5, 'good'), (3, 1, 'good'), (4, 23.5, 'good'),
    (5, 2, 'good'), (6, 1, 'good'), (7, 75, 'good'),
    (8, 22.5, 'good'), (9, 23.0, 'good'), (10, 1200, 'good'),
    (11, 220.5, 'good'), (12, 15.25, 'good'), (13, 3.365, 'good'),
    (14, 25.2, 'good'), (15, 24.8, 'good'), (16, 65.5, 'good');

-- 인덱스 생성
CREATE INDEX IF NOT EXISTS idx_devices_protocol ON devices(protocol_type);
CREATE INDEX IF NOT EXISTS idx_data_points_device ON data_points(device_id);
CREATE INDEX IF NOT EXISTS idx_current_values_timestamp ON current_values(timestamp);

-- 생성 완료 메시지
.print "=== PulseOne 테스트 DB 생성 완료 ==="
.print ""
.print "📊 생성된 데이터:"
SELECT 'tenants: ' || COUNT(*) FROM tenants
UNION ALL SELECT 'sites: ' || COUNT(*) FROM sites  
UNION ALL SELECT 'devices: ' || COUNT(*) FROM devices
UNION ALL SELECT 'data_points: ' || COUNT(*) FROM data_points
UNION ALL SELECT 'current_values: ' || COUNT(*) FROM current_values;
.print ""
.print "🔌 디바이스 목록:"
SELECT '  ' || name || ' (' || protocol_type || ')' FROM devices ORDER BY id;
.print ""
.print "✅ DB 파일: ./db/pulseone_test.db"
SQLEOF
fi

# SQLite DB 생성
sqlite3 db/pulseone_test.db < setup_test_db.sql

echo -e "${GREEN}✅ SQLite 테스트 DB 생성 완료: ./db/pulseone_test.db${NC}"

# =============================================================================
# 3. 기본 설정 파일들 생성 (없을 경우에만)
# =============================================================================

echo -e "${BLUE}⚙️  기본 설정 파일 확인${NC}"

# .env 파일이 없으면 기본 생성
if [ ! -f "config/.env" ]; then
    echo -e "${YELLOW}⚠️  config/.env 생성 중...${NC}"
    cat > config/.env << 'ENVEOF'
# PulseOne Collector 테스트 설정
PULSEONE_TEST_MODE=true
PULSEONE_ENV=test

# 데이터베이스 설정
DATABASE_TYPE=SQLITE
SQLITE_DB_PATH=./db/pulseone_test.db

# Redis 설정 (테스트에서는 비활성화)
REDIS_PRIMARY_ENABLED=false
REDIS_PRIMARY_HOST=localhost
REDIS_PRIMARY_PORT=6379

# 로깅 설정
LOG_LEVEL=DEBUG
LOG_TO_CONSOLE=true
LOG_TO_FILE=true
LOG_FILE_PATH=./logs/collector_test.log

# Collector 설정
COLLECTOR_SCAN_INTERVAL_MS=1000
COLLECTOR_MAX_WORKERS=5
ENVEOF
    echo -e "${GREEN}✅ config/.env 생성 완료${NC}"
else
    echo -e "${GREEN}✅ config/.env 이미 존재${NC}"
fi

# database.env 파일 기본 생성
if [ ! -f "config/database.env" ]; then
    echo -e "${YELLOW}⚠️  config/database.env 생성 중...${NC}"
    cat > config/database.env << 'DBENVEOF'
# 테스트 데이터베이스 설정
DATABASE_TYPE=SQLITE
SQLITE_ENABLED=true
SQLITE_DB_PATH=./db/pulseone_test.db
SQLITE_TIMEOUT_MS=30000

# 다른 DB는 테스트에서 비활성화
POSTGRESQL_ENABLED=false
MYSQL_ENABLED=false
DBENVEOF
    echo -e "${GREEN}✅ config/database.env 생성 완료${NC}"
else
    echo -e "${GREEN}✅ config/database.env 이미 존재${NC}"
fi

# redis.env 파일 기본 생성
if [ ! -f "config/redis.env" ]; then
    echo -e "${YELLOW}⚠️  config/redis.env 생성 중...${NC}"
    cat > config/redis.env << 'REDISENVEOF'
# 테스트 Redis 설정 (비활성화)
REDIS_PRIMARY_ENABLED=false
REDIS_PRIMARY_HOST=localhost
REDIS_PRIMARY_PORT=6379
REDIS_PRIMARY_DB=1
REDIS_TEST_MODE=true
REDISENVEOF
    echo -e "${GREEN}✅ config/redis.env 생성 완료${NC}"
else
    echo -e "${GREEN}✅ config/redis.env 이미 존재${NC}"
fi

# =============================================================================
# 4. 권한 설정
# =============================================================================

echo -e "${BLUE}🔐 권한 설정${NC}"

# 실행 권한 부여
chmod +x setup_test_environment.sh 2>/dev/null || true
chmod +x run_tests.sh 2>/dev/null || true

# DB 파일 권한
chmod 644 db/pulseone_test.db

echo -e "${GREEN}✅ 권한 설정 완료${NC}"

# =============================================================================
# 5. 완료 메시지
# =============================================================================

echo -e "${GREEN}🎉 테스트 환경 구성 완료!${NC}"
echo -e "${YELLOW}================================${NC}"
echo ""
echo -e "${BLUE}📁 생성된 구조 (collector/tests 기준):${NC}"
echo "   ."
echo "   ├── config/"
echo "   │   ├── .env              # 메인 테스트 설정"
echo "   │   ├── database.env      # DB 전용 설정"  
echo "   │   └── redis.env         # Redis 전용 설정"
echo "   ├── db/"
echo "   │   └── pulseone_test.db  # SQLite 테스트 DB (5 devices, 16 points)"
echo "   ├── logs/                 # 테스트 로그"
echo "   ├── backup/               # DB 백업"
echo "   └── bin/                  # 컴파일된 실행 파일"
echo ""
echo -e "${BLUE}🚀 다음 단계:${NC}"
echo "   1. Makefile 준비:"
echo "      - artifacts에서 tests/Makefile 저장"
echo ""
echo "   2. 테스트 코드 준비:"  
echo "      - artifacts에서 test_step1_config.cpp 저장"
echo "      - artifacts에서 test_step2_database.cpp 저장"
echo ""
echo "   3. 테스트 실행:"
echo "      make check-testdb   # 환경 확인"
echo "      make run-step1      # ConfigManager 테스트"
echo "      make run-step2      # DatabaseManager + 실제 DB 테스트"
echo ""
echo -e "${BLUE}💾 DB 확인:${NC}"
echo "   sqlite3 db/pulseone_test.db \"SELECT name, protocol_type FROM devices;\""
echo ""
echo -e "${GREEN}✅ collector/tests 디렉토리에서 모든 설정 완료!${NC}"

# =============================================================================
# 1. 디렉토리 구조 생성
# =============================================================================

echo -e "${BLUE}📁 테스트 디렉토리 구조 생성${NC}"

# 필요한 디렉토리들 생성
mkdir -p config
mkdir -p db
mkdir -p logs
mkdir -p backup
mkdir -p bin

echo -e "${GREEN}✅ 디렉토리 구조 생성 완료${NC}"
echo "   ./config/   - 테스트 전용 설정 파일"
echo "   ./db/       - 테스트 SQLite DB"
echo "   ./logs/     - 테스트 로그 파일"
echo "   ./backup/   - DB 백업"
echo "   ./bin/      - 컴파일된 테스트 실행 파일"

# =============================================================================
# 2. SQLite 테스트 DB 생성
# =============================================================================

echo -e "${BLUE}🗄️  SQLite 테스트 DB 생성${NC}"

if [ -f "db/pulseone_test.db" ]; then
    echo -e "${YELLOW}⚠️  기존 DB 발견 - 백업 생성${NC}"
    timestamp=$(date +%Y%m%d_%H%M%S)
    mv db/pulseone_test.db backup/pulseone_test_backup_$timestamp.db
fi

# setup_test_db.sql이 있는지 확인
if [ ! -f "setup_test_db.sql" ]; then
    echo -e "${YELLOW}⚠️  setup_test_db.sql이 없습니다. 생성 중...${NC}"
    
    # 간단한 테스트 스키마 생성
    cat > setup_test_db.sql << 'SQLEOF'
-- PulseOne 테스트 DB 간단 스키마
CREATE TABLE IF NOT EXISTS tenants (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    company_name VARCHAR(100) NOT NULL,
    company_code VARCHAR(20) NOT NULL UNIQUE,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS sites (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    location VARCHAR(200),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id)
);

CREATE TABLE IF NOT EXISTS devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id INTEGER NOT NULL,
    site_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    protocol_type VARCHAR(50) NOT NULL,
    endpoint VARCHAR(255) NOT NULL,
    config TEXT,
    is_enabled INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (tenant_id) REFERENCES tenants(id),
    FOREIGN KEY (site_id) REFERENCES sites(id)
);

CREATE TABLE IF NOT EXISTS data_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER NOT NULL,
    name VARCHAR(100) NOT NULL,
    address INTEGER NOT NULL,
    data_type VARCHAR(20) NOT NULL,
    unit VARCHAR(20),
    is_enabled INTEGER DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (device_id) REFERENCES devices(id)
);

-- 테스트 데이터 삽입
INSERT INTO tenants (id, company_name, company_code) VALUES 
    (1, 'Test Company', 'TEST001');

INSERT INTO sites (id, tenant_id, name, location) VALUES 
    (1, 1, 'Test Site', 'Test Location');

INSERT INTO devices (id, tenant_id, site_id, name, protocol_type, endpoint, config) VALUES 
    (1, 1, 1, 'Test PLC', 'MODBUS_TCP', '192.168.1.10:502', '{"slave_id": 1}'),
    (2, 1, 1, 'Test Sensor', 'MQTT', 'mqtt://192.168.1.20:1883', '{"topic": "sensors/temp"}');

INSERT INTO data_points (id, device_id, name, address, data_type, unit) VALUES 
    (1, 1, 'Temperature', 40001, 'int16', '°C'),
    (2, 1, 'Pressure', 40002, 'uint16', 'Pa'),
    (3, 2, 'Humidity', 1, 'float', '%');

.print "✅ 테스트 DB 생성 완료 - devices: 2개, data_points: 3개"
SQLEOF
fi

# SQLite DB 생성
sqlite3 db/pulseone_test.db < setup_test_db.sql

echo -e "${GREEN}✅ SQLite 테스트 DB 생성 완료: ./db/pulseone_test.db${NC}"

# =============================================================================
# 3. 설정 파일 생성 확인
# =============================================================================

echo -e "${BLUE}⚙️  테스트 설정 파일 확인${NC}"

config_files=(".env" "database.env" "redis.env")
missing_files=()

for file in "${config_files[@]}"; do
    if [ ! -f "config/$file" ]; then
        missing_files+=("$file")
    else
        echo -e "${GREEN}✅ config/$file 존재${NC}"
    fi
done

if [ ${#missing_files[@]} -gt 0 ]; then
    echo -e "${YELLOW}⚠️  누락된 설정 파일들:${NC}"
    for file in "${missing_files[@]}"; do
        echo -e "   - config/$file"
    done
    echo -e "${YELLOW}   artifacts에서 생성하여 저장해주세요${NC}"
fi

# =============================================================================
# 4. Makefile 확인
# =============================================================================

echo -e "${BLUE}🔧 Makefile 확인${NC}"

if [ ! -f "Makefile" ]; then
    echo -e "${YELLOW}⚠️  Makefile이 없습니다${NC}"
    echo -e "${YELLOW}   artifacts에서 생성하여 저장해주세요${NC}"
else
    echo -e "${GREEN}✅ Makefile 존재${NC}"
fi

# =============================================================================
# 5. 권한 설정
# =============================================================================

echo -e "${BLUE}🔐 권한 설정${NC}"

# 실행 권한 부여
chmod +x setup_test_environment.sh 2>/dev/null || true
chmod +x run_tests.sh 2>/dev/null || true

# DB 파일 권한
chmod 644 db/pulseone_test.db

echo -e "${GREEN}✅ 권한 설정 완료${NC}"

# =============================================================================
# 6. 테스트 실행 준비 완료 메시지
# =============================================================================

echo -e "${GREEN}🎉 테스트 환경 구성 완료!${NC}"
echo -e "${YELLOW}================================${NC}"
echo ""
echo -e "${BLUE}📁 생성된 구조:${NC}"
echo "   collector/tests/"
echo "   ├── config/"
echo "   │   ├── .env              # 메인 테스트 설정"
echo "   │   ├── database.env      # DB 전용 설정"  
echo "   │   └── redis.env         # Redis 전용 설정"
echo "   ├── db/"
echo "   │   └── pulseone_test.db  # SQLite 테스트 DB"
echo "   ├── logs/                 # 테스트 로그"
echo "   ├── backup/               # DB 백업"
echo "   └── bin/                  # 컴파일된 실행 파일"
echo ""
echo -e "${BLUE}🚀 다음 단계:${NC}"
echo "   1. artifacts에서 설정 파일들 저장:"
echo "      - tests/config/.env"
echo "      - tests/config/database.env" 
echo "      - tests/config/redis.env"
echo ""
echo "   2. Makefile 저장:"
echo "      - tests/Makefile"
echo ""
echo "   3. 테스트 실행:"
echo "      make check-deps     # 의존성 확인"
echo "      make quick          # 빠른 테스트"
echo "      make run-all        # 전체 테스트"
echo ""
echo -e "${BLUE}💾 DB 확인:${NC}"
echo "   sqlite3 db/pulseone_test.db \"SELECT * FROM devices;\""
echo ""
echo -e "${GREEN}설정 완료! 이제 기존 ConfigManager가 테스트 설정을 읽을 수 있습니다.${NC}"