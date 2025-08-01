#ifndef PULSEONE_COMMON_STRUCTS_H
#define PULSEONE_COMMON_STRUCTS_H

/**
 * @file Structs.h
 * @brief PulseOne 통합 구조체 정의 (모든 중복 제거)
 * @author PulseOne Development Team
 * @date 2025-07-24
 * 
 * 통합된 구조체들:
 * - Database::DeviceInfo + Drivers::DeviceInfo -> DeviceInfo
 * - Database::DataPointInfo + Drivers::DataPoint -> DataPoint
 * - Drivers::DriverConfig (여러 버전) -> DriverConfig
 * - 점검 기능 추가 (MaintenanceState, 품질 관리 등)
 */

#include "BasicTypes.h"
#include "Enums.h"
#include "Constants.h"
#include "Utils.h"
#include <vector>
#include <optional>
#include <mutex>
#include <chrono>
#include <string>
#include <map>
#include <limits>

// nlohmann/json 조건부 include
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

namespace PulseOne::Structs {
    
    // ✅ 모든 필요한 네임스페이스와 함수 별칭 명시적 선언
    using namespace PulseOne::BasicTypes;
    using namespace PulseOne::Enums;
    using JsonType = json_impl::json;
    
    // ✅ Utils 네임스페이스 별칭 (함수 직접 별칭은 불가능)
    namespace Utils = PulseOne::Utils;
    
    // =========================================================================
    // 기본 타입 별칭들 (기존 CommonTypes.h에서 통합)
    // =========================================================================
    
    /**
     * @brief 데이터 값 타입 (모든 드라이버에서 사용)
     */
    using DataValue = DataVariant;
    
    // =========================================================================
    // 드라이버 관련 열거형들 (기존 CommonTypes.h에서 통합)
    // =========================================================================
    
    /**
     * @brief 드라이버 상태 (기존 여러 정의 통합)
     */
    enum class DriverStatus : uint8_t {
        UNINITIALIZED = 0,
        INITIALIZING = 1,
        INITIALIZED = 2,
        STARTING = 3,
        RUNNING = 4,
        PAUSING = 5,
        PAUSED = 6,
        STOPPING = 7,
        STOPPED = 8,
        ERROR = 9,
        CRASHED = 10,
        MAINTENANCE = 11  // 🆕 점검 모드 추가
    };
    
    // =========================================================================
    // 🔥 에러 정보 구조체 (먼저 정의 - 다른 구조체에서 사용하기 위해)
    // =========================================================================
    
    /**
     * @brief 에러 정보 구조체
     */
    struct ErrorInfo {
        ErrorCode code = ErrorCode::SUCCESS;
        std::string message = "";
        std::string details = "";
        Timestamp occurred_at;
        
        // ✅ 생성자들 - Utils 네임스페이스 사용
        ErrorInfo() : occurred_at(Utils::GetCurrentTimestamp()) {}
        ErrorInfo(ErrorCode err_code, const std::string& msg) 
            : code(err_code), message(msg), occurred_at(Utils::GetCurrentTimestamp()) {}
    };
    
    // =========================================================================
    // 점검 관련 구조체들 (🆕 새로운 기능)
    // =========================================================================
    
    /**
     * @brief 현장 점검 상태 정보
     */
    struct MaintenanceState {
        MaintenanceStatus status = MaintenanceStatus::NORMAL;
        MaintenanceType type = MaintenanceType::SCHEDULED;
        EngineerID engineer_id = "";
        std::string engineer_name = "";
        std::string contact_info = "";
        Timestamp start_time;
        Timestamp expected_end_time;
        std::string work_description = "";
        bool remote_control_blocked = false;
        bool data_collection_paused = false;
        std::string emergency_contact = "";
        
        // ✅ Utils 네임스페이스 사용 (네임스페이스 에러 해결)
        MaintenanceState() : start_time(Utils::GetCurrentTimestamp()), expected_end_time(Utils::GetCurrentTimestamp()) {}
    };
    
    // =========================================================================
    // 통합 디바이스 정보 (🔥 모든 DeviceInfo 통합)
    // =========================================================================
    
    /**
     * @brief 통합 디바이스 정보 (🔥 DeviceSettings 필드들 통합 완료)
     * - Database::DeviceInfo (DB 저장용)
     * - Drivers::DeviceInfo (드라이버용) 
     * - DeviceSettings 모든 필드 포함
     * - 점검 기능 추가
     */
    struct DeviceInfo {
        // 🔥 공통 핵심 필드들
        UUID id;
        uint32_t tenant_id = 0;
        std::string name;
        std::string description = "";
        ProtocolType protocol = ProtocolType::UNKNOWN;
        std::string endpoint = "";
        
        // 🔥 Database 호환 필드들
        std::string connection_string = "";          // endpoint와 동일
        std::string protocol_type = "unknown";       // protocol을 문자열로
        JsonType connection_config;                  // JSON 설정
        std::string status = "disconnected";         // 문자열 상태
        
        // =========================================================================
        // 🆕 DeviceSettings 통합 필드들 (새로 추가)
        // =========================================================================
        
        // 🔥 기본 타이밍 설정 (기존 + DeviceSettings)
        Duration timeout = std::chrono::milliseconds(5000);
        int timeout_ms = 5000;                       // 기존 코드 호환용
        int connection_timeout_ms = 10000;           // 🆕 DeviceSettings - 연결 타임아웃
        Duration polling_interval = std::chrono::milliseconds(1000);
        int polling_interval_ms = 1000;              // 기존 + DeviceSettings
        
        // 🔥 재시도 설정 (기존 + DeviceSettings 확장)
        int retry_count = 3;                         // 기존 (호환성)
        int max_retry_count = 3;                     // 🆕 DeviceSettings - 최대 재시도 횟수
        int retry_interval_ms = 5000;                // 🆕 DeviceSettings - 재시도 간격
        int backoff_time_ms = 60000;                 // 🆕 DeviceSettings - 백오프 시간 (1분)
        double backoff_multiplier = 1.5;             // 🆕 DeviceSettings - 백오프 배수
        int max_backoff_time_ms = 300000;            // 🆕 DeviceSettings - 최대 백오프 시간 (5분)
        
        // 🔥 Keep-Alive 설정 (🆕 DeviceSettings에서 추가)
        bool keep_alive_enabled = true;              // 🆕 Keep-Alive 활성화
        int keep_alive_interval_s = 30;              // 🆕 Keep-Alive 간격 (30초)
        int keep_alive_timeout_s = 10;               // 🆕 Keep-Alive 타임아웃 (10초)
        
        // 🔥 세부 타임아웃 설정 (🆕 DeviceSettings에서 추가)
        int read_timeout_ms = 5000;                  // 🆕 읽기 타임아웃 (5초)
        int write_timeout_ms = 5000;                 // 🆕 쓰기 타임아웃 (5초)
        
        // 🔥 고급 기능 플래그들 (🆕 DeviceSettings에서 추가)
        bool data_validation_enabled = true;         // 🆕 데이터 검증 활성화
        bool performance_monitoring_enabled = true;  // 🆕 성능 모니터링 활성화
        bool diagnostic_mode_enabled = false;        // 🆕 진단 모드 활성화
        
        // 🔥 선택적 설정들 (🆕 DeviceSettings에서 추가)
        std::optional<int> scan_rate_override;       // 🆕 개별 스캔 레이트 오버라이드
        
        // =========================================================================
        // 🔥 기존 필드들 (변경 없음)
        // =========================================================================
        
        // 🔥 연결 상태 관리
        ConnectionStatus connection_status = ConnectionStatus::DISCONNECTED;
        bool is_enabled = true;
        bool auto_reconnect = true;
        Duration reconnect_delay = std::chrono::seconds(5);
        
        // 🆕 점검 기능
        MaintenanceState maintenance_state;
        bool maintenance_allowed = true;
        
        // 🔥 시간 정보 (Database 호환)
        Timestamp last_communication;
        Timestamp last_seen;                         // last_communication과 동일
        Timestamp created_at;
        Timestamp updated_at;
        
        // 🔥 그룹 및 메타데이터 (Database 호환)
        std::optional<UUID> device_group_id;
        std::vector<std::string> tags;
        JsonType metadata;
        
        // 🔥 디바이스 상세 정보 (DeviceEntity에서 가져올 필드들)
        std::string device_type = "";                // 🆕 DeviceEntity.getDeviceType()
        std::string manufacturer = "";               // 🆕 DeviceEntity.getManufacturer()
        std::string model = "";                      // 🆕 DeviceEntity.getModel()
        std::string serial_number = "";              // 🆕 DeviceEntity.getSerialNumber()

        // ✅ 생성자 - Utils 네임스페이스 사용 + 새 필드들 초기화
        DeviceInfo() 
            : timeout(std::chrono::milliseconds(5000))
            , timeout_ms(5000)
            , connection_timeout_ms(10000)           // 🆕 초기값 설정
            , polling_interval(std::chrono::milliseconds(1000))
            , polling_interval_ms(1000)
            , retry_count(3)
            , max_retry_count(3)                     // 🆕 초기값 설정
            , retry_interval_ms(5000)                // 🆕 초기값 설정
            , backoff_time_ms(60000)                 // 🆕 초기값 설정 (1분)
            , backoff_multiplier(1.5)                // 🆕 초기값 설정
            , max_backoff_time_ms(300000)            // 🆕 초기값 설정 (5분)
            , keep_alive_enabled(true)               // 🆕 초기값 설정
            , keep_alive_interval_s(30)              // 🆕 초기값 설정 (30초)
            , keep_alive_timeout_s(10)               // 🆕 초기값 설정 (10초)
            , read_timeout_ms(5000)                  // 🆕 초기값 설정 (5초)
            , write_timeout_ms(5000)                 // 🆕 초기값 설정 (5초)
            , data_validation_enabled(true)          // 🆕 초기값 설정
            , performance_monitoring_enabled(true)   // 🆕 초기값 설정
            , diagnostic_mode_enabled(false)         // 🆕 초기값 설정
            , last_communication(Utils::GetCurrentTimestamp())
            , last_seen(Utils::GetCurrentTimestamp())
            , created_at(Utils::GetCurrentTimestamp())
            , updated_at(Utils::GetCurrentTimestamp())
        {
            SyncCompatibilityFields();
        }
        
        // 🔥 호환성 메서드들 (기존 + 새 필드 동기화)
        int GetTimeoutMs() const {
            return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
        }
        
        void SetTimeoutMs(int ms) {
            timeout_ms = ms;
            timeout = std::chrono::milliseconds(ms);
            // 🆕 connection_timeout_ms와 동기화할지 결정 (기본적으로 별개)
        }
        
        void SyncTimeoutFields() {
            timeout_ms = GetTimeoutMs();
        }
        
        // 🆕 새로운 동기화 메서드들
        void SyncRetryFields() {
            retry_count = max_retry_count;  // 호환성을 위한 동기화
        }
        
        void SyncPollingFields() {
            polling_interval_ms = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(polling_interval).count());
        }
        
        void SyncCompatibilityFields() {
            connection_string = endpoint;
            protocol_type = "unknown";  // 간단화
            last_seen = last_communication;
            
            // 🆕 새 필드들 동기화
            SyncRetryFields();
            SyncPollingFields();
            SyncTimeoutFields();
            
            switch(connection_status) {
                case ConnectionStatus::CONNECTED: status = "connected"; break;
                case ConnectionStatus::CONNECTING: status = "connecting"; break;
                case ConnectionStatus::RECONNECTING: status = "reconnecting"; break;
                case ConnectionStatus::ERROR: status = "error"; break;
                default: status = "disconnected"; break;
            }
        }
        
        // =========================================================================
        // 🆕 DeviceSettings 관련 헬퍼 메서드들
        // =========================================================================
        
        /**
         * @brief DeviceSettings 값들이 유효한지 검증
         */
        bool ValidateDeviceSettings() const {
            if (polling_interval_ms <= 0) return false;
            if (connection_timeout_ms <= 0) return false;
            if (max_retry_count < 0) return false;
            if (retry_interval_ms <= 0) return false;
            if (backoff_time_ms <= 0) return false;
            if (keep_alive_interval_s <= 0) return false;
            if (read_timeout_ms <= 0) return false;
            if (write_timeout_ms <= 0) return false;
            if (backoff_multiplier <= 0.0) return false;
            if (max_backoff_time_ms <= 0) return false;
            if (keep_alive_timeout_s <= 0) return false;
            return true;
        }
        
        /**
         * @brief 산업용 기본값으로 설정
         */
        void SetIndustrialDefaults() {
            polling_interval_ms = 1000;          // 1초
            connection_timeout_ms = 10000;       // 10초
            max_retry_count = 3;
            retry_interval_ms = 5000;            // 5초
            backoff_time_ms = 60000;             // 1분
            keep_alive_enabled = true;
            keep_alive_interval_s = 30;          // 30초
            read_timeout_ms = 5000;              // 5초
            write_timeout_ms = 5000;             // 5초
            backoff_multiplier = 1.5;
            max_backoff_time_ms = 300000;        // 5분
            keep_alive_timeout_s = 10;           // 10초
            data_validation_enabled = true;
            performance_monitoring_enabled = true;
            diagnostic_mode_enabled = false;
            
            SyncCompatibilityFields();
        }
        
        /**
         * @brief 고속 모드 설정
         */
        void SetHighSpeedMode() {
            polling_interval_ms = 500;           // 500ms
            connection_timeout_ms = 3000;        // 3초
            read_timeout_ms = 2000;              // 2초
            write_timeout_ms = 2000;             // 2초
            retry_interval_ms = 2000;            // 2초 간격
            keep_alive_interval_s = 10;          // 10초 주기
            
            SyncCompatibilityFields();
        }
        
        /**
         * @brief 안정성 모드 설정
         */
        void SetStabilityMode() {
            polling_interval_ms = 5000;          // 5초
            connection_timeout_ms = 30000;       // 30초
            max_retry_count = 5;                 // 5회 재시도
            retry_interval_ms = 10000;           // 10초 간격
            backoff_time_ms = 120000;            // 2분 백오프
            keep_alive_interval_s = 60;          // 1분 주기
            
            SyncCompatibilityFields();
        }
        
        /**
         * @brief DeviceSettings 정보를 JSON으로 출력 (디버깅용)
         */
        JsonType GetDeviceSettingsJson() const {
            JsonType json;
            json["polling_interval_ms"] = polling_interval_ms;
            json["connection_timeout_ms"] = connection_timeout_ms;
            json["max_retry_count"] = max_retry_count;
            json["retry_interval_ms"] = retry_interval_ms;
            json["backoff_time_ms"] = backoff_time_ms;
            json["backoff_multiplier"] = backoff_multiplier;
            json["max_backoff_time_ms"] = max_backoff_time_ms;
            json["keep_alive_enabled"] = keep_alive_enabled;
            json["keep_alive_interval_s"] = keep_alive_interval_s;
            json["keep_alive_timeout_s"] = keep_alive_timeout_s;
            json["read_timeout_ms"] = read_timeout_ms;
            json["write_timeout_ms"] = write_timeout_ms;
            json["data_validation_enabled"] = data_validation_enabled;
            json["performance_monitoring_enabled"] = performance_monitoring_enabled;
            json["diagnostic_mode_enabled"] = diagnostic_mode_enabled;
            if (scan_rate_override.has_value()) {
                json["scan_rate_override"] = scan_rate_override.value();
            }
            return json;
        }
    };
    
    // =========================================================================
    // 통합 데이터 포인트 (🔥 모든 DataPoint 통합)
    // =========================================================================
    
    /**
     * @brief 통합 데이터 포인트 구조체 (완성본 v2)
     * - Database::DataPointInfo (DB 저장용)
     * - Drivers::DataPoint (드라이버용)  
     * - WorkerFactory 완전 호환
     * - 현재값/품질코드/통계 필드 추가 완료
     */
    struct DataPoint {
        // =======================================================================
        // 🔥 공통 핵심 필드들
        // =======================================================================
        UUID id;                                     // point_id (Database) + id (Drivers)
        UUID device_id;
        std::string name;
        std::string description = "";
        
        // =======================================================================
        // 🔥 주소 정보 (정규화)
        // =======================================================================
        uint32_t address = 0;                        // 숫자 주소 (Modbus 레지스터, BACnet 인스턴스 등)
        std::string address_string = "";             // 문자열 주소 (MQTT 토픽, OPC UA NodeId 등)
        
        // =======================================================================
        // 🔥 데이터 타입 및 접근성
        // =======================================================================
        std::string data_type = "UNKNOWN";           // INT16, UINT32, FLOAT, BOOL, STRING 등
        std::string access_mode = "read";            // read, write, read_write
        bool is_enabled = true;
        bool is_writable = false;                    // 쓰기 가능 여부 (계산된 필드)
        
        // =======================================================================
        // 🔥 엔지니어링 단위 및 스케일링
        // =======================================================================
        std::string unit = "";                       // 단위 (°C, %, kW 등)
        double scaling_factor = 1.0;                 // 스케일링 팩터
        double scaling_offset = 0.0;                 // 스케일링 오프셋
        double min_value = 0.0;                      // 최솟값 제한
        double max_value = 0.0;                      // 최댓값 제한
        
        // =======================================================================
        // 🔥 로깅 설정 (새로 추가)
        // =======================================================================
        bool log_enabled = true;                     // 로깅 활성화 여부
        uint32_t log_interval_ms = 0;                // 로깅 간격 (0=변화 시에만)
        double log_deadband = 0.0;                   // 로깅 데드밴드
        Timestamp last_log_time = {};                // 마지막 로그 시간
        
        // =======================================================================
        // 🔥 메타데이터 및 태그
        // =======================================================================
        std::vector<std::string> tags;               // 태그 목록
        std::map<std::string, std::string> metadata; // 추가 메타데이터
        std::map<std::string, std::string> properties; // 프로토콜별 속성
        
        // =======================================================================
        // 🔥 시간 정보
        // =======================================================================
        Timestamp created_at = {};
        Timestamp updated_at = {};
        Timestamp last_read_time = {};               // 마지막 읽기 시간
        Timestamp last_write_time = {};              // 마지막 쓰기 시간
        
        // =======================================================================
        // 🔥 현재값 관리 (WorkerFactory 필수 필드들) - 새로 추가
        // =======================================================================
        DataVariant current_value;                   // 현재값 (실시간 데이터)
        DataQuality quality_code = DataQuality::GOOD; // 데이터 품질 코드
        Timestamp value_timestamp = {};              // 값 타임스탬프
        Timestamp quality_timestamp = {};            // 품질 변경 시각
        
        // =======================================================================
        // 🔥 통계 정보 (새로 추가)
        // =======================================================================
        uint64_t read_count = 0;                     // 읽기 횟수
        uint64_t write_count = 0;                    // 쓰기 횟수
        uint64_t error_count = 0;                    // 오류 횟수
        
        // =======================================================================
        // ✅ 생성자
        // =======================================================================
        DataPoint() 
            : last_log_time(std::chrono::system_clock::now())
            , created_at(std::chrono::system_clock::now())
            , updated_at(std::chrono::system_clock::now())
            , last_read_time(std::chrono::system_clock::now())
            , last_write_time(std::chrono::system_clock::now())
            , current_value(0.0)                                    // 기본값 설정
            , value_timestamp(std::chrono::system_clock::now())
            , quality_timestamp(std::chrono::system_clock::now())
        {}
        
        // =======================================================================
        // 🔥 호환성을 위한 연산자들 (STL 컨테이너용)
        // =======================================================================
        bool operator<(const DataPoint& other) const {
            return id < other.id;
        }
        
        bool operator==(const DataPoint& other) const {
            return id == other.id;
        }
        
        // =======================================================================
        // 🔥 핵심 실용 메서드들 (새로 추가)
        // =======================================================================
        
        /**
         * @brief 주소 필드 동기화
         */
        void SyncAddressFields() {
            if (address != 0 && address_string.empty()) {
                address_string = std::to_string(address);
            } else if (address == 0 && !address_string.empty()) {
                try {
                    address = std::stoul(address_string);
                } catch (...) {
                    address = 0;
                }
            }
        }
        
        /**
         * @brief 현재값을 문자열로 변환
         */
        std::string GetCurrentValueAsString() const {
            try {
                return std::visit([](const auto& value) -> std::string {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<T, std::string>) {
                        return value;
                    } else if constexpr (std::is_same_v<T, bool>) {
                        return value ? "true" : "false";
                    } else if constexpr (std::is_arithmetic_v<T>) {
                        return std::to_string(value);
                    } else {
                        return "unknown";
                    }
                }, current_value);
            } catch (...) {
                return "error";
            }
        }
        
        /**
         * @brief 품질 코드를 문자열로 변환
         */
        std::string GetQualityCodeAsString() const {
            return PulseOne::Utils::DataQualityToString(quality_code);
        }
        
        /**
         * @brief 현재값 업데이트 (품질과 함께)
         */
        void UpdateCurrentValue(const DataVariant& new_value, DataQuality new_quality = DataQuality::GOOD) {
            current_value = new_value;
            
            // 품질이 변경된 경우에만 타임스탬프 업데이트
            if (quality_code != new_quality) {
                quality_code = new_quality;
                quality_timestamp = std::chrono::system_clock::now();
            }
            
            value_timestamp = std::chrono::system_clock::now();
            updated_at = std::chrono::system_clock::now();
            
            // 통계 업데이트
            read_count++;
            if (new_quality == DataQuality::BAD || new_quality == DataQuality::TIMEOUT) {
                error_count++;
            }
        }
        
        /**
         * @brief 쓰기 가능 여부 확인
         */
        bool IsWritable() const {
            return is_writable || access_mode == "write" || access_mode == "read_write";
        }
        
        /**
         * @brief 로깅이 필요한지 확인 (interval + deadband 기반)
         */
        bool ShouldLog(const DataVariant& new_value) const {
            if (!log_enabled) return false;
            
            // 시간 간격 체크
            auto now = std::chrono::system_clock::now();
            auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_log_time).count();
            
            if (log_interval_ms > 0 && time_diff < static_cast<int64_t>(log_interval_ms)) {
                return false; // 아직 로깅 간격이 안됨
            }
            
            // 데드밴드 체크 (숫자 값만)
            if (log_deadband > 0.0) {
                try {
                    double current_val = std::visit([](const auto& val) -> double {
                        using T = std::decay_t<decltype(val)>;
                        if constexpr (std::is_arithmetic_v<T>) {
                            return static_cast<double>(val);
                        }
                        return 0.0;
                    }, current_value);
                    
                    double new_val = std::visit([](const auto& val) -> double {
                        using T = std::decay_t<decltype(val)>;
                        if constexpr (std::is_arithmetic_v<T>) {
                            return static_cast<double>(val);
                        }
                        return 0.0;
                    }, new_value);
                    
                    if (std::abs(new_val - current_val) < log_deadband) {
                        return false; // 변화량이 데드밴드 미만
                    }
                } catch (...) {
                    // 숫자가 아닌 경우 데드밴드 무시
                }
            }
            
            return true;
        }
        
        /**
         * @brief 스케일링된 값 계산
         */
        double GetScaledValue(const DataVariant& raw_value) const {
            try {
                double raw_num = std::visit([](const auto& val) -> double {
                    using T = std::decay_t<decltype(val)>;
                    if constexpr (std::is_arithmetic_v<T>) {
                        return static_cast<double>(val);
                    }
                    return 0.0;
                }, raw_value);
                
                return (raw_num * scaling_factor) + scaling_offset;
            } catch (...) {
                return 0.0;
            }
        }
        
        /**
         * @brief 유효성 검사
         */
        bool IsValid() const {
            return !id.empty() && 
                !device_id.empty() && 
                !name.empty() && 
                scaling_factor != 0.0 &&
                min_value <= max_value;
        }
        
        /**
         * @brief 디버깅용 문자열 출력
         */
        std::string ToDebugString() const {
            std::ostringstream oss;
            oss << "DataPoint{" 
                << "id='" << id << "'"
                << ", name='" << name << "'"
                << ", address=" << address
                << ", value=" << GetCurrentValueAsString()
                << ", quality=" << GetQualityCodeAsString()
                << ", writable=" << (IsWritable() ? "true" : "false")
                << ", log_enabled=" << (log_enabled ? "true" : "false")
                << ", read_count=" << read_count
                << ", error_count=" << error_count
                << "}";
            return oss.str();
        }
        /**
         * @brief 품질이 양호한지 확인
         */
        bool IsGoodQuality() const {
            return quality_code == DataQuality::GOOD;
        }

        /**
         * @brief 품질이 나쁜지 확인
         */
        bool IsBadQuality() const {
            return quality_code == DataQuality::BAD || 
                quality_code == DataQuality::NOT_CONNECTED ||
                quality_code == DataQuality::TIMEOUT;
        }

        /**
         * @brief 현재값이 유효한지 확인 (품질 + 범위 체크)
         */
        bool IsCurrentValueValid() const {
            if (!IsGoodQuality()) return false;
            
            try {
                double value = std::visit([](const auto& val) -> double {
                    using T = std::decay_t<decltype(val)>;
                    if constexpr (std::is_arithmetic_v<T>) {
                        return static_cast<double>(val);
                    }
                    return 0.0;
                }, current_value);
                
                // 범위 체크 (min_value와 max_value가 설정된 경우만)
                if (min_value != std::numeric_limits<double>::lowest() && value < min_value) {
                    return false;
                }
                if (max_value != std::numeric_limits<double>::max() && value > max_value) {
                    return false;
                }
                
                return true;
            } catch (...) {
                return false;
            }
        }
    };
    
    // =========================================================================
    // 드라이버 관련 구조체들 (기존 CommonTypes.h에서 통합)
    // =========================================================================
    
    /**
     * @brief 통합 드라이버 설정 (여러 DriverConfig 통합)
     */
    struct DriverConfig {
        // 🔥 기존 필드들 (현재 있음)
        UUID device_id;
        std::string name = "";
        ProtocolType protocol = ProtocolType::UNKNOWN;
        std::string endpoint = "";
        Duration timeout = std::chrono::seconds(5);
        int retry_count = 3;
        Duration polling_interval = std::chrono::seconds(1);
        std::map<std::string, std::string> properties;
        std::map<std::string, std::string> custom_settings;
        JsonType config_json;
        uint32_t timeout_ms = 5000;
        uint32_t polling_interval_ms = 1000;
        bool auto_reconnect = true;
        int device_instance = 0;

        // ✅ 추가 필요한 필드들 (MqttDriver에서 요구)
        
        // 1. 설정 문자열 필드 (가장 중요!)
        std::string connection_string = "";     // MQTTWorker에서 JSON 파싱용
        std::string extra_config = "";          // 추가 설정용 (JSON 문자열)
        
        // 2. MQTT 특화 필드들
        std::string username = "";              // MQTT 인증 사용자명
        std::string password = "";              // MQTT 인증 비밀번호
        std::string client_id = "";             // MQTT 클라이언트 ID
        bool use_ssl = false;                   // SSL/TLS 사용 여부
        int keep_alive_interval = 60;           // Keep-alive 간격 (초)
        bool clean_session = true;              // Clean session 플래그
        int qos_level = 1;                      // 기본 QoS 레벨
        
        // 3. 프로토콜별 공통 필드들
        std::string protocol_version = "";      // 프로토콜 버전
        std::map<std::string, JsonType> protocol_settings;  // 프로토콜별 설정들
        
        // 4. 로깅 및 진단 필드들
        bool enable_logging = true;             // 로깅 활성화
        bool enable_diagnostics = false;        // 진단 모드
        bool enable_debug = false;              // 디버그 모드
        std::string log_level = "INFO";         // 로그 레벨
    };
    
    /**
     * @brief 드라이버 상태 정보 (자세한 정보 포함)
     */
    struct DriverState {
        bool is_connected = false;
        ErrorInfo last_error;  // 🔥 Structs:: 제거 - 같은 네임스페이스 내에서는 직접 참조
        Timestamp connection_time;
        std::map<std::string, std::string> additional_info;
        
        // 기본 생성자
        DriverState() 
            : connection_time(Utils::GetCurrentTimestamp()) {}
    };
    
    /**
     * @brief 드라이버 통계 (기존 CommonTypes.h + 확장)
     */
    struct DriverStatistics {
        uint64_t total_reads = 0;
        uint64_t total_writes = 0;
        uint64_t successful_reads = 0;
        uint64_t successful_writes = 0;
        uint64_t failed_reads = 0;
        uint64_t failed_writes = 0;
        
        // 🔥 BACnetWorker에서 요구하는 필드들 추가
        uint64_t successful_connections = 0;
        uint64_t failed_connections = 0;
        uint64_t total_operations = 0;
        uint64_t successful_operations = 0;
        uint64_t failed_operations = 0;
        uint64_t consecutive_failures = 0;
        
        // 🔥 시간 관련 필드들
        Timestamp last_read_time;
        Timestamp last_write_time;
        Timestamp last_error_time;
        Timestamp start_time;
        Duration average_response_time = std::chrono::milliseconds(0);
        
        // 🔥 IProtocolDriver에서 요구하는 필드들
        uint64_t uptime_seconds = 0;
        double avg_response_time_ms = 0.0;
        double max_response_time_ms = 0.0;
        double min_response_time_ms = 0.0;
        Timestamp last_success_time;
        Timestamp last_connection_time;
        double success_rate = 0.0;
        
        // ✅ 생성자 - Utils 네임스페이스 사용
        DriverStatistics() 
            : last_read_time(Utils::GetCurrentTimestamp())
            , last_write_time(Utils::GetCurrentTimestamp())
            , last_error_time(Utils::GetCurrentTimestamp())
            , start_time(Utils::GetCurrentTimestamp()) 
        {}
        
        double GetSuccessRate() const {
            uint64_t total = total_reads + total_writes;
            if (total == 0) return 100.0;
            uint64_t successful = successful_reads + successful_writes;
            double rate = (static_cast<double>(successful) / total) * 100.0;
            // success_rate 필드도 동기화
            const_cast<DriverStatistics*>(this)->success_rate = rate;
            return rate;
        }
        
        // 🔥 avg_response_time_ms 동기화
        void SyncResponseTime() {
            avg_response_time_ms = static_cast<double>(
                std::chrono::duration_cast<std::chrono::milliseconds>(average_response_time).count()
            );
        }
        
        // 🔥 total_operations 계산
        void UpdateTotalOperations() {
            total_operations = total_reads + total_writes;
            successful_operations = successful_reads + successful_writes;
            failed_operations = failed_reads + failed_writes;
        }
    };
    
    // =========================================================================
    // 기타 구조체들
    // =========================================================================
    
    /**
     * @brief 타임스탬프가 포함된 데이터 값
     */
    struct TimestampedValue {
        DataVariant value;
        DataQuality quality = DataQuality::GOOD;
        Timestamp timestamp;
        UUID source_id = "";
        
        // 🆕 점검 기능 관련
        Duration age_ms = std::chrono::milliseconds(0);
        bool under_maintenance = false;
        std::string engineer_id = "";
        
        // ✅ 생성자들 - Utils 네임스페이스 사용
        TimestampedValue() : timestamp(Utils::GetCurrentTimestamp()) {}
        TimestampedValue(const DataVariant& val, DataQuality qual = DataQuality::GOOD)
            : value(val), quality(qual), timestamp(Utils::GetCurrentTimestamp()) {}
        
        bool IsValid() const noexcept {
            return quality == DataQuality::GOOD || quality == DataQuality::UNCERTAIN;
        }
    };
    
    /**
     * @brief 드라이버 로그 컨텍스트
     */
    struct DriverLogContext {
        UUID device_id = "";
        std::string device_name = "";
        ProtocolType protocol = ProtocolType::UNKNOWN;
        std::string endpoint = "";
        std::string worker_id = "";
        
        DriverLogContext() = default;
        DriverLogContext(const UUID& dev_id, const std::string& dev_name, ProtocolType proto)
            : device_id(dev_id), device_name(dev_name), protocol(proto) {}
    };
    
    /**
     * @brief 로그 통계 정보
     */
    struct LogStatistics {
        uint64_t total_logs = 0;
        uint64_t error_count = 0;
        uint64_t warning_count = 0;
        uint64_t info_count = 0;
        uint64_t debug_count = 0;
        uint64_t trace_count = 0;
        uint64_t maintenance_count = 0;
        Timestamp last_reset_time;
        
        // ✅ 생성자 - Utils 네임스페이스 사용
        LogStatistics() : last_reset_time(Utils::GetCurrentTimestamp()) {}
        
        double GetErrorRate() const {
            return (total_logs > 0) ? (static_cast<double>(error_count) / total_logs) * 100.0 : 0.0;
        }
    };
    
} // namespace PulseOne::Structs

#endif // PULSEONE_COMMON_STRUCTS_H