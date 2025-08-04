// collector/include/Common/UnifiedTypes.h
#ifndef PULSEONE_COMMON_UNIFIED_TYPES_H
#define PULSEONE_COMMON_UNIFIED_TYPES_H

/**
 * @file UnifiedTypes.h
 * @brief PulseOne 통합 데이터 구조체 시스템 (핵심!)
 * @author PulseOne Development Team
 * @date 2025-08-04
 * @details
 * 🎯 목적: **7개 중복 구조체를 1개로 통합하는 핵심 파일**
 * 📋 기존 중복 구조체들:
 * 1. Structs::DeviceInfo (기존 C++ 내부)
 * 2. DeviceEntity (데이터베이스 ORM) 
 * 3. BACnetDeviceInfo (BACnet 특화)
 * 4. ModbusDeviceInfo (Modbus 특화)
 * 5. MQTTDeviceInfo (MQTT 특화)
 * 6. ModbusTcpDeviceInfo (Worker 특화)
 * 7. ModbusRtuSlaveInfo (Worker 특화)
 * 
 * ✅ 해결책: UnifiedDeviceInfo 하나로 모두 대체!
 */

#include "BasicTypes.h"
#include "Enums.h"
#include "Constants.h"
#include <string>
#include <variant>
#include <chrono>
#include <vector>
#include <map>
#include <optional>
#include <cassert>

// 조건부 JSON 라이브러리
#ifdef HAS_NLOHMANN_JSON
    #include <nlohmann/json.hpp>
    namespace json_impl = nlohmann;
#else
    namespace json_impl {
        class json {
        public:
            json() = default;
            std::string dump() const { return "{}"; }
            bool empty() const { return true; }
        };
    }
#endif

namespace PulseOne {
namespace Common {

    // 기본 타입들 import
    using namespace PulseOne::BasicTypes;
    using namespace PulseOne::Enums;
    using JsonType = json_impl::json;

    // =========================================================================
    // 🔥 핵심! 통합 디바이스 정보 구조체 (7개 구조체 → 1개)
    // =========================================================================
    
    /**
     * @brief 통합 디바이스 정보 (모든 중복 구조체 통합!)
     * @details 
     * ✅ 모든 프로토콜을 지원하는 단일 구조체
     * ✅ Union 방식으로 메모리 효율적
     * ✅ 타입 안전한 접근자 제공
     * ✅ 기존 Entity와 완벽 호환
     */
    struct UnifiedDeviceInfo {
        // ===== 공통 필드들 (모든 프로토콜 공통) =====
        UUID id;                               // 디바이스 고유 ID
        std::string name;                      // 디바이스 이름
        std::string description;               // 설명
        ProtocolType protocol;                 // 프로토콜 타입
        std::string endpoint;                  // 연결 정보 (IP:Port, URL 등)
        bool enabled = true;                   // 활성화 상태
        
        // 공통 타이밍 설정
        int polling_interval_ms = Constants::DEFAULT_POLLING_INTERVAL_MS;
        int timeout_ms = Constants::DEFAULT_TIMEOUT_MS;
        int max_retry_count = 3;
        
        // 메타데이터
        Timestamp created_at = CurrentTimestamp();
        Timestamp updated_at = CurrentTimestamp();
        std::string tags;                      // JSON 배열 형태의 태그들
        
        // ===== 프로토콜별 특화 설정 (Union - 메모리 효율적!) =====
        union ProtocolConfig {
            struct ModbusConfig {
                int slave_id = Constants::DEFAULT_MODBUS_SLAVE_ID;
                int max_registers_per_request = Constants::DEFAULT_MODBUS_MAX_REGISTERS;
                bool use_rtu = false;          // false: TCP, true: RTU
                int baudrate = Constants::DEFAULT_MODBUS_RTU_BAUDRATE;  // RTU용
                char parity = 'N';             // RTU용 패리티 (N/E/O)
                int stop_bits = 1;             // RTU용 스톱비트
                int data_bits = 8;             // RTU용 데이터비트
                std::string serial_port;       // RTU용 시리얼 포트
                
                ModbusConfig() = default;
            } modbus;
            
            struct MqttConfig {
                std::string client_id;
                std::string username;
                std::string password;
                int qos = Constants::DEFAULT_MQTT_QOS;
                bool clean_session = true;
                int keepalive_s = Constants::DEFAULT_MQTT_KEEPALIVE_S;
                bool use_ssl = false;
                std::string ca_cert_file;      // SSL 인증서 파일
                std::string cert_file;         // 클라이언트 인증서
                std::string key_file;          // 클라이언트 키
                std::map<std::string, std::string> will_message;  // Last Will 메시지
                
                MqttConfig() { client_id = "pulseone_" + GenerateUUID().substr(0, 8); }
            } mqtt;
            
            struct BacnetConfig {
                uint32_t device_id = Constants::DEFAULT_BACNET_DEVICE_ID;
                uint16_t port = Constants::DEFAULT_BACNET_PORT;
                uint32_t max_apdu = Constants::DEFAULT_BACNET_MAX_APDU;
                bool support_cov = true;       // Change of Value 지원
                bool support_who_is = true;    // Who-Is/I-Am 지원
                int max_segments = 0;          // 0 = no segmentation
                bool support_read_property_multiple = true;
                std::string bbmd_address;      // BBMD 주소 (옵션)
                int bbmd_port = 0;            // BBMD 포트
                
                BacnetConfig() = default;
            } bacnet;
            
            // 향후 확장용 프로토콜들
            struct OpcUaConfig {
                std::string endpoint_url;
                std::string security_policy;   // None, Basic128Rsa15, Basic256, etc.
                std::string security_mode;     // None, Sign, SignAndEncrypt
                std::string username;
                std::string password;
                std::string cert_file;
                std::string key_file;
                int session_timeout_ms = 0;
                int publish_interval_ms = 1000;
                
                OpcUaConfig() = default;
            } opc_ua;
            
            // 기본 생성자 (Modbus로 초기화)
            ProtocolConfig() : modbus() {}
            
            // 프로토콜별 생성자들
            explicit ProtocolConfig(const ModbusConfig& cfg) : modbus(cfg) {}
            explicit ProtocolConfig(const MqttConfig& cfg) : mqtt(cfg) {}
            explicit ProtocolConfig(const BacnetConfig& cfg) : bacnet(cfg) {}
            explicit ProtocolConfig(const OpcUaConfig& cfg) : opc_ua(cfg) {}
        } config;
        
        // ===== 타입 안전 접근자들 =====
        bool IsModbus() const { 
            return protocol == ProtocolType::MODBUS_TCP || protocol == ProtocolType::MODBUS_RTU; 
        }
        bool IsMqtt() const { 
            return protocol == ProtocolType::MQTT || protocol == ProtocolType::MQTT_5; 
        }
        bool IsBacnet() const { 
            return protocol == ProtocolType::BACNET_IP || protocol == ProtocolType::BACNET_MSTP; 
        }
        bool IsOpcUa() const { 
            return protocol == ProtocolType::OPC_UA; 
        }
        
        // 읽기 전용 접근자들
        const auto& GetModbusConfig() const { 
            assert(IsModbus()); 
            return config.modbus; 
        }
        const auto& GetMqttConfig() const { 
            assert(IsMqtt()); 
            return config.mqtt; 
        }
        const auto& GetBacnetConfig() const { 
            assert(IsBacnet()); 
            return config.bacnet; 
        }
        const auto& GetOpcUaConfig() const { 
            assert(IsOpcUa()); 
            return config.opc_ua; 
        }
        
        // 쓰기 가능 접근자들
        auto& GetModbusConfig() { 
            assert(IsModbus()); 
            return config.modbus; 
        }
        auto& GetMqttConfig() { 
            assert(IsMqtt()); 
            return config.mqtt; 
        }
        auto& GetBacnetConfig() { 
            assert(IsBacnet()); 
            return config.bacnet; 
        }
        auto& GetOpcUaConfig() { 
            assert(IsOpcUa()); 
            return config.opc_ua; 
        }
        
        // ===== 변환 메서드들 (기존 Entity와 호환) =====
        
        /**
         * @brief DeviceEntity에서 UnifiedDeviceInfo로 변환
         * @param entity 데이터베이스 엔티티
         * @return 통합 디바이스 정보
         */
        static UnifiedDeviceInfo FromEntity(const class DeviceEntity& entity);
        
        /**
         * @brief UnifiedDeviceInfo를 DeviceEntity로 변환
         * @return 데이터베이스 엔티티
         */
        class DeviceEntity ToEntity() const;
        
        /**
         * @brief JSON 직렬화
         * @return JSON 문자열
         */
        std::string ToJson() const {
            JsonType j;
            j["id"] = id;
            j["name"] = name;
            j["description"] = description;
            j["protocol"] = static_cast<int>(protocol);
            j["endpoint"] = endpoint;
            j["enabled"] = enabled;
            j["polling_interval_ms"] = polling_interval_ms;
            j["timeout_ms"] = timeout_ms;
            j["max_retry_count"] = max_retry_count;
            j["tags"] = tags;
            
            // 프로토콜별 설정 직렬화
            if (IsModbus()) {
                j["modbus_config"] = {
                    {"slave_id", config.modbus.slave_id},
                    {"max_registers", config.modbus.max_registers_per_request},
                    {"use_rtu", config.modbus.use_rtu},
                    {"baudrate", config.modbus.baudrate},
                    {"parity", std::string(1, config.modbus.parity)},
                    {"stop_bits", config.modbus.stop_bits},
                    {"data_bits", config.modbus.data_bits},
                    {"serial_port", config.modbus.serial_port}
                };
            } else if (IsMqtt()) {
                j["mqtt_config"] = {
                    {"client_id", config.mqtt.client_id},
                    {"username", config.mqtt.username},
                    {"qos", config.mqtt.qos},
                    {"clean_session", config.mqtt.clean_session},
                    {"keepalive_s", config.mqtt.keepalive_s},
                    {"use_ssl", config.mqtt.use_ssl}
                };
            } else if (IsBacnet()) {
                j["bacnet_config"] = {
                    {"device_id", config.bacnet.device_id},
                    {"port", config.bacnet.port},
                    {"max_apdu", config.bacnet.max_apdu},
                    {"support_cov", config.bacnet.support_cov},
                    {"support_who_is", config.bacnet.support_who_is}
                };
            }
            
            return j.dump();
        }
        
        /**
         * @brief JSON 역직렬화
         * @param json_str JSON 문자열
         * @return 통합 디바이스 정보
         */
        static UnifiedDeviceInfo FromJson(const std::string& json_str) {
            JsonType j = JsonType::parse(json_str);
            
            UnifiedDeviceInfo info;
            info.id = j.value("id", GenerateUUID());
            info.name = j.value("name", "");
            info.description = j.value("description", "");
            info.protocol = static_cast<ProtocolType>(j.value("protocol", 0));
            info.endpoint = j.value("endpoint", "");
            info.enabled = j.value("enabled", true);
            info.polling_interval_ms = j.value("polling_interval_ms", Constants::DEFAULT_POLLING_INTERVAL_MS);
            info.timeout_ms = j.value("timeout_ms", Constants::DEFAULT_TIMEOUT_MS);
            info.max_retry_count = j.value("max_retry_count", 3);
            info.tags = j.value("tags", "");
            
            // 프로토콜별 설정 역직렬화
            if (info.IsModbus() && j.contains("modbus_config")) {
                auto& mc = j["modbus_config"];
                info.config.modbus.slave_id = mc.value("slave_id", Constants::DEFAULT_MODBUS_SLAVE_ID);
                info.config.modbus.max_registers_per_request = mc.value("max_registers", Constants::DEFAULT_MODBUS_MAX_REGISTERS);
                info.config.modbus.use_rtu = mc.value("use_rtu", false);
                info.config.modbus.baudrate = mc.value("baudrate", Constants::DEFAULT_MODBUS_RTU_BAUDRATE);
                std::string parity = mc.value("parity", "N");
                info.config.modbus.parity = parity.empty() ? 'N' : parity[0];
                info.config.modbus.stop_bits = mc.value("stop_bits", 1);
                info.config.modbus.data_bits = mc.value("data_bits", 8);
                info.config.modbus.serial_port = mc.value("serial_port", "");
            } else if (info.IsMqtt() && j.contains("mqtt_config")) {
                auto& mc = j["mqtt_config"];
                info.config.mqtt.client_id = mc.value("client_id", "pulseone_" + GenerateUUID().substr(0, 8));
                info.config.mqtt.username = mc.value("username", "");
                info.config.mqtt.qos = mc.value("qos", Constants::DEFAULT_MQTT_QOS);
                info.config.mqtt.clean_session = mc.value("clean_session", true);
                info.config.mqtt.keepalive_s = mc.value("keepalive_s", Constants::DEFAULT_MQTT_KEEPALIVE_S);
                info.config.mqtt.use_ssl = mc.value("use_ssl", false);
            } else if (info.IsBacnet() && j.contains("bacnet_config")) {
                auto& bc = j["bacnet_config"];
                info.config.bacnet.device_id = bc.value("device_id", Constants::DEFAULT_BACNET_DEVICE_ID);
                info.config.bacnet.port = bc.value("port", Constants::DEFAULT_BACNET_PORT);
                info.config.bacnet.max_apdu = bc.value("max_apdu", Constants::DEFAULT_BACNET_MAX_APDU);
                info.config.bacnet.support_cov = bc.value("support_cov", true);
                info.config.bacnet.support_who_is = bc.value("support_who_is", true);
            }
            
            return info;
        }
    };

    // =========================================================================
    // 🔥 통합 데이터 포인트 구조체 (기존 여러 DataPoint 통합)
    // =========================================================================
    
    /**
     * @brief 통합 데이터 포인트 정보
     * @details 모든 프로토콜에서 사용하는 통합 데이터 포인트
     */
    struct UnifiedDataPoint {
        UUID id;                              // 포인트 고유 ID
        UUID device_id;                       // 소속 디바이스 ID
        std::string name;                     // 포인트 이름
        std::string description;              // 설명
        
        // ===== 주소 정보 (프로토콜별) =====
        union Address {
            uint32_t numeric;                 // Modbus: 레지스터, BACnet: 인스턴스
            char string[512];                 // MQTT: 토픽, OPC-UA: NodeId
            
            Address() : numeric(0) {}
            Address(uint32_t addr) : numeric(addr) {}
            Address(const std::string& str) {
                std::strncpy(string, str.c_str(), sizeof(string) - 1);
                string[sizeof(string) - 1] = '\0';
            }
            
            std::string ToString() const {
                return std::string(string);
            }
        } address;
        
        // ===== 데이터 타입 및 단위 =====
        DataType data_type = DataType::UNKNOWN;
        std::string unit;                     // 단위 (°C, %, kW 등)
        AccessType access_type = AccessType::READ_ONLY;
        
        // 스케일링 정보
        double scaling_factor = 1.0;
        double scaling_offset = 0.0;
        std::optional<double> min_value;
        std::optional<double> max_value;
        
        // ===== 현재 값 및 상태 =====
        DataVariant current_value;           // 현재 값
        DataQuality quality = DataQuality::UNKNOWN;
        Timestamp last_update = CurrentTimestamp();
        Timestamp last_good_update = CurrentTimestamp();
        
        // 설정
        bool enabled = true;
        int scan_rate_ms = 0;                // 0이면 디바이스 기본값 사용
        double deadband = 0.0;               // 데드밴드 값
        
        // 메타데이터
        std::string tags;                    // JSON 배열 형태
        Timestamp created_at = CurrentTimestamp();
        Timestamp updated_at = CurrentTimestamp();
        
        // ===== 유틸리티 메서드들 =====
        
        /**
         * @brief 값 업데이트 (품질 상태와 함께)
         */
        void UpdateValue(const DataVariant& new_value, DataQuality new_quality = DataQuality::GOOD) {
            current_value = new_value;
            quality = new_quality;
            last_update = CurrentTimestamp();
            updated_at = last_update;
            
            if (new_quality == DataQuality::GOOD) {
                last_good_update = last_update;
            }
        }
        
        /**
         * @brief 스케일링 적용된 값 반환
         */
        DataVariant GetScaledValue() const {
            return std::visit([this](const auto& val) -> DataVariant {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
                    return static_cast<double>(val) * scaling_factor + scaling_offset;
                } else {
                    return val;  // 숫자가 아니면 그대로 반환
                }
            }, current_value);
        }
        
        /**
         * @brief 데이터가 유효한지 확인
         */
        bool IsValid() const {
            return quality == DataQuality::GOOD || quality == DataQuality::UNCERTAIN;
        }
        
        /**
         * @brief 데이터가 오래되었는지 확인
         */
        bool IsStale(int threshold_ms = Constants::DATA_STALE_THRESHOLD_MS) const {
            auto now = CurrentTimestamp();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update);
            return elapsed.count() > threshold_ms;
        }
        
        /**
         * @brief JSON 직렬화
         */
        std::string ToJson() const {
            JsonType j;
            j["id"] = id;
            j["device_id"] = device_id;
            j["name"] = name;
            j["description"] = description;
            j["data_type"] = static_cast<int>(data_type);
            j["unit"] = unit;
            j["access_type"] = static_cast<int>(access_type);
            j["scaling_factor"] = scaling_factor;
            j["scaling_offset"] = scaling_offset;
            j["enabled"] = enabled;
            j["scan_rate_ms"] = scan_rate_ms;
            j["deadband"] = deadband;
            j["tags"] = tags;
            
            // 주소 정보 (타입에 따라)
            if (data_type == DataType::STRING || data_type == DataType::JSON) {
                j["address"] = address.ToString();
            } else {
                j["address"] = address.numeric;
            }
            
            // 현재 값 직렬화
            std::visit([&j](const auto& val) {
                j["current_value"] = val;
            }, current_value);
            
            j["quality"] = static_cast<int>(quality);
            j["last_update"] = TimestampToString(last_update);
            j["last_good_update"] = TimestampToString(last_good_update);
            
            return j.dump();
        }
        
        /**
         * @brief 기존 Entity와 호환 변환
         */
        static UnifiedDataPoint FromEntity(const class DataPointEntity& entity);
        class DataPointEntity ToEntity() const;
    };

    // =========================================================================
    // 🔥 타임스탬프가 포함된 값 구조체
    // =========================================================================
    
    /**
     * @brief 타임스탬프가 포함된 데이터 값
     * @details 실시간 데이터 전송 시 사용
     */
    struct TimestampedValue {
        UUID point_id;                       // 데이터 포인트 ID
        DataVariant value;                   // 값
        DataQuality quality = DataQuality::GOOD;
        Timestamp timestamp = CurrentTimestamp();
        
        // 생성자들
        TimestampedValue() = default;
        TimestampedValue(const UUID& pid, const DataVariant& val, 
                        DataQuality qual = DataQuality::GOOD)
            : point_id(pid), value(val), quality(qual) {}
        
        /**
         * @brief 값이 유효한지 확인
         */
        bool IsValid() const {
            return quality == DataQuality::GOOD || quality == DataQuality::UNCERTAIN;
        }
        
        /**
         * @brief 압축된 JSON 직렬화 (전송 최적화)
         */
        std::string ToCompactJson() const {
            JsonType j;
            j["p"] = point_id;                // point_id 압축
            std::visit([&j](const auto& val) {
                j["v"] = val;                 // value 압축
            }, value);
            j["q"] = static_cast<int>(quality); // quality 압축
            j["t"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count(); // timestamp 압축
            return j.dump();
        }
    };

    // =========================================================================
    // 🔥 메시지 전송용 구조체들
    // =========================================================================
    
    /**
     * @brief 디바이스 데이터 메시지 (RabbitMQ, Redis 전송용)
     */
    struct DeviceDataMessage {
        std::string type = "device_data";
        UUID device_id;
        std::string protocol;
        std::vector<TimestampedValue> points;
        Timestamp timestamp = CurrentTimestamp();
        
        /**
         * @brief 통합 디바이스와 포인트들을 메시지로 변환
         */
        static std::string ToJson(const UnifiedDeviceInfo& device, 
                                 const std::vector<UnifiedDataPoint>& points) {
            JsonType j;
            j["type"] = "device_data";
            j["device_id"] = device.id;
            j["protocol"] = static_cast<int>(device.protocol);
            j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                CurrentTimestamp().time_since_epoch()).count();
            
            j["points"] = JsonType::array();
            for (const auto& point : points) {
                JsonType point_json;
                point_json["id"] = point.id;
                point_json["name"] = point.name;
                std::visit([&point_json](const auto& val) {
                    point_json["value"] = val;
                }, point.current_value);
                point_json["quality"] = static_cast<int>(point.quality);
                point_json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    point.last_update.time_since_epoch()).count();
                j["points"].push_back(point_json);
            }
            
            return j.dump();
        }
    };

    // =========================================================================
    // 🔥 기존 코드 호환성을 위한 별칭들
    // =========================================================================
    
    namespace Compatibility {
        // 기존 코드가 계속 작동하도록 하는 별칭들
        using DeviceInfo = UnifiedDeviceInfo;
        using DataPoint = UnifiedDataPoint;
        using DataValue = TimestampedValue;
        
        // 기존 특화 구조체들을 UnifiedDeviceInfo로 변환하는 헬퍼들
        UnifiedDeviceInfo FromBACnetDeviceInfo(const void* bacnet_info);
        UnifiedDeviceInfo FromModbusDeviceInfo(const void* modbus_info);
        UnifiedDeviceInfo FromMQTTDeviceInfo(const void* mqtt_info);
    }

} // namespace Common
} // namespace PulseOne

#endif // PULSEONE_COMMON_UNIFIED_TYPES_H