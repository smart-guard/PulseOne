/**
 * @file ExportLogRepository.h
 * @brief Export Log Repository
 * @author PulseOne Development Team
 * @date 2025-10-15
 * @version 1.0.0
 * 저장 위치: core/shared/include/Database/Repositories/ExportLogRepository.h
 */

#ifndef EXPORT_LOG_REPOSITORY_H
#define EXPORT_LOG_REPOSITORY_H

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/ExportLogEntity.h"
#include "Database/DatabaseManager.h"
#include "Utils/LogManager.h"
#include <memory>
#include <map>
#include <string>
#include <mutex>
#include <vector>
#include <optional>
#include <chrono>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 타입 별칭 정의
using ExportLogEntity = PulseOne::Database::Entities::ExportLogEntity;

/**
 * @brief Export Log Repository 클래스
 * 
 * 기능:
 * - Export 전송 로그 관리
 * - 타겟별/상태별 로그 조회
 * - 시간 범위 로그 조회
 * - 로그 통계 및 분석
 */
class ExportLogRepository : public IRepository<ExportLogEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    ExportLogRepository() : IRepository<ExportLogEntity>("ExportLogRepository") {
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("📝 ExportLogRepository initialized");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
        }
    }
    
    virtual ~ExportLogRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현
    // =======================================================================
    
    std::vector<ExportLogEntity> findAll() override;
    std::optional<ExportLogEntity> findById(int id) override;
    bool save(ExportLogEntity& entity) override;
    bool update(const ExportLogEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;

    // =======================================================================
    // 벌크 연산
    // =======================================================================
    
    std::vector<ExportLogEntity> findByIds(const std::vector<int>& ids) override;
    
    std::vector<ExportLogEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;

    // =======================================================================
    // Log 전용 조회 메서드
    // =======================================================================
    
    /**
     * @brief 타겟별 로그 조회
     * @param target_id 타겟 ID
     * @param limit 최대 개수 (기본: 100)
     * @return 로그 목록 (최신순)
     */
    std::vector<ExportLogEntity> findByTargetId(int target_id, int limit = 100);
    
    /**
     * @brief 상태별 로그 조회
     * @param status "success", "failed", "retry"
     * @param limit 최대 개수
     * @return 로그 목록
     */
    std::vector<ExportLogEntity> findByStatus(const std::string& status, int limit = 100);
    
    /**
     * @brief 시간 범위 로그 조회
     * @param start_time 시작 시간
     * @param end_time 종료 시간
     * @return 로그 목록
     */
    std::vector<ExportLogEntity> findByTimeRange(
        const std::chrono::system_clock::time_point& start_time,
        const std::chrono::system_clock::time_point& end_time
    );
    
    /**
     * @brief 최근 로그 조회
     * @param hours 최근 N시간
     * @param limit 최대 개수
     * @return 로그 목록
     */
    std::vector<ExportLogEntity> findRecent(int hours = 24, int limit = 100);
    
    /**
     * @brief 실패 로그만 조회
     * @param hours 최근 N시간
     * @param limit 최대 개수
     * @return 실패 로그 목록
     */
    std::vector<ExportLogEntity> findRecentFailures(int hours = 24, int limit = 100);
    
    /**
     * @brief 포인트별 로그 조회
     * @param point_id 포인트 ID
     * @param limit 최대 개수
     * @return 로그 목록
     */
    std::vector<ExportLogEntity> findByPointId(int point_id, int limit = 100);

    // =======================================================================
    // 통계 및 분석
    // =======================================================================
    
    /**
     * @brief 타겟별 성공/실패 통계
     * @param target_id 타겟 ID
     * @param hours 최근 N시간
     * @return {"success": 10, "failed": 2}
     */
    std::map<std::string, int> getTargetStatistics(int target_id, int hours = 24);
    
    /**
     * @brief 전체 통계
     * @param hours 최근 N시간
     * @return 통계 맵
     */
    std::map<std::string, int> getOverallStatistics(int hours = 24);
    
    /**
     * @brief 평균 처리 시간
     * @param target_id 타겟 ID
     * @param hours 최근 N시간
     * @return 평균 시간 (ms)
     */
    double getAverageProcessingTime(int target_id, int hours = 24);

    // =======================================================================
    // 로그 정리
    // =======================================================================
    
    /**
     * @brief 오래된 로그 삭제
     * @param days N일 이전 로그 삭제
     * @return 삭제된 개수
     */
    int deleteOlderThan(int days);
    
    /**
     * @brief 성공 로그만 삭제
     * @param days N일 이전
     * @return 삭제된 개수
     */
    int deleteSuccessLogsOlderThan(int days);

    // =======================================================================
    // 캐시 관리
    // =======================================================================
    
    std::map<std::string, int> getCacheStats() const override;

private:
    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    ExportLogEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    std::map<std::string, std::string> entityToParams(const ExportLogEntity& entity);
    bool validateLog(const ExportLogEntity& entity);
    bool ensureTableExists();
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // EXPORT_LOG_REPOSITORY_H