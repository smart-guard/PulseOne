#ifndef PULSEONE_DATABASE_TYPES_H
#define PULSEONE_DATABASE_TYPES_H

/**
 * @file DatabaseTypes.h
 * @brief PulseOne Database 전용 타입 정의 - v2.0 타입 안전성 강화
 * @author PulseOne Development Team
 * @date 2025-10-22
 * @version 2.0.0
 * 
 * 주요 개선사항:
 * - QueryCondition에 ValueType 추가로 타입 안전성 확보
 * - SQL 인젝션 방어 강화
 * - 성능 최적화 (불필요한 따옴표 제거)
 * 
 * 저장 위치: core/shared/include/Database/DatabaseTypes.h
 */

#include <string>
#include <algorithm>
#include <sstream>
#include <vector>

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
     * @brief 쿼리 값의 타입
     */
    enum class ValueType {
        STRING,      // 문자열 - 따옴표 필요
        INTEGER,     // 정수 - 따옴표 불필요
        REAL,        // 실수 - 따옴표 불필요
        BOOLEAN,     // 불린 - DB별 변환 필요
        NULL_VALUE,  // NULL - 따옴표 불필요
        RAW          // 가공하지 않음 (IN 절 등)
    };

    /**
     * @brief SQL 쿼리 조건 구조체 (v2.0 - 타입 안전성 강화)
     * 
     * 변경사항:
     * - value_type 멤버 추가로 자동 타입 처리
     * - SQL 인젝션 방어 강화
     * - 성능 최적화
     */
    struct QueryCondition {
        std::string field;        // 필드명
        std::string operation;    // 연산자 (=, !=, >, <, >=, <=, LIKE, IN, BETWEEN)
        std::string value;        // 값 (문자열 표현)
        ValueType value_type;     // 🆕 값의 타입
        
        // 기본 생성자
        QueryCondition() 
            : field(""), operation("="), value(""), value_type(ValueType::STRING) {}
        
        // 기존 호환성 생성자 (문자열로 가정)
        QueryCondition(const std::string& f, const std::string& op, const std::string& v)
            : field(f), operation(op), value(v), value_type(ValueType::STRING) {}
        
        // 🆕 타입 명시 생성자 (권장)
        QueryCondition(const std::string& f, const std::string& op, 
                      const std::string& v, ValueType vt)
            : field(f), operation(op), value(v), value_type(vt) {}
        
        // =================================================================
        // 🆕 타입 안전 팩토리 메서드들
        // =================================================================
        
        // 문자열 조건
        static QueryCondition String(const std::string& field, 
                                    const std::string& op, 
                                    const std::string& value) {
            return QueryCondition(field, op, value, ValueType::STRING);
        }
        
        static QueryCondition Equal(const std::string& field, const std::string& value) {
            return String(field, "=", value);
        }
        
        static QueryCondition Like(const std::string& field, const std::string& value) {
            return String(field, "LIKE", value);
        }
        
        // 정수 조건
        static QueryCondition Integer(const std::string& field, 
                                     const std::string& op, 
                                     int value) {
            return QueryCondition(field, op, std::to_string(value), ValueType::INTEGER);
        }
        
        static QueryCondition EqualInt(const std::string& field, int value) {
            return Integer(field, "=", value);
        }
        
        // 실수 조건
        static QueryCondition Real(const std::string& field, 
                                  const std::string& op, 
                                  double value) {
            return QueryCondition(field, op, std::to_string(value), ValueType::REAL);
        }
        
        // 불린 조건
        static QueryCondition Boolean(const std::string& field, bool value) {
            return QueryCondition(field, "=", value ? "1" : "0", ValueType::BOOLEAN);
        }
        
        // IN 절 (Raw - 가공하지 않음)
        static QueryCondition In(const std::string& field, 
                                const std::vector<std::string>& values) {
            std::ostringstream oss;
            for (size_t i = 0; i < values.size(); ++i) {
                if (i > 0) oss << ",";
                oss << "'" << escapeString(values[i]) << "'";
            }
            return QueryCondition(field, "IN", oss.str(), ValueType::RAW);
        }
        
        static QueryCondition InInt(const std::string& field, 
                                   const std::vector<int>& values) {
            std::ostringstream oss;
            for (size_t i = 0; i < values.size(); ++i) {
                if (i > 0) oss << ",";
                oss << values[i];
            }
            return QueryCondition(field, "IN", oss.str(), ValueType::RAW);
        }
        
        // BETWEEN 절
        static QueryCondition Between(const std::string& field, 
                                     const std::string& start, 
                                     const std::string& end) {
            return QueryCondition(field, "BETWEEN", 
                                "'" + escapeString(start) + "' AND '" + escapeString(end) + "'",
                                ValueType::RAW);
        }
        
        // NULL 체크
        static QueryCondition IsNull(const std::string& field) {
            return QueryCondition(field, "IS", "NULL", ValueType::NULL_VALUE);
        }
        
        static QueryCondition IsNotNull(const std::string& field) {
            return QueryCondition(field, "IS NOT", "NULL", ValueType::NULL_VALUE);
        }
        
        // =================================================================
        // 🆕 SQL 생성 메서드 (타입에 맞게 자동 변환)
        // =================================================================
        
        /**
         * @brief SQL WHERE 절 조각 생성
         * @return "field op value" 형태의 문자열
         */
        std::string toSql() const {
            std::ostringstream sql;
            sql << field << " " << operation << " ";
            
            switch (value_type) {
                case ValueType::STRING:
                    sql << "'" << escapeString(value) << "'";
                    break;
                    
                case ValueType::INTEGER:
                case ValueType::REAL:
                    sql << value;
                    break;
                    
                case ValueType::BOOLEAN:
                    // DB별로 다르게 처리할 수 있음 (현재는 1/0)
                    sql << value;
                    break;
                    
                case ValueType::NULL_VALUE:
                    sql << value;  // "NULL" 그대로
                    break;
                    
                case ValueType::RAW:
                    if (operation == "IN") {
                        sql << "(" << value << ")";
                    } else {
                        sql << value;  // BETWEEN 등
                    }
                    break;
            }
            
            return sql.str();
        }
        
    private:
        /**
         * @brief SQL 인젝션 방어용 문자열 이스케이프
         */
        static std::string escapeString(const std::string& str) {
            std::string result;
            result.reserve(str.size());
            
            for (char c : str) {
                if (c == '\'') {
                    result += "''";  // SQL 표준 이스케이프
                } else if (c == '\\') {
                    result += "\\\\";
                } else {
                    result += c;
                }
            }
            
            return result;
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
        
        Pagination prevPage() const {
            int new_offset = (offset - limit > 0) ? (offset - limit) : 0;
            return Pagination(limit, new_offset);
        }
    };

} // namespace Database
} // namespace PulseOne

#endif // PULSEONE_DATABASE_TYPES_H