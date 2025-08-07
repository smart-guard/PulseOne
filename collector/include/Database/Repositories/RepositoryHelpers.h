// =============================================================================
// collector/include/Database/Repositories/RepositoryHelpers.h
// 🔥 중복 정의 문제 완전 해결: 공통 헬퍼 함수들
// =============================================================================

#ifndef REPOSITORY_HELPERS_H
#define REPOSITORY_HELPERS_H

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <ctime>

#include "Database/DatabaseTypes.h"


namespace PulseOne {
namespace Database {
namespace Repositories {

/**
 * @brief Repository 공통 헬퍼 함수들
 * 🔥 모든 Repository에서 중복 정의되던 함수들을 여기에 통합
 */
class RepositoryHelpers {
public:
    // SQL 파라미터 치환 함수들
    static std::string replaceParameter(std::string query, const std::string& value);
    static std::string replaceParameterWithQuotes(std::string query, const std::string& value);
    static std::string replaceTwoParameters(std::string query, const std::string& value1, const std::string& value2);
    
    // 🔥 SQL 절 빌더 함수들 (중복 제거)
    static std::string buildWhereClause(const std::vector<QueryCondition>& conditions);
    static std::string buildWhereClauseWithAlias(const std::vector<QueryCondition>& conditions, const std::string& table_alias);
    static std::string buildOrderByClause(const std::optional<OrderBy>& order_by);
    static std::string buildLimitClause(const std::optional<Pagination>& pagination);
    
    // 안전한 타입 변환
    static std::string safeToString(int value);
    static std::string safeToString(double value);
    static std::string safeToString(bool value);
    
    // SQL 문자열 처리
    static std::string escapeString(const std::string& str);
    static std::string formatTimestamp(const std::time_t& timestamp);
    static std::string formatTimestamp(const std::chrono::system_clock::time_point& timestamp);    
    static std::time_t parseTimestamp(const std::string& timestamp_str);
    
    // 태그 처리 (DataPoint용)
    static std::string tagsToString(const std::vector<std::string>& tags);
    static std::vector<std::string> parseTagsFromString(const std::string& tags_str);

    static std::time_t timePointToTimeT(const std::chrono::system_clock::time_point& tp);
    static std::chrono::system_clock::time_point timeTToTimePoint(const std::time_t& tt);
};

} // namespace Repositories
} // namespace Database  
} // namespace PulseOne

#endif // REPOSITORY_HELPERS_H