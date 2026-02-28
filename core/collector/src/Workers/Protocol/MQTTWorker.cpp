/**
 * @file MQTTWorker.cpp - 통합 MQTT 워커 구현부 (기본 + 프로덕션 모드 +
 * 파이프라인 연동 완성)
 * @brief 하나의 클래스로 모든 MQTT 기능 구현 - ModbusTcpWorker 패턴 완전 적용
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 3.1.0 (파이프라인 연동 완성 버전)
 */

/*
MQTT 디바이스 설정 예시:

devices 테이블:
- endpoint: "mqtt://192.168.2.50:1883"
- config: {
   "topic": "sensors/temperature",
   "qos": 1,
   "client_id": "pulseone_temp_sensor_001",
   "username": "mqtt_user",
   "password": "mqtt_pass",
   "keepalive_interval": 120,
   "clean_session": true,
   "use_ssl": false,
   "timeout": 10000
}

또는 connection_string으로:
- connection_string: "mqtt://192.168.2.50:1883"
- config: {"topic": "sensors/temperature", "qos": 2, "use_ssl": true}
*/

#include "Workers/Protocol/MQTTWorker.h"
#include "Common/Enums.h"
#include "Drivers/Common/DriverFactory.h" // Plugin System Factory
#include "Logging/LogManager.h"
#ifdef HAS_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

#include "Storage/BlobStore.h"
#include "Workers/Protocol/MQTTTransferManager.h"
#include <algorithm>
#include <unordered_set>

using namespace std::chrono;
using namespace PulseOne::Drivers;

using LogLevel = PulseOne::Enums::LogLevel;
namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자 (ModbusTcpWorker 패턴 완전 적용)
// =============================================================================

MQTTWorker::MQTTWorker(const PulseOne::DeviceInfo &device_info,
                       MQTTWorkerMode mode)
    : BaseDeviceWorker(device_info), worker_mode_(mode), mqtt_driver_(nullptr),
      next_subscription_id_(1), message_thread_running_(false),
      publish_thread_running_(false), default_message_timeout_ms_(30000),
      max_publish_queue_size_(10000), auto_reconnect_enabled_(true),
      metrics_thread_running_(false), priority_thread_running_(false),
      alarm_thread_running_(false), start_time_(steady_clock::now()),
      last_throughput_calculation_(steady_clock::now()) {

  LogMessage(LogLevel::INFO,
             "MQTTWorker created for device: " + device_info.name + " (Mode: " +
                 (mode == MQTTWorkerMode::PRODUCTION ? "PRODUCTION" : "BASIC") +
                 ")");

  // 설정 파싱 (ModbusTcpWorker와 동일한 5단계 프로세스)
  if (!ParseMQTTConfig()) {
    LogMessage(LogLevel::LOG_ERROR, "Failed to parse MQTT configuration");
    return;
  }

  // MqttDriver 초기화
  if (!InitializeMQTTDriver()) {
    LogMessage(LogLevel::LOG_ERROR, "Failed to initialize MqttDriver");
    return;
  }

  LogMessage(LogLevel::INFO, "MQTTWorker initialization completed");
}

MQTTWorker::~MQTTWorker() {
  // 프로덕션 모드 스레드들 먼저 정리
  StopProductionThreads();

  // 기본 스레드 정리
  message_thread_running_ = false;
  publish_thread_running_ = false;
  publish_queue_cv_.notify_all();

  if (message_processor_thread_ && message_processor_thread_->joinable()) {
    message_processor_thread_->join();
  }
  if (publish_processor_thread_ && publish_processor_thread_->joinable()) {
    publish_processor_thread_->join();
  }

  // MqttDriver 정리 (자동으로 연결 해제됨)
  LogMessage(LogLevel::INFO,
             "MQTTWorker destroyed for device: " + device_info_.name);
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현
// =============================================================================

std::future<bool> MQTTWorker::Start() {
  auto promise = std::make_shared<std::promise<bool>>();
  auto future = promise->get_future();

  // [CRITICAL FIX] UAF 방어: raw this 대신 shared_ptr 캡처
  auto self = shared_from_this();
  auto mqtt_self = std::dynamic_pointer_cast<MQTTWorker>(self);

  std::thread([mqtt_self, promise]() {
    try {
      mqtt_self->LogMessage(LogLevel::INFO, "Starting MQTT worker...");

      // 1. DataPoint 기반 자동 구독 등록
      // [LOCK ORDER CONTRACT]
      // data_points_mutex_ → subscriptions_mutex_ 순서로만 획득.
      // AddSubscription()은 subscriptions_mutex_를 내부적으로 잡으므로
      // 이 블록 안에서만 호출 허용.
      // ⚠️ 역순서 금지: subscriptions_mutex_ 보유 중 data_points_mutex_ 획득
      // 불가.
      {
        std::lock_guard<std::recursive_mutex> lock(
            mqtt_self->data_points_mutex_);
        for (const auto &dp : mqtt_self->data_points_) {
          if (!dp.address_string.empty()) {
            MQTTSubscription sub;
            sub.topic = dp.address_string;
            sub.qos = MqttQoS::AT_LEAST_ONCE;
            mqtt_self->AddSubscription(sub);
          }
        }
      }

      // 1.1 [Auto-Registration] 설정된 베이스 토픽(와일드카드 포함) 자동 구독
      if (!mqtt_self->mqtt_config_.topic.empty()) {
        std::stringstream ss(mqtt_self->mqtt_config_.topic);
        std::string segment;
        while (std::getline(ss, segment, ',')) {
          segment.erase(0, segment.find_first_not_of(" \t\n\r\f\v"));
          segment.erase(segment.find_last_not_of(" \t\n\r\f\v") + 1);

          if (!segment.empty()) {
            MQTTSubscription sub;
            sub.topic = segment;
            sub.qos = mqtt_self->mqtt_config_.default_qos;
            mqtt_self->LogMessage(LogLevel::INFO,
                                  "자동 등록을 위한 베이스 토픽 구독: " +
                                      sub.topic);
            mqtt_self->AddSubscription(sub);
          }
        }
      }

      // 🔥 BaseDeviceWorker의 재연결 관리 스레드 시작
      mqtt_self->StartReconnectionThread();

      if (mqtt_self->EstablishConnection()) {
        mqtt_self->ChangeState(WorkerState::RUNNING);
      } else {
        mqtt_self->ChangeState(WorkerState::RECONNECTING);
      }

      // 2. 메시지 처리 스레드 시작
      mqtt_self->message_thread_running_ = true;
      mqtt_self->message_processor_thread_ = std::make_unique<std::thread>(
          &MQTTWorker::MessageProcessorThreadFunction, mqtt_self.get());

      // 3. 발행 처리 스레드 시작
      mqtt_self->publish_thread_running_ = true;
      mqtt_self->publish_processor_thread_ = std::make_unique<std::thread>(
          &MQTTWorker::PublishProcessorThreadFunction, mqtt_self.get());

      // 4. 프로덕션 모드라면 고급 스레드들 시작
      if (mqtt_self->IsProductionMode()) {
        mqtt_self->StartProductionThreads();
      }

      mqtt_self->LogMessage(LogLevel::INFO, "MQTT worker started successfully");
      promise->set_value(true);

    } catch (const std::exception &e) {
      mqtt_self->LogMessage(LogLevel::LOG_ERROR,
                            "Failed to start MQTT worker: " +
                                std::string(e.what()));
      promise->set_value(false);
    }
  }).detach();

  return future;
}

std::future<bool> MQTTWorker::Stop() {
  auto promise = std::make_shared<std::promise<bool>>();
  auto future = promise->get_future();

  // [CRITICAL FIX] UAF 방어: raw this 대신 shared_ptr 캡처
  auto self = shared_from_this();
  auto mqtt_self = std::dynamic_pointer_cast<MQTTWorker>(self);

  std::thread([mqtt_self, promise]() {
    try {
      mqtt_self->LogMessage(LogLevel::INFO, "Stopping MQTT worker...");

      // 1. 프로덕션 모드 스레드들 정리
      mqtt_self->StopProductionThreads();

      // 2. 기본 스레드들 정리
      mqtt_self->message_thread_running_ = false;
      mqtt_self->publish_thread_running_ = false;
      mqtt_self->publish_queue_cv_.notify_all();

      if (mqtt_self->message_processor_thread_ &&
          mqtt_self->message_processor_thread_->joinable()) {
        mqtt_self->message_processor_thread_->join();
      }
      if (mqtt_self->publish_processor_thread_ &&
          mqtt_self->publish_processor_thread_->joinable()) {
        mqtt_self->publish_processor_thread_->join();
      }

      // 3. 연결 해제
      mqtt_self->CloseConnection();

      mqtt_self->ChangeState(WorkerState::STOPPED);
      mqtt_self->StopAllThreads();

      mqtt_self->LogMessage(LogLevel::INFO, "MQTT worker stopped successfully");
      promise->set_value(true);

    } catch (const std::exception &e) {
      mqtt_self->LogMessage(LogLevel::LOG_ERROR,
                            "Failed to stop MQTT worker: " +
                                std::string(e.what()));
      promise->set_value(false);
    }
  }).detach();

  return future;
}

bool MQTTWorker::EstablishConnection() {
  if (!mqtt_driver_) {
    LogMessage(LogLevel::LOG_ERROR, "MQTT driver not initialized");
    return false;
  }

  worker_stats_.connection_attempts++;

  if (mqtt_driver_->Connect()) {
    LogMessage(LogLevel::INFO,
               "MQTT connection established to: " + mqtt_config_.broker_url);
    SetConnectionState(true);

    // 재연결 시 기존 구독 리스트 자동 복구
    {
      std::lock_guard<std::mutex> lock(subscriptions_mutex_);
      for (const auto &[id, sub] : active_subscriptions_) {
        LogMessage(LogLevel::INFO,
                   "[MQTT_DEBUG] 🛰️ Subscribing to topic: " + sub.topic);
        if (mqtt_driver_->Subscribe(sub.topic, QosToInt(sub.qos))) {
          LogMessage(LogLevel::INFO,
                     "[MQTT_DEBUG] ✅ Successfully subscribed to: " +
                         sub.topic);
        } else {
          LogMessage(LogLevel::WARN,
                     "[MQTT_DEBUG] ❌ Failed to subscribe to: " + sub.topic);
        }
      }
    }

    // 프로덕션 모드에서는 성능 메트릭스 업데이트
    if (IsProductionMode()) {
      performance_metrics_.connection_count++;
    }

    return true;
  } else {
    std::string error_detail = "Unknown error";
    if (mqtt_driver_) {
      error_detail = mqtt_driver_->GetLastError().message;
    }
    LogMessage(LogLevel::LOG_ERROR,
               "Failed to establish MQTT connection: " + error_detail);
    SetConnectionState(false);

    // 프로덕션 모드에서는 에러 카운트 업데이트
    if (IsProductionMode()) {
      performance_metrics_.error_count++;
      consecutive_failures_++;
    }

    return false;
  }
}

bool MQTTWorker::CloseConnection() {
  if (!mqtt_driver_) {
    return true;
  }

  if (mqtt_driver_->Disconnect()) {
    LogMessage(LogLevel::INFO, "MQTT connection closed");
    return true;
  } else {
    LogMessage(LogLevel::WARN, "Failed to close MQTT connection gracefully");
    return false;
  }
}

bool MQTTWorker::CheckConnection() {
  if (!mqtt_driver_) {
    return false;
  }

  return mqtt_driver_->IsConnected();
}

bool MQTTWorker::SendKeepAlive() {
  // MQTT 자체적으로 Keep-alive를 처리하므로 항상 true 반환
  return CheckConnection();
}

// =============================================================================
// 🔥 파이프라인 연동 메서드들 (ModbusTcpWorker 패턴 완전 적용)
// =============================================================================

bool MQTTWorker::SendMQTTDataToPipeline(
    const std::string &topic, const std::string &payload,
    const PulseOne::Structs::DataPoint *data_point, uint32_t priority) {
  try {
    LogMessage(LogLevel::DEBUG_LEVEL,
               "Processing MQTT message: topic=" + topic +
                   ", size=" + std::to_string(payload.size()));

#ifdef HAS_JSON
    // JSON 파싱 시도
    nlohmann::json json_data;
    try {
      json_data = nlohmann::json::parse(payload);
      return SendJsonValuesToPipeline(json_data, topic, priority);
    } catch (const nlohmann::json::parse_error &e) {
      // JSON이 아닌 경우 문자열로 처리
      LogMessage(LogLevel::DEBUG_LEVEL,
                 "Payload is not JSON, treating as string");
    }
#endif

    // JSON이 아닌 경우 또는 JSON 라이브러리가 없는 경우
    std::vector<PulseOne::Structs::TimestampedValue> values;

    PulseOne::Structs::TimestampedValue tv;
    tv.value = payload; // 문자열로 저장
    tv.timestamp = std::chrono::system_clock::now();
    tv.quality = PulseOne::Enums::DataQuality::GOOD;
    tv.source = "mqtt_" + topic;

    // DataPoint가 있으면 ID 및 Source 설정
    if (data_point) {
      tv.source = data_point->name; // Use semantic name (Hybrid Strategy)
      tv.point_id = std::stoi(data_point->id);

      // 이전값과 비교 (protected 멤버 접근)
      auto prev_it = previous_values_.find(tv.point_id);
      if (prev_it != previous_values_.end()) {
        tv.previous_value = prev_it->second;
        tv.value_changed = (tv.value != prev_it->second);
      } else {
        tv.previous_value = PulseOne::Structs::DataValue{};
        tv.value_changed = true;
      }

      // 이전값 캐시 업데이트
      previous_values_[tv.point_id] = tv.value;

      tv.sequence_number = GetNextSequenceNumber();
    } else if (device_info_.is_auto_registration_enabled) {
      // 🔥 비-JSON 단일 토픽 자동 등록 지원
      std::string inferred_type = "STRING"; // 기본적으로 문자열로 간주
      std::string point_name = topic;
      std::replace(point_name.begin(), point_name.end(), '/', '.');

      uint32_t new_id =
          RegisterNewDataPoint(point_name, topic, "", inferred_type);
      if (new_id > 0) {
        tv.point_id = new_id;
        tv.source = point_name;
        tv.value_changed = true;
        tv.sequence_number = GetNextSequenceNumber();
      } else {
        tv.point_id = std::hash<std::string>{}(topic) % 100000;
        tv.value_changed = true;
        tv.sequence_number = GetNextSequenceNumber();
      }
    } else {
      // DataPoint가 없는 경우 토픽을 기반으로 임시 ID 생성
      tv.point_id = std::hash<std::string>{}(topic) % 100000;
      tv.value_changed = true;
      tv.sequence_number = GetNextSequenceNumber();
    }

    values.push_back(tv);

    // 🔥 BaseDeviceWorker::SendValuesToPipelineWithLogging() 호출
    return SendValuesToPipelineWithLogging(values, "MQTT topic: " + topic,
                                           priority);

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "SendMQTTDataToPipeline 예외: " + std::string(e.what()));
    return false;
  }
}

#ifdef HAS_JSON
bool MQTTWorker::SendJsonValuesToPipeline(const nlohmann::json &json_data,
                                          const std::string &topic_context,
                                          uint32_t priority) {
  try {
    std::vector<PulseOne::Structs::TimestampedValue> values;
    bool has_mapped_points = false;
    std::unordered_set<std::string> handled_keys;

    // 1. 사전 매핑된 포인트(Mapping Key 기반) 처리
    {
      std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);
      for (const auto &point : data_points_) {
        if (point.address_string == topic_context &&
            !point.mapping_key.empty()) {
          try {
            std::string path = point.mapping_key;
            // 단순 점 표기법 및 대괄호를 JSON Pointer로 변환 (예:
            // "sensors[0].temp" -> "/sensors/0/temp")
            bool is_json_pointer = (!path.empty() && path.front() == '/');
            if (!is_json_pointer) {
              std::replace(path.begin(), path.end(), '.', '/');
              std::replace(path.begin(), path.end(), '[', '/');
              path.erase(std::remove(path.begin(), path.end(), ']'),
                         path.end());
              if (path.empty() || path.front() != '/') {
                path = "/" + path;
              }
            }

            nlohmann::json::json_pointer ptr(path);
            if (json_data.contains(ptr)) {
              const auto &val = json_data.at(ptr);
              has_mapped_points = true;
              handled_keys.insert(point.mapping_key); // 처리된 키 기록

              PulseOne::Structs::TimestampedValue tv;
              tv.timestamp = std::chrono::system_clock::now();
              tv.quality = PulseOne::Enums::DataQuality::GOOD;
              tv.source = point.name;            // Use semantic name
              tv.point_id = std::stoi(point.id); // DataPoint ID 사용

              // 값 변환 로직
              if (val.is_number_integer()) {
                tv.value = val.get<int64_t>();
              } else if (val.is_number_float()) {
                tv.value = val.get<double>();
              } else if (val.is_boolean()) {
                tv.value = val.get<bool>();
              } else if (val.is_string()) {
                tv.value = val.get<std::string>();
              } else {
                tv.value = val.dump();
              }

              // 이전값 확인 및 변화 감지
              auto prev_it = previous_values_.find(tv.point_id);
              if (prev_it != previous_values_.end()) {
                tv.previous_value = prev_it->second;
                tv.value_changed = (tv.value != prev_it->second);
              } else {
                tv.value_changed = true;
              }

              previous_values_[tv.point_id] = tv.value;
              tv.sequence_number = GetNextSequenceNumber();

              // 🔥 파일 레퍼런스 추출
              if (json_data.contains("file_ref")) {
                tv.metadata["file_ref"] =
                    json_data["file_ref"].get<std::string>();
              }

              values.push_back(tv);
            }
          } catch (const std::exception &e) {
            LogMessage(LogLevel::WARN, "JSON Mapping error for point " +
                                           point.name + ": " + e.what());
          }
        }
      }
    }

    // 2. 매핑되지 않은 키들에 대해 자동 탐색(Auto-Discovery) 수행
    auto timestamp = std::chrono::system_clock::now();
    if (json_data.is_object()) {
      for (auto &[key, value] : json_data.items()) {
        // 이미 매핑 loop에서 처리된 키는 스킵
        if (handled_keys.count(key))
          continue;

        // [Auto-Registration] DB에 등록된 포인트인지 확인
        uint32_t point_id = 0;
        {
          std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);
          for (const auto &dp : data_points_) {
            if (dp.address_string == topic_context && dp.mapping_key == key) {
              try {
                point_id = std::stoul(dp.id);
              } catch (...) {
              }
              break;
            }
          }
        }

        // 등록되지 않은 경우 자동 등록 실행 (옵션 활성화 시)
        if (point_id == 0 && device_info_.is_auto_registration_enabled) {
          std::string inferred_type = "FLOAT32";
          if (value.is_string())
            inferred_type = "STRING";
          else if (value.is_boolean())
            inferred_type = "BOOL";
          else if (key.find("timestamp") != std::string::npos ||
                   key.find("time") != std::string::npos) {
            inferred_type = "DATETIME";
          }

          std::string point_name = topic_context + "." + key;
          std::replace(point_name.begin(), point_name.end(), '/', '.');

          LogMessage(LogLevel::INFO, "[Auto-Discovery] New key detected: " +
                                         key + " in topic " + topic_context);
          point_id = RegisterNewDataPoint(point_name, topic_context, key,
                                          inferred_type);
        }

        if (point_id == 0)
          continue;

        // 이미 handled_keys에 있는 것은 아니지만, point_id가 매핑 loop에서
        // 처리되었을 수 있음 (드문 경우)
        bool already_sent = false;
        for (const auto &v : values) {
          if (v.point_id == point_id) {
            already_sent = true;
            break;
          }
        }
        if (already_sent)
          continue;

        PulseOne::Structs::TimestampedValue tv;
        if (value.is_number_integer()) {
          int64_t int64_val = value.get<int64_t>();
          if (int64_val >= INT_MIN && int64_val <= INT_MAX) {
            tv.value = static_cast<int>(int64_val);
          } else {
            tv.value = static_cast<double>(int64_val);
          }
        } else if (value.is_number_float()) {
          tv.value = value.get<double>();
        } else if (value.is_boolean()) {
          tv.value = value.get<bool>();
        } else if (value.is_string()) {
          tv.value = value.get<std::string>();
        } else {
          tv.value = value.dump();
        }

        tv.timestamp = timestamp;
        tv.quality = PulseOne::Enums::DataQuality::GOOD;
        tv.source = "mqtt_auto_" + topic_context + "_" + key;
        tv.point_id = point_id;

        auto prev_it = previous_values_.find(tv.point_id);
        if (prev_it != previous_values_.end()) {
          tv.previous_value = prev_it->second;
          tv.value_changed = (tv.value != prev_it->second);
        } else {
          tv.previous_value = PulseOne::Structs::DataValue{};
          tv.value_changed = true;
        }

        previous_values_[tv.point_id] = tv.value;
        tv.sequence_number = GetNextSequenceNumber();
        values.push_back(tv);
      }
    } else {
      // 단일 값 처리 (JSON이 객체가 아닌 경우)
      // 이 부분은 기존 로직을 유지하며, handled_keys는 객체 키에만 해당하므로
      // 영향을 주지 않음.
      PulseOne::Structs::TimestampedValue tv;
      uint32_t point_id = 0;
      {
        std::lock_guard<std::recursive_mutex> lock(data_points_mutex_);
        for (const auto &dp : data_points_) {
          if (dp.address_string == topic_context && dp.mapping_key.empty()) {
            try {
              point_id = std::stoul(dp.id);
            } catch (...) {
            }
            break;
          }
        }
      }

      if (point_id == 0 && device_info_.is_auto_registration_enabled) {
        std::string inferred_type = "FLOAT32";
        if (json_data.is_string())
          inferred_type = "STRING";
        else if (json_data.is_boolean())
          inferred_type = "BOOL";
        else if (topic_context.find("timestamp") != std::string::npos ||
                 topic_context.find("time") != std::string::npos) {
          inferred_type = "UINT64";
        }

        std::string point_name = topic_context;
        std::replace(point_name.begin(), point_name.end(), '/', '.');

        point_id =
            RegisterNewDataPoint(point_name, topic_context, "", inferred_type);
      }

      if (point_id == 0) {
        // 자동 등록 비활성화 상태이거나 등록 실패 시 스킵
        return false;
      }

      if (json_data.is_number_integer()) {
        int64_t int64_val = json_data.get<int64_t>();
        tv.value = static_cast<double>(int64_val);
      } else if (json_data.is_number_float()) {
        tv.value = json_data.get<double>();
      } else if (json_data.is_boolean()) {
        tv.value = json_data.get<bool>();
      } else if (json_data.is_string()) {
        tv.value = json_data.get<std::string>();
      } else {
        tv.value = json_data.dump();
      }

      tv.timestamp = timestamp;
      tv.quality = PulseOne::Enums::DataQuality::GOOD;
      tv.source = "mqtt_auto_" + topic_context;
      tv.point_id = point_id;
      tv.value_changed = true;
      tv.sequence_number = GetNextSequenceNumber();
      values.push_back(tv);
    }

    if (values.empty()) {
      LogMessage(LogLevel::DEBUG_LEVEL, "No values extracted from JSON");
      return true;
    }

    return SendValuesToPipelineWithLogging(
        values,
        "MQTT JSON: " + topic_context + " (" + std::to_string(values.size()) +
            " fields)",
        priority);

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "SendJsonValuesToPipeline 예외: " + std::string(e.what()));
    return false;
  }
}
#else
bool MQTTWorker::SendJsonValuesToPipeline(const std::string &raw_json,
                                          const std::string &topic_context,
                                          uint32_t priority) {
  (void)raw_json;
  (void)topic_context;
  (void)priority;
  LogMessage(LogLevel::WARN,
             "JSON support not available (HAS_JSON not defined)");
  return false;
}
#endif

bool MQTTWorker::SendSingleTopicValueToPipeline(
    const std::string &topic, const PulseOne::Structs::DataValue &value,
    uint32_t priority) {
  try {
    PulseOne::Structs::TimestampedValue tv;
    tv.value = value;
    tv.timestamp = std::chrono::system_clock::now();
    tv.quality = PulseOne::Enums::DataQuality::GOOD;
    tv.source = "mqtt_single_" + topic;

    // 🔥 개선: DataPoint 연결 시도
    const PulseOne::Structs::DataPoint *data_point =
        FindDataPointByTopic(topic);
    if (data_point) {
      tv.source = data_point->name; // Use semantic name (Hybrid Strategy)
      tv.point_id = std::stoi(data_point->id);

      // 🔥 이전값과 비교 (protected 멤버 접근)
      auto prev_it = previous_values_.find(tv.point_id);
      if (prev_it != previous_values_.end()) {
        tv.previous_value = prev_it->second;
        tv.value_changed = (tv.value != prev_it->second);
      } else {
        tv.previous_value = PulseOne::Structs::DataValue{};
        tv.value_changed = true;
      }

      // 🔥 이전값 캐시 업데이트
      previous_values_[tv.point_id] = tv.value;
      tv.sequence_number = GetNextSequenceNumber();

      // DataPoint 설정 적용
      tv.scaling_factor = data_point->scaling_factor;
      tv.scaling_offset = data_point->scaling_offset;
      tv.change_threshold = data_point->log_deadband;
      tv.force_rdb_store = tv.value_changed;

    } else if (device_info_.is_auto_registration_enabled) {
      // 🔥 파일 포인트 등 개별 토픽 자동 등록 지원 (vfd/file 등)
      std::string inferred_type = "FLOAT32";
      std::visit(
          [&inferred_type](const auto &v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>)
              inferred_type = "STRING";
            else if constexpr (std::is_same_v<T, bool>)
              inferred_type = "BOOL";
          },
          value);

      // 타임스탬프 감지
      if (topic.find("timestamp") != std::string::npos ||
          topic.find("time") != std::string::npos) {
        inferred_type = "UINT64";
      }

      std::string point_name = topic;
      std::replace(point_name.begin(), point_name.end(), '/', '.');

      uint32_t new_id =
          RegisterNewDataPoint(point_name, topic, "", inferred_type);
      if (new_id > 0) {
        tv.point_id = new_id;
        tv.source = point_name;
        tv.value_changed = true;
        tv.sequence_number = GetNextSequenceNumber();
      } else {
        tv.point_id = std::hash<std::string>{}(topic) % 100000;
        tv.value_changed = true;
        tv.sequence_number = GetNextSequenceNumber();
      }
      tv.scaling_factor = 1.0;
      tv.scaling_offset = 0.0;
      tv.change_threshold = 0.0;
      tv.force_rdb_store = true;
    } else {
      // DataPoint가 없는 경우 토픽 기반 임시 ID
      tv.point_id = std::hash<std::string>{}(topic) % 100000;
      tv.value_changed = true;
      tv.sequence_number = GetNextSequenceNumber();
      tv.scaling_factor = 1.0;
      tv.scaling_offset = 0.0;
      tv.change_threshold = 0.0;
      tv.force_rdb_store = true;
    }

    // 🔥 BaseDeviceWorker::SendValuesToPipelineWithLogging() 호출
    return SendValuesToPipelineWithLogging({tv}, "Single MQTT value: " + topic,
                                           priority);

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "SendSingleTopicValueToPipeline 예외: " + std::string(e.what()));
    return false;
  }
}

bool MQTTWorker::SendMultipleTopicValuesToPipeline(
    const std::map<std::string, PulseOne::Structs::DataValue> &topic_values,
    const std::string &batch_context, uint32_t priority) {
  if (topic_values.empty()) {
    return false;
  }

  try {
    std::vector<PulseOne::Structs::TimestampedValue> timestamped_values;
    timestamped_values.reserve(topic_values.size());

    auto timestamp = std::chrono::system_clock::now();

    for (const auto &[topic, value] : topic_values) {
      PulseOne::Structs::TimestampedValue tv;

      tv.value = value;
      tv.timestamp = timestamp;
      tv.quality = PulseOne::Enums::DataQuality::GOOD;
      tv.source = "mqtt_batch_" + topic;

      // DataPoint 연결 시도
      const PulseOne::Structs::DataPoint *data_point =
          FindDataPointByTopic(topic);
      if (data_point) {
        tv.source = data_point->name; // Use semantic name (Hybrid Strategy)
        tv.point_id = std::stoi(data_point->id);

        // 이전값과 비교
        auto prev_it = previous_values_.find(tv.point_id);
        if (prev_it != previous_values_.end()) {
          tv.previous_value = prev_it->second;
          tv.value_changed = (tv.value != prev_it->second);
        } else {
          tv.previous_value = PulseOne::Structs::DataValue{};
          tv.value_changed = true;
        }

        // 이전값 캐시 업데이트
        previous_values_[tv.point_id] = tv.value;

        // DataPoint 설정 적용
        tv.scaling_factor = data_point->scaling_factor;
        tv.scaling_offset = data_point->scaling_offset;
        tv.change_threshold = data_point->log_deadband;
        tv.force_rdb_store = tv.value_changed;

      } else {
        // DataPoint가 없는 경우
        tv.point_id = std::hash<std::string>{}(topic) % 100000;
        tv.value_changed = true;
        tv.scaling_factor = 1.0;
        tv.scaling_offset = 0.0;
        tv.change_threshold = 0.0;
        tv.force_rdb_store = true;
      }

      tv.sequence_number = GetNextSequenceNumber();
      timestamped_values.push_back(tv);
    }

    // 일괄 전송
    return SendValuesToPipelineWithLogging(
        timestamped_values,
        "MQTT Batch: " + batch_context + " (" +
            std::to_string(topic_values.size()) + " topics)",
        priority);

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR, "SendMultipleTopicValuesToPipeline 예외: " +
                                        std::string(e.what()));
    return false;
  }
}

// =============================================================================
// 기본 MQTT 기능 (모든 모드에서 사용 가능)
// =============================================================================

bool MQTTWorker::AddSubscription(const MQTTSubscription &subscription) {
  std::lock_guard<std::mutex> lock(subscriptions_mutex_);

  // 구독 유효성 검사
  if (!ValidateSubscription(subscription)) {
    LogMessage(LogLevel::LOG_ERROR,
               "Invalid subscription: " + subscription.topic);
    return false;
  }

  // 고유 ID 할당
  MQTTSubscription new_subscription = subscription;
  new_subscription.subscription_id = next_subscription_id_++;
  new_subscription.is_active = true;

  // 실제 MQTT 구독 (Driver 위임)
  if (mqtt_driver_ && mqtt_driver_->IsConnected()) {
    bool success = mqtt_driver_->Subscribe(new_subscription.topic,
                                           QosToInt(new_subscription.qos));
    // bool success = true; // 현재는 임시로 true (실제 구현에서는 MqttDriver
    // API 사용)

    if (!success) {
      LogMessage(LogLevel::LOG_ERROR,
                 "Failed to subscribe to topic: " + new_subscription.topic);
      return false;
    }
  }

  // 구독 정보 저장
  active_subscriptions_[new_subscription.subscription_id] = new_subscription;
  worker_stats_.successful_subscriptions++;

  LogMessage(LogLevel::INFO,
             "Added subscription (ID: " +
                 std::to_string(new_subscription.subscription_id) +
                 ", Topic: " + new_subscription.topic + ")");

  return true;
}

bool MQTTWorker::RemoveSubscription(uint32_t subscription_id) {
  std::lock_guard<std::mutex> lock(subscriptions_mutex_);

  auto it = active_subscriptions_.find(subscription_id);
  if (it == active_subscriptions_.end()) {
    LogMessage(LogLevel::WARN,
               "Subscription not found: " + std::to_string(subscription_id));
    return false;
  }

  // 실제 MQTT 구독 해제 (Driver 위임)
  if (mqtt_driver_ && mqtt_driver_->IsConnected()) {
    // bool success = mqtt_driver_->Unsubscribe(it->second.topic);
    bool success = true; // 현재는 임시로 true

    if (!success) {
      LogMessage(LogLevel::WARN,
                 "Failed to unsubscribe from topic: " + it->second.topic);
    }
  }

  LogMessage(LogLevel::INFO, "Removed subscription: " + it->second.topic);
  active_subscriptions_.erase(it);

  return true;
}

bool MQTTWorker::PublishMessage(const MQTTPublishTask &task) {
  std::lock_guard<std::mutex> lock(publish_queue_mutex_);

  if (publish_queue_.size() >= max_publish_queue_size_) {
    LogMessage(LogLevel::WARN, "Publish queue full, dropping message");
    worker_stats_.failed_operations++;

    // 프로덕션 모드에서는 드롭된 메시지 카운트
    if (IsProductionMode()) {
      performance_metrics_.messages_dropped++;
    }

    return false;
  }

  publish_queue_.push(task);
  publish_queue_cv_.notify_one();

  return true;
}

bool MQTTWorker::PublishMessage(const std::string &topic,
                                const std::string &payload, int qos,
                                bool retained) {
  MQTTPublishTask task;
  task.topic = topic;
  task.payload = payload;
  task.qos = IntToQos(qos); // int를 MqttQoS enum으로 변환
  task.retained = retained;
  task.scheduled_time = system_clock::now();
  task.retry_count = 0;
  task.priority = 5; // 기본 우선순위

  return PublishMessage(task); // 구조체 버전 호출
}

std::string MQTTWorker::GetMQTTWorkerStats() const {
  std::ostringstream stats;

  auto now = system_clock::now();
  auto uptime = duration_cast<seconds>(now - worker_stats_.start_time).count();

  stats << "{"
        << "\"messages_received\":" << worker_stats_.messages_received.load()
        << ","
        << "\"messages_published\":" << worker_stats_.messages_published.load()
        << ","
        << "\"successful_subscriptions\":"
        << worker_stats_.successful_subscriptions.load() << ","
        << "\"failed_operations\":" << worker_stats_.failed_operations.load()
        << ","
        << "\"json_parse_errors\":" << worker_stats_.json_parse_errors.load()
        << ","
        << "\"connection_attempts\":"
        << worker_stats_.connection_attempts.load() << ","
        << "\"uptime_seconds\":" << uptime << ","
        << "\"active_subscriptions\":" << active_subscriptions_.size() << ","
        << "\"worker_mode\":\"" << (IsProductionMode() ? "PRODUCTION" : "BASIC")
        << "\""
        << "}";

  return stats.str();
}

void MQTTWorker::ResetMQTTWorkerStats() {
  worker_stats_.messages_received = 0;
  worker_stats_.messages_published = 0;
  worker_stats_.successful_subscriptions = 0;
  worker_stats_.failed_operations = 0;
  worker_stats_.json_parse_errors = 0;
  worker_stats_.connection_attempts = 0;
  worker_stats_.last_reset = system_clock::now();

  // 프로덕션 모드에서는 성능 메트릭스도 리셋
  if (IsProductionMode()) {
    ResetMetrics();
  }

  LogMessage(LogLevel::INFO, "MQTT worker statistics reset");
}

// =============================================================================
// 모드 제어 및 설정
// =============================================================================

void MQTTWorker::SetWorkerMode(MQTTWorkerMode mode) {
  if (worker_mode_ == mode) {
    return; // 이미 같은 모드
  }

  MQTTWorkerMode old_mode = worker_mode_;
  worker_mode_ = mode;

  LogMessage(LogLevel::INFO,
             "Worker mode changed: " +
                 std::string(old_mode == MQTTWorkerMode::PRODUCTION
                                 ? "PRODUCTION"
                                 : "BASIC") +
                 " → " +
                 std::string(mode == MQTTWorkerMode::PRODUCTION ? "PRODUCTION"
                                                                : "BASIC"));

  // 모드 전환에 따른 스레드 관리
  if (mode == MQTTWorkerMode::PRODUCTION && old_mode == MQTTWorkerMode::BASIC) {
    // BASIC → PRODUCTION: 고급 스레드들 시작
    StartProductionThreads();
  } else if (mode == MQTTWorkerMode::BASIC &&
             old_mode == MQTTWorkerMode::PRODUCTION) {
    // PRODUCTION → BASIC: 고급 스레드들 정지
    StopProductionThreads();
  }
}

// =============================================================================
// 프로덕션 전용 기능 (PRODUCTION 모드에서만 활성화)
// =============================================================================

bool MQTTWorker::PublishWithPriority(const std::string &topic,
                                     const std::string &payload, int priority,
                                     MqttQoS qos) {
  if (!IsProductionMode()) {
    // 기본 모드에서는 일반 발행으로 처리
    return PublishMessage(topic, payload, QosToInt(qos), false);
  }

  try {
    // 오프라인 메시지 생성
    OfflineMessage message(topic, payload, qos, false, priority);
    message.timestamp = system_clock::now();

    // 연결 확인
    if (!CheckConnection()) {
      // 오프라인 큐에 저장
      SaveOfflineMessage(message);
      LogMessage(
          LogLevel::WARN,
          "Connection not available, message queued for offline processing");
      return false;
    }

    // 실제 메시지 발행
    int qos_int = QosToInt(qos);
    bool success = PublishMessage(topic, payload, qos_int, false);

    if (success) {
      performance_metrics_.messages_sent++;
      performance_metrics_.bytes_sent += payload.size();
    } else {
      performance_metrics_.error_count++;
      SaveOfflineMessage(message);
    }

    return success;

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "Exception in PublishWithPriority: " + std::string(e.what()));
    performance_metrics_.error_count++;
    return false;
  }
}

size_t MQTTWorker::PublishBatch(const std::vector<OfflineMessage> &messages) {
  if (!IsProductionMode()) {
    LogMessage(LogLevel::WARN,
               "PublishBatch is only available in PRODUCTION mode");
    return 0;
  }

  size_t successful = 0;

  for (const auto &msg : messages) {
    bool success =
        PublishWithPriority(msg.topic, msg.payload, msg.priority, msg.qos);
    if (success) {
      successful++;
    }
  }

  LogMessage(LogLevel::INFO,
             "Batch publish completed: " + std::to_string(successful) + "/" +
                 std::to_string(messages.size()) + " messages sent");

  return successful;
}

bool MQTTWorker::PublishIfQueueAvailable(const std::string &topic,
                                         const std::string &payload,
                                         size_t max_queue_size) {
  if (!IsProductionMode()) {
    return PublishMessage(topic, payload, 1, false);
  }

  // 큐 크기 확인
  {
    std::lock_guard<std::mutex> lock(offline_messages_mutex_);
    if (offline_messages_.size() >= max_queue_size) {
      performance_metrics_.messages_dropped++;
      LogMessage(LogLevel::WARN, "Message dropped due to queue overflow");
      return false;
    }
  }

  return PublishWithPriority(topic, payload, 5, MqttQoS::AT_LEAST_ONCE);
}

PerformanceMetrics MQTTWorker::GetPerformanceMetrics() const {
  if (!IsProductionMode()) {
    LogMessage(LogLevel::WARN,
               "Performance metrics are only available in PRODUCTION mode");
    return PerformanceMetrics{}; // 빈 메트릭스 반환
  }

  // std::atomic은 복사 불가능하므로 수동으로 값들을 복사
  PerformanceMetrics metrics;
  metrics.messages_sent = performance_metrics_.messages_sent.load();
  metrics.messages_received = performance_metrics_.messages_received.load();
  metrics.messages_dropped = performance_metrics_.messages_dropped.load();
  metrics.bytes_sent = performance_metrics_.bytes_sent.load();
  metrics.bytes_received = performance_metrics_.bytes_received.load();
  metrics.peak_throughput_bps = performance_metrics_.peak_throughput_bps.load();
  metrics.avg_throughput_bps = performance_metrics_.avg_throughput_bps.load();
  metrics.connection_count = performance_metrics_.connection_count.load();
  metrics.error_count = performance_metrics_.error_count.load();
  metrics.retry_count = performance_metrics_.retry_count.load();
  metrics.avg_latency_ms = performance_metrics_.avg_latency_ms.load();
  metrics.max_latency_ms = performance_metrics_.max_latency_ms.load();
  metrics.min_latency_ms = performance_metrics_.min_latency_ms.load();
  metrics.queue_size = performance_metrics_.queue_size.load();
  metrics.max_queue_size = performance_metrics_.max_queue_size.load();

  return metrics;
}

std::string MQTTWorker::GetPerformanceMetricsJson() const {
  if (!IsProductionMode()) {
    return "{\"error\":\"Performance metrics only available in PRODUCTION "
           "mode\"}";
  }

#ifdef HAS_JSON
  json metrics = {
      {"messages_sent", performance_metrics_.messages_sent.load()},
      {"messages_received", performance_metrics_.messages_received.load()},
      {"messages_dropped", performance_metrics_.messages_dropped.load()},
      {"bytes_sent", performance_metrics_.bytes_sent.load()},
      {"bytes_received", performance_metrics_.bytes_received.load()},
      {"peak_throughput_bps", performance_metrics_.peak_throughput_bps.load()},
      {"avg_throughput_bps", performance_metrics_.avg_throughput_bps.load()},
      {"connection_count", performance_metrics_.connection_count.load()},
      {"error_count", performance_metrics_.error_count.load()},
      {"retry_count", performance_metrics_.retry_count.load()},
      {"avg_latency_ms", performance_metrics_.avg_latency_ms.load()},
      {"max_latency_ms", performance_metrics_.max_latency_ms.load()},
      {"min_latency_ms", performance_metrics_.min_latency_ms.load()},
      {"queue_size", performance_metrics_.queue_size.load()},
      {"max_queue_size", performance_metrics_.max_queue_size.load()}};

  return metrics.dump(2);
#else
  return "{\"error\":\"JSON support not available\"}";
#endif
}

std::string MQTTWorker::GetRealtimeDashboardData() const {
  if (!IsProductionMode()) {
    return "{\"error\":\"Realtime dashboard only available in PRODUCTION "
           "mode\"}";
  }

#ifdef HAS_JSON
  json dashboard;
  // const 메서드에서 비const 메서드 호출 방지 - const_cast 사용
  dashboard["status"] = const_cast<MQTTWorker *>(this)->CheckConnection()
                            ? "connected"
                            : "disconnected";
  dashboard["broker_url"] = mqtt_config_.broker_url;
  dashboard["connection_healthy"] = IsConnectionHealthy();
  dashboard["system_load"] = GetSystemLoad();
  dashboard["active_subscriptions"] = active_subscriptions_.size();
  dashboard["queue_size"] = performance_metrics_.queue_size.load();

  return dashboard.dump(2);
#else
  return "{\"error\":\"JSON support not available\"}";
#endif
}

std::string MQTTWorker::GetDetailedDiagnostics() const {
  if (!IsProductionMode()) {
    return "{\"error\":\"Detailed diagnostics only available in PRODUCTION "
           "mode\"}";
  }

#ifdef HAS_JSON
  auto now = steady_clock::now();
  auto uptime = duration_cast<seconds>(now - start_time_);

  json diagnostics;
  diagnostics["system_info"]["uptime_seconds"] = uptime.count();
  diagnostics["system_info"]["primary_broker"] = mqtt_config_.broker_url;
  diagnostics["system_info"]["circuit_breaker_open"] = circuit_open_.load();
  diagnostics["system_info"]["consecutive_failures"] =
      consecutive_failures_.load();
  diagnostics["system_info"]["worker_mode"] = "PRODUCTION";

  return diagnostics.dump(2);
#else
  return "{\"error\":\"JSON support not available\"}";
#endif
}

bool MQTTWorker::IsConnectionHealthy() const {
  // const 메서드에서 비const 메서드 호출 방지
  if (!const_cast<MQTTWorker *>(this)->CheckConnection()) {
    return false;
  }

  if (!IsProductionMode()) {
    return true; // 기본 모드에서는 연결만 확인
  }

  // 프로덕션 모드에서는 추가 건강 상태 확인
  auto now = steady_clock::now();
  auto uptime = duration_cast<seconds>(now - start_time_);

  // 최소 10초 운영 및 최근 활동 확인
  bool uptime_healthy = uptime.count() > 10;
  bool error_rate_healthy = consecutive_failures_.load() < 5;

  return uptime_healthy && error_rate_healthy;
}

double MQTTWorker::GetSystemLoad() const {
  if (!IsProductionMode()) {
    return 0.0;
  }

  // 큐 크기 기반 시스템 로드 계산
  std::lock_guard<std::mutex> lock(offline_messages_mutex_);
  size_t queue_size = offline_messages_.size();
  size_t max_size = max_queue_size_.load();

  if (max_size == 0)
    return 0.0;

  return static_cast<double>(queue_size) / static_cast<double>(max_size);
}

// =============================================================================
// 프로덕션 모드 설정 및 제어
// =============================================================================

void MQTTWorker::SetMetricsCollectionInterval(int interval_seconds) {
  metrics_collection_interval_ = interval_seconds;
  LogMessage(LogLevel::INFO, "Metrics collection interval set to " +
                                 std::to_string(interval_seconds) + " seconds");
}

void MQTTWorker::SetMaxQueueSize(size_t max_size) {
  max_queue_size_ = max_size;
  LogMessage(LogLevel::INFO,
             "Max queue size set to " + std::to_string(max_size));
}

void MQTTWorker::ResetMetrics() {
  if (IsProductionMode()) {
    performance_metrics_.Reset();
    LogMessage(LogLevel::INFO, "Performance metrics reset");
  }
}

void MQTTWorker::SetBackpressureThreshold(double threshold) {
  backpressure_threshold_ = threshold;
  LogMessage(LogLevel::INFO,
             "Backpressure threshold set to " + std::to_string(threshold));
}

void MQTTWorker::ConfigureAdvanced(const AdvancedMqttConfig &config) {
  advanced_config_ = config;
  LogMessage(LogLevel::INFO, "Advanced MQTT configuration updated");
}

void MQTTWorker::EnableAutoFailover(
    const std::vector<std::string> &backup_brokers, int max_failures) {
  backup_brokers_ = backup_brokers;
  advanced_config_.circuit_breaker_enabled = true;
  advanced_config_.max_failures = max_failures;

  LogMessage(LogLevel::INFO, "Auto failover enabled with " +
                                 std::to_string(backup_brokers.size()) +
                                 " backup brokers");
}

// =============================================================================
// 내부 메서드 (ModbusTcpWorker 패턴 완전 적용)
// =============================================================================

/**
 * @brief ParseMQTTConfig() - ModbusTcpWorker의 ParseModbusConfig()와 동일한
 * 5단계 패턴
 * @details 문서 가이드라인에 따른 5단계 파싱 프로세스
 */
bool MQTTWorker::ParseMQTTConfig() {
  try {
    LogMessage(LogLevel::INFO, "🔧 Starting MQTT configuration parsing...");

    // 🔥 기존 mqtt_config_ 구조 사용 (properties 없음)
    // mqtt_config_ 구조체는 다음과 같음:
    // - broker_url, client_id, username, password
    // - clean_session, use_ssl, keepalive_interval_sec
    // - connection_timeout_sec, max_retry_count, default_qos

    // 🔥 핵심 수정: config와 connection_string 올바른 파싱
    nlohmann::json protocol_config_json;

    // 1단계: device_info_.config에서 JSON 설정 찾기 (우선순위 1)
    if (!device_info_.config.empty()) {
      try {
        protocol_config_json = nlohmann::json::parse(device_info_.config);
        LogMessage(LogLevel::INFO,
                   "✅ MQTT Protocol config loaded from device.config: " +
                       device_info_.config);
      } catch (const std::exception &e) {
        LogMessage(LogLevel::WARN, "⚠️ Failed to parse device.config JSON: " +
                                       std::string(e.what()));
      }
    }

    // 2단계: connection_string이 JSON인지 확인 (우선순위 2)
    if (protocol_config_json.empty() &&
        !device_info_.connection_string.empty()) {
      // JSON 형태인지 확인 ('{' 로 시작하는지)
      if (device_info_.connection_string.front() == '{') {
        try {
          protocol_config_json =
              nlohmann::json::parse(device_info_.connection_string);
          LogMessage(
              LogLevel::INFO,
              "✅ MQTT Protocol config loaded from connection_string JSON");
        } catch (const std::exception &e) {
          LogMessage(LogLevel::WARN,
                     "⚠️ Failed to parse connection_string JSON: " +
                         std::string(e.what()));
        }
      } else {
        LogMessage(LogLevel::INFO, "📝 connection_string is not JSON format, "
                                   "using endpoint as broker URL");
      }
    }

    // 3단계: MQTT 전용 기본값 설정 (DB에서 설정을 못 가져온 경우만)
    if (protocol_config_json.empty()) {
      protocol_config_json = {{"topic", "pulseone/default"},
                              {"qos", 1},
                              {"keepalive_interval", 60},
                              {"clean_session", true},
                              {"client_id", "pulseone_" + device_info_.name}};
      LogMessage(LogLevel::INFO,
                 "📝 Applied default MQTT protocol configuration");
    }

    // 4단계: 실제 DB 설정값들을 mqtt_config_ 구조체에 직접 저장

    // 브로커 URL 설정
    if (protocol_config_json.contains("broker_url")) {
      mqtt_config_.broker_url =
          protocol_config_json.value("broker_url", "mqtt://localhost:1883");
    } else if (!device_info_.endpoint.empty()) {
      mqtt_config_.broker_url = device_info_.endpoint;
    } else {
      mqtt_config_.broker_url = "mqtt://localhost:1883";
    }

    // 클라이언트 ID 설정
    if (protocol_config_json.contains("client_id")) {
      mqtt_config_.client_id = protocol_config_json.value(
          "client_id", "pulseone_" + device_info_.name);
    } else {
      mqtt_config_.client_id =
          "pulseone_" + device_info_.name + "_" + device_info_.id;
    }

    // 인증 정보 (선택사항)
    if (protocol_config_json.contains("username")) {
      mqtt_config_.username = protocol_config_json.value("username", "");
    }
    if (protocol_config_json.contains("password")) {
      mqtt_config_.password = protocol_config_json.value("password", "");
    }

    // SSL/TLS 설정
    mqtt_config_.use_ssl = protocol_config_json.value("use_ssl", false);

    // QoS 설정
    if (protocol_config_json.contains("qos")) {
      int qos_int = protocol_config_json.value("qos", 1);
      mqtt_config_.default_qos = IntToQos(qos_int);
    }

    // Keep-alive 설정
    mqtt_config_.keepalive_interval_sec =
        protocol_config_json.value("keepalive_interval", 60);

    // 🔥 베이스 토픽 설정 (와일드카드 지원용)
    mqtt_config_.topic = protocol_config_json.value("topic", "");

    // Clean Session 설정
    mqtt_config_.clean_session =
        protocol_config_json.value("clean_session", true);

    // 🔥 DB에서 가져온 timeout 값 적용
    if (protocol_config_json.contains("connection_timeout")) {
      mqtt_config_.connection_timeout_sec =
          protocol_config_json.value("connection_timeout", 30);
      LogMessage(LogLevel::INFO,
                 "✅ Applied connection timeout from DB: " +
                     std::to_string(mqtt_config_.connection_timeout_sec) + "s");
    }

    // 재시도 횟수
    mqtt_config_.max_retry_count =
        protocol_config_json.value("max_retry_count", 3);

    // [New] 재시도 간격 (기본 5초)
    if (protocol_config_json.contains("retry_interval_ms")) {
      mqtt_config_.retry_interval_ms =
          protocol_config_json.value("retry_interval_ms", 5000);
    } else if (device_info_.retry_interval_ms > 0) {
      mqtt_config_.retry_interval_ms = device_info_.retry_interval_ms;
    }

    // [New] 백오프 시간 (기본 60초)
    if (protocol_config_json.contains("backoff_time_ms")) {
      mqtt_config_.backoff_time_ms =
          protocol_config_json.value("backoff_time_ms", 60000);
    } else if (device_info_.max_backoff_time_ms > 0) {
      // DeviceInfo의 max_backoff_time_ms를 백오프 시간으로 사용
      mqtt_config_.backoff_time_ms = device_info_.max_backoff_time_ms;
    }

    // 5단계: Worker 레벨 설정 적용
    if (protocol_config_json.contains("message_timeout_ms")) {
      default_message_timeout_ms_ =
          protocol_config_json.value("message_timeout_ms", 30000);
    }

    if (protocol_config_json.contains("max_publish_queue_size")) {
      max_publish_queue_size_ =
          protocol_config_json.value("max_publish_queue_size", 10000);
    }

    // 🔥 파일 저장 경로 설정 (선택사항)
    if (protocol_config_json.contains("file_storage_path")) {
      mqtt_config_.file_storage_path =
          protocol_config_json.value("file_storage_path", "");
    }

    // 🎉 성공 로그 - 실제 적용된 설정 표시 - 🔥 문자열 연결 수정
    std::string config_summary = "✅ MQTT config parsed successfully:\n";
    config_summary += "   🔌 Protocol settings (from ";
    config_summary +=
        (!device_info_.config.empty() ? "device.config" : "connection_string");
    config_summary += "):\n";
    config_summary += "      - broker_url: " + mqtt_config_.broker_url + "\n";
    config_summary += "      - client_id: " + mqtt_config_.client_id + "\n";
    config_summary += "      - topic: " +
                      protocol_config_json.value("topic", "pulseone/default") +
                      "\n";
    config_summary +=
        "      - qos: " + std::to_string(QosToInt(mqtt_config_.default_qos)) +
        "\n";
    config_summary += "      - keepalive_interval: " +
                      std::to_string(mqtt_config_.keepalive_interval_sec) +
                      "s\n";
    config_summary += "      - clean_session: " +
                      (mqtt_config_.clean_session ? std::string("true")
                                                  : std::string("false")) +
                      "\n";
    config_summary +=
        "      - use_ssl: " +
        (mqtt_config_.use_ssl ? std::string("true") : std::string("false")) +
        "\n";
    config_summary += "   ⚙️  Communication settings (from DeviceSettings):\n";
    config_summary += "      - connection_timeout: " +
                      std::to_string(mqtt_config_.connection_timeout_sec) +
                      "s\n";
    config_summary += "      - message_timeout: " +
                      std::to_string(default_message_timeout_ms_) + "ms\n";
    config_summary +=
        "      - max_retries: " + std::to_string(mqtt_config_.max_retry_count) +
        "\n";
    config_summary +=
        "      - max_queue_size: " + std::to_string(max_publish_queue_size_) +
        "\n";
    config_summary += "      - auto_reconnect: " +
                      (auto_reconnect_enabled_ ? std::string("enabled")
                                               : std::string("disabled"));

    LogMessage(LogLevel::INFO, config_summary);

    return true;
  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "ParseMQTTConfig failed: " + std::string(e.what()));
    return false;
  }
}

bool MQTTWorker::InitializeMQTTDriver() {
  LogMessage(LogLevel::INFO, "Initializing MQTT Driver...");

  // MqttDriver 생성 (Plugin System via Factory)
  mqtt_driver_ =
      PulseOne::Drivers::DriverFactory::GetInstance().CreateDriver("MQTT");

  if (!mqtt_driver_) {
    LogMessage(LogLevel::LOG_ERROR,
               "Failed to create MqttDriver instance via Factory");
    return false;
  }

  LogMessage(LogLevel::DEBUG_LEVEL, "MqttDriver instance created");

  // WorkerFactory에서 완전 매핑된 DriverConfig 사용
  PulseOne::Structs::DriverConfig driver_config = device_info_.driver_config;

  // device_info_.properties의 모든 내용을 driver_config.properties로 복사
  for (const auto &[key, value] : device_info_.properties) {
    driver_config.properties[key] = value;
  }

  // 기본 필드 업데이트 (MQTT 특화)
  driver_config.device_id = device_info_.id;
  driver_config.name = device_info_.name;
  driver_config.endpoint = mqtt_config_.broker_url;
  driver_config.protocol = "MQTT";

  if (mqtt_config_.connection_timeout_sec > 0) {
    driver_config.timeout_ms = mqtt_config_.connection_timeout_sec * 1000;
  }

  if (device_info_.retry_count > 0) {
    driver_config.retry_count = static_cast<uint32_t>(device_info_.retry_count);
  } else {
    driver_config.retry_count = 3;
  }

  if (device_info_.polling_interval_ms > 0) {
    driver_config.polling_interval_ms =
        static_cast<uint32_t>(device_info_.polling_interval_ms);
  } else {
    driver_config.polling_interval_ms = 1000;
  }

  // MQTT 특화 설정 추가
  driver_config.properties["broker_url"] = mqtt_config_.broker_url;
  driver_config.properties["client_id"] = mqtt_config_.client_id;
  if (!mqtt_config_.username.empty())
    driver_config.properties["username"] = mqtt_config_.username;
  if (!mqtt_config_.password.empty())
    driver_config.properties["password"] = mqtt_config_.password;
  driver_config.properties["qos"] =
      std::to_string(QosToInt(mqtt_config_.default_qos));

  // 드라이버 초기화
  if (!mqtt_driver_->Initialize(driver_config)) {
    LogMessage(LogLevel::LOG_ERROR, "Failed to initialize MqttDriver");
    return false;
  }

  LogMessage(LogLevel::INFO, "MqttDriver initialized successfully");

  // 🔥 드라이버 콜백 등록 (이게 빠져서 메시지 처리가 안 됨)
  SetupMQTTDriverCallbacks();

  return true;
}

// =============================================================================
// 스레드 함수들
// =============================================================================

void MQTTWorker::MessageProcessorThreadFunction() {
  LogMessage(LogLevel::INFO, "Message processor thread started");

  while (message_thread_running_) {
    try {
      // 주기적으로 연결 상태 확인 및 재연결 요청 (BaseDeviceWorker 엔진 활용)
      if (!CheckConnection() && auto_reconnect_enabled_) {
        // 이미 RECONNECTING 상태거나 WAITING_RETRY인 경우는 중복 요청 방지
        WorkerState current = GetState();
        if (current != WorkerState::RECONNECTING &&
            current != WorkerState::WAITING_RETRY) {
          LogMessage(LogLevel::WARN,
                     "MQTT connection lost, triggering unified reconnection.");
          HandleConnectionError("MQTT connection lost");
        }
      }

      // 메시지 처리는 MqttDriver의 콜백을 통해 처리됨
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    } catch (const std::exception &e) {
      LogMessage(LogLevel::LOG_ERROR,
                 "Message processor thread error: " + std::string(e.what()));
      worker_stats_.failed_operations++;
    }
  }

  LogMessage(LogLevel::INFO, "Message processor thread stopped");
}

void MQTTWorker::PublishProcessorThreadFunction() {
  LogMessage(LogLevel::INFO, "Publish processor thread started");

  while (publish_thread_running_) {
    try {
      std::unique_lock<std::mutex> lock(publish_queue_mutex_);

      // 발행할 메시지가 있을 때까지 대기
      publish_queue_cv_.wait(lock, [this] {
        return !publish_queue_.empty() || !publish_thread_running_;
      });

      if (!publish_thread_running_) {
        break;
      }

      // 큐에서 작업 가져오기
      MQTTPublishTask task = publish_queue_.front();
      publish_queue_.pop();
      lock.unlock();

      // 실제 메시지 발행 (Driver 위임)
      if (mqtt_driver_ && mqtt_driver_->IsConnected()) {
        // 🔥 실제 Driver 호출로 수정 (IProtocolDriver 인터페이스 활용)
        PulseOne::Structs::DataPoint dp;
        dp.address_string = task.topic;
        dp.protocol_params["qos"] = std::to_string(QosToInt(task.qos));
        dp.protocol_params["retained"] = task.retained ? "true" : "false";

        PulseOne::Structs::DataValue val = task.payload;

        bool success = mqtt_driver_->WriteValue(dp, val);

        if (success) {
          worker_stats_.messages_published++;

          // 프로덕션 모드에서는 성능 메트릭스 업데이트
          if (IsProductionMode()) {
            performance_metrics_.messages_sent++;
            performance_metrics_.bytes_sent += task.payload.size();
          }

          // 🔥 제어 이력 파이프라인 전송 추가
          TimestampedValue control_log;
          control_log.value = task.payload; // JSON 제어 명령
          control_log.timestamp = std::chrono::system_clock::now();
          control_log.quality = DataQuality::GOOD;
          control_log.source = "control_mqtt_" + task.topic;

          // 제어 이력은 높은 우선순위로 파이프라인 전송
          SendValuesToPipelineWithLogging({control_log}, "MQTT 제어 이력", 1);

          LogMessage(LogLevel::DEBUG_LEVEL,
                     "Published message to topic: " + task.topic);
        } else {
          worker_stats_.failed_operations++;

          if (IsProductionMode()) {
            performance_metrics_.error_count++;
          }

          // 🔥 실패한 제어도 이력 기록
          TimestampedValue control_log;
          control_log.value = task.payload;
          control_log.timestamp = std::chrono::system_clock::now();
          control_log.quality = DataQuality::BAD; // 실패
          control_log.source = "control_mqtt_" + task.topic;

          SendValuesToPipelineWithLogging({control_log}, "MQTT 제어 실패 이력",
                                          1);

          LogMessage(LogLevel::LOG_ERROR,
                     "Failed to publish message to topic: " + task.topic);
        }
      }

    } catch (const std::exception &e) {
      LogMessage(LogLevel::LOG_ERROR,
                 "Publish processor thread error: " + std::string(e.what()));
      worker_stats_.failed_operations++;
    }
  }

  LogMessage(LogLevel::INFO, "Publish processor thread stopped");
}

// =============================================================================
// 🔥 ProcessReceivedMessage - 파이프라인 연동 완성 (ModbusTcpWorker 패턴)
// =============================================================================

bool MQTTWorker::ProcessReceivedMessage(const std::string &topic,
                                        const std::string &payload) {
  try {
    worker_stats_.messages_received++;

    // packet_logging: COMMUNICATION 카테고리가 TRACE 이하면 기록
    if (static_cast<int>(LogManager::getInstance().getCategoryLogLevel(
            DriverLogCategory::COMMUNICATION)) <=
        static_cast<int>(LogLevel::TRACE)) {
      LogManager::getInstance().logPacket("MQTT", device_info_.name, topic,
                                          payload);
    }

    // 프로덕션 모드에서는 성능 메트릭스 업데이트
    if (IsProductionMode()) {
      performance_metrics_.messages_received++;
      performance_metrics_.bytes_received += payload.size();
    }

    LogMessage(LogLevel::INFO, "[MQTT_DEBUG] Received topic: " + topic +
                                   " (size: " + std::to_string(payload.size()) +
                                   " bytes)");

    // ✅ 대용량 데이터(Blob) 조각 확인 및 처리
    // Topic: .../blob/{transfer_id}/{total}/{index}
    if (topic.find("/blob/") != std::string::npos) {
      LogMessage(LogLevel::INFO,
                 "[MQTT_DEBUG] 🔍 Detected potential blob topic");
      std::vector<std::string> parts;
      std::stringstream ss(topic);
      std::string segment;
      while (std::getline(ss, segment, '/')) {
        parts.push_back(segment);
      }

      // 최소 4개 세그먼트 필요 (blob, id, total, index)
      if (parts.size() >= 4) {
        size_t blob_pos = 0;
        for (size_t i = 0; i < parts.size(); ++i) {
          if (parts[i] == "blob") {
            blob_pos = i;
            break;
          }
        }

        if (blob_pos + 3 < parts.size()) {
          std::string transfer_id = parts[blob_pos + 1];
          int total = std::stoi(parts[blob_pos + 2]);
          int index = std::stoi(parts[blob_pos + 3]);

          std::vector<uint8_t> chunk(payload.begin(), payload.end());
          LogMessage(LogLevel::INFO, "[MQTT_DEBUG] 📦 Processing chunk: " +
                                         std::to_string(index + 1) + "/" +
                                         std::to_string(total) +
                                         " for transfer: " + transfer_id);
          auto complete_data =
              PulseOne::Workers::Protocol::MQTTTransferManager::GetInstance()
                  .AddChunk(transfer_id, total, index, chunk);

          if (complete_data) {
            LogMessage(LogLevel::INFO,
                       "[MQTT_DEBUG] ✨ All chunks received! Saving blob...");
            // 🔥 Blob 저장 (기본값 ".bin", 경로 Override 가능)
            std::string file_uri =
                PulseOne::Storage::BlobStore::GetInstance().SaveBlob(
                    *complete_data, ".bin", mqtt_config_.file_storage_path);

            if (!file_uri.empty()) {
              // 파일 URI를 포함한 데이터 포인트 생성 및 전송
              std::string base_topic = topic.substr(0, topic.find("/blob/"));
              PulseOne::Structs::DataValue val;
              val = file_uri;

              if (SendSingleTopicValueToPipeline(base_topic + "/file", val,
                                                 1)) {
                LogMessage(LogLevel::INFO,
                           "Sent blob file URI to pipeline: " + file_uri +
                               " (Topic: " + base_topic + ")");
              }
            }
          }
          return true; // 조각 메시지 처리는 여기서 종료
        }
      }
    }

    // 🔥 새로 추가: 연관된 데이터포인트 찾기
    const DataPoint *related_point = FindDataPointByTopic(topic);

    // 🔥 새로 추가: 파이프라인 전송
    bool pipeline_success =
        SendMQTTDataToPipeline(topic, payload, related_point);

    if (pipeline_success) {
      LogMessage(LogLevel::DEBUG_LEVEL,
                 "Successfully sent MQTT data to pipeline");
    } else {
      LogMessage(LogLevel::WARN, "Failed to send MQTT data to pipeline");
    }

    return true;

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "Error processing received message: " + std::string(e.what()));
    worker_stats_.json_parse_errors++;
    return false;
  }
}

// =============================================================================
// 🔥 파이프라인 연동 헬퍼 메서드들 (ModbusTcpWorker 패턴)
// =============================================================================

const DataPoint *MQTTWorker::FindDataPointByTopic(const std::string &topic) {
  // GetDataPoints()는 BaseDeviceWorker에서 제공되는 함수
  const auto &data_points = GetDataPoints();

  for (auto &point : data_points) {
    // MQTT는 토픽 기반이므로 address_string과 비교
    if (point.address_string == topic ||
        point.address_string.find(topic) != std::string::npos ||
        topic.find(point.address_string) != std::string::npos) {
      return &point;
    }
  }

  return nullptr; // 찾지 못함
}

std::optional<DataPoint>
MQTTWorker::FindDataPointById(const std::string &point_id) {
  // GetDataPoints()는 BaseDeviceWorker에서 제공되는 함수
  const auto &data_points = GetDataPoints();

  for (const auto &point : data_points) {
    if (point.id == point_id) {
      return point;
    }
  }

  return std::nullopt; // 찾지 못함
}

// =============================================================================
// 프로덕션 모드 전용 스레드 관리
// =============================================================================

void MQTTWorker::StartProductionThreads() {
  if (!IsProductionMode()) {
    return;
  }

  try {
    // 메트릭스 수집 스레드
    metrics_thread_running_ = true;
    metrics_thread_ =
        std::make_unique<std::thread>(&MQTTWorker::MetricsCollectorLoop, this);

    // 우선순위 큐 처리 스레드
    priority_thread_running_ = true;
    priority_queue_thread_ = std::make_unique<std::thread>(
        &MQTTWorker::PriorityQueueProcessorLoop, this);

    // 알람 모니터링 스레드
    alarm_thread_running_ = true;
    alarm_monitor_thread_ =
        std::make_unique<std::thread>(&MQTTWorker::AlarmMonitorLoop, this);

    LogMessage(LogLevel::INFO, "Production threads started successfully");

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "Failed to start production threads: " + std::string(e.what()));
  }
}

void MQTTWorker::StopProductionThreads() {
  // 스레드 정지 플래그 설정
  metrics_thread_running_ = false;
  priority_thread_running_ = false;
  alarm_thread_running_ = false;

  // 스레드들 대기 및 정리
  if (metrics_thread_ && metrics_thread_->joinable()) {
    metrics_thread_->join();
  }
  if (priority_queue_thread_ && priority_queue_thread_->joinable()) {
    priority_queue_thread_->join();
  }
  if (alarm_monitor_thread_ && alarm_monitor_thread_->joinable()) {
    alarm_monitor_thread_->join();
  }

  LogMessage(LogLevel::INFO, "Production threads stopped");
}

void MQTTWorker::MetricsCollectorLoop() {
  LogMessage(LogLevel::INFO, "Metrics collector thread started");

  while (metrics_thread_running_) {
    try {
      CollectPerformanceMetrics();
      std::this_thread::sleep_for(seconds(metrics_collection_interval_.load()));

    } catch (const std::exception &e) {
      LogMessage(LogLevel::LOG_ERROR,
                 "Exception in metrics collector: " + std::string(e.what()));
      std::this_thread::sleep_for(seconds(10));
    }
  }

  LogMessage(LogLevel::INFO, "Metrics collector thread stopped");
}

void MQTTWorker::PriorityQueueProcessorLoop() {
  LogMessage(LogLevel::INFO, "Priority queue processor thread started");

  while (priority_thread_running_) {
    try {
      std::vector<OfflineMessage> batch;

      {
        std::lock_guard<std::mutex> lock(offline_messages_mutex_);

        // 최대 10개씩 배치 처리
        for (int i = 0; i < 10 && !offline_messages_.empty(); ++i) {
          batch.push_back(offline_messages_.top());
          offline_messages_.pop();
        }
      }

      if (!batch.empty()) {
        for (const auto &message : batch) {
          int qos_int = QosToInt(message.qos);
          bool success = PublishMessage(message.topic, message.payload, qos_int,
                                        message.retain);

          if (success) {
            performance_metrics_.messages_sent++;
            performance_metrics_.bytes_sent += message.payload.size();
          } else {
            performance_metrics_.error_count++;
          }
        }
      }

      std::this_thread::sleep_for(milliseconds(100));

    } catch (const std::exception &e) {
      LogMessage(LogLevel::LOG_ERROR,
                 "Exception in priority queue processor: " +
                     std::string(e.what()));
      std::this_thread::sleep_for(seconds(1));
    }
  }

  LogMessage(LogLevel::INFO, "Priority queue processor thread stopped");
}

void MQTTWorker::AlarmMonitorLoop() {
  LogMessage(LogLevel::INFO, "Alarm monitor thread started");

  while (alarm_thread_running_) {
    try {
      // 연결 상태 모니터링
      if (!IsConnectionHealthy()) {
        LogMessage(LogLevel::WARN, "Connection health check failed");
      }

      // 큐 크기 모니터링
      double load = GetSystemLoad();
      if (load > backpressure_threshold_.load()) {
        LogMessage(LogLevel::WARN,
                   "High system load detected: " + std::to_string(load));
      }

      std::this_thread::sleep_for(seconds(30));

    } catch (const std::exception &e) {
      LogMessage(LogLevel::LOG_ERROR,
                 "Exception in alarm monitor: " + std::string(e.what()));
      std::this_thread::sleep_for(seconds(10));
    }
  }

  LogMessage(LogLevel::INFO, "Alarm monitor thread stopped");
}

void MQTTWorker::CollectPerformanceMetrics() {
  auto now = steady_clock::now();

  // 처리량 메트릭스 업데이트
  UpdateThroughputMetrics();

  // 큐 크기 업데이트
  {
    std::lock_guard<std::mutex> queue_lock(offline_messages_mutex_);
    performance_metrics_.queue_size = offline_messages_.size();

    // 최대 큐 크기 업데이트
    size_t current_size = offline_messages_.size();
    size_t max_size = performance_metrics_.max_queue_size.load();
    if (current_size > max_size) {
      performance_metrics_.max_queue_size = current_size;
    }
  }
}

void MQTTWorker::UpdateThroughputMetrics() {
  auto now = steady_clock::now();
  auto elapsed = duration_cast<seconds>(now - last_throughput_calculation_);

  if (elapsed.count() > 0) {
    uint64_t bytes_sent = performance_metrics_.bytes_sent.load();
    uint64_t current_throughput = bytes_sent / elapsed.count();

    // 피크 처리량 업데이트
    uint64_t peak = performance_metrics_.peak_throughput_bps.load();
    if (current_throughput > peak) {
      performance_metrics_.peak_throughput_bps = current_throughput;
    }

    // 평균 처리량 업데이트 (간단한 이동 평균)
    uint64_t avg = performance_metrics_.avg_throughput_bps.load();
    performance_metrics_.avg_throughput_bps = (avg + current_throughput) / 2;

    last_throughput_calculation_ = now;
  }
}

void MQTTWorker::UpdateLatencyMetrics(uint32_t latency_ms) {
  // 최대/최소 레이턴시 업데이트
  uint32_t current_max = performance_metrics_.max_latency_ms.load();
  if (latency_ms > current_max) {
    performance_metrics_.max_latency_ms = latency_ms;
  }

  uint32_t current_min = performance_metrics_.min_latency_ms.load();
  if (latency_ms < current_min) {
    performance_metrics_.min_latency_ms = latency_ms;
  }

  // 간단한 평균 레이턴시 업데이트
  uint32_t current_avg = performance_metrics_.avg_latency_ms.load();
  performance_metrics_.avg_latency_ms = (current_avg + latency_ms) / 2;
}

// =============================================================================
// 헬퍼 메서드들
// =============================================================================

bool MQTTWorker::ValidateSubscription(const MQTTSubscription &subscription) {
  // 토픽 유효성 검사
  if (subscription.topic.empty()) {
    return false;
  }

  // QoS 범위 검사
  int qos_int = QosToInt(subscription.qos);
  if (qos_int < 0 || qos_int > 2) {
    return false;
  }

  return true;
}

std::string MQTTWorker::SelectBroker() {
  if (backup_brokers_.empty()) {
    return mqtt_config_.broker_url;
  }

  // 현재 브로커가 실패한 경우 다음 브로커로 전환
  if (IsCircuitOpen()) {
    current_broker_index_ =
        (current_broker_index_ + 1) % (backup_brokers_.size() + 1);

    if (current_broker_index_ == 0) {
      return mqtt_config_.broker_url;
    } else {
      return backup_brokers_[current_broker_index_ - 1];
    }
  }

  return mqtt_config_.broker_url;
}

bool MQTTWorker::IsCircuitOpen() const {
  if (!advanced_config_.circuit_breaker_enabled) {
    return false;
  }

  std::lock_guard<std::mutex> lock(circuit_mutex_);

  if (circuit_open_.load()) {
    auto now = steady_clock::now();
    auto elapsed = duration_cast<seconds>(now - circuit_open_time_);

    if (elapsed.count() >= advanced_config_.circuit_timeout_seconds) {
      // 타임아웃이 지나면 서킷을 반개방 상태로 변경
      const_cast<std::atomic<bool> &>(circuit_open_) = false;
      const_cast<std::atomic<int> &>(consecutive_failures_) = 0;
      return false;
    }
    return true;
  }

  return consecutive_failures_.load() >= advanced_config_.max_failures;
}

bool MQTTWorker::IsTopicAllowed(const std::string &topic) const {
  // 토픽 필터링 로직 (필요시 구현)
  return !topic.empty();
}

bool MQTTWorker::HandleBackpressure() {
  double load = GetSystemLoad();
  return load < backpressure_threshold_.load();
}

void MQTTWorker::SaveOfflineMessage(const OfflineMessage &message) {
  if (!advanced_config_.offline_mode_enabled) {
    return;
  }

  std::lock_guard<std::mutex> lock(offline_messages_mutex_);

  if (offline_messages_.size() >= advanced_config_.max_offline_messages) {
    // 큐가 가득 찬 경우 로그만 남기고 메시지 드롭
    LogMessage(LogLevel::WARN, "Offline queue full, dropping message");
    performance_metrics_.messages_dropped++;
    return;
  }

  offline_messages_.push(message);
}

bool MQTTWorker::IsDuplicateMessage(const std::string &message_id) {
  if (message_id.empty()) {
    return false;
  }

  std::lock_guard<std::mutex> lock(message_ids_mutex_);

  if (processed_message_ids_.count(message_id) > 0) {
    return true;
  }

  // 메시지 ID 저장 (크기 제한)
  if (processed_message_ids_.size() >= 10000) {
    processed_message_ids_.clear(); // 간단한 LRU 대신 전체 클리어
  }

  processed_message_ids_.insert(message_id);
  return false;
}

double MQTTWorker::CalculateMessagePriority(const std::string &topic,
                                            const std::string & /* payload */) {
  // 토픽 기반 우선순위 계산
  double priority = 5.0; // 기본 우선순위

  // 토픽 기반 우선순위 조정
  if (topic.find("alarm") != std::string::npos ||
      topic.find("alert") != std::string::npos) {
    priority = 9.0; // 알람 메시지는 높은 우선순위
  } else if (topic.find("status") != std::string::npos) {
    priority = 7.0; // 상태 메시지는 중간 우선순위
  } else if (topic.find("data") != std::string::npos) {
    priority = 3.0; // 데이터 메시지는 낮은 우선순위
  }

  return priority;
}

// 정적 콜백 메서드
void MQTTWorker::MessageCallback(MQTTWorker *worker, const std::string &topic,
                                 const std::string &payload) {
  if (worker) {
    worker->ProcessReceivedMessage(topic, payload);
  }
}

#ifdef HAS_JSON
bool MQTTWorker::ConvertJsonToDataValue(
    const nlohmann::json &json_val, PulseOne::Structs::DataValue &data_value) {
  try {
    // PulseOne의 DataValue는 std::variant이므로 직접 값을 할당
    if (json_val.is_number_integer()) {
      // 🔥 int64_t를 int로 안전하게 변환
      int64_t int64_val = json_val.get<int64_t>();
      if (int64_val >= INT_MIN && int64_val <= INT_MAX) {
        data_value = static_cast<int>(int64_val);
      } else {
        // 범위 초과 시 double로 변환
        data_value = static_cast<double>(int64_val);
      }
    } else if (json_val.is_number_float()) {
      data_value = json_val.get<double>();
    } else if (json_val.is_boolean()) {
      data_value = json_val.get<bool>();
    } else if (json_val.is_string()) {
      data_value = json_val.get<std::string>();
    } else {
      return false;
    }

    return true;

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "JSON conversion error: " + std::string(e.what()));
    return false;
  }
}
#endif

void MQTTWorker::SetupMQTTDriverCallbacks() {
  if (!mqtt_driver_) {
    return;
  }

  LogMessage(LogLevel::DEBUG_LEVEL, "🔗 Setting up MQTT driver callbacks...");

  // 메시지 수신 콜백 설정
  mqtt_driver_->SetMessageCallback(
      [this](const std::string &topic, const std::string &payload) {
        ProcessReceivedMessage(topic, payload);
      });

  LogMessage(LogLevel::DEBUG_LEVEL, "✅ MQTT driver callbacks configured");
}

bool MQTTWorker::WriteDataPoint(const std::string &point_id,
                                const DataValue &value) {
  try {
    LogMessage(LogLevel::INFO, "WriteDataPoint 호출: " + point_id);
    return WriteDataPointValue(point_id, value);
  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "WriteDataPoint 예외: " + std::string(e.what()));
    return false;
  }
}

bool MQTTWorker::WriteAnalogOutput(const std::string &output_id, double value) {
  try {
    LogMessage(LogLevel::INFO, "WriteAnalogOutput 호출: " + output_id + " = " +
                                   std::to_string(value));

    // 먼저 DataPoint로 찾아보기
    DataValue data_value = value;
    auto data_point_opt = FindDataPointById(output_id);
    if (data_point_opt.has_value()) {
      return WriteDataPoint(output_id, data_value);
    }

    // DataPoint가 없으면 직접 토픽으로 발행
    std::string control_topic = BuildControlTopic(output_id, "analog");
    return PublishControlMessage(control_topic, data_value);

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "WriteAnalogOutput 예외: " + std::string(e.what()));
    return false;
  }
}

bool MQTTWorker::WriteDigitalOutput(const std::string &output_id, bool value) {
  try {
    LogMessage(LogLevel::INFO, "WriteDigitalOutput 호출: " + output_id + " = " +
                                   (value ? "ON" : "OFF"));

    // 먼저 DataPoint로 찾아보기
    DataValue data_value = value;
    auto data_point_opt = FindDataPointById(output_id);
    if (data_point_opt.has_value()) {
      return WriteDataPoint(output_id, data_value);
    }

    // DataPoint가 없으면 직접 토픽으로 발행
    std::string control_topic = BuildControlTopic(output_id, "digital");
    return PublishControlMessage(control_topic, data_value);

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "WriteDigitalOutput 예외: " + std::string(e.what()));
    return false;
  }
}

bool MQTTWorker::WriteSetpoint(const std::string &setpoint_id, double value) {
  try {
    LogMessage(LogLevel::INFO, "WriteSetpoint 호출: " + setpoint_id + " = " +
                                   std::to_string(value));

    // 세트포인트는 아날로그 출력으로 처리
    return WriteAnalogOutput(setpoint_id, value);

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "WriteSetpoint 예외: " + std::string(e.what()));
    return false;
  }
}

bool MQTTWorker::ControlDigitalDevice(const std::string &device_id,
                                      bool enable) {
  try {
    LogMessage(LogLevel::INFO, "ControlDigitalDevice 호출: " + device_id +
                                   " = " + (enable ? "ENABLE" : "DISABLE"));

    // WriteDigitalOutput을 통해 실제 제어 수행
    bool success = WriteDigitalOutput(device_id, enable);

    if (success) {
      LogMessage(LogLevel::INFO, "MQTT 디지털 장비 제어 성공: " + device_id +
                                     " " + (enable ? "활성화" : "비활성화"));
    } else {
      LogMessage(LogLevel::LOG_ERROR,
                 "MQTT 디지털 장비 제어 실패: " + device_id);
    }

    return success;
  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "ControlDigitalDevice 예외: " + std::string(e.what()));
    return false;
  }
}

bool MQTTWorker::ControlAnalogDevice(const std::string &device_id,
                                     double value) {
  try {
    LogMessage(LogLevel::INFO, "ControlAnalogDevice 호출: " + device_id +
                                   " = " + std::to_string(value));

    // 일반적인 범위 검증 (0-100%)
    if (value < 0.0 || value > 100.0) {
      LogMessage(LogLevel::WARN,
                 "ControlAnalogDevice: 값이 일반적 범위를 벗어남: " +
                     std::to_string(value) +
                     "% (0-100 권장, 하지만 계속 진행)");
    }

    // WriteAnalogOutput을 통해 실제 제어 수행
    bool success = WriteAnalogOutput(device_id, value);

    if (success) {
      LogMessage(LogLevel::INFO, "MQTT 아날로그 장비 제어 성공: " + device_id +
                                     " = " + std::to_string(value));
    } else {
      LogMessage(LogLevel::LOG_ERROR,
                 "MQTT 아날로그 장비 제어 실패: " + device_id);
    }

    return success;
  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "ControlAnalogDevice 예외: " + std::string(e.what()));
    return false;
  }
}

// =============================================================================
// 헬퍼 메서드들 구현
// =============================================================================

bool MQTTWorker::WriteDataPointValue(const std::string &point_id,
                                     const DataValue &value) {
  auto data_point_opt = FindDataPointById(point_id);
  if (!data_point_opt.has_value()) {
    LogMessage(LogLevel::LOG_ERROR, "DataPoint not found: " + point_id);
    return false;
  }

  const auto &data_point = data_point_opt.value();

  try {
    // DataPoint에서 MQTT 토픽 정보 추출
    std::string topic;
    std::string json_path;
    int qos;

    if (!ParseMQTTTopic(data_point, topic, json_path, qos)) {
      LogMessage(LogLevel::LOG_ERROR,
                 "Invalid MQTT topic for DataPoint: " + point_id);
      return false;
    }

    // 제어용 토픽으로 변환 (일반적으로 /set 또는 /cmd 추가)
    std::string control_topic = topic + "/set";
    if (topic.find("/status") != std::string::npos) {
      control_topic = topic.replace(topic.find("/status"), 7, "/set");
    }

    // MQTT 메시지 발행
    bool success = PublishControlMessage(control_topic, value, qos);

    // 제어 이력 로깅
    LogWriteOperation(control_topic, value, success);

    if (success) {
      LogMessage(LogLevel::INFO, "MQTT DataPoint 쓰기 성공: " + point_id);
    }

    return success;
  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "WriteDataPointValue 예외: " + std::string(e.what()));
    return false;
  }
}

bool MQTTWorker::PublishControlMessage(const std::string &topic,
                                       const DataValue &value, int qos) {
  try {
    // DataValue를 JSON 페이로드로 변환
    std::string payload = CreateJsonPayload(value);

    // MQTT 메시지 발행
    bool success = PublishMessage(topic, payload, qos, false);

    if (success) {
      // 통계 업데이트
      worker_stats_.messages_published++;
      if (IsProductionMode()) {
        performance_metrics_.messages_sent++;
        performance_metrics_.bytes_sent += payload.length();
      }
    }

    return success;
  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "PublishControlMessage 예외: " + std::string(e.what()));
    return false;
  }
}

std::string MQTTWorker::BuildControlTopic(const std::string &device_id,
                                          const std::string &control_type) {
  // device_id가 이미 완전한 토픽인지 확인
  if (device_id.find('/') != std::string::npos) {
    // 이미 토픽 형태이면 /set 추가
    return device_id + "/set";
  }

  // 디바이스 ID만 있으면 표준 토픽 구조 생성
  std::string base_topic = "devices/" + device_id + "/control";

  if (control_type == "digital") {
    return base_topic + "/switch";
  } else if (control_type == "analog") {
    return base_topic + "/value";
  }

  return base_topic;
}

std::string MQTTWorker::CreateJsonPayload(const DataValue &value) {
  try {
#ifdef HAS_JSON
    nlohmann::json payload_json;

    if (std::holds_alternative<bool>(value)) {
      payload_json["value"] = std::get<bool>(value);
      payload_json["type"] = "boolean";
    } else if (std::holds_alternative<int32_t>(value)) {
      payload_json["value"] = std::get<int32_t>(value);
      payload_json["type"] = "integer";
    } else if (std::holds_alternative<double>(value)) {
      payload_json["value"] = std::get<double>(value);
      payload_json["type"] = "number";
    } else if (std::holds_alternative<std::string>(value)) {
      payload_json["value"] = std::get<std::string>(value);
      payload_json["type"] = "string";
    }

    // 메타데이터 추가
    payload_json["timestamp"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    payload_json["source"] = "PulseOne_MQTT_Control";

    return payload_json.dump();
#else
    // JSON 라이브러리가 없으면 간단한 문자열 형태
    std::stringstream ss;
    ss << "{\"value\":";

    if (std::holds_alternative<bool>(value)) {
      ss << (std::get<bool>(value) ? "true" : "false");
    } else if (std::holds_alternative<int32_t>(value)) {
      ss << std::get<int32_t>(value);
    } else if (std::holds_alternative<double>(value)) {
      ss << std::get<double>(value);
    } else if (std::holds_alternative<std::string>(value)) {
      ss << "\"" << std::get<std::string>(value) << "\"";
    }

    ss << ",\"timestamp\":"
       << std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count();
    ss << "}";

    return ss.str();
#endif
  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "CreateJsonPayload 예외: " + std::string(e.what()));
    // 폴백: 단순 문자열
    if (std::holds_alternative<std::string>(value)) {
      return std::get<std::string>(value);
    } else if (std::holds_alternative<bool>(value)) {
      return std::get<bool>(value) ? "true" : "false";
    } else if (std::holds_alternative<int32_t>(value)) {
      return std::to_string(std::get<int32_t>(value));
    } else if (std::holds_alternative<double>(value)) {
      return std::to_string(std::get<double>(value));
    }
    return "{}";
  }
}

void MQTTWorker::LogWriteOperation(const std::string &topic,
                                   const DataValue &value, bool success) {
  try {
    // 제어 이력을 파이프라인으로 전송
    TimestampedValue control_log;
    control_log.value = value;
    control_log.timestamp = std::chrono::system_clock::now();
    control_log.quality = success ? DataQuality::GOOD : DataQuality::BAD;
    control_log.source = "control_mqtt_" + topic;

    // 제어 이력은 높은 우선순위로 기록
    SendValuesToPipelineWithLogging({control_log}, "MQTT 제어 이력", 1);

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "LogWriteOperation 예외: " + std::string(e.what()));
  }
}

bool MQTTWorker::ParseMQTTTopic(const PulseOne::DataPoint &data_point,
                                std::string &topic, std::string &json_path,
                                int &qos) {
  try {
    // DataPoint에서 MQTT 토픽 정보 추출

    // 1. 토픽 추출 (name 필드에서 또는 protocol_params에서)
    if (!data_point.name.empty() &&
        data_point.name.find('/') != std::string::npos) {
      // name 필드가 토픽 형태인 경우
      topic = data_point.name;
    } else if (data_point.protocol_params.count("topic")) {
      // protocol_params에 명시적으로 지정된 경우
      topic = data_point.protocol_params.at("topic");
    } else if (data_point.address > 0) {
      // address 필드를 토픽으로 변환 (fallback)
      topic = "data/" + std::to_string(data_point.address);
    } else {
      // 기본 토픽 생성
      topic = "devices/" + data_point.id + "/status";
    }

    // 2. JSON 경로 추출
    if (data_point.protocol_params.count("json_path")) {
      json_path = data_point.protocol_params.at("json_path");
    } else if (data_point.protocol_params.count("property")) {
      json_path = data_point.protocol_params.at("property");
    } else {
      // 기본값: value 필드 사용
      json_path = "value";
    }

    // 3. QoS 레벨 추출
    if (data_point.protocol_params.count("qos")) {
      qos = std::stoi(data_point.protocol_params.at("qos"));
    } else {
      // 기본값: QoS 1
      qos = QosToInt(mqtt_config_.default_qos);
    }

    // QoS 범위 검증
    if (qos < 0 || qos > 2) {
      LogMessage(LogLevel::WARN, "Invalid QoS " + std::to_string(qos) +
                                     " for DataPoint " + data_point.id +
                                     ", using default QoS 1");
      qos = 1;
    }

    LogMessage(LogLevel::DEBUG_LEVEL, "Parsed MQTT topic for DataPoint " +
                                          data_point.id + ": topic=" + topic +
                                          ", json_path=" + json_path +
                                          ", qos=" + std::to_string(qos));

    return true;

  } catch (const std::exception &e) {
    LogMessage(LogLevel::LOG_ERROR,
               "Failed to parse MQTT topic for DataPoint " + data_point.id +
                   ": " + std::string(e.what()));
    return false;
  }
}

std::string MQTTWorker::GetClientId() const { return mqtt_config_.client_id; }

std::string MQTTWorker::GetBrokerUrl() const { return mqtt_config_.broker_url; }

int MQTTWorker::GetQosLevel() const {
  // 🔥 중요: MqttQoS enum을 int로 변환 (컴파일 에러 해결)
  return static_cast<int>(mqtt_config_.default_qos);
}

bool MQTTWorker::GetCleanSession() const { return mqtt_config_.clean_session; }

std::string MQTTWorker::GetUsername() const { return mqtt_config_.username; }

int MQTTWorker::GetKeepAliveInterval() const {
  return mqtt_config_.keepalive_interval_sec;
}

bool MQTTWorker::GetUseSsl() const { return mqtt_config_.use_ssl; }

int MQTTWorker::GetConnectionTimeout() const {
  return mqtt_config_.connection_timeout_sec;
}

int MQTTWorker::GetMaxRetryCount() const {
  return mqtt_config_.max_retry_count;
}

bool MQTTWorker::IsConnected() const {
  return mqtt_driver_ ? mqtt_driver_->IsConnected() : false;
}

std::string MQTTWorker::GetConnectionStatus() const {
  if (!mqtt_driver_)
    return "Driver not initialized";
  if (mqtt_driver_->IsConnected())
    return "Connected";
  return "Disconnected";
}

std::string MQTTWorker::GetDeviceName() const { return device_info_.name; }

std::string MQTTWorker::GetDeviceId() const { return device_info_.id; }

bool MQTTWorker::IsDeviceEnabled() const { return device_info_.is_enabled; }

std::string MQTTWorker::GetBrokerHost() const {
  std::string url = mqtt_config_.broker_url;

  // mqtt://host:port 형식에서 host 추출
  if (url.find("://") != std::string::npos) {
    size_t start = url.find("://") + 3;
    size_t end = url.find(':', start);
    if (end != std::string::npos) {
      return url.substr(start, end - start);
    }
    return url.substr(start);
  }

  // 단순 host:port 형식
  size_t colon_pos = url.find(':');
  return (colon_pos != std::string::npos) ? url.substr(0, colon_pos) : url;
}

int MQTTWorker::GetBrokerPort() const {
  std::string url = mqtt_config_.broker_url;

  // 포트 추출
  size_t colon_pos = url.rfind(':'); // 마지막 ':' 찾기
  if (colon_pos != std::string::npos) {
    try {
      return std::stoi(url.substr(colon_pos + 1));
    } catch (...) {
      // 파싱 실패
    }
  }

  // 기본 MQTT 포트
  return mqtt_config_.use_ssl ? 8883 : 1883;
}

} // namespace Workers
} // namespace PulseOne