#ifndef DBLIB_DATABASE_MANAGER_HPP
#define DBLIB_DATABASE_MANAGER_HPP

#include "DbExport.hpp"
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// ========================================================================
// DB Driver Detection
// ========================================================================

#define HAS_SQLITE 1
#include <sqlite3.h>

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#if __has_include(<pqxx/pqxx>)
#define HAS_POSTGRESQL 1
#include <pqxx/pqxx>
#endif
#if __has_include(<mysql/mysql.h>)
#define HAS_MYSQL 1
#include <mysql/mysql.h>
#endif
#endif

#if defined(_WIN32)
#define HAS_MSSQL 1
#include <sql.h>
#include <sqlext.h>
#else
#define HAS_MSSQL 0
#endif

// Dummy Redis Client if not present
#if __has_include("RedisClientImpl.h")
#include "RedisClientImpl.h"
#define HAS_REDIS 1
#else
namespace DbLib {
class RedisClientImpl {
public:
  bool connect(const std::string &, int, const std::string &) { return false; }
  void disconnect() {}
};
using RedisClient = RedisClientImpl;
} // namespace DbLib
#define HAS_REDIS 1
#endif

namespace DbLib {

/**
 * @brief Configuration for Database connections
 */
struct DBLIB_API DatabaseConfig {
  std::string type = "SQLITE";
  std::string sqlite_path = "pulseone.db";

  // Postgres
  std::string pg_host = "localhost";
  int pg_port = 5432;
  std::string pg_db = "pulseone";
  std::string pg_user = "pulseone";
  std::string pg_pass = "";

  // MySQL
  std::string mysql_host = "localhost";
  std::string mysql_user = "root";
  std::string mysql_pass = "";
  std::string mysql_db = "pulseone";
  int mysql_port = 3306;

  // Redis
  bool use_redis = false;
  std::string redis_host = "localhost";
  int redis_port = 6379;
  std::string redis_pass = "";

  // Influx
  bool use_influx = false;
  std::string influx_url = "http://localhost:8086";
  std::string influx_token = "";
  std::string influx_org = "pulseone";
  std::string influx_bucket = "history";
};

/**
 * @brief Logger interface for DbLib
 */
class DBLIB_API IDbLogger {
public:
  virtual ~IDbLogger() = default;
  virtual void log(const std::string &category, int level,
                   const std::string &message) = 0;
};

class DBLIB_API DatabaseManager {
public:
  enum class DatabaseType { SQLITE, POSTGRESQL, MYSQL, MSSQL, REDIS, INFLUXDB };

  static DatabaseManager &getInstance();

  bool initialize(const DatabaseConfig &config, IDbLogger *logger = nullptr);
  void reinitialize();
  void setDatabaseEnabled(DatabaseType db_type, bool enabled);

  const DatabaseConfig &getConfig() const { return current_config_; }

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

  bool executeQuerySQLite(const std::string &query,
                          int (*callback)(void *, int, char **, char **),
                          void *data = nullptr);
  bool executeNonQuerySQLite(const std::string &query);
  sqlite3 *getSQLiteConnection() { return sqlite_conn_; }

#ifdef HAS_POSTGRESQL
  pqxx::result executeQueryPostgres(const std::string &query);
  bool executeNonQueryPostgres(const std::string &query);
  pqxx::connection *getPostgresConnection() { return pg_conn_.get(); }
#endif

#ifdef HAS_MYSQL
  bool executeQueryMySQL(const std::string &query,
                         std::vector<std::vector<std::string>> &results);
  bool executeNonQueryMySQL(const std::string &query);
  MYSQL *getMySQLConnection() { return mysql_conn_; }
#endif

  bool executeQueryMSSQL(const std::string &query,
                         std::vector<std::vector<std::string>> &results);
  bool executeNonQueryMSSQL(const std::string &query);
  void *getMSSQLConnection();

  RedisClientImpl *getRedisClient() { return redis_client_.get(); }
  bool connectRedis();
  void disconnectRedis();
  bool testRedisConnection();
  std::map<std::string, std::string> getRedisInfo();

  // InfluxDB removed from core library for now or kept as placeholder
  bool isInfluxSupported() { return false; }

  bool executeQuery(const std::string &query,
                    std::vector<std::vector<std::string>> &results);
  bool executeNonQuery(const std::string &query);

  std::map<std::string, bool> getAllConnectionStatus();
  void disconnectAll();

  void log(int level, const std::string &message);

private:
  DatabaseManager();
  ~DatabaseManager();
  DatabaseManager(const DatabaseManager &) = delete;
  DatabaseManager &operator=(const DatabaseManager &) = delete;

  void ensureInitialized();
  bool doInitialize();

  static std::once_flag init_flag_;
  static std::atomic<bool> initialization_success_;

  bool connectSQLite();

#ifdef HAS_POSTGRESQL
  bool connectPostgres();
#endif

#ifdef HAS_MYSQL
  bool connectMySQL();
#endif

  bool connectMSSQL();

  sqlite3 *sqlite_conn_ = nullptr;

#ifdef HAS_POSTGRESQL
  std::unique_ptr<pqxx::connection> pg_conn_;
#endif

#ifdef HAS_MYSQL
  MYSQL *mysql_conn_ = nullptr;
#endif

#if HAS_MSSQL
  SQLHENV mssql_env_ = nullptr;
  SQLHDBC mssql_conn_ = nullptr;
#else
  void *mssql_env_ = nullptr;
  void *mssql_conn_ = nullptr;
#endif

  std::unique_ptr<RedisClientImpl> redis_client_;

  std::map<DatabaseType, bool> enabled_databases_;
  DatabaseType primary_rdb_ = DatabaseType::SQLITE;
  mutable std::mutex db_mutex_;

  DatabaseConfig current_config_;
  IDbLogger *logger_ = nullptr;

  std::string getDatabaseTypeName(DatabaseType type);
  std::string buildConnectionString(DatabaseType type);
};

} // namespace DbLib

#endif // DBLIB_DATABASE_MANAGER_HPP