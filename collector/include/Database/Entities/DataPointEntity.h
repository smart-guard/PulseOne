#ifndef DATA_POINT_ENTITY_H
#define DATA_POINT_ENTITY_H

#include "Common/UnifiedCommonTypes.h"
#include <string>
#include <chrono>

namespace PulseOne {
namespace Database {
namespace Entities {

class DataPointEntity {
public:
    DataPointEntity() : id_(0), device_id_(0), address_(0), enabled_(true), writable_(false) {}
    
    // ID 접근자
    int getId() const { return id_; }
    void setId(int id) { id_ = id; }
    
    // 외래키
    int getDeviceId() const { return device_id_; }
    void setDeviceId(int device_id) { device_id_ = device_id; }
    
    // 기본 속성들
    std::string getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }
    
    int getAddress() const { return address_; }
    void setAddress(int address) { address_ = address; }
    
    std::string getDataType() const { return data_type_; }
    void setDataType(const std::string& type) { data_type_ = type; }
    
    bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }
    
    bool isWritable() const { return writable_; }
    void setWritable(bool writable) { writable_ = writable; }
    
    std::string getTag() const { return tag_; }
    void setTag(const std::string& tag) { tag_ = tag; }
    
    std::string getDescription() const { return description_; }
    void setDescription(const std::string& desc) { description_ = desc; }

private:
    int id_;
    int device_id_;
    std::string name_;
    int address_;
    std::string data_type_;
    bool enabled_;
    bool writable_;
    std::string tag_;
    std::string description_;
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // DATA_POINT_ENTITY_H