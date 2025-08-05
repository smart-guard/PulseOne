// collector/include/Common/Structs.h
#ifndef PULSEONE_COMMON_STRUCTS_H
#define PULSEONE_COMMON_STRUCTS_H

/**
 * @file Structs.h
 * @brief PulseOne 핵심 구조체 정의 (에러 완전 해결)
 * @author PulseOne Development Team
 * @date 2025-08-05
 * 
 * 🔥 주요 수정사항:
 * - ErrorCode 중복 정의 해결
 * - Database::Entities 의존성 완전 제거
 * - 타입 충돌 해결
 * - Utils include 제거 (순환 의존성 방지)
 */

#include "BasicTypes.h"
#include "Enums.h"
#include "Constants.h"
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

// 🔥 JSON 라이브러리 조건부 정의 (한 번만!)
#ifdef HAS_NLOHMANN_JSON
    #include <nlohmann/json.hpp>
    using JsonType = nlohmann::json;
#else
    // 완전한 DummyJson 클래스 - 모든 할당 연산자 포함
    class DummyJson {
    public:
        // 기본 생성자/소멸자
        DummyJson() = default;
        DummyJson(const DummyJson&) = default;
        DummyJson(DummyJson&&) = default;
        ~DummyJson() = default;
        
        // 🔥 모든 기본 타입에 대한 할당 연산자들
        DummyJson& operator=(const DummyJson&) = default;
        DummyJson& operator=(DummyJson&&) = default;
        
        // 문자열 타입들
        DummyJson& operator=(const std::string&) { return *this; }
        DummyJson& operator=(const char*) { return *this; }
        
        // 정수 타입들
        DummyJson& operator=(bool) { return *this; }
        DummyJson& operator=(int) { return *this; }
        DummyJson& operator=(unsigned int) { return *this; }
        DummyJson& operator=(long) { return *this; }
        DummyJson& operator=(unsigned long) { return *this; }
        DummyJson& operator=(long long) { return *this; }
        DummyJson& operator=(unsigned long long) { return *this; }
        DummyJson& operator=(short) { return *this; }
        DummyJson& operator=(unsigned short) { return *this; }
        DummyJson& operator=(char) { return *this; }
        DummyJson& operator=(unsigned char) { return *this; }
        
        // 부동소수점 타입들
        DummyJson& operator=(float) { return *this; }
        DummyJson& operator=(double) { return *this; }
        DummyJson& operator=(long double) { return *this; }
        
        // 인덱싱 연산자들
        DummyJson& operator[](const std::string&) { return *this; }
        const DummyJson& operator[](const std::string&) const { return *this; }
        DummyJson& operator[](int) { return *this; }
        const DummyJson& operator[](int) const { return *this; }
        DummyJson& operator[](size_t) { return *this; }
        const DummyJson& operator[](size_t) const { return *this; }
        
        // 기본 메서드들
        template<typename T> 
        T get() const { return T{}; }
        
        template<typename T> 
        T value(const std::string&, const T& default_val) const { return default_val; }
        
        bool contains(const std::string&) const { return false; }
        std::string dump(int = 0) const { return "{}"; }
        void push_back(const DummyJson&) {}
        bool empty() const { return true; }
        size_t size() const { return 0; }
        void clear() {}
        
        // 정적 메서드들
        static DummyJson parse(const std::string&) { return DummyJson{}; }
        static DummyJson object() { return DummyJson{}; }
        static DummyJson array() { return DummyJson{}; }
        
        // 암시적 변환 연산자들
        operator bool() const { return false; }
        operator int() const { return 0; }
        operator double() const { return 0.0; }
        operator std::string() const { return ""; }
        
        // 반복자 지원 (기본)
        using iterator = DummyJson*;
        using const_iterator = const DummyJson*;
        iterator begin() { return this; }
        iterator end() { return this; }
        const_iterator begin() const { return this; }
        const_iterator end() const { return this; }
        const_iterator cbegin() const { return this; }
        const_iterator cend() const { return this; }
    };
    using JsonType = DummyJson;
#endif

// 🔥 전방 선언으로 순환 의존성 방지
namespace PulseOne::Structs {
    class IProtocolConfig;  // IProtocolConfig.h 전방 선언
}

namespace PulseOne {
namespace Structs {
    
    // 🔥 타입 별칭 명시적 선언 (순환 참조 방지)
    using namespace PulseOne::BasicTypes;
    using namespace PulseOne::Enums;
    using JsonType = json_impl::json;

    // 🔥 핵심 타입들 명시적 별칭 (필수!)
    using DataValue = PulseOne::BasicTypes::DataVariant;   // ✅ 매우 중요!
    using Timestamp = PulseOne::BasicTypes::Timestamp;     // ✅ 매우 중요!
    using UUID = PulseOne::BasicTypes::UUID;               // ✅ 매우 중요!
    using Duration = PulseOne::BasicTypes::Duration;       // ✅ 중요!
    using EngineerID = PulseOne::BasicTypes::EngineerID;   // ✅ 중요!
    
    // 🔥 Enums 타입들 명시적 선언 (ErrorCode 제외!)
    using ProtocolType = PulseOne::Enums::ProtocolType;
    using ConnectionStatus = PulseOne::Enums::ConnectionStatus;
    using DataQuality = PulseOne::Enums::DataQuality;
    using LogLevel = PulseOne::Enums::LogLevel;
    using MaintenanceStatus = PulseOne::Enums::MaintenanceStatus;
    // ❌ ErrorCode 별칭 제거 (DriverError.h와 충돌 방지)
    // using ErrorCode = PulseOne::Enums::ErrorCode;  // 🔥 제거!
    
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
        
        // 🔥 생성자 수정 (Utils 의존성 제거)
        TimestampedValue() : timestamp(std::chrono::system_clock::now()) {}
        
        TimestampedValue(const DataValue& val) 
            : value(val), timestamp(std::chrono::system_clock::now()) {}
        
        TimestampedValue(const DataValue& val, DataQuality qual)
            : value(val), timestamp(std::chrono::system_clock::now()), quality(qual) {}
        
        // 🔥 타입 안전한 값 접근
        template<typename T>
        T GetValue() const {
            return std::get<T>(value);
        }
        
        // 🔥 JSON 직렬화 메소드 추가
        std::string ToJSON() const {
            JsonType j;
            
            // variant 값 처리
            std::visit([&j](const auto& v) {
                j["value"] = v;
            }, value);
            
            // 타임스탬프를 milliseconds로 변환
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count();
            j["timestamp"] = ms;
            j["quality"] = static_cast<int>(quality);
            j["source"] = source;
            
            return j.dump();
        }
        
        bool FromJSON(const std::string& json_str) {
            try {
                JsonType j = JsonType::parse(json_str);
                
                // 타임스탬프 복원
                if (j.contains("timestamp")) {
                    auto ms = j["timestamp"].template get<int64_t>();
                    timestamp = Timestamp(std::chrono::milliseconds(ms));
                }
                
                if (j.contains("quality")) {
                    quality = static_cast<DataQuality>(j["quality"].template get<int>());
                }
                
                if (j.contains("source")) {
                    source = j["source"].template get<std::string>();
                }
                
                return true;
            } catch (...) {
                return false;
            }
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
            created_at = std::chrono::system_clock::now();
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
        std::map<std::string, std::string> properties; // 🔥 프로토콜별 속성 저장 (통합 시스템 핵심)

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
        // 🔥 기존 코드 호환성 메서드들
        // =======================================================================
        
        bool IsModbus() const { 
            return protocol == ProtocolType::MODBUS_TCP || protocol == ProtocolType::MODBUS_RTU;
        }
        
        bool IsMqtt() const { 
            return protocol == ProtocolType::MQTT; 
        }
        
        bool IsBacnet() const { 
            return protocol == ProtocolType::BACNET_IP || protocol == ProtocolType::BACNET_MSTP; 
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
                case ProtocolType::BACNET_IP: return "BACNET_IP";
                case ProtocolType::BACNET_MSTP: return "BACNET_MSTP";
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
                case ProtocolType::BACNET_IP:
                case ProtocolType::BACNET_MSTP:
                    return std::make_unique<BACnetConfig>();
                default:
                    return nullptr;
            }
        }
    };

    // =========================================================================
    // 🔥 완성된 DeviceInfo 구조체 (Database::Entities 의존성 제거)
    // =========================================================================

    /**
     * @brief 완전 통합 디바이스 정보 구조체
     * @details 
     * ✅ Database::Entities 의존성 완전 제거
     * ✅ 기존 필드명 100% 보존
     * ✅ 모든 getter/setter 메서드 호환
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
            auto now = std::chrono::system_clock::now();
            created_at = now;
            updated_at = now;
            installation_date = now;
            last_maintenance = now;
            next_maintenance = now;
            
            // 별칭 동기화
            enabled = is_enabled;
            connection_string = endpoint;
        }
        
        explicit DeviceInfo(ProtocolType protocol) : DeviceInfo() {
            driver_config = DriverConfig(protocol);
            protocol_type = driver_config.GetProtocolName();
        }
        
        // =======================================================================
        // 🔥 기존 DeviceEntity 호환 getter/setter 메서드들 (기존 API 100% 보존)
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
        // 🔥 DriverConfig 위임 메서드들 (Phase 1 호환)
        // =======================================================================
        
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
        // 🔥 변환 및 동기화 메서드들 (Database::Entities 의존성 제거)
        // =======================================================================
        
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
            
            // 시간 정보 - 직접 구현 (Utils 의존성 제거)
            auto time_t = std::chrono::system_clock::to_time_t(created_at);
            std::tm tm_buf;
            #ifdef _WIN32
                gmtime_s(&tm_buf, &time_t);
            #else
                gmtime_r(&time_t, &tm_buf);
            #endif
            char buffer[32];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
            j["created_at"] = std::string(buffer);
            
            time_t = std::chrono::system_clock::to_time_t(updated_at);
            #ifdef _WIN32
                gmtime_s(&tm_buf, &time_t);
            #else
                gmtime_r(&time_t, &tm_buf);
            #endif
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
            j["updated_at"] = std::string(buffer);
            
            j["created_by"] = created_by;
            
            return j;
        }
        
        /**
         * @brief JSON에서 역직렬화
         */
        bool FromJson(const JsonType& j) {
            try {
                // 기본 정보
                if (j.contains("id")) id = j["id"].template get<std::string>();
                if (j.contains("tenant_id")) tenant_id = j["tenant_id"].template get<int>();
                if (j.contains("site_id")) site_id = j["site_id"].template get<int>();
                // optional 필드들은 별도 처리
                
                // 디바이스 정보
                if (j.contains("name")) name = j["name"].template get<std::string>();
                if (j.contains("description")) description = j["description"].template get<std::string>();
                if (j.contains("device_type")) device_type = j["device_type"].template get<std::string>();
                if (j.contains("manufacturer")) manufacturer = j["manufacturer"].template get<std::string>();
                if (j.contains("model")) model = j["model"].template get<std::string>();
                if (j.contains("serial_number")) serial_number = j["serial_number"].template get<std::string>();
                if (j.contains("firmware_version")) firmware_version = j["firmware_version"].template get<std::string>();
                
                // 통신 설정
                if (j.contains("protocol_type")) protocol_type = j["protocol_type"].template get<std::string>();
                if (j.contains("endpoint")) endpoint = j["endpoint"].template get<std::string>();
                if (j.contains("config")) config = j["config"].template get<std::string>();
                if (j.contains("ip_address")) ip_address = j["ip_address"].template get<std::string>();
                if (j.contains("port")) port = j["port"].template get<int>();
                
                // 상태 정보
                if (j.contains("is_enabled")) is_enabled = j["is_enabled"].template get<bool>();
                if (j.contains("connection_status")) connection_status = static_cast<ConnectionStatus>(j["connection_status"].template get<int>());
                
                // 위치 정보
                if (j.contains("location")) location = j["location"].template get<std::string>();
                if (j.contains("building")) building = j["building"].template get<std::string>();
                if (j.contains("floor")) floor = j["floor"].template get<std::string>();
                if (j.contains("room")) room = j["room"].template get<std::string>();
                
                // 시간 정보
                if (j.contains("created_by")) created_by = j["created_by"].template get<int>();
                
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
    // 🔥 메시지 전송용 확장 (향후 사용)
    // =========================================================================
    struct DeviceDataMessage {
        std::string type = "device_data";
        UUID device_id;
        std::string protocol;
        std::vector<TimestampedValue> points;
        Timestamp timestamp;
        
        DeviceDataMessage() : timestamp(std::chrono::system_clock::now()) {}
        
        std::string ToJSON() const {
            JsonType j;
            j["type"] = type;
            j["device_id"] = device_id;
            j["protocol"] = protocol;
            
            // 타임스탬프를 문자열로 변환 (Utils 의존성 제거)
            auto time_t = std::chrono::system_clock::to_time_t(timestamp);
            std::tm tm_buf;
            #ifdef _WIN32
                gmtime_s(&tm_buf, &time_t);
            #else
                gmtime_r(&time_t, &tm_buf);
            #endif
            char buffer[32];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
            j["timestamp"] = std::string(buffer);
            
            j["points"] = JsonType::array();
            for (const auto& point : points) {
                JsonType point_json = JsonType::parse(point.ToJSON());
                j["points"].push_back(point_json);
            }
            
            return j.dump();
        }
    };

    /**
     * @brief 로그 통계 구조체
     */
    struct LogStatistics {
        std::atomic<uint64_t> trace_count{0};
        std::atomic<uint64_t> debug_count{0};
        std::atomic<uint64_t> info_count{0};
        std::atomic<uint64_t> warn_count{0};
        std::atomic<uint64_t> warning_count{0};  // warn_count 별칭
        std::atomic<uint64_t> error_count{0};
        std::atomic<uint64_t> fatal_count{0};
        std::atomic<uint64_t> maintenance_count{0};
        std::atomic<uint64_t> total_logs{0};     // 🔥 추가: resetStatistics에서 사용
        
        Timestamp start_time;
        Timestamp last_log_time;
        Timestamp last_reset_time;               // 🔥 추가: resetStatistics에서 사용
        
        LogStatistics() {
            start_time = std::chrono::system_clock::now();
            last_log_time = start_time;
            last_reset_time = start_time;        // 🔥 초기화 추가
        }
        
        // 🔥 복사 생성자 명시적 구현 (atomic 때문에 필요)
        LogStatistics(const LogStatistics& other) 
            : trace_count(other.trace_count.load())
            , debug_count(other.debug_count.load())
            , info_count(other.info_count.load())
            , warn_count(other.warn_count.load())
            , warning_count(other.warning_count.load())
            , error_count(other.error_count.load())
            , fatal_count(other.fatal_count.load())
            , maintenance_count(other.maintenance_count.load())
            , total_logs(other.total_logs.load())     // 🔥 추가
            , start_time(other.start_time)
            , last_log_time(other.last_log_time)
            , last_reset_time(other.last_reset_time) {  // 🔥 추가
        }
        
        // 🔥 할당 연산자 명시적 구현
        LogStatistics& operator=(const LogStatistics& other) {
            if (this != &other) {
                trace_count.store(other.trace_count.load());
                debug_count.store(other.debug_count.load());
                info_count.store(other.info_count.load());
                warn_count.store(other.warn_count.load());
                warning_count.store(other.warning_count.load());
                error_count.store(other.error_count.load());
                fatal_count.store(other.fatal_count.load());
                maintenance_count.store(other.maintenance_count.load());
                total_logs.store(other.total_logs.load());        // 🔥 추가
                start_time = other.start_time;
                last_log_time = other.last_log_time;
                last_reset_time = other.last_reset_time;          // 🔥 추가
            }
            return *this;
        }
        
        // 🔥 총 로그 수 계산 메소드 (기존 멤버와 중복되지 않도록)
        uint64_t CalculateTotalLogs() const {
            return trace_count.load() + debug_count.load() + info_count.load() + 
                   warn_count.load() + error_count.load() + fatal_count.load() + 
                   maintenance_count.load();
        }
        
        // 🔥 GetTotalLogs 메소드 (기존 에러 메시지에서 제안된 이름)
        uint64_t GetTotalLogs() const {
            return total_logs.load();
        }
        
        // 🔥 total_logs 업데이트 (로그 추가 시 호출)
        void IncrementTotalLogs() {
            total_logs.fetch_add(1);
        }
        
        // 🔥 모든 카운터 리셋
        void ResetAllCounters() {
            trace_count.store(0);
            debug_count.store(0);
            info_count.store(0);
            warn_count.store(0);
            warning_count.store(0);
            error_count.store(0);
            fatal_count.store(0);
            maintenance_count.store(0);
            total_logs.store(0);
            last_reset_time = std::chrono::system_clock::now();
        }
        
        // 🔥 별칭 동기화
        void SyncWarningCount() {
            warning_count.store(warn_count.load());
        }
    };
    
    /**
     * @brief 드라이버 로그 컨텍스트
     */
    struct DriverLogContext {
        UUID device_id;
        std::string device_name;
        ProtocolType protocol;
        std::string endpoint;
        std::string thread_id;
        std::string operation;
        
        DriverLogContext() = default;
        
        DriverLogContext(const UUID& dev_id, const std::string& dev_name, 
                        ProtocolType proto, const std::string& ep)
            : device_id(dev_id), device_name(dev_name), protocol(proto), endpoint(ep) {}
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