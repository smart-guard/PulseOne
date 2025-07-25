// =============================================================================
// collector/src/Utils/ConfigManager.cpp - 완성된 통합 버전
// 기존 코드 + 확장된 모듈별 설정 템플릿 생성
// =============================================================================

#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <cstring>

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

// =============================================================================
// 메인 초기화 (데드락 해결된 버전)
// =============================================================================

void ConfigManager::initialize() {
    PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::INFO, 
        "🔍 ConfigManager 초기화 시작...");
    
    // 1. 설정 디렉토리 찾기
    configDir_ = findConfigDirectory();
    if (configDir_.empty()) {
        PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::ERROR, 
            "❌ 설정 디렉토리를 찾을 수 없습니다!");
        return;
    }
    
    PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::INFO, 
        "✅ 설정 디렉토리: " + configDir_);
    
    // 2. 설정 파일 확인 및 생성
    try {
        ensureConfigFilesExist();
    } catch (const std::exception& e) {
        PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::WARN, 
            "⚠️ 설정 파일 생성 중 오류: " + std::string(e.what()));
    }
    
    // 3. envFilePath 설정
    envFilePath = configDir_ + "/.env";
    
    // 4. 설정 파일들 로드
    loadMainConfig();
    loadAdditionalConfigs();
    
    // 5. 데이터 디렉토리 설정
    dataDir_ = findDataDirectory();
    try {
        ensureDataDirectories();
    } catch (const std::exception& e) {
        PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::WARN, 
            "⚠️ 데이터 디렉토리 생성 중 오류: " + std::string(e.what()));
    }
    
    // 6. 변수 확장 실행 (중요!)
    try {
        expandAllVariables();
        PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::INFO, 
            "✅ 변수 확장 완료");
    } catch (const std::exception& e) {
        PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::WARN, 
            "⚠️ 변수 확장 중 오류: " + std::string(e.what()));
    }
    
    PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::INFO, 
        "✅ ConfigManager 초기화 완료 - " + std::to_string(configMap.size()) + "개 설정 로드됨");
}

void ConfigManager::reload() {
    PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::INFO, "🔄 ConfigManager 재로딩 시작...");
    
    {
        std::lock_guard<std::mutex> lock(configMutex);
        configMap.clear();
        loadedFiles_.clear();
        searchLog_.clear();
    }
    
    initialize();
}

// =============================================================================
// 경로 찾기 (기존 코드 유지)
// =============================================================================

std::string ConfigManager::findConfigDirectory() {
    searchLog_.clear();
    
    // 1. 환경변수 확인 (최우선)
    const char* env_config = std::getenv("PULSEONE_CONFIG_DIR");
    if (env_config && strlen(env_config) > 0 && directoryExists(env_config)) {
        searchLog_.push_back("✅ 환경변수: " + std::string(env_config));
        return std::string(env_config);
    } else if (env_config) {
        searchLog_.push_back("❌ 환경변수 (존재하지 않음): " + std::string(env_config));
    }
    
    // 2. 기본 경로들 확인
    std::vector<std::string> candidates = {
        "./config", "../config", "../../config",
        "/opt/pulseone/config", "/etc/pulseone"
    };
    
    for (const auto& candidate : candidates) {
        try {
            if (directoryExists(candidate)) {
                std::string canonical_path = std::filesystem::canonical(candidate).string();
                searchLog_.push_back("✅ 발견: " + candidate + " → " + canonical_path);
                return canonical_path;
            } else {
                searchLog_.push_back("❌ 없음: " + candidate);
            }
        } catch (const std::exception& e) {
            searchLog_.push_back("❌ 오류: " + candidate + " (" + e.what() + ")");
        }
    }
    
    searchLog_.push_back("❌ 설정 디렉토리를 찾을 수 없음");
    return "";
}

bool ConfigManager::directoryExists(const std::string& path) {
    try {
        return std::filesystem::exists(path) && std::filesystem::is_directory(path);
    } catch (const std::exception&) {
        return false;
    }
}

std::string ConfigManager::findDataDirectory() {
    // 1. 환경변수 확인
    const char* env_data = std::getenv("PULSEONE_DATA_DIR");
    if (env_data && strlen(env_data) > 0) {
        return std::string(env_data);
    }
    
    // 2. 설정에서 확인 (데드락 해결: 뮤텍스 없이 직접 접근)
    auto it = configMap.find("DATA_DIR");
    if (it != configMap.end() && !it->second.empty()) {
        return expandVariables(it->second);
    }
    
    // 3. 기본값
    return "./data";
}

// =============================================================================
// 설정 파일 로딩 (기존 코드 유지)
// =============================================================================

void ConfigManager::loadMainConfig() {
    std::string main_env_path = configDir_ + "/.env";
    
    if (std::filesystem::exists(main_env_path)) {
        loadConfigFile(main_env_path);
        PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::INFO, "✅ 메인 설정 로드: .env");
    } else {
        PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::WARN, "⚠️ 메인 설정 파일 없음: .env");
    }
}

void ConfigManager::loadAdditionalConfigs() {
    PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::INFO, 
        "🔍 추가 설정 파일 확인 시작");
    
    // 데드락 해결: configMap 직접 접근
    std::string config_files;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        auto it = configMap.find("CONFIG_FILES");
        config_files = (it != configMap.end()) ? it->second : "";
    }
    
    if (config_files.empty()) {
        PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::INFO, 
            "ℹ️ 추가 설정 파일 없음 (CONFIG_FILES 비어있음)");
        return;
    }
    
    // 콤마로 분리된 파일 목록 파싱
    std::vector<std::string> file_list;
    std::stringstream ss(config_files);
    std::string item;
    
    while (std::getline(ss, item, ',') && file_list.size() < 10) {
        // 앞뒤 공백 제거
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        
        if (!item.empty()) {
            file_list.push_back(item);
        }
    }
    
    // 각 파일 로드
    for (const auto& filename : file_list) {
        std::string full_path = configDir_ + "/" + filename;
        
        try {
            if (std::filesystem::exists(full_path)) {
                loadConfigFile(full_path);
                PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::INFO, 
                    "✅ 추가 설정 로드: " + filename);
            } else {
                PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::INFO, 
                    "ℹ️ 추가 설정 파일 없음: " + filename);
            }
        } catch (const std::exception& e) {
            PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::WARN, 
                "⚠️ 설정 파일 로드 실패: " + filename + " - " + e.what());
        }
    }
    
    PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::INFO, 
        "✅ 추가 설정 파일 확인 완료");
}

void ConfigManager::loadConfigFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::ERROR, 
            "❌ 파일 열기 실패: " + filepath);
        return;
    }
    
    std::string line;
    int line_count = 0;
    int parsed_count = 0;
    
    // 뮤텍스 스코프 제한
    {
        std::lock_guard<std::mutex> lock(configMutex);
        
        while (std::getline(file, line)) {
            line_count++;
            size_t original_size = configMap.size();
            
            // 각 라인을 안전하게 파싱
            try {
                parseLine(line);
                if (configMap.size() > original_size) {
                    parsed_count++;
                }
            } catch (const std::exception&) {
                // 파싱 실패 시 해당 라인만 건너뛰고 계속 진행
                continue;
            }
        }
        
        loadedFiles_.push_back(filepath);
    }
    
    file.close();
    
    PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::INFO, 
        "📄 " + filepath + " - " + std::to_string(parsed_count) + "/" + std::to_string(line_count) + " 라인 파싱됨");
}


// =============================================================================
// 변수 확장 (기존 코드 유지)
// =============================================================================

void ConfigManager::expandAllVariables() {
    std::map<std::string, std::string> temp_map;
    
    {
        std::lock_guard<std::mutex> lock(configMutex);
        temp_map = configMap;
    }
    
    bool changed = true;
    int max_iterations = 3;
    
    for (int i = 0; i < max_iterations && changed; ++i) {
        changed = false;
        
        for (auto& [key, value] : temp_map) {
            std::string expanded = expandVariables(value);
            if (expanded != value) {
                value = expanded;
                changed = true;
            }
        }
    }
    
    // 결과를 다시 configMap에 저장
    {
        std::lock_guard<std::mutex> lock(configMutex);
        configMap = temp_map;
    }
}

std::string ConfigManager::expandVariables(const std::string& value) const {
    std::string result = value;
    size_t pos = 0;
    int max_replacements = 10;  // 더 많은 반복 허용
    
    while ((pos = result.find("${", pos)) != std::string::npos && max_replacements-- > 0) {
        size_t end_pos = result.find("}", pos + 2);
        if (end_pos == std::string::npos) {
            pos += 2;  // ${가 있지만 }가 없는 경우 건너뛰기
            continue;
        }
        
        std::string var_name = result.substr(pos + 2, end_pos - pos - 2);
        std::string var_value;
        
        // 1. configMap에서 찾기 (뮤텍스 없이 - 이미 호출된 상태에서)
        auto it = configMap.find(var_name);
        if (it != configMap.end()) {
            var_value = it->second;
        } else {
            // 2. 환경변수에서 찾기
            const char* env_value = std::getenv(var_name.c_str());
            if (env_value) {
                var_value = std::string(env_value);
            }
        }
        
        // 3. 치환 (값이 없어도 빈 문자열로 치환)
        result.replace(pos, end_pos - pos + 1, var_value);
        pos += var_value.length();
    }
    
    return result;
}

// =============================================================================
// 기본 인터페이스 (기존 100% 호환)
// =============================================================================

void ConfigManager::parseLine(const std::string& line) {
    // 빈 줄이나 주석 건너뛰기
    if (line.empty() || line[0] == '#') return;

    size_t eqPos = line.find('=');
    if (eqPos == std::string::npos) return;

    std::string key = line.substr(0, eqPos);
    std::string value = line.substr(eqPos + 1);

    // 키 정리 (공백 제거)
    key.erase(std::remove_if(key.begin(), key.end(), ::isspace), key.end());
    
    // 값 정리 (앞뒤 공백 제거)
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);
    
    // 따옴표 제거 (전체가 따옴표로 감싸진 경우만)
    if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.length() - 2);
    } else if (value.length() >= 2 && value.front() == '\'' && value.back() == '\'') {
        value = value.substr(1, value.length() - 2);
    }

    // 키가 유효한 경우에만 저장 (영문자, 숫자, 언더스코어만 허용)
    if (!key.empty()) {
        bool valid_key = true;
        for (char c : key) {
            if (!std::isalnum(c) && c != '_') {
                valid_key = false;
                break;
            }
        }
        
        if (valid_key) {
            configMap[key] = value;
        }
    }
}

std::string ConfigManager::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(configMutex);
    auto it = configMap.find(key);
    return (it != configMap.end()) ? it->second : "";
}

std::string ConfigManager::getOrDefault(const std::string& key, const std::string& defaultValue) const {
    std::lock_guard<std::mutex> lock(configMutex);
    auto it = configMap.find(key);
    return (it != configMap.end()) ? it->second : defaultValue;
}

void ConfigManager::set(const std::string& key, const std::string& value) {
    {
        std::lock_guard<std::mutex> lock(configMutex);
        configMap[key] = value;
    }
    
    // 변수가 포함된 경우 자동 확장 실행
    if (value.find("${") != std::string::npos) {
        try {
            expandAllVariables();
        } catch (const std::exception&) {
            // 확장 실패 시 무시하고 계속 진행
        }
    }
}

bool ConfigManager::hasKey(const std::string& key) const {
    std::lock_guard<std::mutex> lock(configMutex);
    return configMap.find(key) != configMap.end();
}

std::map<std::string, std::string> ConfigManager::listAll() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return configMap;
}

// =============================================================================
// 편의 기능들 (기존 코드 유지)
// =============================================================================

int ConfigManager::getInt(const std::string& key, int defaultValue) const {
    std::string value = get(key);
    if (value.empty()) return defaultValue;
    try { return std::stoi(value); } catch (...) { return defaultValue; }
}

bool ConfigManager::getBool(const std::string& key, bool defaultValue) const {
    std::string value = get(key);
    if (value.empty()) {
        return defaultValue;
    }
    
    // 대소문자 변환
    std::string lower_value = value;
    std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);
    
    // true 값들
    if (lower_value == "true" || lower_value == "yes" || lower_value == "1" || lower_value == "on") {
        return true;
    }
    
    // false 값들
    if (lower_value == "false" || lower_value == "no" || lower_value == "0" || lower_value == "off") {
        return false;
    }
    
    // 인식할 수 없는 값은 기본값 반환
    return defaultValue;
}

double ConfigManager::getDouble(const std::string& key, double defaultValue) const {
    std::string value = get(key);
    if (value.empty()) return defaultValue;
    try { return std::stod(value); } catch (...) { return defaultValue; }
}

std::string ConfigManager::getDataDirectory() const {
    return dataDir_.empty() ? "./data" : dataDir_;
}

std::string ConfigManager::getSQLiteDbPath() const {
    return getDataDirectory() + "/db/pulseone.db";
}

std::string ConfigManager::getBackupDirectory() const {
    return getDataDirectory() + "/backup";
}

void ConfigManager::printConfigSearchLog() const {
    std::cout << "\n🔍 ConfigManager 경로 검색 로그:\n";
    for (const auto& log_entry : searchLog_) {
        std::cout << log_entry << "\n";
    }
    std::cout << "\n📄 로드된 설정 파일들:\n";
    for (const auto& file : loadedFiles_) {
        std::cout << "  " + file << "\n";
    }
    std::cout << "\n⚙️ 총 " << configMap.size() << "개 설정 항목 로드됨\n";
}

// =============================================================================
// 템플릿 생성 (확장된 버전)
// =============================================================================

void ConfigManager::ensureConfigFilesExist() {
    PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::INFO, 
        "🔍 모든 설정 파일 존재 여부 확인 중...");
    
    try {
        // 1. 메인 .env 파일 확인/생성
        std::string main_env_path = configDir_ + "/.env";
        if (!std::filesystem::exists(main_env_path)) {
            PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::WARN, 
                "⚠️ 메인 설정 파일 없음, 기본 템플릿 생성: .env");
            createMainEnvFile();
        }
        
        // 2. 모듈별 설정 파일들 생성
        createSecretsDirectory();
        createDatabaseEnvFile();
        createTimeseriesEnvFile();
        createRedisEnvFile();
        createMessagingEnvFile();
        createSecurityEnvFile();  // 새로 추가
        
        PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::INFO, 
            "✅ 모든 설정 파일 확인 완료");
            
    } catch (const std::exception& e) {
        PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::WARN, 
            "⚠️ 설정 파일 확인 중 오류: " + std::string(e.what()) + " (계속 진행)");
    }
}

void ConfigManager::createMainEnvFile() {
    std::string content = R"(# =============================================================================
# PulseOne 메인 설정 파일 (.env) - 자동 생성됨
# =============================================================================

# 환경 설정
NODE_ENV=development
LOG_LEVEL=info

# 기본 데이터베이스 설정
DATABASE_TYPE=SQLITE

# 기본 디렉토리 설정
DATA_DIR=./data

# 시스템 설정
MAX_WORKER_THREADS=4
DEFAULT_TIMEOUT_MS=5000

# 추가 설정 파일들 (모듈별 분리)
CONFIG_FILES=database.env,timeseries.env,redis.env,messaging.env,security.env
)";
    
    createFileFromTemplate(configDir_ + "/.env", content);
}

void ConfigManager::createDatabaseEnvFile() {
    std::string content = R"(# =============================================================================
# 관계형 데이터베이스 설정 (database.env) - 자동 생성됨
# SQLite, PostgreSQL, MySQL, MariaDB, MS SQL Server 전용
# =============================================================================

# 메인 데이터베이스 타입 선택 (SQLITE/POSTGRESQL/MYSQL/MARIADB/MSSQL)
DATABASE_TYPE=SQLITE

# =============================================================================
# SQLite 설정 (기본값, 가장 안전)
# =============================================================================
SQLITE_PATH=${DATA_DIR}/db/pulseone.db
SQLITE_BACKUP_PATH=${DATA_DIR}/backup/
SQLITE_LOGS_PATH=${DATA_DIR}/logs/
SQLITE_TEMP_PATH=${DATA_DIR}/temp/

# SQLite 성능 설정
SQLITE_JOURNAL_MODE=WAL
SQLITE_SYNCHRONOUS=NORMAL
SQLITE_CACHE_SIZE=2000
SQLITE_BUSY_TIMEOUT_MS=5000
SQLITE_FOREIGN_KEYS=true

# =============================================================================
# PostgreSQL 설정
# =============================================================================
POSTGRES_PRIMARY_ENABLED=false
POSTGRES_PRIMARY_HOST=localhost
POSTGRES_PRIMARY_PORT=5432
POSTGRES_PRIMARY_DB=pulseone_main
POSTGRES_PRIMARY_USER=postgres
POSTGRES_PRIMARY_PASSWORD_FILE=${SECRETS_DIR}/postgres_primary.key

# PostgreSQL 연결 풀 설정
POSTGRES_POOL_MIN=2
POSTGRES_POOL_MAX=10
POSTGRES_POOL_IDLE_TIMEOUT_SEC=30

# =============================================================================
# MySQL/MariaDB 설정
# =============================================================================
MYSQL_ENABLED=false
MYSQL_TYPE=mysql                    # mysql 또는 mariadb
MYSQL_HOST=localhost
MYSQL_PORT=3306
MYSQL_DATABASE=pulseone
MYSQL_USER=user
MYSQL_PASSWORD_FILE=${SECRETS_DIR}/mysql.key

# MySQL/MariaDB 연결 설정
MYSQL_CHARSET=utf8mb4
MYSQL_SSL_MODE=DISABLED
MYSQL_CONNECT_TIMEOUT=10
MYSQL_AUTO_RECONNECT=true

# =============================================================================
# MS SQL Server 설정
# =============================================================================
MSSQL_ENABLED=false
MSSQL_HOST=localhost
MSSQL_PORT=1433
MSSQL_DATABASE=pulseone
MSSQL_USER=sa
MSSQL_PASSWORD_FILE=${SECRETS_DIR}/mssql.key

# MSSQL 보안 설정
MSSQL_ENCRYPT=false
MSSQL_TRUST_SERVER_CERTIFICATE=true
MSSQL_CONNECTION_TIMEOUT=30

# =============================================================================
# 공통 연결 풀 및 성능 설정
# =============================================================================
DB_CONNECTION_TIMEOUT_MS=30000
DB_QUERY_TIMEOUT_MS=15000
DB_RETRY_ATTEMPTS=3
DB_RETRY_DELAY_MS=1000

# =============================================================================
# 백업 및 유지보수 설정
# =============================================================================
DB_BACKUP_ENABLED=true
DB_BACKUP_INTERVAL_HOURS=6
DB_BACKUP_RETENTION_DAYS=30
DB_VACUUM_ENABLED=true              # SQLite 전용
DB_VACUUM_INTERVAL_HOURS=24

# =============================================================================
# 로깅 및 모니터링
# =============================================================================
DB_QUERY_LOG_ENABLED=false         # 성능에 영향, 개발 시에만
DB_SLOW_QUERY_LOG_ENABLED=true
DB_SLOW_QUERY_THRESHOLD_MS=1000
DB_CONNECTION_HEALTH_CHECK_INTERVAL_SEC=60

# =============================================================================
# 외부 시스템 연결 (선택적)
# =============================================================================
# ERP 시스템 연결
ERP_DB_ENABLED=false
ERP_DB_TYPE=mysql
ERP_DB_HOST=erp.company.com
ERP_DB_PORT=3306
ERP_DB_DATABASE=erp_system
ERP_DB_USER=integration_user
ERP_DB_PASSWORD_FILE=${SECRETS_DIR}/erp_db.key

# MES 시스템 연결
MES_DB_ENABLED=false
MES_DB_TYPE=mssql
MES_DB_HOST=mes.factory.com
MES_DB_PORT=1433
MES_DB_DATABASE=MES_Production
MES_DB_USER=pulseone_reader
MES_DB_PASSWORD_FILE=${SECRETS_DIR}/mes_db.key
)";

    createFileFromTemplate(configDir_ + "/database.env", content);
}

void ConfigManager::createTimeseriesEnvFile() {
    std::string content = R"(# =============================================================================
# 시계열 데이터베이스 설정 (timeseries.env) - 자동 생성됨
# InfluxDB 전용 설정
# =============================================================================

# InfluxDB 기본 설정
INFLUX_ENABLED=true
INFLUX_HOST=localhost
INFLUX_PORT=8086
INFLUX_ORG=pulseone
INFLUX_BUCKET=telemetry_data
INFLUX_TOKEN_FILE=${SECRETS_DIR}/influx_token.key
INFLUX_SSL=false

# InfluxDB 성능 설정
INFLUX_BATCH_SIZE=1000
INFLUX_FLUSH_INTERVAL_SEC=10
INFLUX_RETRY_INTERVAL_SEC=5
INFLUX_MAX_RETRIES=3
INFLUX_TIMEOUT_SEC=30

# InfluxDB 데이터 보존 정책
INFLUX_RETENTION_POLICY_DEFAULT=30d
INFLUX_RETENTION_POLICY_RAW=7d      # 원시 데이터
INFLUX_RETENTION_POLICY_HOURLY=90d  # 시간별 집계
INFLUX_RETENTION_POLICY_DAILY=2y    # 일별 집계

# InfluxDB 버킷별 설정
INFLUX_BUCKET_TELEMETRY=telemetry_data      # IoT 센서 데이터
INFLUX_BUCKET_ALARMS=alarm_events           # 알람 이벤트
INFLUX_BUCKET_PERFORMANCE=system_metrics    # 시스템 성능 지표
INFLUX_BUCKET_AUDIT=audit_logs              # 감사 로그

# InfluxDB 쿼리 최적화
INFLUX_QUERY_TIMEOUT_SEC=60
INFLUX_MAX_CONCURRENT_QUERIES=10
INFLUX_QUERY_MEMORY_LIMIT_MB=1024

# InfluxDB 백업 설정
INFLUX_BACKUP_ENABLED=true
INFLUX_BACKUP_INTERVAL_HOURS=12
INFLUX_BACKUP_RETENTION_DAYS=30
INFLUX_BACKUP_PATH=${DATA_DIR}/backup/influx

# 개발/테스트 설정
DEV_INFLUX_RESET_ON_START=false     # 주의: 모든 시계열 데이터 삭제
DEV_INFLUX_SEED_DATA=true           # 샘플 시계열 데이터 생성
DEV_INFLUX_DEBUG_QUERIES=false      # 모든 쿼리 로그
)";

    createFileFromTemplate(configDir_ + "/timeseries.env", content);
}

void ConfigManager::createRedisEnvFile() {
    std::string content = R"(# =============================================================================
# Redis 설정 (redis.env) - 자동 생성됨
# Redis 캐시 및 세션 관리 전용
# =============================================================================

# Primary Redis 서버
REDIS_PRIMARY_ENABLED=true
REDIS_PRIMARY_HOST=localhost
REDIS_PRIMARY_PORT=6379
REDIS_PRIMARY_DB=0
REDIS_PRIMARY_PASSWORD_FILE=${SECRETS_DIR}/redis_primary.key
REDIS_PRIMARY_SSL=false

# Redis 연결 설정
REDIS_CONNECTION_TIMEOUT_MS=5000
REDIS_COMMAND_TIMEOUT_MS=3000
REDIS_MAX_CONNECTIONS=20
REDIS_MIN_CONNECTIONS=5
REDIS_RETRY_ATTEMPTS=3
REDIS_RETRY_DELAY_MS=1000

# Redis 사용 용도별 DB 분리
REDIS_DB_CACHE=0                    # 일반 캐시
REDIS_DB_SESSIONS=1                 # 사용자 세션
REDIS_DB_REALTIME=2                 # 실시간 데이터
REDIS_DB_QUEUES=3                   # 작업 큐

# Redis 캐시 정책
REDIS_DEFAULT_TTL_SEC=3600          # 1시간
REDIS_SESSION_TTL_SEC=28800         # 8시간
REDIS_REALTIME_TTL_SEC=300          # 5분
REDIS_MAX_MEMORY=2gb
REDIS_EVICTION_POLICY=allkeys-lru

# Redis 성능 최적화
REDIS_SAVE_ENABLED=true             # RDB 스냅샷
REDIS_SAVE_INTERVAL_SEC=900         # 15분마다
REDIS_AOF_ENABLED=true              # AOF 로그
REDIS_AOF_SYNC=everysec

# Redis 백업 설정
REDIS_BACKUP_ENABLED=true
REDIS_BACKUP_INTERVAL_HOURS=6
REDIS_BACKUP_RETENTION_DAYS=7
REDIS_BACKUP_PATH=${DATA_DIR}/backup/redis

# 개발/테스트 설정
DEV_REDIS_FLUSH_ON_START=false      # 주의: 모든 캐시 삭제
DEV_REDIS_DEBUG=false               # Redis 명령어 로그
)";

    createFileFromTemplate(configDir_ + "/redis.env", content);
}

void ConfigManager::createMessagingEnvFile() {
    std::string content = R"(# =============================================================================
# 메시지 큐 설정 (messaging.env) - 자동 생성됨
# RabbitMQ, MQTT, Apache Kafka 지원
# =============================================================================

# 메시지 큐 타입 선택 (RABBITMQ/MQTT/KAFKA/DISABLED)
MESSAGING_TYPE=RABBITMQ

# =============================================================================
# RabbitMQ 설정
# =============================================================================
RABBITMQ_ENABLED=true
RABBITMQ_HOST=localhost
RABBITMQ_PORT=5672
RABBITMQ_MANAGEMENT_PORT=15672
RABBITMQ_USERNAME=guest
RABBITMQ_PASSWORD_FILE=${SECRETS_DIR}/rabbitmq.key
RABBITMQ_VHOST=/

# RabbitMQ 연결 설정
RABBITMQ_CONNECTION_TIMEOUT_MS=30000
RABBITMQ_HEARTBEAT_SEC=60
RABBITMQ_MAX_CONNECTIONS=10
RABBITMQ_RETRY_ATTEMPTS=5
RABBITMQ_RETRY_DELAY_MS=2000

# =============================================================================
# MQTT 설정 (IoT 디바이스용)
# =============================================================================
MQTT_ENABLED=false
MQTT_BROKER_HOST=localhost
MQTT_BROKER_PORT=1883
MQTT_CLIENT_ID=pulseone_collector
MQTT_USERNAME=pulseone
MQTT_PASSWORD_FILE=${SECRETS_DIR}/mqtt.key
MQTT_CLEAN_SESSION=true
MQTT_KEEP_ALIVE_SEC=60

# MQTT 토픽 설정
MQTT_TOPIC_PREFIX=pulseone/
MQTT_TOPIC_DEVICES=devices/+/data
MQTT_TOPIC_ALARMS=alarms/+/events
MQTT_QOS_LEVEL=1

# =============================================================================
# 공통 메시징 설정
# =============================================================================
MESSAGE_SERIALIZATION=JSON          # JSON/MSGPACK/PROTOBUF
MESSAGE_COMPRESSION=NONE            # NONE/GZIP/SNAPPY/LZ4
BATCH_SIZE=100
BATCH_TIMEOUT_MS=1000

# 개발/테스트 설정
DEV_MESSAGING_PURGE_ON_START=false  # 주의: 모든 메시지 삭제
DEV_MESSAGING_DEBUG=false           # 메시지 내용 로그
)";

    createFileFromTemplate(configDir_ + "/messaging.env", content);
}

void ConfigManager::createSecurityEnvFile() {
    std::string content = R"(# =============================================================================
# 보안 설정 (security.env) - 자동 생성됨
# 인증, 권한, 암호화, SSL/TLS 설정
# =============================================================================

# JWT 토큰 설정
JWT_SECRET_FILE=${SECRETS_DIR}/jwt_secret.key
JWT_ALGORITHM=HS256
JWT_ACCESS_TOKEN_EXPIRY=15m         # 15분
JWT_REFRESH_TOKEN_EXPIRY=7d         # 7일
JWT_ISSUER=pulseone

# 세션 보안
SESSION_SECRET_FILE=${SECRETS_DIR}/session_secret.key
SESSION_SECURE=false                # HTTPS에서만 true
SESSION_HTTP_ONLY=true
SESSION_SAME_SITE=strict

# 암호화 설정
ENCRYPTION_ALGORITHM=AES-256-GCM
ENCRYPTION_KEY_FILE=${SECRETS_DIR}/encryption.key
PASSWORD_HASH_ROUNDS=12             # bcrypt rounds
PASSWORD_MIN_LENGTH=8

# API 보안
API_RATE_LIMIT_ENABLED=true
API_RATE_LIMIT_WINDOW_MIN=15        # 15분 윈도우
API_RATE_LIMIT_MAX_REQUESTS=1000    # 윈도우당 최대 요청수
API_CORS_ENABLED=true

# SSL/TLS 설정
SSL_ENABLED=false                   # 프로덕션에서는 true
SSL_CERT_FILE=${SECRETS_DIR}/server.crt
SSL_KEY_FILE=${SECRETS_DIR}/server.key
SSL_PROTOCOLS=TLSv1.2,TLSv1.3

# 접근 제어
ACCESS_CONTROL_ENABLED=true
DEFAULT_USER_ROLE=viewer
LOCKOUT_THRESHOLD=5                 # 5회 실패 시 계정 잠금
LOCKOUT_DURATION_MIN=30             # 30분 잠금

# 감사 로그
AUDIT_LOG_ENABLED=true
AUDIT_LOG_LEVEL=info
AUDIT_LOG_RETENTION_DAYS=90
AUDIT_LOG_PATH=${DATA_DIR}/logs/audit.log

# 개발/테스트 설정
DEV_SECURITY_DISABLED=false        # 주의: 모든 보안 비활성화
DEV_SKIP_AUTH=false                 # 주의: 인증 건너뛰기
DEV_ALLOW_HTTP=true                 # HTTP 허용 (개발 시에만)
)";

    createFileFromTemplate(configDir_ + "/security.env", content);
}

void ConfigManager::createSecretsDirectory() {
    PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::INFO, 
        "🔐 보안 디렉토리 및 기본 파일들 생성 중...");
    
    std::string secrets_dir = configDir_ + "/secrets";
    
    try {
        if (!std::filesystem::exists(secrets_dir)) {
            std::filesystem::create_directories(secrets_dir);
            
            // 권한 설정 (유닉스 시스템)
#ifdef __unix__
            std::filesystem::permissions(secrets_dir, 
                std::filesystem::perms::owner_read | 
                std::filesystem::perms::owner_write | 
                std::filesystem::perms::owner_exec);
#endif
        }
        
        // README.md 생성
        std::string readme_content = R"(# Secrets Directory

이 디렉토리는 민감한 정보를 저장합니다:
- 데이터베이스 비밀번호
- API 토큰 및 키  
- SSL/TLS 인증서
- 암호화 키

## 보안 주의사항
1. 이 디렉토리의 파일들은 절대 Git에 커밋하지 마세요
2. 파일 권한을 600 (소유자만 읽기/쓰기)으로 설정하세요
3. 프로덕션에서는 별도의 키 관리 시스템 사용 권장

## 파일 목록
- postgres_primary.key - PostgreSQL 기본 DB 비밀번호
- mysql.key - MySQL/MariaDB 비밀번호
- mssql.key - MS SQL Server 비밀번호
- influx_token.key - InfluxDB 액세스 토큰
- redis_primary.key - Redis 기본 서버 비밀번호
- rabbitmq.key - RabbitMQ 비밀번호
- mqtt.key - MQTT 브로커 비밀번호
- jwt_secret.key - JWT 서명 키
- session_secret.key - 세션 암호화 키
- encryption.key - 데이터 암호화 키
)";
        createFileFromTemplate(secrets_dir + "/README.md", readme_content);
        
        // .gitignore 생성
        createFileFromTemplate(secrets_dir + "/.gitignore", "*\n!README.md\n!.gitignore\n");
        
        // 기본 키 파일들 생성 (빈 파일로)
        std::vector<std::string> key_files = {
            "postgres_primary.key", "mysql.key", "mssql.key", "influx_token.key",
            "redis_primary.key", "rabbitmq.key", "mqtt.key", 
            "jwt_secret.key", "session_secret.key", "encryption.key"
        };
        
        for (const auto& key_file : key_files) {
            std::string key_path = secrets_dir + "/" + key_file;
            if (!std::filesystem::exists(key_path)) {
                createFileFromTemplate(key_path, "# " + key_file + " - 실제 키/비밀번호로 교체하세요\n");
#ifdef __unix__
                std::filesystem::permissions(key_path, 
                    std::filesystem::perms::owner_read | 
                    std::filesystem::perms::owner_write);
#endif
            }
        }
        
        PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::INFO, 
            "✅ 보안 디렉토리 및 기본 파일들 생성 완료");
            
    } catch (const std::exception& e) {
        PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::WARN, 
            "⚠️ 보안 디렉토리 생성 실패: " + std::string(e.what()));
    }
}

bool ConfigManager::createFileFromTemplate(const std::string& filepath, const std::string& content) {
    try {
        if (std::filesystem::exists(filepath)) {
            return true;
        }
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }
        
        file << content;
        file.close();
        
        PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::INFO, 
            "✅ 설정 파일 생성: " + std::filesystem::path(filepath).filename().string());
        
        return true;
        
    } catch (const std::exception& e) {
        PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::WARN, 
            "⚠️ 파일 생성 중 예외: " + std::string(e.what()));
        return false;
    }
}

void ConfigManager::ensureDataDirectories() {
    if (dataDir_.empty()) {
        return;
    }
    
    std::vector<std::string> dirs = {"db", "logs", "backup", "temp"};
    
    for (const auto& dir : dirs) {
        try {
            std::string full_path = dataDir_ + "/" + dir;
            std::filesystem::create_directories(full_path);
        } catch (const std::exception&) {
            // 실패해도 계속 진행
        }
    }
}

// =============================================================================
// 새로 추가된 확장 메서드들
// =============================================================================

std::string ConfigManager::getActiveDatabaseType() const {
        std::string db_type = get("DATABASE_TYPE");
    
    // 빈 문자열이거나 공백만 있는 경우 기본값 반환
    if (db_type.empty() || std::all_of(db_type.begin(), db_type.end(), ::isspace)) {
        return "SQLITE";
    }
    
    // 대문자로 변환
    std::transform(db_type.begin(), db_type.end(), db_type.begin(), ::toupper);
    
    return db_type;
}

bool ConfigManager::isModuleEnabled(const std::string& module_name) const {
    if (module_name == "database") {
        return true;  // 기본 DB는 항상 활성화
    } else if (module_name == "timeseries") {
        return getBool("INFLUX_ENABLED", false);
    } else if (module_name == "redis") {
        return getBool("REDIS_PRIMARY_ENABLED", false);
    } else if (module_name == "messaging") {
        return getBool("RABBITMQ_ENABLED", false) || getBool("MQTT_ENABLED", false);
    } else if (module_name == "security") {
        return getBool("ACCESS_CONTROL_ENABLED", true);
    }
    
    return false;
}

std::string ConfigManager::loadPasswordFromFile(const std::string& password_file_key) const {
    std::string password_file = get(password_file_key);
    if (password_file.empty()) {
        return "";
    }
    
    try {
        std::ifstream file(password_file);
        if (!file.is_open()) {
            PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::WARN, 
                "⚠️ 비밀번호 파일 없음: " + password_file);
            return "";
        }
        
        std::string password;
        std::getline(file, password);
        
        // 앞뒤 공백 제거
        password.erase(0, password.find_first_not_of(" \t\n\r"));
        password.erase(password.find_last_not_of(" \t\n\r") + 1);
        
        return password;
        
    } catch (const std::exception& e) {
        PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::ERROR, 
            "❌ 비밀번호 파일 읽기 실패: " + password_file + " - " + std::string(e.what()));
        return "";
    }
}

std::string ConfigManager::getSecretsDirectory() const {
    return configDir_ + "/secrets";
}

std::map<std::string, bool> ConfigManager::checkAllConfigFiles() const {
    std::map<std::string, bool> status;
    
    std::vector<std::string> config_files = {
        ".env", "database.env", "timeseries.env", 
        "redis.env", "messaging.env", "security.env"
    };
    
    for (const auto& filename : config_files) {
        std::string filepath = configDir_ + "/" + filename;
        status[filename] = std::filesystem::exists(filepath);
    }
    
    return status;
}

void ConfigManager::triggerVariableExpansion() {
    try {
        expandAllVariables();
    } catch (const std::exception& e) {
        PulseOne::LogManager::getInstance().log("config", PulseOne::LogLevel::WARN, 
            "⚠️ 변수 확장 중 오류: " + std::string(e.what()));
    }
}