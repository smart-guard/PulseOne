// collector/include/Drivers/Common/IProtocolDriver.h
// 메서드명 수정본 - DriverStatistics 실제 메서드에 맞춤

#ifndef PULSEONE_IPROTOCOL_DRIVER_H
#define PULSEONE_IPROTOCOL_DRIVER_H

// =============================================================================
// ✅ 필수 헤더들 모두 포함 (순서 중요!)
// =============================================================================
#include "Common/BasicTypes.h"       // UniqueId, Timestamp 등
#include "Common/DriverStatistics.h" // DriverStatistics
#include "Common/Enums.h"            // ProtocolType, ConnectionStatus 등
#include "Common/Structs.h"          // DeviceInfo, DataPoint 등

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace PulseOne {
namespace Drivers {

// =============================================================================
// ✅ 타입 별칭들 (기존 코드 호환성)
// =============================================================================
using UniqueId = PulseOne::BasicTypes::UniqueId;
using ProtocolType = PulseOne::Enums::ProtocolType;
using ConnectionStatus = PulseOne::Enums::ConnectionStatus;
using DriverStatistics = PulseOne::Structs::DriverStatistics;
using DriverConfig = PulseOne::Structs::DriverConfig;
using DeviceInfo = PulseOne::Structs::DeviceInfo;
using DataPoint = PulseOne::Structs::DataPoint;
using TimestampedValue = PulseOne::Structs::TimestampedValue;
using DataValue = PulseOne::Structs::DataValue;
using ErrorInfo = PulseOne::Structs::ErrorInfo;
using DriverStatus = PulseOne::Structs::DriverStatus;

// =============================================================================
// ✅ 콜백 함수 타입들 (간소화)
// =============================================================================
using StatusCallback =
    std::function<void(const UniqueId &device_id, ConnectionStatus old_status,
                       ConnectionStatus new_status)>;

using ErrorCallback =
    std::function<void(const UniqueId &device_id, const ErrorInfo &error)>;

using MessageCallback =
    std::function<void(const std::string &topic, const std::string &payload)>;

// =============================================================================
// ✅ IProtocolDriver 인터페이스 (메서드명 수정)
// =============================================================================
class IProtocolDriver {
protected:
  // Driver 통계 (통신 성능만)
  DriverStatistics statistics_;
  DriverConfig config_;
  // 간소화된 콜백들
  StatusCallback status_callback_;
  ErrorCallback error_callback_;
  MessageCallback message_callback_;
  mutable std::mutex callback_mutex_;

  // 내부 상태
  std::atomic<ConnectionStatus> connection_status_{
      ConnectionStatus::DISCONNECTED};
  std::atomic<bool> is_running_{false};
  ErrorInfo last_error_;

public:
  // =============================================================================
  // ✅ 생성자 및 소멸자
  // =============================================================================
  IProtocolDriver() : statistics_("UNKNOWN") {}
  virtual ~IProtocolDriver() = default;

  // =============================================================================
  // ✅ 순수 가상 함수들 (파생 클래스에서 구현 필수)
  // =============================================================================

  virtual bool Initialize(const DriverConfig &config) = 0;
  virtual bool Connect() = 0;
  virtual bool Disconnect() = 0;
  virtual bool IsConnected() const = 0;
  virtual bool Start() = 0;
  virtual bool Stop() = 0;

  virtual bool ReadValues(const std::vector<DataPoint> &points,
                          std::vector<TimestampedValue> &values) = 0;

  virtual bool WriteValue(const DataPoint &point, const DataValue &value) = 0;

  // ✅ Pub/Sub 프로토콜 지원을 위한 구독 인터페이스 (기본 구현은 지원 안 함)
  virtual bool Subscribe(const std::string &topic, int qos = 0) {
    (void)topic;
    (void)qos;
    return false;
  }

  virtual bool Unsubscribe(const std::string &topic) {
    (void)topic;
    return false;
  }

  /**
   * @brief Discover available data points from the device (Node Browsing)
   * @return List of discovered DataPoints
   */
  virtual std::vector<DataPoint> DiscoverPoints() { return {}; }

  virtual ProtocolType GetProtocolType() const = 0;
  virtual DriverStatus GetStatus() const = 0;
  virtual ErrorInfo GetLastError() const = 0; // ✅ const 참조 제거

  // =============================================================================
  // ✅ 공통 구현 메서드들 (실제 메서드명 사용)
  // =============================================================================

  virtual const DriverStatistics &GetStatistics() const { return statistics_; }

  virtual void ResetStatistics() {
    statistics_.ResetStatistics(); // ✅ 실제 메서드명 사용
  }

  virtual ConnectionStatus GetConnectionStatus() const {
    return connection_status_.load();
  }

  /**
   * @brief 현재 설정 정보 반환 (테스트용)
   */
  virtual const DriverConfig &GetConfiguration() const {
    return config_; // 파생 클래스에서 config_ 멤버 변수 필요
  }

  virtual void SetStatusCallback(const StatusCallback &callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    status_callback_ = callback;
  }

  virtual void SetErrorCallback(const ErrorCallback &callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    error_callback_ = callback;
  }

  virtual void SetMessageCallback(const MessageCallback &callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    message_callback_ = callback;
  }

protected:
  // =============================================================================
  // ✅ 보호된 헬퍼 메서드들 (실제 필드명 사용)
  // =============================================================================

  virtual void NotifyStatusChange(ConnectionStatus new_status) {
    auto old_status = connection_status_.exchange(new_status);
    if (old_status != new_status && status_callback_) {
      UniqueId device_id{}; // 빈 UniqueId
      status_callback_(device_id, old_status, new_status);
    }
  }

  virtual void NotifyError(const ErrorInfo &error) {
    last_error_ = error;
    statistics_.failed_operations.fetch_add(1); // ✅ 실제 필드명 사용

    if (error_callback_) {
      UniqueId device_id{}; // 빈 UniqueId
      error_callback_(device_id, error);
    }
  }

  virtual void UpdateReadStats(bool success, double duration_ms = 0.0) {
    (void)success;     // unused parameter 경고 방지
    (void)duration_ms; // unused parameter 경고 방지
    statistics_.total_reads.fetch_add(1);
    if (success) {
      statistics_.successful_reads.fetch_add(1);
    } else {
      statistics_.failed_reads.fetch_add(1);
    }
  }

  virtual void UpdateWriteStats(bool success, double duration_ms = 0.0) {
    (void)success;     // unused parameter 경고 방지
    (void)duration_ms; // unused parameter 경고 방지
    statistics_.total_writes.fetch_add(1);
    if (success) {
      statistics_.successful_writes.fetch_add(1);
    } else {
      statistics_.failed_writes.fetch_add(1);
    }
  }
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_IPROTOCOL_DRIVER_H