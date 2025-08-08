// =============================================================================
// collector/include/Common/ProtocolConfigRegistry.h
// 🔥 컴파일 에러 완전 수정 버전 - 확장 가능한 프로토콜 설정 시스템
// =============================================================================

#ifndef PULSEONE_PROTOCOL_CONFIG_REGISTRY_H
#define PULSEONE_PROTOCOL_CONFIG_REGISTRY_H

#include "Enums.h"  // 🔥 핵심: ProtocolType을 위해 반드시 필요
#include <map>
#include <string>
#include <vector>
#include <functional>

namespace PulseOne {
namespace Config {

    // 🔥 중요: ProtocolType 별칭 명시적 선언
    using ProtocolType = PulseOne::Enums::ProtocolType;

    // =========================================================================
    // 🔥 프로토콜 파라미터 정의 구조체
    // =========================================================================
    
    struct ProtocolParameter {
        std::string key;
        std::string default_value;
        std::string type;                   // "string", "int", "bool", "double"
        std::string description;
        bool required = false;
        std::vector<std::string> valid_values;
        
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
        std::string endpoint_format = "";
        
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
        
        // 기본값 적용
        void ApplyDefaults(ProtocolType protocol, std::map<std::string, std::string>& properties) const {
            const auto* schema = GetSchema(protocol);
            if (!schema) return;
            
            for (const auto& param : schema->parameters) {
                if (properties.find(param.key) == properties.end()) {
                    properties[param.key] = param.default_value;
                }
            }
        }
        
        // 설정 검증
        bool ValidateProperties(ProtocolType protocol, 
                              const std::map<std::string, std::string>& properties,
                              std::vector<std::string>& errors) const {
            const auto* schema = GetSchema(protocol);
            if (!schema) {
                errors.push_back("Unknown protocol type");
                return false;
            }
            
            bool valid = true;
            
            // 필수 파라미터 체크
            for (const auto& param : schema->parameters) {
                if (param.required && properties.find(param.key) == properties.end()) {
                    errors.push_back("Required parameter missing: " + param.key);
                    valid = false;
                }
                
                // 타입 검증
                auto it = properties.find(param.key);
                if (it != properties.end()) {
                    if (!ValidateParameterType(it->second, param.type)) {
                        errors.push_back("Invalid type for parameter " + param.key + 
                                       " (expected: " + param.type + ")");
                        valid = false;
                    }
                }
            }
            
            return valid;
        }
        
        // 등록된 프로토콜 목록 조회
        std::vector<ProtocolType> GetRegisteredProtocols() const {
            std::vector<ProtocolType> protocols;
            for (const auto& [protocol, schema] : schemas_) {
                protocols.push_back(protocol);
            }
            return protocols;
        }
        
        // 초기화 (기본 프로토콜들 등록)
        void Initialize() {
            RegisterModbusProtocols();
            RegisterMqttProtocol();
            RegisterBacnetProtocol();
        }
        
    private:
        ProtocolConfigRegistry() {
            Initialize();
        }
        
        ~ProtocolConfigRegistry() = default;
        ProtocolConfigRegistry(const ProtocolConfigRegistry&) = delete;
        ProtocolConfigRegistry& operator=(const ProtocolConfigRegistry&) = delete;
        
        // 🔥 수정: 올바른 타입 선언
        std::map<ProtocolType, ProtocolSchema> schemas_;
        
        // 프로토콜별 등록 메서드들
        void RegisterModbusProtocols() {
            // Modbus TCP
            {
                ProtocolSchema schema(ProtocolType::MODBUS_TCP, "Modbus TCP", "Modbus over TCP/IP");
                schema.default_port = 502;
                schema.endpoint_format = "tcp://{host}:{port}";
                
                // 🔥 수정: 초기화 리스트 사용
                schema.parameters = {
                    ProtocolParameter("slave_id", "1", "int", "Modbus slave ID", true),
                    ProtocolParameter("timeout_ms", "3000", "int", "Connection timeout", false),
                    ProtocolParameter("unit_id", "1", "int", "Modbus unit ID", false),
                    ProtocolParameter("byte_order", "big_endian", "string", "Byte order", false),
                    ProtocolParameter("word_order", "high_low", "string", "Word order", false)
                };
                
                RegisterProtocol(schema);
            }
            
            // Modbus RTU
            {
                ProtocolSchema schema(ProtocolType::MODBUS_RTU, "Modbus RTU", "Modbus over serial");
                schema.default_port = 0;
                schema.endpoint_format = "{port}:{baud}:{parity}:{data}:{stop}";
                
                schema.parameters = {
                    ProtocolParameter("slave_id", "1", "int", "Modbus slave ID", true),
                    ProtocolParameter("baud_rate", "9600", "int", "Serial baud rate", true),
                    ProtocolParameter("parity", "N", "string", "Parity (N/E/O)", false),
                    ProtocolParameter("data_bits", "8", "int", "Data bits", false),
                    ProtocolParameter("stop_bits", "1", "int", "Stop bits", false)
                };
                
                RegisterProtocol(schema);
            }
        }
        
        void RegisterMqttProtocol() {
            ProtocolSchema schema(ProtocolType::MQTT, "MQTT", "Message Queuing Telemetry Transport");
            schema.default_port = 1883;
            schema.endpoint_format = "mqtt://{host}:{port}";
            
            schema.parameters = {
                ProtocolParameter("client_id", "", "string", "MQTT client ID", false),
                ProtocolParameter("username", "", "string", "Username", false),
                ProtocolParameter("password", "", "string", "Password", false),
                ProtocolParameter("qos", "1", "int", "Quality of Service", false),
                ProtocolParameter("clean_session", "true", "bool", "Clean session", false),
                ProtocolParameter("keep_alive", "60", "int", "Keep alive seconds", false)
            };
            
            RegisterProtocol(schema);
        }
        
        void RegisterBacnetProtocol() {
            ProtocolSchema schema(ProtocolType::BACNET_IP, "BACnet/IP", "Building Automation and Control Networks over IP");
            schema.default_port = 47808;
            schema.endpoint_format = "bacnet://{host}:{port}";
            
            schema.parameters = {
                ProtocolParameter("device_id", "260001", "int", "BACnet device instance", true),
                ProtocolParameter("network", "0", "int", "Network number", false),
                ProtocolParameter("max_apdu_length", "128", "int", "Maximum APDU length", false),
                ProtocolParameter("timeout_ms", "5000", "int", "Request timeout", false),
                ProtocolParameter("enable_cov", "false", "bool", "Enable COV subscriptions", false)
            };
            
            RegisterProtocol(schema);
        }
        
        // 파라미터 타입 검증
        bool ValidateParameterType(const std::string& value, const std::string& type) const {
            if (type == "int") {
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
            return true; // string은 항상 유효
        }
    };
    
    // =========================================================================
    // 🔥 편의 함수들 (전역 함수)
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