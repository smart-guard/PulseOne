/**
 * @file ExportScheduleRepository.h
 * @brief Export Schedule Repository (올바른 패턴 적용)
 * @author PulseOne Development Team
 * @date 2025-10-22
 * @version 1.0.0
 * 
 * 저장 위치: core/shared/include/Database/Repositories/ExportScheduleRepository.h
 */

#ifndef EXPORT_SCHEDULE_REPOSITORY_H
#define EXPORT_SCHEDULE_REPOSITORY_H

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/ExportScheduleEntity.h"
#include "Database/DatabaseManager.h"
#include "Utils/LogManager.h"
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
using ExportScheduleEntity = PulseOne::Database::Entities::ExportScheduleEntity;

/**
 * @brief Export Schedule Repository 클래스
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 활성화된 스케줄 조회
 * - 실행 대기 중인 스케줄 조회
 * - 타겟별 스케줄 조회
 * - DatabaseAbstractionLayer 사용
 * - 캐싱 및 벌크 연산 지원
 */
class ExportScheduleRepository : public IRepository<ExportScheduleEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    ExportScheduleRepository() 
        : IRepository<ExportScheduleEntity>("ExportScheduleRepository") {
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("📅 ExportScheduleRepository initialized");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
        }
    }
    
    virtual ~ExportScheduleRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현
    // =======================================================================
    
    std::vector<ExportScheduleEntity> findAll() override;
    std::optional<ExportScheduleEntity> findById(int id) override;
    bool save(ExportScheduleEntity& entity) override;
    bool update(const ExportScheduleEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;

    // =======================================================================
    // 벌크 연산
    // =======================================================================
    
    std::vector<ExportScheduleEntity> findByIds(const std::vector<int>& ids) override;
    
    std::vector<ExportScheduleEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;

    // =======================================================================
    // ExportSchedule 특화 조회 메서드
    // =======================================================================
    
    /**
     * @brief 활성화된 스케줄 조회
     * @return 활성화된 스케줄 목록
     */
    std::vector<ExportScheduleEntity> findEnabled();
    
    /**
     * @brief 특정 타겟의 스케줄 조회
     * @param target_id 타겟 ID
     * @return 스케줄 목록
     */
    std::vector<ExportScheduleEntity> findByTargetId(int target_id);
    
    /**
     * @brief 실행 대기 중인 스케줄 조회
     * @return 실행 대기 중인 스케줄 목록
     */
    std::vector<ExportScheduleEntity> findPending();
    
    /**
     * @brief 활성화된 스케줄 수 조회
     * @return 활성화된 스케줄 수
     */
    int countEnabled();
    
    // =======================================================================
    // 스케줄 실행 상태 업데이트
    // =======================================================================
    
    /**
     * @brief 실행 결과 기록
     * @param schedule_id 스케줄 ID
     * @param success 성공 여부
     * @param next_run 다음 실행 시각
     * @return 성공 시 true
     */
    bool updateRunStatus(int schedule_id, bool success, 
                        const std::chrono::system_clock::time_point& next_run);
    
    // =======================================================================
    // 캐시 관리 (IRepository 상속)
    // =======================================================================
    
    void setCacheEnabled(bool enabled) override {
        IRepository<ExportScheduleEntity>::setCacheEnabled(enabled);
        if (logger_) {
            logger_->Info("ExportScheduleRepository cache " + std::string(enabled ? "enabled" : "disabled"));
        }
    }
    
    bool isCacheEnabled() const override {
        return IRepository<ExportScheduleEntity>::isCacheEnabled();
    }
    
    void clearCache() override {
        IRepository<ExportScheduleEntity>::clearCache();
        if (logger_) {
            logger_->Info("ExportScheduleRepository cache cleared");
        }
    }
    
    std::map<std::string, int> getCacheStats() const override;

private:
    // =======================================================================
    // 의존성 관리
    // =======================================================================
    
    DatabaseManager* db_manager_;
    LogManager* logger_;
    
    void initializeDependencies() {
        db_manager_ = &DatabaseManager::getInstance();
        logger_ = &LogManager::getInstance();
    }

    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 테이블 존재 확인 및 생성
     * @return 성공 시 true
     */
    bool ensureTableExists();
    
    /**
     * @brief DB 행을 Entity로 변환
     * @param row DB 행 데이터
     * @return ExportScheduleEntity
     */
    ExportScheduleEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief Entity를 파라미터 맵으로 변환
     * @param entity ExportScheduleEntity
     * @return 파라미터 맵
     */
    std::map<std::string, std::string> entityToParams(const ExportScheduleEntity& entity);
    
    /**
     * @brief Entity 유효성 검증
     * @param entity ExportScheduleEntity
     * @return 유효하면 true
     */
    bool validateEntity(const ExportScheduleEntity& entity) const;
    
    /**
     * @brief 문자열을 timestamp로 변환
     * @param time_str 시간 문자열
     * @return time_point
     */
    std::chrono::system_clock::time_point parseTimestamp(const std::string& time_str);
    
    /**
     * @brief timestamp를 문자열로 변환
     * @param tp time_point
     * @return 시간 문자열
     */
    std::string formatTimestamp(const std::chrono::system_clock::time_point& tp);
    
    // =======================================================================
    // 스레드 안전성
    // =======================================================================
    
    mutable std::mutex repository_mutex_;
    std::atomic<bool> table_created_{false};
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // EXPORT_SCHEDULE_REPOSITORY_H