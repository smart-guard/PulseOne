// =============================================================================
// collector/src/Utils/ConfigManager.cpp - 완전한 크로스 플랫폼 경로 처리
// PlatformCompat.h의 Path 클래스 활용한 완성본
// =============================================================================

#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include "Platform/PlatformCompat.h"  // 강화된 경로 처리 기능 사용
#include <fstream>
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <sstream>

using namespace PulseOne::Platform;

// =============================================================================
// 완전한 크로스 플랫폼 초기화
// =============================================================================
bool ConfigManager::doInitialize() {
    LogManager::getInstance().log("config", LogLevel::INFO, 
        "ConfigManager 크로스 플랫폼 초기화 시작...");
    
    // 1. 설정 디렉토리 찾기 (크로스 플랫폼)
    configDir_ = findConfigDirectory();
    if (configDir_.empty()) {
        LogManager::getInstance().log("config", LogLevel::WARN, 
            "설정 디렉토리 없음 - 환경변수만 사용");
        initialized_.store(true);
        return false;
    }
    
    LogManager::getInstance().log("config", LogLevel::INFO, 
        "설정 디렉토리: " + configDir_);
    
    // 2. 기본 템플릿 생성 (크로스 플랫폼 경로 사용)
    createMainEnvFile();
    createDatabaseEnvFile(); 
    createRedisEnvFile();
    createTimeseriesEnvFile();
    createMessagingEnvFile();
    createSecurityEnvFile();
    createSecretsDirectory();
    ensureDataDirectories();
    
    // 3. 설정 파일들 로드
    loadMainConfig();
    loadAdditionalConfigs();
    
    // 4. dataDir 설정 (크로스 플랫폼)
    dataDir_ = findDataDirectory();
    
    initialized_.store(true);
    
    LogManager::getInstance().log("config", LogLevel::INFO, 
        "ConfigManager 초기화 완료 (" + std::to_string(configMap.size()) + 
        "개 설정, " + std::to_string(loadedFiles_.size()) + "개 파일)");
        
    return true;
}

// =============================================================================
// 크로스 플랫폼 설정 디렉토리 찾기 (PlatformCompat.h 활용)
// =============================================================================
std::string ConfigManager::findConfigDirectory() {
    searchLog_.clear();
    
    // 1. 환경변수 확인
    const char* env_config = std::getenv("PULSEONE_CONFIG_DIR");
    if (env_config && FileSystem::DirectoryExists(env_config)) {
        searchLog_.push_back("환경변수: " + std::string(env_config));
        return std::string(env_config);
    }
    
    // 2. 실행 파일 기준 경로 찾기 (PlatformCompat.h의 Path 클래스 사용)
    std::string exe_dir = Path::GetExecutableDirectory();
    std::vector<std::string> search_paths = {
        Path::Join(exe_dir, "config"),           // 배포: PulseOne/config
        Path::Join(exe_dir, "../config"),        // 개발: collector/../config  
        Path::Join(exe_dir, "../../config"),     // 깊은 빌드: collector/bin/../config
        "./config",                              // 현재 디렉토리
        "../config",                             // 상위 디렉토리
        "../../config"                           // 상위상위 디렉토리
    };
    
    for (const auto& path : search_paths) {
        std::string normalized_path = Path::Normalize(path);
        
        if (FileSystem::DirectoryExists(normalized_path)) {
            try {
                std::string absolute_path = Path::GetAbsolute(normalized_path);
                searchLog_.push_back("발견: " + path + " -> " + absolute_path);
                return absolute_path;
            } catch (const std::exception&) {
                searchLog_.push_back("발견: " + normalized_path + " (절대경로 변환 실패)");
                return normalized_path;
            }
        } else {
            searchLog_.push_back("없음: " + normalized_path);
        }
    }
    
    searchLog_.push_back("설정 디렉토리를 찾을 수 없음");
    return "";
}

bool ConfigManager::directoryExists(const std::string& path) {
    return FileSystem::DirectoryExists(path);
}

std::string ConfigManager::getExecutableDirectory() {
    return Path::GetExecutableDirectory();
}

// =============================================================================
// 크로스 플랫폼 템플릿 파일 생성 (Path 클래스 활용)
// =============================================================================
void ConfigManager::createMainEnvFile() {
    std::string env_path = Path::Join(configDir_, ".env");
    
    if (FileSystem::FileExists(env_path)) {
        return;  // 이미 있으면 건드리지 않음
    }
    
    std::string content = R"(# =============================================================================
# PulseOne 메인 설정 파일 (.env) - 자동 생성됨
# =============================================================================

# 환경 설정
NODE_ENV=development
LOG_LEVEL=info
LOG_TO_CONSOLE=true
LOG_TO_FILE=true
LOG_FILE_PATH=./logs/

# 기본 데이터베이스 설정
DATABASE_TYPE=SQLITE

# 기본 디렉토리 설정
DATA_DIR=./data

# 추가 설정 파일들 (모듈별 분리)
CONFIG_FILES=database.env,timeseries.env,redis.env,messaging.env,security.env

# 시스템 설정
MAX_WORKER_THREADS=4
DEFAULT_TIMEOUT_MS=5000

# 로그 로테이션 설정
LOG_MAX_SIZE_MB=100
LOG_MAX_FILES=30
MAINTENANCE_MODE=false
)";
    
    createFileFromTemplate(env_path, content);
}

void ConfigManager::createDatabaseEnvFile() {
    std::string db_path = Path::Join(configDir_, "database.env");
    if (FileSystem::FileExists(db_path)) return;
    
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
    std::string redis_path = Path::Join(configDir_, "redis.env");
    if (FileSystem::FileExists(redis_path)) return;
    
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
    std::string influx_path = Path::Join(configDir_, "timeseries.env");
    if (FileSystem::FileExists(influx_path)) return;
    
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
    std::string msg_path = Path::Join(configDir_, "messaging.env");
    if (FileSystem::FileExists(msg_path)) return;
    
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
    std::string sec_path = Path::Join(configDir_, "security.env");
    if (FileSystem::FileExists(sec_path)) return;
    
    std::string content = R"(# 보안 설정
ACCESS_CONTROL_ENABLED=true
JWT_SECRET_FILE=${SECRETS_DIR}/jwt_secret.key
SESSION_SECRET_FILE=${SECRETS_DIR}/session_secret.key
SSL_ENABLED=false
)";
    
    createFileFromTemplate(sec_path, content);
}

void ConfigManager::createSecretsDirectory() {
    std::string secrets_dir = Path::Join(configDir_, "secrets");
    
    if (!FileSystem::DirectoryExists(secrets_dir)) {
        FileSystem::CreateDirectoryRecursive(secrets_dir);
        
        // .gitignore 생성
        std::string gitignore_path = Path::Join(secrets_dir, ".gitignore");
        createFileFromTemplate(gitignore_path, "*\n!.gitignore\n!README.md\n");
        
        // README.md 생성
        std::string readme_path = Path::Join(secrets_dir, "README.md");
        std::string readme_content = "# Secrets Directory\n\n이 디렉토리는 민감한 정보를 저장합니다.\n이 파일들은 절대 Git에 커밋하지 마세요!\n";
        createFileFromTemplate(readme_path, readme_content);
        
        // 기본 키 파일들 생성
        std::vector<std::string> key_files = {
            "postgres_primary.key", "mysql.key", "redis_primary.key", 
            "influx_token.key", "rabbitmq.key", "jwt_secret.key", "session_secret.key"
        };
        
        for (const auto& key_file : key_files) {
            std::string key_path = Path::Join(secrets_dir, key_file);
            createFileFromTemplate(key_path, "# " + key_file + " - 실제 키/비밀번호로 교체하세요\n");
        }
        
        LogManager::getInstance().log("config", LogLevel::INFO, "secrets/ 디렉토리 생성");
    }
}

bool ConfigManager::createFileFromTemplate(const std::string& filepath, const std::string& content) {
    try {
        if (FileSystem::FileExists(filepath)) {
            return true;
        }
        
        // 디렉토리가 없으면 생성
        std::string dir = Path::GetDirectory(filepath);
        if (!FileSystem::DirectoryExists(dir)) {
            FileSystem::CreateDirectoryRecursive(dir);
        }
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }
        
        file << content;
        file.close();
        
        LogManager::getInstance().log("config", LogLevel::INFO, 
            "설정 파일 생성: " + Path::GetFileName(filepath));
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("config", LogLevel::WARN, 
            "파일 생성 중 예외: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 설정 파일 로딩 (크로스 플랫폼 경로 처리)
// =============================================================================
void ConfigManager::loadMainConfig() {
    std::string main_env_path = Path::Join(configDir_, ".env");
    
    if (FileSystem::FileExists(main_env_path)) {
        loadConfigFile(main_env_path);
        LogManager::getInstance().log("config", LogLevel::INFO, "메인 설정 로드: .env");
    } else {
        LogManager::getInstance().log("config", LogLevel::WARN, "메인 설정 파일 없음: .env");
    }
}

void ConfigManager::loadAdditionalConfigs() {
    LogManager::getInstance().log("config", LogLevel::INFO, "추가 설정 파일 확인 시작");
    
    // CONFIG_FILES에서 지정된 추가 파일들 로드
    std::string config_files;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        auto it = configMap.find("CONFIG_FILES");
        config_files = (it != configMap.end()) ? it->second : "";
    }
    
    if (config_files.empty()) {
        LogManager::getInstance().log("config", LogLevel::INFO, "추가 설정 파일 없음 (CONFIG_FILES 비어있음)");
        return;
    }
    
    std::stringstream ss(config_files);
    std::string filename;
    
    while (std::getline(ss, filename, ',')) {
        // 공백 제거
        filename.erase(0, filename.find_first_not_of(" \t"));
        filename.erase(filename.find_last_not_of(" \t") + 1);
        
        if (!filename.empty()) {
            std::string full_path = Path::Join(configDir_, filename);
            try {
                if (FileSystem::FileExists(full_path)) {
                    loadConfigFile(full_path);
                    LogManager::getInstance().log("config", LogLevel::INFO, "추가 설정 로드: " + filename);
                } else {
                    LogManager::getInstance().log("config", LogLevel::INFO, "추가 설정 파일 없음: " + filename);
                }
            } catch (const std::exception& e) {
                LogManager::getInstance().log("config", LogLevel::WARN, "설정 파일 로드 실패: " + filename + " - " + e.what());
            }
        }
    }
    
    LogManager::getInstance().log("config", LogLevel::INFO, "추가 설정 파일 확인 완료");
}

void ConfigManager::loadConfigFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        LogManager::getInstance().log("config", LogLevel::LOG_ERROR, "파일 열기 실패: " + filepath);
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
        Path::GetFileName(filepath) + " - " + 
        std::to_string(parsed_count) + "/" + std::to_string(line_count) + " 라인 파싱됨");
}

// =============================================================================
// 라인 파싱 (기존과 동일)
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
        configMap[key] = value;
    }
}

// =============================================================================
// 기본 인터페이스 (기존과 동일)
// =============================================================================
std::string ConfigManager::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(configMutex);
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
// 편의 기능들
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

// =============================================================================
// 경로 관련 메서드들 (크로스 플랫폼 강화)
// =============================================================================
std::string ConfigManager::getDataDirectory() const {
    return dataDir_.empty() ? "./data" : dataDir_;
}

std::string ConfigManager::getSQLiteDbPath() const {
    // SQLITE_DB_PATH 먼저 찾기
    std::string db_path = get("SQLITE_DB_PATH");
    if (!db_path.empty()) {
        return Path::ToNativeStyle(db_path);
    }
    
    // 없으면 SQLITE_PATH 찾기
    db_path = get("SQLITE_PATH");
    if (!db_path.empty()) {
        return Path::ToNativeStyle(db_path);
    }
    
    // 둘 다 없으면 기본값 (크로스 플랫폼 경로)
    return Path::ToNativeStyle("./data/db/pulseone.db");
}

std::string ConfigManager::getBackupDirectory() const {
    return Path::Join(getDataDirectory(), "backup");
}

std::string ConfigManager::getSecretsDirectory() const {
    return Path::Join(configDir_, "secrets");
}

std::string ConfigManager::loadPasswordFromFile(const std::string& password_file_key) const {
    std::string password_file = get(password_file_key);
    if (password_file.empty()) {
        return "";
    }
    
    // ${SECRETS_DIR} 치환
    if (password_file.find("${SECRETS_DIR}") != std::string::npos) {
        std::string secrets_dir = Path::Join(configDir_, "secrets");
        size_t pos = password_file.find("${SECRETS_DIR}");
        password_file.replace(pos, 14, secrets_dir);
    }
    
    // 크로스 플랫폼 경로로 변환
    password_file = Path::ToNativeStyle(password_file);
    
    try {
        std::ifstream file(password_file);
        if (!file.is_open()) {
            LogManager::getInstance().log("config", LogLevel::WARN, "비밀번호 파일 없음: " + password_file);
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
            "비밀번호 파일 읽기 실패: " + password_file + " - " + std::string(e.what()));
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
    LogManager::getInstance().log("config", LogLevel::INFO, "ConfigManager 재로딩 시작...");
    
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
        std::string filepath = Path::Join(configDir_, filename);
        status[filename] = FileSystem::FileExists(filepath);
    }
    
    return status;
}

void ConfigManager::printConfigSearchLog() const {
    LogManager::getInstance().log("config", LogLevel::INFO, "ConfigManager 경로 검색 로그:");
    for (const auto& log_entry : searchLog_) {
        LogManager::getInstance().log("config", LogLevel::INFO, log_entry);
    }
    LogManager::getInstance().log("config", LogLevel::INFO, "로드된 설정 파일들:");
    for (const auto& file : loadedFiles_) {
        LogManager::getInstance().log("config", LogLevel::INFO, "  " + file);
    }
    LogManager::getInstance().log("config", LogLevel::INFO, "총 " + std::to_string(configMap.size()) + "개 설정 항목 로드됨");
}

std::string ConfigManager::findDataDirectory() {
    // 1. 환경변수 확인
    const char* env_data = std::getenv("PULSEONE_DATA_DIR");
    if (env_data && strlen(env_data) > 0) {
        return Path::ToNativeStyle(std::string(env_data));
    }
    
    // 2. 설정에서 확인
    auto it = configMap.find("DATA_DIR");
    if (it != configMap.end() && !it->second.empty()) {
        return Path::ToNativeStyle(it->second);
    }
    
    // 3. 기본값 (크로스 플랫폼)
    return Path::ToNativeStyle("./data");
}

void ConfigManager::ensureDataDirectories() {
    if (dataDir_.empty()) {
        return;
    }
    
    std::vector<std::string> dirs = {"db", "logs", "backup", "temp"};
    
    for (const auto& dir : dirs) {
        try {
            std::string full_path = Path::Join(dataDir_, dir);
            FileSystem::CreateDirectoryRecursive(full_path);
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
    LogManager::getInstance().log("config", LogLevel::INFO, "변수 확장은 실용적 버전에서 제한적 지원");
}

std::string ConfigManager::expandVariables(const std::string& value) const {
    // ${SECRETS_DIR} 만 지원
    std::string result = value;
    if (result.find("${SECRETS_DIR}") != std::string::npos) {
        std::string secrets_dir = Path::Join(configDir_, "secrets");
        size_t pos = result.find("${SECRETS_DIR}");
        result.replace(pos, 14, secrets_dir);
    }
    return result;
}

void ConfigManager::triggerVariableExpansion() {
    expandAllVariables();
}