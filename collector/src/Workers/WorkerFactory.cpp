// =============================================================================
// collector/src/Workers/WorkerFactory.cpp - 컴파일 에러 수정된 버전
// =============================================================================

#include "Workers/WorkerFactory.h"

// Worker 구현체들
#include "Workers/Protocol/ModbusTcpWorker.h"
#include "Workers/Protocol/ModbusRtuWorker.h"
#include "Workers/Protocol/MqttWorker.h"
#include "Workers/Protocol/BACnetWorker.h"

// Database
#include "Database/RepositoryFactory.h"
#include "Database/Entities/DeviceEntity.h"
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DeviceSettingsRepository.h"
#include "Utils/LogManager.h"

// JSON 파싱
#include <nlohmann/json.hpp>
#include <sstream>
#include <mutex>

namespace PulseOne {
namespace Workers {

// =============================================================================
// 핵심 Worker 생성 기능
// =============================================================================

std::unique_ptr<BaseDeviceWorker> WorkerFactory::CreateWorker(const Database::Entities::DeviceEntity& device) {
    try {
        // 프로토콜 Creator 로드 (캐시된 버전 사용)
        auto creators = LoadProtocolCreators();
        
        // protocol_id로 실제 프로토콜 타입 조회
        std::string protocol_type;
        try {
            protocol_type = GetProtocolTypeById(device.getProtocolId());
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("프로토콜 타입 조회 실패: " + std::string(e.what()));
            return nullptr;
        }
        
        auto it = creators.find(protocol_type);
        if (it == creators.end()) {
            LogManager::getInstance().Error("지원하지 않는 프로토콜: " + protocol_type);
            return nullptr;
        }
        
        // DeviceEntity → DeviceInfo 변환
        PulseOne::Structs::DeviceInfo device_info;
        if (!ConvertToDeviceInfoSafe(device, device_info)) {
            LogManager::getInstance().Error("DeviceInfo 변환 실패: " + device.getName());
            return nullptr;
        }
        
        device_info.protocol_type = protocol_type;
        
        // Worker 생성
        std::unique_ptr<BaseDeviceWorker> worker;
        try {
            worker = it->second(device_info);
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Worker 생성 중 예외: " + std::string(e.what()));
            return nullptr;
        }
        
        if (worker) {
            LogManager::getInstance().Info("Worker 생성 완료: " + device.getName() + " (" + protocol_type + ")");
        }
        
        return worker;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("CreateWorker 예외: " + std::string(e.what()));
        return nullptr;
    }
}

std::unique_ptr<BaseDeviceWorker> WorkerFactory::CreateWorkerById(int device_id) {
    if (device_id <= 0) {
        LogManager::getInstance().Error("잘못된 device_id: " + std::to_string(device_id));
        return nullptr;
    }
    
    try {
        auto& repo_factory = Database::RepositoryFactory::getInstance();
        auto device_repo = repo_factory.getDeviceRepository();
        
        if (!device_repo) {
            LogManager::getInstance().Error("DeviceRepository 없음");
            return nullptr;
        }
        
        auto device_opt = device_repo->findById(device_id);
        if (!device_opt.has_value()) {
            LogManager::getInstance().Error("디바이스 없음: " + std::to_string(device_id));
            return nullptr;
        }
        
        return CreateWorker(device_opt.value());
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("CreateWorkerById 예외: " + std::string(e.what()));
        return nullptr;
    }
}

// =============================================================================
// 프로토콜 관리
// =============================================================================

std::vector<std::string> WorkerFactory::GetSupportedProtocols() const {
    try {
        // const_cast 제거하고 직접 LoadProtocolCreators 호출
        static std::mutex static_mutex;
        std::lock_guard<std::mutex> lock(static_mutex);
        
        std::map<std::string, WorkerCreator> creators;
        
        // 지원하는 프로토콜들 등록
        creators["MODBUS_TCP"] = [](const PulseOne::Structs::DeviceInfo& info) {
            return std::make_unique<ModbusTcpWorker>(info);
        };
        creators["MODBUS_RTU"] = [](const PulseOne::Structs::DeviceInfo& info) {
            return std::make_unique<ModbusRtuWorker>(info);
        };
        creators["MQTT"] = [](const PulseOne::Structs::DeviceInfo& info) {
            return std::make_unique<MQTTWorker>(info);
        };
        creators["BACNET"] = [](const PulseOne::Structs::DeviceInfo& info) {
            return std::make_unique<BACnetWorker>(info);
        };
        
        std::vector<std::string> protocols;
        protocols.reserve(creators.size());
        
        for (const auto& [protocol, creator] : creators) {
            protocols.push_back(protocol);
        }
        return protocols;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("GetSupportedProtocols 예외: " + std::string(e.what()));
        return {};
    }
}

bool WorkerFactory::IsProtocolSupported(const std::string& protocol_type) const {
    if (protocol_type.empty()) return false;
    
    auto protocols = GetSupportedProtocols();
    return std::find(protocols.begin(), protocols.end(), protocol_type) != protocols.end();
}

void WorkerFactory::ReloadProtocols() {
    LogManager::getInstance().Info("프로토콜 설정 리로드");
}

// =============================================================================
// 내부 변환 함수들
// =============================================================================

std::string WorkerFactory::GetProtocolTypeById(int protocol_id) {
    if (protocol_id <= 0) {
        throw std::invalid_argument("Invalid protocol_id: " + std::to_string(protocol_id));
    }
    
    // 하드코딩된 매핑 사용
    switch (protocol_id) {
        case 1: return "MODBUS_TCP";
        case 2: return "MODBUS_RTU"; 
        case 3: return "MQTT";
        case 4: return "BACNET";
        default: 
            throw std::runtime_error("Unsupported protocol_id: " + std::to_string(protocol_id));
    }
}

bool WorkerFactory::ConvertToDeviceInfoSafe(const Database::Entities::DeviceEntity& device, 
                                          PulseOne::Structs::DeviceInfo& info) {
    try {
        // 기본 정보
        info.id = std::to_string(device.getId());
        info.tenant_id = device.getTenantId();
        info.site_id = device.getSiteId();
        
        // optional 값 안전 처리
        if (device.getDeviceGroupId().has_value()) {
            info.device_group_id = device.getDeviceGroupId().value();
        }
        
        info.name = device.getName();
        info.description = device.getDescription();
        info.device_type = device.getDeviceType();
        info.manufacturer = device.getManufacturer();
        info.model = device.getModel();
        info.serial_number = device.getSerialNumber();
        
        info.endpoint = device.getEndpoint();
        info.config = device.getConfig();
        info.is_enabled = device.isEnabled();
        
        // 안전한 변환 호출
        if (!ParseEndpointSafe(info)) return false;
        if (!ParseConfigToPropertiesSafe(info)) return false;
        if (!LoadDeviceSettingsSafe(info, device.getId())) return false;
        if (!ApplyProtocolDefaultsSafe(info)) return false;
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ConvertToDeviceInfoSafe 예외: " + std::string(e.what()));
        return false;
    }
}

bool WorkerFactory::ParseEndpointSafe(PulseOne::Structs::DeviceInfo& info) {
    try {
        const std::string& endpoint = info.endpoint;
        if (endpoint.empty()) {
            info.port = GetDefaultPort(info.protocol_type);
            return true;
        }
        
        // "tcp://192.168.1.10:502" 또는 "192.168.1.10:502" 형태 파싱
        std::string cleaned = endpoint;
        size_t pos = cleaned.find("://");
        if (pos != std::string::npos) {
            cleaned = cleaned.substr(pos + 3);
        }
        
        pos = cleaned.find(':');
        if (pos != std::string::npos) {
            info.ip_address = cleaned.substr(0, pos);
            try {
                int port = std::stoi(cleaned.substr(pos + 1));
                if (port < 1 || port > 65535) {
                    info.port = GetDefaultPort(info.protocol_type);
                } else {
                    info.port = port;
                }
            } catch (...) {
                info.port = GetDefaultPort(info.protocol_type);
            }
        } else {
            info.ip_address = cleaned;
            info.port = GetDefaultPort(info.protocol_type);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ParseEndpointSafe 예외: " + std::string(e.what()));
        info.port = GetDefaultPort(info.protocol_type);
        return false;
    }
}

bool WorkerFactory::ParseConfigToPropertiesSafe(PulseOne::Structs::DeviceInfo& info) {
    try {
        if (info.config.empty()) return true;
        
        auto json_obj = nlohmann::json::parse(info.config);
        if (!json_obj.is_object()) return false;
        
        for (auto it = json_obj.begin(); it != json_obj.end(); ++it) {
            const std::string& key = it.key();
            const auto& value = it.value();
            
            std::string str_value;
            if (value.is_string()) {
                str_value = value.get<std::string>();
            } else if (value.is_number_integer()) {
                str_value = std::to_string(value.get<int>());
            } else if (value.is_number_float()) {
                str_value = std::to_string(value.get<double>());
            } else if (value.is_boolean()) {
                str_value = value.get<bool>() ? "true" : "false";
            } else {
                str_value = value.dump();
            }
            
            info.properties[key] = str_value;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ParseConfigToPropertiesSafe 예외: " + std::string(e.what()));
        return false;
    }
}

bool WorkerFactory::LoadDeviceSettingsSafe(PulseOne::Structs::DeviceInfo& info, int device_id) {
    try {
        auto& repo_factory = Database::RepositoryFactory::getInstance();
        auto settings_repo = repo_factory.getDeviceSettingsRepository();
        
        if (!settings_repo) {
            ApplyDefaultSettings(info);
            return true;
        }
        
        auto settings_opt = settings_repo->findById(device_id);
        if (!settings_opt.has_value()) {
            ApplyDefaultSettings(info);
            return true;
        }
        
        const auto& settings = settings_opt.value();
        
        // 값 범위 검증
        int polling = settings.getPollingIntervalMs();
        if (polling < 100 || polling > 300000) {
            polling = 1000;
        }
        info.polling_interval_ms = polling;
        
        int timeout = settings.getReadTimeoutMs();
        if (timeout < 1000 || timeout > 60000) {
            timeout = 5000;
        }
        info.timeout_ms = timeout;
        
        int retry = settings.getMaxRetryCount();
        if (retry < 0 || retry > 10) {
            retry = 3;
        }
        info.retry_count = retry;
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Warn("DeviceSettings 로드 실패: " + std::string(e.what()));
        ApplyDefaultSettings(info);
        return false;
    }
}

bool WorkerFactory::ApplyProtocolDefaultsSafe(PulseOne::Structs::DeviceInfo& info) {
    try {
        const std::string& protocol = info.protocol_type;
        
        if (protocol == "MODBUS_TCP" || protocol == "modbus_tcp") {
            if (info.properties.find("unit_id") == info.properties.end()) {
                info.properties["unit_id"] = "1";
            }
        }
        else if (protocol == "MODBUS_RTU" || protocol == "modbus_rtu") {
            if (info.properties.find("unit_id") == info.properties.end()) {
                info.properties["unit_id"] = "1";
            }
            if (info.properties.find("baud_rate") == info.properties.end()) {
                info.properties["baud_rate"] = "9600";
            }
        }
        else if (protocol == "MQTT" || protocol == "mqtt") {
            if (info.properties.find("client_id") == info.properties.end()) {
                info.properties["client_id"] = "pulseone_" + info.id;
            }
        }
        else if (protocol == "BACNET" || protocol == "bacnet") {
            if (info.properties.find("device_instance") == info.properties.end()) {
                info.properties["device_instance"] = "1";
            }
        }
        
        if (info.polling_interval_ms == 0) {
            ApplyDefaultSettings(info);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ApplyProtocolDefaultsSafe 예외: " + std::string(e.what()));
        ApplyDefaultSettings(info);
        return false;
    }
}

void WorkerFactory::ApplyDefaultSettings(PulseOne::Structs::DeviceInfo& info) {
    try {
        const std::string& protocol = info.protocol_type;
        
        if (protocol == "MODBUS_TCP" || protocol == "modbus_tcp") {
            info.polling_interval_ms = 1000;
            info.timeout_ms = 3000;
            info.retry_count = 3;
        }
        else if (protocol == "MODBUS_RTU" || protocol == "modbus_rtu") {
            info.polling_interval_ms = 2000;
            info.timeout_ms = 5000;
            info.retry_count = 5;
        }
        else if (protocol == "MQTT" || protocol == "mqtt") {
            info.polling_interval_ms = 500;
            info.timeout_ms = 10000;
            info.retry_count = 3;
        }
        else if (protocol == "BACNET" || protocol == "bacnet") {
            info.polling_interval_ms = 5000;
            info.timeout_ms = 8000;
            info.retry_count = 3;
        }
        else {
            info.polling_interval_ms = 1000;
            info.timeout_ms = 5000;
            info.retry_count = 3;
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ApplyDefaultSettings 예외: " + std::string(e.what()));
        info.polling_interval_ms = 1000;
        info.timeout_ms = 5000;
        info.retry_count = 3;
    }
}

int WorkerFactory::GetDefaultPort(const std::string& protocol_type) {
    try {
        if (protocol_type == "MODBUS_TCP" || protocol_type == "modbus_tcp") return 502;
        if (protocol_type == "MODBUS_RTU" || protocol_type == "modbus_rtu") return 0;
        if (protocol_type == "MQTT" || protocol_type == "mqtt") return 1883;
        if (protocol_type == "BACNET" || protocol_type == "bacnet") return 47808;
        return 0;
        
    } catch (const std::exception&) {
        return 0;
    }
}

std::map<std::string, WorkerCreator> WorkerFactory::LoadProtocolCreators() {
    std::map<std::string, WorkerCreator> creators;
    
    try {
        // 지원하는 프로토콜들 등록
        creators["MODBUS_TCP"] = [](const PulseOne::Structs::DeviceInfo& info) -> std::unique_ptr<BaseDeviceWorker> {
            return std::make_unique<ModbusTcpWorker>(info);
        };
        
        creators["MODBUS_RTU"] = [](const PulseOne::Structs::DeviceInfo& info) -> std::unique_ptr<BaseDeviceWorker> {
            return std::make_unique<ModbusRtuWorker>(info);
        };
        
        creators["MQTT"] = [](const PulseOne::Structs::DeviceInfo& info) -> std::unique_ptr<BaseDeviceWorker> {
            return std::make_unique<MQTTWorker>(info);
        };
        
        creators["BACNET"] = [](const PulseOne::Structs::DeviceInfo& info) -> std::unique_ptr<BaseDeviceWorker> {
            return std::make_unique<BACnetWorker>(info);
        };
        
        // 별칭들
        creators["modbus_tcp"] = creators["MODBUS_TCP"];
        creators["modbus_rtu"] = creators["MODBUS_RTU"];
        creators["mqtt"] = creators["MQTT"];
        creators["bacnet"] = creators["BACNET"];
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("LoadProtocolCreators 예외: " + std::string(e.what()));
    }
    
    return creators;
}

} // namespace Workers
} // namespace PulseOne