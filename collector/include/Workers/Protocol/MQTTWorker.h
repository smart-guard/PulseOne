/**
 * @file MQTTWorker.h
 * @brief MQTT 프로토콜 워커 클래스
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 1.0.0
 * 
 * @details
 * BaseDeviceWorker를 상속받아 MQTT 프로토콜 특화 기능을 제공합니다.
 * MQTTDriver를 통신 매체로 사용하여 객체 관리와 통신을 분리합니다.
 * 
 * 역할 분리:
 * - MQTTDriver: 순수 MQTT 통신 (연결, 발행/구독, 메시지 처리)
 * - MQTTWorker: 객체 관리 (토픽 관리, 스케줄링, 데이터 변환, DB 저장)
 */

#ifndef MQTT_WORKER_H
#define MQTT_WORKER_H

#include "Workers/Base/BaseDeviceWorker.h"
#include "Drivers/Mqtt/MQTTDriver.h"
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <queue>
#include <atomic>

namespace PulseOne {
namespace Workers {


// =============================================================================
// MQTT QoS 레벨 정의
// =============================================================================

/**
 * @brief MQTT QoS 레벨
 */
enum class MqttQoS {
    AT_MOST_ONCE = 0,      ///< 최대 한번 전송 (Fire and forget)
    AT_LEAST_ONCE = 1,     ///< 최소 한번 전송 (ACK 필요)
    EXACTLY_ONCE = 2       ///< 정확히 한번 전송 (핸드셰이크)
};

/**
 * @brief MQTT 토픽 구독 정보
 */
struct MQTTSubscription {
    std::string topic;                    ///< MQTT 토픽
    int qos;                              ///< QoS 레벨 (0, 1, 2)
    bool enabled;                         ///< 활성화 여부
    std::string point_id;                 ///< 연결된 데이터 포인트 ID
    Drivers::DataType data_type;          ///< 예상 데이터 타입
    std::string json_path;                ///< JSON 페이로드에서 값 추출 경로 (예: "sensors.temperature")
    double scaling_factor;                ///< 스케일링 팩터
    double scaling_offset;                ///< 스케일링 오프셋
    
    // 통계
    uint64_t messages_received;           ///< 수신된 메시지 수
    std::chrono::system_clock::time_point last_received; ///< 마지막 수신 시간
    
    MQTTSubscription()
        : qos(1), enabled(true), data_type(Drivers::DataType::UNKNOWN)
        , scaling_factor(1.0), scaling_offset(0.0), messages_received(0)
        , last_received(std::chrono::system_clock::now()) {}
};

/**
 * @brief MQTT 발행 작업
 */
struct MQTTPublishTask {
    std::string topic;                    ///< 발행할 토픽
    std::string payload;                  ///< 발행할 페이로드
    int qos;                              ///< QoS 레벨
    bool retained;                        ///< Retain 플래그
    std::chrono::system_clock::time_point scheduled_time; ///< 예약 시간
    int retry_count;                      ///< 재시도 횟수
    
    MQTTPublishTask()
        : qos(1), retained(false)
        , scheduled_time(std::chrono::system_clock::now())
        , retry_count(0) {}
};

/**
 * @brief MQTT 워커 설정
 */
struct MQTTWorkerConfig {
    std::string broker_url;               ///< 브로커 URL (mqtt://host:port)
    std::string client_id;                ///< 클라이언트 ID
    std::string username;                 ///< 사용자명 (옵션)
    std::string password;                 ///< 비밀번호 (옵션)
    bool use_ssl;                         ///< SSL/TLS 사용 여부
    int keep_alive_interval;              ///< Keep-alive 간격 (초)
    bool clean_session;                   ///< Clean Session 플래그
    bool auto_reconnect;                  ///< 자동 재연결
    uint32_t message_timeout_ms;          ///< 메시지 타임아웃 (밀리초)
    uint32_t publish_retry_count;         ///< 발행 재시도 횟수
    
    MQTTWorkerConfig()
        : use_ssl(false), keep_alive_interval(60), clean_session(true)
        , auto_reconnect(true), message_timeout_ms(30000), publish_retry_count(3) {}
};

/**
 * @brief MQTT 워커 통계
 */
struct MQTTWorkerStatistics {
    std::atomic<uint64_t> messages_received{0};    ///< 수신된 메시지 수
    std::atomic<uint64_t> messages_published{0};   ///< 발행된 메시지 수
    std::atomic<uint64_t> subscribe_operations{0}; ///< 구독 작업 수
    std::atomic<uint64_t> publish_operations{0};   ///< 발행 작업 수
    std::atomic<uint64_t> successful_publishes{0}; ///< 성공한 발행 수
    std::atomic<uint64_t> failed_operations{0};    ///< 실패한 작업 수
    std::atomic<uint64_t> json_parse_errors{0};    ///< JSON 파싱 에러 수
    
    std::chrono::system_clock::time_point start_time;     ///< 통계 시작 시간
    std::chrono::system_clock::time_point last_reset;     ///< 마지막 리셋 시간
};

/**
 * @class MQTTWorker
 * @brief MQTT 프로토콜 워커
 * 
 * @details
 * BaseDeviceWorker를 상속받아 MQTT 프로토콜에 특화된 기능을 제공합니다.
 * MQTTDriver를 내부적으로 사용하여 실제 MQTT 통신을 수행합니다.
 * - MQTT 토픽 구독/발행 관리
 * - JSON 페이로드 파싱 및 데이터 추출
 * - 비동기 메시지 처리
 * - 토픽 기반 데이터 포인트 매핑
 */
class MQTTWorker : public BaseDeviceWorker {

public:
    // =============================================================================
    // 생성자 및 소멸자
    // =============================================================================
    
    /**
     * @brief 생성자
     * @param device_info 디바이스 정보
     * @param redis_client Redis 클라이언트
     * @param influx_client InfluxDB 클라이언트
     */
    explicit MQTTWorker(
        const PulseOne::DeviceInfo& device_info,
        std::shared_ptr<RedisClient> redis_client,
        std::shared_ptr<InfluxClient> influx_client
    );
    
    /**
     * @brief 소멸자
     */
    virtual ~MQTTWorker();

    // =============================================================================
    // BaseDeviceWorker 인터페이스 구현
    // =============================================================================
    /**
     * @brief QoS enum을 정수로 변환 (정적 메서드)
     * @param qos QoS enum
     * @return QoS 정수 값
     */
    static int QosToInt(MqttQoS qos) {
        return static_cast<int>(qos);
    }
    
    /**
     * @brief 정수를 QoS enum으로 변환 (정적 메서드)
     * @param qos_int QoS 정수 값
     * @return QoS enum
     */
    static MqttQoS IntToQos(int qos_int) {
        switch (qos_int) {
            case 0: return MqttQoS::AT_MOST_ONCE;
            case 2: return MqttQoS::EXACTLY_ONCE;
            default: return MqttQoS::AT_LEAST_ONCE;
        }
    }

    std::future<bool> Start() override;
    std::future<bool> Stop() override;
    bool EstablishConnection() override;
    bool CloseConnection() override;
    bool CheckConnection() override;
    bool SendKeepAlive() override;

    // =============================================================================
    // MQTT 공개 인터페이스
    // =============================================================================
    
    /**
     * @brief MQTT 워커 설정
     * @param config MQTT 워커 설정
     */
    void ConfigureMQTTWorker(const MQTTWorkerConfig& config);
    
    /**
     * @brief MQTT 워커 통계 정보 조회
     * @return JSON 형태의 통계 정보
     */
    std::string GetMQTTWorkerStats() const;
    
    /**
     * @brief MQTT 워커 통계 리셋
     */
    void ResetMQTTWorkerStats();
    
    /**
     * @brief 토픽 구독 추가
     * @param subscription 구독 정보
     * @return 성공 시 true
     */
    bool AddSubscription(const MQTTSubscription& subscription);
    
    /**
     * @brief 토픽 구독 제거
     * @param topic 토픽 이름
     * @return 성공 시 true
     */
    bool RemoveSubscription(const std::string& topic);
    
    /**
     * @brief 메시지 발행
     * @param topic 토픽
     * @param payload 페이로드
     * @param qos QoS 레벨
     * @param retained Retain 플래그
     * @return 성공 시 true
     */
    bool PublishMessage(const std::string& topic, const std::string& payload, 
                       int qos = 1, bool retained = false);
    
    /**
     * @brief 활성 구독 목록 조회
     * @return JSON 형태의 구독 목록
     */
    std::string GetActiveSubscriptions() const;

protected:
    // =============================================================================
    // MQTT 워커 핵심 기능
    // =============================================================================
    
    /**
     * @brief MQTTDriver 초기화
     * @return 성공 시 true
     */
    bool InitializeMQTTDriver();
    
    /**
     * @brief MQTTDriver 종료
     */
    void ShutdownMQTTDriver();
    
    /**
     * @brief 데이터 포인트에서 MQTT 구독 생성
     * @param points 데이터 포인트 목록
     * @return 성공 시 true
     */
    bool CreateSubscriptionsFromDataPoints(const std::vector<PulseOne::DataPoint>& points);
    
    /**
     * @brief 수신된 MQTT 메시지 처리
     * @param topic 토픽
     * @param payload 페이로드
     * @return 성공 시 true
     */
    bool ProcessReceivedMessage(const std::string& topic, const std::string& payload);
    
    /**
     * @brief JSON 페이로드에서 값 추출
     * @param payload JSON 페이로드
     * @param json_path JSON 경로 (예: "sensors.temperature")
     * @param extracted_value 추출된 값 (출력)
     * @return 성공 시 true
     */
    bool ExtractValueFromJSON(const std::string& payload, const std::string& json_path,
                             Drivers::DataValue& extracted_value);
    
    /**
     * @brief 데이터 포인트에서 MQTT 토픽 정보 파싱
     * @param point 데이터 포인트
     * @param topic 토픽 (출력)
     * @param json_path JSON 경로 (출력)
     * @param qos QoS 레벨 (출력)
     * @return 성공 시 true
     */
    bool ParseMQTTTopic(const PulseOne::DataPoint& point, std::string& topic,
                       std::string& json_path, int& qos);

    // =============================================================================
    // 멤버 변수 (protected)
    // =============================================================================
    
    /// MQTT 워커 설정
    MQTTWorkerConfig worker_config_;
    
    /// MQTT 워커 통계
    mutable MQTTWorkerStatistics worker_stats_;
    
    /// MQTT 드라이버 인스턴스
    std::unique_ptr<Drivers::MqttDriver> mqtt_driver_;
    
    /// 활성 구독 맵 (Topic -> Subscription Info)
    std::map<std::string, MQTTSubscription> active_subscriptions_;
    
    /// 발행 작업 큐
    std::queue<MQTTPublishTask> publish_queue_;
    
    /// 워커 스레드들
    std::unique_ptr<std::thread> message_processor_thread_;
    std::unique_ptr<std::thread> publish_processor_thread_;
    
    /// 스레드 실행 플래그
    std::atomic<bool> threads_running_;
    
    /// 구독 맵 뮤텍스
    mutable std::mutex subscriptions_mutex_;
    
    /// 발행 큐 뮤텍스
    mutable std::mutex publish_queue_mutex_;
    
    /// 발행 큐 조건 변수
    std::condition_variable publish_queue_cv_;

private:
    // =============================================================================
    // 내부 메서드
    // =============================================================================
    
    /**
     * @brief MQTT 워커 설정 파싱
     * @details device_info의 config_json에서 MQTT 워커 설정 추출
     * @return 성공 시 true
     */
    bool ParseMQTTWorkerConfig();
    
    /**
     * @brief 메시지 처리 스레드 함수
     */
    void MessageProcessorThreadFunction();
    
    /**
     * @brief 발행 처리 스레드 함수
     */
    void PublishProcessorThreadFunction();
    
    /**
     * @brief MQTTDriver 설정 생성
     * @return 드라이버 설정
     */
    Drivers::DriverConfig CreateDriverConfig();
    
    /**
     * @brief 통계 업데이트
     * @param operation 작업 타입
     * @param success 성공 여부
     */
    void UpdateWorkerStats(const std::string& operation, bool success);
    
    /**
     * @brief MQTT 메시지 콜백 (정적 메서드)
     * @param worker_instance 워커 인스턴스
     * @param topic 토픽
     * @param payload 페이로드
     */
    static void MessageCallback(MQTTWorker* worker_instance, 
                               const std::string& topic, const std::string& payload);
};

} // namespace Workers
} // namespace PulseOne

#endif // MQTT_WORKER_H