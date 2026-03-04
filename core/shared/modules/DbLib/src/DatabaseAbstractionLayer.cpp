#include "DatabaseAbstractionLayer.hpp"
#include "DatabaseManager.hpp"
#include <algorithm>
#include <regex>
#include <sstream>

namespace DbLib {

// 🎯 Constructor
DatabaseAbstractionLayer::DatabaseAbstractionLayer()
    : db_manager_(&DatabaseManager::getInstance()) {

  // Initialize from config
  current_db_type_ = db_manager_->getConfig().type;

  // Normalize to uppercase
  std::transform(current_db_type_.begin(), current_db_type_.end(),
                 current_db_type_.begin(), ::toupper);

  db_manager_->log(1, "DatabaseAbstractionLayer created"); // 1 for Info/Debug
}

// 🎯 SQL Statement Type detection
SQLStatementType
DatabaseAbstractionLayer::detectStatementType(const std::string &query) {
  std::string upper_query = query.substr(0, std::min(query.size(), size_t(50)));
  std::transform(upper_query.begin(), upper_query.end(), upper_query.begin(),
                 ::toupper);

  static const std::vector<std::string> ddl_keywords = {
      "CREATE", "ALTER", "DROP", "TRUNCATE", "RENAME"};

  for (const auto &keyword : ddl_keywords) {
    if (upper_query.find(keyword) == 0 ||
        upper_query.find(" " + keyword + " ") != std::string::npos) {
      return SQLStatementType::DDL;
    }
  }

  static const std::vector<std::string> dml_keywords = {
      "SELECT", "INSERT", "UPDATE", "DELETE", "MERGE"};

  for (const auto &keyword : dml_keywords) {
    if (upper_query.find(keyword) == 0) {
      return SQLStatementType::DML;
    }
  }

  if (upper_query.find("GRANT") == 0 || upper_query.find("REVOKE") == 0) {
    return SQLStatementType::DCL;
  }

  if (upper_query.find("COMMIT") == 0 || upper_query.find("ROLLBACK") == 0 ||
      upper_query.find("SAVEPOINT") == 0) {
    return SQLStatementType::TCL;
  }

  return SQLStatementType::UNKNOWN;
}

// 🎯 executeQuery
std::vector<std::map<std::string, std::string>>
DatabaseAbstractionLayer::executeQuery(const std::string &query) {
  try {
    // 🔥 DEBUG: Log the original query
    db_manager_->log(0, "🔍 [DEBUG] Original query: " + query);

    std::string adapted_query = adaptQuery(query);
    std::vector<std::string> column_names = extractColumnsFromQuery(query);

    bool has_wildcard = false;
    for (const auto &col : column_names) {
      if (col == "*") {
        has_wildcard = true;
        break;
      }
    }

    if (column_names.empty() || has_wildcard) {
      std::string table_name = extractTableNameFromQuery(query);
      if (!table_name.empty()) {
        column_names = getTableColumnsFromSchema(table_name);
      }

      if (column_names.empty()) {
        db_manager_->log(3, "Failed to extract column names");
        return {};
      }
    }

    std::vector<std::vector<std::string>> raw_results;

    // 🔥 DEBUG: Log the actual query being executed
    db_manager_->log(0, "🔍 [DEBUG] Executing query: " + adapted_query);
    db_manager_->log(0, "🔍 [DEBUG] Column names extracted: " +
                            std::to_string(column_names.size()));

    bool success = db_manager_->executeQuery(adapted_query, raw_results);

    // 🔥 DEBUG: Log the results
    db_manager_->log(0, "🔍 [DEBUG] Query success: " +
                            std::string(success ? "true" : "false"));
    db_manager_->log(0, "🔍 [DEBUG] Raw results count: " +
                            std::to_string(raw_results.size()));

    if (!success) {
      db_manager_->log(3, "Query execution failed");
      return {};
    }

    if (raw_results.empty()) {
      db_manager_->log(0, "🔍 [DEBUG] Query returned 0 rows");
      return {};
    }

    auto map_results = convertToMapResults(raw_results, column_names);
    db_manager_->log(0, "🔍 [DEBUG] Converted to " +
                            std::to_string(map_results.size()) +
                            " map results");
    return map_results;

  } catch (const std::exception &e) {
    db_manager_->log(3, "executeQuery failed: " + std::string(e.what()));
    return {};
  }
}

// 🎯 executeNonQuery
bool DatabaseAbstractionLayer::executeNonQuery(const std::string &query) {
  try {
    std::string upper_query = query;
    std::transform(upper_query.begin(), upper_query.end(), upper_query.begin(),
                   ::toupper);

    bool is_transaction_control =
        (upper_query.find("BEGIN") == 0 || upper_query.find("COMMIT") == 0 ||
         upper_query.find("ROLLBACK") == 0 ||
         upper_query.find("SAVEPOINT") == 0);

    SQLStatementType stmt_type = detectStatementType(query);
    bool is_ddl_dcl = (stmt_type == SQLStatementType::DDL ||
                       stmt_type == SQLStatementType::DCL);

    std::string adapted_query = adaptQuery(query);
    bool needs_auto_transaction = !is_transaction_control && !is_ddl_dcl;

    if (needs_auto_transaction) {
      if (!db_manager_->executeNonQuery("BEGIN TRANSACTION")) {
        needs_auto_transaction = false;
      }
    }

    bool success = db_manager_->executeNonQuery(adapted_query);

    if (needs_auto_transaction) {
      if (success) {
        if (!db_manager_->executeNonQuery("COMMIT")) {
          db_manager_->executeNonQuery("ROLLBACK");
          return false;
        }
      } else {
        db_manager_->executeNonQuery("ROLLBACK");
      }
    }

    return success;

  } catch (const std::exception &e) {
    db_manager_->log(3, "executeNonQuery failed: " + std::string(e.what()));
    try {
      db_manager_->executeNonQuery("ROLLBACK");
    } catch (...) {
    }
    return false;
  }
}

// 🎯 adaptQuery
std::string DatabaseAbstractionLayer::adaptQuery(const std::string &query) {
  std::string adapted = query;
  SQLStatementType stmt_type = detectStatementType(query);

  if (stmt_type == SQLStatementType::DDL) {
    // DDL도 DB별 문법으로 변환 (AUTOINCREMENT, datetime 기본값 등)
    return adaptDDL(adapted);
  }

  if (stmt_type == SQLStatementType::DCL ||
      stmt_type == SQLStatementType::TCL) {
    return adapted;
  }

  if (stmt_type == SQLStatementType::DML) {
    adapted = adaptUpsertQuery(adapted);
    adapted = adaptBooleanValues(adapted);
    adapted = adaptTimestampFunctions(adapted);
    adapted = adaptLimitOffset(adapted);
    adapted = adaptSpecialSyntax(adapted);
  }

  return adapted;
}

// =============================================================================
// 🔥 DDL 변환: CREATE TABLE 등 SQLite 전용 문법 → 각 DB 방언으로 자동 변환
// =============================================================================
std::string DatabaseAbstractionLayer::adaptDDL(const std::string &query) {
  std::string result = query;

  if (current_db_type_ == "SQLITE") {
    // SQLite는 그대로 사용
    return result;
  }

  // ── 공통: datetime('now', 'localtime') 기본값 → CURRENT_TIMESTAMP ─────────
  // CREATE TABLE의 DEFAULT (datetime('now', 'localtime')) 처리
  result = std::regex_replace(
      result,
      std::regex("DEFAULT\\s*\\(datetime\\s*\\('now'\\s*,\\s*'localtime'\\)\\)",
                 std::regex_constants::icase),
      "DEFAULT CURRENT_TIMESTAMP");

  // datetime('now', 'localtime') 단독 사용 (COPY_TO_DEVICE 등)
  result = std::regex_replace(
      result,
      std::regex("datetime\\s*\\('now'\\s*,\\s*'localtime'\\)",
                 std::regex_constants::icase),
      "CURRENT_TIMESTAMP");

  // ── PostgreSQL ────────────────────────────────────────────────────────────
  if (current_db_type_ == "POSTGRESQL") {
    // INTEGER PRIMARY KEY AUTOINCREMENT → SERIAL PRIMARY KEY
    result = std::regex_replace(
        result,
        std::regex("INTEGER\\s+PRIMARY\\s+KEY\\s+AUTOINCREMENT",
                   std::regex_constants::icase),
        "SERIAL PRIMARY KEY");
    // BIGINT PRIMARY KEY AUTOINCREMENT → BIGSERIAL PRIMARY KEY
    result = std::regex_replace(
        result,
        std::regex("BIGINT\\s+PRIMARY\\s+KEY\\s+AUTOINCREMENT",
                   std::regex_constants::icase),
        "BIGSERIAL PRIMARY KEY");
    // 남은 AUTOINCREMENT 제거 (인라인으로 붙은 경우)
    result = std::regex_replace(
        result, std::regex("\\bAUTOINCREMENT\\b", std::regex_constants::icase),
        "");
    // VARCHAR(n) → VARCHAR(n) 호환, TEXT → TEXT 호환 (그대로)
    // REAL → DOUBLE PRECISION
    result = std::regex_replace(
        result, std::regex("\\bREAL\\b", std::regex_constants::icase),
        "DOUBLE PRECISION");
  }

  // ── MySQL / MariaDB ───────────────────────────────────────────────────────
  if (current_db_type_ == "MYSQL" || current_db_type_ == "MARIADB") {
    // AUTOINCREMENT → AUTO_INCREMENT
    result = std::regex_replace(
        result, std::regex("\\bAUTOINCREMENT\\b", std::regex_constants::icase),
        "AUTO_INCREMENT");
    // DATETIME DEFAULT CURRENT_TIMESTAMP (이미 위에서 변환됨)
    // SQLite의 DATE 타입은 그대로 유지 (MySQL도 DATE 사용)
  }

  // ── MSSQL (SQL Server) ────────────────────────────────────────────────────
  if (current_db_type_ == "MSSQL") {
    // INTEGER PRIMARY KEY AUTOINCREMENT → INT PRIMARY KEY IDENTITY(1,1)
    result = std::regex_replace(
        result,
        std::regex("INTEGER\\s+PRIMARY\\s+KEY\\s+AUTOINCREMENT",
                   std::regex_constants::icase),
        "INT PRIMARY KEY IDENTITY(1,1)");
    // BIGINT PRIMARY KEY AUTOINCREMENT → BIGINT PRIMARY KEY IDENTITY(1,1)
    result = std::regex_replace(
        result,
        std::regex("BIGINT\\s+PRIMARY\\s+KEY\\s+AUTOINCREMENT",
                   std::regex_constants::icase),
        "BIGINT PRIMARY KEY IDENTITY(1,1)");
    // 남은 AUTOINCREMENT → IDENTITY(1,1)
    result = std::regex_replace(
        result, std::regex("\\bAUTOINCREMENT\\b", std::regex_constants::icase),
        "IDENTITY(1,1)");
    // CURRENT_TIMESTAMP → GETDATE() (MSSQL 표준)
    result = std::regex_replace(
        result,
        std::regex("\\bCURRENT_TIMESTAMP\\b", std::regex_constants::icase),
        "GETDATE()");
    // DEFAULT CURRENT_TIMESTAMP → DEFAULT GETDATE()
    result = std::regex_replace(
        result,
        std::regex("DEFAULT\\s+CURRENT_TIMESTAMP", std::regex_constants::icase),
        "DEFAULT GETDATE()");
    // TEXT → NVARCHAR(MAX) (MSSQL에서 TEXT는 deprecated)
    result = std::regex_replace(
        result, std::regex("\\bTEXT\\b", std::regex_constants::icase),
        "NVARCHAR(MAX)");
    // VARCHAR → NVARCHAR (유니코드 보장)
    result = std::regex_replace(
        result, std::regex("\\bVARCHAR\\b", std::regex_constants::icase),
        "NVARCHAR");
    // REAL → FLOAT
    result = std::regex_replace(
        result, std::regex("\\bREAL\\b", std::regex_constants::icase), "FLOAT");
    // INTEGER → INT (MSSQL 표준형)
    result = std::regex_replace(
        result,
        std::regex("\\bINTEGER\\b(?!\\s+PRIMARY)", std::regex_constants::icase),
        "INT");
    // IF NOT EXISTS → MSSQL 미지원, CREATE TABLE을 IF 체크로 감싸는 건
    // doesTableExist()가 미리 처리하므로 문법에서만 제거
    result = std::regex_replace(
        result,
        std::regex("CREATE\\s+TABLE\\s+IF\\s+NOT\\s+EXISTS\\s+",
                   std::regex_constants::icase),
        "CREATE TABLE ");
  }

  return result;
}

// 🎯 Upsert Adaptation
std::string
DatabaseAbstractionLayer::adaptUpsertQuery(const std::string &query) {
  std::string result = query;
  std::regex upsert_regex("INSERT\\s+OR\\s+REPLACE\\s+INTO\\s+(\\w+)\\s*\\(([^)"
                          "]+)\\)\\s*VALUES\\s*\\(([^)]+)\\)",
                          std::regex_constants::icase);
  std::smatch match;

  if (std::regex_search(result, match, upsert_regex)) {
    std::string table_name = match[1].str();
    std::string columns = match[2].str();
    std::string values = match[3].str();

    if (current_db_type_ == "POSTGRESQL") {
      std::string first_column = columns.substr(0, columns.find(','));
      result = "INSERT INTO " + table_name + " (" + columns + ") VALUES (" +
               values + ") " + "ON CONFLICT (" + first_column +
               ") DO UPDATE SET ";

      std::stringstream ss(columns);
      std::string column;
      std::vector<std::string> col_list;
      while (std::getline(ss, column, ','))
        col_list.push_back(column);

      for (size_t i = 1; i < col_list.size(); ++i) {
        if (i > 1)
          result += ", ";
        result += col_list[i] + " = EXCLUDED." + col_list[i];
      }

    } else if (current_db_type_ == "MYSQL" || current_db_type_ == "MARIADB") {
      result = "INSERT INTO " + table_name + " (" + columns + ") VALUES (" +
               values + ") " + "ON DUPLICATE KEY UPDATE ";

      std::stringstream ss(columns);
      std::string column;
      std::vector<std::string> col_list;
      while (std::getline(ss, column, ','))
        col_list.push_back(column);

      for (size_t i = 1; i < col_list.size(); ++i) {
        if (i > 1)
          result += ", ";
        result += col_list[i] + " = VALUES(" + col_list[i] + ")";
      }

    } else if (current_db_type_ == "MSSQL") {
      std::string first_column = columns.substr(0, columns.find(','));
      result = "MERGE " + table_name + " AS target USING (VALUES (" + values +
               ")) AS source (" + columns + ") " + "ON target." + first_column +
               " = source." + first_column + " " +
               "WHEN MATCHED THEN UPDATE SET ";

      std::stringstream ss(columns);
      std::string column;
      std::vector<std::string> col_list;
      while (std::getline(ss, column, ','))
        col_list.push_back(column);

      for (size_t i = 1; i < col_list.size(); ++i) {
        if (i > 1)
          result += ", ";
        result += "target." + col_list[i] + " = source." + col_list[i];
      }

      result += " WHEN NOT MATCHED THEN INSERT (" + columns + ") VALUES (" +
                columns + ");";
    }
  }
  return result;
}

std::string
DatabaseAbstractionLayer::adaptBooleanValues(const std::string &query) {
  std::string result = query;
  if (current_db_type_ == "SQLITE" || current_db_type_ == "MYSQL" ||
      current_db_type_ == "MSSQL") {
    result = std::regex_replace(
        result, std::regex("\\btrue\\b", std::regex_constants::icase), "1");
    result = std::regex_replace(
        result, std::regex("\\bfalse\\b", std::regex_constants::icase), "0");
  } else if (current_db_type_ == "POSTGRESQL") {
    // Simple heuristic
    std::regex bool_context(
        "(is_enabled|is_active|enabled|active)\\s*=\\s*([01])",
        std::regex_constants::icase);
    result = std::regex_replace(result, bool_context,
                                "$1 = $2::boolean"); // Simplified for now
  }
  return result;
}

std::string
DatabaseAbstractionLayer::adaptTimestampFunctions(const std::string &query) {
  std::string result = query;
  if (current_db_type_ == "SQLITE") {
    result = std::regex_replace(
        result,
        std::regex("(NOW\\(\\)|(datetime('now', 'localtime'))|GETDATE\\(\\))",
                   std::regex_constants::icase),
        "datetime('now', 'localtime')");
  } else if (current_db_type_ == "POSTGRESQL" || current_db_type_ == "MYSQL") {
    result = std::regex_replace(
        result,
        std::regex("(datetime\\('now',\\s*'localtime'\\)|GETDATE\\(\\))",
                   std::regex_constants::icase),
        "NOW()");
  } else if (current_db_type_ == "MSSQL") {
    result = std::regex_replace(
        result,
        std::regex("(NOW\\(\\)|datetime\\('now',\\s*'localtime'\\)|(datetime('"
                   "now', 'localtime')))",
                   std::regex_constants::icase),
        "GETDATE()");
  }
  return result;
}

std::string
DatabaseAbstractionLayer::adaptLimitOffset(const std::string &query) {
  std::string result = query;
  if (current_db_type_ == "MSSQL") {
    std::regex limit_offset("LIMIT\\s+(\\d+)\\s+OFFSET\\s+(\\d+)",
                            std::regex_constants::icase);
    result = std::regex_replace(result, limit_offset,
                                "OFFSET $2 ROWS FETCH NEXT $1 ROWS ONLY");
  }
  return result;
}

std::string
DatabaseAbstractionLayer::adaptSpecialSyntax(const std::string &query) {
  std::string result = query;
  if (current_db_type_ == "SQLITE") {
    result = std::regex_replace(
        result, std::regex("ISNULL\\(", std::regex_constants::icase),
        "IFNULL(");
  } else if (current_db_type_ == "POSTGRESQL") {
    result = std::regex_replace(
        result,
        std::regex("(IFNULL\\(|ISNULL\\()", std::regex_constants::icase),
        "COALESCE(");
  } else if (current_db_type_ == "MSSQL") {
    result = std::regex_replace(
        result,
        std::regex("(IFNULL\\(|COALESCE\\()", std::regex_constants::icase),
        "ISNULL(");
  }
  return result;
}

std::vector<std::string>
DatabaseAbstractionLayer::extractColumnsFromQuery(const std::string &query) {
  try {
    std::string cleaned_query = preprocessQuery(query);
    std::regex select_from_pattern(R"(SELECT\s+(.*?)(?:\s+FROM|$))",
                                   std::regex_constants::icase);
    std::smatch matches;

    if (!std::regex_search(cleaned_query, matches, select_from_pattern))
      return {};

    std::string columns_section =
        std::regex_replace(matches[1].str(), std::regex(R"(\s+$)"), "");
    return parseColumnList(columns_section);
  } catch (...) {
    return {};
  }
}

std::vector<std::map<std::string, std::string>>
DatabaseAbstractionLayer::convertToMapResults(
    const std::vector<std::vector<std::string>> &raw_results,
    const std::vector<std::string> &column_names) {

  std::vector<std::map<std::string, std::string>> map_results;
  map_results.reserve(raw_results.size());
  for (const auto &row : raw_results) {
    std::map<std::string, std::string> row_map;
    size_t min_size = std::min(column_names.size(), row.size());
    for (size_t i = 0; i < min_size; ++i)
      row_map[column_names[i]] = row[i];
    map_results.push_back(row_map);
  }
  return map_results;
}

std::string
DatabaseAbstractionLayer::preprocessQuery(const std::string &query) {
  std::string result =
      std::regex_replace(query, std::regex(R"(--[^\r\n]*)"), "");
  // Multiline comments removal logic simplified or kept same as original if
  // needed
  result = std::regex_replace(result, std::regex(R"(\s+)"), " ");
  return result;
}

std::vector<std::string>
DatabaseAbstractionLayer::parseColumnList(const std::string &columns_section) {
  std::vector<std::string> columns;
  std::string current_column;
  int paren_depth = 0;
  bool in_quotes = false;
  char quote_char = 0;

  for (char c : columns_section) {
    if ((c == '\'' || c == '"') && !in_quotes) {
      in_quotes = true;
      quote_char = c;
      current_column += c;
    } else if (c == quote_char && in_quotes) {
      in_quotes = false;
      quote_char = 0;
      current_column += c;
    } else if (in_quotes) {
      current_column += c;
    } else if (c == '(') {
      paren_depth++;
      current_column += c;
    } else if (c == ')') {
      paren_depth--;
      current_column += c;
    } else if (c == ',' && paren_depth == 0) {
      columns.push_back(trimColumn(current_column));
      current_column.clear();
    } else {
      current_column += c;
    }
  }
  columns.push_back(trimColumn(current_column));
  return columns;
}

std::string DatabaseAbstractionLayer::trimColumn(const std::string &column) {
  std::string result =
      std::regex_replace(column, std::regex(R"(^\s+|\s+$)"), "");
  std::regex as_pattern(R"(\s+AS\s+(\w+))", std::regex_constants::icase);
  std::smatch as_match;
  if (std::regex_search(result, as_match, as_pattern))
    return as_match[1].str();
  size_t dot_pos = result.find_last_of('.');
  if (dot_pos != std::string::npos)
    return result.substr(dot_pos + 1);
  return result;
}

std::string
DatabaseAbstractionLayer::extractTableNameFromQuery(const std::string &query) {
  std::regex from_pattern(R"(FROM\s+(\w+))", std::regex_constants::icase);
  std::smatch matches;
  if (std::regex_search(query, matches, from_pattern))
    return matches[1].str();
  return "";
}

std::vector<std::string> DatabaseAbstractionLayer::getTableColumnsFromSchema(
    const std::string &table_name) {

  if (current_db_type_ == "SQLITE") {
    // SQLite: PRAGMA table_info()
    std::vector<std::vector<std::string>> pragma_results;
    if (db_manager_->executeQuery("PRAGMA table_info(" + table_name + ")",
                                  pragma_results)) {
      std::vector<std::string> columns;
      for (const auto &row : pragma_results)
        if (row.size() > 1)
          columns.push_back(row[1]); // 인덱스 1 = column name
      return columns;
    }
  } else {
    // PostgreSQL / MySQL / MariaDB: information_schema.columns
    std::string schema_filter;
    if (current_db_type_ == "POSTGRESQL") {
      schema_filter = " AND table_schema = 'public'";
    }
    std::string query = "SELECT column_name FROM information_schema.columns "
                        "WHERE table_name = '" +
                        table_name + "'" + schema_filter +
                        " ORDER BY ordinal_position";

    std::vector<std::vector<std::string>> results;
    if (db_manager_->executeQuery(query, results)) {
      std::vector<std::string> columns;
      for (const auto &row : results)
        if (!row.empty())
          columns.push_back(row[0]);
      return columns;
    }
  }
  return {};
}

bool DatabaseAbstractionLayer::executeCreateTable(
    const std::string &create_sql) {
  std::string table_name = extractTableNameFromCreateSQL(create_sql);
  if (table_name.empty())
    return false;
  if (doesTableExist(table_name))
    return true;
  return executeNonQuery(create_sql);
}

bool DatabaseAbstractionLayer::doesTableExist(const std::string &table_name) {
  if (current_db_type_ == "POSTGRESQL") {
    std::string query = "SELECT table_name FROM information_schema.tables "
                        "WHERE table_schema='public' AND table_name='" +
                        table_name + "'";
    auto results = executeQuery(query);
    return !results.empty();
  }

  if (current_db_type_ == "MYSQL" || current_db_type_ == "MARIADB") {
    std::string query = "SELECT table_name FROM information_schema.tables "
                        "WHERE table_name='" +
                        table_name + "'";
    auto results = executeQuery(query);
    return !results.empty();
  }

  if (current_db_type_ == "MSSQL") {
    // MSSQL: sys.tables 사용 (information_schema.tables도 동작하나 sys가 권장)
    std::string query =
        "SELECT name FROM sys.tables WHERE name='" + table_name + "'";
    auto results = executeQuery(query);
    return !results.empty();
  }

  // SQLite: sqlite_master 사용
  std::string query =
      "SELECT name FROM sqlite_master WHERE type='table' AND name='" +
      table_name + "'";
  auto results = executeQuery(query);
  return !results.empty();
}

std::string DatabaseAbstractionLayer::extractTableNameFromCreateSQL(
    const std::string &create_sql) {
  std::regex table_regex(R"(CREATE\s+TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?(\w+))",
                         std::regex_constants::icase);
  std::smatch matches;
  if (std::regex_search(create_sql, matches, table_regex))
    return matches[1].str();
  return "";
}

std::string DatabaseAbstractionLayer::getCurrentDbType() {
  return current_db_type_;
}

bool DatabaseAbstractionLayer::parseBoolean(const std::string &value) {
  std::string val = value;
  std::transform(val.begin(), val.end(), val.begin(), ::tolower);
  return (val == "1" || val == "true" || val == "yes" || val == "on");
}

std::string DatabaseAbstractionLayer::formatBoolean(bool value) {
  if (current_db_type_ == "POSTGRESQL")
    return value ? "true" : "false";
  return value ? "1" : "0";
}

std::string DatabaseAbstractionLayer::getCurrentTimestamp() {
  if (current_db_type_ == "SQLITE")
    return "datetime('now', 'localtime')";
  if (current_db_type_ == "MSSQL")
    return "GETDATE()";
  return "NOW()";
}

// Generic timestamp string
std::string DatabaseAbstractionLayer::getGenericTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::ostringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

// =============================================================================
// 🔥 getLastInsertId: DB별 마지막 INSERT ID 반환 (last_insert_rowid 완전 대체)
// 각 DB의 세션-로컬 마지막 INSERT 행 ID를 반환하므로 thread-safe
// =============================================================================
int64_t DatabaseAbstractionLayer::getLastInsertId() {
  std::string query;

  if (current_db_type_ == "SQLITE") {
    query = "SELECT last_insert_rowid() as id";
  } else if (current_db_type_ == "POSTGRESQL") {
    // lastval()은 현재 세션에서 마지막으로 사용된 SEQUENCE 값 반환
    query = "SELECT lastval() as id";
  } else if (current_db_type_ == "MYSQL" || current_db_type_ == "MARIADB") {
    query = "SELECT LAST_INSERT_ID() as id";
  } else if (current_db_type_ == "MSSQL") {
    // SCOPE_IDENTITY()는 현재 scope의 마지막 INSERT id (@@IDENTITY보다 안전)
    query = "SELECT SCOPE_IDENTITY() as id";
  } else {
    // 알수 없는 DB: SQLite 문법 시도
    query = "SELECT last_insert_rowid() as id";
  }

  try {
    auto results = executeQuery(query);
    if (!results.empty() && results[0].count("id")) {
      const std::string &val = results[0].at("id");
      if (!val.empty() && val != "NULL" && val != "null") {
        return std::stoll(val);
      }
    }
  } catch (const std::exception &e) {
    db_manager_->log(3, "getLastInsertId failed: " + std::string(e.what()));
  }

  return -1; // 실패 시 -1 반환 (0은 유효한 rowid일 수 있음)
}

// 🔥 Batch: N개 쿼리를 1 트랜잭션으로 처리 (30K 포인트 성능 최적화)
bool DatabaseAbstractionLayer::executeBatch(
    const std::vector<std::string> &queries) {
  if (queries.empty())
    return true;

  try {
    // 1. 트랜잭션 시작 (BEGIN 1회)
    if (!db_manager_->executeNonQuery("BEGIN TRANSACTION")) {
      db_manager_->log(3, "executeBatch: BEGIN TRANSACTION failed");
      return false;
    }

    size_t success_count = 0;
    size_t fail_count = 0;

    // 2. 각 쿼리를 트랜잭션 컨트롤 없이 직접 실행
    for (const auto &query : queries) {
      std::string adapted = adaptQuery(query);
      if (db_manager_->executeNonQuery(adapted)) {
        success_count++;
      } else {
        fail_count++;
        db_manager_->log(2, "executeBatch: query failed (continuing): " +
                                query.substr(0, 80));
      }
    }

    // 3. COMMIT 1회
    if (!db_manager_->executeNonQuery("COMMIT")) {
      db_manager_->log(3, "executeBatch: COMMIT failed, rolling back");
      db_manager_->executeNonQuery("ROLLBACK");
      return false;
    }

    db_manager_->log(0, "executeBatch: " + std::to_string(success_count) + "/" +
                            std::to_string(queries.size()) +
                            " queries committed");
    return fail_count == 0;

  } catch (const std::exception &e) {
    db_manager_->log(3, "executeBatch exception: " + std::string(e.what()));
    try {
      db_manager_->executeNonQuery("ROLLBACK");
    } catch (...) {
    }
    return false;
  }
}

bool DatabaseAbstractionLayer::executeUpsert(
    const std::string &table_name,
    const std::map<std::string, std::string> &data,
    const std::vector<std::string> &primary_keys) {

  try {
    std::vector<std::string> columns;
    std::vector<std::string> values;

    for (const auto &[key, value] : data) {
      columns.push_back(key);
      values.push_back("'" + value + "'");
    }

    std::string query;
    std::string db_type = getCurrentDbType();

    if (db_type == "SQLITE") {
      query = "INSERT OR REPLACE INTO " + table_name + " (";
      for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0)
          query += ", ";
        query += columns[i];
      }
      query += ") VALUES (";
      for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0)
          query += ", ";
        query += values[i];
      }
      query += ")";

    } else if (db_type == "MYSQL") {
      query = "INSERT INTO " + table_name + " (";
      for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0)
          query += ", ";
        query += columns[i];
      }
      query += ") VALUES (";
      for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0)
          query += ", ";
        query += values[i];
      }
      query += ") ON DUPLICATE KEY UPDATE ";

      bool first = true;
      for (size_t i = 0; i < columns.size(); ++i) {
        bool is_primary = std::find(primary_keys.begin(), primary_keys.end(),
                                    columns[i]) != primary_keys.end();
        if (!is_primary) {
          if (!first)
            query += ", ";
          query += columns[i] + " = VALUES(" + columns[i] + ")";
          first = false;
        }
      }

    } else {
      query = "INSERT INTO " + table_name + " (";
      for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0)
          query += ", ";
        query += columns[i];
      }
      query += ") VALUES (";
      for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0)
          query += ", ";
        query += values[i];
      }
      query += ")";
    }

    return executeNonQuery(query);

  } catch (const std::exception &e) {
    db_manager_->log(3, "DatabaseAbstractionLayer::executeUpsert failed: " +
                            std::string(e.what()));
    return false;
  }
}

} // namespace DbLib