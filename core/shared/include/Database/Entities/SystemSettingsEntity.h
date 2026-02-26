#ifndef SYSTEM_SETTINGS_ENTITY_H
#define SYSTEM_SETTINGS_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <optional>
#include <string>

namespace PulseOne {
namespace Database {
namespace Entities {

/**
 * @class SystemSettingsEntity
 * @brief system_settings 테이블의 단일 행. key_name/value 쌍으로 구성.
 *        PostgreSQL / SQLite 모두 동일하게 동작.
 */
class SystemSettingsEntity : public BaseEntity<SystemSettingsEntity> {
public:
  SystemSettingsEntity() = default;
  virtual ~SystemSettingsEntity() = default;

  // --- BaseEntity 인터페이스 ---
  bool loadFromDatabase() override { return false; }
  bool saveToDatabase() override { return false; }
  bool deleteFromDatabase() override { return false; }
  bool updateToDatabase() override { return false; }

  json toJson() const override {
    json j;
    j["id"] = getId();
    j["key_name"] = key_name_;
    j["value"] = value_;
    j["category"] = category_;
    return j;
  }

  bool fromJson(const json &data) override {
    if (data.contains("id"))
      setId(data["id"].get<int>());
    if (data.contains("key_name"))
      key_name_ = data["key_name"].get<std::string>();
    if (data.contains("value"))
      value_ = data["value"].get<std::string>();
    if (data.contains("category"))
      category_ = data["category"].get<std::string>();
    return true;
  }

  std::string toString() const override {
    return "[SystemSettings] " + key_name_ + " = " + value_;
  }

  std::string getTableName() const override { return "system_settings"; }

  // --- Getters / Setters ---
  const std::string &getKeyName() const { return key_name_; }
  void setKeyName(const std::string &k) {
    key_name_ = k;
    markModified();
  }

  const std::string &getValue() const { return value_; }
  void setValue(const std::string &v) {
    value_ = v;
    markModified();
  }

  const std::string &getCategory() const { return category_; }
  void setCategory(const std::string &c) {
    category_ = c;
    markModified();
  }

private:
  std::string key_name_;
  std::string value_;
  std::string category_;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // SYSTEM_SETTINGS_ENTITY_H
