// ==========================================================================
// 📁 파일: collector/include/Workers/Base/BaseDeviceWorker.h
// 🔥 GitHub 구조 준수 + Write 가상함수 추가 + 메모리 누수 수정 + 테스트 지원
// ==========================================================================

#ifndef WORKERS_BASE_DEVICE_WORKER_H
#define WORKERS_BASE_DEVICE_WORKER_H

#include "Common/BasicTypes.h"
#include "Common/Enums.h"
#include "Common/Structs.h"
#include "Logging/LogManager.h"
#include "Pipeline/PipelineManager.h"
#include "Platform/PlatformCompat.h"
#include <atomic>
#include <chrono>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace PulseOne {
namespace Workers {

// =============================================================================
// WorkerState enum (GitHub 구조 기존 패턴 유지)
// =============================================================================
enum class WorkerState {
  UNKNOWN = -1,     ///< 알 수 없는 상태
  STOPPED = 0,      ///< 정지됨
  STARTING = 1,     ///< 시작 중
  RUNNING = 2,      ///< 정상 실행 중
  PAUSED = 3,       ///< 일시정지됨 (소프트웨어)
  STOPPING = 4,     ///< 정지 중
  WORKER_ERROR = 5, ///< 오류 상태

  // 현장 운영 상태들
  MAINTENANCE = 10,   ///< 점검 모드
  SIMULATION = 11,    ///< 시뮬레이션 모드
  CALIBRATION = 12,   ///< 교정 모드
  COMMISSIONING = 13, ///< 시운전 모드

  // 장애 상황들
  DEVICE_OFFLINE = 20,      ///< 디바이스 오프라인
  COMMUNICATION_ERROR = 21, ///< 통신 오류
  DATA_INVALID = 22,        ///< 데이터 이상
  SENSOR_FAULT = 23,        ///< 센서 고장

  // 특수 운영 모드들
  MANUAL_OVERRIDE = 30, ///< 수동 제어 모드
  EMERGENCY_STOP = 31,  ///< 비상 정지
  BYPASS_MODE = 32,     ///< 우회 모드
  DIAGNOSTIC_MODE = 33, ///< 진단 모드

  // 재연결 관련 상태들
  RECONNECTING = 40,        ///< 재연결 시도 중
  WAITING_RETRY = 41,       ///< 재시도 대기 중
  MAX_RETRIES_EXCEEDED = 42 ///< 최대 재시도 횟수 초과
};

// =============================================================================
// 네임스페이스 별칭들 (충돌 방지)
// =============================================================================
using Timestamp = PulseOne::BasicTypes::Timestamp;
using DataValue = PulseOne::Structs::DataValue;
using LogLevel = PulseOne::Enums::LogLevel;

/**
 * @brief 재연결 설정 구조체
 */
struct ReconnectionSettings {
  bool auto_reconnect_enabled = true;         ///< 자동 재연결 활성화
  int retry_interval_ms = 5000;               ///< 재시도 간격 (밀리초)
  int max_retries_per_cycle = 10;             ///< 사이클당 최대 재시도 횟수
  int wait_time_after_max_retries_ms = 60000; ///< 최대 재시도 후 대기 시간
  bool keep_alive_enabled = true;             ///< Keep-alive 활성화
  int keep_alive_interval_seconds = 30;       ///< Keep-alive 간격 (초)
  int connection_timeout_seconds = 10;        ///< 연결 타임아웃 (초)

  std::string ToJson() const;
  bool FromJson(const std::string &json_str);
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
 *
 * 설계 원칙:
 * - READ/WRITE 통합 인터페이스 제공
 * - 프로토콜별 구현체에서 구체적 메서드 오버라이드
 * - WorkerManager를 통한 중앙집중식 관리 지원
 * - 메모리 누수 방지를 위한 스레드 생명주기 관리
 */
class BaseDeviceWorker {
public:
  explicit BaseDeviceWorker(const PulseOne::Structs::DeviceInfo &device_info);
  virtual ~BaseDeviceWorker();

  // 복사/이동 방지
  BaseDeviceWorker(const BaseDeviceWorker &) = delete;
  BaseDeviceWorker &operator=(const BaseDeviceWorker &) = delete;

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
  // 메모리 누수 방지를 위한 스레드 생명주기 관리
  // =============================================================================

  /**
   * @brief 재연결 스레드 시작 (Start() 호출시에만 사용)
   * @note 생성자에서 자동 시작하지 않음 (메모리 누수 방지)
   */
  void StartReconnectionThread();

  /**
   * @brief 모든 스레드 중지 및 리소스 정리
   * @note 소멸자에서 자동 호출됨
   */
  void StopAllThreads();

  // =============================================================================
  // 공통 READ 인터페이스
  // =============================================================================
  virtual WorkerState GetState() const { return current_state_.load(); }
  virtual std::future<bool> Pause();
  virtual std::future<bool> Resume();
  virtual bool AddDataPoint(const PulseOne::Structs::DataPoint &point);
  virtual void ReloadSettings(const PulseOne::Structs::DeviceInfo &new_info);
  virtual void
  ReloadDataPoints(const std::vector<PulseOne::Structs::DataPoint> &new_points);
  virtual std::vector<PulseOne::Structs::DataPoint> GetDataPoints() const;

  // =============================================================================
  // 공통 WRITE 인터페이스 (가상 함수로 추가)
  // 각 프로토콜별 구현체에서 오버라이드
  // =============================================================================

  /**
   * @brief DataPoint ID를 통한 값 쓰기 (범용)
   * @param point_id DataPoint ID
   * @param value 쓸 값
   * @return 성공 시 true
   */
  virtual bool WriteDataPoint(const std::string &point_id,
                              const DataValue &value) {
    (void)point_id; // unused parameter 경고 억제
    (void)value;    // unused parameter 경고 억제
    LogMessage(LogLevel::WARN, "WriteDataPoint not implemented for protocol: " +
                                   GetProtocolType());
    return false;
  }

  /**
   * @brief 아날로그 출력 제어
   * @param output_id 출력 ID (DataPoint ID 또는 주소)
   * @param value 아날로그 값
   * @return 성공 시 true
   */
  virtual bool WriteAnalogOutput(const std::string &output_id, double value) {
    return WriteDataPoint(output_id, DataValue(value));
  }

  /**
   * @brief 디지털 출력 제어
   * @param output_id 출력 ID (DataPoint ID 또는 주소)
   * @param value 디지털 값 (true/false)
   * @return 성공 시 true
   */
  virtual bool WriteDigitalOutput(const std::string &output_id, bool value) {
    return WriteDataPoint(output_id, DataValue(value));
  }

  /**
   * @brief 설정값(Setpoint) 변경
   * @param setpoint_id 설정값 ID
   * @param value 새로운 설정값
   * @return 성공 시 true
   */
  virtual bool WriteSetpoint(const std::string &setpoint_id, double value) {
    return WriteAnalogOutput(setpoint_id, value);
  }

  /**
   * @brief 펌프 제어
   * @param pump_id 펌프 ID
   * @param enable true=시작, false=정지
   * @return 성공 시 true
   */
  virtual bool ControlDigitalDevice(const std::string &pump_id, bool enable) {
    return WriteDigitalOutput(pump_id, enable);
  }

  /**
   * @brief 밸브 제어
   * @param valve_id 밸브 ID
   * @param position 밸브 위치 (0.0~100.0 %)
   * @return 성공 시 true
   */
  virtual bool ControlAnalogDevice(const std::string &valve_id,
                                   double position) {
    return WriteAnalogOutput(valve_id, position);
  }

  // =============================================================================
  // 재연결 관리
  // =============================================================================
  bool UpdateReconnectionSettings(const ReconnectionSettings &settings);
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
  bool SendDataToPipeline(
      const std::vector<PulseOne::Structs::TimestampedValue> &values,
      uint32_t priority = 0);

  /**
   * @brief 새로운 데이터포인트를 DB에 자동으로 생성/등록
   * @param name 데이터포인트 이름
   * @param address_string 주소 문자열 (토픽 등)
   * @param mapping_key JSON 매핑 키 (옵션)
   * @return 생성된 데이터포인트의 uint32_t ID (실패 시 0)
   */
  uint32_t RegisterNewDataPoint(const std::string &name,
                                const std::string &address_string,
                                const std::string &mapping_key = "",
                                const std::string &data_type = "FLOAT32");

  /**
   * @brief Discover available data points from the device
   */
  virtual std::vector<PulseOne::Structs::DataPoint> DiscoverDataPoints();

  const std::string &GetWorkerId() const { return worker_id_; }

  // =============================================================================
  // 테스트 및 디버깅 지원 메서드들 (public)
  // =============================================================================

  /**
   * @brief 속성 존재 여부 확인
   */
  bool HasProperty(const std::string &key) const {
    return device_info_.properties.find(key) != device_info_.properties.end();
  }

  /**
   * @brief 속성 값 가져오기
   */
  std::string GetProperty(const std::string &key,
                          const std::string &default_value = "") const {
    auto it = device_info_.properties.find(key);
    return (it != device_info_.properties.end()) ? it->second : default_value;
  }

  /**
   * @brief 속성 값 설정
   */
  void SetProperty(const std::string &key, const std::string &value) {
    device_info_.properties[key] = value;
  }

  /**
   * @brief 모든 속성 반환
   */
  std::map<std::string, std::string> GetAllProperties() const {
    return device_info_.properties;
  }

  /**
   * @brief 속성 개수 반환
   */
  size_t GetPropertyCount() const { return device_info_.properties.size(); }

  /**
   * @brief 디버깅용 속성 문자열 생성
   */
  std::string GetPropertiesDebugString() const {
    std::stringstream ss;
    ss << "Properties count: " << device_info_.properties.size() << "\n";
    for (const auto &[key, value] : device_info_.properties) {
      ss << "  " << key << " = " << value << "\n";
    }
    return ss.str();
  }

protected:
  // =============================================================================
  // 파생 클래스에서 사용할 수 있는 보호된 메서드들
  // =============================================================================
  void ChangeState(WorkerState new_state);
  void SetConnectionState(bool connected);
  void HandleConnectionError(const std::string &error_message);
  void LogMessage(LogLevel level, const std::string &message) const;

  // =============================================================================
  // WorkerState 유틸리티 함수들
  // =============================================================================
  std::string WorkerStateToString(WorkerState state) const;
  bool IsActiveState(WorkerState state);
  bool IsErrorState(WorkerState state);

  // =============================================================================
  // 파이프라인 전송을 위한 헬퍼 함수들 (protected)
  // =============================================================================
  uint32_t GetNextSequenceNumber();
  double GetRawDoubleValue(const DataValue &value) const;

  /**
   * @brief 파이프라인으로 데이터 전송 (로깅 포함)
   */
  bool SendValuesToPipelineWithLogging(
      const std::vector<PulseOne::Structs::TimestampedValue> &values,
      const std::string &data_type, uint32_t priority = 0);

  // =============================================================================
  // 파생 클래스에서 접근 가능한 데이터 (protected)
  // =============================================================================
  std::unordered_map<int, DataValue> previous_values_;
  std::vector<PulseOne::Structs::DataPoint> data_points_;

  // =============================================================================
  // DeviceInfo 접근자들
  // =============================================================================
  std::string GetProtocolType() const { return device_info_.protocol_type; }

  void SetProtocolType(const std::string &protocol_type) {
    device_info_.protocol_type = protocol_type;
  }

  const std::string &GetDeviceName() const { return device_info_.name; }
  const std::string &GetEndpoint() const { return device_info_.endpoint; }
  bool IsEnabled() const { return device_info_.is_enabled; }
  uint32_t GetPollingInterval() const {
    return device_info_.polling_interval_ms;
  }
  uint32_t GetTimeout() const { return device_info_.timeout_ms; }

  const PulseOne::Structs::DeviceInfo &GetDeviceInfo() const {
    return device_info_;
  }
  PulseOne::Structs::DeviceInfo &GetDeviceInfo() { return device_info_; }

  // =============================================================================
  // 멤버 변수들
  // =============================================================================
  PulseOne::Structs::DeviceInfo device_info_; ///< 디바이스 정보
  std::string worker_id_;

  // =============================================================================
  // 통신 결과 업데이트 메서드들
  // =============================================================================
  void UpdateCommunicationResult(
      bool success, const std::string &error_msg = "", int error_code = 0,
      std::chrono::milliseconds response_time = std::chrono::milliseconds{0});

  void OnStateChanged(WorkerState old_state, WorkerState new_state);

  // =============================================================================
  // 상태 변환 메서드들
  // =============================================================================
  PulseOne::Enums::DeviceStatus
  ConvertWorkerStateToDeviceStatus(WorkerState state) const;
  std::string GetStatusMessage() const;
  std::string GenerateCorrelationId() const;
  std::string GetWorkerIdString() const;

private:
  // =============================================================================
  // 내부 데이터 멤버
  // =============================================================================
protected:
  std::atomic<WorkerState> current_state_{WorkerState::STOPPED};
  std::atomic<bool> is_connected_{false};

  // 통신 상태 관련 멤버들 (하위 클래스 접근 허용)
  uint32_t batch_sequence_counter_ = 0;
  uint32_t consecutive_failures_ = 0;
  uint32_t total_failures_ = 0;
  uint32_t total_attempts_ = 0;
  std::chrono::milliseconds last_response_time_{0};
  Timestamp last_success_time_;
  Timestamp state_change_time_;
  std::string last_error_message_ = "";
  int last_error_code_ = 0;
  WorkerState previous_state_ = WorkerState::UNKNOWN;

private:
  // 시퀀스 카운터 (private)
  std::atomic<uint32_t> sequence_counter_{0};

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
  // 메모리 누수 방지를 위한 백그라운드 스레드 관리
  // =============================================================================
  std::unique_ptr<std::thread> reconnection_thread_;
  std::atomic<bool> thread_running_{false};

  std::string status_channel_;
  std::string reconnection_channel_;

protected:
  // =============================================================================
  // 데이터 포인트 관리
  // =============================================================================
  mutable std::recursive_mutex data_points_mutex_;

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