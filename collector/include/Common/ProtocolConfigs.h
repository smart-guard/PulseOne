#ifndef PULSEONE_PROTOCOL_CONFIGS_H
#define PULSEONE_PROTOCOL_CONFIGS_H

#include <string>
#include <memory>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "IProtocolConfig.h"

namespace PulseOne {
namespace Structs {
    
    /**
     * @brief Modbus 프로토콜 설정
     */
    class ModbusConfig : public IProtocolConfig {
    public:
        // 기존 필드명 100% 보존
        int slave_id = 1;
        int unit_id = 1;
        int max_registers_per_request = 125;
        bool use_rtu = false;
        bool byte_swap = false;
        bool word_swap = false;
        
        // RTU 전용
        int baud_rate = 9600;
        char parity = 'N';
        int data_bits = 8;
        int stop_bits = 1;
        std::string serial_port = "/dev/ttyUSB0";
        
        // IProtocolConfig 구현
        ProtocolType GetProtocol() const override {
            return use_rtu ? ProtocolType::MODBUS_RTU : ProtocolType::MODBUS_TCP;
        }
        
        std::unique_ptr<IProtocolConfig> Clone() const override {
            return std::make_unique<ModbusConfig>(*this);
        }
        
        bool IsValid() const override {
            return slave_id > 0 && slave_id <= 255;
        }
        
        std::string ToJson() const override {
            nlohmann::json j;
            j["slave_id"] = slave_id;
            j["unit_id"] = unit_id;
            j["max_registers_per_request"] = max_registers_per_request;
            j["use_rtu"] = use_rtu;
            j["byte_swap"] = byte_swap;
            j["word_swap"] = word_swap;
            j["baud_rate"] = baud_rate;
            j["parity"] = std::string(1, parity);
            j["data_bits"] = data_bits;
            j["stop_bits"] = stop_bits;
            j["serial_port"] = serial_port;
            return j.dump();
        }
        
        bool FromJson(const std::string& json) override {
            try {
                auto j = nlohmann::json::parse(json);
                slave_id = j.value("slave_id", 1);
                unit_id = j.value("unit_id", slave_id);
                max_registers_per_request = j.value("max_registers_per_request", 125);
                use_rtu = j.value("use_rtu", false);
                byte_swap = j.value("byte_swap", false);
                word_swap = j.value("word_swap", false);
                baud_rate = j.value("baud_rate", 9600);
                auto parity_str = j.value("parity", std::string("N"));
                parity = parity_str.empty() ? 'N' : parity_str[0];
                data_bits = j.value("data_bits", 8);
                stop_bits = j.value("stop_bits", 1);
                serial_port = j.value("serial_port", std::string("/dev/ttyUSB0"));
                return true;
            } catch (...) {
                return false;
            }
        }
    }; // ModbusConfig 끝
    
    /**
     * @brief MQTT 프로토콜 설정
     */
    class MqttConfig : public IProtocolConfig {
    public:
        // 기존 필드명 100% 보존
        std::string broker_url = "";
        std::string client_id = "";
        std::string username = "";
        std::string password = "";
        int qos = 1;
        bool clean_session = true;
        int keep_alive_interval = 60;
        int keepalive_s = 60;
        
        // SSL/TLS
        bool use_ssl = false;
        std::string ca_cert_path = "";
        std::string client_cert_path = "";
        std::string client_key_path = "";
        
        // 고급 설정
        bool auto_reconnect = true;
        int max_reconnect_attempts = 5;
        int reconnect_delay_ms = 1000;
        
        // IProtocolConfig 구현
        ProtocolType GetProtocol() const override {
            return ProtocolType::MQTT;
        }
        
        std::unique_ptr<IProtocolConfig> Clone() const override {
            return std::make_unique<MqttConfig>(*this);
        }
        
        bool IsValid() const override {
            return !broker_url.empty() && !client_id.empty();
        }
        
        std::string ToJson() const override {
            nlohmann::json j;
            j["broker_url"] = broker_url;
            j["client_id"] = client_id;
            j["username"] = username;
            j["password"] = password;
            j["qos"] = qos;
            j["clean_session"] = clean_session;
            j["keep_alive_interval"] = keep_alive_interval;
            j["use_ssl"] = use_ssl;
            j["ca_cert_path"] = ca_cert_path;
            j["auto_reconnect"] = auto_reconnect;
            j["max_reconnect_attempts"] = max_reconnect_attempts;
            j["reconnect_delay_ms"] = reconnect_delay_ms;
            return j.dump();
        }
        
        bool FromJson(const std::string& json) override {
            try {
                auto j = nlohmann::json::parse(json);
                broker_url = j.value("broker_url", std::string(""));
                client_id = j.value("client_id", std::string(""));
                username = j.value("username", std::string(""));
                password = j.value("password", std::string(""));
                qos = j.value("qos", 1);
                clean_session = j.value("clean_session", true);
                keep_alive_interval = j.value("keep_alive_interval", 60);
                keepalive_s = keep_alive_interval;
                use_ssl = j.value("use_ssl", false);
                ca_cert_path = j.value("ca_cert_path", std::string(""));
                auto_reconnect = j.value("auto_reconnect", true);
                max_reconnect_attempts = j.value("max_reconnect_attempts", 5);
                reconnect_delay_ms = j.value("reconnect_delay_ms", 1000);
                return true;
            } catch (...) {
                return false;
            }
        }
    }; // MqttConfig 끝
    
    /**
     * @brief BACnet 프로토콜 설정
     */
    class BACnetConfig : public IProtocolConfig {
    public:
        // 기존 필드명 100% 보존
        uint32_t device_id = 1000;
        uint32_t device_instance = 1000;
        uint16_t port = 47808;
        uint16_t bacnet_port = 47808;
        std::string interface_name = "";
        
        // 서비스 지원
        bool use_cov = true;
        bool support_cov = true;
        int cov_lifetime = 3600;
        bool support_who_is = true;
        bool support_read_property_multiple = true;
        
        // BBMD 설정
        bool use_bbmd = false;
        std::string bbmd_address = "";
        uint16_t bbmd_port = 47808;
        
        // IProtocolConfig 구현
        ProtocolType GetProtocol() const override {
            return ProtocolType::BACNET;
        }
        
        std::unique_ptr<IProtocolConfig> Clone() const override {
            return std::make_unique<BACnetConfig>(*this);
        }
        
        bool IsValid() const override {
            return device_id > 0 && device_id <= 4194303;
        }
        
        std::string ToJson() const override {
            nlohmann::json j;
            j["device_id"] = device_id;
            j["device_instance"] = device_instance;
            j["port"] = port;
            j["bacnet_port"] = bacnet_port;
            j["interface_name"] = interface_name;
            j["use_cov"] = use_cov;
            j["support_cov"] = support_cov;
            j["cov_lifetime"] = cov_lifetime;
            j["support_who_is"] = support_who_is;
            j["support_read_property_multiple"] = support_read_property_multiple;
            j["use_bbmd"] = use_bbmd;
            j["bbmd_address"] = bbmd_address;
            j["bbmd_port"] = bbmd_port;
            return j.dump();
        }
        
        bool FromJson(const std::string& json) override {
            try {
                auto j = nlohmann::json::parse(json);
                device_id = j.value("device_id", 1000U);
                device_instance = j.value("device_instance", device_id);
                port = j.value("port", static_cast<uint16_t>(47808));
                bacnet_port = j.value("bacnet_port", port);
                interface_name = j.value("interface_name", std::string(""));
                use_cov = j.value("use_cov", true);
                support_cov = j.value("support_cov", use_cov);
                cov_lifetime = j.value("cov_lifetime", 3600);
                support_who_is = j.value("support_who_is", true);
                support_read_property_multiple = j.value("support_read_property_multiple", true);
                use_bbmd = j.value("use_bbmd", false);
                bbmd_address = j.value("bbmd_address", std::string(""));
                bbmd_port = j.value("bbmd_port", static_cast<uint16_t>(47808));
                return true;
            } catch (...) {
                return false;
            }
        }
    }; // BACnetConfig 끝

} // namespace Structs
} // namespace PulseOne

#endif // PULSEONE_PROTOCOL_CONFIGS_H