// DatabaseManager.cpp
#include "Database/DatabaseManager.h"
#include <cstdlib>
#include <thread>
#include <chrono>


DatabaseManager::DatabaseManager() {}
DatabaseManager::~DatabaseManager() {
    if (sqlite_conn) {
        sqlite3_close(sqlite_conn);
    }
    if (pg_conn && pg_conn->is_open()) {
        pg_conn->disconnect();
    }
}

DatabaseManager& DatabaseManager::getInstance() {
    static DatabaseManager instance;
    return instance;
}

bool DatabaseManager::initialize() {
    bool pg_ok = connectPostgres();
    bool sqlite_ok = connectSQLite();
    return pg_ok && sqlite_ok;
}

bool DatabaseManager::connectPostgres() {
    for (int i = 0; i < MAX_RETRIES; ++i) {
        try {
            std::string connStr =
                "host=" + std::string(std::getenv("POSTGRES_MAIN_DB_HOST")) +
                " port=" + std::string(std::getenv("POSTGRES_MAIN_DB_PORT")) +
                " dbname=" + std::string(std::getenv("POSTGRES_MAIN_DB_NAME")) +
                " user=" + std::string(std::getenv("POSTGRES_MAIN_DB_USER")) +
                " password=" + std::string(std::getenv("POSTGRES_MAIN_DB_PASSWORD"));

            pg_conn = std::make_unique<pqxx::connection>(connStr);

            if (pg_conn->is_open()) {
                PulseOne::LogManager::getInstance().log(PulseOne::Constants::LOG_MODULE_DATABASE, PulseOne::LogLevel::INFO, "PostgreSQL 연결 성공");
                return true;
            }
        } catch (const std::exception& e) {
            PulseOne::LogManager::getInstance().log(PulseOne::Constants::LOG_MODULE_DATABASE, PulseOne::LogLevel::ERROR, std::string("PostgreSQL 연결 실패: ") + e.what());
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return false;
}

bool DatabaseManager::connectSQLite() {
    const char* db_path = std::getenv("SQLITE_PATH");
    if (!db_path) db_path = "./config/db.sqlite";
    int rc = sqlite3_open(db_path, &sqlite_conn);
    if (rc != SQLITE_OK) {
        PulseOne::LogManager::getInstance().log(PulseOne::Constants::LOG_MODULE_DATABASE, PulseOne::LogLevel::ERROR, "SQLite 연결 실패: " + std::string(sqlite3_errmsg(sqlite_conn)));
        return false;
    }
    PulseOne::LogManager::getInstance().log(PulseOne::Constants::LOG_MODULE_DATABASE, PulseOne::LogLevel::INFO, "SQLite 연결 성공");
    return true;
}

bool DatabaseManager::isPostgresConnected() {
    return pg_conn && pg_conn->is_open();
}

bool DatabaseManager::isSQLiteConnected() {
    return sqlite_conn != nullptr;
}

pqxx::result DatabaseManager::executeQueryPostgres(const std::string& query) {
    try {
        pqxx::work txn(*pg_conn);
        pqxx::result r = txn.exec(query);
        txn.commit();
        PulseOne::LogManager::getInstance().log(PulseOne::Constants::LOG_MODULE_DATABASE, PulseOne::LogLevel::INFO, "PostgreSQL 쿼리 성공: " + query);
        return r;
    } catch (const std::exception& e) {
        PulseOne::LogManager::getInstance().log(PulseOne::Constants::LOG_MODULE_DATABASE, PulseOne::LogLevel::ERROR, std::string("PostgreSQL 쿼리 실패: ") + e.what());
        throw;
    }
}

bool DatabaseManager::executeNonQueryPostgres(const std::string& query) {
    try {
        pqxx::work txn(*pg_conn);
        txn.exec(query);
        txn.commit();
        PulseOne::LogManager::getInstance().log(PulseOne::Constants::LOG_MODULE_DATABASE, PulseOne::LogLevel::INFO, "PostgreSQL non-query 성공: " + query);
        return true;
    } catch (const std::exception& e) {
        PulseOne::LogManager::getInstance().log(PulseOne::Constants::LOG_MODULE_DATABASE, PulseOne::LogLevel::ERROR, std::string("PostgreSQL non-query 실패: ") + e.what());
        return false;
    }
}

bool DatabaseManager::executeQuerySQLite(const std::string& query,
                                         int (*callback)(void*, int, char**, char**),
                                         void* data) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(sqlite_conn, query.c_str(), callback, data, &errMsg);
    if (rc != SQLITE_OK) {
        PulseOne::LogManager::getInstance().log(PulseOne::Constants::LOG_MODULE_DATABASE, PulseOne::LogLevel::ERROR, "SQLite 쿼리 실패: " + std::string(errMsg));
        sqlite3_free(errMsg);
        return false;
    }
    PulseOne::LogManager::getInstance().log(PulseOne::Constants::LOG_MODULE_DATABASE, PulseOne::LogLevel::INFO, "SQLite 쿼리 성공: " + query);
    return true;
}

bool DatabaseManager::executeNonQuerySQLite(const std::string& query) {
    return executeQuerySQLite(query, nullptr);
}