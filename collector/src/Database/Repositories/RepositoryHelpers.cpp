// =============================================================================
// collector/src/Database/Repositories/RepositoryHelpers.cpp
// 🔥 완성본: 구현부
// =============================================================================

#include "Database/Repositories/RepositoryHelpers.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// 🔥 SQL 파라미터 치환 함수들 (기존)
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
// 🔥 새로 추가: CurrentValueRepository용 파라미터 바인딩
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
// 🔥 안전한 타입 변환 (기존 + 추가)
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
// 🔥 새로 추가: 행 데이터 안전 접근
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
// 🔥 SQL 절 빌더 함수들 (기존)
// =============================================================================

std::string RepositoryHelpers::buildWhereClause(const std::vector<QueryCondition>& conditions) {
    if (conditions.empty()) {
        return "";
    }
    
    std::string clause = " WHERE ";
    for (size_t i = 0; i < conditions.size(); ++i) {
        if (i > 0) clause += " AND ";
        clause += conditions[i].field + " " + conditions[i].operation + " " + conditions[i].value;
    }
    return clause;
}

std::string RepositoryHelpers::buildWhereClauseWithAlias(const std::vector<QueryCondition>& conditions, const std::string& table_alias) {
    if (conditions.empty()) {
        return "";
    }
    
    std::string clause = " WHERE ";
    for (size_t i = 0; i < conditions.size(); ++i) {
        if (i > 0) clause += " AND ";
        
        if (!table_alias.empty()) {
            clause += table_alias + "." + conditions[i].field;
        } else {
            clause += conditions[i].field;
        }
        clause += " " + conditions[i].operation + " " + conditions[i].value;
    }
    return clause;
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
// 🔥 SQL 문자열 처리 (기존)
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
    // 간단한 구현 - 실제로는 더 정교한 파싱 필요
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
// 🔥 태그 처리 (DataPoint용) (기존)
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
// 🔥 새로 추가: IN 절 생성 헬퍼
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
// 🔥 새로 추가: JSON 문자열 검증
// =============================================================================

bool RepositoryHelpers::isValidJson(const std::string& json_str) {
    if (json_str.empty()) return false;
    
    // 간단한 JSON 형태 검증 (실제로는 JSON 라이브러리 사용 권장)
    std::string trimmed = trimString(json_str);
    return (trimmed.front() == '{' && trimmed.back() == '}') ||
           (trimmed.front() == '[' && trimmed.back() == ']');
}

std::string RepositoryHelpers::escapeJsonString(const std::string& json_str) {
    std::string escaped = json_str;
    
    // JSON 특수 문자 이스케이프
    std::vector<std::pair<std::string, std::string>> replacements = {
        {"\\", "\\\\"},  // 백슬래시
        {"\"", "\\\""},  // 따옴표
        {"\n", "\\n"},   // 개행
        {"\r", "\\r"},   // 캐리지 리턴
        {"\t", "\\t"}    // 탭
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
// 🔥 새로 추가: 쿼리 성능 관련
// =============================================================================

int RepositoryHelpers::sanitizeLimit(int limit, int max_limit) {
    if (limit <= 0) return 100;  // 기본값
    return std::min(limit, max_limit);
}

int RepositoryHelpers::sanitizeOffset(int offset, int max_offset) {
    if (offset < 0) return 0;
    return std::min(offset, max_offset);
}

// =============================================================================
// 🔥 내부 헬퍼 함수들
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

} // namespace Repositories
} // namespace Database  
} // namespace PulseOne