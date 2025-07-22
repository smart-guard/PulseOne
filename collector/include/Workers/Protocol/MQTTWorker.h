/**
 * @file MQTTWorker.h
 * @brief MQTT 프로토콜 전용 워커 클래스
 * @details TcpBasedWorker를 상속받아 MQTT Pub/Sub 통신 기능 제공
 * @author PulseOne Development Team
 * @date 2025-01-22
 * @version 1.0.0
 */

#ifndef WORKERS_PROTOCOL_MQTT_WORKER_H
#define WORKERS_PROTOCOL_MQTT_WORKER_H

#include "Workers/Base/TcpBasedWorker.h"
#include "Drivers/Common/CommonTypes.h"
#include <memory>
#include <queue>
#include <thread>
#include <atomic>
#include <map>
#include <chrono>
#include <shared_mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>

// Eclipse Paho MQTT C++ 헤더들
#include <mqtt/async_client.h>
#include <mqtt/callback.h>
#include <mqtt/iaction_listener.h>
#include <mqtt/connect_options.h>
#include <mqtt/message.h>
#include <mqtt/token.h>

namespace PulseOne {
namespace Workers {
namespace Protocol {

/**
 * @brief MQTT QoS 레벨
 */
enum class MqttQoS {
    AT_MOST_ONCE = 0,      ///< 최대 한번 전송 (Fire and forget)
    AT_LEAST_ONCE = 1,     ///< 최소 한번 전송 (ACK 필요)
    EXACTLY_ONCE = 2       ///< 정확히 한번 전송 (핸드셰이크)
};

/**
 * @brief MQTT 구독 정보
 */
struct MqttSubscription {
    uint32_t subscription_id;                    ///< 구독 고유 ID
    std::string topic;                           ///< MQTT 토픽
    MqttQoS qos;                                ///< QoS 레벨
    bool is_active;                             ///< 활성화 여부
    std::vector<Drivers::DataPoint> data_points; ///< 이 토픽과 연관된 데이터 포인트들
    
    // 통계 (atomic 대신 일반 변수 사용)
    uint64_t messages_received;                 ///< 수신된 메시지 수
    std::chrono::system_clock::time_point last_message_time; ///< 마지막 메시지 수신 시간
    
    MqttSubscription()
        : subscription_id(0), qos(MqttQoS::AT_MOST_ONCE), is_active(false)
        , messages_received(0), last_message_time(std::chrono::system_clock::now()) {}
};

/**
 * @brief MQTT 발행 정보
 */
struct MqttPublication {
    uint32_t publication_id;                     ///< 발행 고유 ID
    std::string topic;                           ///< MQTT 토픽
    MqttQoS qos;                                ///< QoS 레벨
    bool retain;                                ///< Retain 플래그
    int publish_interval_ms;                    ///< 발행 주기 (밀리초)
    bool is_active;                             ///< 활성화 여부
    
    std::vector<Drivers::DataPoint> data_points; ///< 이 토픽으로 발행할 데이터 포인트들
    
    // 실행 시간 추적
    std::chrono::system_clock::time_point last_publish_time;
    std::chrono::system_clock::time_point next_publish_time;
    
    // 통계 (atomic 대신 일반 변수 사용)
    uint64_t messages_published;                ///< 발행된 메시지 수
    uint64_t failed_publishes;                 ///< 발행 실패 수
    
    MqttPublication()
        : publication_id(0), qos(MqttQoS::AT_MOST_ONCE), retain(false)
        , publish_interval_ms(1000), is_active(false)
        , last_publish_time(std::chrono::system_clock::now())
        , next_publish_time(std::chrono::system_clock::now())
        , messages_published(0), failed_publishes(0) {}
};

/**
 * @brief MQTT 클라이언트 설정
 */
struct MqttClientConfig {
    std::string client_id;                      ///< 클라이언트 ID
    std::string username;                       ///< 사용자명 (선택사항)
    std::string password;                       ///< 패스워드 (선택사항)
    
    // 연결 설정
    int keep_alive_interval_seconds;            ///< Keep-alive 간격 (초)
    bool clean_session;                         ///< Clean Session 플래그
    int connection_timeout_seconds;             ///< 연결 타임아웃 (초)
    
    // SSL/TLS 설정
    bool use_ssl;                               ///< SSL/TLS 사용 여부
    std::string ca_cert_file;                   ///< CA 인증서 파일
    std::string client_cert_file;               ///< 클라이언트 인증서 파일
    std::string client_key_file;                ///< 클라이언트 키 파일
    
    // Will 메시지 설정
    bool use_will_message;                      ///< Will 메시지 사용 여부
    std::string will_topic;                     ///< Will 토픽
    std::string will_payload;                   ///< Will 페이로드
    MqttQoS will_qos;                          ///< Will QoS
    bool will_retain;                           ///< Will Retain 플래그
    
    MqttClientConfig()
        : keep_alive_interval_seconds(60)
        , clean_session(true)
        , connection_timeout_seconds(30)
        , use_ssl(false)
        , use_will_message(false)
        , will_qos(MqttQoS::AT_MOST_ONCE)
        , will_retain(false) {}
};

// 전방 선언
class MqttCallbackHandler;

/**
 * @brief MQTT 워커 클래스
 * @details TcpBasedWorker를 상속받아 MQTT Pub/Sub 통신 특화 기능 제공
 * 
 * 주요 기능:
 * - MQTT 브로커 연결 관리
 * - 토픽 구독/발행 관리
 * - QoS 레벨별 메시지 처리
 * - 자동 재연결 및 세션 복구
 * - SSL/TLS 보안 연결
 * - Will 메시지 지원
 */
class MQTTWorker : public TcpBasedWorker {
public:
    /**
     * @brief 생성자
     * @param device_info 디바이스 정보
     * @param redis_client Redis 클라이언트 (선택적)
     * @param influx_client InfluxDB 클라이언트 (선택적)
     */
    explicit MQTTWorker(
        const Drivers::DeviceInfo& device_info,
        std::shared_ptr<RedisClient> redis_client = nullptr,
        std::shared_ptr<InfluxClient> influx_client = nullptr
    );
    
    /**
     * @brief 소멸자
     */
    virtual ~MQTTWorker();

    // =============================================================================
    // BaseDeviceWorker 인터페이스 구현
    // =============================================================================
    
    std::future<bool> Start() override;
    std::future<bool> Stop() override;
    WorkerState GetState() const override;

    // =============================================================================
    // TcpBasedWorker 인터페이스 구현
    // =============================================================================
    
    bool EstablishProtocolConnection() override;
    bool CloseProtocolConnection() override;
    bool CheckProtocolConnection() override;
    bool SendProtocolKeepAlive();  // 기본 구현이 있음

    // =============================================================================
    // MQTT 클라이언트 설정 관리
    // =============================================================================
    
    /**
     * @brief MQTT 클라이언트 설정
     * @param config 클라이언트 설정
     */
    void ConfigureMqttClient(const MqttClientConfig& config);
    
    /**
     * @brief 브로커 연결 설정
     * @param broker_url 브로커 URL (예: "tcp://localhost:1883", "ssl://broker.example.com:8883")
     * @param client_id 클라이언트 ID
     * @param username 사용자명 (선택사항)
     * @param password 패스워드 (선택사항)
     */
    void ConfigureBroker(const std::string& broker_url,
                        const std::string& client_id,
                        const std::string& username = "",
                        const std::string& password = "");

    // =============================================================================
    // 구독 관리
    // =============================================================================
    
    /**
     * @brief 토픽 구독 추가
     * @param topic MQTT 토픽
     * @param qos QoS 레벨
     * @return 구독 ID (실패 시 0)
     */
    uint32_t AddSubscription(const std::string& topic, MqttQoS qos = MqttQoS::AT_MOST_ONCE);
    
    /**
     * @brief 구독 제거
     * @param subscription_id 구독 ID
     * @return 성공 시 true
     */
    bool RemoveSubscription(uint32_t subscription_id);
    
    /**
     * @brief 구독 활성화/비활성화
     * @param subscription_id 구독 ID
     * @param active 활성화 여부
     * @return 성공 시 true
     */
    bool SetSubscriptionActive(uint32_t subscription_id, bool active);
    
    /**
     * @brief 구독에 데이터 포인트 추가
     * @param subscription_id 구독 ID
     * @param data_point 데이터 포인트
     * @return 성공 시 true
     */
    bool AddDataPointToSubscription(uint32_t subscription_id, const Drivers::DataPoint& data_point);

    // =============================================================================
    // 발행 관리
    // =============================================================================
    
    /**
     * @brief 발행 토픽 추가
     * @param topic MQTT 토픽
     * @param qos QoS 레벨
     * @param retain Retain 플래그
     * @param publish_interval_ms 발행 주기 (밀리초)
     * @return 발행 ID (실패 시 0)
     */
    uint32_t AddPublication(const std::string& topic,
                           MqttQoS qos = MqttQoS::AT_MOST_ONCE,
                           bool retain = false,
                           int publish_interval_ms = 1000);
    
    /**
     * @brief 발행 제거
     * @param publication_id 발행 ID
     * @return 성공 시 true
     */
    bool RemovePublication(uint32_t publication_id);
    
    /**
     * @brief 발행 활성화/비활성화
     * @param publication_id 발행 ID
     * @param active 활성화 여부
     * @return 성공 시 true
     */
    bool SetPublicationActive(uint32_t publication_id, bool active);
    
    /**
     * @brief 발행에 데이터 포인트 추가
     * @param publication_id 발행 ID
     * @param data_point 데이터 포인트
     * @return 성공 시 true
     */
    bool AddDataPointToPublication(uint32_t publication_id, const Drivers::DataPoint& data_point);

    // =============================================================================
    // 메시지 송수신
    // =============================================================================
    
    /**
     * @brief 즉시 메시지 발행
     * @param topic 토픽
     * @param payload 페이로드
     * @param qos QoS 레벨
     * @param retain Retain 플래그
     * @return 성공 시 true
     */
    bool PublishMessage(const std::string& topic,
                       const std::string& payload,
                       MqttQoS qos = MqttQoS::AT_MOST_ONCE,
                       bool retain = false);
    
    /**
     * @brief JSON 형태 메시지 발행
     * @param topic 토픽
     * @param json_data JSON 데이터
     * @param qos QoS 레벨
     * @param retain Retain 플래그
     * @return 성공 시 true
     */
    bool PublishJsonMessage(const std::string& topic,
                           const nlohmann::json& json_data,
                           MqttQoS qos = MqttQoS::AT_MOST_ONCE,
                           bool retain = false);

    // =============================================================================
    // 상태 및 통계 조회
    // =============================================================================
    
    /**
     * @brief MQTT 클라이언트 통계 조회
     * @return JSON 형태의 통계 정보
     */
    std::string GetMqttStats() const;
    
    /**
     * @brief 모든 구독 상태 조회
     * @return JSON 형태의 구독 상태
     */
    std::string GetSubscriptionStatus() const;
    
    /**
     * @brief 모든 발행 상태 조회
     * @return JSON 형태의 발행 상태
     */
    std::string GetPublicationStatus() const;

    // =============================================================================
    // 콜백 메서드 (MqttCallbackHandler에서 호출)
    // =============================================================================
    
    /**
     * @brief 연결 손실 콜백
     * @param cause 원인
     */
    void OnConnectionLost(const std::string& cause);
    
    /**
     * @brief 메시지 수신 콜백
     * @param message 수신된 메시지
     */
    void OnMessageArrived(mqtt::const_message_ptr message);
    
    /**
     * @brief 메시지 전송 완료 콜백
     * @param token 전송 토큰
     */
    void OnDeliveryComplete(mqtt::delivery_token_ptr token);
    
    /**
     * @brief 연결 성공 콜백
     * @param reconnect 재연결 여부
     */
    void OnConnected(bool reconnect);

protected:
    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    // MQTT 클라이언트 설정
    MqttClientConfig mqtt_config_;
    std::string broker_url_;
    
    // MQTT 클라이언트
    std::unique_ptr<mqtt::async_client> mqtt_client_;
    std::unique_ptr<MqttCallbackHandler> callback_handler_;
    
    // 구독 관리
    std::map<uint32_t, MqttSubscription> subscriptions_;
    mutable std::shared_mutex subscriptions_mutex_;
    uint32_t next_subscription_id_;
    
    // 발행 관리
    std::map<uint32_t, MqttPublication> publications_;
    mutable std::shared_mutex publications_mutex_;
    uint32_t next_publication_id_;
    
    // 워커 스레드들
    std::thread publish_worker_thread_;
    std::atomic<bool> stop_workers_;
    
    // 메시지 큐
    std::queue<std::pair<std::string, std::string>> incoming_messages_; // topic, payload
    mutable std::mutex message_queue_mutex_;
    std::condition_variable message_queue_cv_;
    
    // 통계 (mutex로 보호)
    mutable std::mutex stats_mutex_;
    uint64_t total_messages_sent_;
    uint64_t total_messages_received_;
    uint64_t failed_sends_;
    uint64_t connection_attempts_;
    uint64_t successful_connections_;

    // =============================================================================
    // 내부 헬퍼 메소드들
    // =============================================================================
    
    /**
     * @brief 발행 워커 루프
     */
    void PublishWorkerLoop();
    
    /**
     * @brief 개별 발행 처리
     * @param publication 발행 정보
     * @return 처리 성공 시 true
     */
    bool ProcessPublication(MqttPublication& publication);
    
    /**
     * @brief 수신된 메시지 처리
     * @param topic 토픽
     * @param payload 페이로드
     */
    void ProcessReceivedMessage(const std::string& topic, const std::string& payload);
    
    /**
     * @brief 페이로드를 데이터 값으로 파싱
     * @param payload 페이로드 문자열
     * @param data_type 데이터 타입
     * @return 파싱된 데이터 값
     */
    Drivers::DataValue ParsePayloadToDataValue(const std::string& payload, Drivers::DataType data_type);
    
    /**
     * @brief 데이터 값을 페이로드로 변환
     * @param value 데이터 값
     * @return 페이로드 문자열
     */
    std::string DataValueToPayload(const Drivers::DataValue& value);
    
    /**
     * @brief MQTT 에러를 문자열로 변환
     * @param return_code 리턴 코드
     * @return 에러 메시지
     */
    std::string MqttErrorToString(int return_code) const;
    
    /**
     * @brief 통계 업데이트
     * @param operation 연산 타입 ("send", "receive", "connect")
     * @param success 성공 여부
     */
    void UpdateMqttStats(const std::string& operation, bool success);
    
    /**
     * @brief MQTT 로그 메시지 출력
     * @param level 로그 레벨
     * @param message 메시지
     */
    void LogMqttMessage(LogLevel level, const std::string& message) const;
    
    /**
     * @brief QoS enum을 정수로 변환
     * @param qos QoS enum
     * @return QoS 정수 값
     */
    int QosToInt(MqttQoS qos) const;
    
    /**
     * @brief 브로커 URL 파싱 및 검증
     * @param url 브로커 URL
     * @return 유효한 URL인지 여부
     */
    bool ValidateBrokerUrl(const std::string& url) const;
    
    /**
     * @brief MQTT 토픽 유효성 검사
     * @param topic MQTT 토픽
     * @return 유효한 토픽인지 여부
     */
    bool ValidateMqttTopic(const std::string& topic) const;
    
    /**
     * @brief 구독 복원 (재연결 시)
     */
    void RestoreSubscriptions();
};

/**
 * @brief MQTT 콜백 핸들러 클래스
 * @details Eclipse Paho의 콜백을 MQTTWorker로 전달하는 어댑터
 */
class MqttCallbackHandler : public virtual mqtt::callback,
                           public virtual mqtt::iaction_listener {
public:
    explicit MqttCallbackHandler(MQTTWorker* worker) : mqtt_worker_(worker) {}
    
    // mqtt::callback 인터페이스 구현
    void connection_lost(const std::string& cause) override;
    void message_arrived(mqtt::const_message_ptr message) override;
    void delivery_complete(mqtt::delivery_token_ptr token) override;
    
    // mqtt::iaction_listener 인터페이스 구현
    void on_failure(const mqtt::token& token) override;
    void on_success(const mqtt::token& token) override;
    
private:
    MQTTWorker* mqtt_worker_;
};

} // namespace Protocol
} // namespace Workers
} // namespace PulseOne

#endif // WORKERS_PROTOCOL_MQTT_WORKER_H