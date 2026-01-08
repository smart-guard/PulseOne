/**
 * @file DataPointEntity.cpp
 * @brief PulseOne DataPointEntity 구현 (현재 스키마 완전 호환 + 품질/알람 필드 추가)
 * @author PulseOne Development Team
 * @date 2025-08-26
 * 
 * 🎯 현재 DB 스키마 완전 반영:
 * - 품질 관리: quality_check_enabled, range_check_enabled, rate_of_change_limit
 * - 알람 관리: alarm_enabled, alarm_priority
 * - 기존 모든 필드 유지 및 확장
 * 
 * 🎯 DeviceSettingsEntity 패턴 완전 적용:
 * - Repository 패턴으로 DB 작업 위임
 * - 순환 참조 방지 설계
 * - BaseEntity 인터페이스 완전 구현
 */

#include "Database/Entities/DataPointEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/DataPointRepository.h"

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 구현 (모든 필드 초기화)
// =============================================================================

DataPointEntity::DataPointEntity() 
    : BaseEntity<DataPointEntity>()
    , device_id_(0)
    , name_("")
    , description_("")
    , address_(0)
    , address_string_("")
    , data_type_("UNKNOWN")
    , access_mode_("read")
    , is_enabled_(true)
    , is_writable_(false)
    , unit_("")
    , scaling_factor_(1.0)
    , scaling_offset_(0.0)
    , min_value_(std::numeric_limits<double>::lowest())
    , max_value_(std::numeric_limits<double>::max())
    , is_log_enabled_(true)
    , log_interval_ms_(0)
    , log_deadband_(0.0)
    , polling_interval_ms_(1000)
    // 품질 관리 필드 초기화 (새로 추가)
    , is_quality_check_enabled_(true)
    , is_range_check_enabled_(true)
    , rate_of_change_limit_(0.0)
    // 알람 관련 필드 초기화 (새로 추가)
    , is_alarm_enabled_(false)
    , alarm_priority_("medium")
    // 메타데이터
    , group_name_("")
    , tags_()
    , metadata_()
    , protocol_params_()
    // 시간 정보
    , created_at_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now())
    // 통계 정보 (런타임)
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
        
        // Repository의 save 메서드 호출
        DataPointEntity mutable_copy = *this;
        bool success = repo->save(mutable_copy);
        
        if (success) {
            // 저장 성공 시 ID와 상태 업데이트
            if (getId() <= 0) {
                setId(mutable_copy.getId());
            }
            markSaved();
            updateTimestamps();
            
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
        updateTimestamps();
        
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
// 품질 관리 메서드 구현
// =============================================================================

bool DataPointEntity::validateValue(double value) const {
    if (!is_quality_check_enabled_) return true;
    
    bool valid = true;
    
    // 범위 체크
    if (is_range_check_enabled_) {
        valid = valid && (value >= min_value_) && (value <= max_value_);
    }
    
    // NaN이나 무한대 체크
    valid = valid && std::isfinite(value);
    
    return valid;
}

bool DataPointEntity::isRateOfChangeViolation(double previous_value, double current_value, 
                                            double time_diff_seconds) const {
    if (rate_of_change_limit_ <= 0.0 || time_diff_seconds <= 0.0) {
        return false;
    }
    
    double rate = std::abs(current_value - previous_value) / time_diff_seconds;
    return rate > rate_of_change_limit_;
}

// =============================================================================
// 프로토콜 및 유틸리티 메서드 구현
// =============================================================================

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
            data_type_ != "DISCRETE_INPUT" &&
            data_type_ != "INT16" &&
            data_type_ != "UINT16" &&
            data_type_ != "INT32" &&
            data_type_ != "UINT32" &&
            data_type_ != "FLOAT32" &&
            data_type_ != "BOOLEAN") {
            return false;
        }
        
        // Modbus 스테이션 ID 확인
        std::string station_id = getProtocolParam("station_id", "1");
        try {
            int id = std::stoi(station_id);
            if (id < 1 || id > 247) {
                return false;
            }
        } catch (const std::exception&) {
            return false;
        }
        
    } else if (protocol == "MQTT") {
        // MQTT 토픽 유효성 확인
        if (address_string_.empty() || address_string_.find('#') == 0) {
            return false;
        }
        
        // 와일드카드 사용 시 쓰기 불가
        if (is_writable_ && (address_string_.find('+') != std::string::npos || 
                            address_string_.find('#') != std::string::npos)) {
            return false;
        }
        
        // QoS 레벨 확인
        std::string qos = getProtocolParam("qos", "0");
        if (qos != "0" && qos != "1" && qos != "2") {
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
        
        // 유효한 BACnet 객체 타입인지 확인
        if (object_type != "ANALOG_INPUT" && 
            object_type != "ANALOG_OUTPUT" && 
            object_type != "ANALOG_VALUE" &&
            object_type != "BINARY_INPUT" && 
            object_type != "BINARY_OUTPUT" && 
            object_type != "BINARY_VALUE" &&
            object_type != "MULTI_STATE_INPUT" && 
            object_type != "MULTI_STATE_OUTPUT" && 
            object_type != "MULTI_STATE_VALUE") {
            return false;
        }
        
        // Device ID 확인
        std::string device_id = getProtocolParam("device_id");
        if (device_id.empty()) {
            return false;
        }
        
    } else if (protocol == "OPC_UA") {
        // OPC UA NodeId 확인
        if (address_string_.empty()) {
            return false;
        }
        
        // NodeId 형식 확인 (ns=숫자;형식)
        if (address_string_.find("ns=") != 0) {
            return false;
        }
        
    } else if (protocol == "SNMP") {
        // SNMP OID 확인
        if (address_string_.empty()) {
            return false;
        }
        
        // OID 형식 확인 (숫자.숫자.숫자...)
        std::string oid = address_string_;
        if (oid.front() != '1' && oid.front() != '2') {
            return false;
        }
        
        // Community string 확인
        std::string community = getProtocolParam("community", "public");
        if (community.empty()) {
            return false;
        }
    }
    
    return true;
}

double DataPointEntity::applyScaling(double raw_value) const {
    return (raw_value * scaling_factor_) + scaling_offset_;
}

double DataPointEntity::removeScaling(double scaled_value) const {
    if (scaling_factor_ == 0.0) {
        if (logger_) {
            logger_->Warn("DataPointEntity::removeScaling - Zero scaling factor, returning original value");
        }
        return scaled_value;
    }
    return (scaled_value - scaling_offset_) / scaling_factor_;
}

bool DataPointEntity::isWithinDeadband(double previous_value, double new_value) const {
    if (log_deadband_ <= 0.0) {
        return false;  // 데드밴드가 설정되지 않음
    }
    
    double diff = std::abs(new_value - previous_value);
    return diff <= log_deadband_;
}

bool DataPointEntity::isValueInRange(double value) const {
    return value >= min_value_ && value <= max_value_;
}

void DataPointEntity::adjustPollingInterval(bool connection_healthy) {
    if (!connection_healthy) {
        // 연결 불안정 시 폴링 주기 증가 (지수 백오프)
        uint32_t new_interval = polling_interval_ms_ * 2;
        uint32_t max_interval = 60000; // 최대 60초
        
        // 원래 폴링 간격 저장 (처음 한 번만)
        if (metadata_.find("original_polling_interval") == metadata_.end()) {
            setMetadata("original_polling_interval", std::to_string(polling_interval_ms_));
        }
        
        polling_interval_ms_ = std::min(new_interval, max_interval);
        markModified();
        
        if (logger_) {
            logger_->Info("DataPointEntity::adjustPollingInterval - Increased polling interval to " + 
                         std::to_string(polling_interval_ms_) + "ms for point " + name_);
        }
        
    } else {
        // 연결 안정 시 원래 주기로 복원
        auto it = metadata_.find("original_polling_interval");
        if (it != metadata_.end()) {
            try {
                uint32_t original = static_cast<uint32_t>(std::stoul(it->second));
                if (polling_interval_ms_ != original) {
                    polling_interval_ms_ = original;
                    markModified();
                    
                    if (logger_) {
                        logger_->Info("DataPointEntity::adjustPollingInterval - Restored polling interval to " + 
                                     std::to_string(polling_interval_ms_) + "ms for point " + name_);
                    }
                }
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Error("DataPointEntity::adjustPollingInterval - Failed to parse original interval: " + 
                                  std::string(e.what()));
                }
            }
        }
    }
}

// =============================================================================
// 태그 관리 메서드 구현
// =============================================================================

void DataPointEntity::addTag(const std::string& tag) {
    if (tag.empty()) return;
    
    auto it = std::find(tags_.begin(), tags_.end(), tag);
    if (it == tags_.end()) {
        tags_.push_back(tag);
        markModified();
        
        if (logger_) {
            logger_->Debug("DataPointEntity::addTag - Added tag '" + tag + "' to point " + name_);
        }
    }
}

void DataPointEntity::removeTag(const std::string& tag) {
    auto it = std::find(tags_.begin(), tags_.end(), tag);
    if (it != tags_.end()) {
        tags_.erase(it);
        markModified();
        
        if (logger_) {
            logger_->Debug("DataPointEntity::removeTag - Removed tag '" + tag + "' from point " + name_);
        }
    }
}

bool DataPointEntity::hasTag(const std::string& tag) const {
    return std::find(tags_.begin(), tags_.end(), tag) != tags_.end();
}

// =============================================================================
// 메타데이터 관리 메서드 구현
// =============================================================================

void DataPointEntity::setMetadata(const std::string& key, const std::string& value) {
    if (key.empty()) return;
    
    metadata_[key] = value;
    markModified();
    
    if (logger_) {
        logger_->Debug("DataPointEntity::setMetadata - Set metadata['" + key + "'] = '" + value + 
                      "' for point " + name_);
    }
}

std::string DataPointEntity::getMetadata(const std::string& key, const std::string& default_value) const {
    auto it = metadata_.find(key);
    return (it != metadata_.end()) ? it->second : default_value;
}

bool DataPointEntity::belongsToGroup(const std::string& group_name) const {
    return group_name_ == group_name;
}

// =============================================================================
// 컨텍스트 및 메트릭 정보 생성 메서드
// =============================================================================

json DataPointEntity::getAlarmContext() const {
    json context;
    context["point_id"] = getId();
    context["device_id"] = device_id_;
    context["name"] = name_;
    context["description"] = description_;
    context["group"] = group_name_;
    context["unit"] = unit_;
    context["data_type"] = data_type_;
    context["min_value"] = min_value_;
    context["max_value"] = max_value_;
    context["protocol"] = getProtocol();
    context["address"] = address_;
    context["address_string"] = address_string_;
    
    // 알람 관련 정보
    context["alarm_enabled"] = is_alarm_enabled_;
    context["alarm_priority"] = alarm_priority_;
    
    // 품질 관리 정보
    context["quality_check_enabled"] = is_quality_check_enabled_;
    context["range_check_enabled"] = is_range_check_enabled_;
    context["rate_of_change_limit"] = rate_of_change_limit_;
    
    // 통계 정보
    context["stats"] = {
        {"read_count", read_count_},
        {"write_count", write_count_},
        {"error_count", error_count_},
        {"last_read", timestampToString(last_read_time_)},
        {"last_write", timestampToString(last_write_time_)}
    };
    
    // 태그 정보
    context["tags"] = tags_;
    
    // 메타데이터 선택적 포함
    if (!metadata_.empty()) {
        context["metadata"] = metadata_;
    }
    
    return context;
}

json DataPointEntity::getPerformanceMetrics() const {
    json metrics;
    
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::milliseconds>(now - created_at_).count();
    
    metrics["point_id"] = getId();
    metrics["device_id"] = device_id_;
    metrics["name"] = name_;
    metrics["uptime_ms"] = uptime;
    
    // 활동 통계
    metrics["total_reads"] = read_count_;
    metrics["total_writes"] = write_count_;
    metrics["total_errors"] = error_count_;
    
    // 성공률 계산
    uint64_t total_operations = read_count_ + write_count_;
    metrics["success_rate"] = (total_operations > 0) ? 
                             static_cast<double>(total_operations - error_count_) / total_operations : 1.0;
    
    metrics["error_rate"] = (total_operations > 0) ? 
                           static_cast<double>(error_count_) / total_operations : 0.0;
    
    // 폴링 정보
    metrics["polling_interval_ms"] = polling_interval_ms_;
    metrics["log_enabled"] = is_log_enabled_;
    metrics["log_interval_ms"] = log_interval_ms_;
    
    // 최근 활동 시간
    auto last_read_ago = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_read_time_).count();
    auto last_write_ago = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_write_time_).count();
    
    metrics["last_read_ago_ms"] = last_read_ago;
    metrics["last_write_ago_ms"] = last_write_ago;
    
    // 활성 상태 판단 (최근 5분 내 읽기 활동 있으면 활성)
    metrics["is_active"] = (last_read_ago < 300000); // 5분 = 300,000ms
    
    // 품질 및 알람 상태
    metrics["quality_enabled"] = is_quality_check_enabled_;
    metrics["range_check_enabled"] = is_range_check_enabled_;
    metrics["alarm_enabled"] = is_alarm_enabled_;
    
    return metrics;
}

// =============================================================================
// 내부 유틸리티 메서드 구현
// =============================================================================

void DataPointEntity::updateTimestamps() {
    updated_at_ = std::chrono::system_clock::now();
    markModified();
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne