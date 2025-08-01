/**
 * @file BACnetDiscoveryService.cpp
 * @brief BACnet 발견 서비스 구현 - 모든 컴파일 오류 완전 수정
 * @author PulseOne Development Team
 * @date 2025-08-01
 */

#include "Workers/Protocol/BACnetDiscoveryService.h"
#include "Utils/LogManager.h"
#include "Database/Entities/DeviceEntity.h"
#include "Database/Entities/DataPointEntity.h"
#include "Database/Entities/CurrentValueEntity.h"
#include <sstream>

using namespace std::chrono;

namespace PulseOne {
namespace Workers {

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
// 서비스 제어
// =============================================================================

bool BACnetDiscoveryService::RegisterToWorker(std::shared_ptr<BACnetWorker> worker) {
    if (!worker) {
        auto& logger = LogManager::getInstance();
        logger.Error("BACnetDiscoveryService::RegisterToWorker - Worker is null");
        return false;
    }
    
    registered_worker_ = worker;
    is_active_ = true;
    
    // 콜백 등록
    worker->SetDeviceDiscoveredCallback([this](const Drivers::BACnetDeviceInfo& device) {
        OnDeviceDiscovered(device);
    });
    
    worker->SetObjectDiscoveredCallback([this](uint32_t device_id, const std::vector<Drivers::BACnetObjectInfo>& objects) {
        OnObjectDiscovered(device_id, objects);
    });
    
    worker->SetValueChangedCallback([this](const std::string& object_id, const TimestampedValue& value) {
        OnValueChanged(object_id, value);
    });
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnetDiscoveryService registered to worker");
    return true;
}

void BACnetDiscoveryService::UnregisterFromWorker() {
    if (auto worker = registered_worker_.lock()) {
        // 콜백 제거 (nullptr로 설정)
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
        logger.Info("🔍 BACnet objects discovered for device " + std::to_string(device_id) + 
                   ": " + std::to_string(objects.size()) + " objects");
        
        if (SaveDiscoveredObjectsToDatabase(device_id, objects)) {
            statistics_.objects_saved += objects.size();
            logger.Info("✅ Objects saved to database for device: " + std::to_string(device_id));
        } else {
            statistics_.database_errors++;
            logger.Warn("❌ Failed to save objects to database for device: " + std::to_string(device_id));
        }
        
    } catch (const std::exception& e) {
        HandleError("OnObjectDiscovered", e.what());
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.database_errors++;
    }
}

void BACnetDiscoveryService::OnValueChanged(const std::string& object_id, const TimestampedValue& value) {
    try {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.values_processed++;
        statistics_.last_activity = system_clock::now();
        
        auto& logger = LogManager::getInstance();
        logger.Debug("🔄 BACnet value changed: " + object_id);
        
        if (UpdateCurrentValueInDatabase(object_id, value)) {
            statistics_.values_saved++;
        } else {
            statistics_.database_errors++;
            logger.Warn("❌ Failed to update value for object: " + object_id);
        }
        
    } catch (const std::exception& e) {
        HandleError("OnValueChanged", e.what());
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.database_errors++;
    }
}

// =============================================================================
// 데이터베이스 저장 로직
// =============================================================================

bool BACnetDiscoveryService::SaveDiscoveredDeviceToDatabase(const Drivers::BACnetDeviceInfo& device) {
    try {
        // 기존 디바이스 조회
        auto existing_devices = device_repository_->findByProtocol("BACNET_IP");  // ✅ 수정
        for (auto& dev : existing_devices) {
            try {
                auto config = dev.getConfigAsJson();
                if (config.contains("device_id") && 
                    config["device_id"].get<uint32_t>() == device.device_id) {
                    
                    // 기존 디바이스 업데이트
                    dev.setEnabled(true);  // ✅ 수정
                    // ❌ setLastSeen 제거 (DeviceEntity에 없음)
                    return device_repository_->update(dev);
                }
            } catch (...) {
                // JSON 파싱 실패 시 무시
                continue;
            }
        }
        
        // 새 디바이스 생성
        Database::Entities::DeviceEntity new_device;
        new_device.setName(device.device_name);
        new_device.setProtocolType("BACNET_IP");  // ✅ 수정
        new_device.setEndpoint(device.ip_address + ":47808");
        new_device.setEnabled(true);  // ✅ 수정
        new_device.setDescription("Auto-discovered BACnet device");
        // ❌ setLastSeen 제거 (DeviceEntity에 없음)
        
        // 설정 JSON 생성 - BACnetDeviceInfo에 실제 존재하는 필드만 사용
        std::ostringstream config_json;
        config_json << "{"
                   << "\"device_id\":" << device.device_id << ","
                   << "\"ip_address\":\"" << device.ip_address << "\","
                   << "\"port\":" << device.port << ","
                   << "\"max_apdu_length\":" << device.max_apdu_length << ","
                   << "\"segmentation_supported\":" << device.segmentation_supported
                   << "}";
        
        new_device.setConfigAsJson(config_json.str());  // ✅ 수정
        
        // 데이터베이스에 저장 
        return device_repository_->save(new_device);  // ✅ 수정
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("SaveDiscoveredDeviceToDatabase failed: " + std::string(e.what()));
        return false;
    }
}

bool BACnetDiscoveryService::SaveDiscoveredObjectsToDatabase(uint32_t device_id, const std::vector<Drivers::BACnetObjectInfo>& objects) {
    try {
        // 데이터베이스에서 디바이스 ID 조회
        int db_device_id = FindDeviceIdInDatabase(device_id);
        if (db_device_id <= 0) {
            auto& logger = LogManager::getInstance();
            logger.Error("Device not found in database: " + std::to_string(device_id));
            return false;
        }
        
        for (const auto& object : objects) {
            // 고유 DataPoint ID 생성
            std::string datapoint_id = GenerateDataPointId(device_id, object);
            
            // 기존 DataPoint 조회 - metadata 기반으로 검색 ✅
            auto existing_datapoints = datapoint_repository_->findByDeviceId(db_device_id);
            bool found_existing = false;
            
            for (auto& dp : existing_datapoints) {
                const auto& metadata_map = dp.getMetadata();  // ✅ map 타입
                
                // map에서 original_datapoint_id 찾기
                auto it = metadata_map.find("original_datapoint_id");
                if (it != metadata_map.end() && it->second == datapoint_id) {
                    // 기존 DataPoint 업데이트
                    dp.setEnabled(true);  // ✅ 수정
                    // ❌ setLastSeen 제거 (DataPointEntity에 없음)
                    datapoint_repository_->update(dp);
                    found_existing = true;
                    break;
                }
            }
            
            if (!found_existing) {
                // 새 DataPoint 생성
                Database::Entities::DataPointEntity new_datapoint;
                new_datapoint.setDeviceId(db_device_id);
                new_datapoint.setName(object.object_name);
                new_datapoint.setDescription(ObjectTypeToString(static_cast<int>(object.object_type)) + " - Auto-discovered");
                
                // ⚠️ address는 int 타입이므로 hash 값으로 변환 ✅
                std::hash<std::string> hasher;
                size_t hash_value = hasher(datapoint_id);
                new_datapoint.setAddress(static_cast<int>(hash_value & 0x7FFFFFFF));  // 양수로 제한
                
                new_datapoint.setDataType(DetermineDataType(static_cast<int>(object.value.tag)));
                new_datapoint.setEnabled(true);  // ✅ 수정
                new_datapoint.setUnit("");
                // ❌ setLastSeen 제거 (DataPointEntity에 없음)
                
                // 메타데이터 JSON 생성 - map이 아닌 JSON 문자열로 변환 후 map으로 설정
                std::ostringstream config_json;
                config_json << "{"
                           << "\"bacnet_object_type\":" << static_cast<int>(object.object_type) << ","
                           << "\"bacnet_instance\":" << object.object_instance << ","
                           << "\"original_datapoint_id\":\"" << datapoint_id << "\""
                           << "}";
                
                // JSON 문자열을 map으로 변환
                std::map<std::string, std::string> metadata_map;
                metadata_map["bacnet_object_type"] = std::to_string(static_cast<int>(object.object_type));
                metadata_map["bacnet_instance"] = std::to_string(object.object_instance);
                metadata_map["original_datapoint_id"] = datapoint_id;
                
                new_datapoint.setMetadata(metadata_map);  // ✅ 수정
                
                if (!datapoint_repository_->save(new_datapoint)) {  // ✅ 수정
                    auto& logger = LogManager::getInstance();
                    logger.Error("Failed to save DataPoint: " + new_datapoint.getName());
                    return false;
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("SaveDiscoveredObjectsToDatabase failed: " + std::string(e.what()));
        return false;
    }
}

bool BACnetDiscoveryService::UpdateCurrentValueInDatabase(const std::string& object_id, const TimestampedValue& value) {
    try {
        // object_id에서 device_id 추출
        size_t first_colon = object_id.find(':');
        if (first_colon == std::string::npos) {
            return false;
        }
        
        uint32_t device_id = std::stoul(object_id.substr(0, first_colon));
        int db_device_id = FindDeviceIdInDatabase(device_id);
        
        if (db_device_id <= 0) {
            return false;
        }
        
        // DataPoint 조회 - metadata 기반으로 검색 ✅
        std::string datapoint_id = object_id;  // object_id 그대로 사용
        auto datapoints = datapoint_repository_->findByDeviceId(db_device_id);
        Database::Entities::DataPointEntity* target_datapoint = nullptr;
        
        for (auto& dp : datapoints) {
            const auto& metadata_map = dp.getMetadata();  // ✅ map 타입
            
            // map에서 original_datapoint_id 찾기
            auto it = metadata_map.find("original_datapoint_id");
            if (it != metadata_map.end() && it->second == datapoint_id) {
                target_datapoint = &dp;
                break;
            }
        }
        
        if (!target_datapoint) {
            return false;
        }
        
        // CurrentValue 엔티티 생성
        Database::Entities::CurrentValueEntity current_value;
        current_value.setPointId(target_datapoint->getId());  // ✅ 수정
        
        // ✅ string 값을 setStringValue()로 저장
        current_value.setStringValue(ConvertDataValueToString(value.value));
        
        // ✅ quality 캐스팅 수정
        current_value.setQuality(static_cast<PulseOne::Enums::DataQuality>(static_cast<int>(value.quality)));
        current_value.setTimestamp(value.timestamp);
        
        // 기존 CurrentValue 조회 후 업데이트 또는 생성
        if (current_value_repository_) {
            auto existing = current_value_repository_->findByDataPointId(target_datapoint->getId());  // ✅ 수정
            if (existing.has_value()) {
                current_value.setId(existing->getId());
                return current_value_repository_->update(current_value);
            } else {
                return current_value_repository_->save(current_value);  // ✅ 수정
            }
        }
        
        return false;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("UpdateCurrentValueInDatabase failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 헬퍼 메서드들 
// =============================================================================

int BACnetDiscoveryService::FindDeviceIdInDatabase(uint32_t bacnet_device_id) {
    try {
        auto all_devices = device_repository_->findByProtocol("BACNET_IP");  // ✅ 수정
        
        for (const auto& device : all_devices) {
            try {
                auto config = device.getConfigAsJson();
                if (config.contains("device_id") && 
                    config["device_id"].get<uint32_t>() == bacnet_device_id) {
                    return device.getId();
                }
            } catch (...) {
                // JSON 파싱 실패 시 무시
                continue;
            }
        }
        
        return -1;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("FindDeviceIdInDatabase failed: " + std::string(e.what()));
        return -1;
    }
}

std::string BACnetDiscoveryService::GenerateDataPointId(uint32_t device_id, const Drivers::BACnetObjectInfo& object) {
    return std::to_string(device_id) + ":" + 
           std::to_string(static_cast<int>(object.object_type)) + ":" + 
           std::to_string(object.object_instance);
}

std::string BACnetDiscoveryService::ObjectTypeToString(int type) {
    switch (type) {
        case 0:  return "Analog Input";      // OBJECT_ANALOG_INPUT
        case 1:  return "Analog Output";     // OBJECT_ANALOG_OUTPUT  
        case 2:  return "Analog Value";      // OBJECT_ANALOG_VALUE
        case 3:  return "Binary Input";      // OBJECT_BINARY_INPUT
        case 4:  return "Binary Output";     // OBJECT_BINARY_OUTPUT
        case 5:  return "Binary Value";      // OBJECT_BINARY_VALUE
        case 13: return "Multi-state Input"; // OBJECT_MULTI_STATE_INPUT
        case 14: return "Multi-state Output";// OBJECT_MULTI_STATE_OUTPUT
        case 19: return "Multi-state Value"; // OBJECT_MULTI_STATE_VALUE
        case 8:  return "Device";            // OBJECT_DEVICE
        default: return "Unknown Object (" + std::to_string(type) + ")";
    }
}

std::string BACnetDiscoveryService::DetermineDataType(int tag) {
    switch (tag) {
        case 1:  return "BOOLEAN";       // BACNET_APPLICATION_TAG_BOOLEAN
        case 2:  return "UINT32";        // BACNET_APPLICATION_TAG_UNSIGNED_INT
        case 3:  return "INT32";         // BACNET_APPLICATION_TAG_SIGNED_INT
        case 4:  return "FLOAT";         // BACNET_APPLICATION_TAG_REAL
        case 5:  return "DOUBLE";        // BACNET_APPLICATION_TAG_DOUBLE
        case 7:  return "STRING";        // BACNET_APPLICATION_TAG_CHARACTER_STRING
        default: return "VARIANT";
    }
}

std::string BACnetDiscoveryService::ConvertDataValueToString(const PulseOne::Structs::DataValue& value) {
    try {
        if (std::holds_alternative<bool>(value)) {
            return std::get<bool>(value) ? "true" : "false";
        } else if (std::holds_alternative<int>(value)) {
            return std::to_string(std::get<int>(value));
        } else if (std::holds_alternative<double>(value)) {
            return std::to_string(std::get<double>(value));
        } else if (std::holds_alternative<float>(value)) {
            return std::to_string(std::get<float>(value));
        } else if (std::holds_alternative<std::string>(value)) {
            return std::get<std::string>(value);
        }
        return "null";
    } catch (const std::exception& e) {
        return "conversion_error";
    }
}

// =============================================================================
// 통계 및 상태 조회
// =============================================================================

BACnetDiscoveryService::Statistics BACnetDiscoveryService::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

bool BACnetDiscoveryService::IsActive() const {
    return is_active_ && !registered_worker_.expired();
}

void BACnetDiscoveryService::ResetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    statistics_ = Statistics{};
}

// =============================================================================
// 내부 유틸리티
// =============================================================================

void BACnetDiscoveryService::HandleError(const std::string& context, const std::string& error) {
    auto& logger = LogManager::getInstance();
    logger.Error("BACnetDiscoveryService::" + context + " - " + error);
}

} // namespace Workers
} // namespace PulseOne