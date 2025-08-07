#ifndef DATA_POINT_ENTITY_H
#define DATA_POINT_ENTITY_H

/**
 * @file DataPointEntity.h
 * @brief PulseOne DataPointEntity - 새 스키마 완전 호환 버전
 * @author PulseOne Development Team
 * @date 2025-08-07
 * 
 * 🎯 새 data_points 테이블 스키마 완전 반영:
 * - address_string, is_writable, polling_interval_ms 추가
 * - group_name, protocol_params 추가 (JSON 통합)
 * - Struct DataPoint와 100% 호환
 */

#include "Database/Entities/BaseEntity.h"
#include "Common/Utils.h"
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <map>
#include <sstream>
#include <iomanip>
#include <limits>
#include <set>

#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#else
struct json {
    template<typename T> T get() const { return T{}; }
    bool contains(const std::string&) const { return false; }
    std::string dump() const { return "{}"; }
    static json parse(const std::string&) { return json{}; }
    static json object() { return json{}; }
};
#endif

// 매크로 충돌 방지
#ifdef data
#undef data
#endif

namespace PulseOne {

// Forward declarations
namespace Database {
namespace Repositories {
    class DataPointRepository;
}
}

namespace Database {
namespace Entities {

/**
 * @brief 데이터포인트 엔티티 클래스 (새 스키마 완전 호환)
 * 
 * 🎯 새 DB 스키마 매핑:
 * CREATE TABLE data_points (
 *     id INTEGER PRIMARY KEY AUTOINCREMENT,
 *     device_id INTEGER NOT NULL,
 *     name VARCHAR(100) NOT NULL,
 *     description TEXT,
 *     address INTEGER NOT NULL,
 *     address_string VARCHAR(255),                 -- 🔥 새로 추가
 *     data_type VARCHAR(20) NOT NULL DEFAULT 'UNKNOWN',
 *     access_mode VARCHAR(10) DEFAULT 'read',
 *     is_enabled INTEGER DEFAULT 1,
 *     is_writable INTEGER DEFAULT 0,              -- 🔥 새로 추가
 *     unit VARCHAR(50),
 *     scaling_factor REAL DEFAULT 1.0,
 *     scaling_offset REAL DEFAULT 0.0,            -- 🔥 새로 추가
 *     min_value REAL DEFAULT 0.0,
 *     max_value REAL DEFAULT 0.0,
 *     log_enabled INTEGER DEFAULT 1,              -- 🔥 기존 컬럼
 *     log_interval_ms INTEGER DEFAULT 0,          -- 🔥 기존 컬럼
 *     log_deadband REAL DEFAULT 0.0,              -- 🔥 기존 컬럼
 *     polling_interval_ms INTEGER DEFAULT 0,      -- 🔥 새로 추가
 *     group_name VARCHAR(50),                      -- 🔥 새로 추가
 *     tags TEXT,                                   -- 🔥 기존 컬럼
 *     metadata TEXT,                               -- 🔥 기존 컬럼
 *     protocol_params TEXT,                        -- 🔥 새로 추가 (JSON)
 *     created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 *     updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
 * );
 */
class DataPointEntity : public BaseEntity<DataPointEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    DataPointEntity();
    explicit DataPointEntity(int point_id);
    virtual ~DataPointEntity() = default;

    DataPointEntity(const DataPointEntity& other) = default;
    DataPointEntity& operator=(const DataPointEntity& other) = default;
    DataPointEntity(DataPointEntity&& other) noexcept = default;
    DataPointEntity& operator=(DataPointEntity&& other) noexcept = default;    

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (CPP에서 구현)
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool updateToDatabase() override;

    // =======================================================================
    // JSON 직렬화/역직렬화 (새 필드들 포함)
    // =======================================================================
    
    json toJson() const override {
        json j;
        try {
            j["id"] = getId();
            j["device_id"] = device_id_;
            j["name"] = name_;
            j["description"] = description_;
            j["address"] = address_;
            j["address_string"] = address_string_;           // 🔥 새 필드
            j["data_type"] = data_type_;
            j["access_mode"] = access_mode_;
            j["is_enabled"] = is_enabled_;
            j["is_writable"] = is_writable_;                 // 🔥 새 필드
            j["unit"] = unit_;
            j["scaling_factor"] = scaling_factor_;
            j["scaling_offset"] = scaling_offset_;           // 🔥 새 필드
            j["min_value"] = min_value_;
            j["max_value"] = max_value_;
            j["log_enabled"] = log_enabled_;
            j["log_interval_ms"] = log_interval_ms_;
            j["log_deadband"] = log_deadband_;
            j["polling_interval_ms"] = polling_interval_ms_; // 🔥 새 필드
            j["group_name"] = group_name_;                   // 🔥 새 필드
            j["tags"] = tags_;
            j["metadata"] = metadata_;
            j["protocol_params"] = protocol_params_;         // 🔥 새 필드
            
            // 시간 정보
            j["created_at"] = timestampToString(created_at_);
            j["updated_at"] = timestampToString(updated_at_);
            
        } catch (const std::exception& e) {
            if (logger_) logger_->Error("DataPointEntity::toJson failed: " + std::string(e.what()));
        }
        return j;
    }
    
    bool fromJson(const json& json_data) override {
        try {
            if (json_data.contains("id")) setId(json_data["id"]);
            if (json_data.contains("device_id")) device_id_ = json_data["device_id"];
            if (json_data.contains("name")) name_ = json_data["name"];
            if (json_data.contains("description")) description_ = json_data["description"];
            if (json_data.contains("address")) address_ = json_data["address"];
            if (json_data.contains("address_string")) address_string_ = json_data["address_string"];           // 🔥 새 필드
            if (json_data.contains("data_type")) data_type_ = json_data["data_type"];
            if (json_data.contains("access_mode")) access_mode_ = json_data["access_mode"];
            if (json_data.contains("is_enabled")) is_enabled_ = json_data["is_enabled"];
            if (json_data.contains("is_writable")) is_writable_ = json_data["is_writable"];                 // 🔥 새 필드
            if (json_data.contains("unit")) unit_ = json_data["unit"];
            if (json_data.contains("scaling_factor")) scaling_factor_ = json_data["scaling_factor"];
            if (json_data.contains("scaling_offset")) scaling_offset_ = json_data["scaling_offset"];         // 🔥 새 필드
            if (json_data.contains("min_value")) min_value_ = json_data["min_value"];
            if (json_data.contains("max_value")) max_value_ = json_data["max_value"];
            if (json_data.contains("log_enabled")) log_enabled_ = json_data["log_enabled"];
            if (json_data.contains("log_interval_ms")) log_interval_ms_ = json_data["log_interval_ms"];
            if (json_data.contains("log_deadband")) log_deadband_ = json_data["log_deadband"];
            if (json_data.contains("polling_interval_ms")) polling_interval_ms_ = json_data["polling_interval_ms"]; // 🔥 새 필드
            if (json_data.contains("group_name")) group_name_ = json_data["group_name"];                     // 🔥 새 필드
            if (json_data.contains("tags")) tags_ = json_data["tags"];
            if (json_data.contains("metadata")) metadata_ = json_data["metadata"];
            if (json_data.contains("protocol_params")) protocol_params_ = json_data["protocol_params"];     // 🔥 새 필드
            
            markModified();
            return true;
        } catch (const std::exception& e) {
            if (logger_) logger_->Error("DataPointEntity::fromJson failed: " + std::string(e.what()));
            return false;
        }
    }

    // =======================================================================
    // Getter 메서드들 (새 필드들 포함)
    // =======================================================================
    
    int getDeviceId() const { return device_id_; }
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    int getAddress() const { return address_; }
    const std::string& getAddressString() const { return address_string_; }           // 🔥 새 필드
    const std::string& getDataType() const { return data_type_; }
    const std::string& getAccessMode() const { return access_mode_; }
    bool isEnabled() const { return is_enabled_; }
    bool isWritable() const { return is_writable_; }                                 // 🔥 새 필드
    const std::string& getUnit() const { return unit_; }
    double getScalingFactor() const { return scaling_factor_; }
    double getScalingOffset() const { return scaling_offset_; }                      // 🔥 새 필드
    double getMinValue() const { return min_value_; }
    double getMaxValue() const { return max_value_; }
    bool isLogEnabled() const { return log_enabled_; }
    int getLogInterval() const { return log_interval_ms_; }
    double getLogDeadband() const { return log_deadband_; }
    uint32_t getPollingInterval() const { return polling_interval_ms_; }             // 🔥 새 필드
    const std::string& getGroup() const { return group_name_; }                      // 🔥 새 필드
    const std::vector<std::string>& getTags() const { return tags_; }
    const std::map<std::string, std::string>& getMetadata() const { return metadata_; }
    const std::map<std::string, std::string>& getProtocolParams() const { return protocol_params_; } // 🔥 새 필드
    
    // 시간 정보
    const std::chrono::system_clock::time_point& getCreatedAt() const { return created_at_; }
    const std::chrono::system_clock::time_point& getUpdatedAt() const { return updated_at_; }
    std::chrono::system_clock::time_point getLastReadTime() const { return last_read_time_; }
    std::chrono::system_clock::time_point getLastWriteTime() const { return last_write_time_; }

    // =======================================================================
    // Setter 메서드들 (새 필드들 포함)
    // =======================================================================
    
    void setDeviceId(int device_id) { device_id_ = device_id; markModified(); }
    void setName(const std::string& name) { name_ = name; markModified(); }
    void setDescription(const std::string& description) { description_ = description; markModified(); }
    void setAddress(int address) { address_ = address; markModified(); }
    void setAddressString(const std::string& address_string) { address_string_ = address_string; markModified(); } // 🔥 새 필드
    void setAccessMode(const std::string& access_mode) { access_mode_ = access_mode; markModified(); }
    void setEnabled(bool enabled) { is_enabled_ = enabled; markModified(); }
    void setWritable(bool writable) { is_writable_ = writable; markModified(); }     // 🔥 새 필드
    void setUnit(const std::string& unit) { unit_ = unit; markModified(); }
    void setScalingFactor(double factor) { scaling_factor_ = factor; markModified(); }
    void setScalingOffset(double offset) { scaling_offset_ = offset; markModified(); } // 🔥 새 필드
    void setMinValue(double min_val) { min_value_ = min_val; markModified(); }
    void setMaxValue(double max_val) { max_value_ = max_val; markModified(); }
    void setLogEnabled(bool enabled) { log_enabled_ = enabled; markModified(); }
    void setLogInterval(int interval_ms) { log_interval_ms_ = interval_ms; markModified(); }
    void setLogDeadband(double deadband) { log_deadband_ = deadband; markModified(); }
    void setPollingInterval(uint32_t interval_ms) { polling_interval_ms_ = interval_ms; markModified(); } // 🔥 새 필드
    void setGroup(const std::string& group_name) { group_name_ = group_name; markModified(); }           // 🔥 새 필드
    void setTags(const std::vector<std::string>& tags) { tags_ = tags; markModified(); }
    void setMetadata(const std::map<std::string, std::string>& metadata) { metadata_ = metadata; markModified(); }
    void setProtocolParams(const std::map<std::string, std::string>& protocol_params) { protocol_params_ = protocol_params; markModified(); } // 🔥 새 필드
    void setCreatedAt(const std::string& timestamp_str) { 
        created_at_ = PulseOne::Utils::ParseTimestampFromString(timestamp_str); 
        markModified(); 
    }

    void setUpdatedAt(const std::string& timestamp_str) { 
        updated_at_ = PulseOne::Utils::ParseTimestampFromString(timestamp_str); 
        markModified(); 
    }

    // 또는 time_point를 직접 받는 버전도 추가:
    void setCreatedAt(const std::chrono::system_clock::time_point& timestamp) {
        created_at_ = timestamp;
        markModified();
    }

    void setUpdatedAt(const std::chrono::system_clock::time_point& timestamp) {
        updated_at_ = timestamp;
        markModified();
    }

    void setDataType(const std::string& data_type) {
        // 🚀 Utils의 표준 정규화 함수 사용
        data_type_ = PulseOne::Utils::NormalizeDataType(data_type);
        markModified();
    }
    // =======================================================================
    // 유틸리티 메서드들 (개선됨)
    // =======================================================================
    
    /**
     * @brief 쓰기 가능 여부 확인 (is_writable 필드 활용)
     */
    bool canWrite() const {
        return is_writable_ && is_enabled_;
    }
    
    /**
     * @brief 읽기 가능 여부 확인
     */
    bool canRead() const {
        return (access_mode_ == "read" || access_mode_ == "read_write") && is_enabled_;
    }
    
    /**
     * @brief 유효성 검사 (새 필드들 포함)
     */
    bool isValid() const override {
        return device_id_ > 0 && 
               !name_.empty() && 
               !data_type_.empty() && 
               scaling_factor_ != 0.0 &&
               min_value_ <= max_value_ &&
               (access_mode_ == "read" || access_mode_ == "write" || access_mode_ == "read_write");
    }

    /**
     * @brief 모든 설정을 기본값으로 리셋 (새 필드들 포함)
     */
    void resetToDefault() {
        address_string_.clear();                             // 🔥 새 필드
        is_writable_ = false;                                // 🔥 새 필드
        scaling_factor_ = 1.0;
        scaling_offset_ = 0.0;                               // 🔥 새 필드
        min_value_ = std::numeric_limits<double>::lowest();
        max_value_ = std::numeric_limits<double>::max();
        log_enabled_ = true;
        log_interval_ms_ = 0;
        log_deadband_ = 0.0;
        polling_interval_ms_ = 1000;                         // 🔥 새 필드 (기본값 1초)
        group_name_.clear();                                 // 🔥 새 필드
        is_enabled_ = true;
        access_mode_ = "read";
        tags_.clear();
        metadata_.clear();
        protocol_params_.clear();                            // 🔥 새 필드
        markModified();
    }

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현
    // =======================================================================
    
    std::string toString() const override {
        std::ostringstream oss;
        oss << "DataPoint{id=" << getId() 
            << ", device_id=" << device_id_
            << ", name='" << name_ << "'"
            << ", address=" << address_;
        
        if (!address_string_.empty()) {
            oss << ", address_string='" << address_string_ << "'";
        }
        
        oss << ", data_type='" << data_type_ << "'"
            << ", enabled=" << (is_enabled_ ? "true" : "false")
            << ", writable=" << (is_writable_ ? "true" : "false");  // 🔥 새 필드
            
        if (!group_name_.empty()) {
            oss << ", group='" << group_name_ << "'";                // 🔥 새 필드
        }
        
        oss << "}";
        return oss.str();
    }
    
    std::string getTableName() const override {
        return "data_points";
    }

    // =======================================================================
    // 프로토콜 관련 유틸리티 메서드들 (새로 추가)
    // =======================================================================
    
    /**
     * @brief 프로토콜 타입 반환
     */
    std::string getProtocol() const {
        auto it = protocol_params_.find("protocol");
        return (it != protocol_params_.end()) ? it->second : "UNKNOWN";
    }
    
    /**
     * @brief 프로토콜 파라미터 설정
     */
    void setProtocolParam(const std::string& key, const std::string& value) {
        protocol_params_[key] = value;
        markModified();
    }
    
    /**
     * @brief 프로토콜 파라미터 조회
     */
    std::string getProtocolParam(const std::string& key, const std::string& default_value = "") const {
        auto it = protocol_params_.find(key);
        return (it != protocol_params_.end()) ? it->second : default_value;
    }
    
    /**
     * @brief Modbus 관련 설정 확인
     */
    bool isModbusPoint() const {
        return getProtocol() == "MODBUS_TCP" || getProtocol() == "MODBUS_RTU";
    }
    
    /**
     * @brief MQTT 관련 설정 확인
     */
    bool isMqttPoint() const {
        return getProtocol() == "MQTT";
    }
    
    /**
     * @brief BACnet 관련 설정 확인
     */
    bool isBacnetPoint() const {
        return getProtocol() == "BACNET_IP" || getProtocol() == "BACNET";
    }

    // =======================================================================
    // Worker용 컨텍스트 정보 메서드
    // =======================================================================
    
    json getWorkerContext() const {
        json context;
        context["id"] = getId();
        context["device_id"] = device_id_;
        context["name"] = name_;
        context["address"] = address_;
        context["address_string"] = address_string_;
        context["data_type"] = data_type_;
        context["access_mode"] = access_mode_;
        context["is_enabled"] = is_enabled_;
        context["is_writable"] = is_writable_;
        context["polling_interval_ms"] = polling_interval_ms_;
        context["protocol_params"] = protocol_params_;
        context["scaling_factor"] = scaling_factor_;
        context["scaling_offset"] = scaling_offset_;
        context["unit"] = unit_;
        return context;
    }

    // =======================================================================
    // 통계 정보 접근 메서드들
    // =======================================================================
    
    void incrementReadCount() const {
        read_count_++;
        last_read_time_ = std::chrono::system_clock::now();
    }
    
    void incrementWriteCount() const {
        write_count_++;
        last_write_time_ = std::chrono::system_clock::now();
    }
    
    void incrementErrorCount() const {
        error_count_++;
    }
    
    uint64_t getReadCount() const { return read_count_; }
    uint64_t getWriteCount() const { return write_count_; }
    uint64_t getErrorCount() const { return error_count_; }

    bool validateProtocolSpecific() const;
    double applyScaling(double raw_value) const;
    double removeScaling(double scaled_value) const;
    bool isWithinDeadband(double previous_value, double new_value) const;
    bool isValueInRange(double value) const;
    void adjustPollingInterval(bool connection_healthy);
    void addTag(const std::string& tag);
    void removeTag(const std::string& tag);
    bool hasTag(const std::string& tag) const;
    void setMetadata(const std::string& key, const std::string& value);      // 🔥 오버로드 버전
    std::string getMetadata(const std::string& key, const std::string& default_value = "") const;  // 🔥 오버로드 버전
    bool belongsToGroup(const std::string& group_name) const;
    json getAlarmContext() const;
    json getPerformanceMetrics() const;    

private:
    // =======================================================================
    // 유틸리티 함수들
    // =======================================================================
    
    std::string timestampToString(const std::chrono::system_clock::time_point& tp) const {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    // =======================================================================
    // 멤버 변수들 (새 DB 스키마와 1:1 매핑)
    // =======================================================================
    
    // 🔥 기본 정보
    int device_id_ = 0;
    std::string name_;
    std::string description_;
    int address_ = 0;
    std::string address_string_;                             // 🔥 새 필드 (MQTT 토픽, OPC UA NodeId 등)
    std::string data_type_ = "UNKNOWN";
    std::string access_mode_ = "read";
    bool is_enabled_ = true;
    bool is_writable_ = false;                               // 🔥 새 필드
    
    // 🔥 엔지니어링 정보
    std::string unit_;
    double scaling_factor_ = 1.0;
    double scaling_offset_ = 0.0;                            // 🔥 새 필드
    double min_value_ = std::numeric_limits<double>::lowest();
    double max_value_ = std::numeric_limits<double>::max();
    
    // 🔥 로깅 및 수집 설정
    bool log_enabled_ = true;
    int log_interval_ms_ = 0;
    double log_deadband_ = 0.0;
    uint32_t polling_interval_ms_ = 1000;                    // 🔥 새 필드
    
    // 🔥 메타데이터 (확장됨)
    std::string group_name_;                                 // 🔥 새 필드
    std::vector<std::string> tags_;
    std::map<std::string, std::string> metadata_;
    std::map<std::string, std::string> protocol_params_;    // 🔥 새 필드 (JSON으로 저장됨)
    
    // 시간 정보
    std::chrono::system_clock::time_point created_at_ = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point updated_at_ = std::chrono::system_clock::now();
    
    // 통계 정보 (런타임)
    mutable std::chrono::system_clock::time_point last_read_time_ = std::chrono::system_clock::now();
    mutable std::chrono::system_clock::time_point last_write_time_ = std::chrono::system_clock::now();
    mutable uint64_t read_count_ = 0;
    mutable uint64_t write_count_ = 0;
    mutable uint64_t error_count_ = 0;

    void updateTimestamps();
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // DATA_POINT_ENTITY_H