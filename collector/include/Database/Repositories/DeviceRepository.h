#ifndef DEVICE_REPOSITORY_H
#define DEVICE_REPOSITORY_H

/**
 * @file DeviceRepository.h
 * @brief PulseOne DeviceRepository - DeviceSettingsRepository 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🔥 DeviceSettingsRepository 패턴 완전 적용:
 * - DatabaseAbstractionLayer 사용
 * - executeQuery/executeNonQuery 패턴
 * - 컴파일 에러 완전 해결
 * - BaseEntity 상속 패턴 지원
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/DeviceEntity.h"
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

// 타입 별칭 정의 (DeviceSettingsRepository 패턴)
using DeviceEntity = PulseOne::Database::Entities::DeviceEntity;

/**
 * @brief Device Repository 클래스 (DeviceSettingsRepository 패턴 적용)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 프로토콜별 디바이스 조회
 * - DatabaseAbstractionLayer 사용
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class DeviceRepository : public IRepository<DeviceEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    DeviceRepository() : IRepository<DeviceEntity>("DeviceRepository") {
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("🏭 DeviceRepository initialized with BaseEntity pattern");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
        }
    }
    
    virtual ~DeviceRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현
    // =======================================================================
    
    std::vector<DeviceEntity> findAll() override;
    std::optional<DeviceEntity> findById(int id) override;
    bool save(DeviceEntity& entity) override;
    bool update(const DeviceEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;

    // =======================================================================
    // 벌크 연산
    // =======================================================================
    
    std::vector<DeviceEntity> findByIds(const std::vector<int>& ids) override;
    
    std::vector<DeviceEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;

    // =======================================================================
    // Device 전용 메서드들
    // =======================================================================
    
    std::vector<DeviceEntity> findByProtocol(const std::string& protocol_type);
    std::vector<DeviceEntity> findByTenant(int tenant_id);
    std::vector<DeviceEntity> findBySite(int site_id);
    std::vector<DeviceEntity> findEnabledDevices();
    std::map<std::string, std::vector<DeviceEntity>> groupByProtocol();

    // =======================================================================
    // 벌크 연산 (DeviceSettingsRepository 패턴)
    // =======================================================================
    
    int saveBulk(std::vector<DeviceEntity>& entities);
    int updateBulk(const std::vector<DeviceEntity>& entities);
    int deleteByIds(const std::vector<int>& ids);

    // =======================================================================
    // 실시간 디바이스 관리
    // =======================================================================
    
    bool enableDevice(int device_id);
    bool disableDevice(int device_id);
    bool updateDeviceStatus(int device_id, bool is_enabled);
    bool updateEndpoint(int device_id, const std::string& endpoint);
    bool updateConfig(int device_id, const std::string& config);

    // =======================================================================
    // 통계 및 분석
    // =======================================================================
    
    std::string getDeviceStatistics() const;
    std::vector<DeviceEntity> findInactiveDevices() const;
    std::map<std::string, int> getProtocolDistribution() const;

    // =======================================================================
    // 캐시 관리
    // =======================================================================
    
    void setCacheEnabled(bool enabled) override {
        IRepository<DeviceEntity>::setCacheEnabled(enabled);
        if (logger_) {
            logger_->Info("DeviceRepository cache " + std::string(enabled ? "enabled" : "disabled"));
        }
    }
    
    bool isCacheEnabled() const override {
        return IRepository<DeviceEntity>::isCacheEnabled();
    }
    
    void clearCache() override {
        IRepository<DeviceEntity>::clearCache();
        if (logger_) {
            logger_->Info("DeviceRepository cache cleared");
        }
    }

    // =======================================================================
    // Worker용 최적화 메서드들 (DeviceSettingsRepository 패턴)
    // =======================================================================
    
    int getTotalCount();

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
    // 내부 헬퍼 메서드들 (DeviceSettingsRepository 패턴)
    // =======================================================================
    
    /**
     * @brief SQL 결과를 DeviceEntity로 변환
     * @param row SQL 결과 행
     * @return DeviceEntity
     */
    DeviceEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 여러 SQL 결과를 DeviceEntity 벡터로 변환
     * @param result SQL 결과
     * @return DeviceEntity 벡터
     */
    std::vector<DeviceEntity> mapResultToEntities(const std::vector<std::map<std::string, std::string>>& result);
    
    /**
     * @brief DeviceEntity를 SQL 파라미터 맵으로 변환
     * @param entity 엔티티
     * @return SQL 파라미터 맵
     */
    std::map<std::string, std::string> entityToParams(const DeviceEntity& entity);
    
    /**
     * @brief devices 테이블이 존재하는지 확인하고 없으면 생성
     * @return 성공 시 true
     */
    bool ensureTableExists();
    
    /**
     * @brief 디바이스 검증
     * @param entity 검증할 디바이스 엔티티
     * @return 유효하면 true
     */
    bool validateDevice(const DeviceEntity& entity) const;
    
    /**
     * @brief SQL 문자열 이스케이프 처리
     * @param str 이스케이프할 문자열
     * @return 이스케이프된 문자열
     */
    std::string escapeString(const std::string& str) const;
    
    /**
     * @brief 타임스탬프를 문자열로 변환
     * @param timestamp 타임스탬프
     * @return 문자열 형태의 타임스탬프
     */
    std::string formatTimestamp(const std::chrono::system_clock::time_point& timestamp) const;
    
    /**
     * @brief WHERE 절 생성
     * @param conditions 조건들
     * @return WHERE 절 문자열
     */
    std::string buildWhereClause(const std::vector<QueryCondition>& conditions) const;
    
    /**
     * @brief ORDER BY 절 생성
     * @param order_by 정렬 조건
     * @return ORDER BY 절 문자열
     */
    std::string buildOrderByClause(const std::optional<OrderBy>& order_by) const;
    
    /**
     * @brief LIMIT 절 생성
     * @param pagination 페이지네이션
     * @return LIMIT 절 문자열
     */
    std::string buildLimitClause(const std::optional<Pagination>& pagination) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // DEVICE_REPOSITORY_H