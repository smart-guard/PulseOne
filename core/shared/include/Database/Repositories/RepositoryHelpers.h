// =============================================================================
// collector/include/Database/Repositories/RepositoryHelpers.h
// v2.1 - detectValueType 선언 추가
// =============================================================================

#ifndef REPOSITORY_HELPERS_H
#define REPOSITORY_HELPERS_H

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <ctime>
#include <map>
#include <algorithm>
#include <algorithm>
#include "DatabaseTypes.hpp"

namespace PulseOne {
namespace Database {

    // Type Aliases for Backward Compatibility
    using ValueType = DbLib::ValueType;
    using QueryCondition = DbLib::QueryCondition;
    using OrderBy = DbLib::OrderBy;
    using Pagination = DbLib::Pagination;

namespace Repositories {

/**
 * @brief Repository 공통 헬퍼 함수들
 * 🔥 모든 Repository에서 중복 정의되던 함수들을 여기에 통합
 */
class RepositoryHelpers {
public:
    // =============================================================================
    // 🔥 SQL 파라미터 치환 함수들 (기존)
    // =============================================================================
    static std::string replaceParameter(std::string query, const std::string& value);
    static std::string replaceParameterWithQuotes(std::string query, const std::string& value);
    static std::string replaceTwoParameters(std::string query, const std::string& value1, const std::string& value2);

    // =============================================================================
    // 🔥 새로 추가: CurrentValueRepository용 파라미터 바인딩
    // =============================================================================
    
    static void replaceParameterPlaceholders(std::string& query, const std::vector<std::string>& values);
    static void replaceStringPlaceholder(std::string& query, const std::string& placeholder, const std::string& replacement);
    static std::string replaceParameterMarkers(std::string query, const std::vector<std::string>& params);
    static std::string replaceParametersInOrder(const std::string& query, const std::map<std::string, std::string>& params);

    // =============================================================================
    // 🆕 타입 감지 함수 (컴파일 에러 해결!)
    // =============================================================================
    
    static ValueType detectValueType(const std::string& value);

    // =============================================================================
    // 🔥 SQL 절 빌더 함수들 (기존)
    // =============================================================================
    static std::string buildWhereClause(const std::vector<QueryCondition>& conditions);
    static std::string buildWhereClauseWithAlias(const std::vector<QueryCondition>& conditions, const std::string& table_alias);
    static std::string buildOrderByClause(const std::optional<OrderBy>& order_by);
    static std::string buildLimitClause(const std::optional<Pagination>& pagination);

    // =============================================================================
    // 🔥 안전한 타입 변환 (기존 + 추가)
    // =============================================================================
    static std::string safeToString(int value);
    static std::string safeToString(double value);
    static std::string safeToString(bool value);
    static int safeParseInt(const std::string& value, int default_value = 0);
    static double safeParseDouble(const std::string& value, double default_value = 0.0);
    static bool safeParseBool(const std::string& value, bool default_value = false);

    // =============================================================================
    // 🔥 새로 추가: 행 데이터 안전 접근
    // =============================================================================
    
    static std::string getRowValue(const std::map<std::string, std::string>& row, 
                                  const std::string& column_name, 
                                  const std::string& default_value = "");
    static int getRowValueAsInt(const std::map<std::string, std::string>& row, 
                               const std::string& column_name, 
                               int default_value = 0);
    static double getRowValueAsDouble(const std::map<std::string, std::string>& row, 
                                     const std::string& column_name, 
                                     double default_value = 0.0);
    static bool getRowValueAsBool(const std::map<std::string, std::string>& row, 
                                 const std::string& column_name, 
                                 bool default_value = false);

    // =============================================================================
    // 🔥 SQL 문자열 처리 (기존 + 확장)
    // =============================================================================
    static std::string escapeString(const std::string& str);
    static std::string formatTimestamp(const std::time_t& timestamp);
    static std::string formatTimestamp(const std::chrono::system_clock::time_point& timestamp);
    static std::time_t parseTimestamp(const std::string& timestamp_str);

    // =============================================================================
    // 🔥 시간 변환 유틸리티 (기존)
    // =============================================================================
    static std::time_t timePointToTimeT(const std::chrono::system_clock::time_point& tp);
    static std::chrono::system_clock::time_point timeTToTimePoint(const std::time_t& tt);

    // =============================================================================
    // 🔥 태그 처리 (DataPoint용) (기존)
    // =============================================================================
    static std::string tagsToString(const std::vector<std::string>& tags);
    static std::vector<std::string> parseTagsFromString(const std::string& tags_str);

    // =============================================================================
    // 🔥 새로 추가: IN 절 생성 헬퍼
    // =============================================================================
    
    static std::string buildInClause(const std::vector<int>& ids);
    static std::string buildInClause(const std::vector<std::string>& values);

    // =============================================================================
    // 🔥 새로 추가: JSON 문자열 검증
    // =============================================================================
    
    static bool isValidJson(const std::string& json_str);
    static std::string escapeJsonString(const std::string& json_str);

    // =============================================================================
    // 🔥 새로 추가: 쿼리 성능 관련
    // =============================================================================
    
    static int sanitizeLimit(int limit, int max_limit = 1000);
    static int sanitizeOffset(int offset, int max_offset = 100000);

    // =============================================================================
    // 🔥🔥🔥 문자열 처리 유틸리티 (public으로 이동!)
    // =============================================================================

    static std::string trimString(const std::string& str);
    static std::string toLowerString(const std::string& str);
    static std::string join(const std::vector<std::string>& values, const std::string& delimiter);
    static std::string join(const std::vector<int>& values, const std::string& delimiter);
};

} // namespace Repositories
} // namespace Database  
} // namespace PulseOne

#endif // REPOSITORY_HELPERS_H