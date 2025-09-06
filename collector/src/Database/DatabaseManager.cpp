// DatabaseManager.cpp - 크로스플랫폼 버전
#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include <thread>
#include <chrono>

// 정적 멤버 초기화
std::once_flag DatabaseManager::init_flag_;
std::atomic<bool> DatabaseManager::initialization_success_(false);

DatabaseManager::DatabaseManager() {
    enabled_databases_[DatabaseType::SQLITE] = true;  // 기본값
    primary_rdb_ = DatabaseType::SQLITE;
}

DatabaseManager::~DatabaseManager() {
    disconnectAll();
}

DatabaseManager& DatabaseManager::getInstance() {
    static DatabaseManager instance;
    return instance;
}

bool DatabaseManager::initialize() {
    return doInitialize();
}

bool DatabaseManager::doInitialize() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "🔧 DatabaseManager 초기화 시작...");
    
    // 사용 가능한 DB 목록 출력
    logAvailableDatabases();
    
    // 설정 로드
    loadDatabaseConfig();
    
    // SQLite 연결 (항상 시도)
    if (enabled_databases_[DatabaseType::SQLITE]) {
        if (connectSQLite()) {
            LogManager::getInstance().log("database", LogLevel::INFO, 
                "✅ SQLite 연결 성공");
        } else {
            LogManager::getInstance().log("database", LogLevel::ERROR, 
                "❌ SQLite 연결 실패");
            return false;
        }
    }
    
#ifdef HAS_POSTGRESQL
    if (enabled_databases_[DatabaseType::POSTGRESQL]) {
        if (connectPostgres()) {
            LogManager::getInstance().log("database", LogLevel::INFO, 
                "✅ PostgreSQL 연결 성공");
        }
    }
#endif

#ifdef HAS_MYSQL
    if (enabled_databases_[DatabaseType::MYSQL]) {
        if (connectMySQL()) {
            LogManager::getInstance().log("database", LogLevel::INFO, 
                "✅ MySQL 연결 성공");
        }
    }
#endif

#ifdef HAS_MSSQL
    if (enabled_databases_[DatabaseType::MSSQL]) {
        if (connectMSSQL()) {
            LogManager::getInstance().log("database", LogLevel::INFO, 
                "✅ MSSQL 연결 성공");
        }
    }
#endif

#ifdef HAS_REDIS
    if (enabled_databases_[DatabaseType::REDIS]) {
        connectRedis();
    }
#endif

    return true;
}

void DatabaseManager::loadDatabaseConfig() {
    auto& config = ConfigManager::getInstance();
    std::string db_type = config.getOrDefault("DATABASE_TYPE", "SQLITE");
    
    // 모든 DB 비활성화 후 선택된 것만 활성화
    for (auto& [type, enabled] : enabled_databases_) {
        enabled = false;
    }
    
    if (db_type == "SQLITE") {
        enabled_databases_[DatabaseType::SQLITE] = true;
        primary_rdb_ = DatabaseType::SQLITE;
    }
#ifdef HAS_POSTGRESQL
    else if (db_type == "POSTGRESQL") {
        enabled_databases_[DatabaseType::POSTGRESQL] = true;
        primary_rdb_ = DatabaseType::POSTGRESQL;
    }
#endif
#ifdef HAS_MYSQL
    else if (db_type == "MYSQL") {
        enabled_databases_[DatabaseType::MYSQL] = true;
        primary_rdb_ = DatabaseType::MYSQL;
    }
#endif
#ifdef HAS_MSSQL
    else if (db_type == "MSSQL") {
        enabled_databases_[DatabaseType::MSSQL] = true;
        primary_rdb_ = DatabaseType::MSSQL;
    }
#endif
    else {
        // 기본값: SQLite
        enabled_databases_[DatabaseType::SQLITE] = true;
        primary_rdb_ = DatabaseType::SQLITE;
    }
}

// ========================================================================
// SQLite 구현 (항상 포함)
// ========================================================================
bool DatabaseManager::connectSQLite() {
    auto& config = ConfigManager::getInstance();
    std::string db_path = config.getOrDefault("SQLITE_DB_PATH", "pulseone.db");
    
    int result = sqlite3_open(db_path.c_str(), &sqlite_conn_);
    if (result == SQLITE_OK) {
        return true;
    }
    
    if (sqlite_conn_) {
        sqlite3_close(sqlite_conn_);
        sqlite_conn_ = nullptr;
    }
    return false;
}

bool DatabaseManager::executeQuerySQLite(const std::string& sql, 
                                        int (*callback)(void*, int, char**, char**), 
                                        void* data) {
    if (!sqlite_conn_) return false;
    
    char* error_msg = nullptr;
    int result = sqlite3_exec(sqlite_conn_, sql.c_str(), callback, data, &error_msg);
    
    if (result != SQLITE_OK) {
        if (error_msg) sqlite3_free(error_msg);
        return false;
    }
    return true;
}

bool DatabaseManager::executeNonQuerySQLite(const std::string& sql) {
    return executeQuerySQLite(sql, nullptr, nullptr);
}

bool DatabaseManager::isSQLiteConnected() {
    return sqlite_conn_ != nullptr;
}

// ========================================================================
// PostgreSQL 구현 (조건부)
// ========================================================================
#ifdef HAS_POSTGRESQL
bool DatabaseManager::connectPostgres() {
    try {
        std::string connStr = buildConnectionString(DatabaseType::POSTGRESQL);
        pg_conn_ = std::make_unique<pqxx::connection>(connStr);
        return pg_conn_->is_open();
    } catch (...) {
        return false;
    }
}

bool DatabaseManager::isPostgresConnected() {
    return pg_conn_ && pg_conn_->is_open();
}

pqxx::result DatabaseManager::executeQueryPostgres(const std::string& sql) {
    if (!pg_conn_) throw std::runtime_error("PostgreSQL not connected");
    
    pqxx::work txn(*pg_conn_);
    pqxx::result result = txn.exec(sql);
    txn.commit();
    return result;
}

bool DatabaseManager::executeNonQueryPostgres(const std::string& sql) {
    try {
        executeQueryPostgres(sql);
        return true;
    } catch (...) {
        return false;
    }
}
#endif

// ========================================================================
// MySQL 구현 (조건부)
// ========================================================================
#ifdef HAS_MYSQL
bool DatabaseManager::connectMySQL() {
    mysql_conn_ = mysql_init(nullptr);
    if (!mysql_conn_) return false;
    
    auto& config = ConfigManager::getInstance();
    const char* host = config.getOrDefault("MYSQL_HOST", "localhost").c_str();
    const char* user = config.getOrDefault("MYSQL_USER", "root").c_str();
    const char* pass = config.getOrDefault("MYSQL_PASSWORD", "").c_str();
    const char* db = config.getOrDefault("MYSQL_DATABASE", "pulseone").c_str();
    int port = config.getInt("MYSQL_PORT", 3306);
    
    if (mysql_real_connect(mysql_conn_, host, user, pass, db, port, nullptr, 0)) {
        return true;
    }
    
    mysql_close(mysql_conn_);
    mysql_conn_ = nullptr;
    return false;
}

bool DatabaseManager::isMySQLConnected() {
    return mysql_conn_ != nullptr;
}

bool DatabaseManager::executeQueryMySQL(const std::string& query, 
                                       std::vector<std::vector<std::string>>& results) {
    if (!mysql_conn_) return false;
    
    if (mysql_query(mysql_conn_, query.c_str()) != 0) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(mysql_conn_);
    if (!result) return true;
    
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
    return true;
}

bool DatabaseManager::executeNonQueryMySQL(const std::string& query) {
    std::vector<std::vector<std::string>> dummy;
    return executeQueryMySQL(query, dummy);
}
#endif

// ========================================================================
// MSSQL 구현 (Windows 전용)
// ========================================================================
#ifdef HAS_MSSQL
bool DatabaseManager::connectMSSQL() {
    if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &mssql_env_) != SQL_SUCCESS) {
        return false;
    }
    
    SQLSetEnvAttr(mssql_env_, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    
    if (SQLAllocHandle(SQL_HANDLE_DBC, mssql_env_, &mssql_conn_) != SQL_SUCCESS) {
        SQLFreeHandle(SQL_HANDLE_ENV, mssql_env_);
        return false;
    }
    
    std::string conn_str = buildConnectionString(DatabaseType::MSSQL);
    SQLCHAR out_conn_str[1024];
    SQLSMALLINT out_conn_str_len;
    
    SQLRETURN ret = SQLDriverConnect(
        mssql_conn_, nullptr,
        (SQLCHAR*)conn_str.c_str(), SQL_NTS,
        out_conn_str, sizeof(out_conn_str),
        &out_conn_str_len, SQL_DRIVER_NOPROMPT
    );
    
    if (SQL_SUCCEEDED(ret)) {
        return true;
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
#endif

// ========================================================================
// Redis 구현 (조건부)
// ========================================================================
#ifdef HAS_REDIS
bool DatabaseManager::connectRedis() {
    try {
        redis_client_ = std::make_unique<RedisClientImpl>();
        auto& config = ConfigManager::getInstance();
        return redis_client_->connect(
            config.getOrDefault("REDIS_HOST", "localhost"),
            config.getInt("REDIS_PORT", 6379),
            config.getOrDefault("REDIS_PASSWORD", "")
        );
    } catch (...) {
        return false;
    }
}

bool DatabaseManager::isRedisConnected() {
    return redis_client_ != nullptr;
}
#endif

// ========================================================================
// 통합 쿼리 인터페이스
// ========================================================================
bool DatabaseManager::executeQuery(const std::string& query, 
                                  std::vector<std::vector<std::string>>& results) {
    switch (primary_rdb_) {
        case DatabaseType::SQLITE:
            results.clear();
            return executeQuerySQLite(query, 
                [](void* data, int argc, char** argv, char**) -> int {
                    auto* res = static_cast<std::vector<std::vector<std::string>>*>(data);
                    std::vector<std::string> row;
                    for (int i = 0; i < argc; i++) {
                        row.push_back(argv[i] ? argv[i] : "");
                    }
                    res->push_back(row);
                    return 0;
                }, &results);
            
#ifdef HAS_POSTGRESQL
        case DatabaseType::POSTGRESQL: {
            auto pg_result = executeQueryPostgres(query);
            for (const auto& row : pg_result) {
                std::vector<std::string> row_data;
                for (const auto& field : row) {
                    row_data.push_back(field.c_str());
                }
                results.push_back(row_data);
            }
            return true;
        }
#endif

#ifdef HAS_MYSQL
        case DatabaseType::MYSQL:
            return executeQueryMySQL(query, results);
#endif

        default:
            return false;
    }
}

bool DatabaseManager::executeNonQuery(const std::string& query) {
    switch (primary_rdb_) {
        case DatabaseType::SQLITE:
            return executeNonQuerySQLite(query);
            
#ifdef HAS_POSTGRESQL
        case DatabaseType::POSTGRESQL:
            return executeNonQueryPostgres(query);
#endif

#ifdef HAS_MYSQL
        case DatabaseType::MYSQL:
            return executeNonQueryMySQL(query);
#endif

        default:
            return false;
    }
}

// ========================================================================
// 유틸리티
// ========================================================================
void DatabaseManager::disconnectAll() {
    if (sqlite_conn_) {
        sqlite3_close(sqlite_conn_);
        sqlite_conn_ = nullptr;
    }
    
#ifdef HAS_POSTGRESQL
    pg_conn_.reset();
#endif

#ifdef HAS_MYSQL
    if (mysql_conn_) {
        mysql_close(mysql_conn_);
        mysql_conn_ = nullptr;
    }
#endif

#ifdef HAS_MSSQL
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

#ifdef HAS_REDIS
    redis_client_.reset();
#endif
}

std::string DatabaseManager::getDatabaseTypeName(DatabaseType type) {
    switch (type) {
        case DatabaseType::SQLITE: return "SQLite";
        case DatabaseType::POSTGRESQL: return "PostgreSQL";
        case DatabaseType::MYSQL: return "MySQL";
        case DatabaseType::MSSQL: return "MSSQL";
        case DatabaseType::REDIS: return "Redis";
        case DatabaseType::INFLUXDB: return "InfluxDB";
        default: return "Unknown";
    }
}

std::string DatabaseManager::buildConnectionString(DatabaseType type) {
    auto& config = ConfigManager::getInstance();
    
    switch (type) {
        case DatabaseType::POSTGRESQL:
            return "host=" + config.getOrDefault("POSTGRES_HOST", "localhost") +
                   " port=" + config.getOrDefault("POSTGRES_PORT", "5432") +
                   " dbname=" + config.getOrDefault("POSTGRES_DB", "pulseone");
        
        case DatabaseType::MSSQL:
            return "Driver={ODBC Driver 17 for SQL Server};"
                   "Server=" + config.getOrDefault("MSSQL_HOST", "localhost") + ";";
        
        default:
            return "";
    }
}