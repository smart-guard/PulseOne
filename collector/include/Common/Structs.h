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
     * @brief 통합 디바이스 정보
     * - Database::DeviceInfo (DB 저장용)
     * - Drivers::DeviceInfo (드라이버용) 
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
        
        // 🔥 통신 설정 (Duration + 호환용 int)
        Duration timeout = std::chrono::milliseconds(5000);
        int timeout_ms = 5000;                       // 기존 코드 호환용
        Duration polling_interval = std::chrono::milliseconds(1000);
        int polling_interval_ms = 1000;              // 기존 코드 호환용
        int retry_count = 3;
        
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
        
        // ✅ 생성자 - Utils 네임스페이스 사용
        DeviceInfo() 
            : timeout(std::chrono::milliseconds(5000))
            , timeout_ms(5000)
            , polling_interval(std::chrono::milliseconds(1000))
            , polling_interval_ms(1000)
            , last_communication(Utils::GetCurrentTimestamp())
            , last_seen(Utils::GetCurrentTimestamp())
            , created_at(Utils::GetCurrentTimestamp())
            , updated_at(Utils::GetCurrentTimestamp())
        {
            SyncCompatibilityFields();
        }
        
        // 🔥 호환성 메서드들
        int GetTimeoutMs() const {
            return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
        }
        
        void SetTimeoutMs(int ms) {
            timeout_ms = ms;
            timeout = std::chrono::milliseconds(ms);
        }
        
        void SyncTimeoutFields() {
            timeout_ms = GetTimeoutMs();
        }
        
        void SyncCompatibilityFields() {
            connection_string = endpoint;
            protocol_type = "unknown";  // 간단화
            last_seen = last_communication;
            
            switch(connection_status) {
                case ConnectionStatus::CONNECTED: status = "connected"; break;
                case ConnectionStatus::CONNECTING: status = "connecting"; break;
                case ConnectionStatus::RECONNECTING: status = "reconnecting"; break;
                case ConnectionStatus::ERROR: status = "error"; break;
                default: status = "disconnected"; break;
            }
        }
    };
    
    // =========================================================================
    // 통합 데이터 포인트 (🔥 모든 DataPoint 통합)
    // =========================================================================
    
    /**
     * @brief 통합 데이터 포인트 
     * - Database::DataPointInfo (DB 저장용)
     * - Drivers::DataPoint (드라이버용)
     * - 추가 필드들 통합
     */
    struct DataPoint {
        // 🔥 공통 핵심 필드들
        UUID id;                                     // point_id (Database) + id (Drivers)
        UUID device_id;
        std::string name;
        std::string description = "";
        
        // 🔥 주소 정보 (두 방식 모두 지원)
        int address = 0;                             // Drivers 방식 (정수)
        std::string address_string = "";             // Database 방식 (문자열)
        
        // 🔥 데이터 타입 및 변환
        std::string data_type = "UNKNOWN";           // Database 방식 (문자열)
        std::string unit = "";
        double scaling_factor = 1.0;
        double scaling_offset = 0.0;
        double min_value = std::numeric_limits<double>::lowest();
        double max_value = std::numeric_limits<double>::max();
        
        // 🔥 설정 (Database + Drivers 통합)
        bool is_enabled = true;
        bool is_writable = false;
        int scan_rate_ms = 0;                        // Database 방식
        double deadband = 0.0;
        
        // 🔥 로깅 설정 (Database에서 추가)
        bool log_enabled = true;
        int log_interval_ms = 0;
        double log_deadband = 0.0;
        
        // 🔥 메타데이터 (Database + Drivers 통합)
        std::vector<std::string> tags;               // Database 방식
        std::map<std::string, std::string> tag_map;  // Drivers 방식 (호환용)
        JsonType metadata;
        std::string config_json = "";                // 추가 설정
        
        // 🔥 상태 정보
        Timestamp last_read_time;
        Timestamp last_write_time;
        uint64_t read_count = 0;
        uint64_t write_count = 0;
        uint64_t error_count = 0;
        
        // 🔥 Database 시간 필드들
        Timestamp created_at;
        Timestamp updated_at;
        
        // ✅ 생성자 - Utils 네임스페이스 사용
        DataPoint() 
            : last_read_time(Utils::GetCurrentTimestamp())
            , last_write_time(Utils::GetCurrentTimestamp())
            , created_at(Utils::GetCurrentTimestamp())
            , updated_at(Utils::GetCurrentTimestamp())
        {}
        
        // 🔥 호환성을 위한 연산자들 (STL 컨테이너용)
        bool operator<(const DataPoint& other) const {
            return id < other.id;
        }
        
        bool operator==(const DataPoint& other) const {
            return id == other.id;
        }
        
        // 🔥 호환성 메서드들
        void SyncAddressFields() {
            if (address != 0 && address_string.empty()) {
                address_string = std::to_string(address);
            } else if (address == 0 && !address_string.empty()) {
                try {
                    address = std::stoi(address_string);
                } catch (...) {
                    address = 0;
                }
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
        // 🔥 공통 필드들 (모든 버전에서 공통)
        UUID device_id;                              // 일부는 string, 일부는 int였음 -> UUID로 통일
        std::string name = "";
        ProtocolType protocol = ProtocolType::UNKNOWN;
        std::string endpoint = "";
        
        // 🔥 타이밍 설정
        Duration timeout = std::chrono::seconds(5);
        int retry_count = 3;
        Duration polling_interval = std::chrono::seconds(1);
        
        // 🔥 추가 설정들
        std::map<std::string, std::string> properties;  // 🔥 BACnetDriver에서 사용하는 필드
        std::map<std::string, std::string> custom_settings;
        JsonType config_json;
        
        // 🔥 호환성 필드들 (일부 코드에서 요구)
        uint32_t timeout_ms = 5000;                 // Duration과 동기화
        uint32_t polling_interval_ms = 1000;        // Duration과 동기화  
        bool auto_reconnect = true;
        int device_instance = 0;                    // BACnet용 호환 필드

        // 🔥 생성자에서 필드 동기화
        DriverConfig() {
            SyncDurationFields();
        }
        
        // 🔥 Duration 필드와 ms 필드 동기화
        void SyncDurationFields() {
            timeout_ms = static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count()
            );
            polling_interval_ms = static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(polling_interval).count()
            );
        }
        
        // 🔥 ms 필드에서 Duration으로 역동기화
        void SyncFromMs() {
            timeout = std::chrono::milliseconds(timeout_ms);
            polling_interval = std::chrono::milliseconds(polling_interval_ms);
        }
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
    
} // namespace PulseOne::Structs

#endif // PULSEONE_COMMON_STRUCTS_H