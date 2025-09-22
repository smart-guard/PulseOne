#ifndef CURRENT_VALUE_ENTITY_H
#define CURRENT_VALUE_ENTITY_H

/**
 * @file CurrentValueEntity.h  
 * @brief PulseOne CurrentValueEntity - 새 JSON 스키마 완전 호환
 * @author PulseOne Development Team
 * @date 2025-08-07
 * 
 * 🎯 새 current_values 테이블 스키마 완전 반영:
 * - JSON 기반 current_value, raw_value 저장
 * - DataVariant 타입 지원 (bool, int16, uint16, int32, uint32, float, double, string)
 * - 통계 카운터들 (read_count, write_count, error_count)
 * - 다양한 타임스탬프 (value, quality, log, read, write)
 * - Struct DataPoint와 완전 호환
 */

#include "Database/Entities/BaseEntity.h"
#include "Common/Enums.h"
#include "Common/Utils.h"
#include "Common/BasicTypes.h"
#include <string>
#include <chrono>
#include <optional>
#include <sstream>
#include <iomanip>
#include <variant>

// 🔥 기존 패턴 100% 준수: BaseEntity.h에서 이미 json 정의됨
// using json = nlohmann::json; 는 BaseEntity.h에서 가져옴

namespace PulseOne {

// Forward declarations (순환 참조 방지)
namespace Database {
namespace Repositories {
    class CurrentValueRepository;
}
}

namespace Database {
namespace Entities {

/**
 * @brief 현재값 엔티티 클래스 (새 JSON 스키마 완전 호환)
 * 
 * 🎯 새 DB 스키마 매핑:
 * CREATE TABLE current_values (
 *     point_id INTEGER PRIMARY KEY,
 *     current_value TEXT,          -- JSON: {"value": 123.45}
 *     raw_value TEXT,              -- JSON: {"value": 12345}
 *     value_type VARCHAR(10),      -- bool, int16, uint16, int32, uint32, float, double, string
 *     quality_code INTEGER,
 *     quality VARCHAR(20),
 *     value_timestamp DATETIME,
 *     quality_timestamp DATETIME,
 *     last_log_time DATETIME,
 *     last_read_time DATETIME,
 *     last_write_time DATETIME,
 *     read_count INTEGER,
 *     write_count INTEGER,
 *     error_count INTEGER,
 *     updated_at DATETIME
 * );
 */
class CurrentValueEntity : public BaseEntity<CurrentValueEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자 (선언만 - CPP에서 구현)
    // =======================================================================
    
    CurrentValueEntity();
    explicit CurrentValueEntity(int point_id);
    CurrentValueEntity(int point_id, const PulseOne::BasicTypes::DataVariant& value);
    virtual ~CurrentValueEntity() = default;

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (CPP에서 구현)
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool deleteFromDatabase() override;
    bool updateToDatabase() override;

    // =======================================================================
    // JSON 직렬화/역직렬화 (인라인 구현)
    // =======================================================================
    
    json toJson() const override {
        json j;
        try {
            j["point_id"] = point_id_;
            j["current_value"] = current_value_;
            j["raw_value"] = raw_value_;
            j["value_type"] = value_type_;
            j["quality_code"] = static_cast<int>(quality_code_);
            j["quality"] = quality_;
            
            // 타임스탬프들 (ISO 8601 형식)
            j["value_timestamp"] = PulseOne::Utils::TimestampToDBString(value_timestamp_);
            j["quality_timestamp"] = PulseOne::Utils::TimestampToDBString(quality_timestamp_);
            j["last_log_time"] = PulseOne::Utils::TimestampToDBString(last_log_time_);
            j["last_read_time"] = PulseOne::Utils::TimestampToDBString(last_read_time_);
            j["last_write_time"] = PulseOne::Utils::TimestampToDBString(last_write_time_);
            j["updated_at"] = PulseOne::Utils::TimestampToDBString(updated_at_);
            
            // 통계 카운터들
            j["read_count"] = read_count_;
            j["write_count"] = write_count_;
            j["error_count"] = error_count_;
            
        } catch (const std::exception& e) {
            if (logger_) logger_->Error("CurrentValueEntity::toJson failed: " + std::string(e.what()));
        }
        return j;
    }
    
    bool fromJson(const json& data) override {
        try {
            if (data.contains("point_id")) point_id_ = data["point_id"];
            if (data.contains("current_value")) current_value_ = data["current_value"];
            if (data.contains("raw_value")) raw_value_ = data["raw_value"];
            if (data.contains("value_type")) value_type_ = data["value_type"];
            if (data.contains("quality_code")) quality_code_ = static_cast<PulseOne::Enums::DataQuality>(data["quality_code"]);
            if (data.contains("quality")) quality_ = data["quality"];
            
            // 타임스탬프들 파싱
            if (data.contains("value_timestamp")) {
                value_timestamp_ = PulseOne::Utils::ParseTimestampFromString(data["value_timestamp"]);
            }
            if (data.contains("quality_timestamp")) {
                quality_timestamp_ = PulseOne::Utils::ParseTimestampFromString(data["quality_timestamp"]);
            }
            if (data.contains("last_log_time")) {
                last_log_time_ = PulseOne::Utils::ParseTimestampFromString(data["last_log_time"]);
            }
            if (data.contains("last_read_time")) {
                last_read_time_ = PulseOne::Utils::ParseTimestampFromString(data["last_read_time"]);
            }
            if (data.contains("last_write_time")) {
                last_write_time_ = PulseOne::Utils::ParseTimestampFromString(data["last_write_time"]);
            }
            if (data.contains("updated_at")) {
                updated_at_ = PulseOne::Utils::ParseTimestampFromString(data["updated_at"]);
            }
            
            // 통계 카운터들
            if (data.contains("read_count")) read_count_ = data["read_count"];
            if (data.contains("write_count")) write_count_ = data["write_count"];
            if (data.contains("error_count")) error_count_ = data["error_count"];
            
            markModified();
            return true;
        } catch (const std::exception& e) {
            if (logger_) logger_->Error("CurrentValueEntity::fromJson failed: " + std::string(e.what()));
            return false;
        }
    }

    // =======================================================================
    // Getter 메서드들 (인라인 구현)
    // =======================================================================
    
    int getPointId() const { return point_id_; }
    const std::string& getCurrentValue() const { return current_value_; }
    const std::string& getRawValue() const { return raw_value_; }
    const std::string& getValueType() const { return value_type_; }
    PulseOne::Enums::DataQuality getQualityCode() const { return quality_code_; }
    const std::string& getQuality() const { return quality_; }
    
    // 타임스탬프 Getter들
    const std::chrono::system_clock::time_point& getValueTimestamp() const { return value_timestamp_; }
    const std::chrono::system_clock::time_point& getQualityTimestamp() const { return quality_timestamp_; }
    const std::chrono::system_clock::time_point& getLastLogTime() const { return last_log_time_; }
    const std::chrono::system_clock::time_point& getLastReadTime() const { return last_read_time_; }
    const std::chrono::system_clock::time_point& getLastWriteTime() const { return last_write_time_; }
    const std::chrono::system_clock::time_point& getUpdatedAt() const { return updated_at_; }
    
    // 통계 카운터 Getter들
    uint64_t getReadCount() const { return read_count_; }
    uint64_t getWriteCount() const { return write_count_; }
    uint64_t getErrorCount() const { return error_count_; }

    // =======================================================================
    // Setter 메서드들 (인라인 구현)
    // =======================================================================
    
    void setPointId(int point_id) { point_id_ = point_id; markModified(); }
    
    void setCurrentValue(const std::string& current_value) { 
        current_value_ = current_value; 
        updateValueTimestamp(); 
        markModified(); 
    }
    
    void setRawValue(const std::string& raw_value) { raw_value_ = raw_value; markModified(); }
    void setValueType(const std::string& value_type) { value_type_ = value_type; markModified(); }
    
    void setQuality(PulseOne::Enums::DataQuality quality_code) {
        quality_code_ = quality_code;
        quality_ = PulseOne::Utils::DataQualityToString(quality_code, true); // lowercase
        markModified();
    }
    
    void setQuality(const std::string& quality_str) {
        quality_ = quality_str;
        quality_code_ = PulseOne::Utils::StringToDataQuality(quality_str);
        markModified();
    }
    
    // 타임스탬프 Setter들
    void setValueTimestamp(const std::chrono::system_clock::time_point& timestamp) { 
        value_timestamp_ = timestamp; markModified(); 
    }
    void setQualityTimestamp(const std::chrono::system_clock::time_point& timestamp) { 
        quality_timestamp_ = timestamp; markModified(); 
    }
    void setLastLogTime(const std::chrono::system_clock::time_point& timestamp) { 
        last_log_time_ = timestamp; markModified(); 
    }
    void setLastReadTime(const std::chrono::system_clock::time_point& timestamp) { 
        last_read_time_ = timestamp; markModified(); 
    }
    void setLastWriteTime(const std::chrono::system_clock::time_point& timestamp) { 
        last_write_time_ = timestamp; markModified(); 
    }
    void setUpdatedAt(const std::chrono::system_clock::time_point& timestamp) { 
        updated_at_ = timestamp; markModified(); 
    }
    
    // 통계 카운터 Setter들
    void setReadCount(uint64_t count) { read_count_ = count; markModified(); }
    void setWriteCount(uint64_t count) { write_count_ = count; markModified(); }
    void setErrorCount(uint64_t count) { error_count_ = count; markModified(); }

    // =======================================================================
    // 유틸리티 메서드들 (새 스키마 최적화)
    // =======================================================================
    
    /**
     * @brief DataVariant 값을 JSON 문자열로 설정
     */
    void setCurrentValueFromVariant(const PulseOne::BasicTypes::DataVariant& value) {
        json j;
        std::string type_name = "double";  // 기본값
        
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            j["value"] = arg;
            
            if constexpr (std::is_same_v<T, bool>) {
                type_name = "bool";
            } else if constexpr (std::is_same_v<T, int16_t>) {
                type_name = "int16";
            } else if constexpr (std::is_same_v<T, uint16_t>) {
                type_name = "uint16";
            } else if constexpr (std::is_same_v<T, int32_t>) {
                type_name = "int32";
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                type_name = "uint32";
            } else if constexpr (std::is_same_v<T, float>) {
                type_name = "float";
            } else if constexpr (std::is_same_v<T, double>) {
                type_name = "double";
            } else if constexpr (std::is_same_v<T, std::string>) {
                type_name = "string";
            }
        }, value);
        
        current_value_ = j.dump();
        value_type_ = type_name;
        updateValueTimestamp();
        markModified();
    }
    
    /**
     * @brief JSON 문자열에서 DataVariant로 값 추출
     */
    PulseOne::BasicTypes::DataVariant getCurrentValueAsVariant() const {
        try {
            json j = json::parse(current_value_);
            if (!j.contains("value")) {
                return 0.0;  // 기본값
            }
            
            if (value_type_ == "bool") {
                return j["value"].get<bool>();
            } else if (value_type_ == "int16") {
                return j["value"].get<int16_t>();
            } else if (value_type_ == "uint16") {
                return j["value"].get<uint16_t>();
            } else if (value_type_ == "int32") {
                return j["value"].get<int32_t>();
            } else if (value_type_ == "uint32") {
                return j["value"].get<uint32_t>();
            } else if (value_type_ == "float") {
                return j["value"].get<float>();
            } else if (value_type_ == "double") {
                return j["value"].get<double>();
            } else if (value_type_ == "string") {
                return j["value"].get<std::string>();
            }
            
            return j["value"].get<double>();  // 기본값
            
        } catch (const std::exception& e) {
            if (logger_) logger_->Error("CurrentValueEntity::getCurrentValueAsVariant failed: " + std::string(e.what()));
            return 0.0;
        }
    }
    
    /**
     * @brief 현재 시간으로 value_timestamp 업데이트
     */
    void updateValueTimestamp() {
        value_timestamp_ = std::chrono::system_clock::now();
        updated_at_ = value_timestamp_;
        markModified();
    }
    
    /**
     * @brief 읽기 카운터 증가
     */
    void incrementReadCount() {
        read_count_++;
        last_read_time_ = std::chrono::system_clock::now();
        updated_at_ = last_read_time_;
        markModified();
    }
    
    /**
     * @brief 쓰기 카운터 증가
     */
    void incrementWriteCount() {
        write_count_++;
        last_write_time_ = std::chrono::system_clock::now();
        updated_at_ = last_write_time_;
        markModified();
    }
    
    /**
     * @brief 에러 카운터 증가
     */
    void incrementErrorCount() {
        error_count_++;
        updated_at_ = std::chrono::system_clock::now();
        markModified();
    }
    
    /**
     * @brief 로그 시간 업데이트
     */
    void updateLogTime() {
        last_log_time_ = std::chrono::system_clock::now();
        updated_at_ = last_log_time_;
        markModified();
    }
    
    /**
     * @brief 품질 상태를 문자열로 변환
     */
    std::string getQualityString() const { return quality_; }
    
    /**
     * @brief 유효성 검사
     */
    bool isValid() const override {
        return point_id_ > 0 && !value_type_.empty();
    }

    // =======================================================================
    // BaseEntity 순수 가상 함수 구현 (인라인)
    // =======================================================================
    
    /**
     * @brief 문자열 표현 반환
     */
    std::string toString() const override {
        std::ostringstream oss;
        oss << "CurrentValue{point_id=" << point_id_ 
            << ", value=" << current_value_
            << ", type=" << value_type_
            << ", quality=" << quality_
            << ", timestamp=" << PulseOne::Utils::TimestampToDBString(value_timestamp_) 
            << ", reads=" << read_count_
            << ", writes=" << write_count_
            << ", errors=" << error_count_ << "}";
        return oss.str();
    }
    
    /**
     * @brief 테이블명 반환
     */
    std::string getTableName() const override {
        return "current_values";
    }

    // =======================================================================
    // 추가 유틸리티 메서드들
    // =======================================================================
    
    /**
     * @brief 데이터가 오래되었는지 확인
     */
    bool isStale(int max_age_seconds = 3600) const {
        auto now = std::chrono::system_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - value_timestamp_);
        return age.count() > max_age_seconds;
    }
    
    /**
     * @brief 품질이 양호한지 확인
     */
    bool hasGoodQuality() const {
        return quality_code_ == PulseOne::Enums::DataQuality::GOOD;
    }
    
    /**
     * @brief 통계 정보를 JSON으로 반환
     */
    json getStatistics() const {
        json stats;
        stats["read_count"] = read_count_;
        stats["write_count"] = write_count_;
        stats["error_count"] = error_count_;
        stats["last_read_time"] = PulseOne::Utils::TimestampToDBString(last_read_time_);
        stats["last_write_time"] = PulseOne::Utils::TimestampToDBString(last_write_time_);
        
        // 에러율 계산
        uint64_t total_operations = read_count_ + write_count_;
        if (total_operations > 0) {
            stats["error_rate"] = static_cast<double>(error_count_) / total_operations * 100.0;
        } else {
            stats["error_rate"] = 0.0;
        }
        
        return stats;
    }

    // =======================================================================
    // CPP에서 구현될 메서드들 (선언만)
    // =======================================================================
    void updateValue(const PulseOne::BasicTypes::DataVariant& new_value, PulseOne::Enums::DataQuality quality);
    void updateValueWithRaw(const PulseOne::BasicTypes::DataVariant& current_val,
                           const PulseOne::BasicTypes::DataVariant& raw_val, 
                           PulseOne::Enums::DataQuality quality);
    bool shouldLog(int log_interval_ms, double deadband) const;
    void onRead();
    void onWrite();
    void onError();
    double getNumericValue() const;
    std::string getStringValue() const;
    void resetStatistics();
    json getFullStatus() const;

    // 🔥 타입 불일치 수정:
    double getValue() const;           // 기존: getRawValue()와 함께 제공
    void setValue(double value);       // 기존: setRawValue()와 함께 제공
    void setTimestamp(const std::chrono::system_clock::time_point& timestamp);
    std::chrono::system_clock::time_point getTimestamp() const;    

private:
    // =======================================================================
    // 멤버 변수들 (새 DB 스키마와 1:1 매핑)
    // =======================================================================
    
    int point_id_ = 0;                                                      // PRIMARY KEY (DataPoint ID)
    
    // 🔥 JSON 기반 값 저장
    std::string current_value_ = R"({"value": 0.0})";                      // JSON: 현재값 (스케일링 적용됨)
    std::string raw_value_ = R"({"value": 0.0})";                         // JSON: 원시값 (스케일링 전)
    std::string value_type_ = "double";                                     // 값 타입 (bool, int16, uint16, etc.)
    
    // 🔥 품질 정보
    PulseOne::Enums::DataQuality quality_code_ = PulseOne::Enums::DataQuality::GOOD;  // 품질 코드
    std::string quality_ = "good";                                         // 품질 텍스트
    
    // 🔥 다양한 타임스탬프들
    std::chrono::system_clock::time_point value_timestamp_ = std::chrono::system_clock::now();    // 값 변경 시간
    std::chrono::system_clock::time_point quality_timestamp_ = std::chrono::system_clock::now();  // 품질 변경 시간
    std::chrono::system_clock::time_point last_log_time_ = std::chrono::system_clock::now();      // 마지막 로깅 시간
    std::chrono::system_clock::time_point last_read_time_ = std::chrono::system_clock::now();     // 마지막 읽기 시간
    std::chrono::system_clock::time_point last_write_time_ = std::chrono::system_clock::now();    // 마지막 쓰기 시간
    std::chrono::system_clock::time_point updated_at_ = std::chrono::system_clock::now();         // 업데이트 시간
    
    // 🔥 통계 카운터들
    uint64_t read_count_ = 0;                                              // 읽기 횟수
    uint64_t write_count_ = 0;                                             // 쓰기 횟수
    uint64_t error_count_ = 0;                                             // 에러 횟수
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // CURRENT_VALUE_ENTITY_H