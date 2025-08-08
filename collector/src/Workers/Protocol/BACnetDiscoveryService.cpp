/**
 * @file BACnetDiscoveryService.cpp
 * @brief BACnet 발견 서비스 구현 - 🔥 실제 프로젝트 구조 100% 반영
 * @author PulseOne Development Team
 * @date 2025-08-08
 */

#include "Workers/Protocol/BACnetDiscoveryService.h"
#include "Workers/Protocol/BACnetWorker.h"  // 🔥 중요: cpp에서만 include
#include "Utils/LogManager.h"
#include "Common/Enums.h"
#include "Database/DatabaseManager.h"
#include "Database/Entities/DeviceEntity.h"
#include "Database/Entities/DataPointEntity.h"
#include "Database/Entities/CurrentValueEntity.h"
#include "Database/DatabaseTypes.h"
#include <nlohmann/json.hpp>  // 🔥 추가: JSON 사용
#include <sstream>
#include <functional>
#include <regex>

using json = nlohmann::json;  // 🔥 추가: JSON 타입 별칭

using namespace std::chrono;

// 🔥 타입 별칭들 - 모든 타입 실제 구조 사용
using DeviceInfo = PulseOne::Structs::DeviceInfo;
using DataPoint = PulseOne::Structs::DataPoint;
using TimestampedValue = PulseOne::Structs::TimestampedValue;
using DataValue = PulseOne::Structs::DataValue;
using DeviceEntity = PulseOne::Database::Entities::DeviceEntity;
using DataPointEntity = PulseOne::Database::Entities::DataPointEntity;
using CurrentValueEntity = PulseOne::Database::Entities::CurrentValueEntity;
using DataType = PulseOne::Enums::DataType;
using DataQuality = PulseOne::Enums::DataQuality;
using QueryCondition = PulseOne::Database::QueryCondition;
using LogLevel = PulseOne::Enums::LogLevel;

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
    
    // 🔥 이제 BACnetWorker가 완전히 정의되어 있으므로 메서드 호출 가능
    worker->SetObjectDiscoveredCallback([this](const DataPoint& object) {
        OnObjectDiscovered(object);
    });
    
    worker->SetValueChangedCallback([this](const std::string& object_id, const TimestampedValue& value) {
        OnValueChanged(object_id, value);
    });
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnetDiscoveryService registered to worker successfully");
    return true;
}

void BACnetDiscoveryService::UnregisterFromWorker() {
    if (auto worker = registered_worker_.lock()) {
        // 콜백 제거 (nullptr로 설정)
        worker->SetObjectDiscoveredCallback(nullptr);
        worker->SetValueChangedCallback(nullptr);
    }
    
    registered_worker_.reset();
    is_active_ = false;
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnetDiscoveryService unregistered from worker");
}

BACnetDiscoveryService::Statistics BACnetDiscoveryService::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

bool BACnetDiscoveryService::IsActive() const {
    return is_active_.load();
}

void BACnetDiscoveryService::ResetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    statistics_ = Statistics{};
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnetDiscoveryService statistics reset");
}

// =============================================================================
// 🔥 콜백 핸들러들 - 모든 타입 통일
// =============================================================================

void BACnetDiscoveryService::OnDeviceDiscovered(const DeviceInfo& device) {
    try {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.devices_processed++;
        statistics_.last_activity = system_clock::now();
        
        auto& logger = LogManager::getInstance();
        logger.Debug("🔄 BACnet device discovered: " + device.name + 
                     " (ID: " + device.id + ")");
        
        // 데이터베이스에 저장
        if (SaveDiscoveredDeviceToDatabase(device)) {
            statistics_.devices_saved++;
            logger.Info("✅ Device saved to database: " + device.name);
        } else {
            statistics_.database_errors++;
            logger.Error("❌ Failed to save device to database: " + device.name);
        }
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.database_errors++;
        
        auto& logger = LogManager::getInstance();
        logger.Error("Exception in OnDeviceDiscovered: " + std::string(e.what()));
    }
}

void BACnetDiscoveryService::OnObjectDiscovered(const DataPoint& object) {
    try {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.objects_processed++;
        statistics_.last_activity = system_clock::now();
        
        auto& logger = LogManager::getInstance();
        logger.Debug("🔄 BACnet object discovered: " + object.name + " (ID: " + object.id + ")");
        
        // 단일 객체를 벡터로 변환하여 기존 메서드 호출
        std::vector<DataPoint> objects = {object};
        uint32_t device_id = 0;
        try {
            device_id = std::stoul(object.device_id);
        } catch (...) {
            device_id = 260001;  // 기본값
        }
        
        // 데이터베이스에 저장
        if (SaveDiscoveredObjectsToDatabase(device_id, objects)) {
            statistics_.objects_saved++;
            logger.Info("✅ Object saved to database: " + object.name);
        } else {
            statistics_.database_errors++;
            logger.Error("❌ Failed to save object to database: " + object.name);
        }
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.database_errors++;
        
        auto& logger = LogManager::getInstance();
        logger.Error("Exception in OnObjectDiscovered (single): " + std::string(e.what()));
    }
}

void BACnetDiscoveryService::OnObjectDiscovered(uint32_t device_id, const std::vector<DataPoint>& objects) {
    try {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.objects_processed += objects.size();
        statistics_.last_activity = system_clock::now();
        
        auto& logger = LogManager::getInstance();
        logger.Debug("🔄 BACnet objects discovered for device " + std::to_string(device_id) + 
                     ": " + std::to_string(objects.size()) + " objects");
        
        // 데이터베이스에 저장
        if (SaveDiscoveredObjectsToDatabase(device_id, objects)) {
            statistics_.objects_saved += objects.size();
            logger.Info("✅ Objects saved to database for device " + std::to_string(device_id));
        } else {
            statistics_.database_errors++;
            logger.Error("❌ Failed to save objects to database for device " + std::to_string(device_id));
        }
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.database_errors++;
        
        auto& logger = LogManager::getInstance();
        logger.Error("Exception in OnObjectDiscovered (multiple): " + std::string(e.what()));
    }
}

void BACnetDiscoveryService::OnValueChanged(const std::string& object_id, const TimestampedValue& value) {
    try {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.values_processed++;
        statistics_.last_activity = system_clock::now();
        
        auto& logger = LogManager::getInstance();
        logger.Debug("🔄 BACnet value changed for object: " + object_id);
        
        // 현재값 데이터베이스에 업데이트
        if (UpdateCurrentValueInDatabase(object_id, value)) {
            statistics_.values_saved++;
        } else {
            statistics_.database_errors++;
            logger.Warn("❌ Failed to update current value for object: " + object_id);
        }
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.database_errors++;
        
        auto& logger = LogManager::getInstance();
        logger.Error("Exception in OnValueChanged: " + std::string(e.what()));
    }
}

// =============================================================================
// 🔥 데이터베이스 저장 메서드들 - 실제 메서드명 사용
// =============================================================================

bool BACnetDiscoveryService::SaveDiscoveredDeviceToDatabase(const DeviceInfo& device) {
    try {
        // 🔥 실제 메서드명: findAll() (not GetAll)
        auto existing_devices = device_repository_->findAll();
        for (const auto& existing : existing_devices) {
            if (existing.getId() == std::stoi(device.id)) {  // ✅ int 변환
                auto& logger = LogManager::getInstance();
                logger.Debug("Device already exists in database: " + device.id);
                return true;  // 이미 존재하므로 성공으로 처리
            }
        }
        
        // 새 디바이스 엔티티 생성
        DeviceEntity entity;
        entity.setId(std::stoi(device.id));  // ✅ string → int 변환
        entity.setName(device.name);
        entity.setDescription(device.description);
        entity.setProtocolType(device.protocol_type);
        entity.setEndpoint(device.endpoint);
        entity.setEnabled(device.is_enabled);
        // ❌ setTimeout, setRetryCount, setPollingInterval 메서드 없음 - 제거
        
        // properties를 JSON으로 직렬화
        std::stringstream properties_json;
        properties_json << "{";
        bool first = true;
        for (const auto& [key, value] : device.properties) {
            if (!first) properties_json << ",";
            properties_json << "\"" << key << "\":\"" << value << "\"";
            first = false;
        }
        properties_json << "}";
        entity.setConfig(properties_json.str());
        
        // 🔥 실제 메서드명: save() (not Create)
        return device_repository_->save(entity);
        
    } catch (const std::exception& e) {
        HandleError("SaveDiscoveredDeviceToDatabase", e.what());
        return false;
    }
}

bool BACnetDiscoveryService::SaveDiscoveredObjectsToDatabase(uint32_t device_id, const std::vector<DataPoint>& objects) {
    try {
        // ✅ unused parameter 경고 해결
        (void)device_id;  // 명시적으로 사용하지 않음을 표시
        
        bool all_success = true;
        
        for (const auto& object : objects) {
            // 🔥 실제 메서드명: findAll() (not GetAll)
            auto existing_points = datapoint_repository_->findAll();
            bool exists = false;
            for (const auto& existing : existing_points) {
                if (existing.getId() == std::stoi(object.id)) {  // ✅ string → int 변환
                    exists = true;
                    break;
                }
            }
            
            if (exists) {
                continue;  // 이미 존재하므로 건너뛰기
            }
            
            // 새 데이터포인트 엔티티 생성
            DataPointEntity entity;
            entity.setId(std::stoi(object.id));  // ✅ string → int 변환
            entity.setDeviceId(std::stoi(object.device_id));  // ✅ string → int 변환
            entity.setName(object.name);
            entity.setDescription(object.description);
            entity.setAddress(object.address);
            entity.setDataType(object.data_type);
            entity.setUnit(object.unit);
            entity.setEnabled(object.is_enabled);
            
            // ✅ protocol_params를 맵으로 설정 (not JSON string)
            entity.setProtocolParams(object.protocol_params);
            
            // 🔥 실제 메서드명: save() (not Create)
            if (!datapoint_repository_->save(entity)) {
                all_success = false;
            }
        }
        
        return all_success;
        
    } catch (const std::exception& e) {
        HandleError("SaveDiscoveredObjectsToDatabase", e.what());
        return false;
    }
}

bool BACnetDiscoveryService::UpdateCurrentValueInDatabase(const std::string& object_id, const TimestampedValue& value) {
    try {
        if (!current_value_repository_) {
            return false;  // CurrentValue 저장소가 없으면 건너뛰기
        }
        
        // CurrentValue 엔티티 생성/업데이트
        CurrentValueEntity entity;
        entity.setId(std::stoi(object_id));  // ✅ string → int 변환
        // ✅ 실제 메서드명: setPointId() (not setDataPointId)
        entity.setPointId(std::stoi(object_id));
        
        // ✅ 실제 존재하는 메서드 사용 (setValue, setTimestamp는 구현되지 않음)
        // double 값을 JSON 문자열로 설정
        double value_double = ConvertDataValueToDouble(value.value);
        json value_json;
        value_json["value"] = value_double;
        entity.setCurrentValue(value_json.dump());
        
        // ✅ setQuality는 DataQuality enum 요구
        entity.setQuality(value.quality);
        
        // ✅ 실제 존재하는 메서드: setValueTimestamp (not setTimestamp)
        entity.setValueTimestamp(value.timestamp);
        
        // 🔥 실제 조건 생성 방법
        std::vector<QueryCondition> conditions = {
            QueryCondition("point_id", "=", object_id)
        };
        
        // 🔥 실제 메서드명: findByConditions() (not FindByCondition)
        auto existing_values = current_value_repository_->findByConditions(conditions);
        
        if (existing_values.empty()) {
            // 🔥 실제 메서드명: save() (not Create)
            return current_value_repository_->save(entity);
        } else {
            entity.setId(existing_values[0].getId());
            // 🔥 실제 메서드명: update() (not Update)
            return current_value_repository_->update(entity);
        }
        
    } catch (const std::exception& e) {
        HandleError("UpdateCurrentValueInDatabase", e.what());
        return false;
    }
}

// =============================================================================
// 유틸리티 함수들
// =============================================================================

int BACnetDiscoveryService::FindDeviceIdInDatabase(uint32_t bacnet_device_id) {
    try {
        std::vector<QueryCondition> conditions = {
            QueryCondition("config", "LIKE", "%\"bacnet_device_id\":\"" + std::to_string(bacnet_device_id) + "\"%")
        };
        
        // 🔥 실제 메서드명: findByConditions() (not FindByCondition)
        auto devices = device_repository_->findByConditions(conditions);
        
        if (!devices.empty()) {
            return devices[0].getId();
        }
        
        return -1;  // 찾을 수 없음
        
    } catch (const std::exception& e) {
        HandleError("FindDeviceIdInDatabase", e.what());
        return -1;
    }
}

std::string BACnetDiscoveryService::GenerateDataPointId(uint32_t device_id, const DataPoint& object) {
    // BACnet 객체 정보로 고유 ID 생성
    auto obj_type_it = object.protocol_params.find("bacnet_object_type");
    auto obj_inst_it = object.protocol_params.find("bacnet_instance");
    
    if (obj_type_it != object.protocol_params.end() && 
        obj_inst_it != object.protocol_params.end()) {
        return std::to_string(device_id) + ":" + 
               obj_type_it->second + ":" + 
               obj_inst_it->second;
    }
    
    // 기본 ID 생성
    return std::to_string(device_id) + ":" + std::to_string(object.address);
}

std::string BACnetDiscoveryService::ObjectTypeToString(int type) {
    switch (type) {
        case 0: return "ANALOG_INPUT";
        case 1: return "ANALOG_OUTPUT";
        case 2: return "ANALOG_VALUE";
        case 3: return "BINARY_INPUT";
        case 4: return "BINARY_OUTPUT";
        case 5: return "BINARY_VALUE";
        case 8: return "DEVICE";
        case 13: return "MULTI_STATE_INPUT";
        case 14: return "MULTI_STATE_OUTPUT";
        case 19: return "MULTI_STATE_VALUE";
        default: return "UNKNOWN_" + std::to_string(type);
    }
}

PulseOne::Enums::DataType BACnetDiscoveryService::DetermineDataType(int type) {
    switch (type) {
        case 0: case 1: case 2:   // AI, AO, AV
            return DataType::FLOAT32;  // ✅ 실제 enum 값
        case 3: case 4: case 5:   // BI, BO, BV
            return DataType::BOOL;     // ✅ 실제 enum 값
        case 13: case 14: case 19:  // MI, MO, MV
            return DataType::INT32;    // ✅ 실제 enum 값
        default:
            return DataType::STRING;
    }
}

std::string BACnetDiscoveryService::DataTypeToString(PulseOne::Enums::DataType type) {
    switch (type) {
        case DataType::BOOL: return "bool";        // ✅ 실제 enum 값
        case DataType::INT32: return "int";        // ✅ 실제 enum 값
        case DataType::FLOAT32: return "float";    // ✅ 실제 enum 값
        case DataType::STRING: return "string";
        default: return "unknown";
    }
}

std::string BACnetDiscoveryService::ConvertDataValueToString(const DataValue& value) {
    // ✅ DataValue는 variant<bool, int16_t, uint16_t, int32_t, uint32_t, float, double, string>
    try {
        if (std::holds_alternative<bool>(value)) {
            return std::get<bool>(value) ? "true" : "false";
        } else if (std::holds_alternative<int16_t>(value)) {
            return std::to_string(std::get<int16_t>(value));
        } else if (std::holds_alternative<uint16_t>(value)) {
            return std::to_string(std::get<uint16_t>(value));
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
        }
        return "unknown";
    } catch (const std::exception&) {
        return "unknown";
    }
}

double BACnetDiscoveryService::ConvertDataValueToDouble(const DataValue& value) {
    // ✅ DataValue는 variant<bool, int16_t, uint16_t, int32_t, uint32_t, float, double, string>
    try {
        if (std::holds_alternative<bool>(value)) {
            return std::get<bool>(value) ? 1.0 : 0.0;
        } else if (std::holds_alternative<int16_t>(value)) {
            return static_cast<double>(std::get<int16_t>(value));
        } else if (std::holds_alternative<uint16_t>(value)) {
            return static_cast<double>(std::get<uint16_t>(value));
        } else if (std::holds_alternative<int32_t>(value)) {
            return static_cast<double>(std::get<int32_t>(value));
        } else if (std::holds_alternative<uint32_t>(value)) {
            return static_cast<double>(std::get<uint32_t>(value));
        } else if (std::holds_alternative<float>(value)) {
            return static_cast<double>(std::get<float>(value));
        } else if (std::holds_alternative<double>(value)) {
            return std::get<double>(value);
        } else if (std::holds_alternative<std::string>(value)) {
            try {
                return std::stod(std::get<std::string>(value));
            } catch (...) {
                return 0.0;
            }
        }
        return 0.0;
    } catch (const std::exception&) {
        return 0.0;
    }
}

void BACnetDiscoveryService::HandleError(const std::string& context, const std::string& error) {
    auto& logger = LogManager::getInstance();
    logger.Error("BACnetDiscoveryService::" + context + " - " + error);
}

int BACnetDiscoveryService::GuessObjectTypeFromName(const std::string& object_name) {
    // 정규 표현식으로 객체 타입 추정
    std::regex ai_pattern("^AI\\d+", std::regex_constants::icase);
    std::regex ao_pattern("^AO\\d+", std::regex_constants::icase);
    std::regex av_pattern("^AV\\d+", std::regex_constants::icase);
    std::regex bi_pattern("^BI\\d+", std::regex_constants::icase);
    std::regex bo_pattern("^BO\\d+", std::regex_constants::icase);
    std::regex bv_pattern("^BV\\d+", std::regex_constants::icase);
    std::regex mi_pattern("^MI\\d+", std::regex_constants::icase);
    std::regex mo_pattern("^MO\\d+", std::regex_constants::icase);
    std::regex mv_pattern("^MV\\d+", std::regex_constants::icase);
    
    if (std::regex_search(object_name, ai_pattern)) return 0;   // AI
    if (std::regex_search(object_name, ao_pattern)) return 1;   // AO
    if (std::regex_search(object_name, av_pattern)) return 2;   // AV
    if (std::regex_search(object_name, bi_pattern)) return 3;   // BI
    if (std::regex_search(object_name, bo_pattern)) return 4;   // BO
    if (std::regex_search(object_name, bv_pattern)) return 5;   // BV
    if (std::regex_search(object_name, mi_pattern)) return 13;  // MI
    if (std::regex_search(object_name, mo_pattern)) return 14;  // MO
    if (std::regex_search(object_name, mv_pattern)) return 19;  // MV
    
    return 2;  // 기본값: ANALOG_VALUE
}

std::string BACnetDiscoveryService::BACnetAddressToString(const BACNET_ADDRESS& address) {
    // BACNET_ADDRESS를 IP 문자열로 변환
    std::stringstream ss;
    
    if (address.net == 0) {
        // 로컬 네트워크
        for (int i = 0; i < address.len && i < 6; i++) {
            if (i > 0) ss << ".";
            ss << static_cast<int>(address.adr[i]);
        }
    } else {
        // 원격 네트워크
        ss << "Network:" << address.net << ",Address:";
        for (int i = 0; i < address.len && i < 6; i++) {
            if (i > 0) ss << ".";
            ss << static_cast<int>(address.adr[i]);
        }
    }
    
    return ss.str();
}

} // namespace Workers
} // namespace PulseOne