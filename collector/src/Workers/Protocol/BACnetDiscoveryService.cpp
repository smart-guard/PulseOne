// =============================================================================
// BACnetDiscoveryService.cpp - 구현 파일
// 파일 위치: collector/src/Workers/Protocol/BACnetDiscoveryService.cpp
// =============================================================================

/**
 * @file BACnetDiscoveryService.cpp
 * @brief BACnet 발견 서비스 구현
 */

#include "Workers/Protocol/BACnetDiscoveryService.h"
#include "Utils/LogManager.h"
#include "Database/Entities/DeviceEntity.h"
#include "Database/Entities/DataPointEntity.h"
#include "Database/Entities/CurrentValueEntity.h"
#include <sstream>

using namespace std::chrono;

namespace PulseOne {
namespace Workers {  // Services → Workers로 변경

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

BACnetDiscoveryService::BACnetDiscoveryService(
    std::shared_ptr<Database::Repositories::DeviceRepository> device_repo,
    std::shared_ptr<Database::Repositories::DataPointRepository> datapoint_repo,
    std::shared_ptr<Database::Repositories::CurrentValueRepository> current_value_repo)
    : device_repository_(device_repo)
    , datapoint_repository_(datapoint_repo)
    , current_value_repository_(current_value_repo)
    , is_active_(false) {
    
    if (!device_repository_ || !datapoint_repository_) {
        throw std::invalid_argument("DeviceRepository or DataPointRepository is null");
    }
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnetDiscoveryService created");
}

BACnetDiscoveryService::~BACnetDiscoveryService() {
    UnregisterFromWorker();
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnetDiscoveryService destroyed");
}

// =============================================================================
// BACnetWorker 연동
// =============================================================================

bool BACnetDiscoveryService::RegisterWithWorker(std::shared_ptr<Workers::BACnetWorker> worker) {
    if (!worker) {
        HandleError("RegisterWithWorker", "Worker is null");
        return false;
    }
    
    try {
        std::lock_guard<std::mutex> lock(service_mutex_);
        
        // 기존 워커 연동 해제
        UnregisterFromWorker();
        
        // 새 워커 등록
        registered_worker_ = worker;
        
        // 콜백 등록
        worker->SetDeviceDiscoveredCallback(
            [this](const Drivers::BACnetDeviceInfo& device) {
                OnDeviceDiscovered(device);
            }
        );
        
        worker->SetObjectDiscoveredCallback(
            [this](uint32_t device_id, const std::vector<Drivers::BACnetObjectInfo>& objects) {
                OnObjectDiscovered(device_id, objects);
            }
        );
        
        worker->SetValueChangedCallback(
            [this](const std::string& object_id, const PulseOne::TimestampedValue& value) {
                OnValueChanged(object_id, value);
            }
        );
        
        is_active_ = true;
        
        auto& logger = LogManager::getInstance();
        logger.Info("✅ BACnetDiscoveryService registered with BACnetWorker");
        
        return true;
        
    } catch (const std::exception& e) {
        HandleError("RegisterWithWorker", e.what());
        return false;
    }
}

void BACnetDiscoveryService::UnregisterFromWorker() {
    std::lock_guard<std::mutex> lock(service_mutex_);
    
    if (auto worker = registered_worker_.lock()) {
        // 콜백 해제 (nullptr로 설정)
        worker->SetDeviceDiscoveredCallback(nullptr);
        worker->SetObjectDiscoveredCallback(nullptr);
        worker->SetValueChangedCallback(nullptr);
    }
    
    registered_worker_.reset();
    is_active_ = false;
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnetDiscoveryService unregistered from worker");
}

// =============================================================================
// 콜백 핸들러들
// =============================================================================

void BACnetDiscoveryService::OnDeviceDiscovered(const Drivers::BACnetDeviceInfo& device) {
    try {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.devices_processed++;
        statistics_.last_activity = system_clock::now();
        
        auto& logger = LogManager::getInstance();
        logger.Info("🔍 BACnet device discovered: " + device.device_name + 
                   " (ID: " + std::to_string(device.device_id) + ")");
        
        if (SaveDiscoveredDeviceToDatabase(device)) {
            statistics_.devices_saved++;
            logger.Info("✅ Device saved to database: " + device.device_name);
        } else {
            statistics_.database_errors++;
            logger.Warn("❌ Failed to save device to database: " + device.device_name);
        }
        
    } catch (const std::exception& e) {
        HandleError("OnDeviceDiscovered", e.what());
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.database_errors++;
    }
}

void BACnetDiscoveryService::OnObjectDiscovered(uint32_t device_id, const std::vector<Drivers::BACnetObjectInfo>& objects) {
    try {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.objects_processed += objects.size();
        statistics_.last_activity = system_clock::now();
        
        auto& logger = LogManager::getInstance();
        logger.Info("🔍 BACnet objects discovered: " + std::to_string(objects.size()) + 
                   " objects for device " + std::to_string(device_id));
        
        if (SaveDiscoveredObjectsToDatabase(device_id, objects)) {
            statistics_.objects_saved += objects.size();
            logger.Info("✅ Objects saved to database: " + std::to_string(objects.size()) + " objects");
        } else {
            statistics_.database_errors++;
            logger.Warn("❌ Failed to save objects to database for device " + std::to_string(device_id));
        }
        
    } catch (const std::exception& e) {
        HandleError("OnObjectDiscovered", e.what());
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.database_errors++;
    }
}

void BACnetDiscoveryService::OnValueChanged(const std::string& object_id, const PulseOne::TimestampedValue& value) {
    try {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.values_processed++;
        statistics_.last_activity = system_clock::now();
        
        if (UpdateCurrentValueInDatabase(object_id, value)) {
            statistics_.values_saved++;
        } else {
            statistics_.database_errors++;
        }
        
    } catch (const std::exception& e) {
        HandleError("OnValueChanged", e.what());
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.database_errors++;
    }
}

bool BACnetDiscoveryService::SaveDiscoveredDeviceToDatabase(const Drivers::BACnetDeviceInfo& device) {
    try {
        auto& logger = LogManager::getInstance();
        
        // 1. 기존 디바이스 확인
        int db_device_id = FindDeviceIdInDatabase(device.device_id);
        
        if (db_device_id > 0) {
            // 기존 디바이스 업데이트
            auto existing_device = device_repository_->findById(db_device_id);
            if (existing_device.has_value()) {
                existing_device->setName(device.device_name);
                existing_device->setEndpoint("bacnet://" + device.ip_address + ":" + std::to_string(device.port));
                existing_device->setIsEnabled(true);
                existing_device->setLastSeen(device.last_seen);
                
                return device_repository_->update(*existing_device);
            }
        }
        
        // 2. 새 디바이스 생성
        Database::Entities::DeviceEntity new_device;
        new_device.setName(device.device_name);
        new_device.setProtocolType("BACNET_IP");
        new_device.setEndpoint("bacnet://" + device.ip_address + ":" + std::to_string(device.port) + 
                              "/device_id=" + std::to_string(device.device_id));
        new_device.setIsEnabled(true);
        new_device.setDescription("Auto-discovered BACnet device");
        new_device.setLastSeen(device.last_seen);
        
        // BACnet 특화 설정
        std::stringstream config_json;
        config_json << "{"
                   << "\"bacnet_device_id\":" << device.device_id << ","
                   << "\"ip_address\":\"" << device.ip_address << "\","
                   << "\"port\":" << device.port << ","
                   << "\"max_apdu_length\":" << device.max_apdu_length << ","
                   << "\"segmentation_supported\":" << (device.segmentation_supported ? "true" : "false") << ","
                   << "\"auto_discovered\":true"
                   << "}";
        
        new_device.setConfigJson(config_json.str());
        
        // 데이터베이스에 저장
        if (device_repository_->create(new_device)) {
            logger.Info("✅ New BACnet device saved: " + device.device_name + 
                       " (BACnet ID: " + std::to_string(device.device_id) + ")");
            return true;
        } else {
            logger.Error("❌ Failed to save device: " + device.device_name);
            return false;
        }
        
    } catch (const std::exception& e) {
        HandleError("SaveDiscoveredDeviceToDatabase", e.what());
        return false;
    }
}

bool BACnetDiscoveryService::SaveDiscoveredObjectsToDatabase(uint32_t device_id, const std::vector<Drivers::BACnetObjectInfo>& objects) {
    try {
        auto& logger = LogManager::getInstance();
        
        // 1. 데이터베이스 디바이스 ID 찾기
        int db_device_id = FindDeviceIdInDatabase(device_id);
        if (db_device_id <= 0) {
            logger.Error("❌ Device not found in database for BACnet device ID: " + std::to_string(device_id));
            return false;
        }
        
        size_t saved_count = 0;
        
        // 2. 각 객체를 DataPointEntity로 저장
        for (const auto& object : objects) {
            try {
                // 데이터포인트 ID 생성
                std::string datapoint_id = CreateDataPointId(device_id, object);
                
                // 기존 데이터포인트 확인
                auto existing_datapoint = datapoint_repository_->findByAddress(db_device_id, datapoint_id);
                
                if (existing_datapoint.has_value()) {
                    // 기존 데이터포인트 업데이트
                    existing_datapoint->setName(object.object_name);
                    existing_datapoint->setIsEnabled(true);
                    existing_datapoint->setLastSeen(object.timestamp);
                    
                    if (datapoint_repository_->update(*existing_datapoint)) {
                        saved_count++;
                    }
                } else {
                    // 새 데이터포인트 생성
                    Database::Entities::DataPointEntity new_datapoint;
                    new_datapoint.setDeviceId(db_device_id);
                    new_datapoint.setName(object.object_name.empty() ? 
                                         ObjectTypeToString(object.object_type) + "_" + std::to_string(object.object_instance) : 
                                         object.object_name);
                    new_datapoint.setAddress(datapoint_id);
                    new_datapoint.setDataType(DetermineDataType(object.value.tag));
                    new_datapoint.setIsEnabled(true);
                    new_datapoint.setDescription("Auto-discovered BACnet " + ObjectTypeToString(object.object_type));
                    new_datapoint.setLastSeen(object.timestamp);
                    
                    // BACnet 특화 설정
                    std::stringstream config_json;
                    config_json << "{"
                               << "\"bacnet_object_type\":" << static_cast<int>(object.object_type) << ","
                               << "\"bacnet_object_instance\":" << object.object_instance << ","
                               << "\"bacnet_property_id\":" << static_cast<int>(object.property_id) << ","
                               << "\"array_index\":" << object.array_index << ","
                               << "\"auto_discovered\":true"
                               << "}";
                    
                    new_datapoint.setConfigJson(config_json.str());
                    
                    if (datapoint_repository_->create(new_datapoint)) {
                        saved_count++;
                    }
                }
                
            } catch (const std::exception& e) {
                logger.Warn("Failed to save object " + std::to_string(object.object_instance) + ": " + e.what());
            }
        }
        
        logger.Info("✅ Saved " + std::to_string(saved_count) + "/" + std::to_string(objects.size()) + 
                   " BACnet objects for device " + std::to_string(device_id));
        
        return saved_count > 0;
        
    } catch (const std::exception& e) {
        HandleError("SaveDiscoveredObjectsToDatabase", e.what());
        return false;
    }
}

bool BACnetDiscoveryService::UpdateCurrentValueInDatabase(const std::string& object_id, const PulseOne::TimestampedValue& value) {
    if (!current_value_repository_) {
        return true; // CurrentValueRepository가 없으면 성공으로 간주
    }
    
    try {
        // object_id 파싱: "device_id:object_type:instance"
        std::vector<std::string> parts;
        std::stringstream ss(object_id);
        std::string part;
        
        while (std::getline(ss, part, ':')) {
            parts.push_back(part);
        }
        
        if (parts.size() != 3) {
            HandleError("UpdateCurrentValueInDatabase", "Invalid object_id format: " + object_id);
            return false;
        }
        
        uint32_t bacnet_device_id = static_cast<uint32_t>(std::stoul(parts[0]));
        
        // 데이터베이스 디바이스 ID 찾기
        int db_device_id = FindDeviceIdInDatabase(bacnet_device_id);
        if (db_device_id <= 0) {
            return false;
        }
        
        // 데이터포인트 ID로 데이터포인트 찾기
        auto datapoint = datapoint_repository_->findByAddress(db_device_id, object_id);
        if (!datapoint.has_value()) {
            return false;
        }
        
        // CurrentValueEntity 생성/업데이트
        Database::Entities::CurrentValueEntity current_value;
        current_value.setDataPointId(datapoint->getId());
        current_value.setValue(ConvertDataValueToString(value.value));
        current_value.setQuality(static_cast<int>(value.quality));
        current_value.setTimestamp(value.timestamp);
        
        // 기존 값 확인 후 생성/업데이트
        auto existing_value = current_value_repository_->findByDataPointId(datapoint->getId());
        
        if (existing_value.has_value()) {
            existing_value->setValue(current_value.getValue());
            existing_value->setQuality(current_value.getQuality());
            existing_value->setTimestamp(current_value.getTimestamp());
            return current_value_repository_->update(*existing_value);
        } else {
            return current_value_repository_->create(current_value);
        }
        
    } catch (const std::exception& e) {
        HandleError("UpdateCurrentValueInDatabase", e.what());
        return false;
    }
}

// =============================================================================
// 헬퍼 메서드들
// =============================================================================

int BACnetDiscoveryService::FindDeviceIdInDatabase(uint32_t bacnet_device_id) {
    try {
        // BACnet 디바이스 ID를 endpoint나 config_json에서 찾기
        auto all_devices = device_repository_->findByProtocolType("BACNET_IP");
        
        for (const auto& device : all_devices) {
            // endpoint에서 device_id 파라미터 확인
            std::string endpoint = device.getEndpoint();
            std::string search_str = "device_id=" + std::to_string(bacnet_device_id);
            
            if (endpoint.find(search_str) != std::string::npos) {
                return device.getId();
            }
            
            // config_json에서 bacnet_device_id 확인
            std::string config_json = device.getConfigJson();
            std::string json_search = "\"bacnet_device_id\":" + std::to_string(bacnet_device_id);
            
            if (config_json.find(json_search) != std::string::npos) {
                return device.getId();
            }
        }
        
        return -1; // 찾지 못함
        
    } catch (const std::exception& e) {
        HandleError("FindDeviceIdInDatabase", e.what());
        return -1;
    }
}

std::string BACnetDiscoveryService::CreateDataPointId(uint32_t device_id, const Drivers::BACnetObjectInfo& object) {
    return std::to_string(device_id) + ":" + 
           std::to_string(static_cast<int>(object.object_type)) + ":" + 
           std::to_string(object.object_instance);
}

std::string BACnetDiscoveryService::ObjectTypeToString(BACNET_OBJECT_TYPE type) {
    switch (type) {
        case OBJECT_ANALOG_INPUT:       return "Analog Input";
        case OBJECT_ANALOG_OUTPUT:      return "Analog Output";
        case OBJECT_ANALOG_VALUE:       return "Analog Value";
        case OBJECT_BINARY_INPUT:       return "Binary Input";
        case OBJECT_BINARY_OUTPUT:      return "Binary Output";
        case OBJECT_BINARY_VALUE:       return "Binary Value";
        case OBJECT_MULTI_STATE_INPUT:  return "Multi-state Input";
        case OBJECT_MULTI_STATE_OUTPUT: return "Multi-state Output";
        case OBJECT_MULTI_STATE_VALUE:  return "Multi-state Value";
        case OBJECT_DEVICE:             return "Device";
        default:                        return "Unknown Object (" + std::to_string(type) + ")";
    }
}

std::string BACnetDiscoveryService::DetermineDataType(BACNET_APPLICATION_TAG tag) {
    switch (tag) {
        case BACNET_APPLICATION_TAG_BOOLEAN:       return "BOOLEAN";
        case BACNET_APPLICATION_TAG_UNSIGNED_INT:  return "UINT32";
        case BACNET_APPLICATION_TAG_SIGNED_INT:    return "INT32";
        case BACNET_APPLICATION_TAG_REAL:          return "FLOAT";
        case BACNET_APPLICATION_TAG_DOUBLE:        return "DOUBLE";
        case BACNET_APPLICATION_TAG_CHARACTER_STRING: return "STRING";
        default:                                   return "VARIANT";
    }
}

std::string BACnetDiscoveryService::ConvertDataValueToString(const PulseOne::Structs::DataValue& value) {
    try {
        if (std::holds_alternative<bool>(value)) {
            return std::get<bool>(value) ? "true" : "false";
        } else if (std::holds_alternative<int32_t>(value)) {
            return std::to_string(std::get<int32_t>(value));
        } else if (std::holds_alternative<uint32_t>(value)) {
            return std::to_string(std::get<uint32_t>(value));
        } else if (std::holds_alternative<float>(value)) {
            return std::to_string(std::get<float>(value));
        } else if (std::holds_alternative<double>(value)) {
            return std::to_string(std::get<double>(value));
        } else if (std::holds_alternative<std::string>(value)) {
            return std::get<std::string>(value);
        } else {
            return "unknown";
        }
    } catch (const std::exception&) {
        return "error";
    }
}

void BACnetDiscoveryService::HandleError(const std::string& operation, const std::string& error_message) {
    auto& logger = LogManager::getInstance();
    logger.Error("BACnetDiscoveryService::" + operation + " - " + error_message);
}

// =============================================================================
// 통계 및 상태
// =============================================================================

BACnetDiscoveryStats BACnetDiscoveryService::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

void BACnetDiscoveryService::ResetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    statistics_ = BACnetDiscoveryStats();
}

bool BACnetDiscoveryService::IsActive() const {
    std::lock_guard<std::mutex> lock(service_mutex_);
    return is_active_ && !registered_worker_.expired();
}

} // namespace Workers
} // namespace PulseOne