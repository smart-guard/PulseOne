// =============================================================================
// collector/include/Database/Repositories/RepositoryHelpers.h
// 🔥 완성본: 모든 Repository 공통 헬퍼 함수들 (에러 수정 완료)
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
    // =============================================================================
    // 🔥 SQL 파라미터 치환 함수들 (기존)
    // =============================================================================
    static std::string replaceParameter(std::string query, const std::string& value);
    static std::string replaceParameterWithQuotes(std::string query, const std::string& value);
    static std::string replaceTwoParameters(std::string query, const std::string& value1, const std::string& value2);

    // =============================================================================
    // 🔥 새로 추가: CurrentValueRepository용 파라미터 바인딩
    // =============================================================================
    
    /**
     * @brief 쿼리의 ? 플레이스홀더를 실제 값으로 교체
     * @param query 쿼리 문자열 (참조로 수정됨)
     * @param values 교체할 값들
     */
    static void replaceParameterPlaceholders(std::string& query, const std::vector<std::string>& values);

    /**
     * @brief 쿼리의 특정 플레이스홀더를 문자열로 교체
     * @param query 쿼리 문자열 (참조로 수정됨)
     * @param placeholder 찾을 플레이스홀더 (예: "%s")
     * @param replacement 교체할 문자열
     */
    static void replaceStringPlaceholder(std::string& query, const std::string& placeholder, const std::string& replacement);

    /**
     * @brief 쿼리의 ? 마커들을 순서대로 파라미터로 교체
     * @param query 원본 쿼리 문자열
     * @param params 교체할 파라미터 목록
     * @return 파라미터가 치환된 쿼리 문자열
     */
    static std::string replaceParameterMarkers(std::string query, const std::vector<std::string>& params);

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

    // 🔥 새로 추가: 안전한 파싱
    static int safeParseInt(const std::string& value, int default_value = 0);
    static double safeParseDouble(const std::string& value, double default_value = 0.0);
    static bool safeParseBool(const std::string& value, bool default_value = false);

    // =============================================================================
    // 🔥 새로 추가: 행 데이터 안전 접근
    // =============================================================================
    
    /**
     * @brief 행에서 안전하게 값 가져오기 (기본값 지원)
     * @param row 데이터 행
     * @param column_name 컬럼명
     * @param default_value 기본값 (컬럼이 없거나 비어있을 때)
     * @return 컬럼 값 또는 기본값
     */
    static std::string getRowValue(const std::map<std::string, std::string>& row, 
                                  const std::string& column_name, 
                                  const std::string& default_value = "");

    /**
     * @brief 행에서 정수 값 안전하게 가져오기
     */
    static int getRowValueAsInt(const std::map<std::string, std::string>& row, 
                               const std::string& column_name, 
                               int default_value = 0);

    /**
     * @brief 행에서 실수 값 안전하게 가져오기
     */
    static double getRowValueAsDouble(const std::map<std::string, std::string>& row, 
                                     const std::string& column_name, 
                                     double default_value = 0.0);

    /**
     * @brief 행에서 불린 값 안전하게 가져오기
     */
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
    
    /**
     * @brief 정수 배열을 IN 절 문자열로 변환
     * @param ids 정수 ID 목록
     * @return "1,2,3,4" 형태의 문자열
     */
    static std::string buildInClause(const std::vector<int>& ids);

    /**
     * @brief 문자열 배열을 IN 절 문자열로 변환
     * @param values 문자열 목록
     * @return "'a','b','c'" 형태의 문자열
     */
    static std::string buildInClause(const std::vector<std::string>& values);

    // =============================================================================
    // 🔥 새로 추가: JSON 문자열 검증
    // =============================================================================
    
    /**
     * @brief JSON 문자열 유효성 검사
     * @param json_str JSON 문자열
     * @return 유효하면 true
     */
    static bool isValidJson(const std::string& json_str);

    /**
     * @brief JSON 문자열을 안전하게 이스케이프
     * @param json_str JSON 문자열
     * @return 이스케이프된 JSON 문자열
     */
    static std::string escapeJsonString(const std::string& json_str);

    // =============================================================================
    // 🔥 새로 추가: 쿼리 성능 관련
    // =============================================================================
    
    /**
     * @brief LIMIT 절을 안전한 범위로 제한
     * @param limit 원하는 제한 수
     * @param max_limit 최대 허용 제한 수 (기본 1000)
     * @return 안전한 제한 수
     */
    static int sanitizeLimit(int limit, int max_limit = 1000);

    /**
     * @brief OFFSET을 안전한 범위로 제한
     * @param offset 원하는 오프셋
     * @param max_offset 최대 허용 오프셋 (기본 100000)
     * @return 안전한 오프셋
     */
    static int sanitizeOffset(int offset, int max_offset = 100000);

    // =============================================================================
    // 🔥🔥🔥 문자열 처리 유틸리티 (public으로 이동!)
    // =============================================================================

    /**
     * @brief 문자열 양쪽 공백 제거
     * @param str 원본 문자열
     * @return 공백이 제거된 문자열
     */
    static std::string trimString(const std::string& str);

    /**
     * @brief 문자열을 소문자로 변환
     * @param str 원본 문자열  
     * @return 소문자로 변환된 문자열
     */
    static std::string toLowerString(const std::string& str);

    /**
     * @brief 벡터의 문자열들을 구분자로 연결
     * @param values 연결할 문자열 벡터
     * @param delimiter 구분자
     * @return 연결된 문자열
     */
    static std::string join(const std::vector<std::string>& values, const std::string& delimiter);

    /**
     * @brief 벡터의 정수들을 구분자로 연결
     * @param values 연결할 정수 벡터  
     * @param delimiter 구분자
     * @return 연결된 문자열
     */
    static std::string join(const std::vector<int>& values, const std::string& delimiter);
};

} // namespace Repositories
} // namespace Database  
} // namespace PulseOne

#endif // REPOSITORY_HELPERS_H