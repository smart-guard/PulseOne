#ifndef DATA_POINT_ENTITY_H
#define DATA_POINT_ENTITY_H

/**
 * @file DataPointEntity.h
 * @brief PulseOne DataPointEntity - 현재 스키마 완전 호환 버전
 * @author PulseOne Development Team
 * @date 2025-08-26
 * 
 * 🎯 현재 data_points 테이블 스키마 완전 반영:
 * - 데이터 품질 관련: quality_check_enabled, range_check_enabled, rate_of_change_limit
 * - 알람 관련: alarm_enabled, alarm_priority  
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


// 매크로 충돌 방지
#ifdef data
#undef data
#endif

namespace PulseOne {
namespace Database {
namespace Entities {

/**
 * @brief 데이터포인트 엔티티 클래스 (현재 스키마 완전 호환)
 * 
 * 🎯 현재 DB 스키마 매핑:
 * CREATE TABLE data_points (
 *     id INTEGER PRIMARY KEY AUTOINCREMENT,
 *     device_id INTEGER NOT NULL,
 *     name VARCHAR(100) NOT NULL,
 *     description TEXT,
 *     address INTEGER NOT NULL,
 *     address_string VARCHAR(255),
 *     data_type VARCHAR(20) NOT NULL DEFAULT 'UNKNOWN',
 *     access_mode VARCHAR(10) DEFAULT 'read',
 *     is_enabled INTEGER DEFAULT 1,
 *     is_writable INTEGER DEFAULT 0,
 *     unit VARCHAR(50),
 *     scaling_factor REAL DEFAULT 1.0,
 *     scaling_offset REAL DEFAULT 0.0,
 *     min_value REAL DEFAULT 0.0,
 *     max_value REAL DEFAULT 0.0,
 *     log_enabled INTEGER DEFAULT 1,
 *     log_interval_ms INTEGER DEFAULT 0,
 *     log_deadband REAL DEFAULT 0.0,
 *     polling_interval_ms INTEGER DEFAULT 0,
 *     quality_check_enabled INTEGER DEFAULT 1,        -- 추가됨
 *     range_check_enabled INTEGER DEFAULT 1,          -- 추가됨
 *     rate_of_change_limit REAL DEFAULT 0.0,          -- 추가됨
 *     alarm_enabled INTEGER DEFAULT 0,                -- 추가됨
 *     alarm_priority VARCHAR(20) DEFAULT 'medium',    -- 추가됨
 *     group_name VARCHAR(50),
 *     tags TEXT,
 *     metadata TEXT,
 *     protocol_params TEXT,
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
    // BaseEntity 순수 가상 함수 구현
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool updateToDatabase() override;

    // =======================================================================
    // JSON 직렬화/역직렬화 (모든 필드 포함)
    // =======================================================================
    
    json toJson() const override {
        json j;
        try {
            j["id"] = getId();
            j["device_id"] = device_id_;
            j["name"] = name_;
            j["description"] = description_;
            j["address"] = address_;
            j["address_string"] = address_string_;
            j["mapping_key"] = mapping_key_;
            j["data_type"] = data_type_;
            j["access_mode"] = access_mode_;
            j["is_enabled"] = is_enabled_;
            j["is_writable"] = is_writable_;
            j["unit"] = unit_;
            j["scaling_factor"] = scaling_factor_;
            j["scaling_offset"] = scaling_offset_;
            j["min_value"] = min_value_;
            j["max_value"] = max_value_;
            j["log_enabled"] = is_log_enabled_;
            j["log_interval_ms"] = log_interval_ms_;
            j["log_deadband"] = log_deadband_;
            j["polling_interval_ms"] = polling_interval_ms_;
            
            // 품질 관리 필드들
            j["quality_check_enabled"] = is_quality_check_enabled_;
            j["range_check_enabled"] = is_range_check_enabled_;
            j["rate_of_change_limit"] = rate_of_change_limit_;
            
            // 알람 관련 필드들
            j["alarm_enabled"] = is_alarm_enabled_;
            j["alarm_priority"] = alarm_priority_;
            
            j["group_name"] = group_name_;
            j["tags"] = tags_;
            j["metadata"] = metadata_;
            j["protocol_params"] = protocol_params_;
            
            // 시간 정보
            j["created_at"] = timestampToString(created_at_);
            j["updated_at"] = timestampToString(updated_at_);
            
            // 통계 정보 (런타임 전용)
            j["read_count"] = read_count_;
            j["write_count"] = write_count_;
            j["error_count"] = error_count_;
            
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
            if (json_data.contains("address_string")) address_string_ = json_data["address_string"];
            if (json_data.contains("mapping_key")) mapping_key_ = json_data["mapping_key"];
            if (json_data.contains("data_type")) data_type_ = json_data["data_type"];
            if (json_data.contains("access_mode")) access_mode_ = json_data["access_mode"];
            if (json_data.contains("is_enabled")) is_enabled_ = json_data["is_enabled"];
            if (json_data.contains("is_writable")) is_writable_ = json_data["is_writable"];
            if (json_data.contains("unit")) unit_ = json_data["unit"];
            if (json_data.contains("scaling_factor")) scaling_factor_ = json_data["scaling_factor"];
            if (json_data.contains("scaling_offset")) scaling_offset_ = json_data["scaling_offset"];
            if (json_data.contains("min_value")) min_value_ = json_data["min_value"];
            if (json_data.contains("max_value")) max_value_ = json_data["max_value"];
            if (json_data.contains("log_enabled")) is_log_enabled_ = json_data["log_enabled"];
            if (json_data.contains("log_interval_ms")) log_interval_ms_ = json_data["log_interval_ms"];
            if (json_data.contains("log_deadband")) log_deadband_ = json_data["log_deadband"];
            if (json_data.contains("polling_interval_ms")) polling_interval_ms_ = json_data["polling_interval_ms"];
            
            // 품질 관리 필드들
            if (json_data.contains("quality_check_enabled")) is_quality_check_enabled_ = json_data["quality_check_enabled"];
            if (json_data.contains("range_check_enabled")) is_range_check_enabled_ = json_data["range_check_enabled"];
            if (json_data.contains("rate_of_change_limit")) rate_of_change_limit_ = json_data["rate_of_change_limit"];
            
            // 알람 관련 필드들
            if (json_data.contains("alarm_enabled")) is_alarm_enabled_ = json_data["alarm_enabled"];
            if (json_data.contains("alarm_priority")) alarm_priority_ = json_data["alarm_priority"];
            
            if (json_data.contains("group_name")) group_name_ = json_data["group_name"];
            if (json_data.contains("tags")) tags_ = json_data["tags"];
            if (json_data.contains("metadata")) metadata_ = json_data["metadata"];
            if (json_data.contains("protocol_params")) protocol_params_ = json_data["protocol_params"];
            
            markModified();
            return true;
        } catch (const std::exception& e) {
            if (logger_) logger_->Error("DataPointEntity::fromJson failed: " + std::string(e.what()));
            return false;
        }
    }

    // =======================================================================
    // Getter 메서드들 (모든 필드 포함)
    // =======================================================================
    
    int getDeviceId() const { return device_id_; }
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    int getAddress() const { return address_; }
    const std::string& getAddressString() const { return address_string_; }
    const std::string& getMappingKey() const { return mapping_key_; }
    const std::string& getDataType() const { return data_type_; }
    const std::string& getAccessMode() const { return access_mode_; }
    bool isEnabled() const { return is_enabled_; }
    bool isWritable() const { return is_writable_; }
    const std::string& getUnit() const { return unit_; }
    double getScalingFactor() const { return scaling_factor_; }
    double getScalingOffset() const { return scaling_offset_; }
    double getMinValue() const { return min_value_; }
    double getMaxValue() const { return max_value_; }
    bool isLogEnabled() const { return is_log_enabled_; }
    uint32_t getLogInterval() const { return log_interval_ms_; }
    double getLogDeadband() const { return log_deadband_; }
    uint32_t getPollingInterval() const { return polling_interval_ms_; }
    
    // 품질 관리 관련
    bool isQualityCheckEnabled() const { return is_quality_check_enabled_; }
    bool isRangeCheckEnabled() const { return is_range_check_enabled_; }
    double getRateOfChangeLimit() const { return rate_of_change_limit_; }
    
    // 알람 관련
    bool isAlarmEnabled() const { return is_alarm_enabled_; }
    const std::string& getAlarmPriority() const { return alarm_priority_; }
    
    const std::string& getGroup() const { return group_name_; }
    const std::vector<std::string>& getTags() const { return tags_; }
    const std::map<std::string, std::string>& getMetadata() const { return metadata_; }
    const std::map<std::string, std::string>& getProtocolParams() const { return protocol_params_; }
    
    // 시간 정보
    const std::chrono::system_clock::time_point& getCreatedAt() const { return created_at_; }
    const std::chrono::system_clock::time_point& getUpdatedAt() const { return updated_at_; }
    
    // 통계 정보 (런타임 전용)
    std::chrono::system_clock::time_point getLastReadTime() const { return last_read_time_; }
    std::chrono::system_clock::time_point getLastWriteTime() const { return last_write_time_; }
    uint64_t getReadCount() const { return read_count_; }
    uint64_t getWriteCount() const { return write_count_; }
    uint64_t getErrorCount() const { return error_count_; }

    // =======================================================================
    // Setter 메서드들 (모든 필드 포함)
    // =======================================================================
    
    void setDeviceId(int device_id) { device_id_ = device_id; markModified(); }
    void setName(const std::string& name) { name_ = name; markModified(); }
    void setDescription(const std::string& description) { description_ = description; markModified(); }
    void setAddress(int address) { address_ = address; markModified(); }
    void setAddressString(const std::string& address_string) { address_string_ = address_string; markModified(); }
    void setMappingKey(const std::string& mapping_key) { mapping_key_ = mapping_key; markModified(); }
    void setAccessMode(const std::string& access_mode) { access_mode_ = access_mode; markModified(); }
    void setEnabled(bool enabled) { is_enabled_ = enabled; markModified(); }
    void setWritable(bool writable) { is_writable_ = writable; markModified(); }
    void setUnit(const std::string& unit) { unit_ = unit; markModified(); }
    void setScalingFactor(double factor) { scaling_factor_ = factor; markModified(); }
    void setScalingOffset(double offset) { scaling_offset_ = offset; markModified(); }
    void setMinValue(double min_val) { min_value_ = min_val; markModified(); }
    void setMaxValue(double max_val) { max_value_ = max_val; markModified(); }
    void setLogEnabled(bool enabled) { is_log_enabled_ = enabled; markModified(); }
    void setLogInterval(uint32_t interval_ms) { log_interval_ms_ = interval_ms; markModified(); }
    void setLogDeadband(double deadband) { log_deadband_ = deadband; markModified(); }
    void setPollingInterval(uint32_t interval_ms) { polling_interval_ms_ = interval_ms; markModified(); }
    
    // 품질 관리 관련
    void setQualityCheckEnabled(bool enabled) { is_quality_check_enabled_ = enabled; markModified(); }
    void setRangeCheckEnabled(bool enabled) { is_range_check_enabled_ = enabled; markModified(); }
    void setRateOfChangeLimit(double limit) { rate_of_change_limit_ = limit; markModified(); }
    
    // 알람 관련
    void setAlarmEnabled(bool enabled) { is_alarm_enabled_ = enabled; markModified(); }
    void setAlarmPriority(const std::string& priority) { alarm_priority_ = priority; markModified(); }
    
    void setGroup(const std::string& group_name) { group_name_ = group_name; markModified(); }
    void setTags(const std::vector<std::string>& tags) { tags_ = tags; markModified(); }
    void setMetadata(const std::map<std::string, std::string>& metadata) { metadata_ = metadata; markModified(); }
    void setProtocolParams(const std::map<std::string, std::string>& protocol_params) { protocol_params_ = protocol_params; markModified(); }
    
    void setDataType(const std::string& data_type) {
        data_type_ = PulseOne::Utils::NormalizeDataType(data_type);
        markModified();
    }
    
    void setCreatedAt(const std::chrono::system_clock::time_point& timestamp) {
        created_at_ = timestamp;
        markModified();
    }

    void setUpdatedAt(const std::chrono::system_clock::time_point& timestamp) {
        updated_at_ = timestamp;
        markModified();
    }
    
    // 문자열 오버로드 버전 (Repository 호환용)
    void setCreatedAt(const std::string& timestamp_str) { 
        created_at_ = PulseOne::Utils::ParseTimestampFromString(timestamp_str); 
        markModified(); 
    }

    void setUpdatedAt(const std::string& timestamp_str) { 
        updated_at_ = PulseOne::Utils::ParseTimestampFromString(timestamp_str); 
        markModified(); 
    }

    // =======================================================================
    // 유틸리티 메서드들
    // =======================================================================
    
    bool canWrite() const {
        return is_writable_ && is_enabled_;
    }
    
    bool canRead() const {
        return (access_mode_ == "read" || access_mode_ == "read_write") && is_enabled_;
    }
    
    bool isValid() const override {
        return device_id_ > 0 && 
               !name_.empty() && 
               !data_type_.empty() && 
               scaling_factor_ != 0.0 &&
               min_value_ <= max_value_ &&
               (access_mode_ == "read" || access_mode_ == "write" || access_mode_ == "read_write") &&
               (alarm_priority_ == "low" || alarm_priority_ == "medium" || alarm_priority_ == "high" || alarm_priority_ == "critical");
    }

    void resetToDefault() {
        address_string_.clear();
        is_writable_ = false;
        scaling_factor_ = 1.0;
        scaling_offset_ = 0.0;
        min_value_ = std::numeric_limits<double>::lowest();
        max_value_ = std::numeric_limits<double>::max();
        is_log_enabled_ = true;
        log_interval_ms_ = 0;
        log_deadband_ = 0.0;
        polling_interval_ms_ = 1000;
        
        // 품질 관리 기본값
        is_quality_check_enabled_ = true;
        is_range_check_enabled_ = true;
        rate_of_change_limit_ = 0.0;
        
        // 알람 기본값
        is_alarm_enabled_ = false;
        alarm_priority_ = "medium";
        
        group_name_.clear();
        is_enabled_ = true;
        access_mode_ = "read";
        tags_.clear();
        metadata_.clear();
        protocol_params_.clear();
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
        
        if (!mapping_key_.empty()) {
            oss << ", mapping_key='" << mapping_key_ << "'";
        }
        
        oss << ", data_type='" << data_type_ << "'"
            << ", enabled=" << (is_enabled_ ? "true" : "false")
            << ", writable=" << (is_writable_ ? "true" : "false");
            
        if (!group_name_.empty()) {
            oss << ", group='" << group_name_ << "'";
        }
        
        if (is_alarm_enabled_) {
            oss << ", alarm_priority='" << alarm_priority_ << "'";
        }
        
        oss << "}";
        return oss.str();
    }
    
    std::string getTableName() const override {
        return "data_points";
    }

    // =======================================================================
    // 프로토콜 관련 유틸리티 메서드들
    // =======================================================================
    
    std::string getProtocol() const {
        auto it = protocol_params_.find("protocol");
        return (it != protocol_params_.end()) ? it->second : "UNKNOWN";
    }
    
    void setProtocolParam(const std::string& key, const std::string& value) {
        protocol_params_[key] = value;
        markModified();
    }
    
    std::string getProtocolParam(const std::string& key, const std::string& default_value = "") const {
        auto it = protocol_params_.find(key);
        return (it != protocol_params_.end()) ? it->second : default_value;
    }
    
    bool isModbusPoint() const {
        return getProtocol() == "MODBUS_TCP" || getProtocol() == "MODBUS_RTU";
    }
    
    bool isMqttPoint() const {
        return getProtocol() == "MQTT";
    }
    
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
        context["mapping_key"] = mapping_key_;
        context["data_type"] = data_type_;
        context["access_mode"] = access_mode_;
        context["is_enabled"] = is_enabled_;
        context["is_writable"] = is_writable_;
        context["polling_interval_ms"] = polling_interval_ms_;
        context["protocol_params"] = protocol_params_;
        context["scaling_factor"] = scaling_factor_;
        context["scaling_offset"] = scaling_offset_;
        context["unit"] = unit_;
        
        // 품질 관리 정보 추가
        context["quality_check_enabled"] = is_quality_check_enabled_;
        context["range_check_enabled"] = is_range_check_enabled_;
        context["rate_of_change_limit"] = rate_of_change_limit_;
        
        // 알람 정보 추가
        context["alarm_enabled"] = is_alarm_enabled_;
        context["alarm_priority"] = alarm_priority_;
        
        return context;
    }

    // =======================================================================
    // 통계 정보 접근 메서드들 (런타임 전용)
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

    // =======================================================================
    // 품질 관리 메서드들 (CPP에서 구현)
    // =======================================================================
    
    bool validateValue(double value) const;
    bool isRateOfChangeViolation(double previous_value, double current_value, 
                                double time_diff_seconds) const;

    // =======================================================================
    // 기존 유틸리티 메서드들 (선언만, 구현은 CPP에서)
    // =======================================================================
    
    bool validateProtocolSpecific() const;
    double applyScaling(double raw_value) const;
    double removeScaling(double scaled_value) const;
    bool isWithinDeadband(double previous_value, double new_value) const;
    bool isValueInRange(double value) const;
    void adjustPollingInterval(bool connection_healthy);
    void addTag(const std::string& tag);
    void removeTag(const std::string& tag);
    bool hasTag(const std::string& tag) const;
    void setMetadata(const std::string& key, const std::string& value);
    std::string getMetadata(const std::string& key, const std::string& default_value = "") const;
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

    void updateTimestamps();

    // =======================================================================
    // 멤버 변수들 (현재 DB 스키마와 1:1 매핑)
    // =======================================================================
    
    // 기본 정보
    int device_id_ = 0;
    std::string name_;
    std::string description_;
    int address_ = 0;
    std::string address_string_;
    std::string mapping_key_;
    std::string data_type_ = "UNKNOWN";
    std::string access_mode_ = "read";
    bool is_enabled_ = true;
    bool is_writable_ = false;
    
    // 엔지니어링 정보
    std::string unit_;
    double scaling_factor_ = 1.0;
    double scaling_offset_ = 0.0;
    double min_value_ = std::numeric_limits<double>::lowest();
    double max_value_ = std::numeric_limits<double>::max();
    
    // 로깅 및 수집 설정
    bool is_log_enabled_ = true;
    uint32_t log_interval_ms_ = 0;
    double log_deadband_ = 0.0;
    uint32_t polling_interval_ms_ = 1000;
    
    // 품질 관리 설정
    bool is_quality_check_enabled_ = true;
    bool is_range_check_enabled_ = true;
    double rate_of_change_limit_ = 0.0;
    
    // 알람 관련 설정
    bool is_alarm_enabled_ = false;
    std::string alarm_priority_ = "medium";
    
    // 메타데이터
    std::string group_name_;
    std::vector<std::string> tags_;
    std::map<std::string, std::string> metadata_;
    std::map<std::string, std::string> protocol_params_;
    
    // 시간 정보
    std::chrono::system_clock::time_point created_at_ = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point updated_at_ = std::chrono::system_clock::now();
    
    // 통계 정보 (런타임 전용 - DB에 저장되지 않음)
    mutable std::chrono::system_clock::time_point last_read_time_ = std::chrono::system_clock::now();
    mutable std::chrono::system_clock::time_point last_write_time_ = std::chrono::system_clock::now();
    mutable uint64_t read_count_ = 0;
    mutable uint64_t write_count_ = 0;
    mutable uint64_t error_count_ = 0;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // DATA_POINT_ENTITY_H