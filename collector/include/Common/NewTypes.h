/**
 * @file NewTypes.h
 * @brief PulseOne 통합 타입 시스템 - 모든 중복 구조체 해결
 * @version 1.0
 * @date 2024
 */

#ifndef PULSEONE_NEW_TYPES_H
#define PULSEONE_NEW_TYPES_H

#include <string>
#include <vector>
#include <chrono>
#include <variant>
#include <optional>
#include <map>
#include <atomic>
#include <memory>
#include <stdexcept>  // std::runtime_error를 위해 추가
#include <type_traits>

namespace PulseOne {
namespace New {

// =============================================================================
// 🎯 핵심: 통합 프로토콜 열거형
// =============================================================================

enum class ProtocolType {
    MODBUS_TCP = 1,
    MODBUS_RTU = 2,
    MQTT = 3,
    BACNET_IP = 4,
    BACNET_MSTP = 5,
    OPCUA = 6,
    SNMP = 7,
    HTTP_REST = 8,
    WEBSOCKET = 9,
    CUSTOM = 99
};

enum class ConnectionStatus {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    ERROR = 3,
    TIMEOUT = 4,
    RECONNECTING = 5
};

enum class DataQuality {
    GOOD = 0,
    BAD = 1,
    UNCERTAIN = 2,
    STALE = 3,
    OFFLINE = 4
};

// =============================================================================
// 🎯 프로토콜별 설정 구조체들 (Union 대신 개별 구조체 사용)
// =============================================================================

struct ModbusConfig {
    int slave_id = 1;
    int max_registers = 100;
    bool use_function_16 = true;
    int connection_timeout_ms = 3000;
};

struct MqttConfig {
    std::string client_id = "pulseone_client";
    std::string username;
    std::string password;
    int qos = 1;
    bool clean_session = true;
    int keep_alive_seconds = 60;
};

struct BacnetConfig {
    uint32_t device_id = 0;
    uint16_t port = 47808;
    bool support_cov = true;
    int max_apdu_length = 1476;
    std::string network_interface;
};

struct OpcuaConfig {
    std::string security_policy = "None";
    std::string security_mode = "None";
    std::string certificate_path;
    std::string private_key_path;
};

// =============================================================================
// 🎯 핵심: 통합 디바이스 정보 구조체 (수정됨)
// =============================================================================

struct UnifiedDeviceInfo {
    // ===== 공통 필드 (모든 프로토콜) =====
    std::string id;
    std::string name;
    std::string description;
    ProtocolType protocol = ProtocolType::MODBUS_TCP;
    std::string endpoint;
    bool enabled = true;
    int timeout_ms = 5000;
    int poll_interval_ms = 1000;
    
    // ===== 프로토콜별 설정 (개별 구조체로 변경) =====
    ModbusConfig modbus_config;
    MqttConfig mqtt_config;
    BacnetConfig bacnet_config;
    OpcuaConfig opcua_config;
    
    // ===== 런타임 상태 정보 =====
    std::atomic<ConnectionStatus> status{ConnectionStatus::DISCONNECTED};
    std::chrono::system_clock::time_point last_connected;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
    
    // ===== 확장 속성 (동적) =====
    std::map<std::string, std::string> custom_properties;
    
    // =================================================================
    // 🎯 기본 생성자 및 소멸자
    // =================================================================
    
    UnifiedDeviceInfo() {
        created_at = std::chrono::system_clock::now();
        updated_at = created_at;
    }
    
    // 복사 생성자 (atomic 때문에 필요)
    UnifiedDeviceInfo(const UnifiedDeviceInfo& other) 
        : id(other.id), name(other.name), description(other.description),
          protocol(other.protocol), endpoint(other.endpoint), enabled(other.enabled),
          timeout_ms(other.timeout_ms), poll_interval_ms(other.poll_interval_ms),
          modbus_config(other.modbus_config), mqtt_config(other.mqtt_config),
          bacnet_config(other.bacnet_config), opcua_config(other.opcua_config),
          status(other.status.load()), last_connected(other.last_connected),
          created_at(other.created_at), updated_at(other.updated_at),
          custom_properties(other.custom_properties) {}
    
    // 할당 연산자
    UnifiedDeviceInfo& operator=(const UnifiedDeviceInfo& other) {
        if (this != &other) {
            id = other.id;
            name = other.name;
            description = other.description;
            protocol = other.protocol;
            endpoint = other.endpoint;
            enabled = other.enabled;
            timeout_ms = other.timeout_ms;
            poll_interval_ms = other.poll_interval_ms;
            modbus_config = other.modbus_config;
            mqtt_config = other.mqtt_config;
            bacnet_config = other.bacnet_config;
            opcua_config = other.opcua_config;
            status.store(other.status.load());
            last_connected = other.last_connected;
            created_at = other.created_at;
            updated_at = other.updated_at;
            custom_properties = other.custom_properties;
        }
        return *this;
    }
    
    // =================================================================
    // 🎯 타입 안전 접근자 메서드들
    // =================================================================
    
    bool IsModbus() const { 
        return protocol == ProtocolType::MODBUS_TCP || 
               protocol == ProtocolType::MODBUS_RTU; 
    }
    
    bool IsMqtt() const { 
        return protocol == ProtocolType::MQTT; 
    }
    
    bool IsBacnet() const { 
        return protocol == ProtocolType::BACNET_IP || 
               protocol == ProtocolType::BACNET_MSTP; 
    }
    
    bool IsOpcua() const { 
        return protocol == ProtocolType::OPCUA; 
    }
    
    ModbusConfig& GetModbusConfig() { 
        if (!IsModbus()) {
            throw std::runtime_error("Device is not Modbus protocol");
        }
        return modbus_config; 
    }
    
    const ModbusConfig& GetModbusConfig() const { 
        if (!IsModbus()) {
            throw std::runtime_error("Device is not Modbus protocol");
        }
        return modbus_config; 
    }
    
    MqttConfig& GetMqttConfig() { 
        if (!IsMqtt()) {
            throw std::runtime_error("Device is not MQTT protocol");
        }
        return mqtt_config; 
    }
    
    const MqttConfig& GetMqttConfig() const { 
        if (!IsMqtt()) {
            throw std::runtime_error("Device is not MQTT protocol");
        }
        return mqtt_config; 
    }
    
    BacnetConfig& GetBacnetConfig() { 
        if (!IsBacnet()) {
            throw std::runtime_error("Device is not BACnet protocol");
        }
        return bacnet_config; 
    }
    
    const BacnetConfig& GetBacnetConfig() const { 
        if (!IsBacnet()) {
            throw std::runtime_error("Device is not BACnet protocol");
        }
        return bacnet_config; 
    }
    
    // =================================================================
    // 🎯 유틸리티 메서드들
    // =================================================================
    
    std::string GetProtocolString() const {
        switch (protocol) {
            case ProtocolType::MODBUS_TCP: return "modbus_tcp";
            case ProtocolType::MODBUS_RTU: return "modbus_rtu";
            case ProtocolType::MQTT: return "mqtt";
            case ProtocolType::BACNET_IP: return "bacnet_ip";
            case ProtocolType::BACNET_MSTP: return "bacnet_mstp";
            case ProtocolType::OPCUA: return "opcua";
            case ProtocolType::SNMP: return "snmp";
            case ProtocolType::HTTP_REST: return "http_rest";
            case ProtocolType::WEBSOCKET: return "websocket";
            case ProtocolType::CUSTOM: return "custom";
            default: return "unknown";
        }
    }
    
    std::string GetStatusString() const {
        switch (status.load()) {
            case ConnectionStatus::DISCONNECTED: return "disconnected";
            case ConnectionStatus::CONNECTING: return "connecting";
            case ConnectionStatus::CONNECTED: return "connected";
            case ConnectionStatus::ERROR: return "error";
            case ConnectionStatus::TIMEOUT: return "timeout";
            case ConnectionStatus::RECONNECTING: return "reconnecting";
            default: return "unknown";
        }
    }
    
    bool IsActive() const {
        return enabled && (status.load() == ConnectionStatus::CONNECTED ||
                          status.load() == ConnectionStatus::CONNECTING);
    }
    
    void SetCustomProperty(const std::string& key, const std::string& value) {
        custom_properties[key] = value;
        updated_at = std::chrono::system_clock::now();
    }
    
    std::optional<std::string> GetCustomProperty(const std::string& key) const {
        auto it = custom_properties.find(key);
        if (it != custom_properties.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

// =============================================================================
// 🎯 핵심: 통합 데이터 포인트 구조체 (수정됨)
// =============================================================================

struct UnifiedDataPoint {
    std::string id;
    std::string device_id;
    std::string name;
    std::string description;
    
    // ===== 주소 정보 (각 프로토콜별로 분리) =====
    struct ModbusAddress {
        uint16_t register_address = 0;
        uint8_t function_code = 3;
        uint16_t count = 1;
    };
    
    struct MqttAddress {
        std::string topic;
        bool retain = false;
    };
    
    struct BacnetAddress {
        uint32_t object_type = 0;
        uint32_t object_instance = 0;
        uint32_t property_id = 85; // Present_Value
    };
    
    // 실제 주소는 프로토콜에 따라 하나만 사용
    ModbusAddress modbus_address;
    MqttAddress mqtt_address;
    BacnetAddress bacnet_address;
    
    // ===== 데이터 값 및 상태 =====
    std::variant<bool, int32_t, uint32_t, float, double, std::string> current_value;
    DataQuality quality = DataQuality::BAD;
    std::chrono::system_clock::time_point last_update;
    std::chrono::system_clock::time_point last_change;
    
    // ===== 설정 =====
    std::string data_type = "double";
    bool read_only = false;
    double scale_factor = 1.0;
    double offset = 0.0;
    std::string unit;
    
    // ===== 기본 생성자 =====
    UnifiedDataPoint() {
        last_update = std::chrono::system_clock::now();
        last_change = last_update;
        current_value = 0.0;
    }
    
    // ===== 유틸리티 메서드들 =====
    
    std::string GetValueAsString() const {
        return std::visit([](const auto& value) -> std::string {
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::string>) {
                return value;
            } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, bool>) {
                return value ? "true" : "false";
            } else {
                return std::to_string(value);
            }
        }, current_value);
    }
    
    std::optional<double> GetValueAsDouble() const {
        return std::visit([](const auto& value) -> std::optional<double> {
            if constexpr (std::is_arithmetic_v<std::decay_t<decltype(value)>>) {
                return static_cast<double>(value);
            } else {
                return std::nullopt;
            }
        }, current_value);
    }
    
    bool IsGoodQuality() const {
        return quality == DataQuality::GOOD;
    }
    
    std::optional<double> GetScaledValue() const {
        auto raw_value = GetValueAsDouble();
        if (raw_value.has_value()) {
            return raw_value.value() * scale_factor + offset;
        }
        return std::nullopt;
    }
    
    void UpdateValue(const std::variant<bool, int32_t, uint32_t, float, double, std::string>& new_value, 
                     DataQuality new_quality = DataQuality::GOOD) {
        if (current_value != new_value) {
            last_change = std::chrono::system_clock::now();
        }
        current_value = new_value;
        quality = new_quality;
        last_update = std::chrono::system_clock::now();
    }
};

// =============================================================================
// 🎯 유틸리티 함수들
// =============================================================================

inline std::string GenerateUUID() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
    
    static std::atomic<int> counter{0};
    int count = counter.fetch_add(1);
    
    return "dev_" + std::to_string(timestamp) + "_" + std::to_string(count);
}

inline ProtocolType StringToProtocolType(const std::string& protocol_str) {
    if (protocol_str == "modbus_tcp") return ProtocolType::MODBUS_TCP;
    if (protocol_str == "modbus_rtu") return ProtocolType::MODBUS_RTU;
    if (protocol_str == "mqtt") return ProtocolType::MQTT;
    if (protocol_str == "bacnet_ip") return ProtocolType::BACNET_IP;
    if (protocol_str == "bacnet_mstp") return ProtocolType::BACNET_MSTP;
    if (protocol_str == "opcua") return ProtocolType::OPCUA;
    if (protocol_str == "snmp") return ProtocolType::SNMP;
    if (protocol_str == "http_rest") return ProtocolType::HTTP_REST;
    if (protocol_str == "websocket") return ProtocolType::WEBSOCKET;
    return ProtocolType::CUSTOM;
}

inline std::chrono::system_clock::time_point GetCurrentTime() {
    return std::chrono::system_clock::now();
}

} // namespace New
} // namespace PulseOne

#endif // PULSEONE_NEW_TYPES_H
