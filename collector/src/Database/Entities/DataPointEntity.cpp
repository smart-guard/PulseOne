/**
 * @file DataPointEntity.cpp
 * @brief PulseOne DataPointEntity 구현 (DeviceSettingsEntity 패턴 100% 적용)
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 DeviceSettingsEntity 패턴 완전 적용:
 * - 헤더에서는 선언만, CPP에서 Repository 호출
 * - Repository include는 CPP에서만 (순환 참조 방지)
 * - BaseEntity 순수 가상 함수 구현만 포함
 * - DB 작업은 모두 Repository로 위임
 */

#include "Database/Entities/DataPointEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/DataPointRepository.h"

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 구현 (CPP에서 구현하여 중복 제거)
// =============================================================================

DataPointEntity::DataPointEntity() 
    : BaseEntity<DataPointEntity>()
    , device_id_(0)
    , name_("")
    , description_("")
    , address_(0)
    , data_type_("UNKNOWN")
    , access_mode_("read")
    , is_enabled_(true)
    , unit_("")
    , scaling_factor_(1.0)
    , scaling_offset_(0.0)
    , min_value_(std::numeric_limits<double>::lowest())
    , max_value_(std::numeric_limits<double>::max())
    , log_enabled_(true)
    , log_interval_ms_(0)
    , log_deadband_(0.0)
    , tags_()
    , metadata_()
    , created_at_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now())
    , last_read_time_(std::chrono::system_clock::now())
    , last_write_time_(std::chrono::system_clock::now())
    , read_count_(0)
    , write_count_(0)
    , error_count_(0) {
}

DataPointEntity::DataPointEntity(int point_id) 
    : DataPointEntity() {  // 위임 생성자 사용
    setId(point_id);
}

DataPointEntity::DataPointEntity(const DataPoint& data_point) 
    : BaseEntity<DataPointEntity>()
    , device_id_(0)  // device_id는 문자열에서 변환 필요
    , name_(data_point.name)
    , description_(data_point.description)
    , address_(data_point.address)
    , data_type_(data_point.data_type)
    , access_mode_(data_point.is_writable ? "read_write" : "read")
    , is_enabled_(data_point.is_enabled)
    , unit_(data_point.unit)
    , scaling_factor_(data_point.scaling_factor)
    , scaling_offset_(data_point.scaling_offset)
    , min_value_(data_point.min_value)
    , max_value_(data_point.max_value)
    , log_enabled_(data_point.log_enabled)
    , log_interval_ms_(data_point.log_interval_ms)
    , log_deadband_(data_point.log_deadband)
    , tags_(data_point.tags)
    , metadata_(data_point.metadata)
    , created_at_(data_point.created_at)
    , updated_at_(data_point.updated_at)
    , last_read_time_(data_point.last_read_time)
    , last_write_time_(data_point.last_write_time)
    , read_count_(data_point.read_count)
    , write_count_(data_point.write_count)
    , error_count_(data_point.error_count) {
    
    // ID 변환
    if (!data_point.id.empty()) {
        try {
            setId(std::stoi(data_point.id));
        } catch (const std::exception&) {
            setId(0);
        }
    }
    
    // device_id 변환
    if (!data_point.device_id.empty()) {
        try {
            device_id_ = std::stoi(data_point.device_id);
        } catch (const std::exception&) {
            device_id_ = 0;
        }
    }
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (Repository 활용)
// =============================================================================

bool DataPointEntity::loadFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("DataPointEntity::loadFromDatabase - Invalid data point ID: " + std::to_string(getId()));
        }
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getDataPointRepository();
        
        if (!repo) {
            if (logger_) {
                logger_->Error("DataPointEntity::loadFromDatabase - DataPointRepository not available");
            }
            markError();
            return false;
        }
        
        auto loaded = repo->findById(getId());
        if (loaded.has_value()) {
            // 로드된 데이터를 현재 객체에 복사
            *this = loaded.value();
            markSaved();
            
            if (logger_) {
                logger_->Info("DataPointEntity::loadFromDatabase - Loaded data point: " + name_);
            }
            return true;
        } else {
            if (logger_) {
                logger_->Warn("DataPointEntity::loadFromDatabase - Data point not found: " + std::to_string(getId()));
            }
            return false;
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DataPointEntity::loadFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}


// =============================================================================
// 추가 메서드들 (기존 DataPointEntity.cpp에서 이동)
// =============================================================================

json DataPointEntity::getWorkerContext() const {
    json context;
    context["point_id"] = getId();
    context["device_id"] = device_id_;
    context["name"] = name_;
    context["address"] = address_;
    context["data_type"] = data_type_;
    context["is_enabled"] = is_enabled_;
    context["is_writable"] = (access_mode_ == "write" || access_mode_ == "read_write");
    
    // 스케일링 정보
    context["scaling"] = {
        {"factor", scaling_factor_},
        {"offset", scaling_offset_}
    };
    
    // 범위 정보
    context["range"] = {
        {"min", min_value_},
        {"max", max_value_}
    };
    
    return context;
}

bool DataPointEntity::saveToDatabase() {
    if (!isValid()) {
        if (logger_) {
            logger_->Error("DataPointEntity::saveToDatabase - Invalid data point data");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getDataPointRepository();
        
        if (!repo) {
            if (logger_) {
                logger_->Error("DataPointEntity::saveToDatabase - DataPointRepository not available");
            }
            return false;
        }
        
        // Repository의 save 메서드 호출 (Entity를 참조로 전달)
        DataPointEntity mutable_copy = *this;
        bool success = repo->save(mutable_copy);
        
        if (success) {
            // 저장 성공 시 ID와 상태 업데이트
            if (getId() <= 0) {
                setId(mutable_copy.getId());
            }
            markSaved();
            updated_at_ = std::chrono::system_clock::now();
            
            if (logger_) {
                logger_->Info("DataPointEntity::saveToDatabase - Saved data point: " + name_);
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DataPointEntity::saveToDatabase failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool DataPointEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("DataPointEntity::deleteFromDatabase - Invalid data point ID: " + std::to_string(getId()));
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getDataPointRepository();
        
        if (!repo) {
            if (logger_) {
                logger_->Error("DataPointEntity::deleteFromDatabase - DataPointRepository not available");
            }
            return false;
        }
        
        bool success = repo->deleteById(getId());
        
        if (success) {
            markDeleted();
            if (logger_) {
                logger_->Info("DataPointEntity::deleteFromDatabase - Deleted data point: " + std::to_string(getId()));
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DataPointEntity::deleteFromDatabase failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool DataPointEntity::updateToDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("DataPointEntity::updateToDatabase - Invalid data point ID: " + std::to_string(getId()));
        }
        return false;
    }
    
    if (!isValid()) {
        if (logger_) {
            logger_->Error("DataPointEntity::updateToDatabase - Invalid data point data");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getDataPointRepository();
        
        if (!repo) {
            if (logger_) {
                logger_->Error("DataPointEntity::updateToDatabase - DataPointRepository not available");
            }
            return false;
        }
        
        // 업데이트 시간 갱신
        updated_at_ = std::chrono::system_clock::now();
        
        bool success = repo->update(*this);
        
        if (success) {
            markSaved();
            if (logger_) {
                logger_->Info("DataPointEntity::updateToDatabase - Updated data point: " + name_);
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DataPointEntity::updateToDatabase failed: " + std::string(e.what()));
        }
        return false;
    }
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne