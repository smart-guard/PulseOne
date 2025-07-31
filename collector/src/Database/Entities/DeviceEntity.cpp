/**
 * @file DeviceEntity.cpp
 * @brief PulseOne DeviceEntity 구현 (DeviceSettingsEntity 패턴 100% 적용)
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 DeviceSettingsEntity 패턴 완전 적용:
 * - 헤더에서는 선언만, CPP에서 Repository 호출
 * - Repository include는 CPP에서만 (순환 참조 방지)
 * - BaseEntity 순수 가상 함수 구현만 포함
 * - entityToParams 등은 Repository로 이동
 */

#include "Database/Entities/DeviceEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/DeviceRepository.h"

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 구현 (CPP에서 구현하여 중복 제거)
// =============================================================================

DeviceEntity::DeviceEntity() 
    : BaseEntity<DeviceEntity>()
    , tenant_id_(0)
    , site_id_(0)
    , device_group_id_(std::nullopt)
    , edge_server_id_(std::nullopt)
    , name_("")
    , description_("")
    , device_type_("")
    , manufacturer_("")
    , model_("")
    , serial_number_("")
    , protocol_type_("")
    , endpoint_("")
    , config_("{}")
    , is_enabled_(true)
    , installation_date_(std::nullopt)
    , last_maintenance_(std::nullopt)
    , created_by_(std::nullopt)
    , created_at_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now()) {
}

DeviceEntity::DeviceEntity(int device_id) 
    : DeviceEntity() {  // 위임 생성자 사용
    setId(device_id);
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (Repository 활용)
// =============================================================================

bool DeviceEntity::loadFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("DeviceEntity::loadFromDatabase - Invalid device ID: " + std::to_string(getId()));
        }
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getDeviceRepository();
        if (repo) {
            auto loaded = repo->findById(getId());
            if (loaded.has_value()) {
                *this = loaded.value();
                markSaved();
                if (logger_) {
                    logger_->Info("DeviceEntity - Loaded device: " + name_);
                }
                return true;
            }
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DeviceEntity::loadFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool DeviceEntity::saveToDatabase() {
    if (!isValid()) {
        if (logger_) {
            logger_->Error("DeviceEntity::saveToDatabase - Invalid device data");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getDeviceRepository();
        if (repo) {
            bool success = repo->save(*this);
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("DeviceEntity - Saved device: " + name_);
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DeviceEntity::saveToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool DeviceEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("DeviceEntity::deleteFromDatabase - Invalid device ID");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getDeviceRepository();
        if (repo) {
            bool success = repo->deleteById(getId());
            if (success) {
                markDeleted();
                if (logger_) {
                    logger_->Info("DeviceEntity - Deleted device: " + name_);
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DeviceEntity::deleteFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool DeviceEntity::updateToDatabase() {
    if (getId() <= 0 || !isValid()) {
        if (logger_) {
            logger_->Error("DeviceEntity::updateToDatabase - Invalid device data or ID");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getDeviceRepository();
        if (repo) {
            bool success = repo->update(*this);
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("DeviceEntity - Updated device: " + name_);
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DeviceEntity::updateToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne