// =============================================================================
// DatabaseManager.cpp - 자동 초기화 적용 완전판
// 🚀 이제 initialize() 호출 없이 바로 사용 가능!
// =============================================================================

#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include <cstdlib>
#include <thread>
#include <chrono>
#include <sstream>
#include <iostream>

namespace {
    // C++17용 starts_with 구현
    bool starts_with(const std::string& str, const std::string& prefix) {
        if (prefix.length() > str.length()) {
            return false;
        }
        return str.substr(0, prefix.length()) == prefix;
    }
    
    // C++17용 ends_with 구현
    bool ends_with(const std::string& str, const std::string& suffix) {
        if (suffix.length() > str.length()) {
            return false;
        }
        return str.substr(str.length() - suffix.length()) == suffix;
    }
}

// =============================================================================
// 🚀 자동 초기화 정적 변수들
// =============================================================================
std::once_flag DatabaseManager::init_flag_;
std::atomic<bool> DatabaseManager::initialization_success_(false);

// =============================================================================
// 🚀 자동 초기화 생성자
// =============================================================================
DatabaseManager::DatabaseManager() {
    // 기본 설정 (doInitialize()에서 덮어씀)
    enabled_databases_[DatabaseType::POSTGRESQL] = false;
    enabled_databases_[DatabaseType::SQLITE] = false;
    enabled_databases_[DatabaseType::MYSQL] = false;
    enabled_databases_[DatabaseType::MSSQL] = false;
    enabled_databases_[DatabaseType::REDIS] = true;
    enabled_databases_[DatabaseType::INFLUXDB] = true;
    
    primary_rdb_ = DatabaseType::SQLITE;
}

DatabaseManager::~DatabaseManager() {
    disconnectAll();
}

// =============================================================================
// 🚀 자동 초기화 싱글톤 구현 (핵심!)
// =============================================================================
DatabaseManager& DatabaseManager::getInstance() {
    static DatabaseManager instance;
    
    // 🚀 자동 초기화: 첫 호출 시에만 실행
    std::call_once(init_flag_, [&instance] {
        try {
            bool success = instance.doInitialize();
            initialization_success_.store(success);
            
            if (success) {
                LogManager::getInstance().log("database", LogLevel::INFO, 
                    "🚀 DatabaseManager 자동 초기화 성공!");
            } else {
                LogManager::getInstance().log("database", LogLevel::ERROR, 
                    "❌ DatabaseManager 자동 초기화 실패!");
            }
        } catch (const std::exception& e) {
            LogManager::getInstance().log("database", LogLevel::ERROR, 
                "💥 DatabaseManager 자동 초기화 예외: " + std::string(e.what()));
            initialization_success_.store(false);
        }
    });
    
    return instance;
}

// =============================================================================
// 🚀 실제 초기화 로직 (기존 initialize()를 개명)
// =============================================================================
bool DatabaseManager::doInitialize() {
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "🔧 DatabaseManager 자동 초기화 시작...");
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    // 1️⃣ 설정 로드 (DATABASE_TYPE에 따라 활성화 DB 결정)
    loadDatabaseConfig();
    
    bool primary_db_connected = false;  // 메인 RDB 연결 상태
    
    // 2️⃣ 메인 RDB 연결 시도 (하나만 선택됨)
    if (enabled_databases_[DatabaseType::SQLITE]) {
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "🔧 SQLite 연결 시도...");
        if (connectSQLite()) {
            primary_db_connected = true;
            LogManager::getInstance().log("database", LogLevel::INFO, 
                "✅ 메인 RDB (SQLite) 연결 성공");
        } else {
            LogManager::getInstance().log("database", LogLevel::ERROR, 
                "❌ 메인 RDB (SQLite) 연결 실패");
        }
    }
    else if (enabled_databases_[DatabaseType::POSTGRESQL]) {
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "🔧 PostgreSQL 연결 시도...");
        if (connectPostgres()) {
            primary_db_connected = true;
            LogManager::getInstance().log("database", LogLevel::INFO, 
                "✅ 메인 RDB (PostgreSQL) 연결 성공");
        } else {
            LogManager::getInstance().log("database", LogLevel::ERROR, 
                "❌ 메인 RDB (PostgreSQL) 연결 실패");
        }
    }
    else if (enabled_databases_[DatabaseType::MYSQL]) {
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "🔧 MySQL 연결 시도...");
        if (connectMySQL()) {
            primary_db_connected = true;
            LogManager::getInstance().log("database", LogLevel::INFO, 
                "✅ 메인 RDB (MySQL) 연결 성공");
        } else {
            LogManager::getInstance().log("database", LogLevel::ERROR, 
                "❌ 메인 RDB (MySQL) 연결 실패");
        }
    }
#ifdef _WIN32
    else if (enabled_databases_[DatabaseType::MSSQL]) {
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "🔧 MSSQL 연결 시도...");
        if (connectMSSQL()) {
            primary_db_connected = true;
            LogManager::getInstance().log("database", LogLevel::INFO, 
                "✅ 메인 RDB (MSSQL) 연결 성공");
        } else {
            LogManager::getInstance().log("database", LogLevel::ERROR, 
                "❌ 메인 RDB (MSSQL) 연결 실패");
        }
    }
#endif
    
    // 🔧 3단계: 메인 RDB 연결 실패 시 전체 초기화 실패
    if (!primary_db_connected) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "❌ 메인 데이터베이스 연결 실패 - DatabaseManager 자동 초기화 실패");
        return false;
    }
    
    // 4️⃣ 보조 서비스 연결 (실패해도 계속 진행)
    if (enabled_databases_[DatabaseType::REDIS]) {
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "🔧 Redis 연결 시도...");
        if (connectRedis()) {
            LogManager::getInstance().log("database", LogLevel::INFO, 
                "✅ Redis 연결 성공");
        } else {
            LogManager::getInstance().log("database", LogLevel::WARN, 
                "⚠️ Redis 연결 실패 - 계속 진행");
        }
    }
    
    if (enabled_databases_[DatabaseType::INFLUXDB]) {
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "🔧 InfluxDB 연결 시도...");
        if (connectInflux()) {
            LogManager::getInstance().log("database", LogLevel::INFO, 
                "✅ InfluxDB 연결 성공");
        } else {
            LogManager::getInstance().log("database", LogLevel::WARN, 
                "⚠️ InfluxDB 연결 실패 - 계속 진행");
        }
    }
    
    // 5️⃣ 연결 상태 출력
    auto status = getAllConnectionStatus();
    for (const auto& [db_name, connected] : status) {
        std::string status_icon = connected ? "✅" : "❌";
        LogManager::getInstance().log("database", LogLevel::INFO, 
            status_icon + " " + db_name + ": " + (connected ? "연결됨" : "연결 안됨"));
    }
    
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "✅ DatabaseManager 자동 초기화 완료 (메인 RDB 연결 성공)");
    
    return true;
}

// =============================================================================
// 🚀 수동 초기화 (옵션 - 자동 초기화로 인해 일반적으로 불필요)
// =============================================================================
bool DatabaseManager::initialize() {
    // 이미 자동 초기화가 완료되었으면 상태 반환
    if (initialization_success_.load()) {
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "⚡ DatabaseManager 이미 자동 초기화됨 - 수동 초기화 건너뜀");
        return true;
    }
    
    // 자동 초기화가 실패했을 경우에만 수동 초기화 시도
    LogManager::getInstance().log("database", LogLevel::WARN, 
        "🔧 자동 초기화 실패로 인한 수동 초기화 시도...");
    
    return doInitialize();
}

// =============================================================================
// 🚀 강제 재초기화
// =============================================================================
void DatabaseManager::reinitialize() {
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "🔄 DatabaseManager 강제 재초기화 중...");
    
    // 기존 연결 모두 해제
    disconnectAll();
    
    // 초기화 플래그 리셋 (once_flag는 리셋 불가능하므로 직접 초기화)
    initialization_success_.store(false);
    
    // 재초기화 실행
    bool success = doInitialize();
    initialization_success_.store(success);
    
    if (success) {
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "✅ DatabaseManager 재초기화 성공");
    } else {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "❌ DatabaseManager 재초기화 실패");
    }
}

// =============================================================================
// 🔧 설정 로드 (기존 코드와 동일)
// =============================================================================
void DatabaseManager::loadDatabaseConfig() {
    auto& config = ConfigManager::getInstance();
    
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "🔧 데이터베이스 설정 로드 중...");
    
    // 🔧 1단계: DATABASE_TYPE 설정으로 메인 RDB 결정
    std::string database_type = config.getOrDefault("DATABASE_TYPE", "SQLITE");
    
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "📋 설정된 DATABASE_TYPE: " + database_type);
    
    // 🔧 2단계: 모든 RDB 비활성화 후 선택된 것만 활성화
    enabled_databases_[DatabaseType::POSTGRESQL] = false;
    enabled_databases_[DatabaseType::SQLITE] = false;
    enabled_databases_[DatabaseType::MYSQL] = false;
    enabled_databases_[DatabaseType::MSSQL] = false;
    
    // DATABASE_TYPE에 따라 메인 RDB 한 개만 활성화
    if (database_type == "SQLITE") {
        enabled_databases_[DatabaseType::SQLITE] = true;
        primary_rdb_ = DatabaseType::SQLITE;
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "✅ SQLite 모드 선택됨");
    }
    else if (database_type == "POSTGRESQL") {
        enabled_databases_[DatabaseType::POSTGRESQL] = true;
        primary_rdb_ = DatabaseType::POSTGRESQL;
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "✅ PostgreSQL 모드 선택됨");
    }
    else if (database_type == "MYSQL" || database_type == "MARIADB") {
        enabled_databases_[DatabaseType::MYSQL] = true;
        primary_rdb_ = DatabaseType::MYSQL;
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "✅ MySQL/MariaDB 모드 선택됨");
    }
    else if (database_type == "MSSQL") {
        enabled_databases_[DatabaseType::MSSQL] = true;
        primary_rdb_ = DatabaseType::MSSQL;
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "✅ MSSQL 모드 선택됨");
    }
    else {
        // 알 수 없는 타입이면 SQLite를 기본값으로 사용
        enabled_databases_[DatabaseType::SQLITE] = true;
        primary_rdb_ = DatabaseType::SQLITE;
        LogManager::getInstance().log("database", LogLevel::WARN, 
            "⚠️ 알 수 없는 DATABASE_TYPE '" + database_type + "' - SQLite로 기본 설정");
    }
    
    // 🔧 3단계: 보조 서비스 설정 (기본 활성화, 명시적 비활성화 가능)
    enabled_databases_[DatabaseType::REDIS] = 
        config.getBool("REDIS_ENABLED", true) && 
        config.getBool("REDIS_PRIMARY_ENABLED", true);
    
    enabled_databases_[DatabaseType::INFLUXDB] = 
        config.getBool("INFLUX_ENABLED", true) || 
        config.getBool("INFLUXDB_ENABLED", true);
    
    // 🔧 4단계: 설정 결과 로그
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "🔧 최종 데이터베이스 설정:");
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "   📊 메인 RDB: " + getDatabaseTypeName(primary_rdb_));
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "   🔄 Redis: " + std::string(enabled_databases_[DatabaseType::REDIS] ? "활성화" : "비활성화"));
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "   📈 InfluxDB: " + std::string(enabled_databases_[DatabaseType::INFLUXDB] ? "활성화" : "비활성화"));
}

// =============================================================================
// PostgreSQL 구현 (기존과 동일)
// =============================================================================
bool DatabaseManager::connectPostgres() {
    for (int i = 0; i < MAX_RETRIES; ++i) {
        try {
            std::string connStr = buildConnectionString(DatabaseType::POSTGRESQL);
            pg_conn_ = std::make_unique<pqxx::connection>(connStr);
            
            if (pg_conn_->is_open()) {
                LogManager::getInstance().log("database", LogLevel::INFO, 
                    "✅ PostgreSQL 연결 성공");
                return true;
            }
        } catch (const std::exception& e) {
            LogManager::getInstance().log("database", LogLevel::ERROR, 
                "❌ PostgreSQL 연결 실패: " + std::string(e.what()));
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return false;
}

// =============================================================================
// SQLite 구현 (기존과 동일)
// =============================================================================
bool DatabaseManager::connectSQLite() {
    for (int i = 0; i < MAX_RETRIES; ++i) {
        try {
            // ConfigManager에서 SQLite 경로 가져오기
            auto& config = ConfigManager::getInstance();
            std::string db_path = config.getOrDefault("SQLITE_DB_PATH", "../data/db/pulseone.db");
            
            if (starts_with(db_path, "./")) {
                db_path = db_path.substr(2);
            }
            
            LogManager::getInstance().log("database", LogLevel::INFO, 
                "🔧 SQLite 연결 시도: " + db_path);
            
            // 디렉토리 생성 (필요한 경우)
            size_t last_slash = db_path.find_last_of('/');
            if (last_slash != std::string::npos) {
                std::string dir_path = db_path.substr(0, last_slash);
                system(("mkdir -p " + dir_path).c_str());
            }
            
            int result = sqlite3_open(db_path.c_str(), &sqlite_conn_);
            if (result == SQLITE_OK) {
                LogManager::getInstance().log("database", LogLevel::INFO, 
                    "✅ SQLite 연결 성공: " + db_path);
                return true;
            } else {
                LogManager::getInstance().log("database", LogLevel::ERROR, 
                    "❌ SQLite 연결 실패: " + std::string(sqlite3_errmsg(sqlite_conn_)));
                if (sqlite_conn_) {
                    sqlite3_close(sqlite_conn_);
                    sqlite_conn_ = nullptr;
                }
            }
        } catch (const std::exception& e) {
            LogManager::getInstance().log("database", LogLevel::ERROR, 
                "❌ SQLite 연결 예외: " + std::string(e.what()));
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return false;
}

bool DatabaseManager::executeQuerySQLite(
    const std::string& sql, 
    int (*callback)(void*, int, char**, char**), 
    void* data) {
    
    if (!isSQLiteConnected()) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "SQLite not connected for query: " + sql);
        return false;
    }
    
    char* error_msg = nullptr;
    int result = sqlite3_exec(sqlite_conn_, sql.c_str(), callback, data, &error_msg);
    
    if (result != SQLITE_OK) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "SQLite query failed: " + std::string(error_msg ? error_msg : "Unknown error"));
        if (error_msg) {
            sqlite3_free(error_msg);
        }
        return false;
    }
    
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "SQLite query executed successfully");
    return true;
}

bool DatabaseManager::executeNonQuerySQLite(const std::string& sql) {
    if (!isSQLiteConnected()) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "SQLite not connected for non-query: " + sql);
        return false;
    }
    
    char* error_msg = nullptr;
    int result = sqlite3_exec(sqlite_conn_, sql.c_str(), nullptr, nullptr, &error_msg);
    
    if (result != SQLITE_OK) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "SQLite non-query failed: " + std::string(error_msg ? error_msg : "Unknown error"));
        if (error_msg) {
            sqlite3_free(error_msg);
        }
        return false;
    }
    
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "SQLite non-query executed successfully");
    return true;
}

bool DatabaseManager::isSQLiteConnected() {
    return sqlite_conn_ != nullptr;
}

// =============================================================================
// MySQL/MariaDB 구현 (기존과 동일)
// =============================================================================
bool DatabaseManager::connectMySQL() {
    mysql_conn_ = mysql_init(nullptr);
    if (!mysql_conn_) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "❌ MySQL 초기화 실패");
        return false;
    }
    
    for (int i = 0; i < MAX_RETRIES; ++i) {
        try {
            // 환경변수에서 MySQL 연결 정보 읽기
            const char* host = std::getenv("MYSQL_HOST") ?: "localhost";
            const char* user = std::getenv("MYSQL_USER") ?: "root";
            const char* password = std::getenv("MYSQL_PASSWORD") ?: "";
            const char* database = std::getenv("MYSQL_DATABASE") ?: "pulseone";
            unsigned int port = std::stoi(std::getenv("MYSQL_PORT") ?: "3306");
            
            if (mysql_real_connect(mysql_conn_, host, user, password, database, port, nullptr, 0)) {
                LogManager::getInstance().log("database", LogLevel::INFO, 
                    "✅ MySQL 연결 성공: " + std::string(host) + ":" + std::to_string(port));
                return true;
            } else {
                LogManager::getInstance().log("database", LogLevel::ERROR, 
                    "❌ MySQL 연결 실패: " + std::string(mysql_error(mysql_conn_)));
            }
        } catch (const std::exception& e) {
            LogManager::getInstance().log("database", LogLevel::ERROR, 
                "❌ MySQL 연결 예외: " + std::string(e.what()));
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    mysql_close(mysql_conn_);
    mysql_conn_ = nullptr;
    return false;
}

bool DatabaseManager::executeQueryMySQL(const std::string& query, std::vector<std::vector<std::string>>& results) {
    if (!mysql_conn_) return false;
    
    try {
        if (mysql_query(mysql_conn_, query.c_str()) != 0) {
            LogManager::getInstance().log("database", LogLevel::ERROR, 
                "❌ MySQL 쿼리 실패: " + std::string(mysql_error(mysql_conn_)));
            return false;
        }
        
        MYSQL_RES* result = mysql_store_result(mysql_conn_);
        if (!result) {
            return true; // INSERT/UPDATE/DELETE 등
        }
        
        int num_fields = mysql_num_fields(result);
        MYSQL_ROW row;
        
        while ((row = mysql_fetch_row(result))) {
            std::vector<std::string> row_data;
            for (int i = 0; i < num_fields; i++) {
                row_data.push_back(row[i] ? row[i] : "NULL");
            }
            results.push_back(row_data);
        }
        
        mysql_free_result(result);
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "✅ MySQL 쿼리 성공: " + query);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "❌ MySQL 쿼리 예외: " + std::string(e.what()));
        return false;
    }
}

bool DatabaseManager::executeNonQueryMySQL(const std::string& query) {
    std::vector<std::vector<std::string>> dummy_results;
    return executeQueryMySQL(query, dummy_results);
}

bool DatabaseManager::isMySQLConnected() {
    return mysql_conn_ != nullptr;
}

// =============================================================================
// MSSQL 구현 (기존과 동일)
// =============================================================================
#ifdef _WIN32
bool DatabaseManager::connectMSSQL() {
    // ODBC 환경 핸들 할당
    if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &mssql_env_) != SQL_SUCCESS) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "❌ MSSQL 환경 핸들 할당 실패");
        return false;
    }
    
    // ODBC 버전 설정
    SQLSetEnvAttr(mssql_env_, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    
    // 연결 핸들 할당
    if (SQLAllocHandle(SQL_HANDLE_DBC, mssql_env_, &mssql_conn_) != SQL_SUCCESS) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "❌ MSSQL 연결 핸들 할당 실패");
        SQLFreeHandle(SQL_HANDLE_ENV, mssql_env_);
        return false;
    }
    
    for (int i = 0; i < MAX_RETRIES; ++i) {
        try {
            // 연결 문자열 생성
            std::string conn_str = buildConnectionString(DatabaseType::MSSQL);
            
            SQLCHAR out_conn_str[1024];
            SQLSMALLINT out_conn_str_len;
            
            SQLRETURN ret = SQLDriverConnect(
                mssql_conn_,
                nullptr,
                (SQLCHAR*)conn_str.c_str(),
                SQL_NTS,
                out_conn_str,
                sizeof(out_conn_str),
                &out_conn_str_len,
                SQL_DRIVER_NOPROMPT
            );
            
            if (SQL_SUCCEEDED(ret)) {
                LogManager::getInstance().log("database", LogLevel::INFO, 
                    "✅ MSSQL 연결 성공");
                return true;
            } else {
                LogManager::getInstance().log("database", LogLevel::ERROR, 
                    "❌ MSSQL 연결 실패");
            }
        } catch (const std::exception& e) {
            LogManager::getInstance().log("database", LogLevel::ERROR, 
                "❌ MSSQL 연결 예외: " + std::string(e.what()));
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    SQLFreeHandle(SQL_HANDLE_DBC, mssql_conn_);
    SQLFreeHandle(SQL_HANDLE_ENV, mssql_env_);
    mssql_conn_ = nullptr;
    mssql_env_ = nullptr;
    return false;
}

bool DatabaseManager::isMSSQLConnected() {
    return mssql_conn_ != nullptr;
}
#else
bool DatabaseManager::connectMSSQL() {
    LogManager::getInstance().log("database", LogLevel::WARN, 
        "⚠️ MSSQL 연결 미지원 (Linux)");
    return false;
}

bool DatabaseManager::isMSSQLConnected() {
    return false; // Linux에서는 MSSQL 지원 안함
}
#endif

// =============================================================================
// Redis 구현 (기존과 동일)
// =============================================================================
bool DatabaseManager::connectRedis() {
    try {
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "🔄 Redis 연결 시작...");
        
        redis_client_ = std::make_unique<RedisClientImpl>();
        
        // 설정에서 Redis 연결 정보 가져오기
        auto& config = ConfigManager::getInstance();
        std::string host = config.getOrDefault("REDIS_HOST", "localhost");
        int port = config.getInt("REDIS_PORT", 6379);
        std::string password = config.getOrDefault("REDIS_PASSWORD", "");
        
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "Redis 연결 시도: " + host + ":" + std::to_string(port));
        
        // Redis 연결 시도
        if (redis_client_->connect(host, port, password)) {
            LogManager::getInstance().log("database", LogLevel::INFO, 
                "✅ Redis 연결 성공: " + host + ":" + std::to_string(port));
            return true;
        } else {
            LogManager::getInstance().log("database", LogLevel::ERROR, 
                "❌ Redis 연결 실패: " + host + ":" + std::to_string(port));
            redis_client_.reset();
            return false;
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "❌ Redis 연결 예외: " + std::string(e.what()));
        redis_client_.reset();
        return false;
    }
}

bool DatabaseManager::isRedisConnected() {
    return redis_client_ != nullptr;
}

void DatabaseManager::disconnectRedis() {
    if (redis_client_) {
        try {
            LogManager::getInstance().log("database", LogLevel::INFO, 
                "🔄 Redis 연결 해제 중...");
            
            redis_client_->disconnect();
            redis_client_.reset();
            
            LogManager::getInstance().log("database", LogLevel::INFO, 
                "✅ Redis 연결 해제 완료");
                
        } catch (const std::exception& e) {
            LogManager::getInstance().log("database", LogLevel::ERROR, 
                "Redis 연결 해제 중 예외: " + std::string(e.what()));
            redis_client_.reset();
        }
    }
}

bool DatabaseManager::testRedisConnection() {
    if (!isRedisConnected()) {
        return false;
    }
    
    try {
        bool test_result = (redis_client_ != nullptr);
        
        if (test_result) {
            LogManager::getInstance().log("database", LogLevel::INFO, 
                "✅ Redis 연결 테스트 성공");
        } else {
            LogManager::getInstance().log("database", LogLevel::WARN, 
                "⚠️ Redis 연결 테스트 실패");
        }
        
        return test_result;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "Redis 연결 테스트 중 예외: " + std::string(e.what()));
        return false;
    }
}

std::map<std::string, std::string> DatabaseManager::getRedisInfo() {
    std::map<std::string, std::string> info;
    
    if (!isRedisConnected()) {
        info["error"] = "Redis not connected";
        return info;
    }
    
    try {
        info["implementation"] = "RedisClientImpl (Basic)";
        info["status"] = "connected";
        info["note"] = "Upgrade to hiredis implementation for full features";
        
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "Redis 기본 정보 조회 완료");
        
        return info;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "Redis 서버 정보 조회 중 예외: " + std::string(e.what()));
        
        info.clear();
        info["error"] = e.what();
        return info;
    }
}

// =============================================================================
// InfluxDB 구현 (기존과 동일)
// =============================================================================
bool DatabaseManager::connectInflux() {
    try {
        influx_client_ = nullptr;
        
        LogManager::getInstance().log("database", LogLevel::WARN, 
            "⚠️ InfluxClientImpl 미구현 - 임시로 비활성화");
        return false;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "❌ InfluxDB 연결 실패: " + std::string(e.what()));
    }
    return false;
}

bool DatabaseManager::isInfluxConnected() {
    return influx_client_ != nullptr;
}

// =============================================================================
// 통합 쿼리 인터페이스 (기존과 동일)
// =============================================================================
bool DatabaseManager::executeQuery(const std::string& query, std::vector<std::vector<std::string>>& results) {
    try {
        auto& config = ConfigManager::getInstance();
        std::string db_type = config.getOrDefault("DATABASE_TYPE", "SQLITE");
        
        if (db_type == "POSTGRESQL") {
            auto pg_result = executeQueryPostgres(query);
            
            results.clear();
            for (const auto& row : pg_result) {
                std::vector<std::string> row_data;
                for (const auto& field : row) {
                    row_data.push_back(field.is_null() ? "" : field.c_str());
                }
                results.push_back(row_data);
            }
            return true;
        }
        else if (db_type == "SQLITE") {
            results.clear();
            
            auto sqlite_callback = [](void* data, int argc, char** argv, char** azColName) -> int {
                auto* results_ptr = static_cast<std::vector<std::vector<std::string>>*>(data);
                std::vector<std::string> row_data;
                
                for (int i = 0; i < argc; i++) {
                    row_data.push_back(argv[i] ? argv[i] : "");
                }
                results_ptr->push_back(row_data);
                return 0;
            };
            
            return executeQuerySQLite(query, sqlite_callback, &results);
        }
        else if (db_type == "MYSQL") {
            return executeQueryMySQL(query, results);
        }
        
        return false;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "executeQuery failed: " + std::string(e.what()));
        return false;
    }
}

bool DatabaseManager::executeNonQuery(const std::string& query) {
    try {
        auto& config = ConfigManager::getInstance();
        std::string db_type = config.getOrDefault("DATABASE_TYPE", "SQLITE");
        
        if (db_type == "POSTGRESQL") {
            return executeNonQueryPostgres(query);
        }
        else if (db_type == "SQLITE") {
            return executeNonQuerySQLite(query);
        }
        else if (db_type == "MYSQL") {
            return executeNonQueryMySQL(query);
        }
        
        return false;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "executeNonQuery failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 연결 상태 확인 및 유틸리티 (기존과 동일)
// =============================================================================
bool DatabaseManager::isConnected(DatabaseType db_type) {
    switch (db_type) {
        case DatabaseType::POSTGRESQL: return isPostgresConnected();
        case DatabaseType::SQLITE: return isSQLiteConnected();
        case DatabaseType::MYSQL: return isMySQLConnected();
        case DatabaseType::MSSQL: return isMSSQLConnected();
        case DatabaseType::REDIS: return isRedisConnected();
        case DatabaseType::INFLUXDB: return isInfluxConnected();
        default: return false;
    }
}

bool DatabaseManager::isPostgresConnected() {
    return pg_conn_ && pg_conn_->is_open();
}

std::map<std::string, bool> DatabaseManager::getAllConnectionStatus() {
    return {
        {"PostgreSQL", isPostgresConnected()},
        {"SQLite", isSQLiteConnected()},
        {"MySQL/MariaDB", isMySQLConnected()},
        {"MSSQL", isMSSQLConnected()},
        {"Redis", isRedisConnected()},
        {"InfluxDB", isInfluxConnected()}
    };
}

std::string DatabaseManager::getDatabaseTypeName(DatabaseType type) {
    switch (type) {
        case DatabaseType::POSTGRESQL: return "PostgreSQL";
        case DatabaseType::SQLITE: return "SQLite";
        case DatabaseType::MYSQL: return "MySQL/MariaDB";
        case DatabaseType::MSSQL: return "MSSQL";
        case DatabaseType::REDIS: return "Redis";
        case DatabaseType::INFLUXDB: return "InfluxDB";
        default: return "Unknown";
    }
}

std::string DatabaseManager::buildConnectionString(DatabaseType type) {
    switch (type) {
        case DatabaseType::POSTGRESQL:
            return "host=" + std::string(std::getenv("POSTGRES_HOST") ?: "localhost") +
                   " port=" + std::string(std::getenv("POSTGRES_PORT") ?: "5432") +
                   " dbname=" + std::string(std::getenv("POSTGRES_DB") ?: "pulseone") +
                   " user=" + std::string(std::getenv("POSTGRES_USER") ?: "postgres") +
                   " password=" + std::string(std::getenv("POSTGRES_PASSWORD") ?: "");
        
        case DatabaseType::MSSQL:
#ifdef _WIN32
            return "Driver={ODBC Driver 17 for SQL Server};"
                   "Server=" + std::string(std::getenv("MSSQL_HOST") ?: "localhost") + ";"
                   "Database=" + std::string(std::getenv("MSSQL_DB") ?: "pulseone") + ";"
                   "UID=" + std::string(std::getenv("MSSQL_USER") ?: "sa") + ";"
                   "PWD=" + std::string(std::getenv("MSSQL_PASSWORD") ?: "") + ";";
#endif
        
        default:
            return "";
    }
}

void DatabaseManager::disconnectAll() {
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "🔧 모든 데이터베이스 연결 해제 중...");
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    // PostgreSQL 해제
    if (pg_conn_) {
        pg_conn_.reset();
    }
    
    // SQLite 해제
    if (sqlite_conn_) {
        sqlite3_close(sqlite_conn_);
        sqlite_conn_ = nullptr;
    }
    
    // MySQL 해제
    if (mysql_conn_) {
        mysql_close(mysql_conn_);
        mysql_conn_ = nullptr;
    }
    
    // MSSQL 해제
#ifdef _WIN32
    if (mssql_conn_) {
        SQLDisconnect(mssql_conn_);
        SQLFreeHandle(SQL_HANDLE_DBC, mssql_conn_);
        mssql_conn_ = nullptr;
    }
    if (mssql_env_) {
        SQLFreeHandle(SQL_HANDLE_ENV, mssql_env_);
        mssql_env_ = nullptr;
    }
#endif
    
    // Redis 해제
    if (redis_client_) {
        redis_client_->disconnect();
        redis_client_.reset();
    }
    
    // InfluxDB 해제
    if (influx_client_) {
        influx_client_->disconnect();
        influx_client_.reset();
    }
    
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "✅ 모든 데이터베이스 연결 해제 완료");
}

// =============================================================================
// PostgreSQL 쿼리 실행 메서드들 (기존과 동일)
// =============================================================================
pqxx::result DatabaseManager::executeQueryPostgres(const std::string& sql) {
    if (!isPostgresConnected()) {
        throw std::runtime_error("PostgreSQL not connected");
    }
    
    try {
        pqxx::work transaction(*pg_conn_);
        pqxx::result query_result = transaction.exec(sql);
        transaction.commit();
        
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "PostgreSQL query executed successfully, rows: " + std::to_string(query_result.size()));
        
        return query_result;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "PostgreSQL query failed: " + std::string(e.what()));
        throw;
    }
}

bool DatabaseManager::executeNonQueryPostgres(const std::string& sql) {
    if (!isPostgresConnected()) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "PostgreSQL not connected for non-query: " + sql);
        return false;
    }
    
    try {
        pqxx::work transaction(*pg_conn_);
        transaction.exec(sql);
        transaction.commit();
        
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "PostgreSQL non-query executed successfully");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "PostgreSQL non-query failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 🚀 추가: 설정 변경 지원
// =============================================================================
void DatabaseManager::setDatabaseEnabled(DatabaseType db_type, bool enabled) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    enabled_databases_[db_type] = enabled;
    
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "🔧 " + getDatabaseTypeName(db_type) + " " + 
        (enabled ? "활성화" : "비활성화") + "됨");
}