// DatabaseManager.cpp - 완전한 멀티DB 지원 구현
#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Client/RedisClientImpl.h"
#include <cstdlib>
#include <thread>
#include <chrono>
#include <sstream>
#include <iostream>

DatabaseManager::DatabaseManager() {
    // 기본값: PostgreSQL을 메인 RDB로 설정
    enabled_databases_[DatabaseType::POSTGRESQL] = true;
    enabled_databases_[DatabaseType::SQLITE] = true;
    enabled_databases_[DatabaseType::MYSQL] = false;
    enabled_databases_[DatabaseType::MSSQL] = false;
    enabled_databases_[DatabaseType::REDIS] = true;
    enabled_databases_[DatabaseType::INFLUXDB] = true;
}

DatabaseManager::~DatabaseManager() {
    disconnectAll();
}

DatabaseManager& DatabaseManager::getInstance() {
    static DatabaseManager instance;
    return instance;
}

bool DatabaseManager::initialize() {
    bool overall_success = true;
    
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "🔧 DatabaseManager 초기화 시작...");
    
    // database.env에서 설정 로드
    loadDatabaseConfig();
    
    // 활성화된 데이터베이스들만 연결
    if (enabled_databases_[DatabaseType::POSTGRESQL]) {
        if (!connectPostgres()) {
            LogManager::getInstance().log("database", LogLevel::WARN, 
                "⚠️ PostgreSQL 연결 실패 - 계속 진행");
            overall_success = false;
        }
    }
    
    if (enabled_databases_[DatabaseType::SQLITE]) {
        if (!connectSQLite()) {
            LogManager::getInstance().log("database", LogLevel::WARN, 
                "⚠️ SQLite 연결 실패 - 계속 진행");
            overall_success = false;
        }
    }
    
    if (enabled_databases_[DatabaseType::MYSQL]) {
        if (!connectMySQL()) {
            LogManager::getInstance().log("database", LogLevel::WARN, 
                "⚠️ MySQL 연결 실패 - 계속 진행");
            overall_success = false;
        }
    }
    
#ifdef _WIN32
    if (enabled_databases_[DatabaseType::MSSQL]) {
        if (!connectMSSQL()) {
            LogManager::getInstance().log("database", LogLevel::WARN, 
                "⚠️ MSSQL 연결 실패 - 계속 진행");
            overall_success = false;
        }
    }
#endif
    
    if (enabled_databases_[DatabaseType::REDIS]) {
        if (!connectRedis()) {
            LogManager::getInstance().log("database", LogLevel::WARN, 
                "⚠️ Redis 연결 실패 - 계속 진행");
            overall_success = false;
        }
    }
    
    if (enabled_databases_[DatabaseType::INFLUXDB]) {
        if (!connectInflux()) {
            LogManager::getInstance().log("database", LogLevel::WARN, 
                "⚠️ InfluxDB 연결 실패 - 계속 진행");
            overall_success = false;
        }
    }
    
    // 연결 상태 출력
    auto status = getAllConnectionStatus();
    for (const auto& [db_name, connected] : status) {
        std::string status_icon = connected ? "✅" : "❌";
        LogManager::getInstance().log("database", LogLevel::INFO, 
            status_icon + " " + db_name + ": " + (connected ? "연결됨" : "연결 안됨"));
    }
    
    return overall_success;
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
// 🔧 MySQL/MariaDB 구현 (새로 추가)
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

// =============================================================================
// 🔧 MSSQL 구현 (새로 추가)
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
#endif

// =============================================================================
// Redis 구현
// =============================================================================

bool DatabaseManager::connectRedis() {
    try {
        // ✅ 구체적인 구현체 사용 (abstract class가 아닌)
        redis_client_ = std::make_unique<RedisClientImpl>();
        
        std::string host = std::getenv("REDIS_HOST") ?: "localhost";
        int port = std::stoi(std::getenv("REDIS_PORT") ?: "6379");
        std::string password = std::getenv("REDIS_PASSWORD") ?: "";
        
        if (redis_client_->connect(host, port, password)) {
            LogManager::getInstance().log("database", LogLevel::INFO, 
                "✅ Redis 연결 성공: " + host + ":" + std::to_string(port));
            return true;
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "❌ Redis 연결 실패: " + std::string(e.what()));
    }
    return false;
}

// =============================================================================
// InfluxDB 구현
// =============================================================================

bool DatabaseManager::connectInflux() {
    try {
        // ✅ InfluxClientImpl이 없으므로 임시로 nullptr 설정
        influx_client_ = nullptr;
        
        LogManager::getInstance().log("database", LogLevel::WARN, 
            "⚠️ InfluxClientImpl 미구현 - 임시로 비활성화");
        return false;  // 임시로 false 반환
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "❌ InfluxDB 연결 실패: " + std::string(e.what()));
    }
    return false;
}

// =============================================================================
// 연결 상태 확인
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

bool DatabaseManager::isMySQLConnected() {
    return mysql_conn_ != nullptr;
}

#ifdef _WIN32
bool DatabaseManager::isMSSQLConnected() {
    return mssql_conn_ != nullptr;
}
#else
bool DatabaseManager::isMSSQLConnected() {
    return false; // Linux에서는 MSSQL 지원 안함
}
#endif

bool DatabaseManager::isRedisConnected() {
    if (!redis_client_) return false;
    
    // ✅ RedisClientImpl로 캐스팅 (이제 타입이 알려짐)
    auto redis_impl = dynamic_cast<RedisClientImpl*>(redis_client_.get());
    return redis_impl && redis_impl->isConnected();
}

bool DatabaseManager::isInfluxConnected() {
    // ✅ InfluxClientImpl이 없으므로 임시로 false 반환
    return influx_client_ != nullptr;
}

// =============================================================================
// 설정 및 유틸리티
// =============================================================================

void DatabaseManager::loadDatabaseConfig() {
    // ConfigManager에서 설정 로드
    auto& config = ConfigManager::getInstance();
    
    // ✅ getOrDefault() 메서드 사용 (getValue 대신)
    enabled_databases_[DatabaseType::POSTGRESQL] = 
        config.getOrDefault("POSTGRES_ENABLED", "true") == "true";
    enabled_databases_[DatabaseType::SQLITE] = 
        config.getOrDefault("SQLITE_ENABLED", "true") == "true";
    enabled_databases_[DatabaseType::MYSQL] = 
        config.getOrDefault("MYSQL_ENABLED", "false") == "true";
    enabled_databases_[DatabaseType::MSSQL] = 
        config.getOrDefault("MSSQL_ENABLED", "false") == "true";
    enabled_databases_[DatabaseType::REDIS] = 
        config.getOrDefault("REDIS_ENABLED", "true") == "true";
    enabled_databases_[DatabaseType::INFLUXDB] = 
        config.getOrDefault("INFLUX_ENABLED", "true") == "true";
    
    // 메인 RDB 설정
    std::string primary_db = config.getOrDefault("PRIMARY_DB", "postgresql");
    if (primary_db == "mysql") primary_rdb_ = DatabaseType::MYSQL;
    else if (primary_db == "mssql") primary_rdb_ = DatabaseType::MSSQL;
    else if (primary_db == "sqlite") primary_rdb_ = DatabaseType::SQLITE;
    else primary_rdb_ = DatabaseType::POSTGRESQL;
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

bool DatabaseManager::isPostgresConnected() {
    return pg_conn_ && pg_conn_->is_open();
}

// =============================================================================
// 🔥 SQLite 함수들 구현
// =============================================================================

bool DatabaseManager::connectSQLite() {
    for (int i = 0; i < MAX_RETRIES; ++i) {
        try {
            std::string db_path = std::getenv("SQLITE_PATH") ?: "/app/data/pulseone.db";
            
            // 디렉토리 생성
            std::string dir_path = db_path.substr(0, db_path.find_last_of('/'));
            system(("mkdir -p " + dir_path).c_str());
            
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