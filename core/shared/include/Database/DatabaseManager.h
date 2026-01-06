#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include "Platform/PlatformCompat.h"
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <mutex>
#include <atomic>

// ========================================================================
// 데이터베이스 드라이버 지원 감지 (조건부 컴파일)
// ========================================================================

// SQLite는 항상 지원
#define HAS_SQLITE 1
#include <sqlite3.h>

// PostgreSQL 지원 확인
#if !defined(PULSEONE_WINDOWS) && __has_include(<pqxx/pqxx>)
    #define HAS_POSTGRESQL 1
    #include <pqxx/pqxx>
#endif

// MySQL/MariaDB 지원 확인  
#if !defined(PULSEONE_WINDOWS) && __has_include(<mysql/mysql.h>)
    #define HAS_MYSQL 1
    #include <mysql/mysql.h>
#endif

// MSSQL은 Windows ODBC를 통해서만 지원 + 비활성화 체크 먼저!
#if defined(PULSEONE_DISABLE_MSSQL) || defined(DISABLE_MSSQL) || defined(NO_MSSQL)
    #define HAS_MSSQL 0
#elif defined(_WIN32)
    #define HAS_MSSQL 1
    #include <sql.h>
    #include <sqlext.h>
#else
    #define HAS_MSSQL 0
#endif

// Redis 클라이언트 (항상 지원)
#if __has_include("Client/RedisClientImpl.h")
    #define HAS_REDIS 1
    #include "Client/RedisClientImpl.h"
#else
    #define HAS_REDIS 1  // 더미로라도 항상 지원
    class RedisClientImpl {
    public:
        bool connect(const std::string&, int, const std::string&) { return false; }
        void disconnect() {}
    };
    using RedisClient = RedisClientImpl;
#endif

// InfluxDB 클라이언트 (옵션)
#include "Client/InfluxClient.h"
#include "Client/InfluxClientImpl.h"
#define HAS_INFLUX 1

#include "Utils/LogManager.h"

class DatabaseManager {
public:
    enum class DatabaseType {
        SQLITE,      // 모든 플랫폼
        POSTGRESQL,  // Linux/Unix
        MYSQL,       // Linux/Unix
        MSSQL,       // Windows
        REDIS,       // 항상 지원
        INFLUXDB     // 옵션
    };
    
    // 싱글톤 패턴
    static DatabaseManager& getInstance();
    
    // 초기화 및 설정
    bool initialize();
    void reinitialize();
    void setDatabaseEnabled(DatabaseType db_type, bool enabled);
    
    // 연결 상태 확인
    bool isConnected(DatabaseType db_type);
    bool isSQLiteConnected();
    
#ifdef HAS_POSTGRESQL
    bool isPostgresConnected();
#endif

#ifdef HAS_MYSQL
    bool isMySQLConnected();
#endif

    bool isMSSQLConnected();
    bool isRedisConnected();
    bool isInfluxConnected();
    
    // SQLite 관련 (항상 지원)
    bool executeQuerySQLite(const std::string& query,
                           int (*callback)(void*, int, char**, char**),
                           void* data = nullptr);
    bool executeNonQuerySQLite(const std::string& query);
    sqlite3* getSQLiteConnection() { return sqlite_conn_; }
    
    // PostgreSQL 관련 (조건부)
#ifdef HAS_POSTGRESQL
    pqxx::result executeQueryPostgres(const std::string& query);
    bool executeNonQueryPostgres(const std::string& query);
    pqxx::connection* getPostgresConnection() { return pg_conn_.get(); }
#endif
    
    // MySQL/MariaDB 관련 (조건부)
#ifdef HAS_MYSQL
    bool executeQueryMySQL(const std::string& query, 
                          std::vector<std::vector<std::string>>& results);
    bool executeNonQueryMySQL(const std::string& query);
    MYSQL* getMySQLConnection() { return mysql_conn_; }
#endif
    
    // MSSQL 관련
    bool executeQueryMSSQL(const std::string& query, 
                          std::vector<std::vector<std::string>>& results);
    bool executeNonQueryMSSQL(const std::string& query);
    void* getMSSQLConnection();
    
    // Redis 관련 (항상 지원)
    RedisClient* getRedisClient() { return redis_client_.get(); }
    bool connectRedis();
    void disconnectRedis();
    bool testRedisConnection();
    std::map<std::string, std::string> getRedisInfo();
    
    // InfluxDB 관련 (옵션)
#ifdef HAS_INFLUX
    PulseOne::Client::InfluxClient* getInfluxClient() { return influx_client_.get(); }
    bool connectInflux();
    void disconnectInflux();
#endif
    
    // 통합 쿼리 인터페이스
    bool executeQuery(const std::string& query, 
                     std::vector<std::vector<std::string>>& results);
    bool executeNonQuery(const std::string& query);
    
    std::map<std::string, bool> getAllConnectionStatus();
    void disconnectAll();

private:
    DatabaseManager();
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    
    void ensureInitialized();
    bool doInitialize();
    
    static std::once_flag init_flag_;
    static std::atomic<bool> initialization_success_;
    
    // 개별 DB 연결 함수
    bool connectSQLite();
    
#ifdef HAS_POSTGRESQL
    bool connectPostgres();
#endif

#ifdef HAS_MYSQL
    bool connectMySQL();
#endif

    bool connectMSSQL();
    
    // 연결 객체들
    sqlite3* sqlite_conn_ = nullptr;
    
#ifdef HAS_POSTGRESQL
    std::unique_ptr<pqxx::connection> pg_conn_;
#endif

#ifdef HAS_MYSQL
    MYSQL* mysql_conn_ = nullptr;
#endif

#if HAS_MSSQL
    SQLHENV mssql_env_ = nullptr;
    SQLHDBC mssql_conn_ = nullptr;
#else
    void* mssql_env_ = nullptr;
    void* mssql_conn_ = nullptr;
#endif

    std::unique_ptr<RedisClientImpl> redis_client_;

#ifdef HAS_INFLUX
    std::unique_ptr<PulseOne::Client::InfluxClient> influx_client_;
#endif
    
    std::map<DatabaseType, bool> enabled_databases_;
    DatabaseType primary_rdb_ = DatabaseType::SQLITE;
    const int MAX_RETRIES = 3;
    mutable std::mutex db_mutex_;
    
    void loadDatabaseConfig();
    std::string getDatabaseTypeName(DatabaseType type);
    std::string buildConnectionString(DatabaseType type);
    
    bool isDatabaseTypeSupported(DatabaseType type) const {
        switch (type) {
            case DatabaseType::SQLITE: return true;
#ifdef HAS_POSTGRESQL
            case DatabaseType::POSTGRESQL: return true;
#endif
#ifdef HAS_MYSQL
            case DatabaseType::MYSQL: return true;
#endif
#if HAS_MSSQL
            case DatabaseType::MSSQL: return true;
#else
            case DatabaseType::MSSQL: return false;
#endif
            case DatabaseType::REDIS: return true;
#ifdef HAS_INFLUX
            case DatabaseType::INFLUXDB: return true;
#endif
            default: return false;
        }
    }
    
    void logAvailableDatabases() {
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "사용 가능한 데이터베이스:");
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "  SQLite (기본)");
#ifdef HAS_POSTGRESQL
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "  PostgreSQL");
#endif
#ifdef HAS_MYSQL
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "  MySQL/MariaDB");
#endif
#if HAS_MSSQL
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "  MSSQL (Windows ODBC)");
#else
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "  MSSQL (비활성화됨)");
#endif
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "  Redis (항상 지원)");
#ifdef HAS_INFLUX
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "  InfluxDB");
#endif
    }
};

#endif // DATABASE_MANAGER_H