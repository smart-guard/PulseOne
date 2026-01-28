/**
 * @file MqttTargetHandler.h
 * @brief MQTT 타겟 핸들러 - Paho MQTT C++ 라이브러리 기반
 * @author PulseOne Development Team
 * @date 2025-12-03
 * @version 2.1.0 - Paho MQTT C++ (async_client) 사용
 *
 * @note Collector의 MqttDriver와 동일한 Paho C++ 라이브러리 사용
 */

#ifndef MQTT_TARGET_HANDLER_H
#define MQTT_TARGET_HANDLER_H

#include "Export/ExportTypes.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

// Paho MQTT C++ 라이브러리 조건부 포함
#ifdef HAS_PAHO_MQTT
#include <mqtt/async_client.h>
#include <mqtt/callback.h>
#include <mqtt/connect_options.h>
#include <mqtt/message.h>
#include <mqtt/ssl_options.h>
#include <mqtt/token.h>
#define MQTT_AVAILABLE 1
#else
#define MQTT_AVAILABLE 0
#endif

namespace PulseOne {
namespace CSP {

// =============================================================================
// MQTT 상수 (Export Gateway 전용)
// =============================================================================
namespace MqttConstants {
constexpr int DEFAULT_TCP_PORT = 1883;
constexpr int DEFAULT_SSL_PORT = 8883;
constexpr int DEFAULT_CONNECT_TIMEOUT_SEC = 30;
constexpr int DEFAULT_PUBLISH_TIMEOUT_SEC = 10;
constexpr int DEFAULT_KEEPALIVE_SEC = 60;
constexpr int DEFAULT_RECONNECT_INTERVAL_SEC = 5;
constexpr int DEFAULT_MAX_RECONNECT_ATTEMPTS = -1; // 무제한
constexpr size_t DEFAULT_MAX_QUEUE_SIZE = 1000;
constexpr int MAX_CLIENT_ID_LENGTH = 23; // MQTT 3.1.1 제한
} // namespace MqttConstants

// 전방 선언
class MqttPublisherCallback;

/**
 * @brief MQTT 타겟 핸들러 - Paho MQTT C++ 사용 (Publisher 전용)
 *
 * Collector의 MqttDriver와 동일한 Paho C++ 라이브러리 사용
 *
 * 지원 기능:
 * - MQTT 3.1.1 및 5.0 프로토콜
 * - TCP/SSL 연결 지원
 * - QoS 0, 1, 2 지원
 * - Retained 메시지 지원
 * - 자동 재연결
 * - 토픽 템플릿 지원
 * - 사용자명/암호 인증
 * - SSL/TLS 인증서 인증
 * - Keep-alive 및 하트비트
 * - 메시지 큐잉 (연결 끊김 시)
 * - 비동기 발행 (async_client)
 */
class MqttTargetHandler : public ITargetHandler {
private:
#if MQTT_AVAILABLE
  // Paho MQTT C++ 클라이언트
  std::unique_ptr<mqtt::async_client> mqtt_client_;
  std::shared_ptr<MqttPublisherCallback> mqtt_callback_;
#endif

  // 연결 정보
  std::string broker_uri_;
  std::string client_id_;

  // 뮤텍스
  mutable std::mutex client_mutex_; // MQTT 클라이언트 보호용
  mutable std::mutex queue_mutex_;  // 메시지 큐 보호용
  std::condition_variable queue_cv_;

  // 상태 플래그
  std::atomic<bool> is_connected_{false};
  std::atomic<bool> is_connecting_{false};
  std::atomic<bool> is_initialized_{false};

  // 통계
  std::atomic<size_t> publish_count_{0};
  std::atomic<size_t> success_count_{0};
  std::atomic<size_t> failure_count_{0};
  std::atomic<size_t> connection_attempts_{0};

  // 연결 끊김 시 메시지 큐
  std::queue<std::pair<std::string, std::string>>
      message_queue_; // topic, payload
  size_t max_queue_size_ = MqttConstants::DEFAULT_MAX_QUEUE_SIZE;

  // 재연결 스레드
  std::unique_ptr<std::thread> reconnect_thread_;
  std::atomic<bool> should_stop_{false};

  // 설정 캐시 (재연결용)
  ordered_json cached_config_;

  // 타임아웃 설정
  int connect_timeout_ms_ = MqttConstants::DEFAULT_CONNECT_TIMEOUT_SEC * 1000;
  int publish_timeout_ms_ = MqttConstants::DEFAULT_PUBLISH_TIMEOUT_SEC * 1000;
  int keep_alive_sec_ = MqttConstants::DEFAULT_KEEPALIVE_SEC;
  bool clean_session_ = true;
  bool auto_reconnect_ = true;
  int default_qos_ = 1;

public:
  /**
   * @brief 생성자
   */
  MqttTargetHandler();

  /**
   * @brief 소멸자
   */
  ~MqttTargetHandler() override;

  // 복사/이동 생성자 비활성화
  MqttTargetHandler(const MqttTargetHandler &) = delete;
  MqttTargetHandler &operator=(const MqttTargetHandler &) = delete;
  MqttTargetHandler(MqttTargetHandler &&) = delete;
  MqttTargetHandler &operator=(MqttTargetHandler &&) = delete;

  // =======================================================================
  // ITargetHandler 인터페이스 구현
  // =======================================================================

  bool initialize(const ordered_json &config) override;
  TargetSendResult sendAlarm(const AlarmMessage &alarm,
                             const ordered_json &config) override;

  /**
   * @brief 주기적 데이터 배치 전송
   */
  std::vector<TargetSendResult>
  sendValueBatch(const std::vector<ValueMessage> &values,
                 const ordered_json &config) override;

  bool testConnection(const ordered_json &config) override;
  std::string getHandlerType() const override { return "MQTT"; }
  ordered_json getStatus() const override;
  void cleanup() override;
  bool validateConfig(const ordered_json &config,
                      std::vector<std::string> &errors) override;

  // =======================================================================
  // 추가 공개 메서드
  // =======================================================================

  /**
   * @brief MQTT 라이브러리 가용성 확인
   */
  static bool isMqttAvailable();

  /**
   * @brief 현재 연결 상태
   */
  bool isConnected() const { return is_connected_.load(); }

  /**
   * @brief 큐에 대기 중인 메시지 수
   */
  size_t getQueuedMessageCount() const;

  // =======================================================================
  // 콜백 핸들러 (MqttPublisherCallback에서 호출)
  // =======================================================================

#if MQTT_AVAILABLE
  void onConnected(const std::string &cause);
  void onConnectionLost(const std::string &cause);
  void onDeliveryComplete(mqtt::delivery_token_ptr token);
#endif

private:
  // =======================================================================
  // 내부 구현 메서드
  // =======================================================================

  bool connectToBroker(const ordered_json &config);
  void disconnectFromBroker();
  bool initializeMqttClient();

  std::string generateTopic(const AlarmMessage &alarm,
                            const ordered_json &config) const;
  std::string generatePayload(const AlarmMessage &alarm,
                              const ordered_json &config) const;

  TargetSendResult publishMessage(const std::string &topic,
                                  const std::string &payload, int qos,
                                  bool retain);

  std::string generateClientId(const std::string &base_id = "") const;
  void reconnectThread(ordered_json config);
  void processQueuedMessages();
  bool enqueueMessage(const std::string &topic, const std::string &payload);

  std::string expandTemplateVariables(const std::string &template_str,
                                      const AlarmMessage &alarm) const;

  void expandTemplateVariables(ordered_json &template_json,
                               const AlarmMessage &alarm) const;

  void expandTemplateVariables(ordered_json &template_json,
                               const ValueMessage &value) const;
  std::string createJsonMessage(const AlarmMessage &alarm,
                                const ordered_json &config) const;
  std::string createTextMessage(const AlarmMessage &alarm,
                                const ordered_json &config) const;
};

// =============================================================================
// MQTT 콜백 클래스 (Collector의 MqttCallbackImpl과 동일 패턴)
// =============================================================================

#if MQTT_AVAILABLE
/**
 * @brief MQTT 콜백 핸들러 (Publisher 전용)
 */
class MqttPublisherCallback : public virtual mqtt::callback {
public:
  explicit MqttPublisherCallback(MqttTargetHandler *handler)
      : handler_(handler) {}

  void connected(const std::string &cause) override {
    if (handler_) {
      handler_->onConnected(cause);
    }
  }

  void connection_lost(const std::string &cause) override {
    if (handler_) {
      handler_->onConnectionLost(cause);
    }
  }

  void message_arrived(mqtt::const_message_ptr msg) override {
    // Publisher는 메시지 수신하지 않음 (무시)
    (void)msg;
  }

  void delivery_complete(mqtt::delivery_token_ptr token) override {
    if (handler_) {
      handler_->onDeliveryComplete(token);
    }
  }

private:
  MqttTargetHandler *handler_;
};
#endif

} // namespace CSP
} // namespace PulseOne

#endif // MQTT_TARGET_HANDLER_H