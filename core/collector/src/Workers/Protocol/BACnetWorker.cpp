// =============================================================================
// collector/src/Workers/Protocol/BACnetWorker.cpp - 독립 BACnetDriver 사용 완전
// 수정
// =============================================================================

#include "Workers/Protocol/BACnetWorker.h"
#include "Common/Enums.h"
#include "Logging/LogManager.h"

#if HAS_BACNET
#include "Drivers/Bacnet/BACnetServiceManager.h"
#include "Drivers/Common/IProtocolDriver.h"
#endif

#include "Database/Entities/DeviceScheduleEntity.h"
#include "Database/Repositories/DeviceScheduleRepository.h"
#include "Database/RepositoryFactory.h"
#include <sstream>
#include <thread>

using namespace std::chrono;
using LogLevel = PulseOne::Enums::LogLevel;

namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

BACnetWorker::BACnetWorker(const DeviceInfo &device_info)
    : UdpBasedWorker(device_info), data_scan_thread_running_(false),
      cleanup_timer_(0) {

  LogMessage(LogLevel::INFO,
             "BACnetWorker created for device: " + device_info.name);

#if HAS_BACNET
  // BACnet Driver 생성 (Factory 사용)
  bacnet_driver_ =
      PulseOne::Drivers::DriverFactory::GetInstance().CreateDriver("BACNET");
  if (!bacnet_driver_) {
    LogMessage(LogLevel::LOG_ERROR,
               "Failed to create BACnetDriver instance via Factory");
    // 생성자 실패 처리 (예외 던지기 등 필요하지만 여기서는 로그만)
  }

  // BACnet 워커 통계 초기화
  worker_stats_.start_time = system_clock::now();
  worker_stats_.last_reset = worker_stats_.start_time;

  // BACnet 설정 파싱
  if (!ParseBACnetConfigFromDeviceInfo()) {
    LogMessage(LogLevel::WARN,
               "Failed to parse BACnet config from DeviceInfo, using defaults");
  }

  // BACnet Driver 초기화
  if (!InitializeBACnetDriver()) {
    LogMessage(LogLevel::LOG_ERROR, "Failed to initialize BACnet driver");
    return;
  }

  // BACnetServiceManager 초기화
  if (bacnet_driver_) {
    // IProtocolDriver 인터페이스를 직접 전달
    bacnet_service_manager_ =
        std::make_shared<PulseOne::Drivers::BACnetServiceManager>(
            bacnet_driver_.get());
    LogMessage(LogLevel::INFO, "BACnetServiceManager initialized successfully");
  } else {
    LogMessage(LogLevel::LOG_ERROR,
               "Cannot initialize BACnetServiceManager: BACnet driver is null");
  }

  LogMessage(LogLevel::INFO, "BACnetWorker initialization completed");
#else
  LogMessage(LogLevel::WARN, "BACnet support disabled in this build");
#endif
}

BACnetWorker::~BACnetWorker() {
#if HAS_BACNET
  // 스레드 정리
  if (data_scan_thread_running_.load()) {
    data_scan_thread_running_ = false;

    if (data_scan_thread_ && data_scan_thread_->joinable()) {
      data_scan_thread_->join();
    }
  }

  // BACnet 드라이버 정리
  ShutdownBACnetDriver();
#endif

  LogMessage(LogLevel::INFO,
             "BACnetWorker destroyed for device: " + device_info_.name);
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현
// =============================================================================

std::future<bool> BACnetWorker::Start() {
  return std::async(std::launch::async, [this]() -> bool {
    LogMessage(LogLevel::INFO, "Starting BACnetWorker...");

#if HAS_BACNET
    // 1. 통계 초기화
    worker_stats_.Reset();
    StartReconnectionThread();

    // 2. UDP 연결 수립
    if (!EstablishConnection()) {
      LogMessage(LogLevel::LOG_ERROR, "Failed to establish UDP connection");
      return false;
    }

    // 3. 데이터 스캔 스레드 시작
    data_scan_thread_running_ = true;
    data_scan_thread_ = std::make_unique<std::thread>(
        &BACnetWorker::DataScanThreadFunction, this);

    LogMessage(LogLevel::INFO, "BACnetWorker started successfully");
    return true;
#else
        return false;
#endif
  });
}

std::future<bool> BACnetWorker::Stop() {
  return std::async(std::launch::async, [this]() -> bool {
    LogMessage(LogLevel::INFO, "Stopping BACnetWorker...");

#if HAS_BACNET
    // 1. BaseDeviceWorker의 스레드 정리 (재연결 스레드 등)
    StopAllThreads();

    // 2. BACnet 특정 스레드 정리
    if (data_scan_thread_running_.load()) {
      data_scan_thread_running_ = false;

      if (data_scan_thread_ && data_scan_thread_->joinable()) {
        data_scan_thread_->join();
      }
    }

    // 2. BACnet 드라이버 정리
    ShutdownBACnetDriver();

    // 3. UDP 연결 해제
    CloseConnection();

    LogMessage(LogLevel::INFO, "BACnetWorker stopped successfully");
    return true;
#else
        return true;
#endif
  });
}
// =============================================================================
// UdpBasedWorker 순수 가상 함수 구현
// =============================================================================

bool BACnetWorker::EstablishProtocolConnection() {
#if HAS_BACNET
  LogMessage(LogLevel::INFO, "Establishing BACnet protocol connection...");

  if (!bacnet_driver_) {
    LogMessage(LogLevel::LOG_ERROR, "BACnet driver not initialized");
    return false;
  }

  if (bacnet_driver_->Connect()) {
    LogMessage(LogLevel::INFO, "BACnet protocol connection established");
    return true;
  }

  const auto &error = bacnet_driver_->GetLastError();
  LogMessage(LogLevel::WARN,
             "BACnet connection attempt failed: " + error.message);

  return false;
#else
  return false;
#endif
}

bool BACnetWorker::CloseProtocolConnection() {
#if HAS_BACNET
  LogMessage(LogLevel::INFO, "Closing BACnet protocol connection...");
  ShutdownBACnetDriver();
  LogMessage(LogLevel::INFO, "BACnet protocol connection closed");
  return true;
#else
  return true;
#endif
}

bool BACnetWorker::CheckProtocolConnection() {
#if HAS_BACNET
  if (!bacnet_driver_) {
    return false;
  }
  return bacnet_driver_->IsConnected();
#else
  return false;
#endif
}

bool BACnetWorker::SendProtocolKeepAlive() { return CheckProtocolConnection(); }

bool BACnetWorker::ProcessReceivedPacket(const UdpPacket &packet) {
#if HAS_BACNET
  try {
    LogMessage(LogLevel::DEBUG_LEVEL, "Processing BACnet packet (" +
                                          std::to_string(packet.data.size()) +
                                          " bytes)");

    UpdateWorkerStats("packet_received", true);
    return true;

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "Exception in ProcessReceivedPacket: " + std::string(e.what()));
    UpdateWorkerStats("packet_received", false);
    return false;
  }
#else
  return false;
#endif
}

// =============================================================================
// 데이터 스캔 + 파이프라인 전송 기능들
// =============================================================================

bool BACnetWorker::SendBACnetDataToPipeline(
    const std::map<std::string, PulseOne::Structs::DataValue> &object_values,
    const std::string &context, uint32_t priority) {

#if HAS_BACNET
  if (object_values.empty()) {
    return false;
  }

  try {
    std::vector<PulseOne::Structs::TimestampedValue> timestamped_values;
    timestamped_values.reserve(object_values.size());

    auto timestamp = std::chrono::system_clock::now();

    for (const auto &[object_id, value] : object_values) {
      PulseOne::Structs::TimestampedValue tv;

      // 핵심 필드들
      tv.value = value;
      tv.timestamp = timestamp;
      tv.quality = PulseOne::Enums::DataQuality::GOOD;
      tv.source = "bacnet_" + object_id;

      // 최적화된 DataPoint 검색
      PulseOne::Structs::DataPoint *data_point =
          FindDataPointByObjectIdOptimized(object_id);
      if (data_point) {
        tv.point_id = std::stoi(data_point->id);
        tv.scaling_factor = data_point->scaling_factor;
        tv.scaling_offset = data_point->scaling_offset;

        // 스레드 안전한 이전값 비교
        {
          std::lock_guard<std::mutex> lock(previous_values_mutex_);
          auto prev_it = previous_values_.find(object_id);
          if (prev_it != previous_values_.end()) {
            tv.previous_value = prev_it->second;
            tv.value_changed = (tv.value != prev_it->second);
          } else {
            tv.previous_value = PulseOne::Structs::DataValue{};
            tv.value_changed = true;
          }

          // 이전값 캐시 업데이트
          previous_values_[object_id] = tv.value;
        }

      } else {
        // DataPoint가 없는 경우
        tv.point_id = std::hash<std::string>{}(object_id) % 100000;
        tv.value_changed = true;
        tv.scaling_factor = 1.0;
        tv.scaling_offset = 0.0;

        // 스레드 안전한 이전값 관리
        std::lock_guard<std::mutex> lock(previous_values_mutex_);
        auto prev_it = previous_values_.find(object_id);
        if (prev_it != previous_values_.end()) {
          tv.previous_value = prev_it->second;
          tv.value_changed = (tv.value != prev_it->second);
        } else {
          tv.previous_value = PulseOne::Structs::DataValue{};
          tv.value_changed = true;
        }
        previous_values_[object_id] = tv.value;
      }

      tv.sequence_number = GetNextSequenceNumber();
      tv.change_threshold = 0.0;
      tv.force_rdb_store = tv.value_changed;

      timestamped_values.push_back(tv);
    }

    if (timestamped_values.empty()) {
      LogMessage(LogLevel::DEBUG_LEVEL, "No BACnet values to send: " + context);
      return true;
    }

    // BaseDeviceWorker::SendValuesToPipelineWithLogging() 호출
    bool success = SendValuesToPipelineWithLogging(
        timestamped_values,
        "BACnet " + context + " (" + std::to_string(object_values.size()) +
            " objects)",
        priority);

    if (success) {
      LogMessage(LogLevel::DEBUG_LEVEL,
                 "BACnet 데이터 전송 성공: " +
                     std::to_string(timestamped_values.size()) + "/" +
                     std::to_string(object_values.size()) + " 객체");
    }

    return success;

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "SendBACnetDataToPipeline 예외: " + std::string(e.what()));
    return false;
  }
#else
  return false;
#endif
}

bool BACnetWorker::SendBACnetPropertyToPipeline(
    const std::string &object_id,
    const PulseOne::Structs::DataValue &property_value,
    const std::string &object_name, uint32_t priority) {
#if HAS_BACNET
  try {
    PulseOne::Structs::TimestampedValue tv;

    tv.value = property_value;
    tv.timestamp = std::chrono::system_clock::now();
    tv.quality = PulseOne::Enums::DataQuality::GOOD;
    tv.source =
        "bacnet_" + object_id + (!object_name.empty() ? "_" + object_name : "");

    // 최적화된 DataPoint 검색
    PulseOne::Structs::DataPoint *data_point =
        FindDataPointByObjectIdOptimized(object_id);
    if (data_point) {
      tv.point_id = std::stoi(data_point->id);
      tv.scaling_factor = data_point->scaling_factor;
      tv.scaling_offset = data_point->scaling_offset;
    } else {
      tv.point_id = std::hash<std::string>{}(object_id) % 100000;
      tv.scaling_factor = 1.0;
      tv.scaling_offset = 0.0;
    }

    // 스레드 안전한 이전값 처리
    {
      std::lock_guard<std::mutex> lock(previous_values_mutex_);
      auto prev_it = previous_values_.find(object_id);
      if (prev_it != previous_values_.end()) {
        tv.previous_value = prev_it->second;
        tv.value_changed = (tv.value != prev_it->second);
      } else {
        tv.previous_value = PulseOne::Structs::DataValue{};
        tv.value_changed = true;
      }
      previous_values_[object_id] = tv.value;
    }

    tv.sequence_number = GetNextSequenceNumber();
    tv.change_threshold = 0.0;
    tv.force_rdb_store = tv.value_changed;

    bool success = SendValuesToPipelineWithLogging(
        {tv},
        "BACnet Property: " + object_id +
            (!object_name.empty() ? " (" + object_name + ")" : ""),
        priority);

    if (success) {
      LogMessage(LogLevel::DEBUG_LEVEL,
                 "BACnet Property 전송 성공: " + object_id);
    }

    return success;

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "SendBACnetPropertyToPipeline 예외: " + std::string(e.what()));
    return false;
  }
#else
  return false;
#endif
}

bool BACnetWorker::SendCOVNotificationToPipeline(
    const std::string &object_id, const PulseOne::Structs::DataValue &new_value,
    const PulseOne::Structs::DataValue &previous_value) {
#if HAS_BACNET
  (void)previous_value; // 사용하지 않는 매개변수 경고 방지

  try {
    PulseOne::Structs::TimestampedValue tv;
    tv.value = new_value;
    tv.timestamp = std::chrono::system_clock::now();
    tv.quality = PulseOne::Enums::DataQuality::GOOD;
    tv.source = "bacnet_cov_" + object_id;

    // COV 알림은 높은 우선순위
    uint32_t cov_priority = 5;

    // 최적화된 DataPoint 검색
    PulseOne::Structs::DataPoint *data_point =
        FindDataPointByObjectIdOptimized(object_id);
    if (data_point) {
      tv.point_id = std::stoi(data_point->id);
      tv.scaling_factor = data_point->scaling_factor;
      tv.scaling_offset = data_point->scaling_offset;
    } else {
      tv.point_id = std::hash<std::string>{}(object_id) % 100000;
      tv.scaling_factor = 1.0;
      tv.scaling_offset = 0.0;
    }

    tv.sequence_number = GetNextSequenceNumber();
    tv.value_changed = true; // COV는 항상 변화
    tv.change_threshold = 0.0;
    tv.force_rdb_store = true; // COV는 항상 저장

    bool success = SendValuesToPipelineWithLogging(
        {tv}, "BACnet COV: " + object_id, cov_priority);

    if (success) {
      worker_stats_.cov_notifications++;
      LogMessage(LogLevel::INFO,
                 "COV notification sent for object: " + object_id);
    }

    return success;

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "SendCOVNotificationToPipeline 예외: " + std::string(e.what()));
    return false;
  }
#else
  return false;
#endif
}
// =============================================================================
// 최적화된 DataPoint 검색
// =============================================================================

PulseOne::Structs::DataPoint *
BACnetWorker::FindDataPointByObjectId(const std::string &object_id) {
  return FindDataPointByObjectIdOptimized(object_id);
}

PulseOne::Structs::DataPoint *
BACnetWorker::FindDataPointByObjectIdOptimized(const std::string &object_id) {
  std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);

  // 빠른 검색을 위한 인덱스 활용
  auto index_it = datapoint_index_.find(object_id);
  if (index_it != datapoint_index_.end()) {
    return index_it->second;
  }

  // 인덱스에 없으면 전체 검색 후 인덱스 업데이트
  for (auto &point : configured_data_points_) {
    if (point.name == object_id || point.id == object_id) {
      datapoint_index_[object_id] = &point;
      return &point;
    }

    // protocol_params에서 검색
    auto obj_prop = point.protocol_params.find("object_id");
    if (obj_prop != point.protocol_params.end() &&
        obj_prop->second == object_id) {
      datapoint_index_[object_id] = &point;
      return &point;
    }

    auto bacnet_obj_prop = point.protocol_params.find("bacnet_object_id");
    if (bacnet_obj_prop != point.protocol_params.end() &&
        bacnet_obj_prop->second == object_id) {
      datapoint_index_[object_id] = &point;
      return &point;
    }
  }

  return nullptr;
}

void BACnetWorker::RebuildDataPointIndex() {
  std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);

  datapoint_index_.clear();

  for (auto &point : configured_data_points_) {
    // 기본 필드들로 인덱스 생성
    if (!point.name.empty()) {
      datapoint_index_[point.name] = &point;
    }
    if (!point.id.empty()) {
      datapoint_index_[point.id] = &point;
    }

    // protocol_params 기반 인덱스
    auto obj_prop = point.protocol_params.find("object_id");
    if (obj_prop != point.protocol_params.end()) {
      datapoint_index_[obj_prop->second] = &point;
    }

    auto bacnet_obj_prop = point.protocol_params.find("bacnet_object_id");
    if (bacnet_obj_prop != point.protocol_params.end()) {
      datapoint_index_[bacnet_obj_prop->second] = &point;
    }
  }

  LogMessage(LogLevel::DEBUG_LEVEL,
             "DataPoint index rebuilt: " +
                 std::to_string(datapoint_index_.size()) + " entries");
}

// =============================================================================
// 메모리 정리 기능 추가
// =============================================================================

void BACnetWorker::CleanupPreviousValues() {
  std::lock_guard<std::mutex> lock(previous_values_mutex_);

  const size_t max_entries = 10000; // 최대 항목 수

  if (previous_values_.size() > max_entries) {
    // 사용되지 않는 항목들을 정리
    // 간단한 LRU 방식: 절반만 유지
    auto it = previous_values_.begin();
    std::advance(it, previous_values_.size() / 2);
    previous_values_.erase(previous_values_.begin(), it);

    LogMessage(LogLevel::INFO, "Previous values cleaned up, remaining: " +
                                   std::to_string(previous_values_.size()));
  }
}
// =============================================================================
// 설정 및 상태 관리
// =============================================================================

void BACnetWorker::LoadDataPointsFromConfiguration(
    const std::vector<DataPoint> &data_points) {
  {
    std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);
    configured_data_points_ = data_points;
  }

  // 인덱스 재구성
  RebuildDataPointIndex();

  LogMessage(LogLevel::INFO, "Loaded " + std::to_string(data_points.size()) +
                                 " data points from configuration");
}

std::vector<DataPoint> BACnetWorker::GetConfiguredDataPoints() const {
  std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);
  return configured_data_points_;
}

std::string BACnetWorker::GetBACnetWorkerStats() const {
#if HAS_BACNET
  std::stringstream ss;
  ss << "{\n";
  ss << "  \"polling_cycles\": " << worker_stats_.polling_cycles.load()
     << ",\n";
  ss << "  \"read_operations\": " << worker_stats_.read_operations.load()
     << ",\n";
  ss << "  \"write_operations\": " << worker_stats_.write_operations.load()
     << ",\n";
  ss << "  \"failed_operations\": " << worker_stats_.failed_operations.load()
     << ",\n";
  ss << "  \"cov_notifications\": " << worker_stats_.cov_notifications.load()
     << ",\n";

  // 메모리 사용량 정보 추가
  {
    std::lock_guard<std::mutex> lock(previous_values_mutex_);
    ss << "  \"previous_values_count\": " << previous_values_.size() << ",\n";
  }
  {
    std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);
    ss << "  \"datapoint_index_count\": " << datapoint_index_.size() << ",\n";
  }

  auto start_time = std::chrono::duration_cast<std::chrono::seconds>(
                        worker_stats_.start_time.time_since_epoch())
                        .count();
  auto last_reset = std::chrono::duration_cast<std::chrono::seconds>(
                        worker_stats_.last_reset.time_since_epoch())
                        .count();

  ss << "  \"start_time\": " << start_time << ",\n";
  ss << "  \"last_reset\": " << last_reset << "\n";
  ss << "}";

  return ss.str();
#else
  return "{}";
#endif
}

void BACnetWorker::ResetBACnetWorkerStats() {
#if HAS_BACNET
  worker_stats_.Reset();
#endif
  LogMessage(LogLevel::INFO, "BACnet worker statistics reset");
}

void BACnetWorker::SetValueChangedCallback(ValueChangedCallback callback) {
  on_value_changed_ = callback;
}

// =============================================================================
// 내부 구현 메서드들
// =============================================================================

bool BACnetWorker::ParseBACnetConfigFromDeviceInfo() {
#if HAS_BACNET
  try {
    const auto &props = device_info_.properties;

    auto local_device_it = props.find("bacnet_local_device_id");
    if (local_device_it != props.end()) {
      LogMessage(LogLevel::DEBUG_LEVEL,
                 "BACnet local device ID: " + local_device_it->second);
    }

    LogMessage(LogLevel::DEBUG_LEVEL,
               "BACnet endpoint: " + device_info_.endpoint);
    LogMessage(LogLevel::DEBUG_LEVEL,
               "BACnet timeout: " + std::to_string(device_info_.timeout_ms));

    return true;

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "Exception in ParseBACnetConfigFromDeviceInfo: " +
                   std::string(e.what()));
    return false;
  }
#else
  return false;
#endif
}

PulseOne::Structs::DriverConfig
BACnetWorker::CreateDriverConfigFromDeviceInfo() {
#if HAS_BACNET
  LogMessage(LogLevel::INFO, "Creating BACnet DriverConfig from DeviceInfo...");

  PulseOne::Structs::DriverConfig config = device_info_.driver_config;

  const auto &props = device_info_.properties;

  // BACnet device_id 찾기
  std::string bacnet_device_id = "";
  std::vector<std::string> device_id_keys = {"device_id", "local_device_id",
                                             "bacnet_device_id",
                                             "bacnet_local_device_id"};

  for (const auto &key : device_id_keys) {
    auto it = props.find(key);
    if (it != props.end() && !it->second.empty()) {
      bacnet_device_id = it->second;
      break;
    }
  }

  if (!bacnet_device_id.empty()) {
    config.device_id = bacnet_device_id;
  } else {
    config.device_id = "260001";
    LogMessage(LogLevel::WARN,
               "No BACnet device_id found, using default: 260001");
  }

  config.protocol = "BACNET_IP";
  config.endpoint = device_info_.endpoint;

  if (device_info_.timeout_ms > 0) {
    config.timeout_ms = device_info_.timeout_ms;
  }
  if (device_info_.retry_count > 0) {
    config.retry_count = static_cast<uint32_t>(device_info_.retry_count);
  }
  if (device_info_.polling_interval_ms > 0) {
    config.polling_interval_ms =
        static_cast<uint32_t>(device_info_.polling_interval_ms);
  }

  config.auto_reconnect = true;

  // BACnet 특화 설정들
  config.properties["device_id"] = config.device_id;

  auto local_it = props.find("local_device_id");
  if (local_it != props.end()) {
    config.properties["local_device_id"] = local_it->second;
  } else {
    config.properties["local_device_id"] = "1001"; // Default for collector
  }

  auto port_it = props.find("bacnet_port");
  config.properties["port"] =
      (port_it != props.end()) ? port_it->second : "47808";

  auto cov_it = props.find("bacnet_enable_cov");
  config.properties["enable_cov"] =
      (cov_it != props.end()) ? cov_it->second : "false";

  LogMessage(LogLevel::INFO, "BACnet DriverConfig created successfully");

  return config;
#else
  return PulseOne::Structs::DriverConfig{};
#endif
}

bool BACnetWorker::InitializeBACnetDriver() {
#if HAS_BACNET
  try {
    LogMessage(LogLevel::INFO, "Initializing BACnet driver...");

    if (!bacnet_driver_) {
      LogMessage(LogLevel::LOG_ERROR, "BACnet driver is null");
      return false;
    }

    auto driver_config = CreateDriverConfigFromDeviceInfo();

    if (!bacnet_driver_->Initialize(driver_config)) {
      const auto &error = bacnet_driver_->GetLastError();
      LogMessage(LogLevel::LOG_ERROR,
                 "BACnet driver initialization failed: " + error.message);
      return false;
    }

    SetupBACnetDriverCallbacks();

    LogMessage(LogLevel::INFO, "BACnet driver initialized successfully");
    return true;

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "Exception during BACnet driver initialization: " +
                   std::string(e.what()));
    return false;
  }
#else
  return false;
#endif
}

void BACnetWorker::ShutdownBACnetDriver() {
#if HAS_BACNET
  try {
    if (bacnet_driver_) {
      bacnet_driver_->Disconnect();
      bacnet_driver_.reset();
      LogMessage(LogLevel::INFO, "BACnet driver shutdown complete");
    }
  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR, "Exception shutting down BACnet driver: " +
                                        std::string(e.what()));
  }
#endif
}

// =============================================================================
// 데이터 스캔 스레드
// =============================================================================

void BACnetWorker::DataScanThreadFunction() {
#if HAS_BACNET
  LogMessage(LogLevel::INFO, "BACnet data scan thread started");

  uint32_t polling_interval_ms = device_info_.polling_interval_ms;

  while (data_scan_thread_running_.load()) {
    try {
      if (PerformDataScan()) {
        worker_stats_.polling_cycles++;
      }

      // 주기적인 메모리 정리 (10분마다)
      cleanup_timer_++;
      if (cleanup_timer_ >= (600000 / polling_interval_ms)) { // 10분
        CleanupPreviousValues();
        cleanup_timer_ = 0;
      }

      std::this_thread::sleep_for(
          std::chrono::milliseconds(polling_interval_ms));

    } catch (const std::exception &e) {
      LogMessage(LogLevel::LOG_ERROR,
                 "Exception in data scan thread: " + std::string(e.what()));
      std::this_thread::sleep_for(std::chrono::seconds(5));
    }
  }

  LogMessage(LogLevel::INFO, "BACnet data scan thread stopped");
#endif
}

bool BACnetWorker::PerformDataScan() {
#if HAS_BACNET
  LogMessage(LogLevel::DEBUG_LEVEL, "Performing BACnet data scan...");

  try {
    if (!bacnet_driver_ || !bacnet_driver_->IsConnected()) {
      return false;
    }

    // 설정된 DataPoint들 가져오기
    std::vector<DataPoint> data_points_to_scan;

    {
      std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);
      data_points_to_scan = configured_data_points_;
    }

    if (data_points_to_scan.empty()) {
      LogMessage(LogLevel::DEBUG_LEVEL,
                 "No data points configured for scanning");
      return true;
    }

    // BACnet Present Value들 읽기
    std::vector<TimestampedValue> values;
    bool success = bacnet_driver_->ReadValues(data_points_to_scan, values);

    if (success && !values.empty()) {
      worker_stats_.read_operations++;

      // 파이프라인으로 전송
      bool pipeline_success =
          SendValuesToPipelineWithLogging(values, "BACnet Data Scan", 0);

      if (pipeline_success) {
        LogMessage(LogLevel::DEBUG_LEVEL, "Successfully sent " +
                                              std::to_string(values.size()) +
                                              " BACnet values to pipeline");
      }

      // COV 처리
      for (size_t i = 0; i < data_points_to_scan.size() && i < values.size();
           ++i) {
        std::string object_id = CreateObjectId(data_points_to_scan[i]);

        if (on_value_changed_) {
          on_value_changed_(object_id, values[i]);
        }

        ProcessValueChangeForCOV(object_id, values[i]);
      }
    } else {
      worker_stats_.failed_operations++;
    }

    // 5. 스케줄 동기화 수행 (Step 15)
    static auto last_sync_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_sync_time)
            .count() >= 60) {
      SyncSchedulesWithDevices();
      last_sync_time = now;
    }

    return true;

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "Exception in PerformDataScan: " + std::string(e.what()));
    UpdateWorkerStats("failed_operations", true);
    return false;
  }
#else
  return false;
#endif
}

// =============================================================================
// Implementation of missing Write/Control interfaces
// =============================================================================

#if HAS_BACNET

bool BACnetWorker::WriteDataPoint(const std::string &point_id,
                                  const DataValue &value) {
  return WriteDataPointValue(point_id, value);
}

bool BACnetWorker::WriteAnalogOutput(const std::string &output_id,
                                     double value) {
  // Convert output_id to DataPoint ID or address and write
  PulseOne::Structs::DataPoint *point =
      FindDataPointByObjectIdOptimized(output_id);
  if (!point)
    return false;
  return WriteDataPointValue(point->id, DataValue(value));
}

bool BACnetWorker::WriteDigitalOutput(const std::string &output_id,
                                      bool value) {
  PulseOne::Structs::DataPoint *point =
      FindDataPointByObjectIdOptimized(output_id);
  if (!point)
    return false;
  return WriteDataPointValue(point->id, DataValue(value));
}

bool BACnetWorker::WriteSetpoint(const std::string &setpoint_id, double value) {
  return WriteAnalogOutput(setpoint_id, value);
}

bool BACnetWorker::ControlDigitalDevice(const std::string &device_id,
                                        bool enable) {
  return WriteDigitalOutput(device_id, enable);
}

bool BACnetWorker::ControlAnalogDevice(const std::string &device_id,
                                       double value) {
  return WriteAnalogOutput(device_id, value);
}

// Private helper
bool BACnetWorker::WriteDataPointValue(const std::string &point_id,
                                       const DataValue &value) {
  if (!bacnet_driver_)
    return false;

  // Find point by ID
  std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);
  auto it = std::find_if(
      configured_data_points_.begin(), configured_data_points_.end(),
      [&point_id](const DataPoint &p) { return p.id == point_id; });

  if (it != configured_data_points_.end()) {
    return bacnet_driver_->WriteValue(*it, value);
  }
  return false;
}

// Stub for sync schedules
bool BACnetWorker::SyncSchedulesWithDevices() {
  // Basic implementation or stub
  return true;
}

// Missing stubs from header
bool BACnetWorker::WriteProperty(uint32_t, BACNET_OBJECT_TYPE, uint32_t,
                                 BACNET_PROPERTY_ID, const DataValue &,
                                 uint8_t) {
  return false; // Not implemented yet
}
bool BACnetWorker::WriteObjectProperty(const std::string &, const DataValue &,
                                       uint8_t) {
  return false; // Not implemented yet
}
bool BACnetWorker::WriteBACnetDataPoint(const std::string &,
                                        const DataValue &) {
  return false; // Not implemented yet
}

#endif

// -----------------------------------------------------------
// Missing implementations stubbed for HAS_BACNET=0
// -----------------------------------------------------------
#if !HAS_BACNET
bool BACnetWorker::WriteProperty(uint32_t, BACNET_OBJECT_TYPE, uint32_t,
                                 BACNET_PROPERTY_ID, const DataValue &,
                                 uint8_t) {
  return false;
}
bool BACnetWorker::WriteObjectProperty(const std::string &, const DataValue &,
                                       uint8_t) {
  return false;
}
bool BACnetWorker::WriteBACnetDataPoint(const std::string &,
                                        const DataValue &) {
  return false;
}
bool BACnetWorker::SyncSchedulesWithDevices() { return false; }
bool BACnetWorker::WriteDataPoint(const std::string &, const DataValue &) {
  return false;
}
bool BACnetWorker::WriteAnalogOutput(const std::string &, double) {
  return false;
}
bool BACnetWorker::WriteDigitalOutput(const std::string &, bool) {
  return false;
}
bool BACnetWorker::WriteSetpoint(const std::string &, double) { return false; }
bool BACnetWorker::ControlDigitalDevice(const std::string &, bool) {
  return false;
}
bool BACnetWorker::ControlAnalogDevice(const std::string &, double) {
  return false;
}
#endif

// -----------------------------------------------------------
// Private methods implemented conditionally or stubbed
// -----------------------------------------------------------
#if HAS_BACNET
bool BACnetWorker::ProcessDataPoints(const std::vector<DataPoint> &points) {
  (void)points;
  return true;
}
void BACnetWorker::UpdateWorkerStats(const std::string &operation,
                                     bool success) {
  if (operation == "packet_received") {
    // ...
  } else if (operation == "failed_operations") {
    if (!success)
      worker_stats_.failed_operations++;
  }
}
std::string BACnetWorker::CreateObjectId(const DataPoint &point) const {
  if (!point.id.empty())
    return point.id;
  return point.address_string;
}
void BACnetWorker::SetupBACnetDriverCallbacks() {
  // ...
}
void BACnetWorker::ProcessValueChangeForCOV(const std::string &object_id,
                                            const TimestampedValue &new_value) {
  std::lock_guard<std::mutex> lock(previous_values_mutex_);
  // ...
}
bool BACnetWorker::IsValueChanged(const DataValue &previous,
                                  const DataValue &current) {
  return previous != current;
}

bool BACnetWorker::ParseBACnetObjectId(const std::string &object_id,
                                       uint32_t &device_id,
                                       BACNET_OBJECT_TYPE &object_type,
                                       uint32_t &object_instance) {
  return false;
}
std::optional<DataPoint>
BACnetWorker::FindDataPointById(const std::string &point_id) {
  return std::nullopt;
}
void BACnetWorker::LogWriteOperation(const std::string &object_id,
                                     const DataValue &value,
                                     const std::string &property_name,
                                     bool success) {
  // ...
}
#endif

// Stubs for private methods when !HAS_BACNET
#if !HAS_BACNET
bool BACnetWorker::ProcessDataPoints(const std::vector<DataPoint> &) {
  return false;
}
void BACnetWorker::UpdateWorkerStats(const std::string &, bool) {}
std::string BACnetWorker::CreateObjectId(const DataPoint &) const { return ""; }
void BACnetWorker::SetupBACnetDriverCallbacks() {}
void BACnetWorker::ProcessValueChangeForCOV(const std::string &,
                                            const TimestampedValue &) {}
bool BACnetWorker::IsValueChanged(const DataValue &, const DataValue &) {
  return false;
}
bool BACnetWorker::WriteDataPointValue(const std::string &, const DataValue &) {
  return false;
}
bool BACnetWorker::ParseBACnetObjectId(const std::string &, uint32_t &,
                                       BACNET_OBJECT_TYPE &, uint32_t &) {
  return false;
}
std::optional<DataPoint> BACnetWorker::FindDataPointById(const std::string &) {
  return std::nullopt;
}
void BACnetWorker::LogWriteOperation(const std::string &, const DataValue &,
                                     const std::string &, bool) {}
#endif

} // namespace Workers
} // namespace PulseOne