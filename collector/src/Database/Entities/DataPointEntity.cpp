/**
 * @file DataPointEntity.cpp
 * @brief PulseOne DataPointEntity 구현 (새 스키마 완전 반영 + DeviceSettingsEntity 패턴 100% 적용)
 * @author PulseOne Development Team
 * @date 2025-08-07
 * 
 * 🎯 새 DB 스키마 완전 반영:
 * - address_string, is_writable, polling_interval_ms 추가
 * - group_name, protocol_params 추가
 * - scaling_offset 추가
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
// 생성자 구현 (새 필드들 포함)
// =============================================================================

DataPointEntity::DataPointEntity() 
    : BaseEntity<DataPointEntity>()
    , device_id_(0)
    , name_("")
    , description_("")
    , address_(0)
    , address_string_("")                                    // 🔥 새 필드
    , data_type_("UNKNOWN")
    , access_mode_("read")
    , is_enabled_(true)
    , is_writable_(false)                                    // 🔥 새 필드
    , unit_("")
    , scaling_factor_(1.0)
    , scaling_offset_(0.0)                                   // 🔥 새 필드
    , min_value_(std::numeric_limits<double>::lowest())
    , max_value_(std::numeric_limits<double>::max())
    , log_enabled_(true)
    , log_interval_ms_(0)
    , log_deadband_(0.0)
    , polling_interval_ms_(1000)                             // 🔥 새 필드 (기본값 1초)
    , group_name_("")                                        // 🔥 새 필드
    , tags_()
    , metadata_()
    , protocol_params_()                                     // 🔥 새 필드
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

// =============================================================================
// 새로운 Worker 관련 메서드 구현 (헤더에서 인라인 선언된 것을 여기서 재구현하지 않음)
// =============================================================================

// getWorkerContext()는 헤더에서 인라인으로 이미 구현됨

// =============================================================================
// 추가된 유틸리티 메서드들 구현 (새 필드들 활용)
// =============================================================================

/**
 * @brief 프로토콜별 특화 검증 로직
 */
bool DataPointEntity::validateProtocolSpecific() const {
    std::string protocol = getProtocol();
    
    if (protocol == "MODBUS_TCP" || protocol == "MODBUS_RTU") {
        // Modbus 주소 범위 확인
        if (address_ < 1 || address_ > 65535) {
            return false;
        }
        
        // Modbus 데이터 타입 확인
        if (data_type_ != "HOLDING_REGISTER" && 
            data_type_ != "INPUT_REGISTER" && 
            data_type_ != "COIL" && 
            data_type_ != "DISCRETE_INPUT") {
            return false;
        }
        
    } else if (protocol == "MQTT") {
        // MQTT 토픽 유효성 확인
        if (address_string_.empty() || address_string_.find('#') == 0) {
            return false;
        }
        
    } else if (protocol == "BACNET_IP" || protocol == "BACNET") {
        // BACnet Object ID 확인
        if (address_ < 0) {
            return false;
        }
        
        // BACnet 객체 타입 확인
        std::string object_type = getProtocolParam("object_type");
        if (object_type.empty()) {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief 스케일링 값 적용
 */
double DataPointEntity::applyScaling(double raw_value) const {
    return (raw_value * scaling_factor_) + scaling_offset_;
}

/**
 * @brief 역스케일링 값 적용 (쓰기 시 사용)
 */
double DataPointEntity::removeScaling(double scaled_value) const {
    if (scaling_factor_ == 0.0) {
        return scaled_value;  // 0으로 나누기 방지
    }
    return (scaled_value - scaling_offset_) / scaling_factor_;
}

/**
 * @brief 값이 데드밴드 범위 내인지 확인
 */
bool DataPointEntity::isWithinDeadband(double previous_value, double new_value) const {
    if (log_deadband_ <= 0.0) {
        return false;  // 데드밴드가 설정되지 않음
    }
    
    double diff = std::abs(new_value - previous_value);
    return diff <= log_deadband_;
}

/**
 * @brief 값이 유효 범위 내인지 확인
 */
bool DataPointEntity::isValueInRange(double value) const {
    return value >= min_value_ && value <= max_value_;
}

/**
 * @brief 폴링 주기 조정 (네트워크 상태에 따라)
 */
void DataPointEntity::adjustPollingInterval(bool connection_healthy) {
    if (!connection_healthy) {
        // 연결 불안정 시 폴링 주기 증가
        polling_interval_ms_ = std::min(polling_interval_ms_ * 2, static_cast<uint32_t>(60000)); // 최대 60초
    } else {
        // 연결 안정 시 원래 주기로 복원 (metadata에서 original_polling_interval 조회)
        auto it = metadata_.find("original_polling_interval");
        if (it != metadata_.end()) {
            try {
                uint32_t original = static_cast<uint32_t>(std::stoul(it->second));
                polling_interval_ms_ = original;
            } catch (const std::exception&) {
                // 파싱 실패 시 기본값 유지
            }
        }
    }
    markModified();
}

/**
 * @brief 태그 추가
 */
void DataPointEntity::addTag(const std::string& tag) {
    auto it = std::find(tags_.begin(), tags_.end(), tag);
    if (it == tags_.end()) {
        tags_.push_back(tag);
        markModified();
    }
}

/**
 * @brief 태그 제거
 */
void DataPointEntity::removeTag(const std::string& tag) {
    auto it = std::find(tags_.begin(), tags_.end(), tag);
    if (it != tags_.end()) {
        tags_.erase(it);
        markModified();
    }
}

/**
 * @brief 태그 존재 여부 확인
 */
bool DataPointEntity::hasTag(const std::string& tag) const {
    return std::find(tags_.begin(), tags_.end(), tag) != tags_.end();
}

/**
 * @brief 메타데이터 추가/업데이트
 */
void DataPointEntity::setMetadata(const std::string& key, const std::string& value) {
    metadata_[key] = value;
    markModified();
}

/**
 * @brief 메타데이터 조회
 */
std::string DataPointEntity::getMetadata(const std::string& key, const std::string& default_value) const {
    auto it = metadata_.find(key);
    return (it != metadata_.end()) ? it->second : default_value;
}

/**
 * @brief 그룹별 데이터포인트인지 확인
 */
bool DataPointEntity::belongsToGroup(const std::string& group_name) const {
    return group_name_ == group_name;
}

/**
 * @brief 알람/이벤트 생성을 위한 컨텍스트 정보
 */
json DataPointEntity::getAlarmContext() const {
    json context;
    context["point_id"] = getId();
    context["device_id"] = device_id_;
    context["name"] = name_;
    context["group"] = group_name_;
    context["unit"] = unit_;
    context["min_value"] = min_value_;
    context["max_value"] = max_value_;
    context["protocol"] = getProtocol();
    
    // 통계 정보
    context["stats"] = {
        {"read_count", read_count_},
        {"write_count", write_count_},
        {"error_count", error_count_},
        {"last_read", timestampToString(last_read_time_)},
        {"last_write", timestampToString(last_write_time_)}
    };
    
    return context;
}

/**
 * @brief 성능 모니터링 정보 생성
 */
json DataPointEntity::getPerformanceMetrics() const {
    json metrics;
    
    auto now = std::chrono::system_clock::now();
    auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - created_at_).count();
    
    metrics["point_id"] = getId();
    metrics["uptime_ms"] = time_diff;
    metrics["total_reads"] = read_count_;
    metrics["total_writes"] = write_count_;
    metrics["total_errors"] = error_count_;
    metrics["error_rate"] = (read_count_ + write_count_ > 0) ? 
                           static_cast<double>(error_count_) / (read_count_ + write_count_) : 0.0;
    metrics["polling_interval_ms"] = polling_interval_ms_;
    
    // 최근 활동 시간
    metrics["last_read_ago_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_read_time_).count();
    metrics["last_write_ago_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_write_time_).count();
    
    return metrics;
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne