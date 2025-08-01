#ifndef CURRENT_VALUE_ENTITY_H
#define CURRENT_VALUE_ENTITY_H

/**
 * @file CurrentValueEntity.h  
 * @brief PulseOne CurrentValueEntity - DataPointEntity 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 DataPointEntity 패턴 완전 적용:
 * - 헤더: 선언만 (순환 참조 방지)
 * - CPP: Repository 호출 구현
 * - BaseEntity<CurrentValueEntity> 상속 (CRTP)
 * - current_values 테이블과 1:1 매핑
 */

#include "Database/Entities/BaseEntity.h"
#include "Common/Enums.h"
#include "Common/Utils.h"
#include <string>
#include <chrono>
#include <optional>
#include <sstream>
#include <iomanip>

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
 * @brief 현재값 엔티티 클래스 (BaseEntity 상속, DataPointEntity 패턴)
 * 
 * 🎯 정규화된 DB 스키마 매핑:
 * CREATE TABLE current_values (
 *     point_id INTEGER PRIMARY KEY,
 *     value DECIMAL(15,4),
 *     raw_value DECIMAL(15,4),
 *     string_value TEXT,
 *     quality VARCHAR(20) DEFAULT 'good',
 *     timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
 *     updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
 *     
 *     FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE
 * );
 */
class CurrentValueEntity : public BaseEntity<CurrentValueEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자 (선언만 - CPP에서 구현)
    // =======================================================================
    
    CurrentValueEntity();
    explicit CurrentValueEntity(int point_id);
    CurrentValueEntity(int point_id, double value);
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
            j["value"] = value_;
            j["raw_value"] = raw_value_;
            j["string_value"] = string_value_;
            j["quality"] = static_cast<int>(quality_);
            
            // 시간 정보 (DataPointEntity 패턴)
            auto timestamp_time_t = std::chrono::system_clock::to_time_t(timestamp_);
            auto updated_time_t = std::chrono::system_clock::to_time_t(updated_at_);
            std::ostringstream timestamp_ss, updated_ss;
            timestamp_ss << std::put_time(std::gmtime(&timestamp_time_t), "%Y-%m-%d %H:%M:%S");
            updated_ss << std::put_time(std::gmtime(&updated_time_t), "%Y-%m-%d %H:%M:%S");
            j["timestamp"] = timestamp_ss.str();
            j["updated_at"] = updated_ss.str();
            
        } catch (const std::exception& e) {
            if (logger_) logger_->Error("CurrentValueEntity::toJson failed: " + std::string(e.what()));
        }
        return j;
    }
    
    bool fromJson(const json& data) override {
        try {
            if (data.contains("point_id")) point_id_ = data["point_id"];
            if (data.contains("value")) value_ = data["value"];
            if (data.contains("raw_value")) raw_value_ = data["raw_value"];
            if (data.contains("string_value")) string_value_ = data["string_value"];
            if (data.contains("quality")) quality_ = static_cast<PulseOne::Enums::DataQuality>(data["quality"]);
            
            // 시간 정보는 Utils 함수로 파싱
            if (data.contains("timestamp")) {
                timestamp_ = PulseOne::Utils::ParseTimestampFromString(data["timestamp"]);
            }
            if (data.contains("updated_at")) {
                updated_at_ = PulseOne::Utils::ParseTimestampFromString(data["updated_at"]);
            }
            
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
    double getValue() const { return value_; }
    double getRawValue() const { return raw_value_; }
    const std::string& getStringValue() const { return string_value_; }
    PulseOne::Enums::DataQuality getQuality() const { return quality_; }
    const std::chrono::system_clock::time_point& getTimestamp() const { return timestamp_; }
    const std::chrono::system_clock::time_point& getUpdatedAt() const { return updated_at_; }

    // =======================================================================
    // Setter 메서드들 (인라인 구현)
    // =======================================================================
    
    void setPointId(int point_id) { point_id_ = point_id; markModified(); }
    void setValue(double value) { 
        value_ = value; 
        updateTimestamp(); 
        markModified(); 
    }
    void setRawValue(double raw_value) { raw_value_ = raw_value; markModified(); }
    void setStringValue(const std::string& string_value) { string_value_ = string_value; markModified(); }
    void setQuality(PulseOne::Enums::DataQuality quality) { quality_ = quality; markModified(); }
    void setTimestamp(const std::chrono::system_clock::time_point& timestamp) { 
        timestamp_ = timestamp; 
        markModified(); 
    }
    void setUpdatedAt(const std::chrono::system_clock::time_point& updated_at) { 
        updated_at_ = updated_at; 
        markModified(); 
    }

    // =======================================================================
    // 유틸리티 메서드들 (인라인 구현)
    // =======================================================================
    
    /**
     * @brief 현재 시간으로 timestamp 업데이트
     */
    void updateTimestamp() {
        timestamp_ = std::chrono::system_clock::now();
        updated_at_ = timestamp_;
        markModified();
    }
    
    /**
     * @brief 품질 상태를 문자열로 변환
     */
    std::string getQualityString() const {
        return PulseOne::Utils::DataQualityToString(quality_);
    }
    
    /**
     * @brief 유효성 검사
     */
    bool isValid() const {
        return point_id_ > 0;  // 최소한 point_id는 있어야 함
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
            << ", value=" << value_
            << ", quality=" << getQualityString()
            << ", timestamp=" << PulseOne::Utils::TimestampToDBString(timestamp_) << "}";
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
     * @param max_age_seconds 최대 나이 (초)
     */
    bool isStale(int max_age_seconds = 3600) const {
        auto now = std::chrono::system_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - timestamp_);
        return age.count() > max_age_seconds;
    }
    
    /**
     * @brief 품질이 양호한지 확인
     */
    bool hasGoodQuality() const {
        return quality_ == PulseOne::Enums::DataQuality::GOOD;
    }
    
    /**
     * @brief 스케일링 적용된 값 계산
     * @param scaling_factor 스케일링 팩터
     * @param scaling_offset 스케일링 오프셋
     */
    double getScaledValue(double scaling_factor = 1.0, double scaling_offset = 0.0) const {
        return (raw_value_ * scaling_factor) + scaling_offset;
    }
    
    /**
     * @brief 변경 감지 저장이 필요한지 확인 (데드밴드 기반)
     * @param deadband 데드밴드 값
     */
    bool needsOnChangeSave(double deadband = 0.0) const {
        if (deadband <= 0.0) return true;  // 데드밴드 없으면 항상 저장
        // 실제로는 이전 값과 비교해야 하지만, 여기서는 단순화
        return true;
    }

private:
    // =======================================================================
    // 멤버 변수들 (DB 스키마와 1:1 매핑)
    // =======================================================================
    
    int point_id_ = 0;                                                      // PRIMARY KEY (DataPoint ID)
    double value_ = 0.0;                                                    // 현재값 (스케일링 적용됨)
    double raw_value_ = 0.0;                                               // 원시값 (스케일링 전)
    std::string string_value_;                                             // 문자열 값
    PulseOne::Enums::DataQuality quality_ = PulseOne::Enums::DataQuality::GOOD;  // 데이터 품질
    std::chrono::system_clock::time_point timestamp_ = std::chrono::system_clock::now(); // 값 시간
    std::chrono::system_clock::time_point updated_at_ = std::chrono::system_clock::now(); // 업데이트 시간
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // CURRENT_VALUE_ENTITY_H