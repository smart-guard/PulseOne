#ifndef DEVICE_SCHEDULE_REPOSITORY_H
#define DEVICE_SCHEDULE_REPOSITORY_H

/**
 * @file DeviceScheduleRepository.h
 * @brief PulseOne DeviceScheduleRepository - 디바이스 스케줄 관리용 리포지토리
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/DeviceScheduleEntity.h"
#include <memory>
#include <vector>
#include <optional>

namespace PulseOne {
namespace Database {
namespace Repositories {

using DeviceScheduleEntity = PulseOne::Database::Entities::DeviceScheduleEntity;

class DeviceScheduleRepository : public IRepository<DeviceScheduleEntity> {
public:
    DeviceScheduleRepository();
    virtual ~DeviceScheduleRepository() = default;

    // IRepository 인터페이스 구현
    std::vector<DeviceScheduleEntity> findAll() override;
    std::optional<DeviceScheduleEntity> findById(int id) override;
    bool save(DeviceScheduleEntity& entity) override;
    bool update(const DeviceScheduleEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;

    // 확장 메서드
    std::vector<DeviceScheduleEntity> findByDeviceId(int device_id);
    std::vector<DeviceScheduleEntity> findPendingSync(int device_id = -1);
    bool markAsSynced(int id);

private:
    DeviceScheduleEntity mapRowToEntity(const std::map<std::string, std::string>& row);
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // DEVICE_SCHEDULE_REPOSITORY_H
