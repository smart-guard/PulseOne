/**
 * @file PayloadTemplateRepository.h
 * @brief Payload Template Repository
 * @version 1.0.1
 * 저장 위치: core/shared/include/Database/Repositories/PayloadTemplateRepository.h
 * 
 * 🔧 수정 내역:
 * - bulkSave() 반환 타입: bool -> int (IRepository 인터페이스 일치)
 * - bulkUpdate() 반환 타입: bool -> int (IRepository 인터페이스 일치)
 * - bulkDelete() -> deleteByIds()로 메소드명 변경 및 반환 타입 int로 수정
 */
#ifndef PAYLOAD_TEMPLATE_REPOSITORY_H
#define PAYLOAD_TEMPLATE_REPOSITORY_H

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/PayloadTemplateEntity.h"
#include "DatabaseManager.hpp"
#include "Logging/LogManager.h"
#include <memory>
#include <vector>
#include <optional>

namespace PulseOne {
namespace Database {
namespace Repositories {

using PayloadTemplateEntity = Entities::PayloadTemplateEntity;

class PayloadTemplateRepository : public IRepository<PayloadTemplateEntity> {
public:
    PayloadTemplateRepository() : IRepository<PayloadTemplateEntity>("PayloadTemplateRepository") {
        initializeDependencies();
        if (logger_) {
            logger_->Info("PayloadTemplateRepository initialized");
        }
    }
    
    virtual ~PayloadTemplateRepository() = default;

    // =======================================================================
    // IRepository 기본 인터페이스 구현
    // =======================================================================
    
    std::vector<PayloadTemplateEntity> findAll() override;
    std::optional<PayloadTemplateEntity> findById(int id) override;
    bool save(PayloadTemplateEntity& entity) override;
    bool update(const PayloadTemplateEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;

    // =======================================================================
    // IRepository 벌크 연산 override (✅ 반환타입 int로 수정)
    // =======================================================================
    
    /**
     * @brief 여러 ID로 템플릿들 조회
     * @param ids 템플릿 ID 목록
     * @return PayloadTemplateEntity 목록
     */
    std::vector<PayloadTemplateEntity> findByIds(const std::vector<int>& ids) override;
    
    /**
     * @brief 조건부 쿼리
     * @param conditions 검색 조건들
     * @param order_by 정렬 조건 (optional)
     * @param pagination 페이지네이션 (optional)
     * @return PayloadTemplateEntity 목록
     */
    std::vector<PayloadTemplateEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    /**
     * @brief 조건에 맞는 레코드 수 조회
     * @param conditions 검색 조건들
     * @return 레코드 수
     */
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    
    /**
     * @brief 여러 템플릿 일괄 저장
     * @param entities 저장할 템플릿들 (참조로 전달하여 ID 업데이트)
     * @return 저장된 개수
     * 
     * ✅ 수정: IRepository 인터페이스의 saveBulk()는 int 반환
     */
    int saveBulk(std::vector<PayloadTemplateEntity>& entities) override;
    
    /**
     * @brief 여러 템플릿 일괄 업데이트
     * @param entities 업데이트할 템플릿들
     * @return 업데이트된 개수
     * 
     * ✅ 수정: IRepository 인터페이스의 updateBulk()는 int 반환
     */
    int updateBulk(const std::vector<PayloadTemplateEntity>& entities) override;
    
    /**
     * @brief 여러 ID 일괄 삭제
     * @param ids 삭제할 ID들
     * @return 삭제된 개수
     * 
     * ✅ 수정: IRepository 인터페이스의 deleteByIds()는 int 반환
     */
    int deleteByIds(const std::vector<int>& ids) override;

    // =======================================================================
    // 커스텀 쿼리 메소드들
    // =======================================================================
    
    /**
     * @brief 이름으로 템플릿 조회
     * @param name 템플릿 이름
     * @return PayloadTemplateEntity (optional)
     */
    std::optional<PayloadTemplateEntity> findByName(const std::string& name);
    
    /**
     * @brief 시스템 타입으로 템플릿들 조회
     * @param system_type 시스템 타입
     * @return PayloadTemplateEntity 목록
     */
    std::vector<PayloadTemplateEntity> findBySystemType(const std::string& system_type);
    
    /**
     * @brief 활성화된 템플릿들만 조회
     * @return 활성화된 PayloadTemplateEntity 목록
     */
    std::vector<PayloadTemplateEntity> findActive();

    // =======================================================================
    // 헬퍼 메소드들
    // =======================================================================
    
    /**
     * @brief 테이블 존재 여부 확인 및 생성
     * @return 성공 시 true
     */
    bool ensureTableExists();

protected:
    /**
     * @brief DB 행을 Entity로 변환
     * @param row DB 행 데이터
     * @return PayloadTemplateEntity
     */
    PayloadTemplateEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief Entity를 DB 파라미터로 변환
     * @param entity PayloadTemplateEntity
     * @return 파라미터 맵
     */
    std::map<std::string, std::string> entityToParams(const PayloadTemplateEntity& entity);
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // PAYLOAD_TEMPLATE_REPOSITORY_H