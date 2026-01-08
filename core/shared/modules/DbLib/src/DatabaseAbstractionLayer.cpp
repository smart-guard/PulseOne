#include "DatabaseAbstractionLayer.hpp"
#include "DatabaseManager.hpp"
#include <algorithm>
#include <sstream>
#include <regex>

namespace DbLib {

// 🎯 Constructor
DatabaseAbstractionLayer::DatabaseAbstractionLayer() 
    : db_manager_(&DatabaseManager::getInstance()) {
    
    // Default to SQLITE if not set
    current_db_type_ = "SQLITE";
    
    db_manager_->log(1, "DatabaseAbstractionLayer created"); // 1 for Info/Debug
}

// 🎯 SQL Statement Type detection
SQLStatementType DatabaseAbstractionLayer::detectStatementType(const std::string& query) {
    std::string upper_query = query.substr(0, std::min(query.size(), size_t(50)));
    std::transform(upper_query.begin(), upper_query.end(), upper_query.begin(), ::toupper);
    
    static const std::vector<std::string> ddl_keywords = {
        "CREATE", "ALTER", "DROP", "TRUNCATE", "RENAME"
    };
    
    for (const auto& keyword : ddl_keywords) {
        if (upper_query.find(keyword) == 0 || 
            upper_query.find(" " + keyword + " ") != std::string::npos) {
            return SQLStatementType::DDL;
        }
    }
    
    static const std::vector<std::string> dml_keywords = {
        "SELECT", "INSERT", "UPDATE", "DELETE", "MERGE"
    };
    
    for (const auto& keyword : dml_keywords) {
        if (upper_query.find(keyword) == 0) {
            return SQLStatementType::DML;
        }
    }
    
    if (upper_query.find("GRANT") == 0 || upper_query.find("REVOKE") == 0) {
        return SQLStatementType::DCL;
    }
    
    if (upper_query.find("COMMIT") == 0 || 
        upper_query.find("ROLLBACK") == 0 || 
        upper_query.find("SAVEPOINT") == 0) {
        return SQLStatementType::TCL;
    }
    
    return SQLStatementType::UNKNOWN;
}

// 🎯 executeQuery
std::vector<std::map<std::string, std::string>> DatabaseAbstractionLayer::executeQuery(const std::string& query) {
    try {
        std::string adapted_query = adaptQuery(query);
        std::vector<std::string> column_names = extractColumnsFromQuery(query);
        
        bool has_wildcard = false;
        for (const auto& col : column_names) {
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
        bool success = db_manager_->executeQuery(adapted_query, raw_results);
        
        if (!success) {
            return {};
        }
        
        if (raw_results.empty()) {
            return {};
        }
        
        return convertToMapResults(raw_results, column_names);
        
    } catch (const std::exception& e) {
        db_manager_->log(3, "executeQuery failed: " + std::string(e.what()));
        return {};
    }
}

// 🎯 executeNonQuery
bool DatabaseAbstractionLayer::executeNonQuery(const std::string& query) {
    try {
        std::string upper_query = query;
        std::transform(upper_query.begin(), upper_query.end(), upper_query.begin(), ::toupper);
        
        bool is_transaction_control = 
            (upper_query.find("BEGIN") == 0 || 
             upper_query.find("COMMIT") == 0 || 
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
        
    } catch (const std::exception& e) {
        db_manager_->log(3, "executeNonQuery failed: " + std::string(e.what()));
        try {
            db_manager_->executeNonQuery("ROLLBACK");
        } catch (...) {}
        return false;
    }
}

// 🎯 adaptQuery
std::string DatabaseAbstractionLayer::adaptQuery(const std::string& query) {
    std::string adapted = query;
    SQLStatementType stmt_type = detectStatementType(query);
    
    if (stmt_type == SQLStatementType::DDL || 
        stmt_type == SQLStatementType::DCL || 
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

// 🎯 Upsert Adaptation
std::string DatabaseAbstractionLayer::adaptUpsertQuery(const std::string& query) {
    std::string result = query;
    std::regex upsert_regex("INSERT\\s+OR\\s+REPLACE\\s+INTO\\s+(\\w+)\\s*\\(([^)]+)\\)\\s*VALUES\\s*\\(([^)]+)\\)", 
                           std::regex_constants::icase);
    std::smatch match;
    
    if (std::regex_search(result, match, upsert_regex)) {
        std::string table_name = match[1].str();
        std::string columns = match[2].str();
        std::string values = match[3].str();
        
        if (current_db_type_ == "POSTGRESQL") {
            std::string first_column = columns.substr(0, columns.find(','));
            result = "INSERT INTO " + table_name + " (" + columns + ") VALUES (" + values + ") " +
                    "ON CONFLICT (" + first_column + ") DO UPDATE SET ";
            
            std::stringstream ss(columns);
            std::string column;
            std::vector<std::string> col_list;
            while (std::getline(ss, column, ',')) col_list.push_back(column);
            
            for (size_t i = 1; i < col_list.size(); ++i) {
                if (i > 1) result += ", ";
                result += col_list[i] + " = EXCLUDED." + col_list[i];
            }
            
        } else if (current_db_type_ == "MYSQL" || current_db_type_ == "MARIADB") {
            result = "INSERT INTO " + table_name + " (" + columns + ") VALUES (" + values + ") " +
                    "ON DUPLICATE KEY UPDATE ";
            
            std::stringstream ss(columns);
            std::string column;
            std::vector<std::string> col_list;
            while (std::getline(ss, column, ',')) col_list.push_back(column);
            
            for (size_t i = 1; i < col_list.size(); ++i) {
                if (i > 1) result += ", ";
                result += col_list[i] + " = VALUES(" + col_list[i] + ")";
            }
            
        } else if (current_db_type_ == "MSSQL") {
            std::string first_column = columns.substr(0, columns.find(','));
            result = "MERGE " + table_name + " AS target USING (VALUES (" + values + ")) AS source (" + columns + ") " +
                    "ON target." + first_column + " = source." + first_column + " " +
                    "WHEN MATCHED THEN UPDATE SET ";
            
            std::stringstream ss(columns);
            std::string column;
            std::vector<std::string> col_list;
            while (std::getline(ss, column, ',')) col_list.push_back(column);
            
            for (size_t i = 1; i < col_list.size(); ++i) {
                if (i > 1) result += ", ";
                result += "target." + col_list[i] + " = source." + col_list[i];
            }
            
            result += " WHEN NOT MATCHED THEN INSERT (" + columns + ") VALUES (" + columns + ");";
        }
    }
    return result;
}

std::string DatabaseAbstractionLayer::adaptBooleanValues(const std::string& query) {
    std::string result = query;
    if (current_db_type_ == "SQLITE" || current_db_type_ == "MYSQL" || current_db_type_ == "MSSQL") {
        result = std::regex_replace(result, std::regex("\\btrue\\b", std::regex_constants::icase), "1");
        result = std::regex_replace(result, std::regex("\\bfalse\\b", std::regex_constants::icase), "0");
    } else if (current_db_type_ == "POSTGRESQL") {
        // Simple heuristic
        std::regex bool_context("(is_enabled|is_active|enabled|active)\\s*=\\s*([01])", std::regex_constants::icase);
        result = std::regex_replace(result, bool_context, "$1 = $2::boolean"); // Simplified for now
    }
    return result;
}

std::string DatabaseAbstractionLayer::adaptTimestampFunctions(const std::string& query) {
    std::string result = query;
    if (current_db_type_ == "SQLITE") {
        result = std::regex_replace(result, std::regex("(NOW\\(\\)|CURRENT_TIMESTAMP|GETDATE\\(\\))", std::regex_constants::icase), 
                                   "datetime('now', 'localtime')");
    } else if (current_db_type_ == "POSTGRESQL" || current_db_type_ == "MYSQL") {
        result = std::regex_replace(result, std::regex("(datetime\\('now',\\s*'localtime'\\)|GETDATE\\(\\))", std::regex_constants::icase), 
                                   "NOW()");
    } else if (current_db_type_ == "MSSQL") {
        result = std::regex_replace(result, std::regex("(NOW\\(\\)|datetime\\('now',\\s*'localtime'\\)|CURRENT_TIMESTAMP)", std::regex_constants::icase), 
                                   "GETDATE()");
    }
    return result;
}

std::string DatabaseAbstractionLayer::adaptLimitOffset(const std::string& query) {
    std::string result = query;
    if (current_db_type_ == "MSSQL") {
        std::regex limit_offset("LIMIT\\s+(\\d+)\\s+OFFSET\\s+(\\d+)", std::regex_constants::icase);
        result = std::regex_replace(result, limit_offset, "OFFSET $2 ROWS FETCH NEXT $1 ROWS ONLY");
    }
    return result;
}

std::string DatabaseAbstractionLayer::adaptSpecialSyntax(const std::string& query) {
    std::string result = query;
    if (current_db_type_ == "SQLITE") {
        result = std::regex_replace(result, std::regex("ISNULL\\(", std::regex_constants::icase), "IFNULL(");
    } else if (current_db_type_ == "POSTGRESQL") {
        result = std::regex_replace(result, std::regex("(IFNULL\\(|ISNULL\\()", std::regex_constants::icase), "COALESCE(");
    } else if (current_db_type_ == "MSSQL") {
        result = std::regex_replace(result, std::regex("(IFNULL\\(|COALESCE\\()", std::regex_constants::icase), "ISNULL(");
    }
    return result;
}

std::vector<std::string> DatabaseAbstractionLayer::extractColumnsFromQuery(const std::string& query) {
    try {
        std::string cleaned_query = preprocessQuery(query);
        std::regex select_from_pattern(R"(SELECT\s+(.*?)(?:\s+FROM|$))", std::regex_constants::icase);
        std::smatch matches;
        
        if (!std::regex_search(cleaned_query, matches, select_from_pattern)) return {};
        
        std::string columns_section = std::regex_replace(matches[1].str(), std::regex(R"(\s+$)"), "");
        return parseColumnList(columns_section);
    } catch (...) {
        return {};
    }
}

std::vector<std::map<std::string, std::string>> DatabaseAbstractionLayer::convertToMapResults(
    const std::vector<std::vector<std::string>>& raw_results,
    const std::vector<std::string>& column_names) {
    
    std::vector<std::map<std::string, std::string>> map_results;
    map_results.reserve(raw_results.size());
    for (const auto& row : raw_results) {
        std::map<std::string, std::string> row_map;
        size_t min_size = std::min(column_names.size(), row.size());
        for (size_t i = 0; i < min_size; ++i) row_map[column_names[i]] = row[i];
        map_results.push_back(row_map);
    }
    return map_results;
}

std::string DatabaseAbstractionLayer::preprocessQuery(const std::string& query) {
    std::string result = std::regex_replace(query, std::regex(R"(--[^\r\n]*)"), "");
    // Multiline comments removal logic simplified or kept same as original if needed
    result = std::regex_replace(result, std::regex(R"(\s+)"), " ");
    return result;
}

std::vector<std::string> DatabaseAbstractionLayer::parseColumnList(const std::string& columns_section) {
    std::vector<std::string> columns;
    std::string current_column;
    int paren_depth = 0;
    bool in_quotes = false;
    char quote_char = 0;
    
    for (char c : columns_section) {
        if ((c == '\'' || c == '"') && !in_quotes) { in_quotes = true; quote_char = c; current_column += c; }
        else if (c == quote_char && in_quotes) { in_quotes = false; quote_char = 0; current_column += c; }
        else if (in_quotes) { current_column += c; }
        else if (c == '(') { paren_depth++; current_column += c; }
        else if (c == ')') { paren_depth--; current_column += c; }
        else if (c == ',' && paren_depth == 0) { 
            columns.push_back(trimColumn(current_column)); current_column.clear(); 
        } else { current_column += c; }
    }
    columns.push_back(trimColumn(current_column));
    return columns;
}

std::string DatabaseAbstractionLayer::trimColumn(const std::string& column) {
    std::string result = std::regex_replace(column, std::regex(R"(^\s+|\s+$)"), "");
    std::regex as_pattern(R"(\s+AS\s+(\w+))", std::regex_constants::icase);
    std::smatch as_match;
    if (std::regex_search(result, as_match, as_pattern)) return as_match[1].str();
    size_t dot_pos = result.find_last_of('.');
    if (dot_pos != std::string::npos) return result.substr(dot_pos + 1);
    return result;
}

std::string DatabaseAbstractionLayer::extractTableNameFromQuery(const std::string& query) {
    std::regex from_pattern(R"(FROM\s+(\w+))", std::regex_constants::icase);
    std::smatch matches;
    if (std::regex_search(query, matches, from_pattern)) return matches[1].str();
    return "";
}

std::vector<std::string> DatabaseAbstractionLayer::getTableColumnsFromSchema(const std::string& table_name) {
    std::vector<std::vector<std::string>> pragma_results;
    if (db_manager_->executeQuery("PRAGMA table_info(" + table_name + ")", pragma_results)) {
        std::vector<std::string> columns;
        for (const auto& row : pragma_results) if (row.size() > 1) columns.push_back(row[1]);
        return columns;
    }
    return {};
}

bool DatabaseAbstractionLayer::executeCreateTable(const std::string& create_sql) {
    std::string table_name = extractTableNameFromCreateSQL(create_sql);
    if (table_name.empty()) return false;
    if (doesTableExist(table_name)) return true;
    return executeNonQuery(create_sql);
}

bool DatabaseAbstractionLayer::doesTableExist(const std::string& table_name) {
    std::string query = "SELECT name FROM sqlite_master WHERE type='table' AND name='" + table_name + "'";
    auto results = executeQuery(query);
    return !results.empty();
}

std::string DatabaseAbstractionLayer::extractTableNameFromCreateSQL(const std::string& create_sql) {
    std::regex table_regex(R"(CREATE\s+TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?(\w+))", std::regex_constants::icase);
    std::smatch matches;
    if (std::regex_search(create_sql, matches, table_regex)) return matches[1].str();
    return "";
}

std::string DatabaseAbstractionLayer::getCurrentDbType() {
    return current_db_type_;
}

bool DatabaseAbstractionLayer::parseBoolean(const std::string& value) {
    std::string val = value;
    std::transform(val.begin(), val.end(), val.begin(), ::tolower);
    return (val == "1" || val == "true" || val == "yes" || val == "on");
}

std::string DatabaseAbstractionLayer::formatBoolean(bool value) {
    if (current_db_type_ == "POSTGRESQL") return value ? "true" : "false";
    return value ? "1" : "0";
}


std::string DatabaseAbstractionLayer::getCurrentTimestamp() {
    if (current_db_type_ == "SQLITE") return "datetime('now', 'localtime')";
    if (current_db_type_ == "MSSQL") return "GETDATE()";
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

bool DatabaseAbstractionLayer::executeUpsert(
    const std::string& table_name,
    const std::map<std::string, std::string>& data,
    const std::vector<std::string>& primary_keys) {
    
    try {
        std::vector<std::string> columns;
        std::vector<std::string> values;
        
        for (const auto& [key, value] : data) {
            columns.push_back(key);
            values.push_back("'" + value + "'");
        }
        
        std::string query;
        std::string db_type = getCurrentDbType();
        
        if (db_type == "SQLITE") {
            query = "INSERT OR REPLACE INTO " + table_name + " (";
            for (size_t i = 0; i < columns.size(); ++i) {
                if (i > 0) query += ", ";
                query += columns[i];
            }
            query += ") VALUES (";
            for (size_t i = 0; i < values.size(); ++i) {
                if (i > 0) query += ", ";
                query += values[i];
            }
            query += ")";
            
        } else if (db_type == "MYSQL") {
            query = "INSERT INTO " + table_name + " (";
            for (size_t i = 0; i < columns.size(); ++i) {
                if (i > 0) query += ", ";
                query += columns[i];
            }
            query += ") VALUES (";
            for (size_t i = 0; i < values.size(); ++i) {
                if (i > 0) query += ", ";
                query += values[i];
            }
            query += ") ON DUPLICATE KEY UPDATE ";
            
            bool first = true;
            for (size_t i = 0; i < columns.size(); ++i) {
                bool is_primary = std::find(primary_keys.begin(), primary_keys.end(), columns[i]) != primary_keys.end();
                if (!is_primary) {
                    if (!first) query += ", ";
                    query += columns[i] + " = VALUES(" + columns[i] + ")";
                    first = false;
                }
            }
            
        } else {
            query = "INSERT INTO " + table_name + " (";
            for (size_t i = 0; i < columns.size(); ++i) {
                if (i > 0) query += ", ";
                query += columns[i];
            }
            query += ") VALUES (";
            for (size_t i = 0; i < values.size(); ++i) {
                if (i > 0) query += ", ";
                query += values[i];
            }
            query += ")";
        }
        
        return executeNonQuery(query);
        
    } catch (const std::exception& e) {
        db_manager_->log(3, "DatabaseAbstractionLayer::executeUpsert failed: " + std::string(e.what()));
        return false;
    }
}

} // namespace DbLib