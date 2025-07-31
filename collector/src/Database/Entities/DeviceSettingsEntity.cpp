/**
 * @file DeviceSettingsEntity.cpp
 * @brief PulseOne DeviceSettingsEntity 구현 (Repository 패턴)
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 순환 참조 해결:
 * - 헤더에서는 선언만, CPP에서 Repository 호출
 * - Repository include는 CPP에서만
 */

#include "Database/Entities/DeviceSettingsEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/DeviceSettingsRepository.h"

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 구현 (CPP에서 구현하여 중복 제거)
// =============================================================================

DeviceSettingsEntity::DeviceSettingsEntity() 
    : BaseEntity<DeviceSettingsEntity>()
    , device_id_(0)
    , polling_interval_ms_(1000)        // 1초
    , connection_timeout_ms_(10000)     // 10초
    , max_retry_count_(3)
    , retry_interval_ms_(5000)          // 5초
    , backoff_time_ms_(60000)           // 1분
    , keep_alive_enabled_(true)
    , keep_alive_interval_s_(30)        // 30초
    , read_timeout_ms_(5000)            // 5초
    , write_timeout_ms_(5000)           // 5초
    , backoff_multiplier_(1.5)
    , max_backoff_time_ms_(300000)      // 5분
    , keep_alive_timeout_s_(10)         // 10초
    , data_validation_enabled_(true)
    , performance_monitoring_enabled_(true)
    , diagnostic_mode_enabled_(false)
    , updated_at_(std::chrono::system_clock::now()) {
}

DeviceSettingsEntity::DeviceSettingsEntity(int device_id) 
    : DeviceSettingsEntity() {  // 위임 생성자 사용
    device_id_ = device_id;
    setId(device_id);  // BaseEntity의 ID와 동기화
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (Repository 활용)
// =============================================================================

bool DeviceSettingsEntity::loadFromDatabase() {
    if (device_id_ <= 0) {
        if (logger_) {
            logger_->Error("DeviceSettingsEntity::loadFromDatabase - Invalid device ID: " + std::to_string(device_id_));
        }
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getDeviceSettingsRepository();
        if (repo) {
            auto loaded = repo->findById(device_id_);
            if (loaded.has_value()) {
                *this = loaded.value();
                markSaved();
                if (logger_) {
                    logger_->Info("DeviceSettingsEntity - Loaded settings for device: " + std::to_string(device_id_));
                }
                return true;
            }
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DeviceSettingsEntity::loadFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool DeviceSettingsEntity::saveToDatabase() {
    if (!isValid()) {
        if (logger_) {
            logger_->Error("DeviceSettingsEntity::saveToDatabase - Invalid settings data");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getDeviceSettingsRepository();
        if (repo) {
            bool success = repo->save(*this);
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("DeviceSettingsEntity - Saved settings for device: " + std::to_string(device_id_));
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DeviceSettingsEntity::saveToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool DeviceSettingsEntity::deleteFromDatabase() {
    if (device_id_ <= 0) {
        if (logger_) {
            logger_->Error("DeviceSettingsEntity::deleteFromDatabase - Invalid device ID");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getDeviceSettingsRepository();
        if (repo) {
            bool success = repo->deleteById(device_id_);
            if (success) {
                markDeleted();
                if (logger_) {
                    logger_->Info("DeviceSettingsEntity - Deleted settings for device: " + std::to_string(device_id_));
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DeviceSettingsEntity::deleteFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool DeviceSettingsEntity::updateToDatabase() {
    if (device_id_ <= 0 || !isValid()) {
        if (logger_) {
            logger_->Error("DeviceSettingsEntity::updateToDatabase - Invalid settings data or ID");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getDeviceSettingsRepository();
        if (repo) {
            bool success = repo->update(*this);
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("DeviceSettingsEntity - Updated settings for device: " + std::to_string(device_id_));
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DeviceSettingsEntity::updateToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne