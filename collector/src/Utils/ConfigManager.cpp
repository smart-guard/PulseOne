// =============================================================================
// collector/src/Utils/ConfigManager.cpp - 실용적 구현 (다중 파일 지원)
// 🎯 목표: 250줄 내외, 꼭 필요한 기능들만!
// 기존 복잡한 헤더와 호환되도록 수정
// =============================================================================

#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <sstream>

// =============================================================================
// 자동 초기화 정적 변수 (기존 헤더 호환)
// =============================================================================
// 헤더에서 구현된 getInstance() 사용

// =============================================================================
// 🚀 실용적 자동 초기화 (기존 헤더의 doInitialize 호출)
// =============================================================================
bool ConfigManager::doInitialize() {
    LogManager::getInstance().log("config", LogLevel::INFO, 
        "🔧 ConfigManager 실용적 초기화 시작...");
    
    // 1. 설정 디렉토리 찾기
    configDir_ = findConfigDirectory();
    if (configDir_.empty()) {
        LogManager::getInstance().log("config", LogLevel::WARN, 
            "⚠️ 설정 디렉토리 없음 - 환경변수만 사용");
        initialized_.store(true);  // 기존 헤더 호환
        return false;
    }
    
    LogManager::getInstance().log("config", LogLevel::INFO, 
        "📁 설정 디렉토리: " + configDir_);
    
    // 2. 기본 템플릿 생성 (없을 때만) - 실용적 버전 사용
    createMainEnvFile();
    createDatabaseEnvFile(); 
    createRedisEnvFile();
    createTimeseriesEnvFile();
    createMessagingEnvFile();
    createSecretsDirectory();
    ensureDataDirectories();
    
    // 3. 설정 파일들 로드
    loadMainConfig();
    loadAdditionalConfigs();
    
    // 4. dataDir 설정
    dataDir_ = findDataDirectory();
    
    initialized_.store(true);  // 기존 헤더 호환
    
    LogManager::getInstance().log("config", LogLevel::INFO, 
        "✅ ConfigManager 실용적 초기화 완료 (" + std::to_string(configMap.size()) + 
        "개 설정, " + std::to_string(loadedFiles_.size()) + "개 파일)");
        
    return true;
}

// =============================================================================
// 설정 디렉토리 찾기 (기존 헤더와 호환)
// =============================================================================
std::string ConfigManager::findConfigDirectory() {
    searchLog_.clear();  // 기존 헤더 호환
    
    // 환경변수 확인
    const char* env_config = std::getenv("PULSEONE_CONFIG_DIR");
    if (env_config && directoryExists(env_config)) {
        searchLog_.push_back("✅ 환경변수: " + std::string(env_config));
        return std::string(env_config);
    }
    
    // 기본 경로들 확인
    std::vector<std::string> paths = {
        "./config", "../config", "../../config"
    };
    
    for (const auto& path : paths) {
        if (directoryExists(path)) {
            std::string canonical_path = std::filesystem::canonical(path).string();
            searchLog_.push_back("✅ 발견: " + path + " → " + canonical_path);
            return canonical_path;
        } else {
            searchLog_.push_back("❌ 없음: " + path);
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

// =============================================================================
// 🔧 템플릿 생성 메서드들 (기존 헤더 인터페이스 구현)
// =============================================================================
void ConfigManager::createMainEnvFile() {
    std::string env_path = configDir_ + "/.env";
    
    if (std::filesystem::exists(env_path)) {
        return;  // 이미 있으면 건드리지 않음
    }
    
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

# 추가 설정 파일들 (모듈별 분리)
CONFIG_FILES=database.env,timeseries.env,redis.env,messaging.env,security.env

# 시스템 설정
MAX_WORKER_THREADS=4
DEFAULT_TIMEOUT_MS=5000
)";
    
    createFileFromTemplate(env_path, content);
}

void ConfigManager::createDatabaseEnvFile() {
    std::string db_path = configDir_ + "/database.env";
    if (std::filesystem::exists(db_path)) return;
    
    std::string content = R"(# 데이터베이스 설정
DATABASE_TYPE=SQLITE
SQLITE_DB_PATH=./data/db/pulseone.db

# PostgreSQL 설정
POSTGRES_HOST=localhost
POSTGRES_PORT=5432
POSTGRES_DB=pulseone
POSTGRES_USER=postgres
POSTGRES_PASSWORD_FILE=${SECRETS_DIR}/postgres_primary.key

# MySQL 설정
MYSQL_HOST=localhost
MYSQL_PORT=3306
MYSQL_DATABASE=pulseone
MYSQL_USER=user
MYSQL_PASSWORD_FILE=${SECRETS_DIR}/mysql.key
)";
    
    createFileFromTemplate(db_path, content);
}

void ConfigManager::createRedisEnvFile() {
    std::string redis_path = configDir_ + "/redis.env";
    if (std::filesystem::exists(redis_path)) return;
    
    std::string content = R"(# Redis 설정
REDIS_PRIMARY_ENABLED=true
REDIS_PRIMARY_HOST=localhost
REDIS_PRIMARY_PORT=6379
REDIS_PRIMARY_PASSWORD_FILE=${SECRETS_DIR}/redis_primary.key
REDIS_PRIMARY_DB=0
)";
    
    createFileFromTemplate(redis_path, content);
}

void ConfigManager::createTimeseriesEnvFile() {
    std::string influx_path = configDir_ + "/timeseries.env";
    if (std::filesystem::exists(influx_path)) return;
    
    std::string content = R"(# InfluxDB 설정
INFLUX_ENABLED=true
INFLUX_HOST=localhost
INFLUX_PORT=8086
INFLUX_ORG=pulseone
INFLUX_BUCKET=telemetry_data
INFLUX_TOKEN_FILE=${SECRETS_DIR}/influx_token.key
)";
    
    createFileFromTemplate(influx_path, content);
}

void ConfigManager::createMessagingEnvFile() {
    std::string msg_path = configDir_ + "/messaging.env";
    if (std::filesystem::exists(msg_path)) return;
    
    std::string content = R"(# 메시징 설정
MESSAGING_TYPE=RABBITMQ

# RabbitMQ 설정
RABBITMQ_ENABLED=true
RABBITMQ_HOST=localhost
RABBITMQ_PORT=5672
RABBITMQ_USERNAME=guest
RABBITMQ_PASSWORD_FILE=${SECRETS_DIR}/rabbitmq.key

# MQTT 설정
MQTT_ENABLED=false
MQTT_BROKER_HOST=localhost
MQTT_BROKER_PORT=1883
MQTT_CLIENT_ID=pulseone_collector
)";
    
    createFileFromTemplate(msg_path, content);
}

void ConfigManager::createSecurityEnvFile() {
    std::string sec_path = configDir_ + "/security.env";
    if (std::filesystem::exists(sec_path)) return;
    
    std::string content = R"(# 보안 설정
ACCESS_CONTROL_ENABLED=true
JWT_SECRET_FILE=${SECRETS_DIR}/jwt_secret.key
SESSION_SECRET_FILE=${SECRETS_DIR}/session_secret.key
SSL_ENABLED=false
)";
    
    createFileFromTemplate(sec_path, content);
}

void ConfigManager::createSecretsDirectory() {
    std::string secrets_dir = configDir_ + "/secrets";
    
    if (!std::filesystem::exists(secrets_dir)) {
        std::filesystem::create_directories(secrets_dir);
        
        // .gitignore 생성
        createFileFromTemplate(secrets_dir + "/.gitignore", "*\n!.gitignore\n!README.md\n");
        
        // README.md 생성
        std::string readme_content = "# Secrets Directory\n\n이 디렉토리는 민감한 정보를 저장합니다.\n⚠️ 이 파일들은 절대 Git에 커밋하지 마세요!\n";
        createFileFromTemplate(secrets_dir + "/README.md", readme_content);
        
        // 기본 키 파일들 생성
        std::vector<std::string> key_files = {
            "postgres_primary.key", "mysql.key", "redis_primary.key", 
            "influx_token.key", "rabbitmq.key", "jwt_secret.key", "session_secret.key"
        };
        
        for (const auto& key_file : key_files) {
            std::string key_path = secrets_dir + "/" + key_file;
            createFileFromTemplate(key_path, "# " + key_file + " - 실제 키/비밀번호로 교체하세요\n");
        }
        
        LogManager::getInstance().log("config", LogLevel::INFO, "✅ secrets/ 디렉토리 생성");
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
        
        LogManager::getInstance().log("config", LogLevel::INFO, 
            "✅ 설정 파일 생성: " + std::filesystem::path(filepath).filename().string());
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("config", LogLevel::WARN, 
            "⚠️ 파일 생성 중 예외: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 📁 데이터 디렉토리 생성 (기존 헤더 호환)
// =============================================================================

// =============================================================================
// 설정 파일 로딩 (기존 헤더 인터페이스 구현)
// =============================================================================
void ConfigManager::loadMainConfig() {
    std::string main_env_path = configDir_ + "/.env";
    
    if (std::filesystem::exists(main_env_path)) {
        loadConfigFile(main_env_path);
        LogManager::getInstance().log("config", LogLevel::INFO, "✅ 메인 설정 로드: .env");
    } else {
        LogManager::getInstance().log("config", LogLevel::WARN, "⚠️ 메인 설정 파일 없음: .env");
    }
}

void ConfigManager::loadAdditionalConfigs() {
    LogManager::getInstance().log("config", LogLevel::INFO, "🔍 추가 설정 파일 확인 시작");
    
    // CONFIG_FILES에서 지정된 추가 파일들 로드
    std::string config_files;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        auto it = configMap.find("CONFIG_FILES");
        config_files = (it != configMap.end()) ? it->second : "";
    }
    
    if (config_files.empty()) {
        LogManager::getInstance().log("config", LogLevel::INFO, "ℹ️ 추가 설정 파일 없음 (CONFIG_FILES 비어있음)");
        return;
    }
    
    std::stringstream ss(config_files);
    std::string filename;
    
    while (std::getline(ss, filename, ',')) {
        // 공백 제거
        filename.erase(0, filename.find_first_not_of(" \t"));
        filename.erase(filename.find_last_not_of(" \t") + 1);
        
        if (!filename.empty()) {
            std::string full_path = configDir_ + "/" + filename;
            try {
                if (std::filesystem::exists(full_path)) {
                    loadConfigFile(full_path);
                    LogManager::getInstance().log("config", LogLevel::INFO, "✅ 추가 설정 로드: " + filename);
                } else {
                    LogManager::getInstance().log("config", LogLevel::INFO, "ℹ️ 추가 설정 파일 없음: " + filename);
                }
            } catch (const std::exception& e) {
                LogManager::getInstance().log("config", LogLevel::WARN, "⚠️ 설정 파일 로드 실패: " + filename + " - " + e.what());
            }
        }
    }
    
    LogManager::getInstance().log("config", LogLevel::INFO, "✅ 추가 설정 파일 확인 완료");
}

void ConfigManager::loadConfigFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        LogManager::getInstance().log("config", LogLevel::LOG_ERROR, "❌ 파일 열기 실패: " + filepath);
        return;
    }
    
    std::string line;
    int line_count = 0;
    int parsed_count = 0;
    
    {
        std::lock_guard<std::mutex> lock(configMutex);
        size_t original_size = configMap.size();
        
        while (std::getline(file, line)) {
            line_count++;
            try {
                parseLine(line);
                if (configMap.size() > original_size + parsed_count) {
                    parsed_count++;
                }
            } catch (const std::exception&) {
                continue;
            }
        }
        
        loadedFiles_.push_back(filepath);
    }
    
    file.close();
    
    LogManager::getInstance().log("config", LogLevel::INFO, 
        "📄 " + std::filesystem::path(filepath).filename().string() + " - " + 
        std::to_string(parsed_count) + "/" + std::to_string(line_count) + " 라인 파싱됨");
}

// =============================================================================
// 라인 파싱 (기존 헤더와 호환)
// =============================================================================
void ConfigManager::parseLine(const std::string& line) {
    // 빈 줄이나 주석 무시
    if (line.empty() || line[0] == '#') {
        return;
    }
    
    // = 찾기
    size_t pos = line.find('=');
    if (pos == std::string::npos) {
        return;
    }
    
    // 키와 값 추출
    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos + 1);
    
    // 공백 제거
    key.erase(0, key.find_first_not_of(" \t"));
    key.erase(key.find_last_not_of(" \t") + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);
    
    // 따옴표 제거
    if (value.length() >= 2 && 
        ((value.front() == '"' && value.back() == '"') || 
         (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.length() - 2);
    }
    
    if (!key.empty()) {
        configMap[key] = value;  // 기존 헤더의 변수명 사용
    }
}

// =============================================================================
// 기본 인터페이스 (기존 헤더와 호환)
// =============================================================================
std::string ConfigManager::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(configMutex);  // 기존 헤더의 변수명
    auto it = configMap.find(key);
    return (it != configMap.end()) ? it->second : "";
}

std::string ConfigManager::getOrDefault(const std::string& key, const std::string& defaultValue) const {
    // SQLite 관련 키면 두 키 다 찾아보기
    if (key == "SQLITE_DB_PATH") {
        std::string value = get("SQLITE_DB_PATH");
        if (!value.empty()) return value;
        
        value = get("SQLITE_PATH");  // 대체 키 시도
        if (!value.empty()) return value;
        
        return defaultValue;
    }
    
    if (key == "SQLITE_PATH") {
        std::string value = get("SQLITE_PATH");
        if (!value.empty()) return value;
        
        value = get("SQLITE_DB_PATH");  // 대체 키 시도
        if (!value.empty()) return value;
        
        return defaultValue;
    }
    
    // 다른 키들은 기존 로직 그대로
    std::lock_guard<std::mutex> lock(configMutex);
    auto it = configMap.find(key);
    return (it != configMap.end()) ? it->second : defaultValue;
}

void ConfigManager::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(configMutex);
    configMap[key] = value;
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
// 편의 기능 및 실용적 기능들 (기존 헤더 인터페이스 구현)
// =============================================================================
int ConfigManager::getInt(const std::string& key, int defaultValue) const {
    std::string value = get(key);
    if (value.empty()) return defaultValue;
    try { 
        return std::stoi(value); 
    } catch (...) { 
        return defaultValue; 
    }
}

bool ConfigManager::getBool(const std::string& key, bool defaultValue) const {
    std::string value = get(key);
    if (value.empty()) return defaultValue;
    
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    return (value == "true" || value == "yes" || value == "1" || value == "on");
}

double ConfigManager::getDouble(const std::string& key, double defaultValue) const {
    std::string value = get(key);
    if (value.empty()) return defaultValue;
    try { 
        return std::stod(value); 
    } catch (...) { 
        return defaultValue; 
    }
}

std::string ConfigManager::getDataDirectory() const {
    return dataDir_.empty() ? "./data" : dataDir_;
}

std::string ConfigManager::getSQLiteDbPath() const {
    // SQLITE_DB_PATH 먼저 찾기
    std::string db_path = get("SQLITE_DB_PATH");
    if (!db_path.empty()) {
        return db_path;
    }
    
    // 없으면 SQLITE_PATH 찾기
    db_path = get("SQLITE_PATH");
    if (!db_path.empty()) {
        return db_path;
    }
    
    // 둘 다 없으면 기본값
    return "./data/db/pulseone.db";
}

std::string ConfigManager::getBackupDirectory() const {
    return getDataDirectory() + "/backup";
}

std::string ConfigManager::getSecretsDirectory() const {
    return configDir_ + "/secrets";
}

std::string ConfigManager::loadPasswordFromFile(const std::string& password_file_key) const {
    std::string password_file = get(password_file_key);
    if (password_file.empty()) {
        return "";
    }
    
    // ${SECRETS_DIR} 치환 (간단한 버전)
    if (password_file.find("${SECRETS_DIR}") != std::string::npos) {
        std::string secrets_dir = configDir_ + "/secrets";
        size_t pos = password_file.find("${SECRETS_DIR}");
        password_file.replace(pos, 14, secrets_dir);  // 14 = length of "${SECRETS_DIR}"
    }
    
    try {
        std::ifstream file(password_file);
        if (!file.is_open()) {
            LogManager::getInstance().log("config", LogLevel::WARN, "⚠️ 비밀번호 파일 없음: " + password_file);
            return "";
        }
        
        std::string password;
        std::getline(file, password);
        
        // 공백 제거
        password.erase(0, password.find_first_not_of(" \t\n\r"));
        password.erase(password.find_last_not_of(" \t\n\r") + 1);
        
        return password;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("config", LogLevel::LOG_ERROR, 
            "❌ 비밀번호 파일 읽기 실패: " + password_file + " - " + std::string(e.what()));
        return "";
    }
}

std::string ConfigManager::getActiveDatabaseType() const {
    std::string db_type = get("DATABASE_TYPE");
    if (db_type.empty()) {
        return "SQLITE";
    }
    
    std::transform(db_type.begin(), db_type.end(), db_type.begin(), ::toupper);
    return db_type;
}

bool ConfigManager::isModuleEnabled(const std::string& module_name) const {
    if (module_name == "database") {
        return true;  // 항상 활성화
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

void ConfigManager::reload() {
    LogManager::getInstance().log("config", LogLevel::INFO, "🔄 ConfigManager 재로딩 시작...");
    
    {
        std::lock_guard<std::mutex> lock(configMutex);
        configMap.clear();
        loadedFiles_.clear();
        searchLog_.clear();
    }
    
    initialized_.store(false);
    doInitialize();
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

void ConfigManager::printConfigSearchLog() const {
    LogManager::getInstance().log("config", LogLevel::INFO, "🔍 ConfigManager 경로 검색 로그:");
    for (const auto& log_entry : searchLog_) {
        LogManager::getInstance().log("config", LogLevel::INFO, log_entry);
    }
    LogManager::getInstance().log("config", LogLevel::INFO, "📄 로드된 설정 파일들:");
    for (const auto& file : loadedFiles_) {
        LogManager::getInstance().log("config", LogLevel::INFO, "  " + file);
    }
    LogManager::getInstance().log("config", LogLevel::INFO, "⚙️ 총 " + std::to_string(configMap.size()) + "개 설정 항목 로드됨");
}

std::string ConfigManager::findDataDirectory() {
    // 1. 환경변수 확인
    const char* env_data = std::getenv("PULSEONE_DATA_DIR");
    if (env_data && strlen(env_data) > 0) {
        return std::string(env_data);
    }
    
    // 2. 설정에서 확인
    auto it = configMap.find("DATA_DIR");
    if (it != configMap.end() && !it->second.empty()) {
        return it->second;
    }
    
    // 3. 기본값
    return "./data";
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
// 기존 헤더의 나머지 인터페이스들 (최소 구현)
// =============================================================================
void ConfigManager::expandAllVariables() {
    // 실용적 버전에서는 ${SECRETS_DIR} 정도만 지원
    LogManager::getInstance().log("config", LogLevel::INFO, "⚠️ 변수 확장은 실용적 버전에서 제한적 지원");
}

std::string ConfigManager::expandVariables(const std::string& value) const {
    // ${SECRETS_DIR} 만 지원
    std::string result = value;
    if (result.find("${SECRETS_DIR}") != std::string::npos) {
        std::string secrets_dir = configDir_ + "/secrets";
        size_t pos = result.find("${SECRETS_DIR}");
        result.replace(pos, 14, secrets_dir);
    }
    return result;
}

void ConfigManager::triggerVariableExpansion() {
    expandAllVariables();
}