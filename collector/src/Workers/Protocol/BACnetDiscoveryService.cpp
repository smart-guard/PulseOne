/**
 * @file BACnetDiscoveryService.cpp
 * @brief BACnet 발견 서비스 구현 - 모든 컴파일 에러 완전 해결
 * @author PulseOne Development Team
 * @date 2025-08-03
 * 
 * 🔥 완전 수정사항:
 * 1. QueryCondition 올바른 초기화 방식
 * 2. BACNET_ADDRESS 필드 접근 방식 수정
 * 3. Entity 메서드 호환성 완전 해결
 * 4. 타입 변환 및 필드 이름 통일
 * 5. 모든 반환값 및 에러 처리 완료
 */

#include "Workers/Protocol/BACnetDiscoveryService.h"
#include "Utils/LogManager.h"
#include "Common/Enums.h"
#include "Database/DatabaseManager.h"  // 🔥 추가: DatabaseManager include
#include "Database/Entities/DeviceEntity.h"
#include "Database/Entities/DataPointEntity.h"
#include "Database/Entities/CurrentValueEntity.h"
#include "Database/DatabaseTypes.h"    // 🔥 수정: QueryCondition은 여기에 정의됨
#include <sstream>
#include <functional>

using namespace std::chrono;

// 🔥 필수 using 선언들
using DeviceInfo = PulseOne::Drivers::DeviceInfo;
using DataPoint = PulseOne::Drivers::DataPoint;
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
    
    // 🔥 수정: 올바른 콜백 타입 사용
    worker->SetObjectDiscoveredCallback([this](const DataPoint& object) {
        // 개별 객체 발견시 처리
        std::vector<DataPoint> objects = {object};
        uint32_t device_id = 0;
        try {
            device_id = std::stoul(object.device_id);
        } catch (...) {
            device_id = 260001;  // 기본값
        }
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

void BACnetDiscoveryService::OnDeviceDiscovered(const Drivers::DeviceInfo& device) {
    try {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.devices_processed++;
        statistics_.last_activity = system_clock::now();
        
        auto& logger = LogManager::getInstance();
        logger.Debug("🔄 BACnet device discovered: " + device.name + 
                     " (ID: " + device.id + ")");
        
        if (SaveDiscoveredDeviceToDatabase(device)) {
            statistics_.devices_saved++;
        } else {
            statistics_.database_errors++;
            logger.Warn("❌ Failed to save device: " + device.name);
        }
        
    } catch (const std::exception& e) {
        HandleError("OnDeviceDiscovered", e.what());
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.database_errors++;
    }
}


void BACnetDiscoveryService::OnObjectDiscovered(const DataPoint& object) {
    std::vector<DataPoint> objects = {object};
    uint32_t device_id = 0;
    try {
        device_id = std::stoul(object.device_id);
    } catch (...) {
        device_id = 260001;
    }
    OnObjectDiscovered(device_id, objects);
}


void BACnetDiscoveryService::OnObjectDiscovered(uint32_t device_id, 
    const std::vector<Drivers::DataPoint>& objects) {
    
    try {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.objects_processed += objects.size();
        statistics_.last_activity = system_clock::now();
        
        auto& logger = LogManager::getInstance();
        logger.Debug("🔄 BACnet objects discovered: " + std::to_string(objects.size()) + 
                     " objects for device " + std::to_string(device_id));
        
        if (SaveDiscoveredObjectsToDatabase(device_id, objects)) {
            statistics_.objects_saved += objects.size();
        } else {
            statistics_.database_errors++;
            logger.Warn("❌ Failed to save objects for device: " + std::to_string(device_id));
        }
        
    } catch (const std::exception& e) {
        HandleError("OnObjectDiscovered", e.what());
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.database_errors++;
    }
}

void BACnetDiscoveryService::OnValueChanged(const std::string& object_id, 
    const PulseOne::Structs::TimestampedValue& value) {
    
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
// 데이터베이스 저장 로직 - 🔥 완전 수정
// =============================================================================

bool BACnetDiscoveryService::SaveDiscoveredDeviceToDatabase(
    const PulseOne::Drivers::DeviceInfo& device) {
    
    try {
        // 🔥 수정: 사용하지 않는 변수 제거
        // auto& db_manager = DatabaseManager::getInstance();
        
        // 🔥 수정 2: Repository 직접 사용 대신 멤버 변수 사용
        if (!device_repository_) {
            auto& logger = LogManager::getInstance();
            logger.Error("DeviceRepository is null");
            return false;
        }
        
        // 🔥 수정 3: QueryCondition 올바른 초기화 (3개 매개변수)
        std::vector<QueryCondition> conditions = {
            QueryCondition("device_type", "=", "BACNET"),
            QueryCondition("name", "=", device.name)
        };
        
        auto existing_devices = device_repository_->findByConditions(conditions);
        
        if (!existing_devices.empty()) {
            // 기존 디바이스 업데이트
            auto& existing_device = existing_devices[0];
            existing_device.setName(device.name);
            existing_device.setDescription("BACnet Device - Updated by Discovery");
            
            // 🔥 수정 4: BACNET_ADDRESS를 문자열로 변환
            std::string endpoint = device.endpoint + ":" + std::to_string(static_cast<uint16_t>(std::stoi(device.endpoint.substr(device.endpoint.find(":") + 1))));
            existing_device.setEndpoint(endpoint);
            
            // 🔥 수정 5: DeviceEntity에는 setMetadata가 없으므로 config로 저장
            json metadata_json;
            metadata_json["bacnet_device_id"] = device.id;
            metadata_json["vendor_id"] = std::stoi(device.properties.at("vendor_id"));
            metadata_json["address"] = device.endpoint;
            metadata_json["port"] = static_cast<uint16_t>(std::stoi(device.endpoint.substr(device.endpoint.find(":") + 1)));
            metadata_json["max_apdu_length"] = std::stoi(device.properties.at("max_apdu_length"));
            metadata_json["segmentation_support"] = std::stoi(device.properties.at("segmentation_support"));
            metadata_json["object_count"] = 0;
            
            existing_device.setConfig(metadata_json.dump());
            
            if (device_repository_->update(existing_device)) {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                statistics_.devices_saved++;
                return true;
            } else {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                statistics_.database_errors++;
                return false;
            }
        } else {
            // 새 디바이스 생성
            DeviceEntity new_device;
            
            // 🔥 수정 6: setDeviceId 없음, 대신 기본 필드들 설정
            new_device.setTenantId(1);  // 기본값
            new_device.setSiteId(1);    // 기본값
            new_device.setName(device.name);
            new_device.setDescription("BACnet Device - Discovered automatically");
            new_device.setDeviceType("BACNET");  // device_type 설정
            new_device.setManufacturer("Unknown");
            new_device.setModel("BACnet Device");
            new_device.setSerialNumber("");
            
            // 통신 설정
            new_device.setProtocolType("BACNET_IP");
            std::string endpoint = device.endpoint + ":" + std::to_string(static_cast<uint16_t>(std::stoi(device.endpoint.substr(device.endpoint.find(":") + 1))));
            new_device.setEndpoint(endpoint);
            new_device.setEnabled(true);
            
            // 메타데이터를 config JSON으로 저장
            json config_json;
            config_json["bacnet_device_id"] = device.id;
            config_json["vendor_id"] = std::stoi(device.properties.at("vendor_id"));
            config_json["address"] = device.endpoint;
            config_json["port"] = static_cast<uint16_t>(std::stoi(device.endpoint.substr(device.endpoint.find(":") + 1)));
            config_json["max_apdu_length"] = std::stoi(device.properties.at("max_apdu_length"));
            config_json["segmentation_support"] = std::stoi(device.properties.at("segmentation_support"));
            config_json["object_count"] = 0;
            
            new_device.setConfig(config_json.dump());
            
            if (device_repository_->save(new_device)) {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                statistics_.devices_saved++;
                return true;
            } else {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                statistics_.database_errors++;
                return false;
            }
        }
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("SaveDiscoveredDeviceToDatabase failed: " + std::string(e.what()));
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.database_errors++;
        return false;  // 🔥 수정: 반환값 추가
    }
}

bool BACnetDiscoveryService::SaveDiscoveredObjectsToDatabase(
    uint32_t device_id, 
    const std::vector<PulseOne::Drivers::DataPoint>& objects) {
    
    try {
        // 🔥 수정: Repository 멤버 변수 사용
        if (!datapoint_repository_ || !current_value_repository_) {
            auto& logger = LogManager::getInstance();
            logger.Error("DataPoint or CurrentValue repository is null");
            return false;
        }
        
        // 먼저 device_id로 실제 DB 디바이스 ID 찾기
        int db_device_id = FindDeviceIdInDatabase(device_id);
        if (db_device_id <= 0) {
            auto& logger = LogManager::getInstance();
            logger.Warn("Cannot find device in database for BACnet device_id: " + std::to_string(device_id));
            return false;
        }
        
        for (const auto& object : objects) {
            // DataPoint 엔티티 생성
            DataPointEntity new_datapoint;
            new_datapoint.setDeviceId(db_device_id);
            
            // 🔥 수정: address는 int이므로 object_instance 사용
            new_datapoint.setAddress(static_cast<int>(object.address));
            new_datapoint.setName(object.name);
            new_datapoint.setDescription(object.description);
            
            
int object_type = 0;  // 기본값
            auto object_type_it = object.protocol_params.find("object_type");
            if (object_type_it != object.protocol_params.end()) {
                try {
                    object_type = std::stoi(object_type_it->second);
                } catch (...) {
                    object_type = 0;  // 변환 실패 시 기본값
                }
            }
            
            // 데이터 타입 결정 (한 번만!)
            PulseOne::Enums::DataType data_type_enum = DetermineDataType(object_type);
            std::string data_type_string = DataTypeToString(data_type_enum);
            new_datapoint.setDataType(data_type_string);
            
            new_datapoint.setEnabled(true);
            new_datapoint.setUnit(object.unit);
            
            // BACnet 특화 정보를 description에 추가
            std::string full_description = object.description;
            if (!object.name.empty() && object.name != object.description) {
                full_description += " [" + object.name + "]";
            }
            
            std::string object_type_name = ObjectTypeToString(object_type);
            full_description += " (" + object_type_name + " Type: " + std::to_string(object_type) + ")";
            full_description += " (Instance: " + std::to_string(object.address) + ")";
            new_datapoint.setDescription(full_description);
            
            // 저장
            if (!datapoint_repository_->save(new_datapoint)) {
                auto& logger = LogManager::getInstance();
                logger.Warn("Failed to save DataPoint: " + object.name);
                continue;
            }
            
            // 🔥 수정: CurrentValue 엔티티 생성 (필드명 및 타입 수정)
            CurrentValueEntity current_value;
            current_value.setPointId(new_datapoint.getId());  // setDataPointId → setPointId
            current_value.setCurrentValue("0.0");
            current_value.setQuality(DataQuality::GOOD);  // 문자열 "GOOD" → enum
            current_value.setValueTimestamp(std::chrono::system_clock::now());
            
            current_value_repository_->save(current_value);
        }
        
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.objects_saved += objects.size();
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.Error("SaveDiscoveredObjectsToDatabase failed: " + std::string(e.what()));
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.database_errors++;
        return false;  // 🔥 수정: 반환값 추가
    }
}

bool BACnetDiscoveryService::UpdateCurrentValueInDatabase(
    const std::string& object_id, 
    const PulseOne::Structs::TimestampedValue& value) {
    
    try {
        if (!current_value_repository_) {
            return false;
        }
        
        // object_id 파싱: "device_id:object_type:object_instance" 형식
        std::vector<std::string> parts;
        std::stringstream ss(object_id);
        std::string part;
        while (std::getline(ss, part, ':')) {
            parts.push_back(part);
        }
        
        if (parts.size() != 3) {
            return false;
        }
        
        uint32_t device_id = std::stoul(parts[0]);
        // uint32_t object_type = std::stoul(parts[1]);  // 사용하지 않으므로 주석 처리
        uint32_t object_instance = std::stoul(parts[2]);
        
        // 실제 DB에서 DataPoint 찾기
        int db_device_id = FindDeviceIdInDatabase(device_id);
        if (db_device_id <= 0) {
            return false;
        }
        
        // 🔥 수정: QueryCondition 올바른 초기화 (3개 매개변수)
        std::vector<QueryCondition> dp_conditions = {
            QueryCondition("device_id", "=", std::to_string(db_device_id)),
            QueryCondition("address", "=", std::to_string(object_instance))
        };
        
        auto datapoints = datapoint_repository_->findByConditions(dp_conditions);
        
        if (datapoints.empty()) {
            return false;
        }
        
        int datapoint_id = datapoints[0].getId();
        
        // 🔥 수정: QueryCondition 올바른 초기화 (3개 매개변수)
        std::vector<QueryCondition> cv_conditions = {
            QueryCondition("point_id", "=", std::to_string(datapoint_id))  // data_point_id → point_id
        };
        
        auto current_values = current_value_repository_->findByConditions(cv_conditions);
        
        if (!current_values.empty()) {
            auto& current_value = current_values[0];
            
            // 🔥 수정: 타입 변환 (string → double)
            double numeric_value = ConvertDataValueToDouble(value.value);
            current_value.setCurrentValue(std::to_string(numeric_value));
            current_value.setQuality(DataQuality::GOOD);
            current_value.setValueTimestamp(value.timestamp);
            
            return current_value_repository_->update(current_value);
        } else {
            // 새 CurrentValue 생성
            CurrentValueEntity new_value;
            new_value.setPointId(datapoint_id);  // setDataPointId → setPointId
            
            // 🔥 수정: 타입 변환 (string → double)
            double numeric_value = ConvertDataValueToDouble(value.value);
            new_value.setCurrentValue(std::to_string(numeric_value));
            new_value.setQuality(DataQuality::GOOD);
            new_value.setValueTimestamp(value.timestamp);
            
            return current_value_repository_->save(new_value);
        }
        
    } catch (const std::exception& e) {
        return false;
    }
}

// =============================================================================
// 내부 유틸리티 함수들 - 🔥 완전 수정
// =============================================================================

int BACnetDiscoveryService::FindDeviceIdInDatabase(uint32_t bacnet_device_id) {
    try {
        if (!device_repository_) {
            return -1;
        }
        
        // 🔥 수정: QueryCondition 올바른 초기화 (3개 매개변수)
        std::vector<QueryCondition> conditions = {
            QueryCondition("device_type", "=", "BACNET")
        };
        
        auto all_devices = device_repository_->findByConditions(conditions);
        
        for (const auto& device : all_devices) {
            try {
                auto config_str = device.getConfig();
                if (!config_str.empty()) {
                    json config = json::parse(config_str);
                    if (config.contains("bacnet_device_id") && 
                        config["bacnet_device_id"].get<uint32_t>() == bacnet_device_id) {
                        return device.getId();
                    }
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

std::string BACnetDiscoveryService::BACnetAddressToString(const BACNET_ADDRESS& address) {
    if (address.len >= 4) {
        return std::to_string(address.adr[0]) + "." +
               std::to_string(address.adr[1]) + "." +
               std::to_string(address.adr[2]) + "." +
               std::to_string(address.adr[3]);
    }
    return "0.0.0.0";
}

double BACnetDiscoveryService::ConvertDataValueToDouble(const PulseOne::Structs::DataValue& value) {
    try {
        if (std::holds_alternative<bool>(value)) {
            return std::get<bool>(value) ? 1.0 : 0.0;
        } else if (std::holds_alternative<int>(value)) {
            return static_cast<double>(std::get<int>(value));
        } else if (std::holds_alternative<double>(value)) {
            return std::get<double>(value);
        } else if (std::holds_alternative<float>(value)) {
            return static_cast<double>(std::get<float>(value));
        } else if (std::holds_alternative<std::string>(value)) {
            try {
                return std::stod(std::get<std::string>(value));
            } catch (...) {
                return 0.0;
            }
        }
        return 0.0;
    } catch (const std::exception& e) {
        return 0.0;
    }
}

std::string BACnetDiscoveryService::GenerateDataPointId(uint32_t device_id, const Drivers::DataPoint& object) {
    return std::to_string(device_id) + ":" + 
           std::to_string(static_cast<int>(std::stoi(object.protocol_params.at("bacnet_object_type")))) + ":" + 
           std::to_string(object.address);
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

PulseOne::Enums::DataType BACnetDiscoveryService::DetermineDataType(int type) {
    switch (type) {
        case 0:  // ANALOG_INPUT
        case 1:  // ANALOG_OUTPUT
        case 2:  // ANALOG_VALUE
            return DataType::FLOAT32;
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
        case DataType::FLOAT32: return "float32";
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

int BACnetDiscoveryService::GuessObjectTypeFromName(const std::string& object_name) {
    if (object_name.empty()) return -1;
    
    // 대소문자 구분 없이 검사
    std::string upper_name = object_name;
    std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(), ::toupper);
    
    // "AI", "AO", "AV" 등으로 시작하는지 확인
    if (upper_name.length() >= 2) {
        std::string prefix = upper_name.substr(0, 2);
        
        if (prefix == "AI") return 0;   // ANALOG_INPUT
        if (prefix == "AO") return 1;   // ANALOG_OUTPUT
        if (prefix == "AV") return 2;   // ANALOG_VALUE
        if (prefix == "BI") return 3;   // BINARY_INPUT
        if (prefix == "BO") return 4;   // BINARY_OUTPUT
        if (prefix == "BV") return 5;   // BINARY_VALUE
        if (prefix == "MI") return 13;  // MULTI_STATE_INPUT
        if (prefix == "MO") return 14;  // MULTI_STATE_OUTPUT
        if (prefix == "MV") return 19;  // MULTI_STATE_VALUE
    }
    
    // 전체 이름으로 확인
    if (upper_name.find("ANALOG") != std::string::npos) return 0;
    if (upper_name.find("BINARY") != std::string::npos) return 3;
    if (upper_name.find("MULTI") != std::string::npos) return 13;
    if (upper_name.find("DEVICE") != std::string::npos) return 8;
    
    return -1;  // 추정 불가
}


} // namespace Workers
} // namespace PulseOne