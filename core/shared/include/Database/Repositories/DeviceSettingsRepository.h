/**
 * @file DeviceSettingsRepository.h
 * @brief PulseOne DeviceSettingsRepository - 디바이스 설정 리포지토리
 * @author PulseOne Development Team
 * @date 2025-07-31
 */

#ifndef DEVICE_SETTINGS_REPOSITORY_H
#define DEVICE_SETTINGS_REPOSITORY_H

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/DeviceSettingsEntity.h"
#include "DatabaseManager.hpp"
#include "Logging/LogManager.h"
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <optional>
#include <future>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 타입 별칭 정의
using DeviceSettingsEntity = PulseOne::Database::Entities::DeviceSettingsEntity;

class DeviceSettingsRepository : public IRepository<DeviceSettingsEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    DeviceSettingsRepository() : IRepository<DeviceSettingsEntity>("DeviceSettingsRepository") {
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("🔧 DeviceSettingsRepository initialized with device_id-based operations");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
        }
    }
    
    virtual ~DeviceSettingsRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현
    // =======================================================================
    
    std::vector<DeviceSettingsEntity> findAll() override;
    std::optional<DeviceSettingsEntity> findById(int device_id) override;
    bool save(DeviceSettingsEntity& entity) override;
    bool update(const DeviceSettingsEntity& entity) override;
    bool deleteById(int device_id) override;
    bool exists(int device_id) override;

    // =======================================================================
    // 벌크 연산
    // =======================================================================
    
    std::vector<DeviceSettingsEntity> findByIds(const std::vector<int>& device_ids) override;
    
    std::vector<DeviceSettingsEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;

    // =======================================================================
    // DeviceSettings 전용 메서드들
    // =======================================================================
    
    bool createOrUpdateSettings(int device_id, const DeviceSettingsEntity& settings);
    bool createDefaultSettings(int device_id);
    std::vector<DeviceSettingsEntity> findByProtocol(const std::string& protocol_type);
    std::vector<DeviceSettingsEntity> findActiveDeviceSettings();
    std::map<int, std::vector<DeviceSettingsEntity>> groupByPollingInterval();

    // =======================================================================
    // 실시간 설정 변경
    // =======================================================================
    
    bool updatePollingInterval(int device_id, int polling_interval_ms);
    bool updateConnectionTimeout(int device_id, int timeout_ms);
    bool updateRetrySettings(int device_id, int max_retry_count, int retry_interval_ms);
    bool updateAutoRegistrationEnabled(int device_id, bool enabled);
    int bulkUpdateSettings(const std::map<int, DeviceSettingsEntity>& settings_map);

    // =======================================================================
    // 설정 프리셋 관리
    // =======================================================================
    
    bool applyIndustrialDefaults(int device_id);
    bool applyHighSpeedMode(int device_id);
    bool applyStabilityMode(int device_id);
    int applyPresetToProtocol(const std::string& protocol_type, const std::string& preset_mode);

    // =======================================================================
    // 통계 및 분석
    // =======================================================================
    
    std::string getSettingsStatistics() const;
    std::vector<DeviceSettingsEntity> findAbnormalSettings() const;
    void logSettingsChange(int device_id, 
                          const DeviceSettingsEntity& old_settings, 
                          const DeviceSettingsEntity& new_settings);

    // =======================================================================
    // 캐시 관리
    // =======================================================================
    
    void setCacheEnabled(bool enabled) override {
        IRepository<DeviceSettingsEntity>::setCacheEnabled(enabled);
        if (logger_) {
            logger_->Info("DeviceSettingsRepository cache " + std::string(enabled ? "enabled" : "disabled"));
        }
    }
    
    bool isCacheEnabled() const override {
        return IRepository<DeviceSettingsEntity>::isCacheEnabled();
    }
    
    void clearCache() override {
        IRepository<DeviceSettingsEntity>::clearCache();
        if (logger_) {
            logger_->Info("DeviceSettingsRepository cache cleared");
        }
    }

private:
    // =======================================================================
    // 의존성 관리
    // =======================================================================
    
    DbLib::DatabaseManager* db_manager_;
    LogManager* logger_;
    
    void initializeDependencies() {
        db_manager_ = &DbLib::DatabaseManager::getInstance();
        logger_ = &LogManager::getInstance();
    }

    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief SQL 결과를 DeviceSettingsEntity로 변환
     * @param row SQL 결과 행
     * @return DeviceSettingsEntity
     */
    DeviceSettingsEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief DeviceSettingsEntity를 SQL 파라미터 맵으로 변환
     * @param entity 엔티티
     * @return SQL 파라미터 맵
     */
    std::map<std::string, std::string> entityToParams(const DeviceSettingsEntity& entity);
    
    /**
     * @brief device_settings 테이블이 존재하는지 확인하고 없으면 생성
     * @return 성공 시 true
     */
    bool ensureTableExists();
    
    /**
     * @brief 설정 검증
     * @param entity 검증할 설정 엔티티
     * @return 유효하면 true
     */
    bool validateSettings(const DeviceSettingsEntity& entity) const;
    
    /**
     * @brief 프리셋 모드를 DeviceSettingsEntity로 변환
     * @param preset_mode 프리셋 모드 문자열
     * @param device_id 디바이스 ID
     * @return 프리셋 설정이 적용된 엔티티
     */
    DeviceSettingsEntity createPresetEntity(const std::string& preset_mode, int device_id) const;
    

};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // DEVICE_SETTINGS_REPOSITORY_H