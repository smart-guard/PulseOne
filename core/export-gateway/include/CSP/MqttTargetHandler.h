/**
 * @file MqttTargetHandler.h
 * @brief MQTT 타겟 핸들러 - MQTT 브로커를 통한 실시간 메시지 발행
 * @author PulseOne Development Team
 * @date 2025-09-23
 * 저장 위치: core/export-gateway/include/CSP/MqttTargetHandler.h
 */

#ifndef MQTT_TARGET_HANDLER_H
#define MQTT_TARGET_HANDLER_H

#include "CSPDynamicTargets.h"
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <queue>

// Paho MQTT 전방 선언 (실제 구현에서는 #include <MQTTClient.h>)
typedef void* MQTTClient;
typedef struct {
    int QoS;
    int retained;
    int dup;
    int msgid;
} MQTTClient_deliveryToken;

namespace PulseOne {
namespace CSP {

/**
 * @brief MQTT 타겟 핸들러
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
 * - 발행 확인 (QoS > 0)
 */
class MqttTargetHandler : public ITargetHandler {
private:
    mutable std::mutex client_mutex_;  // MQTT 클라이언트 보호용
    mutable std::mutex queue_mutex_;   // 메시지 큐 보호용
    std::condition_variable queue_cv_;
    
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> is_connecting_{false};
    std::atomic<size_t> publish_count_{0};
    std::atomic<size_t> success_count_{0};
    std::atomic<size_t> failure_count_{0};
    
    // 연결 끊김 시 메시지 큐
    std::queue<std::pair<std::string, std::string>> message_queue_;  // topic, payload
    size_t max_queue_size_ = 1000;
    
    // 재연결 스레드
    std::unique_ptr<std::thread> reconnect_thread_;
    std::atomic<bool> should_stop_{false};
    
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
    MqttTargetHandler(const MqttTargetHandler&) = delete;
    MqttTargetHandler& operator=(const MqttTargetHandler&) = delete;
    MqttTargetHandler(MqttTargetHandler&&) = delete;
    MqttTargetHandler& operator=(MqttTargetHandler&&) = delete;
    
    /**
     * @brief 핸들러 초기화
     * @param config JSON 설정 객체
     * @return 초기화 성공 여부
     * 
     * 필수 설정:
     * - broker_host: MQTT 브로커 호스트
     * 
     * 선택 설정:
     * - broker_port: 브로커 포트 (기본: 1883, SSL: 8883)
     * - client_id: 클라이언트 ID (기본: 자동 생성)
     * - username: 사용자명
     * - password_file: 암호 파일 경로
     * - topic_pattern: 토픽 패턴 (기본: "alarms/{building_id}/{nm}")
     * - qos: QoS 레벨 (0, 1, 2) (기본: 1)
     * - retain: Retained 메시지 사용 (기본: false)
     * - ssl_enabled: SSL/TLS 사용 (기본: false)
     * - ssl_ca_file: CA 인증서 파일
     * - ssl_cert_file: 클라이언트 인증서 파일
     * - ssl_key_file: 클라이언트 키 파일
     * - ssl_verify_peer: 서버 인증서 검증 (기본: true)
     * - keep_alive_sec: Keep-alive 간격 (기본: 60초)
     * - connect_timeout_sec: 연결 타임아웃 (기본: 30초)
     * - publish_timeout_sec: 발행 타임아웃 (기본: 10초)
     * - auto_reconnect: 자동 재연결 (기본: true)
     * - reconnect_interval_sec: 재연결 간격 (기본: 5초)
     * - max_reconnect_attempts: 최대 재연결 시도 (기본: -1, 무제한)
     * - clean_session: Clean Session 플래그 (기본: true)
     * - max_inflight: 최대 진행중 메시지 수 (기본: 10)
     * - message_format: 메시지 형식 ("json", "text") (기본: "json")
     * - include_metadata: 메타데이터 포함 여부 (기본: true)
     */
    bool initialize(const json& config) override;
    
    /**
     * @brief 알람 메시지 전송 (MQTT 발행)
     * @param alarm 발행할 알람 메시지
     * @param config 타겟별 설정
     * @return 발행 결과
     */
    TargetSendResult sendAlarm(const AlarmMessage& alarm, const json& config) override;
    
    /**
     * @brief 연결 테스트
     * @param config 타겟별 설정
     * @return 연결 성공 여부
     */
    bool testConnection(const json& config) override;
    
    /**
     * @brief 핸들러 타입 이름 반환
     */
    std::string getTypeName() const override { return "mqtt"; }
    
    /**
     * @brief 핸들러 상태 반환
     */
    json getStatus() const override;
    
    /**
     * @brief 핸들러 정리
     */
    void cleanup() override;

private:
    /**
     * @brief MQTT 클라이언트 생성 및 연결
     * @param config 설정 객체
     * @return 성공 여부
     */
    bool connectToBroker(const json& config);
    
    /**
     * @brief MQTT 클라이언트 연결 해제
     */
    void disconnectFromBroker();
    
    /**
     * @brief 토픽 생성 (템플릿 기반)
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 생성된 토픽
     */
    std::string generateTopic(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 메시지 페이로드 생성
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 메시지 페이로드
     */
    std::string generatePayload(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 단일 메시지 발행
     * @param topic 토픽
     * @param payload 페이로드
     * @param qos QoS 레벨
     * @param retain Retained 플래그
     * @return 발행 결과
     */
    TargetSendResult publishMessage(const std::string& topic, 
                                   const std::string& payload,
                                   int qos, bool retain) const;
    
    /**
     * @brief 암호 로드 (암호화된 파일에서)
     * @param password_file 암호 파일 경로
     * @return 복호화된 암호
     */
    std::string loadPassword(const std::string& password_file) const;
    
    /**
     * @brief SSL 설정 구성
     * @param config 설정 객체
     * @return 성공 여부
     */
    bool configureSsl(const json& config);
    
    /**
     * @brief 클라이언트 ID 생성
     * @param base_id 기본 ID (비어있으면 자동 생성)
     * @return 유니크한 클라이언트 ID
     */
    std::string generateClientId(const std::string& base_id = "") const;
    
    /**
     * @brief 자동 재연결 스레드
     * @param config 설정 객체 (복사본)
     */
    void reconnectThread(json config);
    
    /**
     * @brief 큐된 메시지 처리
     */
    void processQueuedMessages();
    
    /**
     * @brief 메시지를 큐에 추가 (연결 끊김 시)
     * @param topic 토픽
     * @param payload 페이로드
     * @return 큐에 추가 성공 여부
     */
    bool enqueueMessage(const std::string& topic, const std::string& payload);
    
    /**
     * @brief 템플릿 변수 확장
     * @param template_str 템플릿 문자열
     * @param alarm 알람 메시지
     * @return 확장된 문자열
     */
    std::string expandTemplateVariables(const std::string& template_str, const AlarmMessage& alarm) const;
    
    /**
     * @brief 토픽 유효성 검증
     * @param topic 검증할 토픽
     * @return 유효한 토픽인지 여부
     */
    bool isValidTopic(const std::string& topic) const;
    
    /**
     * @brief 브로커 주소 유효성 검증
     * @param broker_host 브로커 호스트
     * @param broker_port 브로커 포트
     * @return 유효한 주소인지 여부
     */
    bool isValidBrokerAddress(const std::string& broker_host, int broker_port) const;
    
    /**
     * @brief QoS 레벨 유효성 검증
     * @param qos QoS 레벨
     * @return 유효한 QoS인지 여부
     */
    bool isValidQoS(int qos) const;
    
    /**
     * @brief 설정 검증
     * @param config 검증할 설정
     * @param error_message 오류 메시지 (출력용)
     * @return 설정이 유효한지 여부
     */
    bool validateConfig(const json& config, std::string& error_message) const;
    
    /**
     * @brief JSON 메시지 생성 (메타데이터 포함)
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return JSON 메시지 문자열
     */
    std::string createJsonMessage(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 텍스트 메시지 생성
     * @param alarm 알람 메시지
     * @param config 설정 객체
     * @return 텍스트 메시지 문자열
     */
    std::string createTextMessage(const AlarmMessage& alarm, const json& config) const;
    
    /**
     * @brief 발행 로깅 (디버그용)
     * @param topic 토픽
     * @param payload 페이로드
     * @param qos QoS 레벨
     * @param retain Retained 플래그
     */
    void logPublish(const std::string& topic, const std::string& payload,
                   int qos, bool retain) const;
    
    /**
     * @brief 발행 결과 로깅
     * @param result 발행 결과
     * @param topic 토픽
     * @param response_time 응답 시간
     */
    void logPublishResult(const TargetSendResult& result, const std::string& topic,
                         std::chrono::milliseconds response_time) const;

    // MQTT 콜백 함수들 (C 스타일)
    static void onConnectionLost(void* context, char* cause);
    static int onMessageArrived(void* context, char* topicName, int topicLen, void* message);
    static void onDeliveryComplete(void* context, MQTTClient_deliveryToken token);
    
    // 내부 상태
    MQTTClient mqtt_client_ = nullptr;
    std::string broker_uri_;
    std::string client_id_;
    std::atomic<int> connection_attempts_{0};
    std::chrono::system_clock::time_point last_connect_attempt_;
};

} // namespace CSP
} // namespace PulseOne

#endif // MQTT_TARGET_HANDLER_H