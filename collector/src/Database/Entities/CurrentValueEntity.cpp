/**
 * @file CurrentValueEntity.cpp
 * @brief PulseOne Current Value Entity 구현 (기존 패턴 100% 준수)
 * @author PulseOne Development Team
 * @date 2025-07-28
 */

#include "Database/Entities/CurrentValueEntity.h"
#include <sstream>
#include <iomanip>

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 및 소멸자 (기존 패턴)
// =============================================================================

CurrentValueEntity::CurrentValueEntity() 
    : BaseEntity()
    , data_point_id_(0)
    , virtual_point_id_(0)
    , value_(0.0)
    , raw_value_(0.0)
    , quality_(PulseOne::Enums::DataQuality::GOOD)
    , timestamp_(std::chrono::system_clock::now())
    , redis_key_("")
    , is_from_redis_(false)
    , storage_type_(PulseOne::Enums::StorageType::PERIODIC)
    , last_save_time_(std::chrono::system_clock::time_point::min())
    , last_saved_value_(0.0) {
}

CurrentValueEntity::CurrentValueEntity(int id) 
    : BaseEntity(id)
    , data_point_id_(0)
    , virtual_point_id_(0)
    , value_(0.0)
    , raw_value_(0.0)
    , quality_(PulseOne::Enums::DataQuality::GOOD)
    , timestamp_(std::chrono::system_clock::now())
    , redis_key_("")
    , is_from_redis_(false)
    , storage_type_(PulseOne::Enums::StorageType::PERIODIC)
    , last_save_time_(std::chrono::system_clock::time_point::min())
    , last_saved_value_(0.0) {
}

CurrentValueEntity::CurrentValueEntity(int data_point_id, double value) 
    : BaseEntity()
    , data_point_id_(data_point_id)
    , virtual_point_id_(0)
    , value_(value)
    , raw_value_(value)
    , quality_(PulseOne::Enums::DataQuality::GOOD)
    , timestamp_(std::chrono::system_clock::now())
    , redis_key_("")
    , is_from_redis_(false)
    , storage_type_(PulseOne::Enums::StorageType::PERIODIC)
    , last_save_time_(std::chrono::system_clock::time_point::min())
    , last_saved_value_(0.0) {
    
    // Redis 키 자동 생성
    redis_key_ = generateRedisKey();
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (기존 패턴)
// =============================================================================

bool CurrentValueEntity::loadFromDatabase() {
    if (getId() <= 0) {
        logger_->Error("CurrentValueEntity::loadFromDatabase - Invalid ID: " + std::to_string(getId()));
        return false;
    }
    
    try {
        std::string sql = "SELECT * FROM current_values WHERE id = " + std::to_string(getId());
        auto result = executeUnifiedQuery(sql);
        
        if (result.empty()) {
            logger_->Warn("CurrentValueEntity::loadFromDatabase - Current value not found: " + std::to_string(getId()));
            return false;
        }
        
        const auto& row = result[0];
        
        // 기본 필드들
        if (row.count("data_point_id")) data_point_id_ = std::stoi(row.at("data_point_id"));
        if (row.count("virtual_point_id")) virtual_point_id_ = std::stoi(row.at("virtual_point_id"));
        if (row.count("value")) value_ = std::stod(row.at("value"));
        if (row.count("raw_value")) raw_value_ = std::stod(row.at("raw_value"));
        if (row.count("quality")) setQualityFromString(row.at("quality"));
        
        // 타임스탬프 파싱
        if (row.count("timestamp")) {
            // ISO 8601 형식의 타임스탬프 파싱 로직 추가 필요
            // 현재는 임시로 현재 시간 사용
            timestamp_ = std::chrono::system_clock::now();
        }
        
        // Redis 관련
        if (row.count("redis_key")) redis_key_ = row.at("redis_key");
        
        markSaved();
        logger_->Debug("CurrentValueEntity::loadFromDatabase - Loaded current value: " + std::to_string(getId()));
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueEntity::loadFromDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool CurrentValueEntity::saveToDatabase() {
    try {
        std::string sql;
        
        if (getId() <= 0) {
            // INSERT 쿼리
            sql = "INSERT INTO current_values ("
                  "data_point_id, virtual_point_id, value, raw_value, quality, "
                  "timestamp, redis_key, created_at, updated_at) VALUES ("
                  + std::to_string(data_point_id_) + ", "
                  + std::to_string(virtual_point_id_) + ", "
                  + std::to_string(value_) + ", "
                  + std::to_string(raw_value_) + ", "
                  "'" + getQualityString() + "', "
                  "datetime('now'), "
                  "'" + redis_key_ + "', "
                  "datetime('now'), datetime('now'))";
        } else {
            // UPDATE 쿼리
            sql = "UPDATE current_values SET "
                  "data_point_id = " + std::to_string(data_point_id_) + ", "
                  "virtual_point_id = " + std::to_string(virtual_point_id_) + ", "
                  "value = " + std::to_string(value_) + ", "
                  "raw_value = " + std::to_string(raw_value_) + ", "
                  "quality = '" + getQualityString() + "', "
                  "redis_key = '" + redis_key_ + "', "
                  "updated_at = datetime('now') "
                  "WHERE id = " + std::to_string(getId());
        }
        
        bool success = executeUnifiedNonQuery(sql);
        
        if (success) {
            markSaved();
            last_save_time_ = std::chrono::system_clock::now();
            last_saved_value_ = value_;
            logger_->Debug("CurrentValueEntity::saveToDatabase - Saved current value: " + std::to_string(getId()));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueEntity::saveToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool CurrentValueEntity::updateToDatabase() {
    if (getId() <= 0) {
        logger_->Error("CurrentValueEntity::updateToDatabase - Invalid ID for update");
        return false;
    }
    
    return saveToDatabase();
}

bool CurrentValueEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        logger_->Error("CurrentValueEntity::deleteFromDatabase - Invalid ID");
        return false;
    }
    
    try {
        std::string sql = "DELETE FROM current_values WHERE id = " + std::to_string(getId());
        bool success = executeUnifiedNonQuery(sql);
        
        if (success) {
            markDeleted();
            logger_->Debug("CurrentValueEntity::deleteFromDatabase - Deleted current value: " + std::to_string(getId()));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueEntity::deleteFromDatabase failed: " + std::string(e.what()));
        return false;
    }
}

bool CurrentValueEntity::isValid() const {
    // 데이터포인트 ID는 필수
    if (data_point_id_ <= 0) return false;
    
    // 타임스탬프 유효성 검사
    auto now = std::chrono::system_clock::now();
    auto epoch = std::chrono::system_clock::time_point();
    if (timestamp_ < epoch || timestamp_ > now + std::chrono::hours(1)) return false;
    
    // Redis 키 유효성 (설정된 경우)
    if (!redis_key_.empty() && redis_key_.length() > 255) return false;
    
    return true;
}

json CurrentValueEntity::toJson() const {
    json data = json::object();
    
    try {
        // 기본 정보
        data["id"] = getId();
        data["data_point_id"] = data_point_id_;
        data["virtual_point_id"] = virtual_point_id_;
        data["value"] = value_;
        data["raw_value"] = raw_value_;
        data["quality"] = getQualityString();
        
        // 타임스탬프 (ISO 8601 형식)
        auto timestamp_time_t = std::chrono::system_clock::to_time_t(timestamp_);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&timestamp_time_t), "%Y-%m-%dT%H:%M:%SZ");
        data["timestamp"] = ss.str();
        
        // Redis 관련
        data["redis_key"] = redis_key_;
        data["is_from_redis"] = is_from_redis_;
        data["storage_type"] = storageTypeToString(storage_type_);
        
        // BaseEntity 정보
        data["state"] = static_cast<int>(getState());
        data["created_at"] = ""; // BaseEntity에서 제공되는 정보
        data["updated_at"] = ""; // BaseEntity에서 제공되는 정보
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueEntity::toJson failed: " + std::string(e.what()));
    }
    
    return data;
}

bool CurrentValueEntity::fromJson(const json& data) {
    try {
        // 기본 정보
        if (data.contains("id") && data["id"].is_number()) {
            setId(data["id"].get<int>());
        }
        if (data.contains("data_point_id") && data["data_point_id"].is_number()) {
            data_point_id_ = data["data_point_id"].get<int>();
        }
        if (data.contains("virtual_point_id") && data["virtual_point_id"].is_number()) {
            virtual_point_id_ = data["virtual_point_id"].get<int>();
        }
        if (data.contains("value") && data["value"].is_number()) {
            value_ = data["value"].get<double>();
        }
        if (data.contains("raw_value") && data["raw_value"].is_number()) {
            raw_value_ = data["raw_value"].get<double>();
        }
        if (data.contains("quality") && data["quality"].is_string()) {
            setQualityFromString(data["quality"].get<std::string>());
        }
        
        // Redis 관련
        if (data.contains("redis_key") && data["redis_key"].is_string()) {
            redis_key_ = data["redis_key"].get<std::string>();
        }
        if (data.contains("is_from_redis") && data["is_from_redis"].is_boolean()) {
            is_from_redis_ = data["is_from_redis"].get<bool>();
        }
        if (data.contains("storage_type") && data["storage_type"].is_string()) {
            storage_type_ = stringToStorageType(data["storage_type"].get<std::string>());
        }
        
        markModified();
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueEntity::fromJson failed: " + std::string(e.what()));
        return false;
    }
}

std::string CurrentValueEntity::toString() const {
    std::stringstream ss;
    ss << "CurrentValue[id=" << getId() 
       << ", data_point_id=" << data_point_id_ 
       << ", value=" << value_ 
       << ", quality=" << getQualityString() 
       << ", redis_key=" << redis_key_ << "]";
    return ss.str();
}

// =============================================================================
// 품질 관련 메서드들
// =============================================================================

std::string CurrentValueEntity::getQualityString() const {
    return qualityToString(quality_);
}

void CurrentValueEntity::setQualityFromString(const std::string& quality_str) {
    setQuality(stringToQuality(quality_str));
}

// =============================================================================
// Redis 연동 전용 메서드들
// =============================================================================

bool CurrentValueEntity::loadFromRedis(const std::string& redis_key) {
    // Redis 구현체가 완료되면 실제 로직 구현
    // 현재는 기본 구조만 제공
    logger_->Debug("CurrentValueEntity::loadFromRedis - Key: " + 
                  (redis_key.empty() ? redis_key_ : redis_key));
    return false; // 임시
}

bool CurrentValueEntity::saveToRedis(int ttl_seconds) {
    // Redis 구현체가 완료되면 실제 로직 구현
    logger_->Debug("CurrentValueEntity::saveToRedis - Key: " + redis_key_ + 
                  ", TTL: " + std::to_string(ttl_seconds));
    return false; // 임시
}

std::string CurrentValueEntity::generateRedisKey() const {
    return "current_value:dp:" + std::to_string(data_point_id_);
}

json CurrentValueEntity::toRedisJson() const {
    json redis_data = json::object();
    
    redis_data["id"] = getId();
    redis_data["data_point_id"] = data_point_id_;
    redis_data["value"] = value_;
    redis_data["raw_value"] = raw_value_;
    redis_data["quality"] = getQualityString();
    redis_data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp_.time_since_epoch()).count();
    
    return redis_data;
}

bool CurrentValueEntity::fromRedisJson(const json& redis_data) {
    try {
        if (redis_data.contains("data_point_id")) {
            data_point_id_ = redis_data["data_point_id"].get<int>();
        }
        if (redis_data.contains("value")) {
            value_ = redis_data["value"].get<double>();
        }
        if (redis_data.contains("raw_value")) {
            raw_value_ = redis_data["raw_value"].get<double>();
        }
        if (redis_data.contains("quality")) {
            setQualityFromString(redis_data["quality"].get<std::string>());
        }
        if (redis_data.contains("timestamp")) {
            auto ms = redis_data["timestamp"].get<int64_t>();
            timestamp_ = std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
        }
        
        is_from_redis_ = true;
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueEntity::fromRedisJson failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// RDB 저장 최적화 메서드들
// =============================================================================

bool CurrentValueEntity::needsPeriodicSave(int interval_seconds) const {
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_save_time_).count();
    return elapsed >= interval_seconds;
}

bool CurrentValueEntity::needsOnChangeSave(double deadband) const {
    if (storage_type_ != PulseOne::Enums::StorageType::ON_CHANGE) return false;
    
    double diff = std::abs(value_ - last_saved_value_);
    return diff > deadband;
}

std::string CurrentValueEntity::generateUpsertSql() const {
    std::stringstream ss;
    
    ss << "INSERT OR REPLACE INTO current_values ("
       << "id, data_point_id, virtual_point_id, value, raw_value, quality, "
       << "timestamp, redis_key, updated_at) VALUES ("
       << (getId() > 0 ? std::to_string(getId()) : "NULL") << ", "
       << data_point_id_ << ", "
       << virtual_point_id_ << ", "
       << value_ << ", "
       << raw_value_ << ", "
       << "'" << getQualityString() << "', "
       << "datetime('now'), "
       << "'" << redis_key_ << "', "
       << "datetime('now'))";
    
    return ss.str();
}

std::string CurrentValueEntity::getValuesForBatchInsert() const {
    std::stringstream ss;
    
    ss << "(" << (getId() > 0 ? std::to_string(getId()) : "NULL") << ", "
       << data_point_id_ << ", "
       << virtual_point_id_ << ", "
       << value_ << ", "
       << raw_value_ << ", "
       << "'" << getQualityString() << "', "
       << "datetime('now'), "
       << "'" << redis_key_ << "', "
       << "datetime('now'))";
    
    return ss.str();
}

// =============================================================================
// 고급 기능 메서드들
// =============================================================================

json CurrentValueEntity::getWorkerContext() const {
    json context = json::object();
    
    context["data_point_id"] = data_point_id_;
    context["value"] = value_;
    context["quality"] = getQualityString();
    context["redis_key"] = redis_key_;
    context["storage_type"] = storageTypeToString(storage_type_);
    
    return context;
}

json CurrentValueEntity::getAlarmContext() const {
    json context = json::object();
    
    context["data_point_id"] = data_point_id_;
    context["current_value"] = value_;
    context["quality"] = getQualityString();
    context["is_good_quality"] = isGoodQuality();
    context["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp_.time_since_epoch()).count();
    
    return context;
}

bool CurrentValueEntity::hasChangedFrom(const CurrentValueEntity& other) const {
    const double EPSILON = 1e-6;
    return std::abs(value_ - other.value_) > EPSILON || 
           quality_ != other.quality_ ||
           data_point_id_ != other.data_point_id_;
}

// =============================================================================
// 정적 헬퍼 메서드들
// =============================================================================

std::string CurrentValueEntity::qualityToString(PulseOne::Enums::DataQuality quality) {
    switch (quality) {
        case PulseOne::Enums::DataQuality::GOOD: return "GOOD";
        case PulseOne::Enums::DataQuality::BAD: return "BAD";
        case PulseOne::Enums::DataQuality::UNCERTAIN: return "UNCERTAIN";
        case PulseOne::Enums::DataQuality::NOT_CONNECTED: return "NOT_CONNECTED";
        case PulseOne::Enums::DataQuality::SCAN_DELAYED: return "SCAN_DELAYED";
        case PulseOne::Enums::DataQuality::UNDER_MAINTENANCE: return "UNDER_MAINTENANCE";
        case PulseOne::Enums::DataQuality::STALE_DATA: return "STALE_DATA";
        case PulseOne::Enums::DataQuality::VERY_STALE_DATA: return "VERY_STALE_DATA";
        case PulseOne::Enums::DataQuality::MAINTENANCE_BLOCKED: return "MAINTENANCE_BLOCKED";
        case PulseOne::Enums::DataQuality::ENGINEER_OVERRIDE: return "ENGINEER_OVERRIDE";
        default: return "UNKNOWN";
    }
}

PulseOne::Enums::DataQuality CurrentValueEntity::stringToQuality(const std::string& quality_str) {
    if (quality_str == "GOOD") return PulseOne::Enums::DataQuality::GOOD;
    if (quality_str == "BAD") return PulseOne::Enums::DataQuality::BAD;
    if (quality_str == "UNCERTAIN") return PulseOne::Enums::DataQuality::UNCERTAIN;
    if (quality_str == "NOT_CONNECTED") return PulseOne::Enums::DataQuality::NOT_CONNECTED;
    if (quality_str == "SCAN_DELAYED") return PulseOne::Enums::DataQuality::SCAN_DELAYED;
    if (quality_str == "UNDER_MAINTENANCE") return PulseOne::Enums::DataQuality::UNDER_MAINTENANCE;
    if (quality_str == "STALE_DATA") return PulseOne::Enums::DataQuality::STALE_DATA;
    if (quality_str == "VERY_STALE_DATA") return PulseOne::Enums::DataQuality::VERY_STALE_DATA;
    if (quality_str == "MAINTENANCE_BLOCKED") return PulseOne::Enums::DataQuality::MAINTENANCE_BLOCKED;
    if (quality_str == "ENGINEER_OVERRIDE") return PulseOne::Enums::DataQuality::ENGINEER_OVERRIDE;
    return PulseOne::Enums::DataQuality::BAD;  // 기본값
}

std::string CurrentValueEntity::storageTypeToString(PulseOne::Enums::StorageType type) {
    switch (type) {
        case PulseOne::Enums::StorageType::IMMEDIATE: return "IMMEDIATE";
        case PulseOne::Enums::StorageType::ON_CHANGE: return "ON_CHANGE";
        case PulseOne::Enums::StorageType::PERIODIC: return "PERIODIC";
        case PulseOne::Enums::StorageType::BUFFERED: return "BUFFERED";
        default: return "UNKNOWN";
    }
}

PulseOne::Enums::StorageType CurrentValueEntity::stringToStorageType(const std::string& type_str) {
    if (type_str == "IMMEDIATE") return PulseOne::Enums::StorageType::IMMEDIATE;
    if (type_str == "ON_CHANGE") return PulseOne::Enums::StorageType::ON_CHANGE;
    if (type_str == "PERIODIC") return PulseOne::Enums::StorageType::PERIODIC;
    if (type_str == "BUFFERED") return PulseOne::Enums::StorageType::BUFFERED;
    return PulseOne::Enums::StorageType::PERIODIC;  // 기본값
}


} // namespace Entities
} // namespace Database
} // namespace PulseOne