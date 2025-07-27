#ifndef DEVICE_ENTITY_H
#define DEVICE_ENTITY_H

#include "Common/UnifiedCommonTypes.h"
#include <string>
#include <chrono>

namespace PulseOne {
namespace Database {
namespace Entities {

class DeviceEntity {
public:
    DeviceEntity() : id_(0), enabled_(true) {}
    
    // ID 접근자
    int getId() const { return id_; }
    void setId(int id) { id_ = id; }
    
    // 기본 속성들
    std::string getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }
    
    std::string getProtocolType() const { return protocol_type_; }
    void setProtocolType(const std::string& type) { protocol_type_ = type; }
    
    std::string getEndpoint() const { return endpoint_; }
    void setEndpoint(const std::string& endpoint) { endpoint_ = endpoint; }
    
    bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }
    
    std::string getDescription() const { return description_; }
    void setDescription(const std::string& desc) { description_ = desc; }

private:
    int id_;
    std::string name_;
    std::string protocol_type_;
    std::string endpoint_;
    bool enabled_;
    std::string description_;
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // DEVICE_ENTITY_H