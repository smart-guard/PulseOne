// =============================================================================
// DatabaseManager.h - 크로스플랫폼 버전 (Windows/Linux 조건부 컴파일)
// =============================================================================
#ifndef DATABASE_ABSTRACTION_LAYER_H
#define DATABASE_ABSTRACTION_LAYER_H

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <mutex>
#include <atomic>

// ========================================================================
// 데이터베이스 드라이버 지원 감지 (조건부 컴파일)
// ========================================================================

// SQLite는 항상 지원 (헤더 온리 또는 정적 라이브러리)
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

// MSSQL은 Windows ODBC를 통해서만 지원
#ifdef _WIN32
    #define HAS_MSSQL 1
    #include <sql.h>
    #include <sqlext.h>
#endif

// Redis 클라이언트 (옵션)
#if __has_include("Client/RedisClientImpl.h")
    #define HAS_REDIS 1
    #include "Client/RedisClientImpl.h"
#else
    // Redis 미지원 시 더미 클래스
    class RedisClientImpl {
    public:
        bool connect(const std::string&, int, const std::string&) { return false; }
        void disconnect() {}
    };
    using RedisClient = RedisClientImpl;
#endif

// InfluxDB 클라이언트 (옵션)
#if __has_include("Client/InfluxClient.h")
    #define HAS_INFLUX 1
    #include "Client/InfluxClient.h"
#else
    // InfluxDB 미지원 시 더미 클래스
    class InfluxClient {
    public:
        bool connect() { return false; }
        void disconnect() {}
    };
#endif

#include "Utils/LogManager.h"

// ========================================================================
// DatabaseManager 클래스 정의
// ========================================================================

/**
 * @brief 크로스플랫폼 멀티 데이터베이스 매니저
 * 
 * 지원 DB:
 * - SQLite (모든 플랫폼)
 * - PostgreSQL (Linux/Unix)
 * - MySQL/MariaDB (Linux/Unix)
 * - MSSQL (Windows)
 * - Redis (옵션)
 * - InfluxDB (옵션)
 */
class DatabaseManager {
public:
    // ====================================================================
    // 데이터베이스 타입
    // ====================================================================
    enum class DatabaseType {
        SQLITE,      // 모든 플랫폼
        POSTGRESQL,  // Linux/Unix
        MYSQL,       // Linux/Unix
        MSSQL,       // Windows
        REDIS,       // 옵션
        INFLUXDB     // 옵션
    };
    
    // ====================================================================
    // 싱글톤 패턴
    // ====================================================================
    static DatabaseManager& getInstance();
    
    // ====================================================================
    // 초기화 및 설정
    // ====================================================================
    bool initialize();
    void reinitialize();
    void setDatabaseEnabled(DatabaseType db_type, bool enabled);
    
    // ====================================================================
    // 연결 상태 확인
    // ====================================================================
    bool isConnected(DatabaseType db_type);
    bool isSQLiteConnected();
    
#ifdef HAS_POSTGRESQL
    bool isPostgresConnected();
#endif

#ifdef HAS_MYSQL
    bool isMySQLConnected();
#endif

#ifdef HAS_MSSQL
    bool isMSSQLConnected();
#endif

#ifdef HAS_REDIS
    bool isRedisConnected();
#endif

#ifdef HAS_INFLUX
    bool isInfluxConnected();
#endif
    
    // ====================================================================
    // SQLite 관련 (항상 지원)
    // ====================================================================
    bool executeQuerySQLite(const std::string& query,
                           int (*callback)(void*, int, char**, char**),
                           void* data = nullptr);
    bool executeNonQuerySQLite(const std::string& query);
    sqlite3* getSQLiteConnection() { return sqlite_conn_; }
    
    // ====================================================================
    // PostgreSQL 관련 (조건부)
    // ====================================================================
#ifdef HAS_POSTGRESQL
    pqxx::result executeQueryPostgres(const std::string& query);
    bool executeNonQueryPostgres(const std::string& query);
    pqxx::connection* getPostgresConnection() { return pg_conn_.get(); }
#endif
    
    // ====================================================================
    // MySQL/MariaDB 관련 (조건부)
    // ====================================================================
#ifdef HAS_MYSQL
    bool executeQueryMySQL(const std::string& query, 
                          std::vector<std::vector<std::string>>& results);
    bool executeNonQueryMySQL(const std::string& query);
    MYSQL* getMySQLConnection() { return mysql_conn_; }
#endif
    
    // ====================================================================
    // MSSQL 관련 (Windows 전용)
    // ====================================================================
#ifdef HAS_MSSQL
    bool executeQueryMSSQL(const std::string& query, 
                          std::vector<std::vector<std::string>>& results);
    bool executeNonQueryMSSQL(const std::string& query);
    SQLHDBC getMSSQLConnection() { return mssql_conn_; }
#endif
    
    // ====================================================================
    // Redis 관련 (옵션)
    // ====================================================================
#ifdef HAS_REDIS
    RedisClient* getRedisClient() { return redis_client_.get(); }
    bool connectRedis();
    void disconnectRedis();
    bool testRedisConnection();
    std::map<std::string, std::string> getRedisInfo();
#endif
    
    // ====================================================================
    // InfluxDB 관련 (옵션)
    // ====================================================================
#ifdef HAS_INFLUX
    InfluxClient* getInfluxClient() { return influx_client_.get(); }
    bool connectInflux();
    void disconnectInflux();
#endif
    
    // ====================================================================
    // 통합 쿼리 인터페이스
    // ====================================================================
    bool executeQuery(const std::string& query, 
                     std::vector<std::vector<std::string>>& results);
    bool executeNonQuery(const std::string& query);
    
    std::map<std::string, bool> getAllConnectionStatus();
    
    // ====================================================================
    // 정리 및 종료
    // ====================================================================
    void disconnectAll();

private:
    // ====================================================================
    // 생성자/소멸자
    // ====================================================================
    DatabaseManager();
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    
    // ====================================================================
    // 자동 초기화
    // ====================================================================
    void ensureInitialized();
    bool doInitialize();
    
    static std::once_flag init_flag_;
    static std::atomic<bool> initialization_success_;
    
    // ====================================================================
    // 개별 DB 연결 함수
    // ====================================================================
    bool connectSQLite();
    
#ifdef HAS_POSTGRESQL
    bool connectPostgres();
#endif

#ifdef HAS_MYSQL
    bool connectMySQL();
#endif

#ifdef HAS_MSSQL
    bool connectMSSQL();
#endif
    
    // ====================================================================
    // 연결 객체들
    // ====================================================================
    
    // SQLite (항상 존재)
    sqlite3* sqlite_conn_ = nullptr;
    
#ifdef HAS_POSTGRESQL
    std::unique_ptr<pqxx::connection> pg_conn_;
#endif

#ifdef HAS_MYSQL
    MYSQL* mysql_conn_ = nullptr;
#endif

#ifdef HAS_MSSQL
    SQLHENV mssql_env_ = nullptr;
    SQLHDBC mssql_conn_ = nullptr;
#endif

#ifdef HAS_REDIS
    std::unique_ptr<RedisClientImpl> redis_client_;
#endif

#ifdef HAS_INFLUX
    std::unique_ptr<InfluxClient> influx_client_;
#endif
    
    // ====================================================================
    // 설정 및 상태 관리
    // ====================================================================
    std::map<DatabaseType, bool> enabled_databases_;
    DatabaseType primary_rdb_ = DatabaseType::SQLITE;
    const int MAX_RETRIES = 3;
    mutable std::mutex db_mutex_;
    
    // ====================================================================
    // 헬퍼 함수
    // ====================================================================
    void loadDatabaseConfig();
    std::string getDatabaseTypeName(DatabaseType type);
    std::string buildConnectionString(DatabaseType type);
    
    // 지원되는 DB 타입 확인
    bool isDatabaseTypeSupported(DatabaseType type) const {
        switch (type) {
            case DatabaseType::SQLITE:
                return true;  // 항상 지원
                
#ifdef HAS_POSTGRESQL
            case DatabaseType::POSTGRESQL:
                return true;
#endif

#ifdef HAS_MYSQL
            case DatabaseType::MYSQL:
                return true;
#endif

#ifdef HAS_MSSQL
            case DatabaseType::MSSQL:
                return true;
#endif

#ifdef HAS_REDIS
            case DatabaseType::REDIS:
                return true;
#endif

#ifdef HAS_INFLUX
            case DatabaseType::INFLUXDB:
                return true;
#endif
            default:
                return false;
        }
    }
    
    // 플랫폼별 가용 DB 목록 로깅
    void logAvailableDatabases() {
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "🔍 사용 가능한 데이터베이스:");
        
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "  ✅ SQLite (기본)");
            
#ifdef HAS_POSTGRESQL
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "  ✅ PostgreSQL");
#endif

#ifdef HAS_MYSQL
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "  ✅ MySQL/MariaDB");
#endif

#ifdef HAS_MSSQL
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "  ✅ MSSQL (Windows ODBC)");
#endif

#ifdef HAS_REDIS
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "  ✅ Redis");
#endif

#ifdef HAS_INFLUX
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "  ✅ InfluxDB");
#endif

#ifdef PULSEONE_WINDOWS
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "  📌 Windows 빌드 - SQLite 우선 사용");
#else
        LogManager::getInstance().log("database", LogLevel::INFO, 
            "  📌 Linux 빌드 - 모든 DB 지원 가능");
#endif
    }
};

#endif // DATABASEMANAGER_H