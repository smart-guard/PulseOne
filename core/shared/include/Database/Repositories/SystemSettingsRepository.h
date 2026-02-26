#ifndef SYSTEM_SETTINGS_REPOSITORY_H
#define SYSTEM_SETTINGS_REPOSITORY_H

#include "Database/Entities/SystemSettingsEntity.h"
#include "Database/Repositories/IRepository.h"
#include <optional>
#include <string>
#include <vector>

namespace PulseOne {
namespace Database {
namespace Repositories {

using SystemSettingsEntity = PulseOne::Database::Entities::SystemSettingsEntity;

/**
 * @class SystemSettingsRepository
 * @brief system_settings 테이블 CRUD. PostgreSQL / SQLite 모두 지원.
 */
class SystemSettingsRepository : public IRepository<SystemSettingsEntity> {
public:
  SystemSettingsRepository();
  virtual ~SystemSettingsRepository() = default;

  std::vector<SystemSettingsEntity> findAll() override;
  std::optional<SystemSettingsEntity> findById(int id) override;
  bool save(SystemSettingsEntity &entity) override;
  bool update(const SystemSettingsEntity &entity) override;
  bool deleteById(int id) override;
  bool exists(int id) override;

  /** key_name으로 단일 설정 조회 */
  std::optional<SystemSettingsEntity> findByKey(const std::string &key);

  /** key_name으로 값만 반환. 없으면 defaultValue */
  std::string getValue(const std::string &key,
                       const std::string &defaultValue = "");

  /** key_name/value upsert. 없으면 insert, 있으면 update */
  bool updateValue(const std::string &key, const std::string &value);

private:
  SystemSettingsEntity
  mapRowToEntity(const std::map<std::string, std::string> &row);
  std::vector<SystemSettingsEntity> mapResultToEntities(
      const std::vector<std::map<std::string, std::string>> &result);
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // SYSTEM_SETTINGS_REPOSITORY_H
