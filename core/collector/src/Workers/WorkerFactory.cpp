// =============================================================================
// collector/src/Workers/WorkerFactory.cpp - 컴파일 에러 수정된 버전
// =============================================================================

#include "Workers/WorkerFactory.h"

// Worker 구현체들
#include "Workers/Protocol/BACnetWorker.h"
#include "Workers/Protocol/MQTTWorker.h"
#include "Workers/Protocol/ModbusWorker.h"
#include "Workers/Protocol/OPCUAWorker.h"
// #include "Workers/Protocol/BleBeaconWorker.h"
#include "Workers/Protocol/HttpRestWorker.h"
#include "Workers/Protocol/ROSWorker.h"

// Database
#include "Database/Entities/DeviceEntity.h"
#include "Database/Repositories/DataPointRepository.h" // 추가
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DeviceSettingsRepository.h"
#include "Database/Repositories/ProtocolRepository.h"
#include "Database/RepositoryFactory.h"
#include "Logging/LogManager.h"

// JSON 파싱
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>

namespace PulseOne {
namespace Workers {

// =============================================================================
// 핵심 Worker 생성 기능
// =============================================================================

std::unique_ptr<BaseDeviceWorker>
WorkerFactory::CreateWorker(const Database::Entities::DeviceEntity &device) {
  try {
    auto creators = LoadProtocolCreators();

    std::string protocol_type;
    try {
      protocol_type = GetProtocolTypeById(device.getProtocolId());
    } catch (const std::exception &e) {
      LogManager::getInstance().Error("프로토콜 타입 조회 실패: " +
                                      std::string(e.what()));
      return nullptr;
    }

    auto it = creators.find(protocol_type);
    if (it == creators.end()) {
      LogManager::getInstance().Error("지원하지 않는 프로토콜: " +
                                      protocol_type);
      return nullptr;
    }

    PulseOne::Structs::DeviceInfo device_info;
    if (!ConvertToDeviceInfoSafe(device, device_info)) {
      LogManager::getInstance().Error("DeviceInfo 변환 실패: " +
                                      device.getName());
      return nullptr;
    }

    // 🔥 DeviceSettings를 DriverConfig properties로 동기화
    device_info.SyncToDriverConfig();

    LogManager::getInstance().Info("[WorkerFactory] After SyncToDriverConfig:");
    LogManager::getInstance().Info(
        "  read_timeout_ms (opt): " +
        (device_info.read_timeout_ms.has_value()
             ? std::to_string(device_info.read_timeout_ms.value())
             : "null"));
    LogManager::getInstance().Info(
        "  Properties size: " +
        std::to_string(device_info.driver_config.properties.size()));

    // Worker 생성
    std::unique_ptr<BaseDeviceWorker> worker;
    try {
      worker = it->second(device_info);
    } catch (const std::exception &e) {
      LogManager::getInstance().Error("Worker 생성 중 예외: " +
                                      std::string(e.what()));
      return nullptr;
    }

    if (worker) {
      LogManager::getInstance().Info("Worker 생성 완료: " + device.getName() +
                                     " (" + protocol_type + ")");
    }

    return worker;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("CreateWorker 예외: " +
                                    std::string(e.what()));
    return nullptr;
  }
}

std::unique_ptr<BaseDeviceWorker>
WorkerFactory::CreateWorkerById(int device_id) {
  if (device_id <= 0) {
    LogManager::getInstance().Error("잘못된 device_id: " +
                                    std::to_string(device_id));
    return nullptr;
  }

  try {
    auto &repo_factory = Database::RepositoryFactory::getInstance();
    auto device_repo = repo_factory.getDeviceRepository();

    if (!device_repo) {
      LogManager::getInstance().Error("DeviceRepository 없음");
      return nullptr;
    }

    auto device_opt = device_repo->findById(device_id);
    if (!device_opt.has_value()) {
      LogManager::getInstance().Error("디바이스 없음: " +
                                      std::to_string(device_id));
      return nullptr;
    }

    return CreateWorker(device_opt.value());

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("CreateWorkerById 예외: " +
                                    std::string(e.what()));
    return nullptr;
  }
}

bool WorkerFactory::GetDeviceInfoById(int device_id,
                                      PulseOne::Structs::DeviceInfo &info) {
  if (device_id <= 0)
    return false;

  try {
    auto &repo_factory = Database::RepositoryFactory::getInstance();
    auto device_repo = repo_factory.getDeviceRepository();

    if (!device_repo)
      return false;

    auto device_opt = device_repo->findById(device_id);
    if (!device_opt.has_value())
      return false;

    return ConvertToDeviceInfoSafe(device_opt.value(), info);
  } catch (...) {
    return false;
  }
}

std::vector<PulseOne::Structs::DataPoint>
WorkerFactory::LoadDeviceDataPoints(int device_id) {
  std::vector<PulseOne::Structs::DataPoint> points;
  if (device_id <= 0)
    return points;

  try {
    auto &repo_factory = Database::RepositoryFactory::getInstance();
    auto datapoint_repo = repo_factory.getDataPointRepository();

    if (!datapoint_repo)
      return points;

    // DataPointRepository::getDataPointsWithCurrentValues 사용
    points = datapoint_repo->getDataPointsWithCurrentValues(
        device_id, true); // enabled_only=true

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("LoadDeviceDataPoints 예외: " +
                                    std::string(e.what()));
  }

  return points;
}

// =============================================================================
// 프로토콜 관리
// =============================================================================

std::vector<std::string> WorkerFactory::GetSupportedProtocols() const {
  try {
    auto creators = LoadProtocolCreators();

    std::vector<std::string> protocols;
    protocols.reserve(creators.size());

    for (const auto &[protocol, creator] : creators) {
      protocols.push_back(protocol);
    }
    return protocols;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("GetSupportedProtocols 예외: " +
                                    std::string(e.what()));
    return {};
  }
}

bool WorkerFactory::IsProtocolSupported(
    const std::string &protocol_type) const {
  if (protocol_type.empty())
    return false;

  auto protocols = GetSupportedProtocols();
  return std::find(protocols.begin(), protocols.end(), protocol_type) !=
         protocols.end();
}

void WorkerFactory::ReloadProtocols() {
  LogManager::getInstance().Info("프로토콜 설정 리로드");
}

// =============================================================================
// 내부 변환 함수들
// =============================================================================

std::string WorkerFactory::GetProtocolTypeById(int protocol_id) {
  if (protocol_id <= 0) {
    throw std::invalid_argument("Invalid protocol_id: " +
                                std::to_string(protocol_id));
  }

  try {
    auto &repo_factory = Database::RepositoryFactory::getInstance();
    auto protocol_repo = repo_factory.getProtocolRepository();

    if (protocol_repo) {
      auto protocol_opt = protocol_repo->findById(protocol_id);
      if (protocol_opt.has_value()) {
        return protocol_opt.value().getProtocolType();
      }
    }
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("GetProtocolTypeById DB 조회 실패: " +
                                    std::string(e.what()));
  }

  // 폴백 (기존 하드코딩된 매핑 유지 - DB 조회가 실패하거나 없을 경우를 대비)
  switch (protocol_id) {
  case 1:
    return "MODBUS_TCP";
  case 2:
    return "MODBUS_RTU";
  case 3:
    return "MQTT";
  case 4:
    return "BACNET";
  case 5:
    return "OPC_UA";
  // case 6: return "BLE_BEACON";
  case 7:
    return "ROS";
  case 8:
    return "HTTP_REST";
  default:
    throw std::runtime_error("Unsupported protocol_id: " +
                             std::to_string(protocol_id));
  }
}

bool WorkerFactory::ConvertToDeviceInfoSafe(
    const Database::Entities::DeviceEntity &device,
    PulseOne::Structs::DeviceInfo &info) {
  try {
    // 기본 정보 설정
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

    // 프로토콜 타입 설정
    info.protocol_type = GetProtocolTypeById(device.getProtocolId());

    // 🔥 핵심 수정: 테스트에서 검증하는 속성들을 properties 맵에 추가
    info.properties["device_id"] = info.id;
    info.properties["device_name"] = info.name;
    info.properties["enabled"] = info.is_enabled ? "true" : "false";
    info.properties["endpoint"] = info.endpoint;
    info.properties["protocol_type"] = info.protocol_type;

    // 추가 기본 속성들도 properties 맵에 포함
    if (!info.description.empty()) {
      info.properties["description"] = info.description;
    }
    if (!info.device_type.empty()) {
      info.properties["device_type"] = info.device_type;
    }
    if (!info.manufacturer.empty()) {
      info.properties["manufacturer"] = info.manufacturer;
    }
    if (!info.model.empty()) {
      info.properties["model"] = info.model;
    }
    if (!info.serial_number.empty()) {
      info.properties["serial_number"] = info.serial_number;
    }

    // 안전한 변환 호출
    if (!ParseEndpointSafe(info))
      return false;
    if (!ParseConfigToPropertiesSafe(info))
      return false;
    if (!LoadDeviceSettingsSafe(info, device.getId()))
      return false;
    if (!ApplyProtocolDefaultsSafe(info))
      return false;
    info.SyncToDriverConfig();
    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("ConvertToDeviceInfoSafe 예외: " +
                                    std::string(e.what()));
    return false;
  }
}

bool WorkerFactory::ParseEndpointSafe(PulseOne::Structs::DeviceInfo &info) {
  try {
    const std::string &endpoint = info.endpoint;
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

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("ParseEndpointSafe 예외: " +
                                    std::string(e.what()));
    info.port = GetDefaultPort(info.protocol_type);
    return false;
  }
}

bool WorkerFactory::ParseConfigToPropertiesSafe(
    PulseOne::Structs::DeviceInfo &info) {
  try {
    if (info.config.empty())
      return true;

    auto json_obj = nlohmann::json::parse(info.config);
    if (!json_obj.is_object())
      return false;

    for (auto it = json_obj.begin(); it != json_obj.end(); ++it) {
      const std::string &key = it.key();
      const auto &value = it.value();

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

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("ParseConfigToPropertiesSafe 예외: " +
                                    std::string(e.what()));
    return false;
  }
}

bool WorkerFactory::LoadDeviceSettingsSafe(PulseOne::Structs::DeviceInfo &info,
                                           int device_id) {
  try {
    auto &repo_factory = Database::RepositoryFactory::getInstance();
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

    const auto &settings = settings_opt.value();

    // =========================================================================
    // 🔥 DeviceSettings의 모든 필드를 DeviceInfo로 복사 (누락된 필드 추가)
    // =========================================================================

    // 1. 기본 타이밍 설정
    int polling = settings.getPollingIntervalMs();
    if (polling < 100 || polling > 300000)
      polling = 1000;
    info.polling_interval_ms = polling;

    int timeout = settings.getReadTimeoutMs(); // 기본 타임아웃으로 사용
    if (timeout < 1000 || timeout > 60000)
      timeout = 5000;
    info.timeout_ms = timeout;

    int retry = settings.getMaxRetryCount();
    if (retry < 0 || retry > 10)
      retry = 3;
    info.retry_count = retry;

    // 2. 추가 타이밍 설정
    info.connection_timeout_ms = settings.getConnectionTimeoutMs(); // optional
    info.read_timeout_ms = settings.getReadTimeoutMs();             // int
    info.write_timeout_ms = settings.getWriteTimeoutMs();           // int
    info.scan_rate_override = settings.getScanRateOverride();       // optional

    // 3. 재시도 정책
    info.max_retry_count = settings.getMaxRetryCount();
    info.retry_interval_ms = settings.getRetryIntervalMs();
    info.backoff_multiplier = settings.getBackoffMultiplier();
    info.backoff_time_ms = settings.getBackoffTimeMs();
    info.max_backoff_time_ms = settings.getMaxBackoffTimeMs();

    // 4. Keep-alive 설정
    info.is_keep_alive_enabled = settings.isKeepAliveEnabled();
    info.keep_alive_interval_s = settings.getKeepAliveIntervalS();
    info.keep_alive_timeout_s = settings.getKeepAliveTimeoutS();

    // 5. 모니터링 및 진단
    info.is_data_validation_enabled = settings.isDataValidationEnabled();
    info.is_diagnostic_mode_enabled = settings.isDiagnosticModeEnabled();
    info.is_auto_registration_enabled = settings.isAutoRegistrationEnabled();
    info.updated_by = 0; // updated_by not available in entity

    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Warn("DeviceSettings 로드 실패: " +
                                   std::string(e.what()));
    ApplyDefaultSettings(info);
    return false;
  }
}

bool WorkerFactory::ApplyProtocolDefaultsSafe(
    PulseOne::Structs::DeviceInfo &info) {
  try {
    const std::string &protocol = info.protocol_type;

    if (protocol == "MODBUS_TCP" || protocol == "modbus_tcp") {
      if (info.properties.find("unit_id") == info.properties.end()) {
        info.properties["unit_id"] = "1";
      }
    } else if (protocol == "MODBUS_RTU" || protocol == "modbus_rtu") {
      if (info.properties.find("unit_id") == info.properties.end()) {
        info.properties["unit_id"] = "1";
      }
      if (info.properties.find("baud_rate") == info.properties.end()) {
        info.properties["baud_rate"] = "9600";
      }
    } else if (protocol == "MQTT" || protocol == "mqtt") {
      if (info.properties.find("client_id") == info.properties.end()) {
        info.properties["client_id"] = "pulseone_" + info.id;
      }
    } else if (protocol == "BACNET" || protocol == "bacnet") {
      if (info.properties.find("device_instance") == info.properties.end()) {
        info.properties["device_instance"] = "1";
      }
    }

    if (info.polling_interval_ms == 0) {
      ApplyDefaultSettings(info);
    }

    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("ApplyProtocolDefaultsSafe 예외: " +
                                    std::string(e.what()));
    ApplyDefaultSettings(info);
    return false;
  }
}

void WorkerFactory::ApplyDefaultSettings(PulseOne::Structs::DeviceInfo &info) {
  try {
    const std::string &protocol = info.protocol_type;

    if (protocol == "MODBUS_TCP" || protocol == "modbus_tcp") {
      info.polling_interval_ms = 1000;
      info.timeout_ms = 3000;
      info.retry_count = 3;
    } else if (protocol == "MODBUS_RTU" || protocol == "modbus_rtu") {
      info.polling_interval_ms = 2000;
      info.timeout_ms = 5000;
      info.retry_count = 5;
    } else if (protocol == "MQTT" || protocol == "mqtt") {
      info.polling_interval_ms = 500;
      info.timeout_ms = 10000;
      info.retry_count = 3;
    } else if (protocol == "BACNET" || protocol == "bacnet") {
      info.polling_interval_ms = 5000;
      info.timeout_ms = 8000;
      info.retry_count = 3;
    } else {
      info.polling_interval_ms = 1000;
      info.timeout_ms = 5000;
      info.retry_count = 3;
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("ApplyDefaultSettings 예외: " +
                                    std::string(e.what()));
    info.polling_interval_ms = 1000;
    info.timeout_ms = 5000;
    info.retry_count = 3;
  }
}

int WorkerFactory::GetDefaultPort(const std::string &protocol_type) {
  try {
    if (protocol_type == "MODBUS_TCP" || protocol_type == "modbus_tcp")
      return 502;
    if (protocol_type == "MODBUS_RTU" || protocol_type == "modbus_rtu")
      return 0;
    if (protocol_type == "MQTT" || protocol_type == "mqtt")
      return 1883;
    if (protocol_type == "BACNET" || protocol_type == "bacnet")
      return 47808;
    return 0;

  } catch (const std::exception &) {
    return 0;
  }
}

std::map<std::string, WorkerCreator>
WorkerFactory::LoadProtocolCreators() const {
  std::map<std::string, WorkerCreator> creators;

  try {
    // 지원하는 프로토콜들 등록
    creators["MODBUS_TCP"] = [](const PulseOne::Structs::DeviceInfo &info)
        -> std::unique_ptr<BaseDeviceWorker> {
      return std::make_unique<ModbusWorker>(info);
    };

    creators["MODBUS_RTU"] = [](const PulseOne::Structs::DeviceInfo &info)
        -> std::unique_ptr<BaseDeviceWorker> {
      return std::make_unique<ModbusWorker>(info);
    };

    creators["MQTT"] = [](const PulseOne::Structs::DeviceInfo &info)
        -> std::unique_ptr<BaseDeviceWorker> {
      return std::make_unique<MQTTWorker>(info);
    };

    creators["BACNET"] = [](const PulseOne::Structs::DeviceInfo &info)
        -> std::unique_ptr<BaseDeviceWorker> {
      return std::make_unique<BACnetWorker>(info);
    };

    creators["OPC_UA"] = [](const PulseOne::Structs::DeviceInfo &info)
        -> std::unique_ptr<BaseDeviceWorker> {
      return std::make_unique<OPCUAWorker>(info);
    };

    // creators["BLE_BEACON"] = [](const PulseOne::Structs::DeviceInfo& info) ->
    // std::unique_ptr<BaseDeviceWorker> {
    //     return std::make_unique<BleBeaconWorker>(info);
    // };

    creators["HTTP_REST"] = [](const PulseOne::Structs::DeviceInfo &info)
        -> std::unique_ptr<BaseDeviceWorker> {
      return std::make_unique<HttpRestWorker>(info);
    };

    creators["ROS"] = [](const PulseOne::Structs::DeviceInfo &info)
        -> std::unique_ptr<BaseDeviceWorker> {
      return std::make_unique<ROSWorker>(info);
    };

    // 별칭들 (Aliases)
    creators["modbus_tcp"] = creators["MODBUS_TCP"];
    creators["modbus_rtu"] = creators["MODBUS_RTU"];
    creators["mqtt"] = creators["MQTT"];
    creators["bacnet"] = creators["BACNET"];
    creators["opc_ua"] = creators["OPC_UA"];
    creators["ros"] = creators["ROS"];

    // Database protocol_type mappings
    creators["tcp"] = creators["MODBUS_TCP"];
    creators["serial"] = creators["MODBUS_RTU"];

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("LoadProtocolCreators 예외: " +
                                    std::string(e.what()));
  }

  return creators;
}

} // namespace Workers
} // namespace PulseOne