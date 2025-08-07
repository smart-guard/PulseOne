// =============================================================================
// collector/src/Database/Repositories/RepositoryHelpers.cpp
// 🔥 헬퍼 함수 구현부
// =============================================================================

#include "Database/Repositories/RepositoryHelpers.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// SQL 파라미터 치환 함수들
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
// 안전한 타입 변환
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

// =============================================================================
// 🔥 SQL 절 빌더 함수들 (중복 제거)
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
// SQL 문자열 처리
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

std::time_t RepositoryHelpers::parseTimestamp(const std::string& timestamp_str) {
    // 간단한 구현 - 실제로는 더 정교한 파싱 필요
    std::tm tm = {};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return std::mktime(&tm);
}

// =============================================================================
// 태그 처리 (DataPoint용)
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
        // 공백 제거
        tag.erase(0, tag.find_first_not_of(" \t\n\r"));
        tag.erase(tag.find_last_not_of(" \t\n\r") + 1);
        if (!tag.empty()) {
            tags.push_back(tag);
        }
    }
    return tags;
}

} // namespace Repositories
} // namespace Database  
} // namespace PulseOne