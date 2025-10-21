/**
 * @file ExportTargetEntity.h
 * @brief Export Target 엔티티 (설정 전용)
 * @version 3.0.0 - 통계 필드 완전 제거
 * @date 2025-10-21
 * 
 * 저장 위치: core/shared/include/Database/Entities/ExportTargetEntity.h
 * 
 * 주요 변경사항:
 *   - 통계 필드 제거 (total_exports, successful_exports, failed_exports 등)
 *   - 설정 정보만 보관 (name, target_type, config, export_mode 등)
 *   - 통계는 export_logs 테이블에서 집계
 */

#ifndef EXPORT_TARGET_ENTITY_H
#define EXPORT_TARGET_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <string>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Database {

// Forward declarations
namespace Repositories {
    class ExportTargetRepository;
}

namespace Entities {

using json = nlohmann::json;

/**
 * @class ExportTargetEntity
 * @brief Export Target 설정 엔티티
 * 
 * 역할: 외부 전송 타겟의 설정 정보만 관리
 * 통계: export_logs 테이블에서 집계 (이 클래스에서는 제거됨)
 */
class ExportTargetEntity : public BaseEntity<ExportTargetEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    ExportTargetEntity();
    explicit ExportTargetEntity(int id);
    virtual ~ExportTargetEntity();
    
    // =======================================================================
    // Repository 패턴 지원
    // =======================================================================
    
    std::shared_ptr<Repositories::ExportTargetRepository> getRepository() const;
    
    // =======================================================================
    // Getter - 설정 정보만 (통계 필드 제거됨)
    // =======================================================================
    
    int getProfileId() const { return profile_id_; }
    std::string getName() const { return name_; }
    std::string getTargetType() const { return target_type_; }
    std::string getDescription() const { return description_; }
    bool isEnabled() const { return is_enabled_; }
    std::string getConfig() const { return config_; }
    std::string getExportMode() const { return export_mode_; }
    int getExportInterval() const { return export_interval_; }
    int getBatchSize() const { return batch_size_; }
    
    // =======================================================================
    // Setter - 설정 정보만 (통계 필드 제거됨)
    // =======================================================================
    
    void setProfileId(int profile_id) { 
        profile_id_ = profile_id; 
        markModified(); 
    }
    
    void setName(const std::string& name) { 
        name_ = name; 
        markModified(); 
    }
    
    void setTargetType(const std::string& target_type) { 
        target_type_ = target_type; 
        markModified(); 
    }
    
    void setDescription(const std::string& description) { 
        description_ = description; 
        markModified(); 
    }
    
    void setEnabled(bool enabled) { 
        is_enabled_ = enabled; 
        markModified(); 
    }
    
    void setConfig(const std::string& config) { 
        config_ = config; 
        markModified(); 
    }
    
    void setExportMode(const std::string& mode) { 
        export_mode_ = mode; 
        markModified(); 
    }
    
    void setExportInterval(int interval) { 
        export_interval_ = interval; 
        markModified(); 
    }
    
    void setBatchSize(int size) { 
        batch_size_ = size; 
        markModified(); 
    }
    
    // =======================================================================
    // 비즈니스 로직 (선언만, cpp에서 구현)
    // =======================================================================
    
    /**
     * @brief 설정 유효성 검사
     * @return true if valid, false otherwise
     */
    bool validate() const;
    
    /**
     * @brief Config JSON 파싱
     * @return Parsed JSON config
     */
    json parseConfig() const;
    
    /**
     * @brief Config JSON 설정
     * @param config JSON object
     */
    void setConfigJson(const json& config);
    
    /**
     * @brief 엔티티 타입명 반환
     */
    std::string getEntityTypeName() const;
    
    // =======================================================================
    // JSON 직렬화 (선언만, cpp에서 구현)
    // =======================================================================
    
    json toJson() const override;
    bool fromJson(const json& data) override;
    std::string toString() const override;
    
    // =======================================================================
    // DB 연산 (선언만, cpp에서 구현)
    // =======================================================================
    
    bool loadFromDatabase() override;
    bool saveToDatabase() override;
    bool updateToDatabase() override;
    bool deleteFromDatabase() override;
    
    std::string getTableName() const override { 
        return "export_targets"; 
    }

private:
    // =======================================================================
    // 멤버 변수 - 설정 정보만 (통계 필드 완전 제거)
    // =======================================================================
    
    int profile_id_ = 0;                        // FK to export_profiles
    std::string name_;                          // Target 이름
    std::string target_type_;                   // 'http', 's3', 'file' 등
    std::string description_;                   // 설명
    bool is_enabled_ = true;                    // 활성화 여부
    std::string config_;                        // JSON 설정
    std::string export_mode_ = "on_change";     // 전송 모드
    int export_interval_ = 0;                   // 주기 전송 간격 (초)
    int batch_size_ = 100;                      // 배치 크기
    
    // ❌ 제거된 필드들 (export_logs에서 집계)
    // uint64_t total_exports_;
    // uint64_t successful_exports_;
    // uint64_t failed_exports_;
    // std::optional<std::chrono::system_clock::time_point> last_export_at_;
    // std::optional<std::chrono::system_clock::time_point> last_success_at_;
    // std::string last_error_;
    // std::optional<std::chrono::system_clock::time_point> last_error_at_;
    // int avg_export_time_ms_;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // EXPORT_TARGET_ENTITY_H