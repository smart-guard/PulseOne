// =============================================================================
// collector/src/Drivers/Mqtt/MqttDriver.cpp
// MQTT 드라이버 구현 - 컴파일 에러 수정 완료
// =============================================================================

#include "Drivers/Mqtt/MqttDriver.h"
#include "Database/Repositories/ProtocolRepository.h"
#include "Database/RepositoryFactory.h"
#include "Drivers/Common/DriverFactory.h"
#include "Logging/LogManager.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>

#include <nlohmann/json.hpp>
using json = nlohmann::json;
#if HAS_MQTT_CPP
// Eclipse Paho MQTT C++ 헤더들
#include <mqtt/async_client.h>
#include <mqtt/callback.h>
#include <mqtt/connect_options.h>
#include <mqtt/iaction_listener.h>
#include <mqtt/ssl_options.h>
#endif

namespace PulseOne {
namespace Drivers {

using namespace std::chrono;

// 타입 별칭들 - 헤더와 동일하게
using ErrorCode = PulseOne::Structs::ErrorCode;
using ErrorInfo = PulseOne::Structs::ErrorInfo;
using TimestampedValue = PulseOne::Structs::TimestampedValue;
using DataQuality = PulseOne::Enums::DataQuality; // ✅ 추가

#if HAS_MQTT_CPP

// =============================================================================
// MQTT 콜백 클래스 구현
// =============================================================================

class MqttCallbackImpl : public virtual mqtt::callback {
public:
  explicit MqttCallbackImpl(MqttDriver *driver) : driver_(driver) {}

  void connected(const std::string &cause) override {
    if (driver_) {
      driver_->OnConnected(cause);
    }
  }

  void connection_lost(const std::string &cause) override {
    if (driver_) {
      driver_->OnConnectionLost(cause);
    }
  }

  void message_arrived(mqtt::const_message_ptr msg) override {
    if (driver_) {
      driver_->OnMessageArrived(msg);
    }
  }

  void delivery_complete(mqtt::delivery_token_ptr token) override {
    if (driver_) {
      driver_->OnDeliveryComplete(token);
    }
  }

private:
  MqttDriver *driver_;
};

// =============================================================================
// 액션 리스너 클래스 구현
// =============================================================================

class MqttActionListener : public virtual mqtt::iaction_listener {
public:
  explicit MqttActionListener(MqttDriver *driver) : driver_(driver) {}

  void on_failure(const mqtt::token &token) override {
    if (driver_) {
      driver_->OnActionFailure(token);
    }
  }

  void on_success(const mqtt::token &token) override {
    if (driver_) {
      driver_->OnActionSuccess(token);
    }
  }

private:
  MqttDriver *driver_;
};

// =============================================================================
// 생성자/소멸자
// =============================================================================

MqttDriver::MqttDriver()
    : driver_statistics_("MQTT") // ✅ 표준 통계 초기화
      ,
      status_(Structs::DriverStatus::UNINITIALIZED), is_connected_(false),
      connection_in_progress_(false), last_error_(), broker_url_(),
      client_id_(), default_qos_(0), keep_alive_seconds_(60),
      timeout_ms_(30000), clean_session_(true), auto_reconnect_(true),
      message_processor_running_(false), connection_monitor_running_(false),
      console_output_enabled_(false), packet_logging_enabled_(false),
      connection_start_time_(system_clock::now()), load_balancer_(nullptr) {
  // ✅ MQTT 특화 통계 카운터 초기화
  InitializeMqttCounters();

  // 기본 설정
  client_id_ = GenerateClientId();

  LogMessage("INFO", "MqttDriver created with client_id: " + client_id_,
             "MQTT");
}

MqttDriver::~MqttDriver() {
  // 정리 작업
  Stop();
  Disconnect();
  CleanupMqttClient();
  // 🚀 로드밸런서 정리 (새로 추가)
  if (load_balancer_) {
    load_balancer_->EnableLoadBalancing(false);
  }
  load_balancer_.reset();
  LogMessage("INFO", "MqttDriver destroyed", "MQTT");
}

void MqttDriver::InitializeMqttCounters() {
  // ✅ 표준 DriverStatistics에 MQTT 특화 카운터들 초기화
  driver_statistics_.IncrementProtocolCounter("mqtt_messages", 0);
  driver_statistics_.IncrementProtocolCounter("published_messages", 0);
  driver_statistics_.IncrementProtocolCounter("received_messages", 0);
  driver_statistics_.IncrementProtocolCounter("qos0_messages", 0);
  driver_statistics_.IncrementProtocolCounter("qos1_messages", 0);
  driver_statistics_.IncrementProtocolCounter("qos2_messages", 0);
  driver_statistics_.IncrementProtocolCounter("retained_messages", 0);
  driver_statistics_.IncrementProtocolCounter("connection_failures", 0);
  driver_statistics_.IncrementProtocolCounter("subscription_count", 0);

  // MQTT 특화 메트릭들 초기화
  driver_statistics_.SetProtocolMetric("avg_message_size_bytes", 0.0);
  driver_statistics_.SetProtocolMetric("max_message_size_bytes", 0.0);
  driver_statistics_.SetProtocolMetric("broker_latency_ms", 0.0);
}

// =============================================================================
// IProtocolDriver 인터페이스 구현
// =============================================================================

bool MqttDriver::Initialize(const DriverConfig &config) {
  config_ = config;
  LogMessage("INFO", "Initializing MQTT driver", "MQTT");

  // 설정 파싱
  if (!ParseDriverConfig(config_)) {
    SetError("Failed to parse driver configuration");
    return false;
  }

  // MQTT 클라이언트 초기화
  if (!InitializeMqttClient()) {
    SetError("Failed to initialize MQTT client");
    return false;
  }

  status_ = Structs::DriverStatus::INITIALIZED;
  LogMessage("INFO", "MQTT driver initialized successfully", "MQTT");
  return true;
}

bool MqttDriver::Connect() { return EstablishConnection(); }

bool MqttDriver::Disconnect() {
  if (mqtt_client_ && mqtt_client_->is_connected()) {
    auto token = mqtt_client_->disconnect();
    token->wait();
  }

  is_connected_ = false;
  status_ = Structs::DriverStatus::STOPPED; // ✅ DISCONNECTED → STOPPED
  LogMessage("INFO", "MQTT driver disconnected", "MQTT");
  return true;
}

bool MqttDriver::IsConnected() const {
  return is_connected_.load() && mqtt_client_ && mqtt_client_->is_connected();
}

bool MqttDriver::ReadValues(const std::vector<DataPoint> &points,
                            std::vector<TimestampedValue> &values) {
  if (!IsConnected()) {
    return false;
  }

  values.clear();
  values.reserve(points.size());

  // message_queue_mutex_ 사용 (올바른 뮤텍스)
  std::lock_guard<std::mutex> lock(message_queue_mutex_);

  for (const auto &point : points) {
    TimestampedValue result_value;

    // ✅ 수정: source_id → source (Structs.h의 실제 필드명에 맞춤)
    result_value.source = point.id;      // source_id에서 source로 변경
    result_value.value = DataValue(0.0); // 기본값
    result_value.timestamp = Utils::GetCurrentTimestamp();
    result_value.quality = DataQuality::BAD;

    values.push_back(result_value);
  }

  // 통계 업데이트 (기존 방식 유지)
  driver_statistics_.total_reads.fetch_add(1);
  driver_statistics_.successful_reads.fetch_add(1);

  return true;
}

bool MqttDriver::WriteValue(const DataPoint &point, const DataValue &value) {
  if (!IsConnected()) {
    SetError("MQTT client not connected");
    return false;
  }

  try {
    // DataValue를 문자열로 변환 (variant는 직접 접근)
    std::string payload;
    if (std::holds_alternative<std::string>(value)) {
      payload = std::get<std::string>(value);
    } else if (std::holds_alternative<double>(value)) {
      payload = std::to_string(std::get<double>(value));
    } else if (std::holds_alternative<int>(value)) { // ✅ int64_t → int
      payload = std::to_string(std::get<int>(value));
    } else if (std::holds_alternative<bool>(value)) {
      payload = std::get<bool>(value) ? "true" : "false";
    }

    // ✅ address_string이 있으면 그것을 토픽으로 사용 (MQTT 표준)
    std::string topic;
    if (!point.address_string.empty()) {
      topic = point.address_string;
    } else {
      topic = std::to_string(point.address);
    }

    // ✅ QoS 동적 설정 (protocol_params에서)
    int qos = default_qos_;
    if (point.protocol_params.count("qos")) {
      try {
        qos = std::stoi(point.protocol_params.at("qos"));
      } catch (...) {
      }
    }

    // ✅ Retained 동적 설정
    bool retained = false;
    if (point.protocol_params.count("retained")) {
      std::string ret_str = point.protocol_params.at("retained");
      retained = (ret_str == "true" || ret_str == "1");
    }

    return Publish(topic, payload, qos, retained);

  } catch (const std::exception &e) {
    SetError("Exception during write: " + std::string(e.what()));
    return false;
  }
}

const DriverStatistics &MqttDriver::GetStatistics() const {
  return driver_statistics_;
}

void MqttDriver::ResetStatistics() {
  driver_statistics_.ResetStatistics();
  InitializeMqttCounters(); // MQTT 카운터들 재초기화
  LogMessage("INFO", "MQTT driver statistics reset", "MQTT");
}

std::string MqttDriver::GetProtocolType() const { return "MQTT"; }

Structs::DriverStatus MqttDriver::GetStatus() const { return status_.load(); }

ErrorInfo MqttDriver::GetLastError() const {
  std::lock_guard<std::mutex> lock(error_mutex_);
  return last_error_;
}

// =============================================================================
// MQTT 특화 메서드들
// =============================================================================

bool MqttDriver::Subscribe(const std::string &topic, int qos) {
  if (!IsConnected()) {
    SetError("MQTT client not connected");
    return false;
  }

  auto start_time = steady_clock::now();

  try {
    auto token = mqtt_client_->subscribe(topic, qos);
    token->wait();

    auto end_time = steady_clock::now();
    double duration_ms =
        duration_cast<milliseconds>(end_time - start_time).count();

    {
      std::lock_guard<std::mutex> lock(subscriptions_mutex_);
      subscriptions_[topic] = qos;
    }

    UpdateStats("subscribe", true, duration_ms);
    driver_statistics_.IncrementProtocolCounter("subscription_count", 1);

    LogMessage("INFO",
               "Subscribed to topic: " + topic +
                   " with QoS: " + std::to_string(qos),
               "MQTT");
    return true;

  } catch (const std::exception &e) {
    auto end_time = steady_clock::now();
    double duration_ms =
        duration_cast<milliseconds>(end_time - start_time).count();
    UpdateStats("subscribe", false, duration_ms);
    SetError("Failed to subscribe to topic " + topic + ": " +
             std::string(e.what()));
    return false;
  }
}

bool MqttDriver::Unsubscribe(const std::string &topic) {
  if (!IsConnected()) {
    SetError("MQTT client not connected");
    return false;
  }

  try {
    auto token = mqtt_client_->unsubscribe(topic);
    token->wait();

    {
      std::lock_guard<std::mutex> lock(subscriptions_mutex_);
      subscriptions_.erase(topic);
    }

    driver_statistics_.IncrementProtocolCounter("subscription_count", -1);
    LogMessage("INFO", "Unsubscribed from topic: " + topic, "MQTT");
    return true;

  } catch (const std::exception &e) {
    SetError("Failed to unsubscribe from topic " + topic + ": " +
             std::string(e.what()));
    return false;
  }
}

bool MqttDriver::Publish(const std::string &topic, const std::string &payload,
                         int qos, bool retain) {
  if (!IsConnected()) {
    SetError("발행 실패: 브로커에 연결되지 않음");
    UpdateStats("publish", false);
    return false;
  }

  auto start_time = std::chrono::high_resolution_clock::now();

  try {
    // 🚀 로드밸런싱이 활성화된 경우 토픽별 최적 브로커 확인
    if (load_balancer_ && load_balancer_->IsLoadBalancingEnabled()) {
      std::string optimal_broker =
          load_balancer_->SelectBroker(topic, payload.size());

      if (!optimal_broker.empty() && optimal_broker != broker_url_) {
        LogMessage("DEBUG",
                   "토픽 " + topic + "을 위한 최적 브로커: " + optimal_broker,
                   "MQTT");

        // 필요시 브로커 전환 (고급 기능 - 선택사항)
        if (SwitchBroker(optimal_broker)) {
          LogMessage("INFO", "브로커 전환 완료: " + optimal_broker, "MQTT");
        }
      }
    }

    // MQTT 메시지 생성 및 발행
    mqtt::message_ptr msg = mqtt::make_message(topic, payload);
    msg->set_qos(qos);
    msg->set_retained(retain);

    mqtt::delivery_token_ptr delivery_token = mqtt_client_->publish(msg);
    bool success =
        delivery_token->wait_for(std::chrono::milliseconds(timeout_ms_));

    // 성능 측정
    auto end_time = std::chrono::high_resolution_clock::now();
    double duration_ms =
        std::chrono::duration<double, std::milli>(end_time - start_time)
            .count();

    // 🚀 로드밸런서에 성능 정보 업데이트
    if (load_balancer_ && success) {
      load_balancer_->UpdateBrokerLoad(broker_url_, 0, duration_ms, 0.0, 0.0);
    }

    // 통계 업데이트
    UpdateStats("publish", success, duration_ms);

    if (success) {
      // QoS별 통계
      switch (qos) {
      case 0:
        driver_statistics_.IncrementProtocolCounter("qos0_messages", 1);
        break;
      case 1:
        driver_statistics_.IncrementProtocolCounter("qos1_messages", 1);
        break;
      case 2:
        driver_statistics_.IncrementProtocolCounter("qos2_messages", 1);
        break;
      }

      if (retain) {
        driver_statistics_.IncrementProtocolCounter("retained_messages", 1);
      }
    }

    return success;

  } catch (const std::exception &e) {
    SetError("메시지 발행 중 예외 발생: " + std::string(e.what()));
    UpdateStats("publish", false);
    return false;
  }
}

void MqttDriver::UpdateStats(const std::string &operation, bool success,
                             double duration_ms) {
  if (operation == "read") {
    driver_statistics_.total_reads.fetch_add(1);
    if (success) {
      driver_statistics_.successful_reads.fetch_add(1);
    } else {
      driver_statistics_.failed_reads.fetch_add(1);
    }
  } else if (operation == "write" || operation == "publish") {
    driver_statistics_.total_writes.fetch_add(1);
    if (success) {
      driver_statistics_.successful_writes.fetch_add(1);
    } else {
      driver_statistics_.failed_writes.fetch_add(1);
    }
  }

  // MQTT 메시지 카운터 업데이트
  driver_statistics_.IncrementProtocolCounter("mqtt_messages", 1);

  // ✅ total_errors 제거 - DriverStatistics에 없음
  if (!success) {
    driver_statistics_.failed_reads.fetch_add(1); // 실패 카운터는 따로 관리
  }

  // 응답 시간 통계는 DriverStatistics에서 내부적으로 처리
  // duration_ms 사용 (경고 제거)
  (void)duration_ms; // 경고 방지
}

// =============================================================================
// 콜백 핸들러들
// =============================================================================

void MqttDriver::OnConnected(const std::string &cause) {
  is_connected_ = true;
  status_ = Structs::DriverStatus::RUNNING;
  connection_start_time_ = system_clock::now();

  // 통계 업데이트
  // driver_statistics_.total_connections.fetch_add(1);
  driver_statistics_.successful_connections.fetch_add(1);

  LogMessage("INFO", "MQTT connected: " + cause, "MQTT");

  // ===== 고급 기능들에 연결 성공 알림 =====

  // 진단 기능에 연결 이벤트 기록
  if (diagnostics_) {
    diagnostics_->RecordConnectionEvent(true, broker_url_);
  }

  // ✅ 페일오버에 연결 성공 알림 - 메서드명 수정됨
  if (failover_) {
    failover_->HandleConnectionSuccess();
  }

  // 구독 복원
  RestoreSubscriptions();
  // 연결 변화 알림
  NotifyConnectionChange(true, broker_url_, cause);
}

// OnConnectionLost 함수 수정 (기존 함수 내용에 추가)
void MqttDriver::OnConnectionLost(const std::string &cause) {
  is_connected_ = false;
  status_ = Structs::DriverStatus::DRIVER_ERROR;

  LogMessage("WARN", "MQTT connection lost: " + cause, "MQTT");

  // ===== 고급 기능들에 연결 끊김 알림 =====

  // 진단 기능에 연결 끊김 기록
  if (diagnostics_) {
    diagnostics_->RecordConnectionEvent(false, broker_url_);
    diagnostics_->RecordOperation("connection_lost", false, 0.0);
  }

  // 페일오버 트리거 (자동 재연결)
  if (failover_ && failover_->IsFailoverEnabled()) {
    failover_->TriggerFailover("Connection lost: " + cause);
  } else if (auto_reconnect_) {
    HandleConnectionLoss(cause);
  }
}

// OnMessageArrived 함수 수정 (기존 함수 내용에 추가)
void MqttDriver::OnMessageArrived(mqtt::const_message_ptr msg) {
  if (!msg)
    return;

  try {
    std::string topic = msg->get_topic();
    std::string payload = msg->to_string();
    int qos = msg->get_qos();

    ProcessReceivedMessage(topic, payload, qos);

    driver_statistics_.IncrementProtocolCounter("received_messages", 1);

    // QoS별 통계
    switch (qos) {
    case 0:
      driver_statistics_.IncrementProtocolCounter("qos0_messages", 1);
      break;
    case 1:
      driver_statistics_.IncrementProtocolCounter("qos1_messages", 1);
      break;
    case 2:
      driver_statistics_.IncrementProtocolCounter("qos2_messages", 1);
      break;
    }

    // ===== 고급 기능들에 메시지 수신 알림 =====

    // 진단 기능에 메시지 분석 정보 기록 - ✅ 메서드명 수정
    if (diagnostics_) {
      diagnostics_->RecordOperation("message_received", true, 0.0);
      // RecordQosPerformance 메서드가 없으므로 제거
    }

    LogPacket("RECV", topic, payload, qos);

  } catch (const std::exception &e) {
    LogMessage("ERROR",
               "Exception in message handler: " + std::string(e.what()),
               "MQTT");

    // 진단 기능에 에러 기록
    if (diagnostics_) {
      diagnostics_->RecordOperation("message_error", false, 0.0);
    }
  }
}

// OnDeliveryComplete 함수 수정 (기존 함수 내용에 추가)
void MqttDriver::OnDeliveryComplete(mqtt::delivery_token_ptr token) {
  // 메시지 전송 완료
  LogMessage("DEBUG", "Message delivery complete", "MQTT");

  // ===== 고급 기능들에 전송 완료 알림 =====

  // 진단 기능에 전송 성공 기록
  if (diagnostics_) {
    diagnostics_->RecordOperation("message_delivered", true, 0.0);
  }

  (void)token; // 경고 방지
}

void MqttDriver::OnActionFailure(const mqtt::token &token) {
  std::string error_msg = "MQTT operation failed";
  LogMessage("ERROR", error_msg, "MQTT");
  SetError(error_msg);
  (void)token; // 경고 방지
}

void MqttDriver::OnActionSuccess(const mqtt::token &token) {
  LogMessage("DEBUG", "MQTT operation successful", "MQTT");
  (void)token; // 경고 방지
}

// =============================================================================
// 고급 기능 관련 내부 메서드들 구현
// =============================================================================

void MqttDriver::NotifyAdvancedFeatures(const std::string &event_type,
                                        const std::string &data) {
  // 모든 고급 기능들에 이벤트 전파
  if (diagnostics_) {
    // 진단 기능에 이벤트 알림 (필요시 추가 구현)
  }

  if (failover_) {
    // 페일오버에 이벤트 알림 (필요시 추가 구현)
  }

  // 경고 방지
  (void)event_type;
  (void)data;
}

void MqttDriver::RecordDiagnosticEvent(const std::string &operation,
                                       bool success, double duration_ms,
                                       const std::string &details) {
  if (diagnostics_) {
    diagnostics_->RecordOperation(operation, success, duration_ms);
  }

  // 경고 방지
  (void)details;
}

void MqttDriver::NotifyConnectionChange(bool connected,
                                        const std::string &broker_url,
                                        const std::string &reason) {
  // 사용하지 않는 매개변수 경고 방지
  (void)reason;

  if (connected) {
    // 연결 성공 시
    if (failover_) {
      failover_->HandleConnectionSuccess();
    }

    // ✅ connection_callback_ 필드가 없으므로 제거 또는 다른 방식으로 처리
    // 연결 성공 로깅만 수행
    LogMessage("INFO", "Connection established to broker: " + broker_url,
               "MQTT");

  } else {
    // 연결 실패 시
    if (failover_) {
      failover_->HandleConnectionFailure(reason);
    }

    // 연결 실패 로깅
    LogMessage("WARN",
               "Connection failed to broker: " + broker_url +
                   ", reason: " + reason,
               "MQTT");
  }
}

void MqttDriver::NotifyMessageProcessing(const std::string &broker_url,
                                         const std::string &topic, bool success,
                                         double processing_time_ms) {
  if (diagnostics_) {
    std::string operation = "process_" + topic;
    diagnostics_->RecordOperation(operation, success, processing_time_ms);
  }

  // 경고 방지
  (void)broker_url;
}

// =============================================================================
// 지원 메서드들
// =============================================================================

std::string MqttDriver::GenerateClientId() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::ostringstream oss;
  oss << "PulseOne_MQTT_" << time_t;
  return oss.str();
}

bool MqttDriver::Start() {
  if (message_processor_running_) {
    return true; // 이미 시작됨
  }

  try {
    message_processor_running_ = true;
    message_processor_thread_ =
        std::thread(&MqttDriver::MessageProcessorLoop, this);

    if (auto_reconnect_) {
      connection_monitor_running_ = true;
      connection_monitor_thread_ =
          std::thread(&MqttDriver::ConnectionMonitorLoop, this);
    }

    status_ = Structs::DriverStatus::RUNNING;
    LogMessage("INFO", "MQTT driver started", "MQTT");
    return true;

  } catch (const std::exception &e) {
    SetError("Failed to start MQTT driver: " + std::string(e.what()));
    return false;
  }
}

bool MqttDriver::Stop() {
  message_processor_running_ = false;
  connection_monitor_running_ = false;

  // 조건 변수들 깨우기
  message_queue_cv_.notify_all();
  connection_cv_.notify_all();

  // 스레드들 정리
  if (message_processor_thread_.joinable()) {
    message_processor_thread_.join();
  }

  if (connection_monitor_thread_.joinable()) {
    connection_monitor_thread_.join();
  }

  status_ = Structs::DriverStatus::STOPPED;
  LogMessage("INFO", "MQTT driver stopped", "MQTT");
  return true;
}

void MqttDriver::SetError(const std::string &error_message) {
  {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.code = ErrorCode::DEVICE_ERROR;
    last_error_.message = error_message;
    // ✅ timestamp 필드 제거 - ErrorInfo에 occurred_at이 자동 설정됨
  }

  status_ = Structs::DriverStatus::DRIVER_ERROR;
  LogMessage("ERROR", error_message, "MQTT");
}

void MqttDriver::LogMessage(const std::string &level,
                            const std::string &message,
                            const std::string &category) const {
  // ✅ logger_는 객체이므로 포인터 체크 불가, 객체 존재 여부를 다른 방식으로
  // 확인 현재는 단순하게 콘솔 출력만 사용

  // 콘솔 출력
  if (console_output_enabled_) {
    std::cout << "[" << level << "] [" << category << "] " << message
              << std::endl;
  }
}

void MqttDriver::LogPacket(const std::string &direction,
                           const std::string &topic, const std::string &payload,
                           int qos) const {
  if (packet_logging_enabled_) {
    std::ostringstream log_msg;
    log_msg << direction << " Topic: " << topic << " QoS: " << qos
            << " Payload: " << payload.substr(0, 100);
    if (payload.size() > 100) {
      log_msg << "... (" << payload.size() << " bytes total)";
    }

    LogMessage("DEBUG", log_msg.str(), "PACKET");
  }
}

// =============================================================================
// 개별 메서드들 (간소화 버전)
// =============================================================================

bool MqttDriver::EstablishConnection() {
  // 🚀 로드밸런싱 브로커 선택 (맨 앞에 추가)
  std::string target_broker = broker_url_;
  if (load_balancer_ && load_balancer_->IsLoadBalancingEnabled()) {
    std::string selected_broker = load_balancer_->SelectBroker("connection");
    if (!selected_broker.empty()) {
      target_broker = selected_broker;
      LogMessage("INFO", "로드밸런서에서 브로커 선택: " + target_broker,
                 "MQTT");
    }
  }

  if (connection_in_progress_) {
    return false;
  }

  connection_in_progress_ = true;
  auto start_time = steady_clock::now();

  try {
    // 🚀 중요: 선택된 브로커로 클라이언트 재생성 (새로 추가)
    if (!mqtt_client_ || target_broker != broker_url_) {
      // 기존 클라이언트 정리
      if (mqtt_client_) {
        mqtt_client_.reset();
      }

      // 선택된 브로커로 새 클라이언트 생성
      mqtt_client_ =
          std::make_unique<mqtt::async_client>(target_broker, client_id_);

      // 콜백 재설정 (필요시)
      if (mqtt_callback_) {
        mqtt_client_->set_callback(*mqtt_callback_);
      }

      LogMessage("DEBUG", "MQTT 클라이언트 재생성: " + target_broker, "MQTT");
    }

    if (!mqtt_client_) {
      SetError("MQTT client not initialized");
      connection_in_progress_ = false;
      return false;
    }

    if (mqtt_client_->is_connected()) {
      LogMessage("INFO", "Already connected to MQTT broker", "MQTT");
      is_connected_ = true;
      status_ = Structs::DriverStatus::RUNNING;
      connection_in_progress_ = false;
      return true;
    }

    mqtt::connect_options connOpts;
    connOpts.set_keep_alive_interval(keep_alive_seconds_);
    connOpts.set_clean_session(clean_session_);
    connOpts.set_automatic_reconnect(auto_reconnect_);

    if (!username_.empty()) {
      connOpts.set_user_name(username_);
    }
    if (!password_.empty()) {
      connOpts.set_password(password_);
    }

    auto token = mqtt_client_->connect(connOpts);
    bool success = token->wait_for(std::chrono::milliseconds(timeout_ms_));

    auto end_time = steady_clock::now();
    double duration_ms =
        duration_cast<milliseconds>(end_time - start_time).count();

    UpdateStats("connect", success, duration_ms);

    if (success) {
      is_connected_ = true;
      status_ = Structs::DriverStatus::RUNNING;

      // 🚀 성공 시 브로커 URL 업데이트 및 로드밸런서 알림
      broker_url_ = target_broker; // 성공한 브로커로 업데이트

      if (load_balancer_) {
        load_balancer_->UpdateBrokerLoad(target_broker, 1, duration_ms, 0.0,
                                         0.0);
      }

      LogMessage("INFO",
                 "Successfully connected to MQTT broker: " + broker_url_,
                 "MQTT");
    } else {
      // 🚀 실패 시 로드밸런서에 알림
      if (load_balancer_) {
        load_balancer_->UpdateBrokerLoad(target_broker, 0, 0.0, 100.0, 0.0);
      }

      SetError("Failed to connect to MQTT broker: " + target_broker);
    }

    connection_in_progress_ = false;
    return success;

  } catch (const std::exception &e) {
    auto end_time = steady_clock::now();
    double duration_ms =
        duration_cast<milliseconds>(end_time - start_time).count();

    UpdateStats("connect", false, duration_ms);

    // 🚀 예외 발생 시 로드밸런서에 알림
    if (load_balancer_) {
      load_balancer_->UpdateBrokerLoad(target_broker, 0, 0.0, 100.0, 0.0);
    }

    SetError("MQTT connection exception: " + std::string(e.what()));
    connection_in_progress_ = false;
    return false;
  }
}

bool MqttDriver::InitializeMqttClient() {
  try {
    if (broker_url_.empty()) {
      SetError("Broker URL not specified");
      return false;
    }

    mqtt_client_ =
        std::make_unique<mqtt::async_client>(broker_url_, client_id_);

    // 콜백 설정
    mqtt_callback_ = std::make_shared<MqttCallbackImpl>(this);
    mqtt_action_listener_ = std::make_shared<MqttActionListener>(this);

    mqtt_client_->set_callback(*mqtt_callback_);

    LogMessage("INFO", "MQTT client initialized for broker: " + broker_url_,
               "MQTT");
    return true;

  } catch (const std::exception &e) {
    SetError("Failed to initialize MQTT client: " + std::string(e.what()));
    return false;
  }
}

void MqttDriver::CleanupMqttClient() {
  if (mqtt_client_) {
    try {
      if (mqtt_client_->is_connected()) {
        mqtt_client_->disconnect();
      }
    } catch (...) {
      // 무시
    }
    mqtt_client_.reset();
  }

  mqtt_callback_.reset();
  mqtt_action_listener_.reset();
}

bool MqttDriver::ParseDriverConfig(const DriverConfig &config) {
  try {
    // =================================================================
    // 1. 기본 연결 정보 설정 (기존 필드만 사용)
    // =================================================================

    // 브로커 URL 설정 (endpoint 우선)
    if (!config.endpoint.empty()) {
      broker_url_ = config.endpoint;
      LogMessage("INFO", "Using endpoint as broker URL: " + broker_url_,
                 "MQTT");
    } else {
      // 기본값 설정
      broker_url_ = "tcp://localhost:1883";
      LogMessage("WARN", "No endpoint specified, using default: " + broker_url_,
                 "MQTT");
    }

    // 클라이언트 ID 설정
    if (!config.name.empty()) {
      client_id_ = config.name + "_" + std::to_string(std::time(nullptr));
    } else {
      client_id_ = GenerateClientId();
    }

    // 기본 타이밍 설정
    timeout_ms_ = config.timeout_ms;
    auto_reconnect_ = config.auto_reconnect;

    // 🔥 config.properties에서 직접 인증 정보 추출 추가 (ModbusDriver 패턴)
    if (config.properties.count("username")) {
      username_ = config.properties.at("username");
    }
    if (config.properties.count("password")) {
      password_ = config.properties.at("password");
    }

    LogMessage("INFO",
               "Basic MQTT config set: client_id=" + client_id_ +
                   ", timeout=" + std::to_string(timeout_ms_) +
                   "ms, user=" + (username_.empty() ? "none" : username_),
               "MQTT");

    // 🔥 진단 모드 설정 확인 (properties 우선)
    bool diag_enabled = false;
    if (config.properties.count("diagnostic_mode")) {
      diag_enabled = (config.properties.at("diagnostic_mode") == "true");
    } else if (config.properties.count("debug")) {
      diag_enabled = (config.properties.at("debug") == "true");
    }

    if (diag_enabled) {
      EnableDiagnostics(true, true, true);
    }

    // =================================================================
    // 2. JSON 설정 파싱 (endpoint를 JSON으로 처리)
    // =================================================================

    std::string json_config_str;
    bool has_json_config = false;

    // endpoint가 JSON 형태인지 확인
    if (!config.endpoint.empty() && config.endpoint.front() == '{' &&
        config.endpoint.back() == '}') {
      json_config_str = config.endpoint;
      has_json_config = true;
      LogMessage("DEBUG", "Using endpoint as JSON config", "MQTT");
    }

    // JSON 설정이 있다면 파싱
    if (has_json_config) {
      try {
        // nlohmann::json 직접 사용 (using 선언으로 json 별칭 사용)
        json json_config = json::parse(json_config_str);

        // =============================================================
        // MQTT 특화 설정들 파싱 (기존 필드만)
        // =============================================================

        // QoS 설정
        if (json_config.contains("qos")) {
          // .get<int>() 대신 직접 캐스팅
          if (json_config["qos"].is_number_integer()) {
            default_qos_ = json_config["qos"];
            if (default_qos_ < 0 || default_qos_ > 2) {
              LogMessage("WARN", "Invalid QoS value, using default: 0", "MQTT");
              default_qos_ = 0;
            }
          }
        }

        // Keep Alive 설정
        if (json_config.contains("keep_alive")) {
          if (json_config["keep_alive"].is_number_integer()) {
            keep_alive_seconds_ = json_config["keep_alive"];
            if (keep_alive_seconds_ < 10 || keep_alive_seconds_ > 300) {
              LogMessage("WARN", "Keep alive out of range (10-300s), using 60s",
                         "MQTT");
              keep_alive_seconds_ = 60;
            }
          }
        }

        // Clean Session 설정
        if (json_config.contains("clean_session")) {
          if (json_config["clean_session"].is_boolean()) {
            clean_session_ = json_config["clean_session"];
          }
        }

        // Auto Reconnect 설정 (JSON이 config보다 우선)
        if (json_config.contains("auto_reconnect")) {
          if (json_config["auto_reconnect"].is_boolean()) {
            auto_reconnect_ = json_config["auto_reconnect"];
          }
        }

        // Timeout 설정 (JSON이 config보다 우선)
        if (json_config.contains("timeout_ms")) {
          if (json_config["timeout_ms"].is_number_integer()) {
            timeout_ms_ = json_config["timeout_ms"];
            if (timeout_ms_ < 1000 || timeout_ms_ > 60000) {
              LogMessage("WARN", "Timeout out of range (1-60s), using 30s",
                         "MQTT");
              timeout_ms_ = 30000;
            }
          }
        }

        // =============================================================
        // 브로커 설정 오버라이드 (JSON이 endpoint보다 우선)
        // =============================================================

        if (json_config.contains("broker_host")) {
          std::string broker_host = "";
          if (json_config["broker_host"].is_string()) {
            broker_host = json_config["broker_host"];
          }

          int broker_port = 1883;
          if (json_config.contains("broker_port") &&
              json_config["broker_port"].is_number_integer()) {
            broker_port = json_config["broker_port"];
          }

          if (!broker_host.empty()) {
            // URL 재구성 (기본 tcp 프로토콜 사용)
            broker_url_ =
                "tcp://" + broker_host + ":" + std::to_string(broker_port);
            LogMessage("INFO", "Broker URL updated from JSON: " + broker_url_,
                       "MQTT");
          }
        }

        // 전체 broker_url이 JSON에 있으면 사용
        if (json_config.contains("broker_url")) {
          if (json_config["broker_url"].is_string()) {
            broker_url_ = json_config["broker_url"];
            LogMessage("INFO", "Broker URL from JSON: " + broker_url_, "MQTT");
          }
        }

        // =============================================================
        // 클라이언트 ID 오버라이드
        // =============================================================

        if (json_config.contains("client_id")) {
          if (json_config["client_id"].is_string()) {
            std::string json_client_id = json_config["client_id"];
            if (!json_client_id.empty()) {
              client_id_ = json_client_id;
            } else {
              LogMessage("WARN",
                         "Empty client_id in JSON, keeping generated: " +
                             client_id_,
                         "MQTT");
            }
          }
        }

        // =============================================================
        // 로그 출력을 위한 추가 정보 (실제 기능은 구현하지 않음)
        // =============================================================

        bool use_ssl = false;
        if (json_config.contains("use_ssl") &&
            json_config["use_ssl"].is_boolean()) {
          use_ssl = json_config["use_ssl"];
        }

        if (json_config.contains("username") &&
            json_config["username"].is_string()) {
          username_ = json_config["username"];
        }
        if (json_config.contains("password") &&
            json_config["password"].is_string()) {
          password_ = json_config["password"];
        }

        LogMessage("INFO", "MQTT JSON configuration parsed successfully",
                   "MQTT");

        // SSL 및 인증 정보 로그 (실제 적용은 안함)
        if (use_ssl) {
          LogMessage("INFO",
                     "  SSL requested (not implemented in current driver)",
                     "MQTT");
        }
        if (!username_.empty()) {
          LogMessage("INFO", "  Authentication requested", "MQTT");
        }

      } catch (const json::exception &e) {
        LogMessage("WARN",
                   "Failed to parse JSON config: " + std::string(e.what()) +
                       ", using basic configuration",
                   "MQTT");
        // JSON 파싱 실패해도 기본 설정으로 계속 진행
      } catch (const std::exception &e) {
        LogMessage("WARN",
                   "General exception parsing JSON config: " +
                       std::string(e.what()) + ", using basic configuration",
                   "MQTT");
      }
    } else {
      LogMessage("INFO", "No JSON configuration found, using basic settings",
                 "MQTT");
    }

    // =================================================================
    // 3. 설정 검증 및 최종 조정
    // =================================================================

    // 브로커 URL 검증
    if (broker_url_.find("://") == std::string::npos) {
      // 프로토콜이 없으면 기본 프로토콜 추가
      broker_url_ = "tcp://" + broker_url_;
      LogMessage("INFO", "Added protocol to broker URL: " + broker_url_,
                 "MQTT");
    }

    // 클라이언트 ID 최종 검증
    if (client_id_.empty()) {
      client_id_ = GenerateClientId();
      LogMessage("WARN", "Client ID was empty, generated: " + client_id_,
                 "MQTT");
    }

    // =================================================================
    // 4. 최종 로그 및 성공 반환 (기존 필드만 사용)
    // =================================================================

    LogMessage("INFO", "MQTT configuration completed successfully:", "MQTT");
    LogMessage("INFO", "  Broker: " + broker_url_, "MQTT");
    LogMessage("INFO", "  Client ID: " + client_id_, "MQTT");
    LogMessage("INFO", "  QoS: " + std::to_string(default_qos_), "MQTT");
    LogMessage("INFO",
               "  Keep Alive: " + std::to_string(keep_alive_seconds_) + "s",
               "MQTT");
    LogMessage("INFO",
               "  Clean Session: " +
                   std::string(clean_session_ ? "true" : "false"),
               "MQTT");
    LogMessage("INFO",
               "  Auto Reconnect: " +
                   std::string(auto_reconnect_ ? "true" : "false"),
               "MQTT");
    LogMessage("INFO", "  Timeout: " + std::to_string(timeout_ms_) + "ms",
               "MQTT");

    return true;

  } catch (const std::exception &e) {
    std::string error_msg =
        "Failed to parse driver config: " + std::string(e.what());
    SetError(error_msg);
    LogMessage("ERROR", error_msg, "MQTT");
    return false;
  }
}

void MqttDriver::ProcessReceivedMessage(const std::string &topic,
                                        const std::string &payload, int qos) {
  // 기본 메시지 처리 로직
  LogMessage("DEBUG", "Processing message from topic: " + topic, "MQTT");

  {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (message_callback_) {
      message_callback_(topic, payload);
    }
  }

  // 경고 방지
  (void)qos;
}

void MqttDriver::MessageProcessorLoop() {
  while (message_processor_running_) {
    std::unique_lock<std::mutex> lock(message_queue_mutex_);
    message_queue_cv_.wait(lock, [this] {
      return !message_queue_.empty() || !message_processor_running_;
    });

    while (!message_queue_.empty() && message_processor_running_) {
      auto [topic, payload] = message_queue_.front();
      message_queue_.pop();
      lock.unlock();

      ProcessReceivedMessage(topic, payload, 0);

      lock.lock();
    }
  }
}

void MqttDriver::ConnectionMonitorLoop() {
  while (connection_monitor_running_) {
    std::unique_lock<std::mutex> lock(connection_mutex_);
    connection_cv_.wait_for(lock, std::chrono::seconds(30),
                            [this] { return !connection_monitor_running_; });

    if (!connection_monitor_running_)
      break;

    // 연결 상태 확인 및 재연결 시도
    if (!IsConnected() && ShouldReconnect()) {
      LogMessage("INFO", "Attempting automatic reconnection...", "MQTT");
      EstablishConnection();
    }
  }
}

bool MqttDriver::ShouldReconnect() const {
  return auto_reconnect_ && !connection_in_progress_;
}

bool MqttDriver::RestoreSubscriptions() {
  if (!IsConnected()) {
    return false;
  }

  std::lock_guard<std::mutex> lock(subscriptions_mutex_);

  bool all_success = true;
  for (const auto &[topic, qos] : subscriptions_) {
    try {
      auto token = mqtt_client_->subscribe(topic, qos);
      if (!token->wait_for(std::chrono::milliseconds(timeout_ms_))) {
        all_success = false;
      }
    } catch (const std::exception &e) {
      all_success = false;
    }
  }

  return all_success;
}

void MqttDriver::HandleConnectionLoss(const std::string &cause) {
  driver_statistics_.IncrementProtocolCounter("connection_failures", 1);

  LogMessage("INFO", "Will attempt to reconnect due to: " + cause, "MQTT");

  // 재연결 로직은 ConnectionMonitorLoop에서 처리
}

bool MqttDriver::EnableDiagnostics(bool enable, bool packet_logging,
                                   bool console_output) {
  try {
    if (enable && !diagnostics_) {
      diagnostics_ = std::make_unique<MqttDiagnostics>(this);

      // 진단 옵션 설정
      diagnostics_->EnableMessageTracking(true);
      diagnostics_->EnableQosAnalysis(true);
      diagnostics_->SetMaxHistorySize(1000);

      packet_logging_enabled_ = packet_logging;
      console_output_enabled_ = console_output;

      LogMessage("INFO",
                 "MQTT diagnostics enabled (Log:" +
                     std::string(packet_logging ? "ON" : "OFF") + ", Console:" +
                     std::string(console_output ? "ON" : "OFF") + ")",
                 "MQTT-DRIVER");
      return true;

    } else if (!enable && diagnostics_) {
      diagnostics_.reset();
      packet_logging_enabled_ = false;
      console_output_enabled_ = false;
      LogMessage("INFO", "MQTT diagnostics disabled", "MQTT-DRIVER");
    }

    return true;

  } catch (const std::exception &e) {
    SetError("Exception in EnableDiagnostics: " + std::string(e.what()));
    return false;
  }
}

void MqttDriver::DisableDiagnostics() {
  diagnostics_.reset();
  LogMessage("INFO", "MQTT diagnostics disabled", "MQTT-DRIVER");
}

bool MqttDriver::IsDiagnosticsEnabled() const {
  return diagnostics_ != nullptr;
}

bool MqttDriver::EnableFailover(const std::vector<std::string> &backup_brokers,
                                const ReconnectStrategy &strategy) {
  try {
    if (!failover_) {
      failover_ = std::make_unique<MqttFailover>(this);
    }

    // 재연결 전략 설정
    failover_->SetReconnectStrategy(strategy);

    // 백업 브로커들 추가
    for (const auto &broker_url : backup_brokers) {
      failover_->AddBroker(broker_url);
    }

    // 페일오버 활성화
    failover_->EnableFailover(true);

    LogMessage("INFO",
               "MQTT failover enabled with " +
                   std::to_string(backup_brokers.size()) + " backup brokers",
               "MQTT-DRIVER");

    return true;

  } catch (const std::exception &e) {
    SetError("Failed to enable MQTT failover: " + std::string(e.what()));
    return false;
  }
}

void MqttDriver::DisableFailover() {
  if (failover_) {
    failover_->EnableFailover(false);
    failover_.reset();
    LogMessage("INFO", "MQTT failover disabled", "MQTT-DRIVER");
  }
}

bool MqttDriver::IsFailoverEnabled() const {
  return failover_ && failover_->IsFailoverEnabled();
}

bool MqttDriver::TriggerManualFailover(const std::string &reason) {
  if (!failover_) {
    SetError("Failover not enabled");
    return false;
  }

  return failover_->TriggerFailover(reason);
}

std::string MqttDriver::GetFailoverStatistics() const {
  if (!failover_) {
    return "{\"error\":\"Failover not enabled\"}";
  }

  return failover_->GetStatisticsJSON();
}

// =============================================================================
// 진단 정보 조회 메서드들
// =============================================================================

std::string MqttDriver::GetDetailedDiagnosticsJSON() const {
  if (!diagnostics_) {
    return "{\"error\":\"Diagnostics not enabled\"}";
  }

  return diagnostics_->GetDiagnosticsJSON();
}

double MqttDriver::GetMessageLossRate() const {
  if (!diagnostics_) {
    return 0.0;
  }

  return diagnostics_->GetMessageLossRate();
}

std::map<int, QosAnalysis> MqttDriver::GetQosAnalysis() const {
  if (!diagnostics_) {
    return {};
  }

  return diagnostics_->GetQosAnalysis();
}

std::map<std::string, TopicStats> MqttDriver::GetDetailedTopicStats() const {
  if (!diagnostics_) {
    return {};
  }

  return diagnostics_->GetTopicStatistics();
}

// =============================================================================
// 페일오버 관련 메서드들
// =============================================================================

void MqttDriver::AddBackupBroker(const std::string &broker_url,
                                 const std::string &name, int priority) {
  if (!failover_) {
    // 페일오버가 비활성화된 경우 자동 활성화
    std::vector<std::string> backup_brokers = {broker_url};
    EnableFailover(backup_brokers);
  } else {
    failover_->AddBroker(broker_url, name, priority);
  }
}

BrokerInfo MqttDriver::GetCurrentBrokerInfo() const {
  BrokerInfo info;
  info.url = broker_url_;
  info.name = "Primary";
  // ✅ is_connected 필드가 없으므로 제거
  info.priority = 0;

  if (failover_) {
    // 페일오버에서 현재 브로커 정보 가져오기
    auto brokers = failover_->GetAllBrokers();
    for (const auto &broker : brokers) {
      if (broker.url == broker_url_) {
        return broker;
      }
    }
  }

  return info;
}

std::vector<BrokerInfo> MqttDriver::GetAllBrokerStatus() const {
  std::vector<BrokerInfo> brokers;

  // 기본 브로커 추가
  brokers.push_back(GetCurrentBrokerInfo());

  // 페일오버 브로커들 추가
  if (failover_) {
    auto failover_brokers = failover_->GetAllBrokers();
    brokers.insert(brokers.end(), failover_brokers.begin(),
                   failover_brokers.end());
  }

  return brokers;
}

bool MqttDriver::SwitchToOptimalBroker() {
  if (!failover_) {
    return false;
  }

  // ✅ 메서드명 수정
  return failover_->SwitchToBestBroker(); // SwitchToOptimalBroker 대신
}

std::string MqttDriver::GetDiagnosticsJSON() const {
  // ✅ 참조로 사용하여 복사 생성자 문제 해결
  const auto &stats = GetStatistics();

#ifdef HAS_JSON
  try {
    json diagnostics;

    // 기본 통계 (참조 사용)
    json basic_stats;
    basic_stats["total_reads"] = stats.total_reads.load();
    basic_stats["successful_reads"] = stats.successful_reads.load();
    basic_stats["failed_reads"] = stats.failed_reads.load();
    basic_stats["total_writes"] = stats.total_writes.load();
    basic_stats["successful_writes"] = stats.successful_writes.load();
    basic_stats["failed_writes"] = stats.failed_writes.load();
    basic_stats["success_rate"] = stats.GetSuccessRate();
    // ✅ GetAverageResponseTime() → avg_response_time_ms.load() 수정
    basic_stats["avg_response_time_ms"] = stats.avg_response_time_ms.load();

    diagnostics["basic_statistics"] = basic_stats;

    // MQTT 특화 통계
    json mqtt_stats;
    // ✅ GetProtocolCounters() → 개별 GetProtocolCounter() 호출로 수정
    mqtt_stats["mqtt_messages"] = stats.GetProtocolCounter("mqtt_messages");
    mqtt_stats["published_messages"] =
        stats.GetProtocolCounter("published_messages");
    mqtt_stats["received_messages"] =
        stats.GetProtocolCounter("received_messages");
    mqtt_stats["qos0_messages"] = stats.GetProtocolCounter("qos0_messages");
    mqtt_stats["qos1_messages"] = stats.GetProtocolCounter("qos1_messages");
    mqtt_stats["qos2_messages"] = stats.GetProtocolCounter("qos2_messages");
    mqtt_stats["connection_failures"] =
        stats.GetProtocolCounter("connection_failures");

    // ✅ GetProtocolMetrics() → 개별 GetProtocolMetric() 호출로 수정
    mqtt_stats["avg_message_size_bytes"] =
        stats.GetProtocolMetric("avg_message_size_bytes");
    mqtt_stats["connection_uptime_seconds"] =
        stats.GetProtocolMetric("connection_uptime_seconds");

    diagnostics["mqtt_statistics"] = mqtt_stats;

    // 연결 정보
    json connection_info;
    connection_info["broker_url"] = broker_url_;
    connection_info["client_id"] = client_id_;
    connection_info["is_connected"] = is_connected_.load();
    connection_info["auto_reconnect"] = auto_reconnect_;
    // ✅ keep_alive_interval_ → keep_alive_seconds_ 수정
    connection_info["keep_alive_seconds"] = keep_alive_seconds_;

    diagnostics["connection_info"] = connection_info;

    return diagnostics.dump(2);

  } catch (const std::exception &e) {
    return "{\"error\":\"Failed to generate diagnostics JSON: " +
           std::string(e.what()) + "\"}";
  }
#else
  // JSON 라이브러리가 없는 경우 간단한 문자열 포맷
  std::ostringstream oss;
  oss << "{"
      << "\"total_reads\":" << stats.total_reads.load() << ","
      << "\"successful_reads\":" << stats.successful_reads.load() << ","
      << "\"failed_reads\":" << stats.failed_reads.load() << ","
      << "\"success_rate\":" << std::fixed << std::setprecision(2)
      << stats.GetSuccessRate() << ","
      << "\"broker_url\":\"" << broker_url_ << "\","
      << "\"is_connected\":" << (is_connected_.load() ? "true" : "false")
      << "}";
  return oss.str();
#endif
}

bool MqttDriver::EnableLoadBalancing(const std::vector<std::string> &brokers,
                                     LoadBalanceAlgorithm algorithm) {
  if (brokers.empty()) {
    SetError("로드밸런싱 활성화 실패: 브로커 목록이 비어있음");
    return false;
  }

  try {
    // 로드밸런서 생성
    if (!load_balancer_) {
      load_balancer_ = std::make_unique<MqttLoadBalancer>(this);
    }

    // 기존 브로커들 정리
    load_balancer_->EnableLoadBalancing(false);

    // 새 브로커들 추가
    for (size_t i = 0; i < brokers.size(); ++i) {
      const auto &broker = brokers[i];
      if (!broker.empty()) {
        // 첫 번째 브로커에 높은 우선순위
        int weight = (i == 0) ? 10 : 5;
        load_balancer_->AddBroker(broker, "Broker-" + std::to_string(i + 1),
                                  weight);
      }
    }

    // 알고리즘 설정 및 활성화
    load_balancer_->SetDefaultAlgorithm(algorithm);
    load_balancer_->EnableLoadBalancing(true);

    // 부하 모니터링 시작 (5초 간격)
    load_balancer_->EnableLoadMonitoring(true, 5000);

    LogMessage("INFO",
               "MQTT 로드밸런싱 활성화됨 - 브로커 수: " +
                   std::to_string(brokers.size()),
               "MQTT");

    // 통계 업데이트
    driver_statistics_.IncrementProtocolCounter("loadbalancer_activations", 1);

    return true;

  } catch (const std::exception &e) {
    SetError("로드밸런싱 활성화 실패: " + std::string(e.what()));
    return false;
  }
}

void MqttDriver::DisableLoadBalancing() {
  if (load_balancer_) {
    load_balancer_->EnableLoadBalancing(false);
    load_balancer_->EnableLoadMonitoring(false);
  }
  load_balancer_.reset();

  LogMessage("INFO", "MQTT 로드밸런싱 비활성화됨", "MQTT");

  // 통계 업데이트
  driver_statistics_.IncrementProtocolCounter("loadbalancer_deactivations", 1);
}

bool MqttDriver::IsLoadBalancingEnabled() const {
  return load_balancer_ && load_balancer_->IsLoadBalancingEnabled();
}

std::string MqttDriver::GetLoadBalancingStatusJSON() const {
  if (load_balancer_) {
    return load_balancer_->GetStatusJSON();
  }

  return "{\"load_balancing_enabled\":false,\"broker_count\":0}";
}

std::string MqttDriver::SelectOptimalBroker(const std::string &topic,
                                            size_t message_size) {
  if (load_balancer_ && load_balancer_->IsLoadBalancingEnabled()) {
    return load_balancer_->SelectBroker(topic, message_size);
  }

  // 로드밸런싱이 비활성화된 경우 기본 브로커 반환
  return broker_url_;
}

// =============================================================================
// 6. 헬퍼 메서드들 구현
// =============================================================================

bool MqttDriver::SwitchBroker(const std::string &new_broker_url) {
  if (new_broker_url == broker_url_) {
    return true; // 이미 연결된 브로커
  }

  try {
    LogMessage("INFO",
               "브로커 전환 시도: " + broker_url_ + " → " + new_broker_url,
               "MQTT");

    // 현재 연결 종료
    if (IsConnected()) {
      mqtt_client_->disconnect()->wait_for(
          std::chrono::milliseconds(timeout_ms_));
      is_connected_.store(false);
    }

    // 새 브로커로 연결
    broker_url_ = new_broker_url;
    bool success = Connect();

    if (success) {
      LogMessage("INFO", "브로커 전환 성공: " + new_broker_url, "MQTT");
      driver_statistics_.IncrementProtocolCounter("broker_switches", 1);
    } else {
      LogMessage("ERROR", "브로커 전환 실패: " + new_broker_url, "MQTT");
    }

    return success;

  } catch (const std::exception &e) {
    SetError("브로커 전환 중 예외 발생: " + std::string(e.what()));
    return false;
  }
}

bool MqttDriver::CreateMqttClientForBroker(const std::string &broker_url) {
  try {
    // 기존 클라이언트 정리
    if (mqtt_client_) {
      mqtt_client_.reset();
    }

    // 새 클라이언트 생성
    mqtt_client_ = std::make_unique<mqtt::async_client>(broker_url, client_id_);

    // 콜백 설정
    if (!mqtt_callback_) {
      mqtt_callback_ = std::make_unique<MqttCallbackImpl>(this);
    }
    mqtt_client_->set_callback(*mqtt_callback_);

    LogMessage("DEBUG", "MQTT 클라이언트 생성 완료: " + broker_url, "MQTT");
    return true;

  } catch (const std::exception &e) {
    SetError("MQTT 클라이언트 생성 실패: " + std::string(e.what()));
    return false;
  }
}

#else

// =============================================================================
// STUB IMPLEMENTATION (When HAS_MQTT_CPP is not defined)
// =============================================================================

MqttDriver::MqttDriver()
    : driver_statistics_("MQTT"), status_(Structs::DriverStatus::UNINITIALIZED),
      is_connected_(false), load_balancer_(nullptr) {
  std::cout << "[DEBUG][MQTT] MqttDriver stub instance created (HAS_MQTT_CPP "
               "not defined)"
            << std::endl;
}
MqttDriver::~MqttDriver() {}

bool MqttDriver::Initialize(const DriverConfig &config) {
  (void)config;
  std::cerr << "[ERROR][MQTT] MqttDriver::Initialize failed: STUB "
               "implementation is being used. Please check if HAS_MQTT_CPP "
               "is defined and libmqtt-paho is linked."
            << std::endl;
  return false;
}
bool MqttDriver::Connect() { return false; }
bool MqttDriver::Disconnect() { return true; }
bool MqttDriver::IsConnected() const { return false; }
bool MqttDriver::ReadValues(const std::vector<DataPoint> &,
                            std::vector<TimestampedValue> &) {
  return false;
}
bool MqttDriver::WriteValue(const DataPoint &, const DataValue &) {
  return false;
}
const DriverStatistics &MqttDriver::GetStatistics() const {
  static DriverStatistics s("MQTT");
  return s;
}
void MqttDriver::ResetStatistics() {}
std::string MqttDriver::GetProtocolType() const { return "MQTT"; }
Structs::DriverStatus MqttDriver::GetStatus() const {
  return Structs::DriverStatus::STOPPED;
}
ErrorInfo MqttDriver::GetLastError() const { return ErrorInfo(); }

bool MqttDriver::Start() { return false; }
bool MqttDriver::Stop() { return true; }
bool MqttDriver::Subscribe(const std::string &, int) { return false; }
bool MqttDriver::Unsubscribe(const std::string &) { return false; }
bool MqttDriver::Publish(const std::string &, const std::string &, int, bool) {
  return false;
}

// Advanced features stubs
std::string MqttDriver::GetLoadBalancingStatusJSON() const { return "{}"; }
void MqttDriver::DisableLoadBalancing() {}
bool MqttDriver::EnableLoadBalancing(const std::vector<std::string> &,
                                     LoadBalanceAlgorithm) {
  return false;
}
bool MqttDriver::IsLoadBalancingEnabled() const { return false; }
std::string MqttDriver::SelectOptimalBroker(const std::string &, size_t) {
  return "";
}
bool MqttDriver::SwitchBroker(const std::string &) { return false; }
bool MqttDriver::CreateMqttClientForBroker(const std::string &) {
  return false;
}
void MqttDriver::NotifyAdvancedFeatures(const std::string &,
                                        const std::string &) {}
void MqttDriver::RecordDiagnosticEvent(const std::string &, bool, double,
                                       const std::string &) {}
void MqttDriver::NotifyConnectionChange(bool, const std::string &,
                                        const std::string &) {}
void MqttDriver::NotifyMessageProcessing(const std::string &,
                                         const std::string &, bool, double) {}
#endif

} // namespace Drivers
} // namespace PulseOne

// =============================================================================
// 🔥 Plugin Entry Point
// =============================================================================
#ifndef TEST_BUILD
extern "C" {
#ifdef _WIN32
__declspec(dllexport)
#endif
void RegisterPlugin() {
  // 1. DB에 프로토콜 정보 자동 등록 (없을 경우)
  try {
    auto &repo_factory = PulseOne::Database::RepositoryFactory::getInstance();
    auto protocol_repo = repo_factory.getProtocolRepository();
    if (protocol_repo) {
      if (!protocol_repo->findByType("MQTT").has_value()) {
        PulseOne::Database::Entities::ProtocolEntity entity;
        entity.setProtocolType("MQTT");
        entity.setDisplayName("MQTT Protocol");
        entity.setCategory("iot");
        entity.setDescription("MQTT v3.1.1/v5.0 Protocol Driver");
        entity.setRequiresBroker(true);
        entity.setDefaultPort(1883);
        protocol_repo->save(entity);
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "[MqttDriver] DB Registration failed: " << e.what()
              << std::endl;
  }

  // 2. 메모리 Factory에 드라이버 생성자 등록
  PulseOne::Drivers::DriverFactory::GetInstance().RegisterDriver("MQTT", []() {
    return std::make_unique<PulseOne::Drivers::MqttDriver>();
  });
  std::cout << "[MqttDriver] Plugin Registered Successfully" << std::endl;
}

// Legacy wrapper for static linking
void RegisterMqttDriver() { RegisterPlugin(); }
}
#endif