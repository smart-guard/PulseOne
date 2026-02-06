/**
 * @file MqttTargetHandler.cpp
 * @brief MQTT 타겟 핸들러 구현 - Paho MQTT C++ (async_client) 사용
 * @author PulseOne Development Team
 * @date 2025-12-03
 * @version 2.2.0 - 데드락 수정 버전
 */

#include "CSP/MqttTargetHandler.h"
#include "Constants/ExportConstants.h"
#include "Logging/LogManager.h"
#include "Transform/PayloadTransformer.h"
#include "Utils/ConfigManager.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <random>
#include <regex>
#include <sstream>

namespace PulseOne {
namespace CSP {

// using json = nlohmann::json;
// using json = nlohmann::json;
using namespace std::chrono;

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

MqttTargetHandler::MqttTargetHandler() {
  LogManager::getInstance().Info(
      "MqttTargetHandler 초기화 (Paho MQTT C++: " +
      std::string(isMqttAvailable() ? "활성화" : "Mock 모드") + ")");
}

MqttTargetHandler::~MqttTargetHandler() {
  cleanup();
  LogManager::getInstance().Info("MqttTargetHandler 종료");
}

// =============================================================================
// 정적 메서드
// =============================================================================

bool MqttTargetHandler::isMqttAvailable() {
#if MQTT_AVAILABLE
  return true;
#else
  return false;
#endif
}

size_t MqttTargetHandler::getQueuedMessageCount() const {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  return message_queue_.size();
}

// =============================================================================
// ITargetHandler 인터페이스 구현
// =============================================================================

bool MqttTargetHandler::initialize(const json &config) {
  try {
    LogManager::getInstance().Info("MQTT 타겟 핸들러 초기화 시작");

    // 설정 검증
    std::vector<std::string> errors;
    if (!validateConfig(config, errors)) {
      for (const auto &error : errors) {
        LogManager::getInstance().Error("초기화 검증 실패: " + error);
      }
      return false;
    }

    // 설정 캐시 (재연결용)
    cached_config_ = config;

    // 브로커 연결 정보 파싱
    std::string broker_host = ConfigManager::getInstance().expandVariables(
        config["broker_host"].get<std::string>());
    int broker_port =
        config.value("broker_port", MqttConstants::DEFAULT_TCP_PORT);
    bool ssl_enabled = config.value("ssl_enabled", false);

    // SSL 사용 시 기본 포트 변경
    if (ssl_enabled && broker_port == MqttConstants::DEFAULT_TCP_PORT) {
      broker_port = MqttConstants::DEFAULT_SSL_PORT;
    }

    // 브로커 URI 생성
    std::string protocol = ssl_enabled ? "ssl://" : "tcp://";
    broker_uri_ = protocol + broker_host + ":" + std::to_string(broker_port);

    LogManager::getInstance().Info("MQTT 브로커 URI: " + broker_uri_);

    // 클라이언트 ID 생성
    std::string base_id = config.value("client_id", "");
    client_id_ = generateClientId(base_id);

    // 타임아웃 설정
    connect_timeout_ms_ =
        config.value("connect_timeout_sec",
                     MqttConstants::DEFAULT_CONNECT_TIMEOUT_SEC) *
        1000;
    publish_timeout_ms_ =
        config.value("publish_timeout_sec",
                     MqttConstants::DEFAULT_PUBLISH_TIMEOUT_SEC) *
        1000;
    keep_alive_sec_ =
        config.value("keep_alive_sec", MqttConstants::DEFAULT_KEEPALIVE_SEC);
    clean_session_ = config.value("clean_session", true);
    auto_reconnect_ = config.value("auto_reconnect", true);
    default_qos_ = config.value("qos", 1);

    // 메시지 큐 크기 설정
    max_queue_size_ =
        config.value("max_queue_size", MqttConstants::DEFAULT_MAX_QUEUE_SIZE);

    // MQTT 클라이언트 초기화
    if (!initializeMqttClient()) {
      LogManager::getInstance().Error("MQTT 클라이언트 초기화 실패");
      return false;
    }

    is_initialized_ = true;

    // 브로커 연결 시도
    bool auto_connect = config.value("auto_connect", true);
    if (auto_connect) {
      if (!connectToBroker(config)) {
        LogManager::getInstance().Warn(
            "초기 MQTT 연결 실패 - 자동 재연결 활성화됨");
      }
    }

    // 자동 재연결 스레드 시작
    if (auto_reconnect_) {
      should_stop_ = false;
      reconnect_thread_ = std::make_unique<std::thread>(
          &MqttTargetHandler::reconnectThread, this, config);
      LogManager::getInstance().Info("MQTT 자동 재연결 스레드 시작");
    }

    LogManager::getInstance().Info(
        "MQTT 타겟 핸들러 초기화 완료 - Client ID: " + client_id_);
    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("MQTT 타겟 핸들러 초기화 실패: " +
                                    std::string(e.what()));
    return false;
  }
}

bool MqttTargetHandler::validateConfig(const json &config,
                                       std::vector<std::string> &errors) {
  errors.clear();

  // 필수 필드: broker_host
  if (!config.contains("broker_host")) {
    errors.push_back("broker_host 필드가 필수입니다");
    return false;
  }

  std::string broker_host = config["broker_host"].get<std::string>();
  if (broker_host.empty()) {
    errors.push_back("broker_host가 비어있습니다");
    return false;
  }

  // broker_port 검증 (선택사항)
  if (config.contains("broker_port")) {
    int port = config["broker_port"].get<int>();
    if (port <= 0 || port > 65535) {
      errors.push_back("broker_port는 1-65535 범위여야 합니다");
      return false;
    }
  }

  // QoS 검증 (선택사항)
  if (config.contains("qos")) {
    int qos = config["qos"].get<int>();
    if (qos < 0 || qos > 2) {
      errors.push_back("qos는 0, 1, 2 중 하나여야 합니다");
      return false;
    }
  }

  return true;
}

TargetSendResult MqttTargetHandler::sendAlarm(const AlarmMessage &alarm,
                                              const json &config) {
  TargetSendResult result;
  result.target_type = "MQTT";
  result.target_name = broker_uri_;
  result.success = false;

  auto start_time = steady_clock::now();

  try {
    // 토픽 생성
    std::string topic = generateTopic(alarm, config);

    // 페이로드 생성
    std::string payload;
    if (config.contains("body_template") &&
        config["body_template"].is_object()) {
      json payload_json = config["body_template"];
      expandTemplateVariables(payload_json, alarm);
      payload = payload_json.dump();
    } else {
      payload = generatePayload(alarm, config);
    }

    // QoS 및 Retain 설정
    int qos = config.value("qos", default_qos_);
    bool retain = config.value("retain", false);

    LogManager::getInstance().Debug("MQTT 발행 준비 - Topic: " + topic);

    // 연결 상태 확인 및 발행
    if (is_connected_.load()) {
      result = publishMessage(topic, payload, qos, retain);
    } else {
      // 연결되지 않은 경우 메시지 큐에 저장
      if (enqueueMessage(topic, payload)) {
        result.success = false;
        result.error_message = "MQTT 연결 끊김 - 메시지를 큐에 저장";
        LogManager::getInstance().Warn("MQTT 연결 끊김 - 메시지 큐에 저장: " +
                                       topic);
      } else {
        result.success = false;
        result.error_message = "MQTT 연결 끊김 및 큐 저장 실패";
        LogManager::getInstance().Error("MQTT 메시지 큐 저장 실패: " + topic);
      }
    }

    auto end_time = steady_clock::now();
    result.response_time = duration_cast<milliseconds>(end_time - start_time);
    result.mqtt_topic = topic;
    result.content_size = payload.length();

    // 통계 업데이트
    publish_count_++;
    if (result.success) {
      success_count_++;
    } else {
      failure_count_++;
    }

  } catch (const std::exception &e) {
    result.error_message = "MQTT 발행 예외: " + std::string(e.what());
    LogManager::getInstance().Error(result.error_message);
    failure_count_++;
  }

  return result;
}

bool MqttTargetHandler::testConnection(const json &config) {
  try {
    LogManager::getInstance().Info("MQTT 연결 테스트 시작");

    // 설정 검증
    std::vector<std::string> errors;
    if (!validateConfig(config, errors)) {
      for (const auto &error : errors) {
        LogManager::getInstance().Error("MQTT 연결 테스트 실패: " + error);
      }
      return false;
    }

    if (is_connected_.load()) {
      // 테스트 메시지 발행
      std::string test_topic = "test/" + client_id_ + "/connection";
      json test_payload = {
          {"test", true},
          {"timestamp", PulseOne::Export::getCurrentTimestamp()},
          {"broker", broker_uri_},
          {"client_id", client_id_}};

      auto result = publishMessage(test_topic, test_payload.dump(), 0, false);

      if (result.success) {
        LogManager::getInstance().Info("MQTT 연결 테스트 성공");
        return true;
      } else {
        LogManager::getInstance().Error("MQTT 테스트 메시지 발행 실패: " +
                                        result.error_message);
        return false;
      }
    } else {
      LogManager::getInstance().Info("MQTT 연결 시도");
      return connectToBroker(config);
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("MQTT 연결 테스트 예외: " +
                                    std::string(e.what()));
    return false;
  }
}

json MqttTargetHandler::getStatus() const {
  std::lock_guard<std::mutex> lock(queue_mutex_);

  return json{{"type", "MQTT"},
              {"broker_uri", broker_uri_},
              {"client_id", client_id_},
              {"connected", is_connected_.load()},
              {"connecting", is_connecting_.load()},
              {"initialized", is_initialized_.load()},
              {"mqtt_available", isMqttAvailable()},
              {"publish_count", publish_count_.load()},
              {"success_count", success_count_.load()},
              {"failure_count", failure_count_.load()},
              {"queue_size", message_queue_.size()},
              {"max_queue_size", max_queue_size_},
              {"connection_attempts", connection_attempts_.load()}};
}

void MqttTargetHandler::cleanup() {
  LogManager::getInstance().Info("MqttTargetHandler 정리 시작");

  // 재연결 스레드 중지
  should_stop_ = true;
  if (reconnect_thread_ && reconnect_thread_->joinable()) {
    reconnect_thread_->join();
    reconnect_thread_.reset();
  }

  // MQTT 클라이언트 연결 해제
  disconnectFromBroker();

  // 메시지 큐 정리
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!message_queue_.empty()) {
      message_queue_.pop();
    }
  }

  is_initialized_ = false;

  LogManager::getInstance().Info("MqttTargetHandler 정리 완료");
}

// =============================================================================
// MQTT 클라이언트 초기화
// =============================================================================

bool MqttTargetHandler::initializeMqttClient() {
#if MQTT_AVAILABLE
  try {
    LogManager::getInstance().Info("MQTT 클라이언트 초기화 - URI: " +
                                   broker_uri_ + ", Client ID: " + client_id_);

    // async_client 생성
    mqtt_client_ =
        std::make_unique<mqtt::async_client>(broker_uri_, client_id_);

    // 콜백 설정
    mqtt_callback_ = std::make_shared<MqttPublisherCallback>(this);
    mqtt_client_->set_callback(*mqtt_callback_);

    LogManager::getInstance().Info("MQTT 클라이언트 초기화 완료");
    return true;

  } catch (const mqtt::exception &e) {
    LogManager::getInstance().Error("MQTT 클라이언트 생성 실패: " +
                                    std::string(e.what()));
    return false;
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("MQTT 클라이언트 초기화 예외: " +
                                    std::string(e.what()));
    return false;
  }
#else
  // Mock 모드
  LogManager::getInstance().Info("MQTT Mock 모드 - 클라이언트 초기화 스킵");
  return true;
#endif
}

// =============================================================================
// 브로커 연결 (데드락 수정 버전)
// =============================================================================

bool MqttTargetHandler::connectToBroker(const json &config) {
  bool should_process_queue = false;

  {
    std::lock_guard<std::mutex> lock(client_mutex_);

    if (is_connected_.load()) {
      return true;
    }

    if (is_connecting_.load()) {
      return false;
    }

    is_connecting_ = true;
    connection_attempts_++;

    auto start_time = steady_clock::now();
    (void)start_time; // Suppress unused warning in Mock mode

    try {
      LogManager::getInstance().Info(
          "MQTT 브로커 연결 시도: " + broker_uri_ + " (시도 #" +
          std::to_string(connection_attempts_.load()) + ")");

#if MQTT_AVAILABLE
      if (!mqtt_client_) {
        LogManager::getInstance().Error("MQTT 클라이언트가 초기화되지 않음");
        is_connecting_ = false;
        return false;
      }

      // 연결 옵션 설정
      mqtt::connect_options connOpts;
      connOpts.set_keep_alive_interval(keep_alive_sec_);
      connOpts.set_clean_session(clean_session_);
      connOpts.set_automatic_reconnect(false);

      // 인증 설정
      std::string username = ConfigManager::getInstance().expandVariables(
          config.value("username", ""));
      std::string password = ConfigManager::getInstance().expandVariables(
          config.value("password", ""));

      if (!username.empty()) {
        connOpts.set_user_name(username);
        if (!password.empty()) {
          connOpts.set_password(password);
        }
      }

      // SSL 설정
      bool ssl_enabled = config.value("ssl_enabled", false);
      if (ssl_enabled) {
        mqtt::ssl_options sslOpts;

        std::string ca_file = config.value("ssl_ca_file", "");
        std::string cert_file = config.value("ssl_cert_file", "");
        std::string key_file = config.value("ssl_key_file", "");

        if (!ca_file.empty()) {
          sslOpts.set_trust_store(ca_file);
        }
        if (!cert_file.empty()) {
          sslOpts.set_key_store(cert_file);
        }
        if (!key_file.empty()) {
          sslOpts.set_private_key(key_file);
        }

        sslOpts.set_enable_server_cert_auth(
            config.value("ssl_verify_peer", true));
        connOpts.set_ssl(sslOpts);
      }

      LogManager::getInstance().Debug(
          "MQTT 연결 옵션 - Keep Alive: " + std::to_string(keep_alive_sec_) +
          "s, Clean Session: " + (clean_session_ ? "true" : "false") +
          ", SSL: " + (ssl_enabled ? "enabled" : "disabled"));

      // 연결 시도
      auto token = mqtt_client_->connect(connOpts);
      bool success =
          token->wait_for(std::chrono::milliseconds(connect_timeout_ms_));

      auto end_time = steady_clock::now();
      double duration_ms =
          duration_cast<milliseconds>(end_time - start_time).count();

      if (success && mqtt_client_->is_connected()) {
        is_connected_ = true;
        is_connecting_ = false;

        LogManager::getInstance().Info("MQTT 브로커 연결 성공: " + broker_uri_ +
                                       " (" + std::to_string(duration_ms) +
                                       "ms)");

        should_process_queue = true; // 락 밖에서 처리
      } else {
        is_connecting_ = false;
        LogManager::getInstance().Error("MQTT 브로커 연결 실패 (타임아웃)");
      }

#else
      // Mock 모드
      (void)config;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));

      if (broker_uri_.empty() ||
          broker_uri_.find("nonexistent") != std::string::npos ||
          broker_uri_.find("invalid") != std::string::npos ||
          broker_uri_.find("0.0.0.0:0") != std::string::npos) {
        is_connected_ = false;
        is_connecting_ = false;
        LogManager::getInstance().Error("MQTT 브로커 연결 실패 (Mock): " +
                                        broker_uri_);
      } else {
        is_connected_ = true;
        is_connecting_ = false;
        LogManager::getInstance().Info("MQTT 브로커 연결 성공 (Mock): " +
                                       broker_uri_);
        should_process_queue = true;
      }
#endif

    } catch (const std::exception &e) {
      is_connected_ = false;
      is_connecting_ = false;
      LogManager::getInstance().Error("MQTT 브로커 연결 예외: " +
                                      std::string(e.what()));
    }
  }
  // client_mutex_ 해제됨

  // 큐된 메시지 처리 (락 밖에서)
  if (should_process_queue) {
    processQueuedMessages();
  }

  return is_connected_.load();
}

void MqttTargetHandler::disconnectFromBroker() {
  std::lock_guard<std::mutex> lock(client_mutex_);

#if MQTT_AVAILABLE
  if (mqtt_client_ && mqtt_client_->is_connected()) {
    try {
      LogManager::getInstance().Info("MQTT 브로커 연결 해제 시작");

      auto token = mqtt_client_->disconnect();
      token->wait_for(std::chrono::milliseconds(1000));

      LogManager::getInstance().Info("MQTT 브로커 연결 해제 완료");
    } catch (const std::exception &e) {
      LogManager::getInstance().Warn("MQTT 연결 해제 중 예외: " +
                                     std::string(e.what()));
    }
  }

  mqtt_client_.reset();
  mqtt_callback_.reset();
#endif

  is_connected_ = false;
}

// =============================================================================
// 메시지 발행
// =============================================================================

TargetSendResult MqttTargetHandler::publishMessage(const std::string &topic,
                                                   const std::string &payload,
                                                   int qos, bool retain) {
  TargetSendResult result;
  result.target_type = "MQTT";
  result.target_name = broker_uri_;
  result.mqtt_topic = topic;
  result.content_size = payload.length();
  result.success = false;

  auto start_time = steady_clock::now();

  try {
    if (!is_connected_.load()) {
      result.error_message = "MQTT 클라이언트가 연결되지 않음";
      return result;
    }

#if MQTT_AVAILABLE
    std::lock_guard<std::mutex> lock(client_mutex_);

    if (!mqtt_client_ || !mqtt_client_->is_connected()) {
      result.error_message = "MQTT 클라이언트가 null이거나 연결 끊김";
      return result;
    }

    // 메시지 생성
    mqtt::message_ptr msg = mqtt::make_message(topic, payload);
    msg->set_qos(qos);
    msg->set_retained(retain);

    // 발행 및 대기
    mqtt::delivery_token_ptr delivery_token = mqtt_client_->publish(msg);
    bool success = delivery_token->wait_for(
        std::chrono::milliseconds(publish_timeout_ms_));

    auto end_time = steady_clock::now();
    result.response_time = duration_cast<milliseconds>(end_time - start_time);

    if (success) {
      result.success = true;
      LogManager::getInstance().Debug(
          "MQTT 발행 성공 - Topic: " + topic +
          ", Size: " + std::to_string(payload.length()) + " bytes" +
          ", QoS: " + std::to_string(qos) +
          ", Time: " + std::to_string(result.response_time.count()) + "ms");
    } else {
      result.error_message = "MQTT 발행 타임아웃";
      LogManager::getInstance().Warn("MQTT 발행 타임아웃 - Topic: " + topic);
    }

#else
    // Mock 모드
    (void)qos;
    (void)retain;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    result.success = true;

    auto end_time = steady_clock::now();
    result.response_time = duration_cast<milliseconds>(end_time - start_time);

    LogManager::getInstance().Debug("MQTT 발행 성공 (Mock) - Topic: " + topic);
#endif

  } catch (const std::exception &e) {
    result.error_message = "MQTT 발행 예외: " + std::string(e.what());
    LogManager::getInstance().Error(result.error_message);
  }

  return result;
}

// =============================================================================
// 콜백 핸들러 (데드락 수정 - 큐 처리 제거)
// =============================================================================

#if MQTT_AVAILABLE
void MqttTargetHandler::onConnected(const std::string &cause) {
  is_connected_ = true;
  is_connecting_ = false;

  LogManager::getInstance().Info("MQTT 연결됨: " + cause);

  // 콜백에서는 큐 처리하지 않음 - connectToBroker()에서 처리
}

void MqttTargetHandler::onConnectionLost(const std::string &cause) {
  is_connected_ = false;

  LogManager::getInstance().Warn("MQTT 연결 끊김: " + cause);
}

void MqttTargetHandler::onDeliveryComplete(mqtt::delivery_token_ptr token) {
  (void)token;
  LogManager::getInstance().Debug("MQTT 전송 완료");
}
#endif

// =============================================================================
// 유틸리티 메서드
// =============================================================================

std::string
MqttTargetHandler::generateClientId(const std::string &base_id) const {
  std::string prefix = base_id.empty() ? "pulseone_csp" : base_id;

  // 타임스탬프 + 랜덤 suffix 생성
  auto now = std::chrono::system_clock::now();
  auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch())
                    .count();

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(1000, 9999);
  int random_suffix = dis(gen);

  std::string client_id = prefix + "_" + std::to_string(millis % 100000) + "_" +
                          std::to_string(random_suffix);

  // MQTT 3.1.1 클라이언트 ID 길이 제한 (23자)
  if (client_id.length() > MqttConstants::MAX_CLIENT_ID_LENGTH) {
    client_id = client_id.substr(0, MqttConstants::MAX_CLIENT_ID_LENGTH);
  }

  return client_id;
}

std::string MqttTargetHandler::generateTopic(const AlarmMessage &alarm,
                                             const json &config) const {
  std::string topic_pattern =
      config.value("topic_pattern", "alarms/{building_id}/{nm}");
  return expandTemplateVariables(topic_pattern, alarm);
}

std::string MqttTargetHandler::generatePayload(const AlarmMessage &alarm,
                                               const json &config) const {
  std::string format = config.value("message_format", "json");

  if (format == "json") {
    return createJsonMessage(alarm, config);
  } else if (format == "text") {
    return createTextMessage(alarm, config);
  }

  return createJsonMessage(alarm, config);
}

std::vector<TargetSendResult>
MqttTargetHandler::sendValueBatch(const std::vector<ValueMessage> &values,
                                  const json &config) {

  std::vector<TargetSendResult> results;
  if (values.empty())
    return results;

  std::string topic_template = config.value("topic", "icos/values/{bd}");

  for (const auto &val : values) {
    TargetSendResult res;
    res.target_type = "MQTT";
    res.target_name = config.value("name", "Unknown");
    res.success = false;

    // 토픽 생성 (bd 치환)
    std::string topic = topic_template;
    topic = std::regex_replace(topic, std::regex("\\{bd\\}"),
                               std::to_string(val.bd));
    topic = std::regex_replace(topic, std::regex("\\{building_id\\}"),
                               std::to_string(val.bd));

    // 페이로드 생성
    std::string payload;
    if (config.contains("body_template") &&
        config["body_template"].is_object()) {
      json payload_json = config["body_template"];
      expandTemplateVariables(payload_json, val);
      payload = payload_json.dump();
    } else {
      payload = val.to_json().dump();
    }

    int qos = config.value("qos", default_qos_);
    bool retain = config.value("retain", false);

    res = publishMessage(topic, payload, qos, retain);
    results.push_back(res);
  }

  return results;
}

std::string
MqttTargetHandler::expandTemplateVariables(const std::string &template_str,
                                           const AlarmMessage &alarm) const {
  std::string result = template_str;

  result = std::regex_replace(result, std::regex("\\{building_id\\}"),
                              std::to_string(alarm.bd));
  result = std::regex_replace(result, std::regex("\\{bd\\}"),
                              std::to_string(alarm.bd));
  result = std::regex_replace(result, std::regex("\\{nm\\}"), alarm.nm);
  result = std::regex_replace(result, std::regex("\\{point_name\\}"), alarm.nm);
  result = std::regex_replace(result, std::regex("\\{value\\}"),
                              std::to_string(alarm.vl));
  result = std::regex_replace(result, std::regex("\\{vl\\}"),
                              std::to_string(alarm.vl));
  result = std::regex_replace(result, std::regex("\\{al\\}"),
                              std::to_string(alarm.al));
  result = std::regex_replace(result, std::regex("\\{st\\}"),
                              std::to_string(alarm.st));
  result =
      std::regex_replace(result, std::regex("\\{client_id\\}"), client_id_);

  // 특수 문자 정리
  result = std::regex_replace(result, std::regex("[^a-zA-Z0-9/_.-]"), "_");

  return result;
}

std::string MqttTargetHandler::createJsonMessage(const AlarmMessage &alarm,
                                                 const json &config) const {
  json message;

  message["bd"] = alarm.bd;
  message["nm"] = alarm.nm;
  message["vl"] = alarm.vl;
  message["tm"] = alarm.tm;
  message["al"] = alarm.al;
  message["st"] = alarm.st;
  message["des"] = alarm.des;

  if (config.value("include_metadata", true)) {
    message["source"] = "PulseOne-CSPGateway";
    message["version"] = "2.2";
    message["client_id"] = client_id_;
    message["timestamp"] =
        duration_cast<milliseconds>(system_clock::now().time_since_epoch())
            .count();
  }

  return message.dump();
}

std::string MqttTargetHandler::createTextMessage(const AlarmMessage &alarm,
                                                 const json &config) const {
  std::ostringstream text;
  std::string format = config.value("text_format", "default");

  if (format == "simple") {
    text << alarm.nm << "=" << alarm.vl;
  } else if (format == "detailed") {
    text << "[" << alarm.bd << "] " << alarm.nm << " = " << alarm.vl << " | "
         << alarm.des;
  } else {
    text << "Building " << alarm.bd << " - " << alarm.nm << ": " << alarm.vl;
  }

  return text.str();
}

// =============================================================================
// 메시지 큐 관리
// =============================================================================

bool MqttTargetHandler::enqueueMessage(const std::string &topic,
                                       const std::string &payload) {
  std::lock_guard<std::mutex> lock(queue_mutex_);

  if (message_queue_.size() >= max_queue_size_) {
    LogManager::getInstance().Warn(
        "MQTT 메시지 큐 가득참 - 오래된 메시지 제거");
    message_queue_.pop();
  }

  message_queue_.emplace(topic, payload);
  queue_cv_.notify_one();

  return true;
}

// 데드락 수정 버전 - 락을 먼저 해제하고 발행
void MqttTargetHandler::processQueuedMessages() {
  // 큐에서 메시지를 먼저 꺼내고, 락 해제 후 발행
  std::vector<std::pair<std::string, std::string>> messages_to_send;

  {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (message_queue_.empty()) {
      return;
    }

    LogManager::getInstance().Info(
        "큐된 MQTT 메시지 처리 시작: " + std::to_string(message_queue_.size()) +
        "개");

    while (!message_queue_.empty()) {
      messages_to_send.push_back(message_queue_.front());
      message_queue_.pop();
    }
  }
  // 여기서 queue_mutex_ 해제됨

  size_t processed = 0;
  size_t failed = 0;

  for (const auto &message : messages_to_send) {
    if (!is_connected_.load()) {
      // 연결 끊기면 남은 메시지 다시 큐에 넣기
      std::lock_guard<std::mutex> lock(queue_mutex_);
      for (size_t i = processed + failed; i < messages_to_send.size(); ++i) {
        message_queue_.push(messages_to_send[i]);
      }
      break;
    }

    auto result =
        publishMessage(message.first, message.second, default_qos_, false);

    if (result.success) {
      processed++;
    } else {
      failed++;
    }
  }

  LogManager::getInstance().Info(
      "큐된 MQTT 메시지 처리 완료 - 성공: " + std::to_string(processed) +
      ", 실패: " + std::to_string(failed));
}

// =============================================================================
// 자동 재연결 스레드
// =============================================================================

void MqttTargetHandler::reconnectThread(json config) {
  int reconnect_interval = config.value(
      "reconnect_interval_sec", MqttConstants::DEFAULT_RECONNECT_INTERVAL_SEC);
  int max_attempts = config.value(
      "max_reconnect_attempts", MqttConstants::DEFAULT_MAX_RECONNECT_ATTEMPTS);

  LogManager::getInstance().Info("MQTT 재연결 스레드 시작 (간격: " +
                                 std::to_string(reconnect_interval) + "초)");

  while (!should_stop_.load()) {
    for (int i = 0; i < reconnect_interval && !should_stop_.load(); ++i) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (should_stop_.load())
      break;

    if (!is_connected_.load() && !is_connecting_.load()) {
      if (max_attempts > 0 &&
          static_cast<int>(connection_attempts_.load()) >= max_attempts) {
        LogManager::getInstance().Warn("MQTT 최대 재연결 시도 횟수 초과");
        continue;
      }

      LogManager::getInstance().Info("MQTT 자동 재연결 시도");

      if (connectToBroker(config)) {
        LogManager::getInstance().Info("MQTT 자동 재연결 성공");
      }
    }
  }

  LogManager::getInstance().Info("MQTT 재연결 스레드 종료");
}

void MqttTargetHandler::expandTemplateVariables(
    json &template_json, const AlarmMessage &alarm) const {
  auto &transformer = Transform::PayloadTransformer::getInstance();
  auto context = transformer.createContext(alarm);

  if (template_json.is_object()) {
    for (auto it = template_json.begin(); it != template_json.end(); ++it) {
      if (it.value().is_string()) {
        std::string val = it.value().get<std::string>();
        it.value() = transformer.transformString(val, context);
      } else if (it.value().is_object() || it.value().is_array()) {
        expandTemplateVariables(it.value(), alarm);
      }
    }
  } else if (template_json.is_array()) {
    for (auto &item : template_json) {
      expandTemplateVariables(item, alarm);
    }
  }
}

void MqttTargetHandler::expandTemplateVariables(
    json &template_json, const ValueMessage &value) const {
  auto &transformer = Transform::PayloadTransformer::getInstance();
  auto context = transformer.createContext(value);

  if (template_json.is_object()) {
    for (auto it = template_json.begin(); it != template_json.end(); ++it) {
      if (it.value().is_string()) {
        std::string val = it.value().get<std::string>();
        it.value() = transformer.transformString(val, context);
      } else if (it.value().is_object() || it.value().is_array()) {
        expandTemplateVariables(it.value(), value);
      }
    }
  } else if (template_json.is_array()) {
    for (auto &item : template_json) {
      expandTemplateVariables(item, value);
    }
  }
}

REGISTER_TARGET_HANDLER(PulseOne::Constants::Export::TargetType::MQTT,
                        MqttTargetHandler);

} // namespace CSP
} // namespace PulseOne
