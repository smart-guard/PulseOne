// DatabaseManager.h
#pragma once

#include <string>
#include <pqxx/pqxx>
#include <sqlite3.h>
#include <memory>
#include "Utils/LogManager.h"

// DatabaseManager 클래스는 PostgreSQL 및 SQLite 연결을 관리하고
// 쿼리 실행 시 로깅과 에러 처리를 수행합니다.
class DatabaseManager {
public:
    // 싱글턴 인스턴스 반환
    static DatabaseManager& getInstance();

    // 초기화 (모든 DB 연결 시도)
    bool initialize();

    // 연결 상태 확인
    bool isPostgresConnected();
    bool isSQLiteConnected();

    // PostgreSQL 쿼리 실행 (SELECT)
    pqxx::result executeQueryPostgres(const std::string& query);

    // PostgreSQL 쿼리 실행 (INSERT/UPDATE/DELETE)
    bool executeNonQueryPostgres(const std::string& query);

    // SQLite 쿼리 실행 (SELECT)
    bool executeQuerySQLite(const std::string& query,
                            int (*callback)(void*, int, char**, char**),
                            void* data = nullptr);

    // SQLite 쿼리 실행 (INSERT/UPDATE/DELETE)
    bool executeNonQuerySQLite(const std::string& query);

private:
    DatabaseManager();
    ~DatabaseManager();

    // 내부 연결 함수
    bool connectPostgres();
    bool connectSQLite();

    // PostgreSQL 연결 객체
    std::unique_ptr<pqxx::connection> pg_conn;

    // SQLite 연결 객체
    sqlite3* sqlite_conn = nullptr;

    // 재접속 최대 시도 횟수
    const int MAX_RETRIES = 3;
};