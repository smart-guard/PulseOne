// DatabaseManager.h - 모든 데이터베이스 지원 완전판
#pragma once
#include <string>
#include <memory>
#include <map>
#include <vector>

// RDB 관련 헤더들
#include <pqxx/pqxx>           // PostgreSQL
#include <sqlite3.h>           // SQLite
#include <mysql/mysql.h>       // MySQL/MariaDB
#ifdef _WIN32
#include <sql.h>               // MSSQL (Windows ODBC)
#include <sqlext.h>
#endif

// NoSQL/시계열 DB 관련
#include "Client/RedisClient.h"
#include "Client/InfluxClient.h"
#include "Utils/LogManager.h"

/**
 * @brief 통합 멀티 데이터베이스 매니저
 * 지원 DB: PostgreSQL, SQLite, MySQL/MariaDB, MSSQL, Redis, InfluxDB
 */
class DatabaseManager {
public:
    // =======================================================================
    // 데이터베이스 타입 열거형
    // =======================================================================
    enum class DatabaseType {
        POSTGRESQL,
        SQLITE,
        MYSQL,      // MySQL/MariaDB
        MSSQL,      // SQL Server
        REDIS,      // Redis Cache
        INFLUXDB    // InfluxDB 시계열
    };
    
    // =======================================================================
    // 싱글턴 패턴
    // =======================================================================
    static DatabaseManager& getInstance();
    
    // =======================================================================
    // 초기화 및 설정
    // =======================================================================
    
    /**
     * @brief 전체 데이터베이스 매니저 초기화
     * database.env에서 활성화된 DB들만 연결
     */
    bool initialize();
    
    /**
     * @brief 특정 DB만 활성화 설정
     * @param db_type 활성화할 DB 타입
     * @param enabled 활성화 여부
     */
    void setDatabaseEnabled(DatabaseType db_type, bool enabled);
    
    // =======================================================================
    // 연결 상태 확인
    // =======================================================================
    bool isConnected(DatabaseType db_type);
    bool isPostgresConnected();
    bool isSQLiteConnected();
    bool isMySQLConnected();     // 🔧 추가
    bool isMSSQLConnected();     // 🔧 추가
    bool isRedisConnected();
    bool isInfluxConnected();
    
    // =======================================================================
    // PostgreSQL 관련
    // =======================================================================
    pqxx::result executeQueryPostgres(const std::string& query);
    bool executeNonQueryPostgres(const std::string& query);
    pqxx::connection* getPostgresConnection() { return pg_conn_.get(); }
    
    // =======================================================================
    // SQLite 관련
    // =======================================================================
    bool executeQuerySQLite(const std::string& query,
                           int (*callback)(void*, int, char**, char**),
                           void* data = nullptr);
    bool executeNonQuerySQLite(const std::string& query);
    sqlite3* getSQLiteConnection() { return sqlite_conn_; }
    
    // =======================================================================
    // 🔧 MySQL/MariaDB 관련 (새로 추가)
    // =======================================================================
    bool executeQueryMySQL(const std::string& query, std::vector<std::vector<std::string>>& results);
    bool executeNonQueryMySQL(const std::string& query);
    MYSQL* getMySQLConnection() { return mysql_conn_; }
    
    // =======================================================================
    // 🔧 MSSQL 관련 (새로 추가)
    // =======================================================================
#ifdef _WIN32
    bool executeQueryMSSQL(const std::string& query, std::vector<std::vector<std::string>>& results);
    bool executeNonQueryMSSQL(const std::string& query);
    SQLHDBC getMSSQLConnection() { return mssql_conn_; }
#endif
    
    // =======================================================================
    // Redis 관련
    // =======================================================================
    RedisClient* getRedisClient() { return redis_client_.get(); }
    bool connectRedis();
    void disconnectRedis();
    
    // =======================================================================
    // InfluxDB 관련
    // =======================================================================
    InfluxClient* getInfluxClient() { return influx_client_.get(); }
    bool connectInflux();
    void disconnectInflux();
    
    // =======================================================================
    // 통합 쿼리 인터페이스 (선택사항)
    // =======================================================================
    
    /**
     * @brief 설정된 메인 RDB에 쿼리 실행
     * database.env의 PRIMARY_DB 설정에 따라 자동 라우팅
     */
    bool executeQuery(const std::string& query, std::vector<std::vector<std::string>>& results);
    bool executeNonQuery(const std::string& query);
    
    /**
     * @brief 활성화된 모든 DB 연결 상태 조회
     */
    std::map<std::string, bool> getAllConnectionStatus();
    
    // =======================================================================
    // 정리 및 종료
    // =======================================================================
    void disconnectAll();

private:
    DatabaseManager();
    ~DatabaseManager();
    
    // =======================================================================
    // 개별 DB 연결 함수들
    // =======================================================================
    bool connectPostgres();
    bool connectSQLite();
    bool connectMySQL();        // 🔧 추가
    bool connectMSSQL();        // 🔧 추가
    
    // =======================================================================
    // 연결 객체들
    // =======================================================================
    
    // PostgreSQL
    std::unique_ptr<pqxx::connection> pg_conn_;
    
    // SQLite
    sqlite3* sqlite_conn_ = nullptr;
    
    // 🔧 MySQL/MariaDB (새로 추가)
    MYSQL* mysql_conn_ = nullptr;
    
    // 🔧 MSSQL (새로 추가)
#ifdef _WIN32
    SQLHENV mssql_env_ = nullptr;   // Environment handle
    SQLHDBC mssql_conn_ = nullptr;  // Connection handle
#endif
    
    // Redis
    std::unique_ptr<RedisClient> redis_client_;
    
    // InfluxDB
    std::unique_ptr<InfluxClient> influx_client_;
    
    // =======================================================================
    // 설정 및 상태 관리
    // =======================================================================
    
    // 활성화된 DB 타입들
    std::map<DatabaseType, bool> enabled_databases_;
    
    // 메인 RDB 타입 (database.env에서 설정)
    DatabaseType primary_rdb_ = DatabaseType::POSTGRESQL;
    
    // 재접속 최대 시도 횟수
    const int MAX_RETRIES = 3;
    
    // =======================================================================
    // 헬퍼 함수들
    // =======================================================================
    
    /**
     * @brief database.env에서 설정 로드
     */
    void loadDatabaseConfig();
    
    /**
     * @brief DB 타입을 문자열로 변환
     */
    std::string getDatabaseTypeName(DatabaseType type);
    
    /**
     * @brief 연결 문자열 생성
     */
    std::string buildConnectionString(DatabaseType type);
};