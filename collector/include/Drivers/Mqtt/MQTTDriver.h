// =============================================================================
// collector/include/Drivers/MqttDriver.h
// MQTT 프로토콜 드라이버 헤더
// =============================================================================

#ifndef PULSEONE_DRIVERS_MQTT_DRIVER_H
#define PULSEONE_DRIVERS_MQTT_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Drivers/Common/DriverLogger.h"
#include <mqtt/async_client.h>
#include <mqtt/callback.h>
#include <mqtt/iaction_listener.h>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <queue>
#include <condition_variable>
// #include <json/json.h>

namespace PulseOne {
namespace Drivers {

/**
 * @brief MQTT 프로토콜 드라이버
 * 
 * Eclipse Paho C++ 라이브러리를 사용하여 MQTT 통신을 구현합니다.
 * 비동기 통신, 자동 재연결, QoS 지원 등의 기능을 제공합니다.
 */
class MqttDriver : public IProtocolDriver, 
                   public virtual mqtt::callback,
                   public virtual mqtt::iaction_listener {
public:
    MqttDriver();
    virtual ~MqttDriver();
    
    // IProtocolDriver 인터페이스 구현
    bool Initialize(const DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    bool ReadValues(
        const std::vector<DataPoint>& points,
        std::vector<TimestampedValue>& values
    ) override;
    
    bool WriteValue(
        const DataPoint& point,
        const DataValue& value
    ) override;
    
    ProtocolType GetProtocolType() const override;
    DriverStatus GetStatus() const override;
    ErrorInfo GetLastError() const override;
    DriverStatistics GetStatistics() const override;
    void ResetStatistics() override;
    std::map<std::string, std::string> GetDiagnostics() const override;
    
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
    bool PublishJson(const std::string& topic, const Json::Value& json_data,
                     int qos = 1, bool retained = false);
    
    /**
     * @brief 데이터 포인트를 JSON으로 발행
     * @param data_points 발행할 데이터 포인트들
     * @param base_topic 기본 토픽 (데이터 포인트별로 확장됨)
     * @return 성공 시 true
     */
    bool PublishDataPoints(const std::vector<std::pair<DataPoint, TimestampedValue>>& data_points,
                          const std::string& base_topic = "data");
    
    // MQTT 콜백 인터페이스 구현
    void connected(const std::string& cause) override;
    void connection_lost(const std::string& cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;
    void delivery_complete(mqtt::delivery_token_ptr tok) override;
    
    // MQTT 액션 리스너 인터페이스 구현
    void on_failure(const mqtt::token& tok) override;
    void on_success(const mqtt::token& tok) override;

    // ✅ 새로 추가: 진단 메소드들
    bool EnableDiagnostics(DatabaseManager& db_manager, 
                          bool packet_log = true, bool console = false);
    void DisableDiagnostics();
    std::string GetDiagnosticsJSON() const;
    std::string GetTopicPointName(const std::string& topic) const;

private:
    // ==========================================================================
    // 내부 구조체 및 열거형
    // ==========================================================================
    
    struct MqttConfig {
        std::string broker_url;
        std::string client_id;
        std::string username;
        std::string password;
        int keep_alive_interval = 60;
        int clean_session = true;
        bool use_ssl = false;
        std::string ca_cert_path;
        std::string client_cert_path;
        std::string client_key_path;
        int max_inflight = 100;
        int connection_timeout = 30;
        int retry_interval = 10;
        bool auto_reconnect = true;
    };
    
    struct SubscriptionInfo {
        std::string topic;
        int qos;
        bool subscribed;
        Timestamp last_message;
        uint64_t message_count;
        
        SubscriptionInfo(const std::string& t, int q) 
            : topic(t), qos(q), subscribed(false), message_count(0) {}
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
    
    // ==========================================================================
    // 내부 메소드들
    // ==========================================================================
    
    bool ParseConfig(const DriverConfig& config);
    bool SetupSslOptions();
    bool CreateMqttClient();
    void SetupCallbacks();
    
    // 연결 관리
    bool EstablishConnection();
    void HandleReconnection();
    void ProcessReconnection();
    
    // 메시지 처리
    void ProcessIncomingMessage(mqtt::const_message_ptr msg);
    DataValue ParseMessagePayload(const std::string& payload, DataType expected_type);
    Json::Value CreateDataPointJson(const DataPoint& point, const TimestampedValue& value);
    
    // 구독 관리
    void RestoreSubscriptions();
    void UpdateSubscriptionStatus(const std::string& topic, bool subscribed);
    
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
    
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // 설정
    DriverConfig config_;
    MqttConfig mqtt_config_;
    
    // MQTT 클라이언트
    std::unique_ptr<mqtt::async_client> client_;
    mqtt::connect_options connect_opts_;
    mqtt::ssl_options ssl_opts_;
    
    // 상태 관리
    std::atomic<DriverStatus> status_;
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
    
    // 데이터 포인트 매핑 (토픽 -> 데이터 포인트)
    mutable std::mutex data_mapping_mutex_;
    std::unordered_map<std::string, std::vector<DataPoint>> topic_to_datapoints_;
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
    std::map<std::string, std::string> diagnostics_;
    
    // 성능 추적
    Timestamp last_successful_operation_;
    std::atomic<uint64_t> total_messages_received_;
    std::atomic<uint64_t> total_messages_sent_;
    std::atomic<uint64_t> total_bytes_received_;
    std::atomic<uint64_t> total_bytes_sent_;

        // ✅ 새로 추가: 진단 관련 멤버들
    bool diagnostics_enabled_;
    bool packet_logging_enabled_;
    bool console_output_enabled_;
    
    LogManager* log_manager_;
    DatabaseManager* db_manager_;
    
    struct MqttDataPointInfo {
        std::string name;
        std::string description;
        std::string unit;
        double scaling_factor;
        double scaling_offset;
        std::string topic;
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
    
    mutable std::mutex mqtt_diagnostics_mutex_;
    mutable std::mutex mqtt_points_mutex_;
    mutable std::mutex mqtt_packet_log_mutex_;
    
    std::map<std::string, MqttDataPointInfo> mqtt_point_info_map_;
    std::deque<MqttPacketLog> mqtt_packet_history_;
    
    // 진단 헬퍼 메소드들
    void LogMqttPacket(const std::string& direction, const std::string& topic,
                      int qos, size_t payload_size, bool success,
                      const std::string& error = "", double response_time_ms = 0.0);
    
    std::string FormatMqttValue(const std::string& topic, 
                               const std::string& payload) const;
    
    std::string FormatMqttPacketForConsole(const MqttPacketLog& log) const;
    bool LoadMqttPointsFromDB();
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_MQTT_DRIVER_H