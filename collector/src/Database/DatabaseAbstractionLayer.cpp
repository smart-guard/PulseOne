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
        LogManager::getInstance().Debug("Original: " + query.substr(0, 50) + "...");
        LogManager::getInstance().Debug("Adapted: " + adapted_query.substr(0, 50) + "...");
        
        // 2. 컬럼명 추출 (원본 쿼리에서)
        std::vector<std::string> column_names = extractColumnsFromQuery(query);
        if (column_names.empty()) {
            LogManager::getInstance().Error("DatabaseAbstractionLayer::executeQuery - Failed to extract column names");
            return {};
        }
        
        // 3. DatabaseManager를 통해 실제 실행
        std::vector<std::vector<std::string>> raw_results;
        bool success = db_manager_->executeQuery(adapted_query, raw_results);
        
        if (!success) {
            LogManager::getInstance().Error("DatabaseAbstractionLayer::executeQuery - Query execution failed");
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
    // SQL 파싱 로직 (이전과 동일)
    try {
        std::string lower_query = query;
        std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
        
        size_t select_pos = lower_query.find("select");
        size_t from_pos = lower_query.find("from");
        
        if (select_pos == std::string::npos || from_pos == std::string::npos || from_pos <= select_pos) {
            return {};
        }
        
        size_t start = select_pos + 6;
        std::string columns_section = query.substr(start, from_pos - start);
        
        // 간단한 콤마 분리 (실제로는 더 정교한 파싱 필요)
        std::vector<std::string> columns;
        std::stringstream ss(columns_section);
        std::string column;
        
        while (std::getline(ss, column, ',')) {
            // 공백 제거
            column.erase(0, column.find_first_not_of(" \t\n\r"));
            column.erase(column.find_last_not_of(" \t\n\r") + 1);
            
            // 테이블 프리픽스 제거
            size_t dot_pos = column.find_last_of('.');
            if (dot_pos != std::string::npos) {
                column = column.substr(dot_pos + 1);
            }
            
            if (!column.empty()) {
                columns.push_back(column);
            }
        }
        
        return columns;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DatabaseAbstractionLayer::extractColumnsFromQuery failed: " + std::string(e.what()));
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

} // namespace Database
} // namespace PulseOne