/**
 * @file ExportTargetRepository.h
 * @brief Export Target Repository (통계 필드 제거 버전)
 * @version 3.0.0 - 통계 필드 완전 제거
 * @date 2025-10-21
 * 
 * 저장 위치: core/shared/include/Database/Repositories/ExportTargetRepository.h
 * 
 * 주요 변경사항:
 *   - updateStatistics() 제거
 *   - findHealthyTargets() 제거
 *   - findRecentErrorTargets() 제거
 *   - 설정 정보만 관리
 */

#ifndef EXPORT_TARGET_REPOSITORY_H
#define EXPORT_TARGET_REPOSITORY_H

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/ExportTargetEntity.h"
#include "DatabaseManager.hpp"
#include "Logging/LogManager.h"
#include <memory>
#include <map>
#include <string>
#include <mutex>
#include <vector>
#include <optional>
#include <chrono>
#include <atomic>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 타입 별칭 정의
using ExportTargetEntity = PulseOne::Database::Entities::ExportTargetEntity;

/**
 * @brief Export Target Repository 클래스
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 타겟 타입별 조회 (HTTP, S3, File, MQTT)
 * - 활성 타겟 조회
 * - 프로파일별 타겟 조회
 * - DatabaseAbstractionLayer 사용
 * - 캐싱 및 벌크 연산 지원
 * 
 * 역할:
 * - Export Target 설정만 관리
 * - 통계는 ExportLogRepository에서 조회
 */
class ExportTargetRepository : public IRepository<ExportTargetEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    ExportTargetRepository() : IRepository<ExportTargetEntity>("ExportTargetRepository") {
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("🚀 ExportTargetRepository initialized (v3.0 - stats removed)");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
        }
    }
    
    virtual ~ExportTargetRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현
    // =======================================================================
    
    std::vector<ExportTargetEntity> findAll() override;
    std::optional<ExportTargetEntity> findById(int id) override;
    bool save(ExportTargetEntity& entity) override;
    bool update(const ExportTargetEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;

    // =======================================================================
    // 벌크 연산
    // =======================================================================
    
    std::vector<ExportTargetEntity> findByIds(const std::vector<int>& ids) override;
    
    std::vector<ExportTargetEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;

    // =======================================================================
    // Export Target 전용 조회 메서드 (설정 조회만)
    // =======================================================================
    
    /**
     * @brief 활성화된 타겟 조회
     * @param enabled true=활성, false=비활성
     * @return 타겟 목록
     */
    std::vector<ExportTargetEntity> findByEnabled(bool enabled);
    
    /**
     * @brief 타겟 타입별 조회
     * @param target_type "http", "s3", "file", "mqtt"
     * @return 타겟 목록
     */
    std::vector<ExportTargetEntity> findByTargetType(const std::string& target_type);
    
    /**
     * @brief 프로파일별 타겟 조회
     * @param profile_id 프로파일 ID
     * @return 타겟 목록
     */
    std::vector<ExportTargetEntity> findByProfileId(int profile_id);
    
    /**
     * @brief 이름으로 타겟 조회
     * @param name 타겟 이름 (UNIQUE)
     * @return 타겟 (optional)
     */
    std::optional<ExportTargetEntity> findByName(const std::string& name);

    // =======================================================================
    // 카운트 및 통계 (설정 기반만)
    // =======================================================================
    
    /**
     * @brief 전체 타겟 수
     */
    int getTotalCount();
    
    /**
     * @brief 활성 타겟 수
     */
    int getActiveCount();
    
    /**
     * @brief 타겟 타입별 개수
     * @return map<target_type, count>
     */
    std::map<std::string, int> getCountByType();

    // =======================================================================
    // 캐시 관리
    // =======================================================================
    
    std::map<std::string, int> getCacheStats() const override;

private:
    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief DB 행을 Entity로 변환 (통계 필드 제거됨)
     */
    ExportTargetEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief Entity를 DB 파라미터로 변환 (통계 필드 제거됨)
     */
    std::map<std::string, std::string> entityToParams(const ExportTargetEntity& entity);
    
    /**
     * @brief 테이블 존재 확인 및 생성
     */
    bool ensureTableExists();
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // EXPORT_TARGET_REPOSITORY_H