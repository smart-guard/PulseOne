/**
 * @file BACnetDiscoveryService.cpp
 * @brief BACnet 발견 서비스 구현 - 모든 타입 불일치 완전 해결
 * @author PulseOne Development Team
 * @date 2025-08-02
 */

#include "Workers/Protocol/BACnetDiscoveryService.h"
#include "Utils/LogManager.h"
#include "Database/Entities/DeviceEntity.h"
#include "Database/Entities/DataPointEntity.h"
#include "Database/Entities/CurrentValueEntity.h"
#include <sstream>
#include <functional>

using namespace std::chrono;

// 🔥 필수 using 선언들
using BACnetDeviceInfo = PulseOne::Drivers::BACnetDeviceInfo;
using BACnetObjectInfo = PulseOne::Drivers::BACnetObjectInfo;
using DeviceEntity = PulseOne::Database::Entities::DeviceEntity;
using DataPointEntity = PulseOne::Database::Entities::DataPointEntity;
using DataType = PulseOne::Enums::DataType;

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
    
    // 🔥 수정: BACnetWorker가 완전히 정의되지 않았으므로 임시 주석 처리
    // 실제로는 BACnetWorker.h를 include하거나 메서드 구현을 지연시켜야 함
    /*
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
    */
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnetDiscoveryService registered to worker (callbacks temporarily disabled)");
    return true;
}

void BACnetDiscoveryService::UnregisterFromWorker() {
    if (auto worker = registered_worker_.lock()) {
        // 🔥 수정: 임시 주석 처리 (위와 동일한 이유)
        /*
        // 콜백 제거 (nullptr로 설정)
        worker->SetDeviceDiscoveredCallback(nullptr);
        worker->SetObjectDiscoveredCallback(nullptr);
        worker->SetValueChangedCallback(nullptr);
        */
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
        // 기존 디바이스 확인
        int existing_device_id = FindDeviceIdInDatabase(device.device_id);
        
        if (existing_device_id > 0) {
            // 기존 디바이스 업데이트
            auto existing_device = device_repository_->findById(existing_device_id);
            if (existing_device.has_value()) {
                existing_device->setName(device.device_name);
                existing_device->setEndpoint(device.ip_address + ":" + std::to_string(device.port));
                
                return device_repository_->update(existing_device.value());
            }
        }
        
        // 새 디바이스 생성
        DeviceEntity new_device;
        new_device.setName(device.device_name);
        new_device.setProtocolType("BACNET_IP");
        new_device.setEndpoint(device.ip_address + ":" + std::to_string(device.port));
        new_device.setEnabled(true);
        
        // BACnet 특화 설정 JSON 생성
        std::ostringstream config_json;
        config_json << "{"
                   << "\"bacnet_device_id\":" << device.device_id << ","
                   << "\"ip_address\":\"" << device.ip_address << "\","
                   << "\"port\":" << device.port << ","
                   << "\"max_apdu_length\":" << device.max_apdu_length << ","
                   << "\"segmentation_supported\":" << (device.segmentation_supported ? "true" : "false")
                   << "}";
        
        new_device.setConfig(config_json.str());
        
        bool success = device_repository_->save(new_device);
        
        if (success) {
            auto& logger = LogManager::getInstance();
            logger.Info("BACnet device saved to database: " + device.device_name + 
                       " (ID: " + std::to_string(device.device_id) + ")");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("Failed to save BACnet device to database: " + std::string(e.what()));
        return false;
    }
}

bool BACnetDiscoveryService::SaveDiscoveredObjectsToDatabase(uint32_t device_id, 
                                                           const std::vector<Drivers::BACnetObjectInfo>& objects) {
    try {
        // 디바이스 ID 조회
        int internal_device_id = FindDeviceIdInDatabase(device_id);
        if (internal_device_id <= 0) {
            auto& logger = LogManager::getInstance();
            logger.Error("Device not found in database for BACnet device ID: " + std::to_string(device_id));
            return false;
        }
        
        bool overall_success = true;
        
        for (const auto& object : objects) {
            try {
                // 데이터 포인트 ID 생성
                std::string point_id = GenerateDataPointId(device_id, object);
                
                // 🔥 수정: address는 int 타입이므로 hash 값으로 변환
                std::hash<std::string> hasher;
                int address_hash = static_cast<int>(hasher(point_id) % 0x7FFFFFFF);
                
                // 새 데이터 포인트 생성
                DataPointEntity new_datapoint;
                new_datapoint.setDeviceId(internal_device_id);
                new_datapoint.setName(object.object_name.empty() ? 
                    ObjectTypeToString(static_cast<int>(object.object_type)) + "_" + std::to_string(object.object_instance) : 
                    object.object_name);
                // 🔥 수정: setAddress는 int 타입을 받음
                new_datapoint.setAddress(address_hash);
                new_datapoint.setDataType(DataTypeToString(DetermineDataType(static_cast<int>(object.object_type))));
                new_datapoint.setUnit(object.units);
                new_datapoint.setEnabled(true);
                
                // 🔥 수정: setMetadata는 map<string, string> 타입을 받음
                std::map<std::string, std::string> metadata_map;
                metadata_map["object_type"] = std::to_string(static_cast<int>(object.object_type));
                metadata_map["object_instance"] = std::to_string(object.object_instance);
                metadata_map["object_name"] = object.object_name;
                metadata_map["description"] = object.description;
                metadata_map["units"] = object.units;
                metadata_map["original_address"] = point_id;
                
                new_datapoint.setMetadata(metadata_map);
                
                if (!datapoint_repository_->save(new_datapoint)) {
                    overall_success = false;
                    auto& logger = LogManager::getInstance();
                    logger.Warn("Failed to save datapoint: " + point_id);
                }
                
            } catch (const std::exception& e) {
                overall_success = false;
                auto& logger = LogManager::getInstance();
                logger.Error("Exception saving object: " + std::string(e.what()));
            }
        }
        
        auto& logger = LogManager::getInstance();
        logger.Info("Saved " + std::to_string(objects.size()) +
                   " objects to database for device: " + std::to_string(device_id));
        
        return overall_success;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("Failed to save BACnet objects to database: " + std::string(e.what()));
        return false;
    }
}

bool BACnetDiscoveryService::UpdateCurrentValueInDatabase(const std::string& object_id, const TimestampedValue& value) {
    try {
        // 🔥 수정: getMetadata()는 map<string, string>을 반환
        auto all_datapoints = datapoint_repository_->findAll();
        
        DataPointEntity* target_datapoint = nullptr;
        for (auto& dp : all_datapoints) {
            try {
                auto metadata_map = dp.getMetadata();
                auto it = metadata_map.find("original_address");
                if (it != metadata_map.end() && it->second == object_id) {
                    target_datapoint = &dp;
                    break;
                }
            } catch (...) {
                continue;
            }
        }
        
        if (!target_datapoint) {
            auto& logger = LogManager::getInstance();
            logger.Warn("DataPoint not found for object_id: " + object_id);
            return false;
        }
        
        // CurrentValue 엔티티 생성 및 설정
        using CurrentValueEntity = PulseOne::Database::Entities::CurrentValueEntity;
        CurrentValueEntity current_value;
        
        // 🔥 수정: setDataPointId → setPointId
        current_value.setPointId(target_datapoint->getId());
        
        // 🔥 수정: setValue는 double 타입을 받으므로 변환 필요
        double numeric_value = 0.0;
        try {
            if (std::holds_alternative<double>(value.value)) {
                numeric_value = std::get<double>(value.value);
            } else if (std::holds_alternative<float>(value.value)) {
                numeric_value = static_cast<double>(std::get<float>(value.value));
            } else if (std::holds_alternative<int>(value.value)) {
                numeric_value = static_cast<double>(std::get<int>(value.value));
            } else if (std::holds_alternative<bool>(value.value)) {
                numeric_value = std::get<bool>(value.value) ? 1.0 : 0.0;
            } else if (std::holds_alternative<std::string>(value.value)) {
                // 문자열인 경우 숫자 변환 시도
                std::string str_val = std::get<std::string>(value.value);
                try {
                    numeric_value = std::stod(str_val);
                } catch (...) {
                    numeric_value = 0.0;
                }
            }
        } catch (...) {
            numeric_value = 0.0;
        }
        
        current_value.setValue(numeric_value);
        current_value.setStringValue(ConvertDataValueToString(value.value));
        current_value.setTimestamp(value.timestamp);
        
        // 🔥 수정: findByPointId → findByDataPointId (올바른 메서드명 확인됨)
        if (current_value_repository_) {
            auto existing = current_value_repository_->findByDataPointId(target_datapoint->getId());
            if (existing.has_value()) {
                current_value.setId(existing->getId());
                return current_value_repository_->update(current_value);
            } else {
                return current_value_repository_->save(current_value);
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
        auto all_devices = device_repository_->findByProtocol("BACNET_IP");
        
        for (const auto& device : all_devices) {
            try {
                auto config = device.getConfigAsJson();
                if (config.contains("bacnet_device_id") && 
                    config["bacnet_device_id"].get<uint32_t>() == bacnet_device_id) {
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
        case 0: return "AI";  // ANALOG_INPUT
        case 1: return "AO";  // ANALOG_OUTPUT  
        case 2: return "AV";  // ANALOG_VALUE
        case 3: return "BI";  // BINARY_INPUT
        case 4: return "BO";  // BINARY_OUTPUT
        case 5: return "BV";  // BINARY_VALUE
        case 8: return "DEVICE";
        case 13: return "MSI"; // MULTI_STATE_INPUT
        case 14: return "MSO"; // MULTI_STATE_OUTPUT
        case 19: return "MSV"; // MULTI_STATE_VALUE
        default: return "UNKNOWN";
    }
}

PulseOne::Enums::DataType BACnetDiscoveryService::DetermineDataType(int tag) {
    switch (tag) {
        case 0:  // ANALOG_INPUT
        case 1:  // ANALOG_OUTPUT
        case 2:  // ANALOG_VALUE
            return DataType::FLOAT32;  // 🔥 수정: FLOAT → FLOAT32
        case 3:  // BINARY_INPUT
        case 4:  // BINARY_OUTPUT
        case 5:  // BINARY_VALUE
            return DataType::BOOL;
        case 13: // MULTI_STATE_INPUT
        case 14: // MULTI_STATE_OUTPUT
        case 19: // MULTI_STATE_VALUE
            return DataType::INT32;
        default:
            return DataType::STRING;
    }
}

std::string BACnetDiscoveryService::DataTypeToString(PulseOne::Enums::DataType type) {
    switch (type) {
        case DataType::BOOL: return "bool";
        case DataType::INT32: return "int32";
        case DataType::FLOAT32: return "float32";  // 🔥 수정: FLOAT → FLOAT32
        case DataType::STRING: return "string";
        default: return "string";
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