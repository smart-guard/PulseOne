#include "Database/Entities/DeviceScheduleEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/DeviceScheduleRepository.h"

namespace PulseOne {
namespace Database {
namespace Entities {

DeviceScheduleEntity::DeviceScheduleEntity() 
    : BaseEntity<DeviceScheduleEntity>() {
}

DeviceScheduleEntity::DeviceScheduleEntity(int id) 
    : BaseEntity<DeviceScheduleEntity>(id) {
}

std::shared_ptr<Repositories::DeviceScheduleRepository> DeviceScheduleEntity::getRepository() const {
    return RepositoryFactory::getInstance().getDeviceScheduleRepository();
}

bool DeviceScheduleEntity::loadFromDatabase() {
    auto repo = RepositoryFactory::getInstance().getDeviceScheduleRepository();
    if (repo) {
        auto loaded = repo->findById(getId());
        if (loaded.has_value()) {
            *this = loaded.value();
            markSaved();
            return true;
        }
    }
    return false;
}

bool DeviceScheduleEntity::saveToDatabase() {
    auto repo = RepositoryFactory::getInstance().getDeviceScheduleRepository();
    if (repo) {
        bool success = repo->save(*this);
        if (success) markSaved();
        return success;
    }
    return false;
}

bool DeviceScheduleEntity::updateToDatabase() {
    auto repo = RepositoryFactory::getInstance().getDeviceScheduleRepository();
    if (repo) {
        bool success = repo->update(*this);
        if (success) markSaved();
        return success;
    }
    return false;
}

bool DeviceScheduleEntity::deleteFromDatabase() {
    auto repo = RepositoryFactory::getInstance().getDeviceScheduleRepository();
    if (repo) {
        bool success = repo->deleteById(getId());
        if (success) markDeleted();
        return success;
    }
    return false;
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne
