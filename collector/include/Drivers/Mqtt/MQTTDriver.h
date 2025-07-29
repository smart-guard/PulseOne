// =============================================================================
// collector/include/Drivers/Mqtt/MqttDriver.h
// MQTT 프로토콜 드라이버 헤더 (기존 구조 호환)
// =============================================================================

#ifndef PULSEONE_DRIVERS_MQTT_DRIVER_H
#define PULSEONE_DRIVERS_MQTT_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Drivers/Common/DriverLogger.h"
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <queue>
#include <optional>
#include <condition_variable>
#include <nlohmann/json.hpp>
/*
// MQTT 라이브러리는 나중에 추가 (현재는 포인터 타입으로 전방 선언)
namespace mqtt {
    class async_client;
    class callback;
    class iaction_listener;
    class token;
    class message;
    class delivery_token;
    
    // 포인터 타입들
    using const_message_ptr = std::shared_ptr<const message>;
    using delivery_token_ptr = std::shared_ptr<delivery_token>;
}
*/
#include <mqtt/async_client.h>
#include <mqtt/callback.h>
#include <mqtt/iaction_listener.h>
#include <mqtt/connect_options.h>
#include <mqtt/message.h>
#include <mqtt/token.h>    

namespace PulseOne {
namespace Drivers {

/**
 * @brief MQTT 프로토콜 드라이버
 * 
 * 현재는 스텁 구현으로, 나중에 실제 MQTT 기능을 추가할 예정
 */
class MqttDriver : public IProtocolDriver {
public:
    MqttDriver();
    virtual ~MqttDriver();
    
    // IProtocolDriver 인터페이스 구현
    bool Initialize(const DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    bool ReadValues(
        const std::vector<Structs::DataPoint>& points,
        std::vector<TimestampedValue>& values
    ) override;
    
    bool WriteValue(
        const Structs::DataPoint& point,
        const Structs::DataValue& value
    ) override;
    
    struct MqttDataPointInfo {
        std::string point_id;           ///< 데이터 포인트 고유 ID
        std::string name;               ///< 데이터 포인트 이름  
        std::string description;        ///< 데이터 포인트 설명
        std::string topic;              ///< MQTT 토픽
        int qos;                        ///< QoS 레벨 (0, 1, 2)
        Structs::DataType data_type;             ///< 데이터 타입
        std::string unit;               ///< 단위 (예: "°C", "%RH")
        double scaling_factor;          ///< 스케일링 팩터
        double scaling_offset;          ///< 스케일링 오프셋
        bool is_writable;               ///< 쓰기 가능 여부
        bool auto_subscribe;            ///< 자동 구독 여부
    
        /**
         * @brief 기본 생성자
        */
        MqttDataPointInfo()
        : qos(1)
        , data_type(Structs::DataType::UNKNOWN)
        , scaling_factor(1.0)
        , scaling_offset(0.0)
        , is_writable(false)
        , auto_subscribe(true) {}
    
        /**
         * @brief 매개변수 생성자
        */
        MqttDataPointInfo(const std::string& id, const std::string& point_name, 
                     const std::string& desc, const std::string& mqtt_topic,
                     int qos_level = 1, Structs::DataType type = Structs::DataType::FLOAT32)
        : point_id(id)
        , name(point_name)
        , description(desc)
        , topic(mqtt_topic)
        , qos(qos_level)
        , data_type(type)
        , scaling_factor(1.0)
        , scaling_offset(0.0)
        , is_writable(false)
        , auto_subscribe(true) {}
        
    /**
     * @brief 전체 설정 생성자
     */
        MqttDataPointInfo(const std::string& id, const std::string& point_name,
                     const std::string& desc, const std::string& mqtt_topic,
                     int qos_level, Structs::DataType type, const std::string& point_unit,
                     double scale_factor, double scale_offset, 
                     bool writable, bool auto_sub)
        : point_id(id)
        , name(point_name)
        , description(desc)
        , topic(mqtt_topic)
        , qos(qos_level)
        , data_type(type)
        , unit(point_unit)
        , scaling_factor(scale_factor)
        , scaling_offset(scale_offset)
        , is_writable(writable)
        , auto_subscribe(auto_sub) {}
    };
    
    ProtocolType GetProtocolType() const override;
    Structs::DriverStatus GetStatus() const override;
    ErrorInfo GetLastError() const override;
    const DriverStatistics& GetStatistics() const override;
    void ResetStatistics() override;
    // GetDiagnostics() 메소드를 override하지 않음 (기본 구현 사용)
    
    // MQTT 특화 메소드들
    
    /**
     * @brief 토픽 구독
     * @param topic 구독할 토픽
     * @param qos QoS 레벨 (0, 1, 2)
     * @return 성공 시 true
     */
    bool Subscribe(const std::string& topic, int qos = 1);
    
    /**
     * @brief 토픽 구독 해제
     * @param topic 구독 해제할 토픽
     * @return 성공 시 true
     */
    bool Unsubscribe(const std::string& topic);
    
    /**
     * @brief 메시지 발행
     * @param topic 발행할 토픽
     * @param payload 메시지 내용
     * @param qos QoS 레벨
     * @param retained Retained 메시지 여부
     * @return 성공 시 true
     */
    bool Publish(const std::string& topic, const std::string& payload, 
                 int qos = 1, bool retained = false);
    
    /**
     * @brief JSON 메시지 발행
     * @param topic 발행할 토픽
     * @param json_data JSON 객체
     * @param qos QoS 레벨
     * @param retained Retained 메시지 여부
     * @return 성공 시 true
     */
    bool PublishJson(const std::string& topic, const nlohmann::json& json_data,
                     int qos = 1, bool retained = false);
    
    /**
     * @brief 데이터 포인트를 JSON으로 발행
     * @param data_points 발행할 데이터 포인트들
     * @param base_topic 기본 토픽 (데이터 포인트별로 확장됨)
     * @return 성공 시 true
     */
    bool PublishDataPoints(const std::vector<std::pair<Structs::DataPoint, TimestampedValue>>& data_points,
                          const std::string& base_topic = "data");

    // ✅ 여기에 새 메소드들 추가 ✅
    /**
     * @brief 토픽으로 데이터 포인트 정보 찾기
     * @param topic 검색할 토픽
     * @return 데이터 포인트 정보 (없으면 nullopt)
     */
    std::optional<MqttDataPointInfo> FindPointByTopic(const std::string& topic) const;
    
    /**
     * @brief 포인트 ID로 토픽 찾기
     * @param point_id 검색할 포인트 ID
     * @return 토픽 이름 (없으면 빈 문자열)
     */
    std::string FindTopicByPointId(const std::string& point_id) const;
    
    /**
     * @brief 로드된 포인트 수 반환
     * @return 현재 로드된 MQTT 데이터 포인트 개수
     */
    size_t GetLoadedPointCount() const;                      
    
    // MQTT 콜백 인터페이스 구현 (스텁)
    virtual void connected(const std::string& cause);
    virtual void connection_lost(const std::string& cause);
    virtual void message_arrived(mqtt::const_message_ptr msg);
    virtual void delivery_complete(mqtt::delivery_token_ptr tok);
    
    // MQTT 액션 리스너 인터페이스 구현 (스텁)
    virtual void on_failure(const mqtt::token& tok);
    virtual void on_success(const mqtt::token& tok);

    // 진단 메소드들
    bool EnableDiagnostics(LogManager& log_manager, 
                          bool packet_log = true, bool console = false);
    void DisableDiagnostics();
    std::string GetDiagnosticsJSON() const;
    std::string GetTopicPointName(const std::string& topic) const;

        // ✅ 이 위치에 MQTT 콜백 메소드들 추가
    /**
     * @brief MQTT 연결 끊김 콜백
     * @param cause 연결 끊김 원인
     */
    void OnConnectionLost(const std::string& cause);
    
    /**
     * @brief MQTT 메시지 수신 콜백
     * @param msg 수신된 메시지
     */
    void OnMessageArrived(mqtt::const_message_ptr msg);
    
    /**
     * @brief MQTT 메시지 전송 완료 콜백
     * @param token 전송 토큰
     */
    void OnDeliveryComplete(mqtt::delivery_token_ptr token);

private:
    // ==========================================================================
    // 내부 구조체 및 열거형
    // ==========================================================================
    
    struct MqttConfig {
        std::string broker_url;                 ///< 브로커 URL (mqtt://localhost:1883)
        std::string broker_address;             ///< 브로커 주소 (localhost)
        int broker_port;                        ///< 브로커 포트 (1883)
        std::string client_id;                  ///< 클라이언트 ID
        std::string username;                   ///< 사용자명 (옵션)
        std::string password;                   ///< 패스워드 (옵션)
        int keep_alive_interval;                ///< Keep-alive 간격 (초)
        bool clean_session;                     ///< Clean session 플래그
        bool auto_reconnect;                    ///< 자동 재연결 플래그
        bool use_ssl;                           ///< SSL 사용 여부
        int qos_level;                          ///< 기본 QoS 레벨
        std::string ca_cert_path;              ///< 🆕 추가: CA 인증서 경로
        
        MqttConfig() 
            : broker_url("mqtt://localhost:1883")
            , broker_address("localhost")
            , broker_port(1883)
            , client_id("pulseone_client")
            , keep_alive_interval(60)
            , clean_session(true)
            , auto_reconnect(true)
            , use_ssl(false)
            , qos_level(1) {}
    } mqtt_config_;
    
    // 구독 정보 구조체 수정
    struct SubscriptionInfo {
        int qos;                                ///< QoS 레벨
        Timestamp subscribed_at;                ///< 구독 시간
        bool is_active;                         ///< 활성 상태
        
        SubscriptionInfo() : qos(1), is_active(false) {}
        SubscriptionInfo(int q) : qos(q), is_active(true) {
            subscribed_at = std::chrono::system_clock::now();
        }
    };
    
    struct PublishRequest {
        std::string topic;
        std::string payload;
        int qos;
        bool retained;
        Timestamp request_time;
        std::promise<bool> promise;
        
        PublishRequest(const std::string& t, const std::string& p, int q, bool r)
            : topic(t), payload(p), qos(q), retained(r),
              request_time(std::chrono::system_clock::now()) {}
    };
    

    
    struct MqttPacketLog {
        std::string direction;        // "PUBLISH", "SUBSCRIBE", "RECEIVE"
        Timestamp timestamp;
        std::string topic;
        int qos;
        size_t payload_size;
        bool success;
        std::string error_message;
        double response_time_ms;
        std::string decoded_value;    // 엔지니어 친화적 값
        std::string raw_payload;      // 원시 페이로드 (일부)
    };
    
    // ==========================================================================
    // 내부 메소드들
    // ==========================================================================
    
    bool ParseConfig(const DriverConfig& config);
    bool SetupSslOptions();
    bool CreateMqttClient();
    bool SetupCallbacks();
    
    // 연결 관리
    bool EstablishConnection();
    void HandleReconnection();
    void ProcessReconnection();
    
    // 메시지 처리
    void ProcessIncomingMessage(mqtt::const_message_ptr msg);
    Structs::DataValue ParseMessagePayload(const std::string& payload, Structs::DataType expected_type);
    nlohmann::json CreateDataPointJson(const Structs::DataPoint& point, const TimestampedValue& value);
    
    // 구독 관리
    void RestoreSubscriptions();
    void UpdateSubscriptionStatus(const std::string& topic, bool subscribed);
    void ProcessReceivedMessage(const std::string& topic, const std::string& payload, int qos);
    // 에러 처리
    void SetError(ErrorCode code, const std::string& message);
    void UpdateStatistics(const std::string& operation, bool success, double duration_ms = 0);
    
    // 진단 및 상태
    void UpdateDiagnostics();
    void CleanupResources();
    
    // 비동기 작업 관리
    void StartBackgroundTasks();
    void StopBackgroundTasks();
    void PublishWorkerLoop();
    void ReconnectWorkerLoop();
    
    // 진단 헬퍼 메소드들
    void LogMqttPacket(const std::string& direction, const std::string& topic,
                      int qos, size_t payload_size, bool success,
                      const std::string& error = "", double response_time_ms = 0.0);
    
    std::string FormatMqttValue(const std::string& topic, 
                               const std::string& payload) const;
    
    std::string FormatMqttPacketForConsole(const MqttPacketLog& log) const;
    bool LoadMqttPointsFromDB();
    
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // 설정
    DriverConfig config_;
    
    // MQTT 클라이언트 (나중에 구현)
    // std::unique_ptr<mqtt::async_client> client_;
    
    // 상태 관리
    std::atomic<Structs::DriverStatus> status_;
    std::atomic<bool> is_connected_;
    std::atomic<bool> connection_in_progress_;
    
    // 에러 및 통계
    mutable std::mutex error_mutex_;
    ErrorInfo last_error_;
    mutable std::mutex stats_mutex_;
    DriverStatistics statistics_;
    
    // 구독 관리
    mutable std::mutex subscriptions_mutex_;
    std::unordered_map<std::string, SubscriptionInfo> subscriptions_;
    
    // 데이터 포인트 매핑 (토픽 -> 데이터 포인트)std::unordered_map<std::string, SubscriptionInfo> subscriptions_;
    mutable std::mutex data_mapping_mutex_;
    std::unordered_map<std::string, std::vector<Structs::DataPoint>> topic_to_datapoints_;
    std::unordered_map<UUID, std::string> datapoint_to_topic_;
    
    // 최신 값 저장
    mutable std::mutex values_mutex_;
    std::unordered_map<UUID, TimestampedValue> latest_values_;
    
    // 비동기 작업 관리
    std::thread publish_worker_;
    std::thread reconnect_worker_;
    std::atomic<bool> stop_workers_;
    
    // 발행 요청 큐
    std::mutex publish_queue_mutex_;
    std::condition_variable publish_queue_cv_;
    std::queue<std::unique_ptr<PublishRequest>> publish_queue_;
    
    // 재연결 관리
    std::mutex reconnect_mutex_;
    std::condition_variable reconnect_cv_;
    std::atomic<bool> need_reconnect_;
    Timestamp last_reconnect_attempt_;
    
    // 로깅
    std::unique_ptr<DriverLogger> logger_;
    
    // 진단 정보
    mutable std::mutex diagnostics_mutex_;
    
    // 성능 추적
    Timestamp last_successful_operation_;
    std::atomic<uint64_t> total_messages_received_;
    std::atomic<uint64_t> total_messages_sent_;
    std::atomic<uint64_t> total_bytes_received_;
    std::atomic<uint64_t> total_bytes_sent_;

    // 진단 관련 멤버들
    bool diagnostics_enabled_;
    bool packet_logging_enabled_;
    bool console_output_enabled_;
    
    LogManager* log_manager_;
    
    mutable std::mutex mqtt_diagnostics_mutex_;
    mutable std::mutex mqtt_points_mutex_;
    mutable std::mutex mqtt_packet_log_mutex_;
    
    std::map<std::string, MqttDataPointInfo> mqtt_point_info_map_;
    std::deque<MqttPacketLog> mqtt_packet_history_;

    std::unique_ptr<mqtt::async_client> mqtt_client_;
    std::unique_ptr<mqtt::callback> mqtt_callback_;
    mqtt::connect_options connect_options_;
    std::mutex connection_mutex_;               ///< 연결 상태 뮤텍스

    mutable std::mutex message_queue_mutex_;
    std::queue<std::pair<std::string, std::string>> incoming_messages_; // topic, payload
    std::condition_variable message_queue_cv_;
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_MQTT_DRIVER_H

