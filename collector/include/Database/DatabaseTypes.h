#ifndef PULSEONE_DATABASE_TYPES_H
#define PULSEONE_DATABASE_TYPES_H

/**
 * @file DatabaseTypes.h
 * @brief PulseOne Database 전용 타입 정의 - 매크로 충돌 해결
 * @author PulseOne Development Team
 * @date 2025-07-30
 */

#include <string>
#include <algorithm>

// 🔧 매크로 충돌 방지
#ifdef max
#undef max
#endif
#ifdef min  
#undef min
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

namespace PulseOne {
namespace Database {

    /**
     * @brief SQL 쿼리 조건 구조체
     */
    struct QueryCondition {
        std::string field;        // 필드명
        std::string operation;    // 연산자
        std::string value;        // 값
        
        QueryCondition() = default;
        
        QueryCondition(const std::string& f, const std::string& op, const std::string& v)
            : field(f), operation(op), value(v) {}
        
        // 편의 생성자들
        static QueryCondition Equal(const std::string& field, const std::string& value) {
            return QueryCondition(field, "=", value);
        }
        
        static QueryCondition Like(const std::string& field, const std::string& value) {
            return QueryCondition(field, "LIKE", value);
        }
        
        static QueryCondition In(const std::string& field, const std::string& values) {
            return QueryCondition(field, "IN", values);
        }
        
        static QueryCondition Greater(const std::string& field, const std::string& value) {
            return QueryCondition(field, ">", value);
        }
        
        static QueryCondition Less(const std::string& field, const std::string& value) {
            return QueryCondition(field, "<", value);
        }
    };
    
    /**
     * @brief SQL ORDER BY 절 구조체
     */
    struct OrderBy {
        std::string field;        // 정렬할 필드명
        bool ascending;           // true: ASC, false: DESC
        
        OrderBy() : field("id"), ascending(true) {}
        
        OrderBy(const std::string& f, bool asc = true)
            : field(f), ascending(asc) {}
        
        // 편의 생성자들
        static OrderBy Asc(const std::string& field) {
            return OrderBy(field, true);
        }
        
        static OrderBy Desc(const std::string& field) {
            return OrderBy(field, false);
        }
        
        // SQL 문자열 생성
        std::string toSql() const {
            return field + (ascending ? " ASC" : " DESC");
        }
    };
    
    /**
     * @brief 페이징 구조체 - 매크로 충돌 해결
     */
    struct Pagination {
        int limit;                // 한 페이지당 항목 수
        int offset;               // 시작 위치 (0부터)
        
        Pagination() : limit(100), offset(0) {}
        
        Pagination(int lim, int off = 0)
            : limit(lim), offset(off) {}
        
        // 페이지 번호로 생성 (1부터 시작)
        static Pagination FromPage(int page, int page_size) {
            return Pagination(page_size, (page - 1) * page_size);
        }
        
        // Getter 메서드들
        int getLimit() const { return limit; }
        int getOffset() const { return offset; }
        
        // 페이지 정보 계산
        int getPage() const { return (offset / limit) + 1; }
        int getPageSize() const { return limit; }
        
        // 다음/이전 페이지
        Pagination nextPage() const {
            return Pagination(limit, offset + limit);
        }
        
        // 🔧 수정: std::max 대신 삼항 연산자 사용
        Pagination prevPage() const {
            int new_offset = (offset - limit > 0) ? (offset - limit) : 0;
            return Pagination(limit, new_offset);
        }
    };

} // namespace Database
} // namespace PulseOne

#endif // PULSEONE_DATABASE_TYPES_H