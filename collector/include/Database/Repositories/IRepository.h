#ifndef PULSEONE_IREPOSITORY_H
#define PULSEONE_IREPOSITORY_H

/**
 * @file IRepository.h
 * @brief PulseOne Repository 인터페이스 템플릿
 * @author PulseOne Development Team
 * @date 2025-07-26
 * 
 * Repository 패턴 구현:
 * - 공통 CRUD 연산
 * - 벌크 연산 지원
 * - 캐싱 기능
 * - N+1 문제 해결
 * - DatabaseManager 통합 사용
 */

#include "Common/UnifiedCommonTypes.h"
#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include <vector>
#include <optional>
#include <map>
#include <memory>
#include <string>
#include <functional>
#include <functional>

namespace PulseOne {
namespace Database {

/**
 * @brief 쿼리 조건 구조체
 */
struct QueryCondition {
    std::string field;
    std::string operation; // "=", "!=", ">", "<", ">=", "<=", "LIKE", "IN"
    std::string value;
    
    QueryCondition(const std::string& f, const std::string& op, const std::string& v)
        : field(f), operation(op), value(v) {}
};

/**
 * @brief 정렬 조건 구조체
 */
struct OrderBy {
    std::string field;
    bool ascending;
    
    OrderBy(const std::string& f, bool asc = true) : field(f), ascending(asc) {}
};

/**
 * @brief 페이징 정보 구조체
 */
struct Pagination {
    int page;
    int size;
    
    Pagination(int p = 1, int s = 50) : page(p), size(s) {}
    
    int getOffset() const { return (page - 1) * size; }
    int getLimit() const { return size; }
};

/**
 * @brief Repository 인터페이스 템플릿
 * @tparam EntityType 엔티티 타입 (DeviceEntity, DataPointEntity 등)
 */
template<typename EntityType>
class IRepository {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~IRepository() = default;

    // =======================================================================
    // 기본 CRUD 연산
    // =======================================================================
    
    /**
     * @brief 모든 엔티티 조회
     * @return 엔티티 목록
     */
    virtual std::vector<EntityType> findAll() = 0;
    
    /**
     * @brief ID로 엔티티 조회
     * @param id 엔티티 ID
     * @return 엔티티 (없으면 nullopt)
     */
    virtual std::optional<EntityType> findById(int id) = 0;
    
    /**
     * @brief 엔티티 저장
     * @param entity 저장할 엔티티 (참조로 전달하여 ID 업데이트)
     * @return 성공 시 true
     */
    virtual bool save(EntityType& entity) = 0;
    
    /**
     * @brief 엔티티 업데이트
     * @param entity 업데이트할 엔티티
     * @return 성공 시 true
     */
    virtual bool update(const EntityType& entity) = 0;
    
    /**
     * @brief ID로 엔티티 삭제
     * @param id 삭제할 엔티티 ID
     * @return 성공 시 true
     */
    virtual bool deleteById(int id) = 0;
    
    /**
     * @brief 엔티티 삭제
     * @param entity 삭제할 엔티티
     * @return 성공 시 true
     */
    virtual bool deleteEntity(const EntityType& entity) {
        return deleteById(entity.getId());
    }

    // =======================================================================
    // 벌크 연산 (성능 최적화)
    // =======================================================================
    
    /**
     * @brief 여러 ID로 엔티티들 조회
     * @param ids ID 목록
     * @return 엔티티 목록
     */
    virtual std::vector<EntityType> findByIds(const std::vector<int>& ids) = 0;
    
    /**
     * @brief 여러 엔티티 일괄 저장
     * @param entities 저장할 엔티티들 (참조로 전달하여 ID 업데이트)
     * @return 저장된 엔티티 수
     */
    virtual int saveBulk(std::vector<EntityType>& entities) = 0;
    
    /**
     * @brief 여러 엔티티 일괄 업데이트
     * @param entities 업데이트할 엔티티들
     * @return 업데이트된 엔티티 수
     */
    virtual int updateBulk(const std::vector<EntityType>& entities) = 0;
    
    /**
     * @brief 여러 ID 일괄 삭제
     * @param ids 삭제할 ID들
     * @return 삭제된 엔티티 수
     */
    virtual int deleteByIds(const std::vector<int>& ids) = 0;

    // =======================================================================
    // 조건부 조회
    // =======================================================================
    
    /**
     * @brief 조건으로 엔티티 조회
     * @param conditions 쿼리 조건들
     * @param order_by 정렬 조건 (선택사항)
     * @param pagination 페이징 정보 (선택사항)
     * @return 조건에 맞는 엔티티 목록
     */
    virtual std::vector<EntityType> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt) = 0;
    
    /**
     * @brief 단일 조건으로 엔티티 조회
     * @param field 필드명
     * @param operation 연산자
     * @param value 값
     * @return 조건에 맞는 엔티티 목록
     */
    virtual std::vector<EntityType> findBy(const std::string& field, 
                                          const std::string& operation, 
                                          const std::string& value) {
        return findByConditions({QueryCondition(field, operation, value)});
    }
    
    /**
     * @brief 조건으로 첫 번째 엔티티 조회
     * @param conditions 쿼리 조건들
     * @return 첫 번째 엔티티 (없으면 nullopt)
     */
    virtual std::optional<EntityType> findFirstByConditions(
        const std::vector<QueryCondition>& conditions) {
        auto results = findByConditions(conditions, std::nullopt, Pagination(1, 1));
        return results.empty() ? std::nullopt : std::make_optional(results[0]);
    }
    
    /**
     * @brief 조건에 맞는 엔티티 개수 조회
     * @param conditions 쿼리 조건들
     * @return 엔티티 개수
     */
    virtual int countByConditions(const std::vector<QueryCondition>& conditions) = 0;

    // =======================================================================
    // 캐싱 관리
    // =======================================================================
    
    /**
     * @brief 캐시 활성화/비활성화
     * @param enabled 캐시 사용 여부
     */
    virtual void setCacheEnabled(bool enabled) = 0;
    
    /**
     * @brief 캐시 상태 조회
     * @return 캐시 활성화 여부
     */
    virtual bool isCacheEnabled() const = 0;
    
    /**
     * @brief 모든 캐시 삭제
     */
    virtual void clearCache() = 0;
    
    /**
     * @brief 특정 엔티티 캐시 삭제
     * @param id 엔티티 ID
     */
    virtual void clearCacheForId(int id) = 0;
    
    /**
     * @brief 캐시 통계 조회
     * @return 캐시 통계 (hits, misses, size 등)
     */
    virtual std::map<std::string, int> getCacheStats() const = 0;

    // =======================================================================
    // 관계 데이터 로딩 (N+1 문제 해결)
    // =======================================================================
    
    /**
     * @brief 관계 데이터 사전 로딩
     * @param entities 엔티티들
     * @param relation_name 관계명
     */
    virtual void preloadRelation(std::vector<EntityType>& entities, 
                                const std::string& relation_name) {
        // 기본 구현은 비어둠 (파생 클래스에서 필요시 오버라이드)
        (void)entities;
        (void)relation_name;
    }

    // =======================================================================
    // 통계 및 유틸리티
    // =======================================================================
    
    /**
     * @brief 전체 엔티티 개수 조회
     * @return 전체 엔티티 개수
     */
    virtual int getTotalCount() = 0;
    
    /**
     * @brief Repository가 비어있는지 확인
     * @return 비어있으면 true
     */
    virtual bool isEmpty() {
        return getTotalCount() == 0;
    }
    
    /**
     * @brief 특정 ID가 존재하는지 확인
     * @param id 확인할 ID
     * @return 존재하면 true
     */
    virtual bool exists(int id) {
        return findById(id).has_value();
    }
    
    /**
     * @brief Repository 이름 조회 (디버깅용)
     * @return Repository 이름
     */
    virtual std::string getRepositoryName() const = 0;

    // =======================================================================
    // 트랜잭션 지원 (선택사항)
    // =======================================================================
    
    /**
     * @brief 트랜잭션 시작
     * @return 성공 시 true
     */
    virtual bool beginTransaction() {
        // 기본 구현은 비어둠 (파생 클래스에서 필요시 구현)
        return true;
    }
    
    /**
     * @brief 트랜잭션 커밋
     * @return 성공 시 true
     */
    virtual bool commitTransaction() {
        // 기본 구현은 비어둠
        return true;
    }
    
    /**
     * @brief 트랜잭션 롤백
     * @return 성공 시 true
     */
    virtual bool rollbackTransaction() {
        // 기본 구현은 비어둠
        return true;
    }
    
    /**
     * @brief 트랜잭션 내에서 작업 실행
     * @param work 실행할 작업 (람다 함수)
     * @return 성공 시 true
     */
    virtual bool executeInTransaction(std::function<bool()> work) {
        if (!beginTransaction()) {
            return false;
        }
        
        try {
            if (work()) {
                return commitTransaction();
            } else {
                rollbackTransaction();
                return false;
            }
        } catch (const std::exception&) {
            rollbackTransaction();
            return false;
        }
    }

protected:
    // =======================================================================
    // 보호된 헬퍼 메서드들 (파생 클래스에서 사용)
    // =======================================================================
    
    /**
     * @brief QueryCondition들을 WHERE 절로 변환
     * @param conditions 쿼리 조건들
     * @return WHERE 절 문자열
     */
    virtual std::string buildWhereClause(const std::vector<QueryCondition>& conditions) const {
        if (conditions.empty()) {
            return "";
        }
        
        std::string where_clause = " WHERE ";
        for (size_t i = 0; i < conditions.size(); ++i) {
            if (i > 0) {
                where_clause += " AND ";
            }
            
            const auto& condition = conditions[i];
            where_clause += condition.field + " " + condition.operation + " ";
            
            // IN 연산자는 특별 처리
            if (condition.operation == "IN") {
                where_clause += "(" + condition.value + ")";
            } else if (condition.operation == "LIKE") {
                where_clause += "'%" + condition.value + "%'";
            } else {
                where_clause += "'" + condition.value + "'";
            }
        }
        
        return where_clause;
    }
    
    /**
     * @brief OrderBy를 ORDER BY 절로 변환
     * @param order_by 정렬 조건
     * @return ORDER BY 절 문자열
     */
    virtual std::string buildOrderByClause(const std::optional<OrderBy>& order_by) const {
        if (!order_by.has_value()) {
            return "";
        }
        
        return " ORDER BY " + order_by->field + 
               (order_by->ascending ? " ASC" : " DESC");
    }
    
    /**
     * @brief Pagination을 LIMIT/OFFSET 절로 변환
     * @param pagination 페이징 정보
     * @return LIMIT/OFFSET 절 문자열
     */
    virtual std::string buildLimitClause(const std::optional<Pagination>& pagination) const {
        if (!pagination.has_value()) {
            return "";
        }
        
        return " LIMIT " + std::to_string(pagination->getLimit()) + 
               " OFFSET " + std::to_string(pagination->getOffset());
    }
};

} // namespace Database
} // namespace PulseOne

#endif // PULSEONE_IREPOSITORY_H