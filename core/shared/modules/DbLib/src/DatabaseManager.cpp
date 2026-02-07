#include "DatabaseManager.hpp"
#include <algorithm>
#include <chrono>
#include <thread>

namespace DbLib {

// Static member initialization
std::once_flag DatabaseManager::init_flag_;
std::atomic<bool> DatabaseManager::initialization_success_(false);

DatabaseManager::DatabaseManager() {
  enabled_databases_[DatabaseType::SQLITE] = true;
  primary_rdb_ = DatabaseType::SQLITE;
}

DatabaseManager::~DatabaseManager() { disconnectAll(); }

DatabaseManager &DatabaseManager::getInstance() {
  static DatabaseManager instance;
  return instance;
}

bool DatabaseManager::initialize(const DatabaseConfig &config,
                                 IDbLogger *logger) {
  current_config_ = config;
  logger_ = logger;
  return doInitialize();
}

bool DatabaseManager::doInitialize() {
  std::lock_guard<std::mutex> lock(db_mutex_);

  log(1, "DatabaseManager initialization started...");

  // Determine primary DB type from config
  std::string db_type = current_config_.type;
  std::transform(db_type.begin(), db_type.end(), db_type.begin(), ::toupper);

  // Reset enabled databases
  for (auto &[type, enabled] : enabled_databases_) {
    enabled = false;
  }

  if (db_type == "POSTGRESQL") {
    enabled_databases_[DatabaseType::POSTGRESQL] = true;
    primary_rdb_ = DatabaseType::POSTGRESQL;
  } else if (db_type == "MYSQL") {
    enabled_databases_[DatabaseType::MYSQL] = true;
    primary_rdb_ = DatabaseType::MYSQL;
  } else if (db_type == "MSSQL") {
    enabled_databases_[DatabaseType::MSSQL] = true;
    primary_rdb_ = DatabaseType::MSSQL;
  } else {
    enabled_databases_[DatabaseType::SQLITE] = true;
    primary_rdb_ = DatabaseType::SQLITE;
  }

  enabled_databases_[DatabaseType::REDIS] = current_config_.use_redis;
  enabled_databases_[DatabaseType::INFLUXDB] = current_config_.use_influx;

  // Connect SQLite if enabled
  if (enabled_databases_[DatabaseType::SQLITE]) {
    if (!connectSQLite()) {
      log(3, "Failed to connect to SQLite");
      return false;
    }
  }

#ifdef HAS_POSTGRESQL
  if (enabled_databases_[DatabaseType::POSTGRESQL]) {
    if (!connectPostgres())
      log(2, "Failed to connect to PostgreSQL");
  }
#endif

#ifdef HAS_MYSQL
  if (enabled_databases_[DatabaseType::MYSQL]) {
    if (!connectMySQL())
      log(2, "Failed to connect to MySQL");
  }
#endif

#if HAS_MSSQL
  if (enabled_databases_[DatabaseType::MSSQL]) {
    if (!connectMSSQL())
      log(2, "Failed to connect to MSSQL");
  }
#endif

  if (enabled_databases_[DatabaseType::REDIS]) {
    connectRedis();
  }

  return true;
}

// ========================================================================
// SQLite Implementation
// ========================================================================
bool DatabaseManager::connectSQLite() {
  std::string db_path = current_config_.sqlite_path;
  log(1, "Attempting SQLite connection: " + db_path);

  int result = sqlite3_open(db_path.c_str(), &sqlite_conn_);
  if (result == SQLITE_OK) {
    log(1, "SQLite connection successful: " + db_path);

    // 🔥 v3.2 - Concurrency Stabilization
    // 1. Set busy timeout (wait for locks up to 5 seconds)
    sqlite3_busy_timeout(sqlite_conn_, 5000);

    // 2. Enable WAL mode for better multi-process concurrency
    char *err_wal = nullptr;
    sqlite3_exec(sqlite_conn_, "PRAGMA journal_mode=WAL;", nullptr, nullptr,
                 &err_wal);
    if (err_wal) {
      log(2, "Failed to set SQLite WAL mode: " + std::string(err_wal));
      sqlite3_free(err_wal);
    }

    // 3. Set synchronous mode to NORMAL (safe with WAL)
    sqlite3_exec(sqlite_conn_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr,
                 nullptr);

    return true;
  }

  std::string error_msg = sqlite3_errmsg(sqlite_conn_);
  log(3, "SQLite connection failed: " + error_msg);

  if (sqlite_conn_) {
    sqlite3_close(sqlite_conn_);
    sqlite_conn_ = nullptr;
  }
  return false;
}

bool DatabaseManager::executeQuerySQLite(const std::string &sql,
                                         int (*callback)(void *, int, char **,
                                                         char **),
                                         void *data) {
  if (!sqlite_conn_)
    return false;

  char *error_msg = nullptr;
  int result =
      sqlite3_exec(sqlite_conn_, sql.c_str(), callback, data, &error_msg);

  if (result != SQLITE_OK) {
    std::string error_str =
        error_msg ? std::string(error_msg) : "Unknown SQLite error";
    log(3, "SQLite error: " + error_str + " in query: " + sql.substr(0, 100));
    if (error_msg)
      sqlite3_free(error_msg);
    return false;
  }
  return true;
}

bool DatabaseManager::executeNonQuerySQLite(const std::string &sql) {
  return executeQuerySQLite(sql, nullptr, nullptr);
}

bool DatabaseManager::isSQLiteConnected() { return sqlite_conn_ != nullptr; }

// ========================================================================
// PostgreSQL Implementation
// ========================================================================
#ifdef HAS_POSTGRESQL
bool DatabaseManager::connectPostgres() {
  try {
    std::string connStr = buildConnectionString(DatabaseType::POSTGRESQL);
    pg_conn_ = std::make_unique<pqxx::connection>(connStr);
    return pg_conn_->is_open();
  } catch (const std::exception &e) {
    log(3, "PostgreSQL connection failed: " + std::string(e.what()));
    return false;
  }
}

bool DatabaseManager::isPostgresConnected() {
  return pg_conn_ && pg_conn_->is_open();
}

pqxx::result DatabaseManager::executeQueryPostgres(const std::string &sql) {
  if (!pg_conn_)
    throw std::runtime_error("PostgreSQL not connected");
  pqxx::work txn(*pg_conn_);
  pqxx::result result = txn.exec(sql);
  txn.commit();
  return result;
}

bool DatabaseManager::executeNonQueryPostgres(const std::string &sql) {
  try {
    executeQueryPostgres(sql);
    return true;
  } catch (...) {
    return false;
  }
}
#endif

// ========================================================================
// MySQL Implementation
// ========================================================================
#ifdef HAS_MYSQL
bool DatabaseManager::connectMySQL() {
  mysql_conn_ = mysql_init(nullptr);
  if (!mysql_conn_)
    return false;

  const char *host = current_config_.mysql_host.c_str();
  const char *user = current_config_.mysql_user.c_str();
  const char *pass = current_config_.mysql_pass.c_str();
  const char *db = current_config_.mysql_db.c_str();
  int port = current_config_.mysql_port;

  if (mysql_real_connect(mysql_conn_, host, user, pass, db, port, nullptr, 0)) {
    return true;
  }

  log(3, "MySQL connection failed: " + std::string(mysql_error(mysql_conn_)));
  mysql_close(mysql_conn_);
  mysql_conn_ = nullptr;
  return false;
}

bool DatabaseManager::isMySQLConnected() { return mysql_conn_ != nullptr; }

bool DatabaseManager::executeQueryMySQL(
    const std::string &query, std::vector<std::vector<std::string>> &results) {
  if (!mysql_conn_)
    return false;

  if (mysql_query(mysql_conn_, query.c_str()) != 0) {
    log(3, "MySQL query failed: " + std::string(mysql_error(mysql_conn_)));
    return false;
  }

  MYSQL_RES *result = mysql_store_result(mysql_conn_);
  if (!result)
    return true;

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

bool DatabaseManager::executeNonQueryMySQL(const std::string &query) {
  std::vector<std::vector<std::string>> dummy;
  return executeQueryMySQL(query, dummy);
}
#endif

// ========================================================================
// MSSQL Implementation
// ========================================================================
#if HAS_MSSQL
bool DatabaseManager::connectMSSQL() {
  if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &mssql_env_) !=
      SQL_SUCCESS)
    return false;
  SQLSetEnvAttr(mssql_env_, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
  if (SQLAllocHandle(SQL_HANDLE_DBC, mssql_env_, &mssql_conn_) != SQL_SUCCESS) {
    SQLFreeHandle(SQL_HANDLE_ENV, mssql_env_);
    return false;
  }

  std::string conn_str = buildConnectionString(DatabaseType::MSSQL);
  SQLCHAR out_conn_str[1024];
  SQLSMALLINT out_conn_str_len;

  SQLRETURN ret = SQLDriverConnect(
      mssql_conn_, nullptr, (SQLCHAR *)conn_str.c_str(), SQL_NTS, out_conn_str,
      sizeof(out_conn_str), &out_conn_str_len, SQL_DRIVER_NOPROMPT);

  if (SQL_SUCCEEDED(ret))
    return true;

  log(3, "MSSQL connection failed");
  SQLFreeHandle(SQL_HANDLE_DBC, mssql_conn_);
  SQLFreeHandle(SQL_HANDLE_ENV, mssql_env_);
  mssql_conn_ = nullptr;
  mssql_env_ = nullptr;
  return false;
}

bool DatabaseManager::isMSSQLConnected() { return mssql_conn_ != nullptr; }

bool DatabaseManager::executeQueryMSSQL(
    const std::string &query, std::vector<std::vector<std::string>> &results) {
  if (!mssql_conn_)
    return false;
  SQLHSTMT stmt;
  if (SQLAllocHandle(SQL_HANDLE_STMT, mssql_conn_, &stmt) != SQL_SUCCESS)
    return false;

  SQLRETURN ret = SQLExecDirect(stmt, (SQLCHAR *)query.c_str(), SQL_NTS);
  if (!SQL_SUCCEEDED(ret)) {
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return false;
  }

  SQLSMALLINT col_count;
  SQLNumResultCols(stmt, &col_count);
  while (SQLFetch(stmt) == SQL_SUCCESS) {
    std::vector<std::string> row;
    for (int i = 1; i <= col_count; ++i) {
      SQLCHAR buffer[256];
      SQLLEN indicator;
      if (SQLGetData(stmt, i, SQL_C_CHAR, buffer, sizeof(buffer), &indicator) ==
          SQL_SUCCESS) {
        row.push_back(indicator == SQL_NULL_DATA
                          ? "NULL"
                          : reinterpret_cast<char *>(buffer));
      } else
        row.push_back("");
    }
    results.push_back(row);
  }
  SQLFreeHandle(SQL_HANDLE_STMT, stmt);
  return true;
}

bool DatabaseManager::executeNonQueryMSSQL(const std::string &query) {
  if (!mssql_conn_)
    return false;
  SQLHSTMT stmt;
  if (SQLAllocHandle(SQL_HANDLE_STMT, mssql_conn_, &stmt) != SQL_SUCCESS)
    return false;
  SQLRETURN ret = SQLExecDirect(stmt, (SQLCHAR *)query.c_str(), SQL_NTS);
  bool success = SQL_SUCCEEDED(ret);
  SQLFreeHandle(SQL_HANDLE_STMT, stmt);
  return success;
}

void *DatabaseManager::getMSSQLConnection() {
  return static_cast<void *>(mssql_conn_);
}
#else
bool DatabaseManager::connectMSSQL() { return false; }
bool DatabaseManager::isMSSQLConnected() { return false; }
bool DatabaseManager::executeQueryMSSQL(
    const std::string &, std::vector<std::vector<std::string>> &) {
  return false;
}
bool DatabaseManager::executeNonQueryMSSQL(const std::string &) {
  return false;
}
void *DatabaseManager::getMSSQLConnection() { return nullptr; }
#endif

// ========================================================================
// Redis Implementation
// ========================================================================
bool DatabaseManager::connectRedis() {
  try {
    redis_client_ = std::make_unique<RedisClientImpl>();
    return redis_client_->connect(current_config_.redis_host,
                                  current_config_.redis_port,
                                  current_config_.redis_pass);
  } catch (...) {
    return false;
  }
}

bool DatabaseManager::isRedisConnected() { return redis_client_ != nullptr; }

void DatabaseManager::disconnectRedis() {
  if (redis_client_) {
    redis_client_->disconnect();
    redis_client_.reset();
  }
}

bool DatabaseManager::testRedisConnection() { return isRedisConnected(); }

std::map<std::string, std::string> DatabaseManager::getRedisInfo() {
  return {};
}

// ========================================================================
// Integrated Query Interface
// ========================================================================
bool DatabaseManager::executeQuery(
    const std::string &query, std::vector<std::vector<std::string>> &results) {
  bool result = false;
  switch (primary_rdb_) {
  case DatabaseType::SQLITE:
    results.clear();
    result = executeQuerySQLite(
        query,
        [](void *data, int argc, char **argv, char **) -> int {
          auto *res =
              static_cast<std::vector<std::vector<std::string>> *>(data);
          std::vector<std::string> row;
          for (int i = 0; i < argc; i++)
            row.push_back(argv[i] ? argv[i] : "");
          res->push_back(row);
          return 0;
        },
        &results);
    break;
#ifdef HAS_POSTGRESQL
  case DatabaseType::POSTGRESQL: {
    try {
      auto pg_result = executeQueryPostgres(query);
      for (const auto &row : pg_result) {
        std::vector<std::string> row_data;
        for (const auto &field : row)
          row_data.push_back(field.c_str());
        results.push_back(row_data);
      }
      result = true;
    } catch (...) {
      result = false;
    }
    break;
  }
#endif
#ifdef HAS_MYSQL
  case DatabaseType::MYSQL:
    result = executeQueryMySQL(query, results);
    break;
#endif
  case DatabaseType::MSSQL:
    result = executeQueryMSSQL(query, results);
    break;
  default:
    result = false;
  }
  return result;
}

bool DatabaseManager::executeNonQuery(const std::string &query) {
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
  case DatabaseType::MSSQL:
    return executeNonQueryMSSQL(query);
  default:
    return false;
  }
}

// ========================================================================
// Utilities
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
#if HAS_MSSQL
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
  disconnectRedis();
}

std::string DatabaseManager::getDatabaseTypeName(DatabaseType type) {
  switch (type) {
  case DatabaseType::SQLITE:
    return "SQLite";
  case DatabaseType::POSTGRESQL:
    return "PostgreSQL";
  case DatabaseType::MYSQL:
    return "MySQL";
  case DatabaseType::MSSQL:
    return "MSSQL";
  case DatabaseType::REDIS:
    return "Redis";
  case DatabaseType::INFLUXDB:
    return "InfluxDB";
  default:
    return "Unknown";
  }
}

std::string DatabaseManager::buildConnectionString(DatabaseType type) {
  switch (type) {
  case DatabaseType::POSTGRESQL:
    return "host=" + current_config_.pg_host +
           " port=" + std::to_string(current_config_.pg_port) +
           " dbname=" + current_config_.pg_db +
           " user=" + current_config_.pg_user +
           " password=" + current_config_.pg_pass;
  case DatabaseType::MSSQL:
    return "Driver={ODBC Driver 17 for SQL Server};Server=" +
           current_config_.mysql_host +
           ";"; // Reusing mysql_host as generic server host
  default:
    return "";
  }
}

std::map<std::string, bool> DatabaseManager::getAllConnectionStatus() {
  std::map<std::string, bool> status;
  status["SQLite"] = isSQLiteConnected();
#ifdef HAS_POSTGRESQL
  status["PostgreSQL"] = isPostgresConnected();
#endif
#ifdef HAS_MYSQL
  status["MySQL"] = isMySQLConnected();
#endif
  status["MSSQL"] = isMSSQLConnected();
  status["Redis"] = isRedisConnected();
  return status;
}

bool DatabaseManager::isConnected(DatabaseType db_type) {
  switch (db_type) {
  case DatabaseType::SQLITE:
    return isSQLiteConnected();
#ifdef HAS_POSTGRESQL
  case DatabaseType::POSTGRESQL:
    return isPostgresConnected();
#endif
#ifdef HAS_MYSQL
  case DatabaseType::MYSQL:
    return isMySQLConnected();
#endif
  case DatabaseType::MSSQL:
    return isMSSQLConnected();
  case DatabaseType::REDIS:
    return isRedisConnected();
  default:
    return false;
  }
}

void DatabaseManager::reinitialize() {
  disconnectAll();
  doInitialize();
}

void DatabaseManager::setDatabaseEnabled(DatabaseType db_type, bool enabled) {
  enabled_databases_[db_type] = enabled;
}

void DatabaseManager::ensureInitialized() {
  std::call_once(init_flag_,
                 [this]() { initialization_success_.store(doInitialize()); });
}

void DatabaseManager::log(int level, const std::string &message) {
  if (logger_)
    logger_->log("DbLib", level, message);
}

} // namespace DbLib