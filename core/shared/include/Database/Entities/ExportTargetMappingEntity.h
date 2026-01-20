/**
 * @file ExportTargetMappingEntity.h
 * @version 2.0.0 - inline 구현 제거
 */

#ifndef EXPORT_TARGET_MAPPING_ENTITY_H
#define EXPORT_TARGET_MAPPING_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <chrono>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace PulseOne {
namespace Database {
namespace Entities {

using json = nlohmann::json;

class ExportTargetMappingEntity : public BaseEntity<ExportTargetMappingEntity> {
public:
  // ✅ 선언만 (cpp로 이동)
  ExportTargetMappingEntity();
  explicit ExportTargetMappingEntity(int id);
  virtual ~ExportTargetMappingEntity();

  // ✅ Repository 패턴
  std::shared_ptr<Repositories::ExportTargetMappingRepository>
  getRepository() const;

  // Getters (inline 허용)
  int getTargetId() const { return target_id_; }
  int getPointId() const { return point_id_; }
  std::string getTargetFieldName() const { return target_field_name_; }
  std::string getTargetDescription() const { return target_description_; }
  std::string getConversionConfig() const { return conversion_config_; }
  bool isEnabled() const { return is_enabled_; }

  // Setters (inline 허용)
  void setTargetId(int target_id) {
    target_id_ = target_id;
    markModified();
  }
  void setPointId(int point_id) {
    point_id_ = point_id;
    markModified();
  }
  void setTargetFieldName(const std::string &name) {
    target_field_name_ = name;
    markModified();
  }
  void setTargetDescription(const std::string &description) {
    target_description_ = description;
    markModified();
  }
  void setConversionConfig(const std::string &config) {
    conversion_config_ = config;
    markModified();
  }
  void setEnabled(bool enabled) {
    is_enabled_ = enabled;
    markModified();
  }

  // ✅ Conversion
  double getScale() const;
  double getOffset() const;

  // ✅ 비즈니스 로직 선언만 (cpp로 이동)
  bool hasConversion() const;
  bool validate() const;
  std::string getEntityTypeName() const;

  // ✅ JSON 직렬화 선언만 (cpp로 이동)
  json toJson() const override;
  bool fromJson(const json &data) override;
  std::string toString() const override;

  // ✅ DB 연산 선언만
  bool loadFromDatabase() override;
  bool saveToDatabase() override;
  bool updateToDatabase() override;
  bool deleteFromDatabase() override;

  std::string getTableName() const override { return "export_target_mappings"; }

private:
  int target_id_ = 0;
  int point_id_ = 0;
  std::string target_field_name_;
  std::string target_description_;
  std::string conversion_config_;
  bool is_enabled_ = true;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif