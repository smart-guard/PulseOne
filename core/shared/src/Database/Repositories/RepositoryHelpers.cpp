// =============================================================================
// core/shared/src/Database/Repositories/RepositoryHelpers.cpp
// v2.0 - 원본 기능 100% 유지 + 타입 안전성 강화
// =============================================================================

#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/DatabaseTypes.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <cctype>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// 🔥 SQL 파라미터 치환 함수들 (원본 유지)
// =============================================================================

std::string RepositoryHelpers::replaceParameter(std::string query, const std::string& value) {
    size_t pos = query.find('?');
    if (pos != std::string::npos) {
        query.replace(pos, 1, value);
    }
    return query;
}

std::string RepositoryHelpers::replaceParameterWithQuotes(std::string query, const std::string& value) {
    size_t pos = query.find('?');
    if (pos != std::string::npos) {
        query.replace(pos, 1, "'" + escapeString(value) + "'");
    }
    return query;
}

std::string RepositoryHelpers::replaceTwoParameters(std::string query, const std::string& value1, const std::string& value2) {
    size_t pos = query.find('?');
    if (pos != std::string::npos) {
        query.replace(pos, 1, value1);
    }
    pos = query.find('?');
    if (pos != std::string::npos) {
        query.replace(pos, 1, value2);
    }
    return query;
}

// =============================================================================
// 🔥 CurrentValueRepository용 파라미터 바인딩 (원본 유지)
// =============================================================================

void RepositoryHelpers::replaceParameterPlaceholders(std::string& query, const std::vector<std::string>& values) {
    size_t pos = 0;
    for (const auto& value : values) {
        pos = query.find('?', pos);
        if (pos != std::string::npos) {
            // SQL 인젝션 방지를 위한 기본적인 이스케이핑
            std::string escaped_value = "'" + escapeString(value) + "'";
            query.replace(pos, 1, escaped_value);
            pos += escaped_value.length();
        }
    }
}

void RepositoryHelpers::replaceStringPlaceholder(std::string& query, const std::string& placeholder, const std::string& replacement) {
    size_t pos = query.find(placeholder);
    if (pos != std::string::npos) {
        query.replace(pos, placeholder.length(), replacement);
    }
}

// =============================================================================
// 🔥 안전한 타입 변환 (원본 유지)
// =============================================================================

std::string RepositoryHelpers::safeToString(int value) {
    return std::to_string(value);
}

std::string RepositoryHelpers::safeToString(double value) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6) << value;
    return ss.str();
}

std::string RepositoryHelpers::safeToString(bool value) {
    return value ? "1" : "0";
}

int RepositoryHelpers::safeParseInt(const std::string& value, int default_value) {
    try {
        if (value.empty()) return default_value;
        return std::stoi(trimString(value));
    } catch (...) {
        return default_value;
    }
}

double RepositoryHelpers::safeParseDouble(const std::string& value, double default_value) {
    try {
        if (value.empty()) return default_value;
        return std::stod(trimString(value));
    } catch (...) {
        return default_value;
    }
}

bool RepositoryHelpers::safeParseBool(const std::string& value, bool default_value) {
    if (value.empty()) return default_value;
    
    std::string lower_value = toLowerString(trimString(value));
    return (lower_value == "1" || lower_value == "true" || lower_value == "yes" || lower_value == "on");
}

// =============================================================================
// 🔥 행 데이터 안전 접근 (원본 유지)
// =============================================================================

std::string RepositoryHelpers::getRowValue(const std::map<std::string, std::string>& row, 
                                          const std::string& column_name, 
                                          const std::string& default_value) {
    auto it = row.find(column_name);
    if (it != row.end() && !it->second.empty()) {
        return it->second;
    }
    return default_value;
}

int RepositoryHelpers::getRowValueAsInt(const std::map<std::string, std::string>& row, 
                                       const std::string& column_name, 
                                       int default_value) {
    std::string value = getRowValue(row, column_name);
    return safeParseInt(value, default_value);
}

double RepositoryHelpers::getRowValueAsDouble(const std::map<std::string, std::string>& row, 
                                             const std::string& column_name, 
                                             double default_value) {
    std::string value = getRowValue(row, column_name);
    return safeParseDouble(value, default_value);
}

bool RepositoryHelpers::getRowValueAsBool(const std::map<std::string, std::string>& row, 
                                         const std::string& column_name, 
                                         bool default_value) {
    std::string value = getRowValue(row, column_name);
    return safeParseBool(value, default_value);
}

// =============================================================================
// 🆕 타입 감지 (자동 타입 추론)
// =============================================================================

ValueType RepositoryHelpers::detectValueType(const std::string& value) {
    if (value.empty()) {
        return ValueType::STRING;
    }
    
    // NULL 체크
    std::string upper_value = value;
    std::transform(upper_value.begin(), upper_value.end(), upper_value.begin(), ::toupper);
    if (upper_value == "NULL") {
        return ValueType::NULL_VALUE;
    }
    
    // 불린 체크
    if (upper_value == "TRUE" || upper_value == "FALSE" ||
        value == "1" || value == "0") {
        return ValueType::BOOLEAN;
    }
    
    // 정수 체크
    bool is_integer = true;
    size_t start = 0;
    if (value[0] == '-' || value[0] == '+') {
        start = 1;
    }
    
    for (size_t i = start; i < value.size(); ++i) {
        if (!std::isdigit(value[i])) {
            is_integer = false;
            break;
        }
    }
    
    if (is_integer && value.size() > start) {
        return ValueType::INTEGER;
    }
    
    // 실수 체크
    bool has_dot = false;
    bool is_real = true;
    start = 0;
    if (value[0] == '-' || value[0] == '+') {
        start = 1;
    }
    
    for (size_t i = start; i < value.size(); ++i) {
        if (value[i] == '.') {
            if (has_dot) {
                is_real = false;
                break;
            }
            has_dot = true;
        } else if (!std::isdigit(value[i])) {
            is_real = false;
            break;
        }
    }
    
    if (is_real && has_dot) {
        return ValueType::REAL;
    }
    
    return ValueType::STRING;
}

// =============================================================================
// 🔥 SQL 절 빌더 - v2.0 타입 안전 버전
// =============================================================================

std::string RepositoryHelpers::buildWhereClause(const std::vector<QueryCondition>& conditions) {
    if (conditions.empty()) {
        return "";
    }
    
    std::ostringstream where_clause;
    where_clause << " WHERE ";
    
    for (size_t i = 0; i < conditions.size(); ++i) {
        if (i > 0) {
            where_clause << " AND ";
        }
        
        const auto& condition = conditions[i];
        where_clause << condition.field << " " << condition.operation << " ";
        
        // 🔥 핵심: 타입별 처리
        std::string op_upper = condition.operation;
        std::transform(op_upper.begin(), op_upper.end(), op_upper.begin(), ::toupper);
        
        if (op_upper == "IN") {
            // IN 절: 괄호 처리
            if (condition.value.find('(') == std::string::npos) {
                where_clause << "(" << condition.value << ")";
            } else {
                where_clause << condition.value;
            }
        } else if (op_upper == "BETWEEN") {
            // BETWEEN: value에 "val1 AND val2" 형태
            where_clause << condition.value;
        } else if (op_upper == "IS" || op_upper == "IS NOT") {
            // IS NULL, IS NOT NULL
            where_clause << condition.value;
        } else {
            // 일반 연산자: 타입 감지 후 처리
            ValueType detected_type = detectValueType(condition.value);
            
            if (detected_type == ValueType::STRING) {
                where_clause << "'" << escapeString(condition.value) << "'";
            } else if (detected_type == ValueType::NULL_VALUE) {
                where_clause << "NULL";
            } else {
                // INTEGER, REAL, BOOLEAN: 따옴표 없이
                where_clause << condition.value;
            }
        }
    }
    
    return where_clause.str();
}

std::string RepositoryHelpers::buildWhereClauseWithAlias(const std::vector<QueryCondition>& conditions, 
                                                         const std::string& table_alias) {
    if (conditions.empty()) {
        return "";
    }
    
    std::ostringstream where_clause;
    where_clause << " WHERE ";
    
    for (size_t i = 0; i < conditions.size(); ++i) {
        if (i > 0) {
            where_clause << " AND ";
        }
        
        const auto& condition = conditions[i];
        
        // 테이블 별칭 추가
        if (!table_alias.empty()) {
            where_clause << table_alias << ".";
        }
        
        where_clause << condition.field << " " << condition.operation << " ";
        
        // 나머지는 buildWhereClause와 동일
        std::string op_upper = condition.operation;
        std::transform(op_upper.begin(), op_upper.end(), op_upper.begin(), ::toupper);
        
        if (op_upper == "IN") {
            if (condition.value.find('(') == std::string::npos) {
                where_clause << "(" << condition.value << ")";
            } else {
                where_clause << condition.value;
            }
        } else if (op_upper == "BETWEEN") {
            where_clause << condition.value;
        } else if (op_upper == "IS" || op_upper == "IS NOT") {
            where_clause << condition.value;
        } else {
            ValueType detected_type = detectValueType(condition.value);
            
            if (detected_type == ValueType::STRING) {
                where_clause << "'" << escapeString(condition.value) << "'";
            } else if (detected_type == ValueType::NULL_VALUE) {
                where_clause << "NULL";
            } else {
                where_clause << condition.value;
            }
        }
    }
    
    return where_clause.str();
}

std::string RepositoryHelpers::buildOrderByClause(const std::optional<OrderBy>& order_by) {
    if (!order_by.has_value()) {
        return "";
    }
    return " ORDER BY " + order_by->field + (order_by->ascending ? " ASC" : " DESC");
}

std::string RepositoryHelpers::buildLimitClause(const std::optional<Pagination>& pagination) {
    if (!pagination.has_value()) {
        return "";
    }
    return " LIMIT " + std::to_string(pagination->getLimit()) + 
           " OFFSET " + std::to_string(pagination->getOffset());
}

// =============================================================================
// 🔥 SQL 문자열 처리 (원본 유지)
// =============================================================================

std::string RepositoryHelpers::escapeString(const std::string& str) {
    std::string escaped = str;
    size_t pos = 0;
    while ((pos = escaped.find("'", pos)) != std::string::npos) {
        escaped.replace(pos, 1, "''");
        pos += 2;
    }
    return escaped;
}

std::string RepositoryHelpers::formatTimestamp(const std::time_t& timestamp) {
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&timestamp), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string RepositoryHelpers::formatTimestamp(const std::chrono::system_clock::time_point& timestamp) {
    std::time_t time_t_val = std::chrono::system_clock::to_time_t(timestamp);
    return formatTimestamp(time_t_val);
}

std::time_t RepositoryHelpers::parseTimestamp(const std::string& timestamp_str) {
    std::tm tm = {};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return std::mktime(&tm);
}

std::time_t RepositoryHelpers::timePointToTimeT(const std::chrono::system_clock::time_point& tp) {
    return std::chrono::system_clock::to_time_t(tp);
}

std::chrono::system_clock::time_point RepositoryHelpers::timeTToTimePoint(const std::time_t& tt) {
    return std::chrono::system_clock::from_time_t(tt);
}

// =============================================================================
// 🔥 태그 처리 (원본 유지)
// =============================================================================

std::string RepositoryHelpers::tagsToString(const std::vector<std::string>& tags) {
    if (tags.empty()) {
        return "";
    }
    
    std::ostringstream ss;
    for (size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) ss << ",";
        ss << tags[i];
    }
    return ss.str();
}

std::vector<std::string> RepositoryHelpers::parseTagsFromString(const std::string& tags_str) {
    std::vector<std::string> tags;
    if (tags_str.empty()) {
        return tags;
    }
    
    std::stringstream ss(tags_str);
    std::string tag;
    while (std::getline(ss, tag, ',')) {
        tag = trimString(tag);
        if (!tag.empty()) {
            tags.push_back(tag);
        }
    }
    return tags;
}

// =============================================================================
// 🔥 IN 절 생성 헬퍼 (원본 유지)
// =============================================================================

std::string RepositoryHelpers::buildInClause(const std::vector<int>& ids) {
    if (ids.empty()) return "";
    
    std::ostringstream ss;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << ids[i];
    }
    return ss.str();
}

std::string RepositoryHelpers::buildInClause(const std::vector<std::string>& values) {
    if (values.empty()) return "";
    
    std::ostringstream ss;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "'" << escapeString(values[i]) << "'";
    }
    return ss.str();
}

// =============================================================================
// 🔥 JSON 문자열 검증 (원본 유지)
// =============================================================================

bool RepositoryHelpers::isValidJson(const std::string& json_str) {
    if (json_str.empty()) return false;
    
    std::string trimmed = trimString(json_str);
    return (trimmed.front() == '{' && trimmed.back() == '}') ||
           (trimmed.front() == '[' && trimmed.back() == ']');
}

std::string RepositoryHelpers::escapeJsonString(const std::string& json_str) {
    std::string escaped = json_str;
    
    std::vector<std::pair<std::string, std::string>> replacements = {
        {"\\", "\\\\"},
        {"\"", "\\\""},
        {"\n", "\\n"},
        {"\r", "\\r"},
        {"\t", "\\t"}
    };
    
    for (const auto& replacement : replacements) {
        size_t pos = 0;
        while ((pos = escaped.find(replacement.first, pos)) != std::string::npos) {
            escaped.replace(pos, replacement.first.length(), replacement.second);
            pos += replacement.second.length();
        }
    }
    
    return escaped;
}

// =============================================================================
// 🔥 쿼리 성능 관련 (원본 유지)
// =============================================================================

int RepositoryHelpers::sanitizeLimit(int limit, int max_limit) {
    if (limit <= 0) return 100;
    return std::min(limit, max_limit);
}

int RepositoryHelpers::sanitizeOffset(int offset, int max_offset) {
    if (offset < 0) return 0;
    return std::min(offset, max_offset);
}

// =============================================================================
// 🔥 내부 헬퍼 함수들 (원본 유지)
// =============================================================================

std::string RepositoryHelpers::trimString(const std::string& str) {
    if (str.empty()) return str;
    
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::string RepositoryHelpers::toLowerString(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string RepositoryHelpers::join(const std::vector<std::string>& values, const std::string& delimiter) {
    if (values.empty()) {
        return "";
    }
    
    std::ostringstream ss;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            ss << delimiter;
        }
        ss << values[i];
    }
    return ss.str();
}

std::string RepositoryHelpers::join(const std::vector<int>& values, const std::string& delimiter) {
    if (values.empty()) {
        return "";
    }
    
    std::ostringstream ss;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            ss << delimiter;
        }
        ss << values[i];
    }
    return ss.str();
}

std::string RepositoryHelpers::replaceParameterMarkers(std::string query, const std::vector<std::string>& params) {
    for (const auto& param : params) {
        size_t pos = query.find('?');
        if (pos != std::string::npos) {
            query.replace(pos, 1, param);
        }
    }
    return query;
}

std::string RepositoryHelpers::replaceParametersInOrder(const std::string& query, 
                                                       const std::map<std::string, std::string>& params) {
    std::string result = query;
    
    for (const auto& pair : params) {
        size_t pos = 0;
        std::string placeholder = "{" + pair.first + "}";
        
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), pair.second);
            pos += pair.second.length();
        }
    }
    
    return result;
}

} // namespace Repositories
} // namespace Database  
} // namespace PulseOne