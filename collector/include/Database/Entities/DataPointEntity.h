#ifndef DATA_POINT_ENTITY_H
#define DATA_POINT_ENTITY_H

/**
 * @file DataPointEntity.h
 * @brief PulseOne DataPointEntity - DeviceSettingsEntity 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-08-05 (매크로 충돌 수정)
 * 
 * 🔥 매크로 충돌 완전 해결:
 * - data 매개변수 → json_data로 변경
 * - 매크로 보호 구문 추가
 * - 문법 오류 모두 수정
 */

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <map>
#include <sstream>
#include <iomanip>
#include <limits>

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

// 🔥 매크로 충돌 방지: data 매크로가 정의되어 있다면 제거
#ifdef data
#undef data
#endif

namespace PulseOne {

// Forward declarations (순환 참조 방지)
namespace Database {
namespace Repositories {
    class DataPointRepository;
}
}

namespace Database {
namespace Entities {

/**
 * @brief 데이터포인트 엔티티 클래스 (BaseEntity 상속, DeviceSettingsEntity 패턴)
 * 
 * 🎯 정규화된 DB 스키마 매핑:
 * CREATE TABLE data_points (
 *     id INTEGER PRIMARY KEY AUTOINCREMENT,
 *     device_id INTEGER NOT NULL,
 *     name VARCHAR(100) NOT NULL,
 *     description TEXT,
 *     address INTEGER NOT NULL,
 *     data_type VARCHAR(20) NOT NULL,
 *     access_mode VARCHAR(10) DEFAULT 'read',
 *     is_enabled BOOLEAN DEFAULT true,
 *     unit VARCHAR(20),
 *     scaling_factor REAL DEFAULT 1.0,
 *     scaling_offset REAL DEFAULT 0.0,
 *     min_value REAL,
 *     max_value REAL,
 *     log_enabled BOOLEAN DEFAULT true, 
 *     log_interval_ms INTEGER DEFAULT 0,
 *     log_deadband REAL DEFAULT 0.0,
 *     tags TEXT,
 *     metadata TEXT,
 *     created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 *     updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
 * );
 */
class DataPointEntity : public BaseEntity<DataPointEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자 (선언만 - CPP에서 구현)
    // =======================================================================
    
    DataPointEntity();
    explicit DataPointEntity(int point_id);
    // 🔥 수정: DataPoint 타입 확인 필요 (UnifiedCommonTypes에서 가져와야 함)
    // explicit DataPointEntity(const DataPoint& data_point);
    virtual ~DataPointEntity() = default;

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (CPP에서 구현)
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool updateToDatabase() override;

    // =======================================================================
    // JSON 직렬화/역직렬화 (인라인 구현) - 🔥 매크로 충돌 수정
    // =======================================================================
    
    json toJson() const override {
        json j;
        try {
            j["id"] = getId();                    // 🔥 수정: id_ → getId()
            j["device_id"] = device_id_;
            j["name"] = name_;
            j["description"] = description_;
            j["address"] = address_;
            j["data_type"] = data_type_;
            j["access_mode"] = access_mode_;
            j["is_enabled"] = is_enabled_;
            j["unit"] = unit_;
            j["scaling_factor"] = scaling_factor_;
            j["scaling_offset"] = scaling_offset_;
            j["min_value"] = min_value_;
            j["max_value"] = max_value_;
            j["log_enabled"] = log_enabled_;
            j["log_interval_ms"] = log_interval_ms_;
            j["log_deadband"] = log_deadband_;
            j["tags"] = tags_;
            j["metadata"] = metadata_;
            
            // 시간 정보
            auto created_time_t = std::chrono::system_clock::to_time_t(created_at_);
            auto updated_time_t = std::chrono::system_clock::to_time_t(updated_at_);
            std::ostringstream created_ss, updated_ss;
            created_ss << std::put_time(std::gmtime(&created_time_t), "%Y-%m-%d %H:%M:%S");
            updated_ss << std::put_time(std::gmtime(&updated_time_t), "%Y-%m-%d %H:%M:%S");
            j["created_at"] = created_ss.str();
            j["updated_at"] = updated_ss.str();
            
        } catch (const std::exception& e) {
            if (logger_) logger_->Error("DataPointEntity::toJson failed: " + std::string(e.what()));
        }
        return j;
    }
    
    // 🔥 핵심 수정: data → json_data로 매개변수명 변경
    bool fromJson(const json& json_data) override {
        try {
            if (json_data.contains("id")) setId(json_data["id"]);                              // 🔥 수정
            if (json_data.contains("device_id")) device_id_ = json_data["device_id"];          // 🔥 수정
            if (json_data.contains("name")) name_ = json_data["name"];                         // 🔥 수정
            if (json_data.contains("description")) description_ = json_data["description"];   // 🔥 수정
            if (json_data.contains("address")) address_ = json_data["address"];               // 🔥 수정
            if (json_data.contains("data_type")) data_type_ = json_data["data_type"];         // 🔥 수정
            if (json_data.contains("access_mode")) access_mode_ = json_data["access_mode"];   // 🔥 수정
            if (json_data.contains("is_enabled")) is_enabled_ = json_data["is_enabled"];      // 🔥 수정
            if (json_data.contains("unit")) unit_ = json_data["unit"];                        // 🔥 수정
            if (json_data.contains("scaling_factor")) scaling_factor_ = json_data["scaling_factor"]; // 🔥 수정
            if (json_data.contains("scaling_offset")) scaling_offset_ = json_data["scaling_offset"]; // 🔥 수정
            if (json_data.contains("min_value")) min_value_ = json_data["min_value"];          // 🔥 수정
            if (json_data.contains("max_value")) max_value_ = json_data["max_value"];          // 🔥 수정
            if (json_data.contains("log_enabled")) log_enabled_ = json_data["log_enabled"];   // 🔥 수정
            if (json_data.contains("log_interval_ms")) log_interval_ms_ = json_data["log_interval_ms"]; // 🔥 수정
            if (json_data.contains("log_deadband")) log_deadband_ = json_data["log_deadband"]; // 🔥 수정
            if (json_data.contains("tags")) tags_ = json_data["tags"];                        // 🔥 수정
            if (json_data.contains("metadata")) metadata_ = json_data["metadata"];            // 🔥 수정
            
            markModified();
            return true;
        } catch (const std::exception& e) {
            if (logger_) logger_->Error("DataPointEntity::fromJson failed: " + std::string(e.what()));
            return false;
        }
    }

    // =======================================================================
    // Getter 메서드들 (인라인 구현)
    // =======================================================================
    
    int getDeviceId() const { return device_id_; }
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    int getAddress() const { return address_; }
    const std::string& getDataType() const { return data_type_; }
    const std::string& getAccessMode() const { return access_mode_; }
    bool isEnabled() const { return is_enabled_; }
    const std::string& getUnit() const { return unit_; }
    double getScalingFactor() const { return scaling_factor_; }
    double getScalingOffset() const { return scaling_offset_; }
    double getMinValue() const { return min_value_; }
    double getMaxValue() const { return max_value_; }
    bool isLogEnabled() const { return log_enabled_; }
    int getLogInterval() const { return log_interval_ms_; }
    double getLogDeadband() const { return log_deadband_; }
    const std::vector<std::string>& getTags() const { return tags_; }
    const std::map<std::string, std::string>& getMetadata() const { return metadata_; }
    
    // 시간 정보
    const std::chrono::system_clock::time_point& getCreatedAt() const { return created_at_; }
    const std::chrono::system_clock::time_point& getUpdatedAt() const { return updated_at_; }
    std::chrono::system_clock::time_point getLastReadTime() const { return last_read_time_; }
    std::chrono::system_clock::time_point getLastWriteTime() const { return last_write_time_; }

    // =======================================================================
    // Setter 메서드들 (인라인 구현)
    // =======================================================================
    
    void setDeviceId(int device_id) { device_id_ = device_id; markModified(); }
    void setName(const std::string& name) { name_ = name; markModified(); }
    void setDescription(const std::string& description) { description_ = description; markModified(); }
    void setAddress(int address) { address_ = address; markModified(); }
    void setDataType(const std::string& data_type) { data_type_ = data_type; markModified(); }
    void setAccessMode(const std::string& access_mode) { access_mode_ = access_mode; markModified(); }
    void setEnabled(bool enabled) { is_enabled_ = enabled; markModified(); }
    void setUnit(const std::string& unit) { unit_ = unit; markModified(); }
    void setScalingFactor(double factor) { scaling_factor_ = factor; markModified(); }
    void setScalingOffset(double offset) { scaling_offset_ = offset; markModified(); }
    void setMinValue(double min_val) { min_value_ = min_val; markModified(); }
    void setMaxValue(double max_val) { max_value_ = max_val; markModified(); }
    void setLogEnabled(bool enabled) { log_enabled_ = enabled; markModified(); }
    void setLogInterval(int interval_ms) { log_interval_ms_ = interval_ms; markModified(); }
    void setLogDeadband(double deadband) { log_deadband_ = deadband; markModified(); }
    void setTags(const std::vector<std::string>& tags) { tags_ = tags; markModified(); }
    void setMetadata(const std::map<std::string, std::string>& metadata) { metadata_ = metadata; markModified(); }

    // =======================================================================
    // 유틸리티 메서드들 (인라인 구현)
    // =======================================================================
    
    /**
     * @brief 쓰기 가능 여부 확인
     */
    bool isWritable() const {
        return access_mode_ == "write" || access_mode_ == "read_write";
    }
    
    /**
     * @brief 읽기 가능 여부 확인
     */
    bool isReadable() const {
        return access_mode_ == "read" || access_mode_ == "read_write";
    }
    
    /**
     * @brief 유효성 검사
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
     * @brief 모든 설정을 기본값으로 리셋
     */
    void resetToDefault() {
        scaling_factor_ = 1.0;
        scaling_offset_ = 0.0;
        min_value_ = std::numeric_limits<double>::lowest();
        max_value_ = std::numeric_limits<double>::max();
        log_enabled_ = true;
        log_interval_ms_ = 0;
        log_deadband_ = 0.0;
        is_enabled_ = true;
        access_mode_ = "read";
        tags_.clear();
        metadata_.clear();
        markModified();
    }

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (인라인)
    // =======================================================================
    
    /**
     * @brief 문자열 표현 반환
     */
    std::string toString() const override {
        std::ostringstream oss;
        oss << "DataPoint{id=" << getId() 
            << ", device_id=" << device_id_
            << ", name='" << name_ << "'"
            << ", address=" << address_
            << ", data_type='" << data_type_ << "'"
            << ", enabled=" << (is_enabled_ ? "true" : "false") << "}";
        return oss.str();
    }
    
    /**
     * @brief 테이블명 반환
     */
    std::string getTableName() const override {
        return "data_points";
    }

    // =======================================================================
    // 추가 유틸리티 메서드들
    // =======================================================================
    
    /**
     * @brief Worker용 컨텍스트 정보 반환
     */
    json getWorkerContext() const;

    // =======================================================================
    // 통계 정보 접근 메서드들 (단순화)
    // =======================================================================
    
    /**
     * @brief 읽기 횟수 증가
     */
    void incrementReadCount() const {
        read_count_++;
        last_read_time_ = std::chrono::system_clock::now();
    }
    
    /**
     * @brief 쓰기 횟수 증가
     */
    void incrementWriteCount() const {
        write_count_++;
        last_write_time_ = std::chrono::system_clock::now();
    }
    
    /**
     * @brief 에러 횟수 증가
     */
    void incrementErrorCount() const {
        error_count_++;
    }
    
    /**
     * @brief 읽기 횟수 조회
     */
    uint64_t getReadCount() const { return read_count_; }
    
    /**
     * @brief 쓰기 횟수 조회
     */
    uint64_t getWriteCount() const { return write_count_; }
    
    /**
     * @brief 에러 횟수 조회
     */
    uint64_t getErrorCount() const { return error_count_; }

private:
    // =======================================================================
    // 멤버 변수들 (DB 스키마와 1:1 매핑)
    // =======================================================================
    
    // 기본 정보
    int device_id_;
    std::string name_;
    std::string description_;
    int address_;
    std::string data_type_;
    std::string access_mode_;     // "read", "write", "read_write"
    bool is_enabled_;
    
    // 엔지니어링 정보
    std::string unit_;
    double scaling_factor_;
    double scaling_offset_;
    double min_value_;
    double max_value_;
    
    // 로깅 설정
    bool log_enabled_;
    int log_interval_ms_;
    double log_deadband_;
    
    // 메타데이터
    std::vector<std::string> tags_;
    std::map<std::string, std::string> metadata_;
    
    // 시간 정보
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;
    
    // 통계 정보 (DB에는 없지만 런타임에서 사용 - 단순화)
    mutable std::chrono::system_clock::time_point last_read_time_;
    mutable std::chrono::system_clock::time_point last_write_time_;
    mutable uint64_t read_count_ = 0;
    mutable uint64_t write_count_ = 0;
    mutable uint64_t error_count_ = 0;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // DATA_POINT_ENTITY_H