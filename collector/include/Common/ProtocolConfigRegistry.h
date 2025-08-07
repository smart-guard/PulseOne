// =============================================================================
// collector/include/Common/ProtocolConfigRegistry.h
// 🎯 확장 가능한 프로토콜 설정 시스템 - 설정 기반 접근
// =============================================================================

#ifndef PULSEONE_PROTOCOL_CONFIG_REGISTRY_H
#define PULSEONE_PROTOCOL_CONFIG_REGISTRY_H

#include "Enums.h"
#include <map>
#include <string>
#include <vector>
#include <functional>

namespace PulseOne {
namespace Config {

    // =========================================================================
    // 🔥 프로토콜 파라미터 정의 구조체
    // =========================================================================
    
    struct ProtocolParameter {
        std::string key;                    // 파라미터 키
        std::string default_value;          // 기본값
        std::string type;                   // "string", "int", "bool", "double"
        std::string description;            // 설명
        bool required = false;              // 필수 여부
        std::vector<std::string> valid_values; // 유효한 값들 (선택사항)
        
        ProtocolParameter() = default;
        
        ProtocolParameter(const std::string& k, const std::string& def_val, 
                         const std::string& t, const std::string& desc, bool req = false)
            : key(k), default_value(def_val), type(t), description(desc), required(req) {}
    };
    
    // =========================================================================
    // 🔥 프로토콜 설정 스키마
    // =========================================================================
    
    struct ProtocolSchema {
        ProtocolType protocol_type;
        std::string name;
        std::string description;
        std::vector<ProtocolParameter> parameters;
        int default_port = 0;
        std::string endpoint_format = "";   // 예: "tcp://{host}:{port}"
        
        ProtocolSchema() = default;
        
        ProtocolSchema(ProtocolType type, const std::string& n, const std::string& desc)
            : protocol_type(type), name(n), description(desc) {}
    };
    
    // =========================================================================
    // 🔥 프로토콜 설정 레지스트리 (싱글톤)
    // =========================================================================
    
    class ProtocolConfigRegistry {
    public:
        static ProtocolConfigRegistry& getInstance() {
            static ProtocolConfigRegistry instance;
            return instance;
        }
        
        // 프로토콜 스키마 등록
        void RegisterProtocol(const ProtocolSchema& schema) {
            schemas_[schema.protocol_type] = schema;
        }
        
        // 프로토콜 스키마 조회
        const ProtocolSchema* GetSchema(ProtocolType protocol) const {
            auto it = schemas_.find(protocol);
            return (it != schemas_.end()) ? &it->second : nullptr;
        }
        
        // 기본값들을 properties 맵에 적용
        void ApplyDefaults(ProtocolType protocol, std::map<std::string, std::string>& properties) const {
            const auto* schema = GetSchema(protocol);
            if (!schema) return;
            
            for (const auto& param : schema->parameters) {
                // 기존에 값이 없는 경우에만 기본값 설정
                if (properties.find(param.key) == properties.end()) {
                    properties[param.key] = param.default_value;
                }
            }
        }
        
        // 값 검증
        bool ValidateProperties(ProtocolType protocol, 
                               const std::map<std::string, std::string>& properties,
                               std::vector<std::string>& errors) const {
            const auto* schema = GetSchema(protocol);
            if (!schema) {
                errors.push_back("Unknown protocol type");
                return false;
            }
            
            bool is_valid = true;
            
            for (const auto& param : schema->parameters) {
                auto it = properties.find(param.key);
                
                // 필수 파라미터 체크
                if (param.required && it == properties.end()) {
                    errors.push_back("Missing required parameter: " + param.key);
                    is_valid = false;
                    continue;
                }
                
                if (it != properties.end()) {
                    const std::string& value = it->second;
                    
                    // 타입 검증
                    if (!ValidateType(value, param.type)) {
                        errors.push_back("Invalid type for " + param.key + 
                                       " (expected " + param.type + ")");
                        is_valid = false;
                    }
                    
                    // 유효값 검증
                    if (!param.valid_values.empty()) {
                        if (std::find(param.valid_values.begin(), param.valid_values.end(), value) 
                            == param.valid_values.end()) {
                            errors.push_back("Invalid value for " + param.key + ": " + value);
                            is_valid = false;
                        }
                    }
                }
            }
            
            return is_valid;
        }
        
        // 모든 등록된 프로토콜 목록
        std::vector<ProtocolType> GetRegisteredProtocols() const {
            std::vector<ProtocolType> protocols;
            for (const auto& [protocol, schema] : schemas_) {
                protocols.push_back(protocol);
            }
            return protocols;
        }
        
    private:
        std::map<ProtocolType, ProtocolSchema> schemas_;
        
        ProtocolConfigRegistry() {
            RegisterBuiltinProtocols();
        }
        
        // 기본 프로토콜들 등록
        void RegisterBuiltinProtocols() {
            RegisterModbusProtocols();
            RegisterMqttProtocol();
            RegisterBacnetProtocol();
        }
        
        void RegisterModbusProtocols() {
            // Modbus TCP
            {
                ProtocolSchema schema(ProtocolType::MODBUS_TCP, "Modbus TCP", "Modbus over TCP/IP");
                schema.default_port = 502;
                schema.endpoint_format = "tcp://{host}:{port}";
                
                schema.parameters = {
                    {"slave_id", "1", "int", "Modbus slave ID (1-247)", true},
                    {"function_code", "3", "int", "Default function code", false},
                    {"byte_order", "big_endian", "string", "Byte order", false, {"big_endian", "little_endian"}},
                    {"word_order", "high_low", "string", "Word order", false, {"high_low", "low_high"}},
                    {"register_type", "HOLDING_REGISTER", "string", "Default register type", false, 
                     {"COIL", "DISCRETE_INPUT", "INPUT_REGISTER", "HOLDING_REGISTER"}},
                    {"max_registers_per_group", "125", "int", "Max registers per read", false},
                    {"auto_group_creation", "true", "bool", "Enable automatic grouping", false}
                };
                
                RegisterProtocol(schema);
            }
            
            // Modbus RTU
            {
                ProtocolSchema schema(ProtocolType::MODBUS_RTU, "Modbus RTU", "Modbus over serial");
                schema.endpoint_format = "serial://{port}:{baudrate}:{parity}:{data_bits}:{stop_bits}";
                
                schema.parameters = {
                    {"slave_id", "1", "int", "Modbus slave ID (1-247)", true},
                    {"function_code", "3", "int", "Default function code", false},
                    {"byte_order", "big_endian", "string", "Byte order", false, {"big_endian", "little_endian"}},
                    {"word_order", "high_low", "string", "Word order", false, {"high_low", "low_high"}},
                    {"baudrate", "9600", "int", "Serial baudrate", false, {"9600", "19200", "38400", "57600", "115200"}},
                    {"parity", "N", "string", "Parity", false, {"N", "E", "O"}},
                    {"data_bits", "8", "int", "Data bits", false, {"7", "8"}},
                    {"stop_bits", "1", "int", "Stop bits", false, {"1", "2"}}
                };
                
                RegisterProtocol(schema);
            }
        }
        
        void RegisterMqttProtocol() {
            ProtocolSchema schema(ProtocolType::MQTT, "MQTT", "Message Queuing Telemetry Transport");
            schema.default_port = 1883;
            schema.endpoint_format = "mqtt://{host}:{port}";
            
            schema.parameters = {
                {"client_id", "", "string", "MQTT client ID", false},
                {"username", "", "string", "Username for authentication", false},
                {"password", "", "string", "Password for authentication", false},
                {"qos_level", "1", "int", "Quality of Service level", false, {"0", "1", "2"}},
                {"clean_session", "true", "bool", "Clean session flag", false},
                {"retain", "false", "bool", "Retain messages", false},
                {"keep_alive", "60", "int", "Keep alive interval (seconds)", false},
                {"ssl_enabled", "false", "bool", "Enable SSL/TLS", false},
                {"ssl_ca_file", "", "string", "CA certificate file", false},
                {"ssl_cert_file", "", "string", "Client certificate file", false},
                {"ssl_key_file", "", "string", "Client key file", false}
            };
            
            RegisterProtocol(schema);
        }
        
        void RegisterBacnetProtocol() {
            ProtocolSchema schema(ProtocolType::BACNET_IP, "BACnet/IP", "Building Automation and Control Networks over IP");
            schema.default_port = 47808;
            schema.endpoint_format = "bacnet://{host}:{port}";
            
            schema.parameters = {
                {"device_id", "1001", "int", "BACnet device ID", true},
                {"network_number", "1", "int", "Network number", false},
                {"max_apdu_length", "1476", "int", "Maximum APDU length", false},
                {"segmentation_support", "both", "string", "Segmentation support", false, 
                 {"no", "receive", "send", "both"}},
                {"vendor_id", "0", "int", "Vendor ID", false},
                {"max_info_frames", "1", "int", "Max info frames", false},
                {"max_master", "127", "int", "Max master address", false},
                {"discovery_timeout", "30", "int", "Discovery timeout (seconds)", false}
            };
            
            RegisterProtocol(schema);
        }
        
        bool ValidateType(const std::string& value, const std::string& type) const {
            if (type == "string") {
                return true;  // 모든 문자열 허용
            } else if (type == "int") {
                try {
                    std::stoi(value);
                    return true;
                } catch (...) {
                    return false;
                }
            } else if (type == "bool") {
                return (value == "true" || value == "false" || 
                       value == "1" || value == "0" || 
                       value == "yes" || value == "no");
            } else if (type == "double") {
                try {
                    std::stod(value);
                    return true;
                } catch (...) {
                    return false;
                }
            }
            return false;
        }
    };
    
    // =========================================================================
    // 🔥 편의 함수들
    // =========================================================================
    
    /**
     * @brief 프로토콜 기본값 적용
     */
    inline void ApplyProtocolDefaults(ProtocolType protocol, std::map<std::string, std::string>& properties) {
        ProtocolConfigRegistry::getInstance().ApplyDefaults(protocol, properties);
    }
    
    /**
     * @brief 프로토콜 설정 검증
     */
    inline bool ValidateProtocolConfig(ProtocolType protocol, 
                                      const std::map<std::string, std::string>& properties,
                                      std::vector<std::string>& errors) {
        return ProtocolConfigRegistry::getInstance().ValidateProperties(protocol, properties, errors);
    }
    
    /**
     * @brief 프로토콜 파라미터 정보 조회
     */
    inline const ProtocolSchema* GetProtocolSchema(ProtocolType protocol) {
        return ProtocolConfigRegistry::getInstance().GetSchema(protocol);
    }

} // namespace Config
} // namespace PulseOne

#endif // PULSEONE_PROTOCOL_CONFIG_REGISTRY_H

// =============================================================================
// 🔥 DeviceInfo 구조체 수정 버전 (설정 기반)
// =============================================================================

// DeviceInfo 구조체에서 InitializeProtocolDefaults() 대신:
/**
 * @brief 프로토콜별 기본 properties 설정 (설정 기반)
 */
void InitializeProtocolDefaults(ProtocolType protocol) {
    // 간단한 한 줄로 해결!
    PulseOne::Config::ApplyProtocolDefaults(protocol, properties);
}

// 프로토콜별 편의 메서드들도 동적 생성 가능:
/**
 * @brief 동적 파라미터 접근
 */
std::string GetProtocolParam(const std::string& key) const {
    const auto* schema = PulseOne::Config::GetProtocolSchema(driver_config.protocol);
    if (!schema) return "";
    
    // 스키마에서 기본값 찾기
    for (const auto& param : schema->parameters) {
        if (param.key == key) {
            return GetProperty(key, param.default_value);
        }
    }
    return GetProperty(key, "");
}

/**
 * @brief 설정 검증
 */
bool ValidateConfig(std::vector<std::string>& errors) const {
    return PulseOne::Config::ValidateProtocolConfig(driver_config.protocol, properties, errors);
}