// ==========================================================================
// 📁 파일: collector/include/Workers/Base/BaseDeviceWorker.h  
// 🔥 컴파일 에러 수정: Timestamp, WorkerState::UNKNOWN, 멤버 변수 등
// ==========================================================================

#ifndef WORKERS_BASE_DEVICE_WORKER_H
#define WORKERS_BASE_DEVICE_WORKER_H

#include "Common/Structs.h"
#include "Common/Enums.h"
#include "Common/BasicTypes.h"        // 🔥 Timestamp 타입을 위해 추가
#include "Utils/LogManager.h"
#include "Pipeline/PipelineManager.h"
#include <memory>
#include <future>
#include <atomic>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <mutex>

namespace PulseOne {
namespace Workers {

// 🔥 WorkerState enum 수정 (UNKNOWN 추가)
enum class WorkerState {
    UNKNOWN = -1,               ///< 알 수 없는 상태 (🔥 추가!)
    STOPPED = 0,                ///< 정지됨
    STARTING = 1,               ///< 시작 중
    RUNNING = 2,                ///< 정상 실행 중
    PAUSED = 3,                 ///< 일시정지됨 (소프트웨어)
    STOPPING = 4,               ///< 정지 중
    ERROR = 5,                  ///< 오류 상태
    
    // 현장 운영 상태들
    MAINTENANCE = 10,           ///< 점검 모드 (하드웨어 점검 중)
    SIMULATION = 11,            ///< 시뮬레이션 모드 (테스트 데이터)
    CALIBRATION = 12,           ///< 교정 모드 (센서 교정 중)
    COMMISSIONING = 13,         ///< 시운전 모드 (초기 설정 중)
    
    // 장애 상황들
    DEVICE_OFFLINE = 20,        ///< 디바이스 오프라인
    COMMUNICATION_ERROR = 21,   ///< 통신 오류
    DATA_INVALID = 22,          ///< 데이터 이상 (범위 벗어남)
    SENSOR_FAULT = 23,          ///< 센서 고장
    
    // 특수 운영 모드들
    MANUAL_OVERRIDE = 30,       ///< 수동 제어 모드
    EMERGENCY_STOP = 31,        ///< 비상 정지
    BYPASS_MODE = 32,           ///< 우회 모드 (백업 시스템 사용)
    DIAGNOSTIC_MODE = 33,       ///< 진단 모드 (상세 로깅)
    
    // 재연결 관련 상태들
    RECONNECTING = 40,          ///< 재연결 시도 중
    WAITING_RETRY = 41,         ///< 재시도 대기 중
    MAX_RETRIES_EXCEEDED = 42   ///< 최대 재시도 횟수 초과
};

// 🔥 Timestamp 별칭 정의 (BasicTypes에서 가져오기)
using Timestamp = PulseOne::BasicTypes::Timestamp;

/**
 * @brief 재연결 설정 구조체 (데이터베이스에서 로드)
 */
struct ReconnectionSettings {
    bool auto_reconnect_enabled = true;         ///< 자동 재연결 활성화
    int retry_interval_ms = 5000;               ///< 재시도 간격 (밀리초)
    int max_retries_per_cycle = 10;             ///< 사이클당 최대 재시도 횟수
    int wait_time_after_max_retries_ms = 60000; ///< 최대 재시도 후 대기 시간 (밀리초)
    bool keep_alive_enabled = true;             ///< Keep-alive 활성화
    int keep_alive_interval_seconds = 30;       ///< Keep-alive 간격 (초)
    int connection_timeout_seconds = 10;        ///< 연결 타임아웃 (초)
    
    std::string ToJson() const;
    bool FromJson(const std::string& json_str);
};

/**
 * @brief 재연결 통계 정보
 */
struct ReconnectionStats {
    std::atomic<uint64_t> total_connections{0};
    std::atomic<uint64_t> successful_connections{0};
    std::atomic<uint64_t> failed_connections{0};
    std::atomic<uint64_t> reconnection_cycles{0};
    std::atomic<uint64_t> wait_cycles{0};
    std::atomic<uint64_t> keep_alive_sent{0};
    std::atomic<uint64_t> keep_alive_failed{0};
    std::atomic<double> avg_connection_duration_seconds{0.0};
    
    std::chrono::system_clock::time_point first_connection_time;
    std::chrono::system_clock::time_point last_successful_connection;
    std::chrono::system_clock::time_point last_failure_time;
};

/**
 * @brief 모든 디바이스 워커의 기반 클래스
 */
class BaseDeviceWorker {
public:
    explicit BaseDeviceWorker(const PulseOne::Structs::DeviceInfo& device_info); // 🔥 Structs:: 추가
    virtual ~BaseDeviceWorker();
    
    // 복사/이동 방지
    BaseDeviceWorker(const BaseDeviceWorker&) = delete;
    BaseDeviceWorker& operator=(const BaseDeviceWorker&) = delete;

    // =============================================================================
    // 순수 가상 함수들 (파생 클래스에서 구현 필수)
    // =============================================================================
    virtual std::future<bool> Start() = 0;
    virtual std::future<bool> Stop() = 0;
    virtual bool EstablishConnection() = 0;
    virtual bool CloseConnection() = 0;
    virtual bool CheckConnection() = 0;
    virtual bool SendKeepAlive() { return true; }
    
    // =============================================================================
    // 공통 인터페이스
    // =============================================================================
    virtual WorkerState GetState() const { return current_state_.load(); }
    virtual std::future<bool> Pause();
    virtual std::future<bool> Resume();
    virtual bool AddDataPoint(const PulseOne::Structs::DataPoint& point); // 🔥 Structs:: 추가
    virtual std::vector<PulseOne::Structs::DataPoint> GetDataPoints() const; // 🔥 Structs:: 추가
    
    // =============================================================================
    // 재연결 관리
    // =============================================================================
    bool UpdateReconnectionSettings(const ReconnectionSettings& settings);
    ReconnectionSettings GetReconnectionSettings() const;
    std::future<bool> ForceReconnect();
    std::string GetReconnectionStats() const;
    void ResetReconnectionState();
    WorkerState GetCurrentState() const { return current_state_.load(); }
    bool IsConnected() const { return is_connected_.load(); }
    
    // =============================================================================
    // 상태 관리 및 모니터링
    // =============================================================================
    virtual std::string GetStatusJson() const;
    
    // =============================================================================
    // 파이프라인 연결
    // =============================================================================
    bool SendDataToPipeline(const std::vector<PulseOne::Structs::TimestampedValue>& values, // 🔥 Structs:: 추가
                           uint32_t priority = 0);
    
    const std::string& GetWorkerId() const { return worker_id_; }

protected:
    // =============================================================================
    // 파생 클래스에서 사용할 수 있는 보호된 메서드들
    // =============================================================================
    void ChangeState(WorkerState new_state);
    void SetConnectionState(bool connected);
    void HandleConnectionError(const std::string& error_message);
    void LogMessage(LogLevel level, const std::string& message) const;
    
    // =============================================================================
    // WorkerState 유틸리티 함수들
    // =============================================================================
    std::string WorkerStateToString(WorkerState state) const;
    bool IsActiveState(WorkerState state);
    bool IsErrorState(WorkerState state);
    
    // DeviceInfo 접근자들
    std::string GetProtocolType() const { 
        return device_info_.protocol_type;
    }
    
    void SetProtocolType(const std::string& protocol_type) { 
        device_info_.protocol_type = protocol_type;
    }
    
    std::string GetProperty(const std::string& key, const std::string& default_value = "") const {
        auto it = device_info_.properties.find(key);
        return (it != device_info_.properties.end()) ? it->second : default_value;
    }
    
    void SetProperty(const std::string& key, const std::string& value) {
        device_info_.properties[key] = value;
    }
    
    const std::string& GetDeviceName() const { return device_info_.name; }
    const std::string& GetEndpoint() const { return device_info_.endpoint; }
    bool IsEnabled() const { return device_info_.is_enabled; }
    uint32_t GetPollingInterval() const { return device_info_.polling_interval_ms; }
    uint32_t GetTimeout() const { return device_info_.timeout_ms; }
    
    const PulseOne::Structs::DeviceInfo& GetDeviceInfo() const { return device_info_; }
    PulseOne::Structs::DeviceInfo& GetDeviceInfo() { return device_info_; }
    
    std::vector<PulseOne::Structs::DataPoint>& GetDataPoints() { return data_points_; }

    // =============================================================================
    // 통신 결과 업데이트 메서드들 (CPP에서 구현)
    // =============================================================================
    void UpdateCommunicationResult(bool success, 
                                 const std::string& error_msg = "",
                                 int error_code = 0,
                                 std::chrono::milliseconds response_time = std::chrono::milliseconds{0});
    
    void OnStateChanged(WorkerState old_state, WorkerState new_state);

    // =============================================================================
    // 상태 변환 메서드들 (CPP에서 구현)
    // =============================================================================
    PulseOne::Enums::DeviceStatus ConvertWorkerStateToDeviceStatus(WorkerState state) const;
    std::string GetStatusMessage() const;
    std::string GenerateCorrelationId() const;
    std::string GetWorkerIdString() const;

    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    PulseOne::Structs::DeviceInfo device_info_;              ///< 디바이스 정보
    std::string worker_id_;

private:
    // =============================================================================
    // 내부 데이터 멤버
    // =============================================================================
    std::atomic<WorkerState> current_state_{WorkerState::STOPPED};
    std::atomic<bool> is_connected_{false};
    
    // 🔥 누락된 멤버 변수들 추가!
    uint32_t batch_sequence_counter_ = 0;
    uint32_t consecutive_failures_ = 0;
    uint32_t total_failures_ = 0;
    uint32_t total_attempts_ = 0;
    std::chrono::milliseconds last_response_time_{0};
    Timestamp last_success_time_;                    // 🔥 이제 정의됨!
    Timestamp state_change_time_;                    // 🔥 이제 정의됨!
    std::string last_error_message_ = "";
    int last_error_code_ = 0;
    WorkerState previous_state_ = WorkerState::UNKNOWN; // 🔥 이제 UNKNOWN 사용 가능!
    
    // =============================================================================
    // 재연결 관리
    // =============================================================================
    mutable std::mutex settings_mutex_;
    ReconnectionSettings reconnection_settings_;
    ReconnectionStats reconnection_stats_;
    
    std::atomic<int> current_retry_count_{0};
    std::atomic<bool> in_wait_cycle_{false};
    std::chrono::system_clock::time_point wait_start_time_;
    std::chrono::system_clock::time_point last_keep_alive_time_;
    
    // =============================================================================
    // 백그라운드 스레드 관리
    // =============================================================================
    std::unique_ptr<std::thread> reconnection_thread_;
    std::atomic<bool> thread_running_{false};
    
    std::string status_channel_;
    std::string reconnection_channel_;
    
    // =============================================================================
    // 데이터 포인트 관리
    // =============================================================================
    mutable std::mutex data_points_mutex_;
    std::vector<PulseOne::Structs::DataPoint> data_points_;   // 🔥 Structs:: 추가
    
    // =============================================================================
    // 내부 메서드들
    // =============================================================================
    void ReconnectionThreadMain();
    bool AttemptReconnection();
    bool HandleWaitCycle();
    void HandleKeepAlive();
    void InitializeRedisChannels();
    void UpdateReconnectionStats(bool connection_successful);
};

} // namespace Workers
} // namespace PulseOne

#endif // WORKERS_BASE_DEVICE_WORKER_H