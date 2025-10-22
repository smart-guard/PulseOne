// =============================================================================
// core/shared/src/Database/DatabaseAbstractionLayer.cpp
// 🎯 올바른 DatabaseAbstractionLayer - SQL 방언 차이점 처리
// =============================================================================

#include "Database/DatabaseAbstractionLayer.h"
#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include <algorithm>
#include <sstream>
#include <regex>

namespace PulseOne {
namespace Database {

// =============================================================================
// 🎯 생성자
// =============================================================================

DatabaseAbstractionLayer::DatabaseAbstractionLayer() 
    : db_manager_(&DatabaseManager::getInstance()) {
    
    // 현재 DB 타입 확인
    current_db_type_ = ConfigManager::getInstance().getOrDefault("DATABASE_TYPE", "SQLITE");
    std::transform(current_db_type_.begin(), current_db_type_.end(), current_db_type_.begin(), ::toupper);
    
    LogManager::getInstance().Debug("DatabaseAbstractionLayer created for DB type: " + current_db_type_);
}

// =============================================================================
// 🎯 SQL 구문 타입 판별
// =============================================================================

SQLStatementType DatabaseAbstractionLayer::detectStatementType(const std::string& query) {
    // 쿼리 앞부분만 추출 (성능 최적화)
    std::string upper_query = query.substr(0, std::min(query.size(), size_t(50)));
    std::transform(upper_query.begin(), upper_query.end(), upper_query.begin(), ::toupper);
    
    // DDL 키워드 체크 (스키마 변경 작업)
    static const std::vector<std::string> ddl_keywords = {
        "CREATE", "ALTER", "DROP", "TRUNCATE", "RENAME"
    };
    
    for (const auto& keyword : ddl_keywords) {
        if (upper_query.find(keyword) == 0 || 
            upper_query.find(" " + keyword + " ") != std::string::npos) {
            return SQLStatementType::DDL;
        }
    }
    
    // DML 키워드 체크 (데이터 조작)
    static const std::vector<std::string> dml_keywords = {
        "SELECT", "INSERT", "UPDATE", "DELETE", "MERGE"
    };
    
    for (const auto& keyword : dml_keywords) {
        if (upper_query.find(keyword) == 0) {
            return SQLStatementType::DML;
        }
    }
    
    // DCL 키워드 체크 (권한 관리)
    if (upper_query.find("GRANT") == 0 || upper_query.find("REVOKE") == 0) {
        return SQLStatementType::DCL;
    }
    
    // TCL 키워드 체크 (트랜잭션)
    if (upper_query.find("COMMIT") == 0 || 
        upper_query.find("ROLLBACK") == 0 || 
        upper_query.find("SAVEPOINT") == 0) {
        return SQLStatementType::TCL;
    }
    
    return SQLStatementType::UNKNOWN;
}

// =============================================================================
// 🎯 executeQuery - 표준 SQL을 DB별 방언으로 변환 후 실행
// =============================================================================

std::vector<std::map<std::string, std::string>> DatabaseAbstractionLayer::executeQuery(const std::string& query) {
    try {
        LogManager::getInstance().Debug("DatabaseAbstractionLayer::executeQuery - Processing query");
        
        // 1. 표준 SQL을 DB 방언으로 변환
        std::string adapted_query = adaptQuery(query);
        LogManager::getInstance().Debug("Original: " + query.substr(0, 100) + "...");
        LogManager::getInstance().Debug("Adapted: " + adapted_query.substr(0, 100) + "...");
        
        // 2. 컬럼명 추출 (개선된 파싱 사용)
        std::vector<std::string> column_names = extractColumnsFromQuery(query);
        
        if (column_names.empty()) {
            LogManager::getInstance().Error("Failed to extract column names from query");
            // 폴백 시도: 쿼리에서 테이블명 추출해서 스키마 조회
            std::string table_name = extractTableNameFromQuery(query);
            if (!table_name.empty()) {
                LogManager::getInstance().Warn("Attempting fallback: getting columns from table schema");
                column_names = getTableColumnsFromSchema(table_name);
            }
            
            if (column_names.empty()) {
                LogManager::getInstance().Error("All column extraction methods failed");
                return {};
            }
        }
        
        // 3. DatabaseManager를 통해 실제 실행
        std::vector<std::vector<std::string>> raw_results;
        bool success = db_manager_->executeQuery(adapted_query, raw_results);
        
        if (!success) {
            LogManager::getInstance().Error("Query execution failed");
            return {};
        }
        
        if (raw_results.empty()) {
            LogManager::getInstance().Debug("Query executed successfully but returned no rows");
            return {};
        }
        
        // 4. 결과를 맵으로 변환
        return convertToMapResults(raw_results, column_names);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DatabaseAbstractionLayer::executeQuery failed: " + std::string(e.what()));
        return {};
    }
}

// =============================================================================
// 🎯 executeNonQuery - 자동 트랜잭션 관리 추가 (v2.0 - 핵심 수정!)
// =============================================================================

bool DatabaseAbstractionLayer::executeNonQuery(const std::string& query) {
    try {
        // 1. 쿼리 타입 판별
        std::string upper_query = query;
        std::transform(upper_query.begin(), upper_query.end(), upper_query.begin(), ::toupper);
        
        // 2. 트랜잭션 제어 구문인지 확인
        bool is_transaction_control = 
            (upper_query.find("BEGIN") == 0 || 
             upper_query.find("COMMIT") == 0 || 
             upper_query.find("ROLLBACK") == 0 ||
             upper_query.find("SAVEPOINT") == 0);
        
        // 3. DDL/DCL 구문인지 확인
        SQLStatementType stmt_type = detectStatementType(query);
        bool is_ddl_dcl = (stmt_type == SQLStatementType::DDL || 
                           stmt_type == SQLStatementType::DCL);
        
        // 4. 표준 SQL을 DB 방언으로 변환
        std::string adapted_query = adaptQuery(query);
        LogManager::getInstance().Debug("DatabaseAbstractionLayer::executeNonQuery - Adapted query");
        
        // 5. 자동 트랜잭션 필요 여부 판단
        // - DML만 자동 트랜잭션 관리
        // - TCL(트랜잭션 제어), DDL, DCL은 그대로 실행
        bool needs_auto_transaction = !is_transaction_control && !is_ddl_dcl;
        
        if (needs_auto_transaction) {
            // ✅ 자동 트랜잭션 시작
            LogManager::getInstance().Debug("🔄 Auto-transaction: BEGIN");
            if (!db_manager_->executeNonQuery("BEGIN TRANSACTION")) {
                LogManager::getInstance().Warn("⚠️ Failed to start auto-transaction, continuing without it");
                needs_auto_transaction = false; // 트랜잭션 없이 계속 진행
            }
        }
        
        // 6. 실제 쿼리 실행
        bool success = db_manager_->executeNonQuery(adapted_query);
        
        // 7. 자동 트랜잭션 종료
        if (needs_auto_transaction) {
            if (success) {
                // ✅ 성공 시 자동 커밋
                LogManager::getInstance().Debug("✅ Auto-transaction: COMMIT");
                if (!db_manager_->executeNonQuery("COMMIT")) {
                    LogManager::getInstance().Error("❌ Failed to commit auto-transaction");
                    db_manager_->executeNonQuery("ROLLBACK");
                    return false;
                }
            } else {
                // ✅ 실패 시 자동 롤백
                LogManager::getInstance().Debug("🔙 Auto-transaction: ROLLBACK (query failed)");
                db_manager_->executeNonQuery("ROLLBACK");
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DatabaseAbstractionLayer::executeNonQuery failed: " + 
                                       std::string(e.what()));
        
        // ✅ 예외 발생 시 롤백 시도
        try {
            db_manager_->executeNonQuery("ROLLBACK");
            LogManager::getInstance().Debug("🔙 Auto-transaction: ROLLBACK (exception)");
        } catch (...) {
            // 롤백 실패는 무시 (이미 에러 상태)
        }
        
        return false;
    }
}

// =============================================================================
// 🎯 adaptQuery - 핵심! 표준 SQL을 DB 방언으로 변환
// =============================================================================

std::string DatabaseAbstractionLayer::adaptQuery(const std::string& query) {
    std::string adapted = query;
    
    // 🔥 핵심 개선: SQL 구문 타입 판별
    SQLStatementType stmt_type = detectStatementType(query);
    
    // DDL/DCL/TCL은 변환하지 않음
    if (stmt_type == SQLStatementType::DDL) {
        LogManager::getInstance().Debug("DDL statement detected - skipping adaptations");
        return adapted;
    }
    
    if (stmt_type == SQLStatementType::DCL || stmt_type == SQLStatementType::TCL) {
        LogManager::getInstance().Debug("DCL/TCL statement detected - skipping adaptations");
        return adapted;
    }
    
    // DML만 변환 수행
    if (stmt_type == SQLStatementType::DML) {
        // 1. UPSERT 구문 변환
        adapted = adaptUpsertQuery(adapted);
        
        // 2. Boolean 값 변환
        adapted = adaptBooleanValues(adapted);
        
        // 3. 타임스탬프 함수 변환
        adapted = adaptTimestampFunctions(adapted);
        
        // 4. LIMIT/OFFSET 구문 변환
        adapted = adaptLimitOffset(adapted);
        
        // 5. 기타 DB별 특수 구문 변환
        adapted = adaptSpecialSyntax(adapted);
    }
    
    return adapted;
}

// =============================================================================
// 🎯 개별 변환 메서드들
// =============================================================================

std::string DatabaseAbstractionLayer::adaptUpsertQuery(const std::string& query) {
    std::string result = query;
    
    // "INSERT OR REPLACE" 패턴 찾기
    std::regex upsert_regex("INSERT\\s+OR\\s+REPLACE\\s+INTO\\s+(\\w+)\\s*\\(([^)]+)\\)\\s*VALUES\\s*\\(([^)]+)\\)", 
                           std::regex_constants::icase);
    std::smatch match;
    
    if (std::regex_search(result, match, upsert_regex)) {
        std::string table_name = match[1].str();
        std::string columns = match[2].str();
        std::string values = match[3].str();
        
        if (current_db_type_ == "POSTGRESQL") {
            // PostgreSQL: ON CONFLICT 사용
            std::string first_column = columns.substr(0, columns.find(','));
            result = "INSERT INTO " + table_name + " (" + columns + ") VALUES (" + values + ") " +
                    "ON CONFLICT (" + first_column + ") DO UPDATE SET ";
            
            // UPDATE 절 생성
            std::stringstream ss(columns);
            std::string column;
            std::vector<std::string> col_list;
            while (std::getline(ss, column, ',')) {
                col_list.push_back(column);
            }
            
            for (size_t i = 1; i < col_list.size(); ++i) {
                if (i > 1) result += ", ";
                result += col_list[i] + " = EXCLUDED." + col_list[i];
            }
            
        } else if (current_db_type_ == "MYSQL" || current_db_type_ == "MARIADB") {
            // MySQL: ON DUPLICATE KEY UPDATE 사용
            result = "INSERT INTO " + table_name + " (" + columns + ") VALUES (" + values + ") " +
                    "ON DUPLICATE KEY UPDATE ";
            
            std::stringstream ss(columns);
            std::string column;
            std::vector<std::string> col_list;
            while (std::getline(ss, column, ',')) {
                col_list.push_back(column);
            }
            
            for (size_t i = 1; i < col_list.size(); ++i) {
                if (i > 1) result += ", ";
                result += col_list[i] + " = VALUES(" + col_list[i] + ")";
            }
            
        } else if (current_db_type_ == "MSSQL" || current_db_type_ == "SQLSERVER") {
            // MSSQL: MERGE 구문 사용
            std::string first_column = columns.substr(0, columns.find(','));
            
            result = "MERGE " + table_name + " AS target USING (VALUES (" + values + ")) AS source (" + columns + ") " +
                    "ON target." + first_column + " = source." + first_column + " " +
                    "WHEN MATCHED THEN UPDATE SET ";
            
            std::stringstream ss(columns);
            std::string column;
            std::vector<std::string> col_list;
            while (std::getline(ss, column, ',')) {
                col_list.push_back(column);
            }
            
            for (size_t i = 1; i < col_list.size(); ++i) {
                if (i > 1) result += ", ";
                result += "target." + col_list[i] + " = source." + col_list[i];
            }
            
            result += " WHEN NOT MATCHED THEN INSERT (" + columns + ") VALUES (" + columns + ");";
        }
        // SQLite는 그대로 유지
    }
    
    return result;
}

std::string DatabaseAbstractionLayer::adaptBooleanValues(const std::string& query) {
    std::string result = query;
    
    if (current_db_type_ == "SQLITE") {
        // true/false → 1/0
        result = std::regex_replace(result, std::regex("\\btrue\\b", std::regex_constants::icase), "1");
        result = std::regex_replace(result, std::regex("\\bfalse\\b", std::regex_constants::icase), "0");
        
    } else if (current_db_type_ == "POSTGRESQL") {
        // 1/0 → true/false
        std::regex bool_context("(is_enabled|is_active|enabled|active)\\s*=\\s*([01])", std::regex_constants::icase);
        
        std::string temp_result;
        std::sregex_iterator iter(result.begin(), result.end(), bool_context);
        std::sregex_iterator end;
        
        size_t last_pos = 0;
        for (; iter != end; ++iter) {
            const std::smatch& match = *iter;
            temp_result += result.substr(last_pos, match.position() - last_pos);
            std::string field = match[1].str();
            std::string value = match[2].str();
            std::string replacement = field + " = " + (value == "1" ? "true" : "false");
            temp_result += replacement;
            last_pos = match.position() + match.length();
        }
        temp_result += result.substr(last_pos);
        result = temp_result;
        
    } else if (current_db_type_ == "MYSQL" || current_db_type_ == "MARIADB") {
        result = std::regex_replace(result, std::regex("\\btrue\\b", std::regex_constants::icase), "1");
        result = std::regex_replace(result, std::regex("\\bfalse\\b", std::regex_constants::icase), "0");
        
    } else if (current_db_type_ == "MSSQL" || current_db_type_ == "SQLSERVER") {
        result = std::regex_replace(result, std::regex("\\btrue\\b", std::regex_constants::icase), "1");
        result = std::regex_replace(result, std::regex("\\bfalse\\b", std::regex_constants::icase), "0");
    }
    
    return result;
}

std::string DatabaseAbstractionLayer::adaptTimestampFunctions(const std::string& query) {
    std::string result = query;
    
    if (current_db_type_ == "SQLITE") {
        result = std::regex_replace(result, std::regex("NOW\\(\\)", std::regex_constants::icase), 
                                   "datetime('now', 'localtime')");
        result = std::regex_replace(result, std::regex("CURRENT_TIMESTAMP", std::regex_constants::icase), 
                                   "datetime('now', 'localtime')");
        result = std::regex_replace(result, std::regex("GETDATE\\(\\)", std::regex_constants::icase), 
                                   "datetime('now', 'localtime')");
                                   
    } else if (current_db_type_ == "POSTGRESQL") {
        result = std::regex_replace(result, std::regex("datetime\\('now',\\s*'localtime'\\)", std::regex_constants::icase), 
                                   "NOW()");
        result = std::regex_replace(result, std::regex("GETDATE\\(\\)", std::regex_constants::icase), "NOW()");
        
    } else if (current_db_type_ == "MYSQL" || current_db_type_ == "MARIADB") {
        result = std::regex_replace(result, std::regex("datetime\\('now',\\s*'localtime'\\)", std::regex_constants::icase), 
                                   "NOW()");
        result = std::regex_replace(result, std::regex("GETDATE\\(\\)", std::regex_constants::icase), "NOW()");
        
    } else if (current_db_type_ == "MSSQL" || current_db_type_ == "SQLSERVER") {
        result = std::regex_replace(result, std::regex("NOW\\(\\)", std::regex_constants::icase), "GETDATE()");
        result = std::regex_replace(result, std::regex("datetime\\('now',\\s*'localtime'\\)", std::regex_constants::icase), 
                                   "GETDATE()");
        result = std::regex_replace(result, std::regex("CURRENT_TIMESTAMP", std::regex_constants::icase), "GETDATE()");
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
        result = std::regex_replace(result, std::regex("IFNULL\\(", std::regex_constants::icase), "COALESCE(");
        result = std::regex_replace(result, std::regex("ISNULL\\(([^,]+),([^)]+)\\)", std::regex_constants::icase), 
                                   "COALESCE($1, $2)");
        
    } else if (current_db_type_ == "MSSQL" || current_db_type_ == "SQLSERVER") {
        result = std::regex_replace(result, std::regex("IFNULL\\(", std::regex_constants::icase), "ISNULL(");
        result = std::regex_replace(result, std::regex("COALESCE\\(([^,]+),([^)]+)\\)", std::regex_constants::icase), 
                                   "ISNULL($1, $2)");
    }
    
    return result;
}

// =============================================================================
// 🎯 컬럼 추출 및 결과 변환
// =============================================================================

std::vector<std::string> DatabaseAbstractionLayer::extractColumnsFromQuery(const std::string& query) {
    try {
        LogManager::getInstance().Debug("DatabaseAbstractionLayer::extractColumnsFromQuery - Processing query");
        
        std::string cleaned_query = preprocessQuery(query);
        LogManager::getInstance().Debug("Cleaned query: " + cleaned_query.substr(0, 200) + "...");
        
        // 🔥 개선: FROM 없는 SELECT도 처리 (예: SELECT last_insert_rowid() as id)
        std::regex select_from_pattern(R"(SELECT\s+(.*?)(?:\s+FROM|$))", std::regex_constants::icase);
        //                                                    ↑ FROM 또는 문장 끝
        std::smatch matches;
        
        if (!std::regex_search(cleaned_query, matches, select_from_pattern)) {
            LogManager::getInstance().Error("No SELECT pattern found");
            return {};
        }
        
        std::string columns_section = matches[1].str();
        
        // 🔥 추가: 문장 끝 공백 제거
        columns_section = std::regex_replace(columns_section, std::regex(R"(\s+$)"), "");
        
        LogManager::getInstance().Debug("Columns section: " + columns_section);
        
        std::vector<std::string> columns = parseColumnList(columns_section);
        
        if (columns.empty()) {
            LogManager::getInstance().Warn("No columns extracted from query");
            return {};
        }
        
        LogManager::getInstance().Debug("Successfully extracted " + std::to_string(columns.size()) + " columns");
        for (const auto& col : columns) {
            LogManager::getInstance().Debug("  Column: " + col);
        }
        
        return columns;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("extractColumnsFromQuery failed: " + std::string(e.what()));
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
        for (size_t i = 0; i < min_size; ++i) {
            row_map[column_names[i]] = row[i];
        }
        
        map_results.push_back(row_map);
    }
    
    return map_results;
}

// =============================================================================
// 🎯 헬퍼 메서드들
// =============================================================================

std::string DatabaseAbstractionLayer::preprocessQuery(const std::string& query) {
    std::string result = query;
    
    // 1. SQL 주석 제거 (-- 형태)
    result = std::regex_replace(result, std::regex(R"(--[^\r\n]*)"), "");
    
    // 2. C 스타일 주석 제거 (/* */ 형태)
    std::string temp;
    std::istringstream stream(result);
    std::string line;
    bool in_multiline_comment = false;
    
    while (std::getline(stream, line)) {
        size_t comment_start = 0;
        while ((comment_start = line.find("/*", comment_start)) != std::string::npos) {
            size_t comment_end = line.find("*/", comment_start + 2);
            if (comment_end != std::string::npos) {
                line.erase(comment_start, comment_end - comment_start + 2);
            } else {
                line.erase(comment_start);
                in_multiline_comment = true;
                break;
            }
        }
        
        if (in_multiline_comment) {
            size_t comment_end = line.find("*/");
            if (comment_end != std::string::npos) {
                line = line.substr(comment_end + 2);
                in_multiline_comment = false;
            } else {
                continue;
            }
        }
        
        temp += line + " ";
    }
    result = temp;
    
    // 3. 연속된 공백을 하나로 변경
    result = std::regex_replace(result, std::regex(R"(\s+)"), " ");
    
    // 4. 앞뒤 공백 제거
    result = std::regex_replace(result, std::regex(R"(^\s+|\s+$)"), "");
    
    return result;
}

std::vector<std::string> DatabaseAbstractionLayer::parseColumnList(const std::string& columns_section) {
    std::vector<std::string> columns;
    
    std::string current_column;
    int paren_depth = 0;
    bool in_quotes = false;
    char quote_char = 0;
    
    for (size_t i = 0; i < columns_section.size(); ++i) {
        char c = columns_section[i];
        
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
            std::string trimmed = trimColumn(current_column);
            if (!trimmed.empty()) {
                columns.push_back(trimmed);
            }
            current_column.clear();
        } else {
            current_column += c;
        }
    }
    
    std::string trimmed = trimColumn(current_column);
    if (!trimmed.empty()) {
        columns.push_back(trimmed);
    }
    
    return columns;
}

std::string DatabaseAbstractionLayer::trimColumn(const std::string& column) {
    if (column.empty()) return "";
    
    std::string result = column;
    result = std::regex_replace(result, std::regex(R"(^\s+|\s+$)"), "");
    
    if (result.empty()) return "";
    
    std::regex as_pattern(R"(\s+AS\s+(\w+))", std::regex_constants::icase);
    std::smatch as_match;
    if (std::regex_search(result, as_match, as_pattern)) {
        return as_match[1].str();
    }
    
    size_t dot_pos = result.find_last_of('.');
    if (dot_pos != std::string::npos && dot_pos < result.size() - 1) {
        result = result.substr(dot_pos + 1);
    }
    
    return result;
}

std::string DatabaseAbstractionLayer::extractTableNameFromQuery(const std::string& query) {
    try {
        std::string cleaned = preprocessQuery(query);
        std::regex from_pattern(R"(FROM\s+(\w+))", std::regex_constants::icase);
        std::smatch matches;
        
        if (std::regex_search(cleaned, matches, from_pattern)) {
            return matches[1].str();
        }
        
        return "";
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("extractTableNameFromQuery failed: " + std::string(e.what()));
        return "";
    }
}

std::vector<std::string> DatabaseAbstractionLayer::getTableColumnsFromSchema(const std::string& table_name) {
    try {
        std::string pragma_query = "PRAGMA table_info(" + table_name + ")";
        std::vector<std::vector<std::string>> pragma_results;
        
        bool success = db_manager_->executeQuery(pragma_query, pragma_results);
        if (!success || pragma_results.empty()) {
            LogManager::getInstance().Warn("Failed to get table schema for: " + table_name);
            return {};
        }
        
        std::vector<std::string> columns;
        for (const auto& row : pragma_results) {
            if (row.size() > 1) {
                columns.push_back(row[1]);
            }
        }
        
        LogManager::getInstance().Debug("Retrieved " + std::to_string(columns.size()) + 
                                       " columns from table schema: " + table_name);
        
        return columns;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("getTableColumnsFromSchema failed: " + std::string(e.what()));
        return {};
    }
}

// =============================================================================
// 🎯 CREATE TABLE 관련 메서드들
// =============================================================================

// DatabaseAbstractionLayer.cpp - executeCreateTable() 수정
bool DatabaseAbstractionLayer::executeCreateTable(const std::string& create_sql) {
    std::string table_name = extractTableNameFromCreateSQL(create_sql);
    if (table_name.empty()) {
        LogManager::getInstance().log("database", LogLevel::LOG_ERROR, 
            "테이블 이름을 추출할 수 없음: " + create_sql.substr(0, 100) + "...");
        return false;
    }
    
    // ✅ 추가: 테이블 존재 확인 전 로그
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "🔍 테이블 확인 중: " + table_name);
    
    if (doesTableExist(table_name)) {
        LogManager::getInstance().log("database", LogLevel::DEBUG, 
            "✅ 테이블 이미 존재: " + table_name);
        return true;
    }
    
    // ✅ 추가: 생성 시도 전 로그
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "📋 테이블 생성 실행: " + table_name);
    
    bool result = executeNonQuery(create_sql);
    
    // ✅ 추가: 결과 로그
    LogManager::getInstance().log("database", result ? LogLevel::INFO : LogLevel::LOG_ERROR, 
        result ? "✅ 테이블 생성 성공: " + table_name : "❌ 테이블 생성 실패: " + table_name);
    
    return result;
}

bool DatabaseAbstractionLayer::doesTableExist(const std::string& table_name) {
    try {
        std::string check_query = "SELECT name FROM sqlite_master WHERE type='table' AND name='" + table_name + "'";
        auto results = executeQuery(check_query);
        bool exists = !results.empty();
        
        LogManager::getInstance().log("database", LogLevel::DEBUG, 
            "🔍 테이블 '" + table_name + "' 존재 여부: " + (exists ? "존재" : "없음"));
        
        return exists;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("database", LogLevel::LOG_ERROR, 
            "테이블 존재 확인 실패: " + table_name + " - " + std::string(e.what()));
        return false;
    }
}

std::string DatabaseAbstractionLayer::extractTableNameFromCreateSQL(const std::string& create_sql) {
    std::regex table_regex(R"(CREATE\s+TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?(\w+))", 
                          std::regex_constants::icase);
    
    std::smatch matches;
    if (std::regex_search(create_sql, matches, table_regex)) {
        return matches[1].str();
    }
    
    return "";
}

// =============================================================================
// 🎯 유틸리티 메서드들
// =============================================================================

std::string DatabaseAbstractionLayer::getCurrentDbType() {
    if (!db_manager_) {
        return "SQLITE";
    }
    
    if (db_manager_->isSQLiteConnected()) {
        return "SQLITE";
    }
    
#ifdef HAS_POSTGRESQL
    else if (db_manager_->isPostgresConnected()) {
        return "POSTGRESQL";
    }
#endif

#ifdef HAS_MYSQL
    else if (db_manager_->isMySQLConnected()) {
        return "MYSQL";
    }
#endif

#ifdef HAS_MSSQL
    else if (db_manager_->isMSSQLConnected()) {
        return "MSSQL";
    }
#endif

    else {
        return "SQLITE";
    }
}

bool DatabaseAbstractionLayer::parseBoolean(const std::string& value) {
    if (value.empty()) return false;
    
    std::string lower_value = value;
    std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);
    
    return (lower_value == "1" || lower_value == "true" || lower_value == "yes" || lower_value == "on");
}

std::string DatabaseAbstractionLayer::formatBoolean(bool value) {
    std::string db_type = getCurrentDbType();
    
    if (db_type == "POSTGRESQL") {
        return value ? "true" : "false";
    } else {
        return value ? "1" : "0";
    }
}

std::string DatabaseAbstractionLayer::getCurrentTimestamp() {
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
        LogManager::getInstance().Error("DatabaseAbstractionLayer::executeUpsert failed: " + std::string(e.what()));
        return false;
    }
}

} // namespace Database
} // namespace PulseOne