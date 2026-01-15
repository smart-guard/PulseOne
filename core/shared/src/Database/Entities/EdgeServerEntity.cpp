#include "Database/Entities/EdgeServerEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/EdgeServerRepository.h"
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Entities {

EdgeServerEntity::EdgeServerEntity()
    : BaseEntity<EdgeServerEntity>()
    , tenant_id_(0)
    , ip_address_("")
    , port_(50051)
    , is_enabled_(true)
    , max_devices_(100)
    , max_data_points_(1000)
    , created_at_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now()) {
}

EdgeServerEntity::EdgeServerEntity(int id)
    : EdgeServerEntity() {
    setId(id);
}

bool EdgeServerEntity::loadFromDatabase() {
    return loadViaRepository();
}

bool EdgeServerEntity::saveToDatabase() {
    return saveViaRepository();
}

bool EdgeServerEntity::deleteFromDatabase() {
    return deleteViaRepository();
}

bool EdgeServerEntity::updateToDatabase() {
    return updateViaRepository();
}

json EdgeServerEntity::toJson() const {
    json j;
    j["id"] = getId();
    j["tenant_id"] = tenant_id_;
    j["name"] = name_;
    j["description"] = description_;
    j["ip_address"] = ip_address_;
    j["port"] = port_;
    j["is_enabled"] = is_enabled_;
    j["max_devices"] = max_devices_;
    j["max_data_points"] = max_data_points_;
    j["created_at"] = timestampToString(created_at_);
    j["updated_at"] = timestampToString(updated_at_);
    return j;
}

bool EdgeServerEntity::fromJson(const json& data) {
    try {
        if (data.contains("id")) setId(data["id"]);
        if (data.contains("tenant_id")) tenant_id_ = data["tenant_id"];
        if (data.contains("name")) name_ = data["name"];
        if (data.contains("description")) description_ = data["description"];
        if (data.contains("ip_address")) ip_address_ = data["ip_address"];
        if (data.contains("port")) port_ = data["port"];
        if (data.contains("is_enabled")) is_enabled_ = data["is_enabled"];
        if (data.contains("max_devices")) max_devices_ = data["max_devices"];
        if (data.contains("max_data_points")) max_data_points_ = data["max_data_points"];
        return true;
    } catch (...) {
        return false;
    }
}

std::string EdgeServerEntity::toString() const {
    std::ostringstream oss;
    oss << "EdgeServerEntity[id=" << getId() << ", name=" << name_ << ", tenant_id=" << tenant_id_ << "]";
    return oss.str();
}

std::shared_ptr<Repositories::EdgeServerRepository> EdgeServerEntity::getRepository() const {
    return RepositoryFactory::getInstance().getEdgeServerRepository();
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne
