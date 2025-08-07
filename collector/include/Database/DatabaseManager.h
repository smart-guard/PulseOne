// =============================================================================
// DatabaseManager.h - 자동 초기화 적용 완전판
// 🚀 이제 initialize() 호출 없이 바로 사용 가능!
// =============================================================================
#pragma once
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <mutex>
#include <atomic>

// RDB 관련 헤더들
#include <pqxx/pqxx>           // PostgreSQL
#include <sqlite3.h>           // SQLite
#include <mysql/mysql.h>       // MySQL/MariaDB
#ifdef _WIN32
#include <sql.h>               // MSSQL (Windows ODBC)
#include <sqlext.h>
#endif

// NoSQL/시계열 DB 관련
#include "Client/RedisClientImpl.h"
#include "Client/InfluxClient.h"
#include "Utils/LogManager.h"

/**
 * @brief 🚀 자동 초기화 통합 멀티 데이터베이스 매니저
 * 
 * 🔥 주요 개선사항:
 * - 자동 초기화: 첫 호출 시 자동으로 initialize() 실행
 * - 어디서든 바로 사용: getInstance()만 호출하면 즉시 사용 가능
 * - 테스트 코드 간소화: 초기화 관련 코드 제거
 * - 실수 방지: 초기화 깜빡함 완전 방지
 * 
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
    // 🚀 자동 초기화 싱글톤 (핵심 개선!)
    // =======================================================================
    
    /**
     * @brief 싱글톤 인스턴스 반환 + 자동 초기화
     * 
     * 🔧 변경사항:
     * - 첫 호출 시 자동으로 initialize() 실행
     * - std::once_flag로 중복 초기화 방지
     * - 어디서든 바로 사용 가능
     * 
     * @return DatabaseManager 인스턴스 (완전히 초기화됨)
     */
    static DatabaseManager& getInstance();
    
    // =======================================================================
    // 초기화 및 설정 (옵션)
    // =======================================================================
    
    /**
     * @brief 수동 초기화 (옵션 - 자동 초기화 때문에 일반적으로 불필요)
     * database.env에서 활성화된 DB들만 연결
     * 
     * @return 초기화 성공 여부
     */
    bool initialize();
    
    /**
     * @brief 강제 재초기화
     * 설정이 변경되었을 때 사용
     */
    void reinitialize();
    
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
    bool isMySQLConnected();
    bool isMSSQLConnected();
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
    // MySQL/MariaDB 관련
    // =======================================================================
    bool executeQueryMySQL(const std::string& query, std::vector<std::vector<std::string>>& results);
    bool executeNonQueryMySQL(const std::string& query);
    MYSQL* getMySQLConnection() { return mysql_conn_; }
    
    // =======================================================================
    // MSSQL 관련
    // =======================================================================
#ifdef _WIN32
    bool executeQueryMSSQL(const std::string& query, std::vector<std::vector<std::string>>& results);
    bool executeNonQueryMSSQL(const std::string& query);
    SQLHDBC getMSSQLConnection() { return mssql_conn_; }
#endif
    
    // =======================================================================
    // Redis 관련
    // =======================================================================
    /**
     * @brief Redis 클라이언트 인스턴스 반환
     * @return RedisClient 포인터 (hiredis 기반 구현체)
     */
    RedisClient* getRedisClient() { return redis_client_.get(); }
    bool connectRedis();
    void disconnectRedis();
    bool testRedisConnection();
    std::map<std::string, std::string> getRedisInfo();
    
    // =======================================================================
    // InfluxDB 관련
    // =======================================================================
    InfluxClient* getInfluxClient() { return influx_client_.get(); }
    bool connectInflux();
    void disconnectInflux();
    
    // =======================================================================
    // 통합 쿼리 인터페이스
    // =======================================================================
    
    /**
     * @brief 설정된 메인 RDB에 쿼리 실행
     * database.env의 DATABASE_TYPE 설정에 따라 자동 라우팅
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
    // =======================================================================
    // 🚀 자동 초기화 구현
    // =======================================================================
    DatabaseManager();
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    
    /**
     * @brief 실제 초기화 작업 수행
     * @return 초기화 성공 여부
     */
    bool doInitialize();
    
    // 자동 초기화 상태 관리
    static std::once_flag init_flag_;
    static std::atomic<bool> initialization_success_;
    
    // =======================================================================
    // 개별 DB 연결 함수들
    // =======================================================================
    bool connectPostgres();
    bool connectSQLite();
    bool connectMySQL();
    bool connectMSSQL();
    
    // =======================================================================
    // 연결 객체들
    // =======================================================================
    
    // PostgreSQL
    std::unique_ptr<pqxx::connection> pg_conn_;
    
    // SQLite
    sqlite3* sqlite_conn_ = nullptr;
    
    // MySQL/MariaDB
    MYSQL* mysql_conn_ = nullptr;
    
    // MSSQL
#ifdef _WIN32
    SQLHENV mssql_env_ = nullptr;   // Environment handle
    SQLHDBC mssql_conn_ = nullptr;  // Connection handle
#endif
    
    // Redis
    std::unique_ptr<RedisClientImpl> redis_client_;
    
    // InfluxDB
    std::unique_ptr<InfluxClient> influx_client_;
    
    // =======================================================================
    // 설정 및 상태 관리
    // =======================================================================
    
    // 활성화된 DB 타입들
    std::map<DatabaseType, bool> enabled_databases_;
    
    // 메인 RDB 타입 (database.env에서 설정)
    DatabaseType primary_rdb_ = DatabaseType::SQLITE;
    
    // 재접속 최대 시도 횟수
    const int MAX_RETRIES = 3;
    
    // 스레드 안전성을 위한 뮤텍스
    mutable std::mutex db_mutex_;
    
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