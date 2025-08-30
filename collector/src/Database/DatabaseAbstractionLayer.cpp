// =============================================================================
// collector/src/Database/DatabaseAbstractionLayer.cpp
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
// 🎯 adaptQuery - 핵심! 표준 SQL을 DB 방언으로 변환
// =============================================================================

std::string DatabaseAbstractionLayer::adaptQuery(const std::string& query) {
    std::string adapted = query;
    
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
    
    return adapted;
}

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
            // MSSQL: MERGE 구문 사용 (가장 복잡함!)
            std::string first_column = columns.substr(0, columns.find(','));
            
            result = "MERGE " + table_name + " AS target USING (VALUES (" + values + ")) AS source (" + columns + ") " +
                    "ON target." + first_column + " = source." + first_column + " " +
                    "WHEN MATCHED THEN UPDATE SET ";
            
            // UPDATE 절 생성
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
        // 🔥 수정: 람다 함수 사용 대신 수동 처리
        // 1/0 → true/false (PostgreSQL은 native boolean 지원)
        std::regex bool_context("(is_enabled|is_active|enabled|active)\\s*=\\s*([01])", std::regex_constants::icase);
        
        // 🔥 수정: std::sregex_iterator를 사용한 수동 교체
        std::string temp_result;
        std::sregex_iterator iter(result.begin(), result.end(), bool_context);
        std::sregex_iterator end;
        
        size_t last_pos = 0;
        for (; iter != end; ++iter) {
            const std::smatch& match = *iter;
            
            // 매치 이전 부분 복사
            temp_result += result.substr(last_pos, match.position() - last_pos);
            
            // 매치된 부분 교체
            std::string field = match[1].str();
            std::string value = match[2].str();
            std::string replacement = field + " = " + (value == "1" ? "true" : "false");
            temp_result += replacement;
            
            last_pos = match.position() + match.length();
        }
        
        // 남은 부분 복사
        temp_result += result.substr(last_pos);
        result = temp_result;
        
    } else if (current_db_type_ == "MYSQL" || current_db_type_ == "MARIADB") {
        // MySQL은 TINYINT(1)을 boolean으로 사용, 0/1 그대로 유지
        // 필요시 true/false → 1/0 변환
        result = std::regex_replace(result, std::regex("\\btrue\\b", std::regex_constants::icase), "1");
        result = std::regex_replace(result, std::regex("\\bfalse\\b", std::regex_constants::icase), "0");
        
    } else if (current_db_type_ == "MSSQL" || current_db_type_ == "SQLSERVER") {
        // MSSQL: BIT 타입 사용, 0/1 또는 true/false 모두 지원하지만 0/1 권장
        result = std::regex_replace(result, std::regex("\\btrue\\b", std::regex_constants::icase), "1");
        result = std::regex_replace(result, std::regex("\\bfalse\\b", std::regex_constants::icase), "0");
    }
    
    return result;
}

std::string DatabaseAbstractionLayer::adaptTimestampFunctions(const std::string& query) {
    std::string result = query;
    
    if (current_db_type_ == "SQLITE") {
        // NOW() → datetime('now', 'localtime')
        result = std::regex_replace(result, std::regex("NOW\\(\\)", std::regex_constants::icase), 
                                   "datetime('now', 'localtime')");
        // CURRENT_TIMESTAMP → datetime('now', 'localtime')
        result = std::regex_replace(result, std::regex("CURRENT_TIMESTAMP", std::regex_constants::icase), 
                                   "datetime('now', 'localtime')");
        // GETDATE() (MSSQL) → datetime('now', 'localtime')
        result = std::regex_replace(result, std::regex("GETDATE\\(\\)", std::regex_constants::icase), 
                                   "datetime('now', 'localtime')");
                                   
    } else if (current_db_type_ == "POSTGRESQL") {
        // datetime('now', 'localtime') → NOW()
        result = std::regex_replace(result, std::regex("datetime\\('now',\\s*'localtime'\\)", std::regex_constants::icase), 
                                   "NOW()");
        // GETDATE() (MSSQL) → NOW()
        result = std::regex_replace(result, std::regex("GETDATE\\(\\)", std::regex_constants::icase), "NOW()");
        
    } else if (current_db_type_ == "MYSQL" || current_db_type_ == "MARIADB") {
        // datetime('now', 'localtime') → NOW()
        result = std::regex_replace(result, std::regex("datetime\\('now',\\s*'localtime'\\)", std::regex_constants::icase), 
                                   "NOW()");
        // GETDATE() (MSSQL) → NOW()
        result = std::regex_replace(result, std::regex("GETDATE\\(\\)", std::regex_constants::icase), "NOW()");
        
    } else if (current_db_type_ == "MSSQL" || current_db_type_ == "SQLSERVER") {
        // NOW() → GETDATE()
        result = std::regex_replace(result, std::regex("NOW\\(\\)", std::regex_constants::icase), "GETDATE()");
        // datetime('now', 'localtime') → GETDATE()
        result = std::regex_replace(result, std::regex("datetime\\('now',\\s*'localtime'\\)", std::regex_constants::icase), 
                                   "GETDATE()");
        // CURRENT_TIMESTAMP → GETDATE()
        result = std::regex_replace(result, std::regex("CURRENT_TIMESTAMP", std::regex_constants::icase), "GETDATE()");
    }
    
    return result;
}

std::string DatabaseAbstractionLayer::adaptLimitOffset(const std::string& query) {
    std::string result = query;
    
    // LIMIT x OFFSET y 구문 처리 (대부분 DB가 지원하지만 문법이 약간 다름)
    if (current_db_type_ == "MSSQL") {
        // SQL Server: OFFSET x ROWS FETCH NEXT y ROWS ONLY
        std::regex limit_offset("LIMIT\\s+(\\d+)\\s+OFFSET\\s+(\\d+)", std::regex_constants::icase);
        result = std::regex_replace(result, limit_offset, "OFFSET $2 ROWS FETCH NEXT $1 ROWS ONLY");
    }
    
    return result;
}

std::string DatabaseAbstractionLayer::adaptSpecialSyntax(const std::string& query) {
    std::string result = query;
    
    if (current_db_type_ == "SQLITE") {
        // ISNULL() → IFNULL()
        result = std::regex_replace(result, std::regex("ISNULL\\(", std::regex_constants::icase), "IFNULL(");
        // TOP n → LIMIT n
        result = std::regex_replace(result, std::regex("SELECT\\s+TOP\\s+(\\d+)", std::regex_constants::icase), 
                                   "SELECT");
        std::regex top_regex("TOP\\s+(\\d+)", std::regex_constants::icase);
        std::smatch top_match;
        if (std::regex_search(result, top_match, top_regex)) {
            result = std::regex_replace(result, top_regex, "");
            result += " LIMIT " + top_match[1].str();
        }
        
    } else if (current_db_type_ == "POSTGRESQL") {
        // IFNULL() → COALESCE()
        result = std::regex_replace(result, std::regex("IFNULL\\(", std::regex_constants::icase), "COALESCE(");
        // ISNULL() → COALESCE() (두 번째 파라미터 추가 필요)
        result = std::regex_replace(result, std::regex("ISNULL\\(([^,]+),([^)]+)\\)", std::regex_constants::icase), 
                                   "COALESCE($1, $2)");
        // TOP n → LIMIT n
        result = std::regex_replace(result, std::regex("SELECT\\s+TOP\\s+(\\d+)", std::regex_constants::icase), 
                                   "SELECT");
        std::regex top_regex("TOP\\s+(\\d+)", std::regex_constants::icase);
        std::smatch top_match;
        if (std::regex_search(result, top_match, top_regex)) {
            result = std::regex_replace(result, top_regex, "");
            result += " LIMIT " + top_match[1].str();
        }
        
    } else if (current_db_type_ == "MYSQL" || current_db_type_ == "MARIADB") {
        // ISNULL() → IFNULL() (MySQL은 IFNULL 사용)
        // TOP n → LIMIT n
        result = std::regex_replace(result, std::regex("SELECT\\s+TOP\\s+(\\d+)", std::regex_constants::icase), 
                                   "SELECT");
        std::regex top_regex("TOP\\s+(\\d+)", std::regex_constants::icase);
        std::smatch top_match;
        if (std::regex_search(result, top_match, top_regex)) {
            result = std::regex_replace(result, top_regex, "");
            result += " LIMIT " + top_match[1].str();
        }
        
    } else if (current_db_type_ == "MSSQL" || current_db_type_ == "SQLSERVER") {
        // IFNULL() → ISNULL()
        result = std::regex_replace(result, std::regex("IFNULL\\(", std::regex_constants::icase), "ISNULL(");
        // COALESCE() → ISNULL() (두 파라미터만 지원)
        result = std::regex_replace(result, std::regex("COALESCE\\(([^,]+),([^)]+)\\)", std::regex_constants::icase), 
                                   "ISNULL($1, $2)");
        // LIMIT n → TOP n (ORDER BY 이전에 위치해야 함)
        std::regex limit_regex("LIMIT\\s+(\\d+)", std::regex_constants::icase);
        std::smatch limit_match;
        if (std::regex_search(result, limit_match, limit_regex)) {
            result = std::regex_replace(result, limit_regex, "");
            result = std::regex_replace(result, std::regex("SELECT", std::regex_constants::icase), 
                                       "SELECT TOP " + limit_match[1].str());
        }
        
        // [] 대괄호로 컬럼명 감싸기 (예약어 충돌 방지)
        // 간단한 구현: 특정 예약어만 처리
        std::vector<std::string> mssql_keywords = {"order", "group", "key", "value", "table", "database"};
        for (const auto& keyword : mssql_keywords) {
            std::regex keyword_regex("\\b" + keyword + "\\b(?![.])", std::regex_constants::icase);
            result = std::regex_replace(result, keyword_regex, "[" + keyword + "]");
        }
    }
    
    return result;
}

// =============================================================================
// 🎯 executeNonQuery - 표준 SQL을 DB 방언으로 변환 후 실행
// =============================================================================

bool DatabaseAbstractionLayer::executeNonQuery(const std::string& query) {
    try {
        // 표준 SQL을 DB 방언으로 변환
        std::string adapted_query = adaptQuery(query);
        
        LogManager::getInstance().Debug("DatabaseAbstractionLayer::executeNonQuery - Adapted query");
        
        // DatabaseManager를 통해 실행
        return db_manager_->executeNonQuery(adapted_query);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DatabaseAbstractionLayer::executeNonQuery failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 🎯 기본 메서드들 (이전과 동일)
// =============================================================================

std::vector<std::string> DatabaseAbstractionLayer::extractColumnsFromQuery(const std::string& query) {
    try {
        LogManager::getInstance().Debug("DatabaseAbstractionLayer::extractColumnsFromQuery - Processing query");
        
        // 1. 전처리: 주석 제거 및 공백 정리
        std::string cleaned_query = preprocessQuery(query);
        LogManager::getInstance().Debug("Cleaned query: " + cleaned_query.substr(0, 200) + "...");
        
        // 2. SELECT와 FROM 구간 찾기 (dotall 제거)
        std::regex select_from_pattern(R"(SELECT\s+(.*?)\s+FROM)", std::regex_constants::icase);
        std::smatch matches;
        
        if (!std::regex_search(cleaned_query, matches, select_from_pattern)) {
            LogManager::getInstance().Error("No SELECT...FROM pattern found");
            return {};
        }
        
        std::string columns_section = matches[1].str();
        LogManager::getInstance().Debug("Columns section: " + columns_section);
        
        // 3. 컬럼들을 파싱
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

DatabaseAbstractionLayer::DatabaseAbstractionLayer() 
    : db_manager_(&DatabaseManager::getInstance()) {
    
    // 현재 DB 타입 확인
    current_db_type_ = ConfigManager::getInstance().getOrDefault("DATABASE_TYPE", "SQLITE");
    std::transform(current_db_type_.begin(), current_db_type_.end(), current_db_type_.begin(), ::toupper);
    
    LogManager::getInstance().Debug("DatabaseAbstractionLayer created for DB type: " + current_db_type_);
}

bool DatabaseAbstractionLayer::executeUpsert(
    const std::string& table_name,
    const std::map<std::string, std::string>& data,
    const std::vector<std::string>& primary_keys) {
    
    try {
        // 1. 컬럼명과 값 분리
        std::vector<std::string> columns;
        std::vector<std::string> values;
        
        for (const auto& [key, value] : data) {
            columns.push_back(key);
            values.push_back("'" + value + "'");
        }
        
        // 2. DB별 UPSERT 쿼리 생성
        std::string query;
        std::string db_type = getCurrentDbType();
        
        if (db_type == "SQLITE") {
            // SQLite: INSERT OR REPLACE
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
            // MySQL: INSERT ... ON DUPLICATE KEY UPDATE
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
                // 기본키가 아닌 컬럼만 업데이트
                bool is_primary = std::find(primary_keys.begin(), primary_keys.end(), columns[i]) != primary_keys.end();
                if (!is_primary) {
                    if (!first) query += ", ";
                    query += columns[i] + " = VALUES(" + columns[i] + ")";
                    first = false;
                }
            }
            
        } else {
            // 다른 DB는 일단 일반 INSERT로 처리
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
        
        // 3. 쿼리 실행
        return executeNonQuery(query);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DatabaseAbstractionLayer::executeUpsert failed: " + std::string(e.what()));
        return false;
    }
}
// 🔥 누락된 parseBoolean 구현
bool DatabaseAbstractionLayer::parseBoolean(const std::string& value) {
    if (value.empty()) return false;
    
    std::string lower_value = value;
    std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);
    
    return (lower_value == "1" || lower_value == "true" || lower_value == "yes" || lower_value == "on");
}

// 🔥 누락된 formatBoolean 구현
std::string DatabaseAbstractionLayer::formatBoolean(bool value) {
    std::string db_type = getCurrentDbType();
    
    if (db_type == "POSTGRESQL") {
        return value ? "true" : "false";
    } else {
        return value ? "1" : "0";  // SQLite, MySQL 등
    }
}

// 🔥 누락된 getCurrentTimestamp 구현
std::string DatabaseAbstractionLayer::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// 🔥 누락된 executeCreateTable 구현
bool DatabaseAbstractionLayer::executeCreateTable(const std::string& create_sql) {
    // 🚀 개선: 테이블이 이미 존재하면 CREATE 시도하지 않음
    
    // 1. 테이블 이름 추출
    std::string table_name = extractTableNameFromCreateSQL(create_sql);
    if (table_name.empty()) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "테이블 이름을 추출할 수 없음: " + create_sql.substr(0, 100) + "...");
        return false;
    }
    
    // 2. 테이블 존재 여부 확인
    if (doesTableExist(table_name)) {
        LogManager::getInstance().log("database", LogLevel::DEBUG, 
            "✅ 테이블 이미 존재: " + table_name);
        return true;  // 이미 존재하면 성공으로 처리
    }
    
    // 3. 테이블이 없으면 생성 시도
    LogManager::getInstance().log("database", LogLevel::INFO, 
        "📋 테이블 생성 시도: " + table_name);
    
    return executeNonQuery(create_sql);
}

bool DatabaseAbstractionLayer::doesTableExist(const std::string& table_name) {
    try {
        // SQLite 테이블 존재 확인 쿼리
        std::string check_query = "SELECT name FROM sqlite_master WHERE type='table' AND name='" + table_name + "'";
        
        auto results = executeQuery(check_query);
        
        bool exists = !results.empty();
        LogManager::getInstance().log("database", LogLevel::DEBUG, 
            "🔍 테이블 '" + table_name + "' 존재 여부: " + (exists ? "존재" : "없음"));
        
        return exists;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("database", LogLevel::ERROR, 
            "테이블 존재 확인 실패: " + table_name + " - " + std::string(e.what()));
        return false;
    }
}

std::string DatabaseAbstractionLayer::extractTableNameFromCreateSQL(const std::string& create_sql) {
    // CREATE TABLE IF NOT EXISTS table_name ... 에서 table_name 추출
    std::regex table_regex(R"(CREATE\s+TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?(\w+))", 
                          std::regex_constants::icase);
    
    std::smatch matches;
    if (std::regex_search(create_sql, matches, table_regex)) {
        return matches[1].str();
    }
    
    return "";
}

// 🔥 현재 DB 타입 반환 헬퍼
std::string DatabaseAbstractionLayer::getCurrentDbType() {
    if (!db_manager_) {
        return "SQLITE";  // 기본값
    }
    
    if (db_manager_->isSQLiteConnected()) {
        return "SQLITE";
    } else if (db_manager_->isPostgresConnected()) {
        return "POSTGRESQL";
    } else if (db_manager_->isRedisConnected()) {
        return "REDIS";
    } else {
        return "SQLITE";  // 기본값
    }
}

std::string DatabaseAbstractionLayer::preprocessQuery(const std::string& query) {
    std::string result = query;
    
    // 1. SQL 주석 제거 (-- 형태)
    result = std::regex_replace(result, std::regex(R"(--[^\r\n]*)"), "");
    
    // 2. C 스타일 주석 제거 (/* */ 형태) - dotall 제거하고 다른 방식 사용
    // 여러 줄 주석을 한 줄씩 처리
    std::string temp;
    std::istringstream stream(result);
    std::string line;
    bool in_multiline_comment = false;
    
    while (std::getline(stream, line)) {
        size_t comment_start = 0;
        while ((comment_start = line.find("/*", comment_start)) != std::string::npos) {
            size_t comment_end = line.find("*/", comment_start + 2);
            if (comment_end != std::string::npos) {
                // 같은 줄에서 주석이 끝남
                line.erase(comment_start, comment_end - comment_start + 2);
            } else {
                // 여러 줄 주석 시작
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
                continue; // 주석 안의 줄이므로 건너뜀
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
    
    // 1. 기본적인 콤마 분리 (괄호 안의 콤마는 무시)
    std::string current_column;
    int paren_depth = 0;
    bool in_quotes = false;
    char quote_char = 0;
    
    for (size_t i = 0; i < columns_section.size(); ++i) {
        char c = columns_section[i];
        
        // 따옴표 처리
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
            // 컬럼 구분자 발견
            std::string trimmed = trimColumn(current_column);
            if (!trimmed.empty()) {
                columns.push_back(trimmed);
            }
            current_column.clear();
        } else {
            current_column += c;
        }
    }
    
    // 마지막 컬럼 처리
    std::string trimmed = trimColumn(current_column);
    if (!trimmed.empty()) {
        columns.push_back(trimmed);
    }
    
    return columns;
}

std::string DatabaseAbstractionLayer::trimColumn(const std::string& column) {
    if (column.empty()) return "";
    
    std::string result = column;
    
    // 1. 앞뒤 공백 제거
    result = std::regex_replace(result, std::regex(R"(^\s+|\s+$)"), "");
    
    if (result.empty()) return "";
    
    // 2. AS 별칭 처리 (예: "device.name AS device_name" -> "device_name")
    std::regex as_pattern(R"(\s+AS\s+(\w+))", std::regex_constants::icase);
    std::smatch as_match;
    if (std::regex_search(result, as_match, as_pattern)) {
        return as_match[1].str();
    }
    
    // 3. 테이블 접두사 제거 (예: "devices.name" -> "name")
    size_t dot_pos = result.find_last_of('.');
    if (dot_pos != std::string::npos && dot_pos < result.size() - 1) {
        result = result.substr(dot_pos + 1);
    }
    
    return result;
}

std::string DatabaseAbstractionLayer::extractTableNameFromQuery(const std::string& query) {
    try {
        // FROM 절에서 테이블명 추출
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
        // SQLite PRAGMA 사용해서 컬럼 정보 조회
        std::string pragma_query = "PRAGMA table_info(" + table_name + ")";
        std::vector<std::vector<std::string>> pragma_results;
        
        bool success = db_manager_->executeQuery(pragma_query, pragma_results);
        if (!success || pragma_results.empty()) {
            LogManager::getInstance().Warn("Failed to get table schema for: " + table_name);
            return {};
        }
        
        std::vector<std::string> columns;
        for (const auto& row : pragma_results) {
            // PRAGMA table_info 결과: cid, name, type, notnull, dflt_value, pk
            // name은 인덱스 1에 위치
            if (row.size() > 1) {
                columns.push_back(row[1]); // column name
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

} // namespace Database
} // namespace PulseOne