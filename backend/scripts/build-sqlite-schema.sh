#!/bin/bash
# =============================================================================
# PulseOne SQLite Schema Builder (Complete Version - Corrected Path)
# backend/scripts/build-sqlite-schema.sh
# SQLite 데이터베이스 스키마를 빌드하는 완전한 스크립트
# =============================================================================

# 색상 코드
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

# 설정 (올바른 경로)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SCHEMA_DIR="$PROJECT_ROOT/backend/lib/database/schemas"
DATA_DIR="$PROJECT_ROOT/backend/lib/database/data"
DB_DIR="$PROJECT_ROOT/data/db"
DB_FILE="$DB_DIR/pulseone.db"

echo -e "${CYAN}🗄️ PulseOne SQLite Schema Builder${NC}"
echo "=============================================="
echo -e "${MAGENTA}📅 실행 시간: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
echo -e "${YELLOW}📍 데이터베이스 경로: ${NC}$DB_FILE"

# =============================================================================
# 함수 정의
# =============================================================================

# 오류 체크 함수
check_error() {
    if [[ $? -ne 0 ]]; then
        echo -e "${RED}❌ 오류 발생: $1${NC}"
        cleanup_temp_files
        exit 1
    fi
}

# 파일 존재 확인 함수
check_file() {
    if [[ ! -f "$1" ]]; then
        echo -e "${RED}❌ 파일 없음: $1${NC}"
        return 1
    fi
    return 0
}

# 임시 파일 정리 함수
cleanup_temp_files() {
    if [[ -n "$SQL_COMMANDS_FILE" ]]; then
        rm -f "$SQL_COMMANDS_FILE" "$SQL_COMMANDS_FILE.bak"
    fi
}

# SQLite 설치 확인
check_sqlite() {
    if ! command -v sqlite3 &> /dev/null; then
        echo -e "${RED}❌ SQLite3가 설치되지 않았습니다.${NC}"
        echo -e "${YELLOW}설치 방법:${NC}"
        echo -e "${YELLOW}  macOS: brew install sqlite${NC}"
        echo -e "${YELLOW}  Ubuntu: sudo apt-get install sqlite3${NC}"
        echo -e "${YELLOW}  CentOS: sudo yum install sqlite${NC}"
        exit 1
    fi
    
    local version=$(sqlite3 --version | cut -d' ' -f1)
    echo -e "${GREEN}  ✅ SQLite3 버전: $version${NC}"
}

# 스키마 파일 확인
check_schema_files() {
    local missing_files=()
    
    for file in "${SCHEMA_FILES[@]}"; do
        local file_path="$SCHEMA_DIR/$file"
        if check_file "$file_path"; then
            echo -e "${GREEN}  ✅ $file${NC}"
        else
            missing_files+=("$file")
        fi
    done
    
    if [[ ${#missing_files[@]} -gt 0 ]]; then
        echo -e "${RED}❌ 누락된 스키마 파일들:${NC}"
        for file in "${missing_files[@]}"; do
            echo -e "${RED}  - $file${NC}"
        done
        echo -e "${YELLOW}먼저 create-schema-files.sh를 실행하세요.${NC}"
        exit 1
    fi
}

# 백업 함수
backup_existing_db() {
    if [[ -f "$DB_FILE" ]]; then
        local backup_file="$DB_DIR/pulseone_backup_$(date +%Y%m%d_%H%M%S).db"
        echo -e "${YELLOW}📦 기존 DB 백업: $(basename "$backup_file")${NC}"
        cp "$DB_FILE" "$backup_file"
        check_error "백업 실패"
        
        # 기존 DB 파일 크기 표시
        local old_size=$(du -h "$DB_FILE" | cut -f1)
        echo -e "${CYAN}  📊 기존 DB 크기: $old_size${NC}"
    fi
}

# SQLite 명령 파일 생성
create_sql_commands() {
    SQL_COMMANDS_FILE="$DB_DIR/build_commands.sql"
    
    echo -e "${BLUE}📝 SQLite 명령 파일 생성...${NC}"
    
    cat > "$SQL_COMMANDS_FILE" << 'END_OF_SQL'
-- =============================================================================
-- PulseOne SQLite Schema Builder Commands
-- Generated automatically - Do not edit manually
-- =============================================================================

-- SQLite 성능 최적화 설정
PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA cache_size = 10000;
PRAGMA temp_store = memory;

-- 빌드 시작 로그
SELECT '🏗️ Schema build started at: ' || datetime('now', 'localtime') as build_info;

-- 스키마 파일들 순차 실행
.read $SCHEMA_DIR/01-core-tables.sql
SELECT '✅ Core tables loaded' as progress;

.read $SCHEMA_DIR/02-users-sites.sql
SELECT '✅ Users and sites loaded' as progress;

.read $SCHEMA_DIR/03-device-tables.sql
SELECT '✅ Device tables loaded' as progress;

.read $SCHEMA_DIR/04-virtual-points.sql
SELECT '✅ Virtual points loaded' as progress;

.read $SCHEMA_DIR/05-alarm-tables.sql
SELECT '✅ Alarm tables loaded' as progress;

.read $SCHEMA_DIR/06-log-tables.sql
SELECT '✅ Log tables loaded' as progress;

.read $SCHEMA_DIR/07-indexes.sql
SELECT '✅ Indexes created' as progress;

-- 초기 데이터 삽입
.read $DATA_DIR/initial-data.sql
SELECT '✅ Initial data loaded' as progress;

-- 완료 로그
SELECT '🎉 Schema build completed at: ' || datetime('now', 'localtime') as build_info;

-- 결과 확인
SELECT '📊 Database Statistics:' as info;
SELECT 'Tables: ' || COUNT(*) as table_count FROM sqlite_master WHERE type='table';
SELECT 'Indexes: ' || COUNT(*) as index_count FROM sqlite_master WHERE type='index' AND name NOT LIKE 'sqlite_%';

-- 테이블 목록
.headers on
.mode table
SELECT name as 'Created Tables' FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name;
END_OF_SQL

    # 변수 치환
    sed -i.bak "s|\$SCHEMA_DIR|$SCHEMA_DIR|g" "$SQL_COMMANDS_FILE"
    sed -i.bak "s|\$DATA_DIR|$DATA_DIR|g" "$SQL_COMMANDS_FILE"
    
    echo -e "${GREEN}  ✅ SQLite 명령 파일 생성 완료${NC}"
}

# 데이터베이스 실행
execute_schema_build() {
    echo -e "${BLUE}⚙️ SQLite 스키마 빌드 실행...${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    
    if sqlite3 "$DB_FILE" < "$SQL_COMMANDS_FILE"; then
        echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
        echo -e "${GREEN}✅ 스키마 빌드 성공!${NC}"
    else
        echo -e "${RED}❌ 스키마 빌드 실패${NC}"
        cleanup_temp_files
        exit 1
    fi
}

# 결과 검증
verify_database() {
    echo -e "\n${YELLOW}🔍 데이터베이스 검증 중...${NC}"
    
    # 파일 존재 및 크기 확인
    if [[ -f "$DB_FILE" ]]; then
        local file_size=$(du -h "$DB_FILE" | cut -f1)
        echo -e "${GREEN}  ✅ 데이터베이스 파일 생성됨${NC}"
        echo -e "${CYAN}  💾 파일 크기: $file_size${NC}"
    else
        echo -e "${RED}  ❌ 데이터베이스 파일이 생성되지 않음${NC}"
        return 1
    fi
    
    # 테이블 개수 확인
    local table_count=$(sqlite3 "$DB_FILE" "SELECT COUNT(*) FROM sqlite_master WHERE type='table';" 2>/dev/null || echo "0")
    if [[ $table_count -gt 0 ]]; then
        echo -e "${GREEN}  ✅ 테이블 생성됨: $table_count개${NC}"
    else
        echo -e "${RED}  ❌ 테이블이 생성되지 않음${NC}"
        return 1
    fi
    
    # 기본 데이터 확인
    local tenant_count=$(sqlite3 "$DB_FILE" "SELECT COUNT(*) FROM tenants;" 2>/dev/null || echo "0")
    local user_count=$(sqlite3 "$DB_FILE" "SELECT COUNT(*) FROM users;" 2>/dev/null || echo "0")
    
    if [[ $tenant_count -gt 0 && $user_count -gt 0 ]]; then
        echo -e "${GREEN}  ✅ 초기 데이터 로드됨 (테넌트: $tenant_count, 사용자: $user_count)${NC}"
    else
        echo -e "${YELLOW}  ⚠️ 초기 데이터 확인 필요${NC}"
    fi
    
    return 0
}

# 상세 정보 표시
show_database_info() {
    echo -e "\n${CYAN}📊 데이터베이스 상세 정보${NC}"
    echo "=============================================="
    echo -e "${CYAN}📁 경로: ${NC}$DB_FILE"
    echo -e "${CYAN}📅 생성일: ${NC}$(date '+%Y-%m-%d %H:%M:%S')"
    
    # 테이블별 데이터 개수
    echo -e "\n${YELLOW}📋 테이블별 데이터 현황:${NC}"
    
    local tables=(
        "tenants:👥"
        "users:👤" 
        "sites:🏭"
        "devices:📱"
        "data_points:📊"
        "virtual_points:🔮"
        "alarm_definitions:🚨"
    )
    
    for table_info in "${tables[@]}"; do
        local table_name="${table_info%:*}"
        local icon="${table_info#*:}"
        local count=$(sqlite3 "$DB_FILE" "SELECT COUNT(*) FROM $table_name;" 2>/dev/null || echo "0")
        printf "${CYAN}  %s %-20s${NC} %s\n" "$icon" "$table_name:" "$count"
    done
}

# 사용법 안내
show_usage_guide() {
    echo -e "\n${YELLOW}💡 사용법 안내${NC}"
    echo "=============================================="
    echo -e "${YELLOW}🔧 SQLite CLI 연결:${NC}"
    echo "  sqlite3 \"$DB_FILE\""
    echo "  .tables"
    echo "  .schema tenants"
    echo "  SELECT * FROM tenants;"
    echo ""
    echo -e "${YELLOW}🌐 웹 브라우저 도구:${NC}"
    echo "  DB Browser for SQLite: https://sqlitebrowser.org/"
    echo ""
    echo -e "${YELLOW}📝 Node.js 환경변수:${NC}"
    echo "  export SQLITE_PATH=\"$DB_FILE\""
    echo "  npm start"
    echo ""
    echo -e "${YELLOW}🔄 재빌드:${NC}"
    echo "  ./backend/scripts/build-sqlite-schema.sh"
}

# =============================================================================
# 메인 실행 로직
# =============================================================================

# 스키마 파일 목록 정의
SCHEMA_FILES=(
    "01-core-tables.sql"
    "02-users-sites.sql"
    "03-device-tables.sql"
    "04-virtual-points.sql"
    "05-alarm-tables.sql"
    "06-log-tables.sql"
    "07-indexes.sql"
)

echo -e "\n${BLUE}🔍 환경 확인 중...${NC}"

# 1. SQLite 설치 확인
check_sqlite

# 2. 스키마 디렉토리 확인
if [[ ! -d "$SCHEMA_DIR" ]]; then
    echo -e "${RED}❌ 스키마 디렉토리 없음: $SCHEMA_DIR${NC}"
    echo -e "${YELLOW}먼저 create-schema-files.sh를 실행하세요.${NC}"
    exit 1
fi
echo -e "${GREEN}  ✅ 스키마 디렉토리 확인${NC}"

# 3. 스키마 파일들 확인
echo -e "${YELLOW}📋 스키마 파일 확인...${NC}"
check_schema_files

# 4. 초기 데이터 파일 확인
DATA_FILE="$DATA_DIR/initial-data.sql"
if check_file "$DATA_FILE"; then
    echo -e "${GREEN}  ✅ initial-data.sql${NC}"
else
    echo -e "${RED}❌ 초기 데이터 파일 없음: $DATA_FILE${NC}"
    exit 1
fi

# 5. 데이터베이스 디렉토리 생성
echo -e "\n${YELLOW}📁 데이터베이스 디렉토리 준비...${NC}"
mkdir -p "$DB_DIR"
check_error "디렉토리 생성 실패"
echo -e "${GREEN}  ✅ 디렉토리 준비 완료: $DB_DIR${NC}"

# 6. 기존 데이터베이스 백업
backup_existing_db

# 7. SQLite 명령 파일 생성
create_sql_commands

# 8. 스키마 빌드 실행
execute_schema_build

# 9. 결과 검증
if verify_database; then
    echo -e "${GREEN}✅ 데이터베이스 검증 성공${NC}"
else
    echo -e "${RED}❌ 데이터베이스 검증 실패${NC}"
    cleanup_temp_files
    exit 1
fi

# 10. 임시 파일 정리
cleanup_temp_files

# 11. 상세 정보 표시
show_database_info

# 12. 사용법 안내
show_usage_guide

# 13. 완료 메시지
echo -e "\n${GREEN}🎉 PulseOne SQLite 데이터베이스 구축 완료!${NC}"
echo -e "${MAGENTA}📅 완료 시간: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"