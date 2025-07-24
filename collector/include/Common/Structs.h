#ifndef PULSEONE_COMMON_STRUCTS_H
#define PULSEONE_COMMON_STRUCTS_H

/**
 * @file Structs.h
 * @brief PulseOne 구조체 정의 (기존 중복 통합 + 점검 기능)
 * @author PulseOne Development Team
 * @date 2025-07-24
 * 
 * 기존 중복 구조체들 통합:
 * - Database::DeviceInfo + Drivers::DeviceInfo -> DeviceInfo
 * - Database::DataPointInfo + Drivers::DataPoint -> DataPoint
 * - 점검 기능 추가 (MaintenanceState, 품질 관리 등)
 */

#include "BasicTypes.h"
#include "Enums.h"
#include "Constants.h"
#include <vector>
#include <optional>
#include <mutex>
#include <chrono>
#include <string>
#include <map>
#include <thread>
#include <sstream>

namespace PulseOne::Structs {
    
    // BasicTypes 네임스페이스에서 타입들 가져오기
    using namespace PulseOne::BasicTypes;
    using namespace PulseOne::Enums;
    
    /**
     * @brief 현장 점검 상태 정보 (🆕 핵심 신규 기능)
     */
    struct MaintenanceState {
        MaintenanceStatus status = MaintenanceStatus::NORMAL;
        MaintenanceType type = MaintenanceType::ROUTINE;
        EngineerID engineer_id = "";                 // 현재 작업 중인 엔지니어 ID
        std::string engineer_name = "";              // 엔지니어 이름
        std::string contact_info = "";               // 연락처 (비상시)
        Timestamp start_time;                        // 점검 시작 시간
        Timestamp expected_end_time;                 // 예상 종료 시간
        std::string work_description = "";           // 작업 내용 설명
        bool remote_control_blocked = false;         // 🔥 원격 제어 차단 여부 (중요!)
        RemoteControlStatus remote_status = RemoteControlStatus::ENABLED;
        std::vector<std::string> blocked_operations; // 차단된 작업 목록
        std::string notes = "";                      // 특이사항
        
        // 자동 해제 설정
        Duration auto_release_duration = std::chrono::hours(8);
        bool auto_release_enabled = true;
        
        // 🔧 mutex 제거 - 스레드 안전성은 상위 레벨에서 관리
        
        /**
         * @brief 점검 중인지 확인
         */
        bool IsUnderMaintenance() const {
            return status == MaintenanceStatus::UNDER_MAINTENANCE ||
                   status == MaintenanceStatus::EMERGENCY_STOP;
        }
        
        /**
         * @brief 원격 제어가 차단되었는지 확인
         */
        bool IsRemoteControlBlocked() const {
            return remote_control_blocked || 
                   remote_status == RemoteControlStatus::BLOCKED_BY_MAINTENANCE ||
                   remote_status == RemoteControlStatus::BLOCKED_BY_ENGINEER ||
                   remote_status == RemoteControlStatus::EMERGENCY_BLOCKED;
        }
    };
    
    /**
     * @brief 범용 주소 구조체 (모든 프로토콜 지원)
     */
    struct UniversalAddress {
        ProtocolType protocol = ProtocolType::UNKNOWN;
        
        // Modbus 주소
        uint16_t modbus_register = 0;
        uint8_t modbus_slave_id = 1;
        
        // MQTT 토픽
        std::string mqtt_topic = "";
        
        // BACnet 객체
        uint32_t bacnet_device_instance = 0;
        uint16_t bacnet_object_type = 0;
        uint32_t bacnet_object_instance = 0;
        uint8_t bacnet_property_id = 0;
        
        // OPC UA 노드
        std::string opcua_node_id = "";
        
        // 사용자 정의 주소
        std::string custom_address = "";
        
        /**
         * @brief 주소가 유효한지 확인
         */
        bool IsValid() const {
            return protocol != ProtocolType::UNKNOWN &&
                   !GetAddressString().empty();
        }
        
        /**
         * @brief 주소를 문자열로 변환
         */
        std::string GetAddressString() const;
    };
    
    /**
     * @brief 통합 디바이스 정보 (🔥 기존 중복 구조체 통합)
     * Database::DeviceInfo + Drivers::DeviceInfo 통합
     */
    struct DeviceInfo {
        UUID id;                                     // 고유 식별자
        std::string name;                            // 디바이스 이름
        std::string description = "";                // 설명
        DeviceType device_type = DeviceType::GENERIC; // 디바이스 타입
        ProtocolType protocol = ProtocolType::UNKNOWN; // 통신 프로토콜
        Endpoint endpoint;                           // 연결 엔드포인트
        ConnectionStatus connection_status = ConnectionStatus::DISCONNECTED;
        
        // 통신 설정
        Duration timeout = std::chrono::milliseconds(5000);
        int retry_count = 3;
        Duration polling_interval = std::chrono::milliseconds(1000);
         
        // 그룹 및 분류
        std::string group_name = "Default";
        std::string location = "";
        std::string manufacturer = "";
        std::string model = "";
        std::string firmware_version = "";
        
        // 🆕 점검 관련 설정 (핵심 신규 기능)
        MaintenanceState maintenance_state;          // 현재 점검 상태
        bool maintenance_allowed = true;             // 점검 허용 여부
        std::vector<EngineerID> authorized_engineers; // 인증된 엔지니어 목록
        Duration max_maintenance_duration = std::chrono::hours(8); // 최대 점검 시간
        
        // 상태 정보
        Timestamp last_seen;                         // 마지막 통신 시간
        std::string last_error = "";                 // 마지막 에러 메시지
        ErrorCode last_error_code = ErrorCode::SUCCESS;
        
        // 통계 정보
        uint64_t total_reads = 0;
        uint64_t total_writes = 0;
        uint64_t total_errors = 0;
        
        // 🔧 mutex 제거 - 스레드 안전성은 상위 레벨에서 관리
        
        /**
         * @brief 디바이스가 연결되어 있는지 확인
         */
        bool IsConnected() const {
            return connection_status == ConnectionStatus::CONNECTED;
        }
        
        /**
         * @brief 점검 중인지 확인
         */
        bool IsUnderMaintenance() const {
            return maintenance_state.IsUnderMaintenance();
        }
        
        /**
         * @brief 원격 제어가 가능한지 확인
         */
        bool IsRemoteControlAllowed() const {
            return !maintenance_state.IsRemoteControlBlocked();
        }
        
        // 🔧 기존 코드 호환성을 위한 유틸리티 함수들
        
        /**
         * @brief timeout을 밀리초로 가져오기 (기존 코드 호환)
         */
        int GetTimeoutMs() const {
            return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
        }
        
        /**
         * @brief timeout_ms 값으로 timeout Duration 설정
         */
        void SetTimeoutMs(int ms) {
            timeout_ms = ms;
            timeout = std::chrono::milliseconds(ms);
        }
        
        /**
         * @brief timeout Duration 값으로 timeout_ms 설정  
         */
        void SyncTimeoutFields() {
            timeout_ms = GetTimeoutMs();
        }
    };
    
    /**
     * @brief 통합 데이터 포인트 (🔥 기존 중복 구조체 통합)
     * Database::DataPointInfo + Drivers::DataPoint 통합
     */
    struct DataPoint {
        UUID id;                                     // 고유 식별자
        UUID device_id;                              // 소속 디바이스 ID
        std::string name;                            // 데이터 포인트 이름
        std::string description = "";                // 설명
        UniversalAddress address;                    // 통합 주소
        AccessType access_type = AccessType::READ_ONLY; // 접근 타입
        
        // 데이터 타입 정보
        std::string data_type = "UNKNOWN";           // 데이터 타입
        std::optional<double> scale_factor;          // 스케일 팩터
        std::optional<double> offset;                // 오프셋
        std::string unit = "";                       // 단위
        
        // 범위 및 제한
        std::optional<DataVariant> min_value;        // 최솟값
        std::optional<DataVariant> max_value;        // 최댓값
        DataVariant default_value;                   // 기본값
        
        // 품질 관리 설정 (🆕 점검 기능)
        Duration stale_threshold = std::chrono::seconds(30);    // 오래된 데이터 임계값
        Duration very_stale_threshold = std::chrono::minutes(5); // 매우 오래된 데이터 임계값
        bool maintenance_quality_check = true;       // 점검 중 품질 체크 여부
        
        // 알람 설정
        std::optional<DataVariant> high_alarm;       // 상한 알람
        std::optional<DataVariant> low_alarm;        // 하한 알람
        bool alarm_enabled = false;
        
        // 상태 정보
        Timestamp last_read_time;                    // 마지막 읽기 시간
        Timestamp last_write_time;                   // 마지막 쓰기 시간
        uint64_t read_count = 0;
        uint64_t write_count = 0;
        uint64_t error_count = 0;
        
        /**
         * @brief 쓰기 가능한지 확인 (점검 상태 고려)
         */
        bool IsWritable(const DeviceInfo& device) const {
            if (access_type == AccessType::READ_ONLY) return false;
            if (access_type == AccessType::READ_WRITE_MAINTENANCE_ONLY) {
                return device.IsUnderMaintenance();
            }
            return !device.maintenance_state.IsRemoteControlBlocked();
        }
    };
    
    /**
     * @brief 타임스탬프가 있는 값 (🆕 점검 품질 정보 추가)
     */
    struct TimestampedValue {
        DataVariant value;                           // 실제 데이터 값
        DataQuality quality = DataQuality::GOOD;     // 🆕 확장된 품질 코드
        Timestamp timestamp;                         // 데이터 타임스탬프
        
        // 🆕 점검 관련 품질 정보
        Duration age = std::chrono::milliseconds(0); // 데이터 나이 (현재 시간 - 타임스탬프)
        bool under_maintenance = false;              // 점검 중 여부
        EngineerID engineer_id = "";                 // 점검 엔지니어 (점검 중인 경우)
        std::string quality_note = "";               // 품질 관련 메모
        
        // 스캔 지연 정보
        Duration scan_delay = std::chrono::milliseconds(0); // 스캔 지연 시간
        bool scan_delayed = false;                   // 스캔 지연 여부
        
        /**
         * @brief 데이터가 신뢰할 수 있는지 확인
         */
        bool IsReliable() const {
            return quality == DataQuality::GOOD && 
                   !under_maintenance &&
                   age < std::chrono::seconds(30);
        }
        
        /**
         * @brief 데이터가 오래된지 확인
         */
        bool IsStale(Duration stale_threshold = std::chrono::seconds(30)) const {
            return age > stale_threshold;
        }
        
        /**
         * @brief 품질 상태를 업데이트
         */
        void UpdateQuality(Duration stale_threshold, Duration very_stale_threshold) {
            if (under_maintenance) {
                quality = DataQuality::UNDER_MAINTENANCE;
            } else if (age > very_stale_threshold) {
                quality = DataQuality::VERY_STALE_DATA;
            } else if (age > stale_threshold) {
                quality = DataQuality::STALE_DATA;
            } else if (scan_delayed) {
                quality = DataQuality::SCAN_DELAYED;
            } else {
                quality = DataQuality::GOOD;
            }
        }
    };
    
    /**
     * @brief 드라이버 로그 컨텍스트 (DriverLogger.h에서 이동)
     */
    struct DriverLogContext {
        UUID device_id;                    // 디바이스 ID
        ProtocolType protocol = ProtocolType::UNKNOWN; // 프로토콜 타입
        std::string endpoint = "";         // 연결 엔드포인트
        std::string function_name = "";    // 함수명
        std::string file_name = "";        // 파일명
        int line_number = 0;               // 라인 번호
        std::thread::id thread_id;         // 스레드 ID
        std::map<std::string, std::string> extra_data; // 추가 데이터
        
        DriverLogContext() : thread_id(std::this_thread::get_id()) {}
        
        /**
         * @brief JSON 형태로 변환
         */
        std::string ToJson() const {
            std::ostringstream oss;
            oss << "{\"device_id\":\"" << device_id 
                << "\",\"protocol\":\"" << static_cast<int>(protocol)
                << "\",\"endpoint\":\"" << endpoint
                << "\",\"function\":\"" << function_name
                << "\",\"file\":\"" << file_name
                << "\",\"line\":" << line_number << "}";
            return oss.str();
        }
    };
    
    /**
     * @brief 비동기 읽기 요청 (IProtocolDriver.h에서 이동)
     */
    struct AsyncReadRequest {
        UUID request_id;                                  // 요청 고유 ID
        std::vector<DataPoint> points;                    // 읽을 데이터 포인트들
        Timestamp requested_at;                           // 요청 시간
        int priority = 0;                                 // 우선순위 (높을수록 우선)
        Duration timeout = std::chrono::seconds(5);       // 개별 타임아웃
        EngineerID requester_id = "";                     // 요청자 (점검 추적용)
        bool maintenance_override = false;                // 점검 모드 무시 (관리자용)
        
        // 기본 생성자 - 함수 호출 없이 간단하게
        AsyncReadRequest() {
            request_id = "REQ_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
            requested_at = std::chrono::system_clock::now();
        }
        
        AsyncReadRequest(const std::vector<DataPoint>& data_points, int req_priority = 0)
            : points(data_points), priority(req_priority) {
            request_id = "REQ_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
            requested_at = std::chrono::system_clock::now();
        }
    };
    
    /**
     * @brief 비동기 쓰기 요청
     */
    struct AsyncWriteRequest {
        UUID request_id;                                  // 요청 고유 ID
        std::vector<std::pair<DataPoint, DataVariant>> write_data; // 쓸 데이터들
        Timestamp requested_at;                           // 요청 시간
        int priority = 0;                                 // 우선순위
        Duration timeout = std::chrono::seconds(5);       // 타임아웃
        EngineerID requester_id = "";                     // 요청자 ID
        bool maintenance_override = false;                // 점검 무시 여부
        
        AsyncWriteRequest() {
            request_id = "WRT_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
            requested_at = std::chrono::system_clock::now();
        }
    };
    
    /**
     * @brief 로그 통계 정보 (DriverLogger.h에서 이동)
     */
    struct LogStatistics {
        uint64_t debug_count = 0;
        uint64_t info_count = 0;
        uint64_t warn_count = 0;
        uint64_t error_count = 0;
        uint64_t fatal_count = 0;
        uint64_t maintenance_count = 0;       // 🆕 점검 로그 카운트
        
        /**
         * @brief 전체 로그 카운트 계산
         */
        uint64_t GetTotalCount() const {
            return debug_count + info_count + warn_count + error_count + fatal_count + maintenance_count;
        }
        
        /**
         * @brief 통계 초기화
         */
        void Reset() {
            debug_count = info_count = warn_count = error_count = fatal_count = maintenance_count = 0;
        }
    };
    
    /**
     * @brief 읽기/쓰기 응답 정보
     */
    struct DataResponse {
        UUID request_id;                             // 요청 ID (매칭용)
        std::vector<TimestampedValue> values;        // 응답 데이터
        ErrorCode result = ErrorCode::SUCCESS;       // 전체 결과
        std::string error_message = "";              // 에러 메시지
        Timestamp response_time;                     // 응답 시간
        Duration processing_time;                    // 처리 시간
    };
    
    // =============================================================================
    // 🆕 드라이버 시스템을 위한 추가 구조체들
    // =============================================================================
    
    /**
     * @brief 에러 정보 구조체 (드라이버용)
     */
    struct ErrorInfo {
        ErrorCode code = ErrorCode::SUCCESS;
        std::string message = "";
        std::string details = "";
        Timestamp occurred_at;
        
        ErrorInfo() : occurred_at(GetCurrentTimestamp()) {}
        ErrorInfo(ErrorCode err_code, const std::string& msg) 
            : code(err_code), message(msg), occurred_at(GetCurrentTimestamp()) {}
    };
    
    /**
     * @brief 데이터 값 타입 (모든 드라이버에서 사용)
     */
    using DataValue = DataVariant;
    
    /**
     * @brief 드라이버 상태 열거형
     */
    enum class DriverStatus : uint8_t {
        UNINITIALIZED = 0,
        INITIALIZED = 1,
        STARTING = 2,
        RUNNING = 3,
        STOPPING = 4,
        STOPPED = 5,
        ERROR = 6,
        MAINTENANCE = 7
    };
    
    /**
     * @brief 드라이버 설정 구조체
     */
    struct DriverConfig {
        UUID device_id;
        ProtocolType protocol = ProtocolType::UNKNOWN;
        std::string endpoint = "";
        Duration timeout = std::chrono::seconds(5);
        int retry_count = 3;
        Duration polling_interval = std::chrono::seconds(1);
        std::map<std::string, std::string> custom_settings;
    };
    
    /**
     * @brief 드라이버 통계 구조체
     */
    struct DriverStatistics {
        uint64_t total_reads = 0;
        uint64_t total_writes = 0;
        uint64_t successful_reads = 0;
        uint64_t successful_writes = 0;
        uint64_t failed_reads = 0;
        uint64_t failed_writes = 0;
        Timestamp last_read_time;
        Timestamp last_write_time;
        Timestamp last_error_time;
        Duration average_response_time = std::chrono::milliseconds(0);
        
        double GetSuccessRate() const {
            uint64_t total = total_reads + total_writes;
            if (total == 0) return 100.0;
            uint64_t successful = successful_reads + successful_writes;
            return (static_cast<double>(successful) / total) * 100.0;
        }
    };
    
} // namespace PulseOne::Structs

#endif // PULSEONE_COMMON_STRUCTS_H