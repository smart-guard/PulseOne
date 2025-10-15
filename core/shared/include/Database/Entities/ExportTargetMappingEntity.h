/**
 * @file ExportTargetMappingEntity.h
 * @brief Export Target Mapping Entity - BaseEntity 패턴 적용
 * @author PulseOne Development Team
 * @date 2025-10-15
 * @version 1.0.0
 * 저장 위치: core/shared/include/Database/Entities/ExportTargetMappingEntity.h
 * 
 * 테이블: export_target_mappings
 * 기능: 타겟별 포인트 매핑 및 변환 설정 관리
 */

#ifndef EXPORT_TARGET_MAPPING_ENTITY_H
#define EXPORT_TARGET_MAPPING_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>  // 🔧 추가

namespace PulseOne {
namespace Database {
namespace Entities {

using json = nlohmann::json;  // 🔧 추가

/**
 * @brief Export Target Mapping Entity
 * 
 * export_target_mappings 테이블과 매핑
 */
class ExportTargetMappingEntity : public BaseEntity<ExportTargetMappingEntity> {
public:
    // =======================================================================
    // 생성자
    // =======================================================================
    
    ExportTargetMappingEntity() = default;
    
    explicit ExportTargetMappingEntity(int id) : BaseEntity(id) {}
    
    virtual ~ExportTargetMappingEntity() = default;
    
    // =======================================================================
    // Getters
    // =======================================================================
    
    int getTargetId() const { return target_id_; }
    int getPointId() const { return point_id_; }
    std::string getTargetFieldName() const { return target_field_name_; }
    std::string getTargetDescription() const { return target_description_; }
    std::string getConversionConfig() const { return conversion_config_; }
    bool isEnabled() const { return is_enabled_; }
    
    // =======================================================================
    // Setters
    // =======================================================================
    
    void setTargetId(int target_id) { 
        target_id_ = target_id; 
        markModified(); 
    }
    
    void setPointId(int point_id) { 
        point_id_ = point_id; 
        markModified(); 
    }
    
    void setTargetFieldName(const std::string& name) { 
        target_field_name_ = name; 
        markModified(); 
    }
    
    void setTargetDescription(const std::string& description) { 
        target_description_ = description; 
        markModified(); 
    }
    
    void setConversionConfig(const std::string& config) { 
        conversion_config_ = config; 
        markModified(); 
    }
    
    void setEnabled(bool enabled) { 
        is_enabled_ = enabled; 
        markModified(); 
    }
    
    // =======================================================================
    // 비즈니스 로직
    // =======================================================================
    
    /**
     * @brief 변환 설정 유무 확인
     */
    bool hasConversion() const {
        return !conversion_config_.empty() && conversion_config_ != "{}";
    }
    
    /**
     * @brief 유효성 검증
     */
    bool validate() const {  // 🔧 override 제거
        if (target_id_ <= 0) return false;
        if (point_id_ <= 0) return false;
        return true;
    }
    
    /**
     * @brief JSON 변환
     */
    json toJson() const override {  // 🔧 반환타입 변경
        json j;
        j["id"] = id_;
        j["target_id"] = target_id_;
        j["point_id"] = point_id_;
        j["target_field_name"] = target_field_name_;
        j["target_description"] = target_description_;
        j["conversion_config"] = conversion_config_;
        j["is_enabled"] = is_enabled_;
        
        return j;  // 🔧 변경
    }
    
    // 🔧 추가: BaseEntity 순수 가상 함수
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool updateToDatabase() override;
    bool deleteFromDatabase() override;
    bool fromJson(const json& data) override;
    std::string toString() const override { return toJson().dump(2); }
    
    /**
     * @brief 엔티티 타입 이름
     */
    std::string getEntityTypeName() const {  // 🔧 override 제거
        return "ExportTargetMapping";
    }
    std::string getTableName() const override { return "export_target_mappings"; } 
    
private:
    int target_id_ = 0;
    int point_id_ = 0;
    std::string target_field_name_;
    std::string target_description_;
    std::string conversion_config_;  // JSON 변환 규칙
    bool is_enabled_ = true;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // EXPORT_TARGET_MAPPING_ENTITY_H