// collector/include/Common/Structs.h (완전히 새로 작성)
#ifndef PULSEONE_COMMON_STRUCTS_H
#define PULSEONE_COMMON_STRUCTS_H

/**
 * @file Structs.h
 * @brief PulseOne 핵심 구조체 정의 (Unified 내용 통합 완료!)
 * @author PulseOne Development Team
 * @date 2025-08-05
 * 
 * 🎯 변경사항:
 * - UnifiedDeviceInfo → DeviceInfo (표준 이름으로 통합)
 * - UnifiedDataPoint → DataPoint (표준 이름으로 통합)  
 * - TimestampedValue 내용 완전 교체
 * - UnifiedXXX 구조체들 모두 삭제
 */

#include "BasicTypes.h"
#include "Enums.h"
#include "Constants.h"
#include "Utils.h"
#include "DriverStatistics.h"
#include "DriverError.h"
#include "IProtocolConfig.h"
#include "ProtocolConfigs.h"
#include <vector>
#include <optional>
#include <mutex>
#include <chrono>
#include <string>
#include <map>
#include <cassert>
#include <cstring>

// 조건부 JSON 라이브러리
#ifdef HAS_NLOHMANN_JSON
    #include <nlohmann/json.hpp>
    namespace json_impl = nlohmann;
#else
    namespace json_impl {
        class json {
        public:
            json() = default;
            json(const std::string& str) { (void)str; }
            json& operator=(const std::string& str) { (void)str; return *this; }
            bool empty() const { return true; }
            void clear() {}
            std::string dump() const { return "{}"; }
        };
    }
#endif

namespace PulseOne {
namespace Structs {
    
    // ✅ 네임스페이스 import
    using namespace PulseOne::BasicTypes;
    using namespace PulseOne::Enums;
    using JsonType = json_impl::json;
    namespace Utils = PulseOne::Utils;
    

    // =========================================================================
    // 🔥 Phase 1: 타임스탬프 값 구조체 (기존 확장)
    // =========================================================================
    
    /**
     * @brief 타임스탬프가 포함된 데이터 값
     * @details 모든 드라이버에서 사용하는 표준 값 구조체
     */
    struct TimestampedValue {
        DataValue value;                          // 실제 값 (DataVariant 별칭)
        Timestamp timestamp;                      // 수집 시간
        DataQuality quality = DataQuality::GOOD;  // 데이터 품질
        std::string source = "";                  // 데이터 소스
        
        TimestampedValue() : timestamp(Utils::CurrentTimestamp()) {}
        
        TimestampedValue(const DataValue& val) 
            : value(val), timestamp(Utils::CurrentTimestamp()) {}
        
        TimestampedValue(const DataValue& val, DataQuality qual)
            : value(val), timestamp(Utils::CurrentTimestamp()), quality(qual) {}
            
        // 🔥 기존 코드 호환을 위한 변환 메서드
        template<typename T>
        T GetValue() const {
            return std::get<T>(value);
        }
        
        bool IsGoodQuality() const {
            return quality == DataQuality::GOOD;
        }
    };


    // =========================================================================
    // 🔥 Phase 1: 통합 DataPoint 구조체 (기존 여러 DataPoint 통합)
    // =========================================================================
    
    /**
     * @brief 통합 데이터 포인트 구조체
     * @details 
     * - Database::DataPointEntity.toDataPointStruct() 호환
     * - Drivers::DataPoint 호환
     * - UnifiedDataPoint 호환
     * - 프로토콜별 편의 메서드 포함
     */
    struct DataPoint {
        // =======================================================================
        // 🔥 기본 식별 정보 (기존 호환)
        // =======================================================================
        UUID id;                                  // point_id (Database) + id (Drivers)
        UUID device_id;                           // 소속 디바이스 ID
        std::string name = "";                    // 표시 이름
        std::string description = "";             // 설명
        
        // =======================================================================
        // 🔥 주소 정보 (기존 호환)
        // =======================================================================
        uint32_t address = 0;                     // 숫자 주소 (Modbus 레지스터, BACnet 인스턴스 등)
        std::string address_string = "";          // 문자열 주소 (MQTT 토픽, OPC UA NodeId 등)
        
        // =======================================================================
        // 🔥 데이터 타입 및 접근성 (기존 호환)
        // =======================================================================
        std::string data_type = "UNKNOWN";        // INT16, UINT32, FLOAT, BOOL, STRING 등
        std::string access_mode = "read";         // read, write, read_write
        bool is_enabled = true;                   // 활성화 상태
        bool is_writable = false;                 // 쓰기 가능 여부
        
        // =======================================================================
        // 🔥 엔지니어링 단위 및 스케일링 (기존 호환)
        // =======================================================================
        std::string unit = "";                    // 단위 (℃, %, kW 등)
        double scaling_factor = 1.0;              // 스케일 인수 (기존 필드명)
        double scale_factor = 1.0;                // 별칭
        double scaling_offset = 0.0;              // 오프셋 (기존 필드명)
        double offset = 0.0;                      // 별칭
        double min_value = 0.0;                   // 최소값
        double max_value = 0.0;                   // 최대값
        
        // =======================================================================
        // 🔥 로깅 및 수집 설정 (기존 호환)
        // =======================================================================
        bool log_enabled = true;                  // 로깅 활성화
        uint32_t log_interval_ms = 0;             // 로깅 간격
        double log_deadband = 0.0;                // 로깅 데드밴드
        uint32_t polling_interval_ms = 0;         // 개별 폴링 간격 (0이면 디바이스 기본값)
        
        // =======================================================================
        // 🔥 메타데이터 (기존 호환)
        // =======================================================================
        std::string group = "";                   // 그룹명
        std::string tags = "";                    // JSON 배열 형태 (기존 호환)
        std::string metadata = "";                // JSON 객체 형태 (기존 호환)
        
        // =======================================================================
        // 🔥 프로토콜별 설정 (신규 - 편의성 향상)
        // =======================================================================
        std::map<std::string, std::string> protocol_params;  // 프로토콜 특화 매개변수
        
        // =======================================================================
        // 🔥 시간 정보 (기존 호환)
        // =======================================================================
        Timestamp created_at;
        Timestamp updated_at;
        
        // =======================================================================
        // 🔥 생성자들
        // =======================================================================
        DataPoint() {
            created_at = Utils::CurrentTimestamp();
            updated_at = created_at;
            
            // 기존 호환성을 위한 별칭 동기화
            scale_factor = scaling_factor;
            offset = scaling_offset;
        }
        
        // =======================================================================
        // 🔥 프로토콜별 편의 메서드들 (신규)
        // =======================================================================
        
        /**
         * @brief Modbus 레지스터 주소 설정
         */
        void SetModbusAddress(uint16_t register_addr, const std::string& reg_type = "HOLDING_REGISTER") {
            address = register_addr;
            address_string = std::to_string(register_addr);
            protocol_params["register_type"] = reg_type;
            protocol_params["function_code"] = (reg_type == "HOLDING_REGISTER") ? "3" : 
                                              (reg_type == "INPUT_REGISTER") ? "4" :
                                              (reg_type == "COIL") ? "1" : "2";
        }
        
        /**
         * @brief MQTT 토픽 설정
         */
        void SetMqttTopic(const std::string& topic, const std::string& json_path = "") {
            address_string = topic;
            protocol_params["topic"] = topic;
            if (!json_path.empty()) {
                protocol_params["json_path"] = json_path;
            }
        }
        
        /**
         * @brief BACnet Object 설정
         */
        void SetBACnetObject(uint32_t object_instance, const std::string& object_type = "ANALOG_INPUT", 
                            const std::string& property_id = "PRESENT_VALUE") {
            address = object_instance;
            address_string = std::to_string(object_instance);
            protocol_params["object_type"] = object_type;
            protocol_params["property_id"] = property_id;
        }
        
        /**
         * @brief 프로토콜 매개변수 조회
         */
        template<typename T>
        T GetProtocolParam(const std::string& key, const T& default_value = T{}) const {
            auto it = protocol_params.find(key);
            if (it != protocol_params.end()) {
                if constexpr (std::is_same_v<T, int>) {
                    return std::stoi(it->second);
                } else if constexpr (std::is_same_v<T, double>) {
                    return std::stod(it->second);
                } else if constexpr (std::is_same_v<T, bool>) {
                    return it->second == "true" || it->second == "1";
                } else {
                    return T(it->second);
                }
            }
            return default_value;
        }
        
        /**
         * @brief 값 유효성 검증
         */
        bool IsValueInRange(double value) const {
            if (max_value > min_value) {
                return value >= min_value && value <= max_value;
            }
            return true;  // 범위가 설정되지 않은 경우
        }
        
        /**
         * @brief 스케일링 적용
         */
        double ApplyScaling(double raw_value) const {
            return (raw_value * scaling_factor) + scaling_offset;
        }
        
        /**
         * @brief 역스케일링 적용 (쓰기 시 사용)
         */
        double RemoveScaling(double scaled_value) const {
            return (scaled_value - scaling_offset) / scaling_factor;
        }
        
        /**
         * @brief 별칭 필드 동기화
         */
        void SyncAliasFields() {
            scale_factor = scaling_factor;
            offset = scaling_offset;
        }
        
        // =======================================================================
        // 🔥 기존 DataPointEntity 호환 메서드들
        // =======================================================================
        
        bool isWritable() const { return is_writable; }
        void setWritable(bool writable) { is_writable = writable; }
        
        std::string getDataType() const { return data_type; }
        void setDataType(const std::string& type) { data_type = type; }
        
        std::string getUnit() const { return unit; }
        void setUnit(const std::string& u) { unit = u; }
    };

        // =========================================================================
    // 🔥 Phase 1: 스마트 포인터 기반 DriverConfig (Union 대체)
    // =========================================================================
    
    /**
     * @brief 통합 드라이버 설정 (스마트 포인터 기반)
     * @details 
     * - 기존 모든 *DriverConfig 구조체 대체
     * - 무제한 확장성 (새 프로토콜 추가 용이)
     * - 메모리 효율성 (필요한 크기만 할당)
     * - 타입 안전성 (dynamic_cast 활용)
     */
    struct DriverConfig {
        // =======================================================================
        // 🔥 공통 필드들 (기존 호환)
        // =======================================================================
        UUID device_id;                           // 디바이스 ID
        std::string name = "";                    // 디바이스 이름
        ProtocolType protocol = ProtocolType::UNKNOWN;  // 프로토콜 타입
        std::string endpoint = "";                // 연결 엔드포인트
        
        // 타이밍 설정
        uint32_t polling_interval_ms = 1000;      // 폴링 간격
        uint32_t timeout_ms = 5000;               // 타임아웃
        int retry_count = 3;                      // 재시도 횟수
        bool auto_reconnect = true;               // 자동 재연결
        
        // =======================================================================
        // 🔥 핵심: 스마트 포인터 기반 프로토콜 설정
        // =======================================================================
        std::unique_ptr<IProtocolConfig> protocol_config;
        
        // =======================================================================
        // 🔥 생성자들
        // =======================================================================
        
        DriverConfig() = default;
        
        explicit DriverConfig(ProtocolType proto) : protocol(proto) {
            protocol_config = CreateProtocolConfig(proto);
        }
        
        // 복사 생성자 (Clone 사용)
        DriverConfig(const DriverConfig& other) 
            : device_id(other.device_id)
            , name(other.name)
            , protocol(other.protocol)
            , endpoint(other.endpoint)
            , polling_interval_ms(other.polling_interval_ms)
            , timeout_ms(other.timeout_ms)
            , retry_count(other.retry_count)
            , auto_reconnect(other.auto_reconnect)
            , protocol_config(other.protocol_config ? other.protocol_config->Clone() : nullptr) {
        }
        
        // 할당 연산자
        DriverConfig& operator=(const DriverConfig& other) {
            if (this != &other) {
                device_id = other.device_id;
                name = other.name;
                protocol = other.protocol;
                endpoint = other.endpoint;
                polling_interval_ms = other.polling_interval_ms;
                timeout_ms = other.timeout_ms;
                retry_count = other.retry_count;
                auto_reconnect = other.auto_reconnect;
                protocol_config = other.protocol_config ? other.protocol_config->Clone() : nullptr;
            }
            return *this;
        }
        
        // =======================================================================
        // 🔥 타입 안전한 프로토콜 설정 접근자들
        // =======================================================================
        
        /**
         * @brief Modbus 설정 조회 (기존 GetModbusConfig() 대체)
         */
        ModbusConfig* GetModbusConfig() {
            return dynamic_cast<ModbusConfig*>(protocol_config.get());
        }
        
        const ModbusConfig* GetModbusConfig() const {
            return dynamic_cast<const ModbusConfig*>(protocol_config.get());
        }
        
        /**
         * @brief MQTT 설정 조회 (기존 GetMqttConfig() 대체)
         */
        MqttConfig* GetMqttConfig() {
            return dynamic_cast<MqttConfig*>(protocol_config.get());
        }
        
        const MqttConfig* GetMqttConfig() const {
            return dynamic_cast<const MqttConfig*>(protocol_config.get());
        }
        
        /**
         * @brief BACnet 설정 조회 (기존 GetBACnetConfig() 대체)
         */
        BACnetConfig* GetBACnetConfig() {
            return dynamic_cast<BACnetConfig*>(protocol_config.get());
        }
        
        const BACnetConfig* GetBACnetConfig() const {
            return dynamic_cast<const BACnetConfig*>(protocol_config.get());
        }
        
        // =======================================================================
        // 🔥 기존 코드 호환성 메서드들
        // =======================================================================
        
        bool IsModbus() const { 
            return protocol == ProtocolType::MODBUS_TCP || protocol == ProtocolType::MODBUS_RTU;
        }
        
        bool IsMqtt() const { 
            return protocol == ProtocolType::MQTT; 
        }
        
        bool IsBacnet() const { 
            return protocol == ProtocolType::BACNET; 
        }
        
        bool IsValid() const {
            return protocol != ProtocolType::UNKNOWN && 
                   !endpoint.empty() && 
                   protocol_config && 
                   protocol_config->IsValid();
        }
        
        std::string GetProtocolName() const {
            switch (protocol) {
                case ProtocolType::MODBUS_TCP: return "MODBUS_TCP";
                case ProtocolType::MODBUS_RTU: return "MODBUS_RTU";
                case ProtocolType::MQTT: return "MQTT";
                case ProtocolType::BACNET: return "BACNET";
                default: return "UNKNOWN";
            }
        }
        
    private:
        // =======================================================================
        // 🔥 팩토리 메서드
        // =======================================================================
        static std::unique_ptr<IProtocolConfig> CreateProtocolConfig(ProtocolType type) {
            switch (type) {
                case ProtocolType::MODBUS_TCP:
                case ProtocolType::MODBUS_RTU:
                    return std::make_unique<ModbusConfig>();
                case ProtocolType::MQTT:
                    return std::make_unique<MqttConfig>();
                case ProtocolType::BACNET:
                    return std::make_unique<BACnetConfig>();
                default:
                    return nullptr;
            }
        }
    };

    // =========================================================================
    // 🔥 완성된 DeviceInfo 구조체 (모든 기존 구조체 통합)
    // =========================================================================

    /**
     * @brief 완전 통합 디바이스 정보 구조체
     * @details 
     * ✅ 통합 대상:
     * - DeviceEntity (데이터베이스 엔티티)
     * - DeviceSettings (설정 관리)
     * - ModbusTcpDeviceInfo, MqttDeviceInfo, BACnetDeviceInfo (Worker용)
     * - 모든 프로토콜별 드라이버 구조체들
     * 
     * ✅ 특징:
     * - 스마트 포인터 기반 DriverConfig 포함
     * - 기존 필드명 100% 보존
     * - 모든 getter/setter 메서드 호환
     * - DeviceEntity ↔ DeviceInfo 완벽 변환 지원
     */
    struct DeviceInfo {
        // =======================================================================
        // 🔥 핵심: 스마트 포인터 기반 DriverConfig (Phase 1 완성)
        // =======================================================================
        DriverConfig driver_config;                    // 🔥 핵심! 스마트 포인터 기반
        
        // =======================================================================
        // 🔥 DeviceEntity 호환 필드들 (기존 필드명 100% 보존)
        // =======================================================================
        
        // 기본 식별 정보
        UUID id;                                       // device_id → id (Entity 호환)
        int tenant_id = 0;                             // 테넌트 ID
        int site_id = 0;                               // 사이트 ID
        std::optional<int> device_group_id;            // 디바이스 그룹 ID
        std::optional<int> edge_server_id;             // 엣지 서버 ID
        
        // 디바이스 기본 정보 (Entity 호환)
        std::string name = "";                         // 디바이스 이름
        std::string description = "";                  // 설명
        std::string device_type = "";                  // 디바이스 타입 (PLC, HMI, SENSOR 등)
        std::string manufacturer = "";                 // 제조사
        std::string model = "";                        // 모델명
        std::string serial_number = "";               // 시리얼 번호
        std::string firmware_version = "";            // 펌웨어 버전
        
        // 통신 설정 (Entity 호환)
        std::string protocol_type = "";                // 프로토콜 타입 (문자열)
        std::string endpoint = "";                     // 엔드포인트
        std::string connection_string = "";            // 연결 문자열 (endpoint 별칭)
        std::string config = "";                       // JSON 설정 (Entity 호환)
        
        // 네트워크 설정 (Entity 확장)
        std::string ip_address = "";                   // IP 주소
        int port = 0;                                  // 포트 번호
        std::string mac_address = "";                  // MAC 주소
        
        // 타이밍 설정 (Entity 확장)
        int polling_interval_ms = 1000;                // 폴링 간격
        int timeout_ms = 5000;                         // 타임아웃
        int retry_count = 3;                           // 재시도 횟수
        
        // 상태 정보 (Entity 호환)
        bool is_enabled = true;                        // 활성화 상태
        bool enabled = true;                           // is_enabled 별칭
        ConnectionStatus connection_status = ConnectionStatus::DISCONNECTED;
        
        // 위치 및 물리적 정보 (Entity 확장)
        std::string location = "";                     // 설치 위치
        std::string rack_position = "";               // 랙 위치
        std::string building = "";                     // 건물
        std::string floor = "";                        // 층
        std::string room = "";                         // 방
        
        // 유지보수 정보 (Entity 확장)
        Timestamp installation_date;                   // 설치일
        Timestamp last_maintenance;                    // 마지막 점검일
        Timestamp next_maintenance;                    // 다음 점검 예정일
        std::string maintenance_notes = "";           // 점검 메모
        
        // 메타데이터 (Entity + DeviceSettings 호환)
        std::string tags = "";                         // JSON 배열 형태
        std::vector<std::string> tag_list;            // 태그 목록
        std::map<std::string, std::string> metadata;  // 추가 메타데이터
        std::map<std::string, std::string> properties; // 커스텀 속성들
        
        // 시간 정보 (Entity 호환)
        Timestamp created_at;
        Timestamp updated_at;
        int created_by = 0;                           // 생성자 ID
        
        // 성능 및 모니터링 (DeviceSettings 호환)
        bool monitoring_enabled = true;               // 모니터링 활성화
        std::string log_level = "INFO";               // 로그 레벨
        bool diagnostics_enabled = false;            // 진단 모드
        bool performance_monitoring = true;          // 성능 모니터링
        
        // 보안 설정 (DeviceSettings 확장)
        std::string security_level = "NORMAL";        // 보안 레벨
        bool encryption_enabled = false;             // 암호화 사용
        std::string certificate_path = "";           // 인증서 경로
        
        // =======================================================================
        // 🔥 생성자들
        // =======================================================================
        
        DeviceInfo() {
            id = Utils::GenerateUUID();
            created_at = Utils::CurrentTimestamp();
            updated_at = created_at;
            installation_date = created_at;
            last_maintenance = created_at;
            next_maintenance = created_at;
            
            // 별칭 동기화
            enabled = is_enabled;
            connection_string = endpoint;
        }
        
        explicit DeviceInfo(ProtocolType protocol) : DeviceInfo() {
            driver_config = DriverConfig(protocol);
            protocol_type = driver_config.GetProtocolName();
        }
        
        // =======================================================================
        // 🔥 DriverConfig 위임 메서드들 (Phase 1 호환)
        // =======================================================================
        
        /**
         * @brief 스마트 포인터 기반 Modbus 설정 접근
         */
        ModbusConfig* GetModbusConfig() {
            return driver_config.GetModbusConfig();
        }
        const ModbusConfig* GetModbusConfig() const {
            return driver_config.GetModbusConfig();
        }
        
        /**
         * @brief 스마트 포인터 기반 MQTT 설정 접근
         */
        MqttConfig* GetMqttConfig() {
            return driver_config.GetMqttConfig();
        }
        const MqttConfig* GetMqttConfig() const {
            return driver_config.GetMqttConfig();
        }
        
        /**
         * @brief 스마트 포인터 기반 BACnet 설정 접근
         */
        BACnetConfig* GetBACnetConfig() {
            return driver_config.GetBACnetConfig();
        }
        const BACnetConfig* GetBACnetConfig() const {
            return driver_config.GetBACnetConfig();
        }
        
        /**
         * @brief IProtocolDriver 호환 - DriverConfig 접근
         */
        DriverConfig& GetDriverConfig() {
            SyncToDriverConfig();  // 동기화 후 반환
            return driver_config;
        }
        const DriverConfig& GetDriverConfig() const {
            return driver_config;
        }
        
        // =======================================================================
        // 🔥 DeviceEntity 호환 getter/setter 메서드들 (기존 API 100% 보존)
        // =======================================================================
        
        // 식별 정보
        const UUID& getId() const { return id; }
        void setId(const UUID& device_id) { id = device_id; }
        
        int getTenantId() const { return tenant_id; }
        void setTenantId(int tenant) { tenant_id = tenant; }
        
        int getSiteId() const { return site_id; }
        void setSiteId(int site) { site_id = site; }
        
        std::optional<int> getDeviceGroupId() const { return device_group_id; }
        void setDeviceGroupId(const std::optional<int>& group_id) { device_group_id = group_id; }
        void setDeviceGroupId(int group_id) { device_group_id = group_id; }
        
        std::optional<int> getEdgeServerId() const { return edge_server_id; }
        void setEdgeServerId(const std::optional<int>& server_id) { edge_server_id = server_id; }
        void setEdgeServerId(int server_id) { edge_server_id = server_id; }
        
        // 기본 정보
        const std::string& getName() const { return name; }
        void setName(const std::string& device_name) { 
            name = device_name;
            driver_config.name = device_name;  // 동기화
        }
        
        const std::string& getDescription() const { return description; }
        void setDescription(const std::string& desc) { description = desc; }
        
        const std::string& getDeviceType() const { return device_type; }
        void setDeviceType(const std::string& type) { device_type = type; }
        
        const std::string& getManufacturer() const { return manufacturer; }
        void setManufacturer(const std::string& mfg) { manufacturer = mfg; }
        
        const std::string& getModel() const { return model; }
        void setModel(const std::string& device_model) { model = device_model; }
        
        const std::string& getSerialNumber() const { return serial_number; }
        void setSerialNumber(const std::string& serial) { serial_number = serial; }
        
        const std::string& getFirmwareVersion() const { return firmware_version; }
        void setFirmwareVersion(const std::string& version) { firmware_version = version; }
        
        // 통신 설정
        const std::string& getProtocolType() const { return protocol_type; }
        void setProtocolType(const std::string& protocol) { 
            protocol_type = protocol;
            // TODO: driver_config.protocol 동기화 로직 필요
        }
        
        const std::string& getEndpoint() const { return endpoint; }
        void setEndpoint(const std::string& ep) { 
            endpoint = ep;
            connection_string = ep;  // 별칭 동기화
            driver_config.endpoint = ep;  // 동기화
        }
        
        const std::string& getConfig() const { return config; }
        void setConfig(const std::string& cfg) { config = cfg; }
        
        const std::string& getIpAddress() const { return ip_address; }
        void setIpAddress(const std::string& ip) { ip_address = ip; }
        
        int getPort() const { return port; }
        void setPort(int port_num) { port = port_num; }
        
        // 상태 정보
        bool isEnabled() const { return is_enabled; }
        void setEnabled(bool enable) { 
            is_enabled = enable;
            enabled = enable;  // 별칭 동기화
        }
        
        bool getEnabled() const { return enabled; }  // 별칭 메서드
        
        ConnectionStatus getConnectionStatus() const { return connection_status; }
        void setConnectionStatus(ConnectionStatus status) { connection_status = status; }
        
        // 위치 정보
        const std::string& getLocation() const { return location; }
        void setLocation(const std::string& loc) { location = loc; }
        
        const std::string& getBuilding() const { return building; }
        void setBuilding(const std::string& bldg) { building = bldg; }
        
        const std::string& getFloor() const { return floor; }
        void setFloor(const std::string& fl) { floor = fl; }
        
        const std::string& getRoom() const { return room; }
        void setRoom(const std::string& rm) { room = rm; }
        
        // 시간 정보
        const Timestamp& getCreatedAt() const { return created_at; }
        void setCreatedAt(const Timestamp& timestamp) { created_at = timestamp; }
        
        const Timestamp& getUpdatedAt() const { return updated_at; }
        void setUpdatedAt(const Timestamp& timestamp) { updated_at = timestamp; }
        
        int getCreatedBy() const { return created_by; }
        void setCreatedBy(int user_id) { created_by = user_id; }
        
        // =======================================================================
        // 🔥 Worker 호환 메서드들 (기존 Worker 클래스들 호환)
        // =======================================================================
        
        // 프로토콜 타입 확인 (기존 Worker 코드 호환)
        bool IsModbus() const { return driver_config.IsModbus(); }
        bool IsMqtt() const { return driver_config.IsMqtt(); }
        bool IsBacnet() const { return driver_config.IsBacnet(); }
        
        // 타이밍 설정 (Worker에서 사용)
        uint32_t GetPollingInterval() const { 
            return static_cast<uint32_t>(polling_interval_ms); 
        }
        void SetPollingInterval(uint32_t interval_ms) { 
            polling_interval_ms = static_cast<int>(interval_ms);
            driver_config.polling_interval_ms = interval_ms;
        }
        
        uint32_t GetTimeout() const { 
            return static_cast<uint32_t>(timeout_ms); 
        }
        void SetTimeout(uint32_t timeout) { 
            timeout_ms = static_cast<int>(timeout);
            driver_config.timeout_ms = timeout;
        }
        
        // =======================================================================
        // 🔥 변환 및 동기화 메서드들
        // =======================================================================
        
        /**
         * @brief DeviceEntity로부터 데이터 로드
         */
        void LoadFromDeviceEntity(const Database::Entities::DeviceEntity& entity) {
            // 기본 정보
            setId(std::to_string(entity.getId()));
            setTenantId(entity.getTenantId());
            setSiteId(entity.getSiteId());
            setDeviceGroupId(entity.getDeviceGroupId());
            setEdgeServerId(entity.getEdgeServerId());
            
            // 디바이스 정보
            setName(entity.getName());
            setDescription(entity.getDescription());
            setDeviceType(entity.getDeviceType());
            setManufacturer(entity.getManufacturer());
            setModel(entity.getModel());
            setSerialNumber(entity.getSerialNumber());
            setFirmwareVersion(entity.getFirmwareVersion());
            
            // 통신 설정
            setProtocolType(entity.getProtocolType());
            setEndpoint(entity.getEndpoint());
            setConfig(entity.getConfig());
            setIpAddress(entity.getIpAddress());
            setPort(entity.getPort());
            
            // 상태 정보
            setEnabled(entity.isEnabled());
            
            // 위치 정보
            setLocation(entity.getLocation());
            setBuilding(entity.getBuilding());
            setFloor(entity.getFloor());
            setRoom(entity.getRoom());
            
            // 시간 정보
            setCreatedAt(entity.getCreatedAt());
            setUpdatedAt(entity.getUpdatedAt());
            setCreatedBy(entity.getCreatedBy());
            
            // DriverConfig 동기화
            SyncToDriverConfig();
        }
        
        /**
         * @brief DeviceEntity로 데이터 저장
         */
        void SaveToDeviceEntity(Database::Entities::DeviceEntity& entity) const {
            // 기본 정보
            entity.setTenantId(getTenantId());
            entity.setSiteId(getSiteId());
            entity.setDeviceGroupId(getDeviceGroupId());
            entity.setEdgeServerId(getEdgeServerId());
            
            // 디바이스 정보
            entity.setName(getName());
            entity.setDescription(getDescription());
            entity.setDeviceType(getDeviceType());
            entity.setManufacturer(getManufacturer());
            entity.setModel(getModel());
            entity.setSerialNumber(getSerialNumber());
            entity.setFirmwareVersion(getFirmwareVersion());
            
            // 통신 설정
            entity.setProtocolType(getProtocolType());
            entity.setEndpoint(getEndpoint());
            entity.setConfig(getConfig());
            entity.setIpAddress(getIpAddress());
            entity.setPort(getPort());
            
            // 상태 정보
            entity.setEnabled(isEnabled());
            
            // 위치 정보
            entity.setLocation(getLocation());
            entity.setBuilding(getBuilding());
            entity.setFloor(getFloor());
            entity.setRoom(getRoom());
            
            // 시간 정보
            entity.setCreatedBy(getCreatedBy());
        }
        
        /**
         * @brief 필드 동기화 (별칭 필드들 동기화)
         */
        void SyncAliasFields() {
            enabled = is_enabled;
            connection_string = endpoint;
            
            // DriverConfig와 동기화
            driver_config.name = name;
            driver_config.endpoint = endpoint;
            driver_config.polling_interval_ms = static_cast<uint32_t>(polling_interval_ms);
            driver_config.timeout_ms = static_cast<uint32_t>(timeout_ms);
            driver_config.retry_count = retry_count;
        }
        
        /**
         * @brief DriverConfig로 동기화
         */
        void SyncToDriverConfig() {
            SyncAliasFields();
            driver_config.device_id = id;
            
            // 프로토콜별 설정 동기화 (필요시 구현)
            // if (IsModbus()) { /* Modbus 특화 동기화 */ }
            // if (IsMqtt()) { /* MQTT 특화 동기화 */ }
            // if (IsBacnet()) { /* BACnet 특화 동기화 */ }
        }
        
        /**
         * @brief 유효성 검증
         */
        bool IsValid() const {
            return !name.empty() && 
                !protocol_type.empty() && 
                !endpoint.empty() && 
                driver_config.IsValid();
        }
        
        /**
         * @brief JSON 직렬화 (DeviceEntity 호환)
         */
        JsonType ToJson() const {
            JsonType j;
            
            // 기본 정보
            j["id"] = id;
            j["tenant_id"] = tenant_id;
            j["site_id"] = site_id;
            if (device_group_id.has_value()) j["device_group_id"] = device_group_id.value();
            if (edge_server_id.has_value()) j["edge_server_id"] = edge_server_id.value();
            
            // 디바이스 정보
            j["name"] = name;
            j["description"] = description;
            j["device_type"] = device_type;
            j["manufacturer"] = manufacturer;
            j["model"] = model;
            j["serial_number"] = serial_number;
            j["firmware_version"] = firmware_version;
            
            // 통신 설정
            j["protocol_type"] = protocol_type;
            j["endpoint"] = endpoint;
            j["config"] = config;
            j["ip_address"] = ip_address;
            j["port"] = port;
            
            // 상태 정보
            j["is_enabled"] = is_enabled;
            j["connection_status"] = static_cast<int>(connection_status);
            
            // 위치 정보
            j["location"] = location;
            j["building"] = building;
            j["floor"] = floor;
            j["room"] = room;
            
            // 시간 정보
            j["created_at"] = Utils::TimestampToString(created_at);
            j["updated_at"] = Utils::TimestampToString(updated_at);
            j["created_by"] = created_by;
            
            return j;
        }
        
        /**
         * @brief JSON에서 역직렬화
         */
        bool FromJson(const JsonType& j) {
            try {
                // 기본 정보
                if (j.contains("id")) id = j["id"];
                if (j.contains("tenant_id")) tenant_id = j["tenant_id"];
                if (j.contains("site_id")) site_id = j["site_id"];
                if (j.contains("device_group_id")) device_group_id = j["device_group_id"];
                if (j.contains("edge_server_id")) edge_server_id = j["edge_server_id"];
                
                // 디바이스 정보
                if (j.contains("name")) name = j["name"];
                if (j.contains("description")) description = j["description"];
                if (j.contains("device_type")) device_type = j["device_type"];
                if (j.contains("manufacturer")) manufacturer = j["manufacturer"];
                if (j.contains("model")) model = j["model"];
                if (j.contains("serial_number")) serial_number = j["serial_number"];
                if (j.contains("firmware_version")) firmware_version = j["firmware_version"];
                
                // 통신 설정
                if (j.contains("protocol_type")) protocol_type = j["protocol_type"];
                if (j.contains("endpoint")) endpoint = j["endpoint"];
                if (j.contains("config")) config = j["config"];
                if (j.contains("ip_address")) ip_address = j["ip_address"];
                if (j.contains("port")) port = j["port"];
                
                // 상태 정보
                if (j.contains("is_enabled")) is_enabled = j["is_enabled"];
                if (j.contains("connection_status")) connection_status = static_cast<ConnectionStatus>(j["connection_status"]);
                
                // 위치 정보
                if (j.contains("location")) location = j["location"];
                if (j.contains("building")) building = j["building"];
                if (j.contains("floor")) floor = j["floor"];
                if (j.contains("room")) room = j["room"];
                
                // 시간 정보
                if (j.contains("created_by")) created_by = j["created_by"];
                
                // 별칭 동기화
                SyncAliasFields();
                
                return true;
            } catch (...) {
                return false;
            }
        }
        
        // =======================================================================
        // 🔥 기존 typedef 호환성 (기존 코드에서 사용하던 타입명들)
        // =======================================================================
        
        // Worker에서 사용하던 별칭들
        std::string GetProtocolName() const { return driver_config.GetProtocolName(); }
        ProtocolType GetProtocol() const { return driver_config.protocol; }
    };
    // =========================================================================
    // 🔥 기존 코드 호환성을 위한 별칭들
    // =========================================================================
    
    // 기존 Database, Drivers 네임스페이스 호환성
    namespace Database {
        using DeviceInfo = Structs::DeviceInfo;
        using DataPoint = Structs::DataPoint;
    }
    
    // 메시지 전송용 확장 (향후 사용)
    struct DeviceDataMessage {
        std::string type = "device_data";
        UUID device_id;
        std::string protocol;
        std::vector<TimestampedValue> points;
        Timestamp timestamp;
        
        DeviceDataMessage() : timestamp(Utils::CurrentTimestamp()) {}
        
        std::string ToJSON() const {
            JsonType j;
            j["type"] = type;
            j["device_id"] = device_id;
            j["protocol"] = protocol;
            j["timestamp"] = Utils::TimestampToString(timestamp);
            
            j["points"] = JsonType::array();
            for (const auto& point : points) {
                JsonType point_json = JsonType::parse(point.ToJSON());
                j["points"].push_back(point_json);
            }
            
            return j.dump();
        }
    };

} // namespace Structs

// =========================================================================
// 🔥 전역 네임스페이스 호환성 (최상위 PulseOne에서 직접 사용 가능)
// =========================================================================
using DeviceInfo = Structs::DeviceInfo;
using DataPoint = Structs::DataPoint;
using TimestampedValue = Structs::TimestampedValue;

// Drivers 네임스페이스 호환성 유지
namespace Drivers {
    using DeviceInfo = PulseOne::DeviceInfo;
    using DataPoint = PulseOne::DataPoint;
    using TimestampedValue = PulseOne::TimestampedValue;
}

} // namespace PulseOne

#endif // PULSEONE_COMMON_STRUCTS_H