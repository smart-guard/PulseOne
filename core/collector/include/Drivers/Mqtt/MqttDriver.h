// =============================================================================
// collector/include/Drivers/Mqtt/MqttDriver.h
// MQTT 프로토콜 드라이버 헤더 - 컴파일 에러 수정 완료
// =============================================================================

#ifndef PULSEONE_DRIVERS_MQTT_DRIVER_H
#define PULSEONE_DRIVERS_MQTT_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Common/DriverError.h"
#include "Common/DriverStatistics.h"
#include "Common/Structs.h"
#include "Common/Enums.h"  // ✅ DataQuality enum 포함
#include "Drivers/Mqtt/MqttDiagnostics.h"
#include "Drivers/Mqtt/MqttFailover.h"
#include "Drivers/Mqtt/MqttLoadBalancer.h"
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <queue>
#include <optional>
#include <condition_variable>
#include <deque>
#include <future>
#include <set>
#include <chrono>

#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

// Eclipse Paho MQTT C++ 헤더들
#include <mqtt/async_client.h>
#include <mqtt/callback.h>
#include <mqtt/iaction_listener.h>
#include <mqtt/connect_options.h>
#include <mqtt/message.h>
#include <mqtt/token.h>

namespace PulseOne {
namespace Drivers {
    // 타입 별칭들 - IProtocolDriver와 동일하게
    using ErrorCode = PulseOne::Structs::ErrorCode;
    using ErrorInfo = PulseOne::Structs::ErrorInfo;
    using ProtocolType = PulseOne::Enums::ProtocolType;
    using DataPoint = PulseOne::Structs::DataPoint;
    using DataValue = PulseOne::Structs::DataValue;
    using TimestampedValue = PulseOne::Structs::TimestampedValue;

// =============================================================================
// 전방 선언
// =============================================================================
class MqttCallbackImpl;
class MqttActionListener;

/**
 * @brief MQTT 프로토콜 드라이버 - 표준화 완성본
 * @details Eclipse Paho C++ 기반 + 표준 DriverStatistics
 * 
 * 🎯 주요 특징:
 * - ✅ 표준 DriverStatistics 완전 적용
 * - ✅ Eclipse Paho C++ 기반 MQTT 3.1.1/5.0 지원
 * - ✅ ModbusDriver와 동일한 아키텍처 패턴
 * - ✅ 멀티스레드 안전성 보장
 * 
 * 사용 예시:
 * auto driver = std::make_shared<MqttDriver>();
 * driver->Initialize(config);
 * driver->Connect();
 * 
 * // 표준 통계 접근
 * const auto& stats = driver->GetStatistics();
 * uint64_t reads = stats.total_reads.load();
 * uint64_t mqtt_messages = stats.GetProtocolCounter("mqtt_messages");
 */
class MqttDriver : public IProtocolDriver {
public:
    // =======================================================================
    // 생성자/소멸자
    // =======================================================================
    MqttDriver();
    virtual ~MqttDriver();
    
    // 복사/이동 방지 (리소스 관리)
    MqttDriver(const MqttDriver&) = delete;
    MqttDriver& operator=(const MqttDriver&) = delete;
    MqttDriver(MqttDriver&&) = delete;
    MqttDriver& operator=(MqttDriver&&) = delete;

    // =======================================================================
    // IProtocolDriver 인터페이스 구현
    // =======================================================================
    bool Initialize(const DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    bool ReadValues(const std::vector<DataPoint>& points, 
                   std::vector<TimestampedValue>& values) override;
    bool WriteValue(const DataPoint& point, const DataValue& value) override;
    
    // ✅ 표준 통계 인터페이스 (ModbusDriver와 동일)
    const DriverStatistics& GetStatistics() const override;
    void ResetStatistics() override;
    
    bool Start() override;
    bool Stop() override;

    // ✅ IProtocolDriver 순수 가상 함수들 구현
    ProtocolType GetProtocolType() const override;
    Structs::DriverStatus GetStatus() const override;
    ErrorInfo GetLastError() const override;
    
    // =======================================================================
    // MQTT 특화 인터페이스
    // =======================================================================
    
    /**
     * @brief MQTT 토픽 구독
     * @param topic 구독할 토픽
     * @param qos QoS 레벨 (0, 1, 2)
     * @return 성공 여부
     */
    bool Subscribe(const std::string& topic, int qos = 0);
    
    /**
     * @brief MQTT 토픽 구독 해제
     * @param topic 구독 해제할 토픽
     * @return 성공 여부
     */
    bool Unsubscribe(const std::string& topic);
    
    /**
     * @brief MQTT 메시지 발행
     * @param topic 발행할 토픽
     * @param payload 메시지 내용
     * @param qos QoS 레벨
     * @param retain 유지 여부
     * @return 성공 여부
     */
    bool Publish(const std::string& topic, const std::string& payload, int qos = 0, bool retain = false);
    
    // =======================================================================
    // 설정 및 상태 조회
    // =======================================================================
    
    /**
     * @brief 브로커 URL 조회
     */
    std::string GetBrokerUrl() const;
    
    /**
     * @brief 클라이언트 ID 조회
     */
    std::string GetClientId() const;
    
    /**
     * @brief 구독 중인 토픽 목록 조회
     */
    std::vector<std::string> GetSubscribedTopics() const;
    
    /**
     * @brief 특정 토픽 구독 여부 확인
     */
    bool IsSubscribed(const std::string& topic) const;
    
    /**
     * @brief 진단 정보 JSON 형태로 조회
     */
    std::string GetDiagnosticsJSON() const;
    
    // =======================================================================
    // 콜백 메서드들 (Eclipse Paho 콜백에서 호출)
    // =======================================================================
    
    /**
     * @brief 연결 성공 콜백
     */
    void OnConnected(const std::string& cause);
    
    /**
     * @brief 연결 끊김 콜백
     */
    void OnConnectionLost(const std::string& cause);
    
    /**
     * @brief 메시지 수신 콜백
     */
    void OnMessageArrived(mqtt::const_message_ptr msg);
    
    /**
     * @brief 메시지 전송 완료 콜백
     */
    void OnDeliveryComplete(mqtt::delivery_token_ptr token);
    
    /**
     * @brief 액션 실패 콜백
     */
    void OnActionFailure(const mqtt::token& token);
    
    /**
     * @brief 액션 성공 콜백
     */
    void OnActionSuccess(const mqtt::token& token);

    // =======================================================================
    // 🔍 진단 기능 제어 (MqttDiagnostics)
    // =======================================================================
    
    /**
     * @brief 고급 진단 기능 활성화/비활성화
     * @param enable 활성화 여부
     * @param packet_logging 패킷 로깅 활성화 여부
     * @param console_output 콘솔 출력 활성화 여부
     * @return 성공 여부
     */
    bool EnableDiagnostics(bool enable = true, bool packet_logging = false, bool console_output = false);
    
    /**
     * @brief 진단 기능 비활성화
     */
    void DisableDiagnostics();
    
    /**
     * @brief 진단 기능 활성화 여부 확인
     * @return 활성화 상태
     */
    bool IsDiagnosticsEnabled() const;
    
    /**
     * @brief 상세 진단 정보 JSON 형태로 조회
     * @return JSON 형태의 진단 정보
     */
    std::string GetDetailedDiagnosticsJSON() const;
    
    /**
     * @brief 메시지 손실률 조회
     * @return 손실률 (0.0 ~ 100.0)
     */
    double GetMessageLossRate() const;
    
    /**
     * @brief QoS별 성능 분석 조회
     * @return QoS별 분석 데이터 맵
     */
    std::map<int, QosAnalysis> GetQosAnalysis() const;
    
    /**
     * @brief 토픽별 상세 통계 조회
     * @return 토픽별 통계 맵
     */
    std::map<std::string, TopicStats> GetDetailedTopicStats() const;
    
    // =======================================================================
    // 🔄 페일오버 기능 제어 (MqttFailover)
    // =======================================================================
    
    /**
     * @brief 페일오버 기능 활성화
     * @param backup_brokers 백업 브로커 URL 리스트
     * @param strategy 재연결 전략 (선택사항)
     * @return 성공 여부
     */
    bool EnableFailover(const std::vector<std::string>& backup_brokers, 
                       const ReconnectStrategy& strategy = ReconnectStrategy{});
    
    /**
     * @brief 페일오버 기능 비활성화
     */
    void DisableFailover();
    
    /**
     * @brief 페일오버 기능 활성화 여부 확인
     * @return 활성화 상태
     */
    bool IsFailoverEnabled() const;
    
    /**
     * @brief 수동 페일오버 트리거
     * @param reason 페일오버 원인
     * @return 성공 여부
     */
    bool TriggerManualFailover(const std::string& reason = "Manual failover");
    
    /**
     * @brief 최적 브로커로 전환
     * @return 전환 성공 여부
     */
    bool SwitchToOptimalBroker();
    
    /**
     * @brief 백업 브로커 추가
     * @param broker_url 백업 브로커 URL
     * @param name 브로커 이름 (선택사항)
     * @param priority 우선순위 (선택사항)
     */
    void AddBackupBroker(const std::string& broker_url, const std::string& name = "", int priority = 0);
    
    /**
     * @brief 현재 활성 브로커 정보 조회
     * @return 브로커 정보
     */
    BrokerInfo GetCurrentBrokerInfo() const;
    
    /**
     * @brief 모든 브로커 상태 조회
     * @return 브로커 정보 리스트
     */
    std::vector<BrokerInfo> GetAllBrokerStatus() const;
    
    /**
     * @brief 재연결 통계 조회
     * @return JSON 형태의 통계 정보
     */
    std::string GetFailoverStatistics() const;
    
    // =======================================================================
    // ⚖️ 로드밸런싱 기능 제어 (MqttLoadBalancer)
    // =======================================================================
    
    /**
     * @brief 로드밸런싱 기능 활성화
     * @param brokers 로드밸런싱 대상 브로커 리스트
     * @param algorithm 기본 로드밸런싱 알고리즘
     * @return 성공 여부
     */
    bool EnableLoadBalancing(const std::vector<std::string>& brokers, 
                            LoadBalanceAlgorithm algorithm = LoadBalanceAlgorithm::ROUND_ROBIN);
    
    /**
     * @brief 로드밸런싱 기능 비활성화
     */
    void DisableLoadBalancing();
    
    /**
     * @brief 로드밸런싱 기능 활성화 여부 확인
     * @return 활성화 상태
     */
    bool IsLoadBalancingEnabled() const;
    
    /**
     * @brief 토픽에 대한 최적 브로커 선택
     * @param topic 토픽명
     * @param message_size 메시지 크기 (선택사항)
     * @return 선택된 브로커 URL
     */
    std::string SelectOptimalBroker(const std::string& topic, size_t message_size = 0);
    
    /**
     * @brief 라우팅 규칙 추가
     * @param rule 라우팅 규칙
     */
    void AddRoutingRule(const RoutingRule& rule);
    
    /**
     * @brief 브로커 가중치 설정
     * @param broker_url 브로커 URL
     * @param weight 가중치 (1-100)
     */
    void SetBrokerWeight(const std::string& broker_url, int weight);
    
    /**
     * @brief 부하 재분산 트리거
     * @param force_rebalance 강제 재분산 여부
     * @return 재분산 수행 여부
     */
    bool TriggerLoadRebalancing(bool force_rebalance = false);
    
    /**
     * @brief 로드밸런싱 통계 조회
     * @return JSON 형태의 통계 정보
     */
    std::string GetLoadBalancingStatistics() const;
    
    // =======================================================================
    // 🔐 보안 기능 제어 (MqttSecurity - 향후 구현)
    // =======================================================================
    
    /**
     * @brief 고급 보안 기능 활성화 (향후 구현)
     * @param enable 활성화 여부
     * @return 성공 여부
     */
    bool EnableSecurity(bool enable = true);
    
    /**
     * @brief 클라이언트 인증서 설정 (향후 구현)
     * @param cert_path 인증서 파일 경로
     * @param key_path 개인키 파일 경로
     * @param ca_path CA 인증서 파일 경로
     * @return 성공 여부
     */
    bool SetClientCertificate(const std::string& cert_path, const std::string& key_path, const std::string& ca_path);
    
    // =======================================================================
    // ⚡ 성능 최적화 기능 제어 (MqttPerformance - 향후 구현)
    // =======================================================================
    
    /**
     * @brief 성능 최적화 기능 활성화 (향후 구현)
     * @param enable 활성화 여부
     * @return 성공 여부
     */
    bool EnablePerformanceOptimization(bool enable = true);
    
    /**
     * @brief 메시지 압축 활성화 (향후 구현)
     * @param enable 활성화 여부
     * @param compression_level 압축 레벨 (1-9)
     * @return 성공 여부
     */
    bool EnableMessageCompression(bool enable = true, int compression_level = 6);
    
    /**
     * @brief 배치 처리 활성화 (향후 구현)
     * @param enable 활성화 여부
     * @param batch_size 배치 크기
     * @param batch_timeout_ms 배치 타임아웃
     * @return 성공 여부
     */
    bool EnableBatchProcessing(bool enable = true, size_t batch_size = 100, int batch_timeout_ms = 1000);

    
    /**
     * @brief 로드밸런싱 상태 JSON 조회
     * @return JSON 형태의 상태 정보
     */
    std::string GetLoadBalancingStatusJSON() const;
    
 private:
    // =======================================================================
    // Core 멤버 변수들 (필수)
    // =======================================================================
    
    // ✅ 표준 통계 구조체 (ModbusDriver와 동일)
    DriverStatistics driver_statistics_;
    
    // 드라이버 상태 관리
    std::atomic<Structs::DriverStatus> status_;
    std::atomic<bool> is_connected_;
    std::atomic<bool> connection_in_progress_;
    
    // 에러 관리
    mutable std::mutex error_mutex_;
    ErrorInfo last_error_;
    
    // =======================================================================
    // MQTT 클라이언트 관련
    // =======================================================================
    
    // Eclipse Paho MQTT 클라이언트
    std::unique_ptr<mqtt::async_client> mqtt_client_;
    std::shared_ptr<MqttCallbackImpl> mqtt_callback_;
    std::shared_ptr<MqttActionListener> mqtt_action_listener_;
    
    // 연결 설정
    std::string broker_url_;
    std::string client_id_;
    int default_qos_;
    int keep_alive_seconds_;
    int timeout_ms_;
    bool clean_session_;
    bool auto_reconnect_;
    
    // =======================================================================
    // 구독 관리
    // =======================================================================
    
    mutable std::mutex subscriptions_mutex_;
    std::unordered_map<std::string, int> subscriptions_;  // topic -> qos
    
    // =======================================================================
    // 스레드 관리
    // =======================================================================
    
    // 메시지 처리 스레드
    std::atomic<bool> message_processor_running_;
    std::thread message_processor_thread_;
    std::mutex message_queue_mutex_;
    std::condition_variable message_queue_cv_;
    std::queue<std::pair<std::string, std::string>> message_queue_;  // topic, payload
    
    // 연결 모니터링 스레드
    std::atomic<bool> connection_monitor_running_;
    std::thread connection_monitor_thread_;
    std::mutex connection_mutex_;
    std::condition_variable connection_cv_;
    
    // =======================================================================
    // 로깅 및 진단
    // =======================================================================
    
    std::atomic<bool> console_output_enabled_;
    std::atomic<bool> packet_logging_enabled_;
    std::chrono::system_clock::time_point connection_start_time_;
    

    // =======================================================================
    // 고급 기능 클래스들 (선택적 활성화)
    // =======================================================================
    
    // 진단 기능
    std::unique_ptr<MqttDiagnostics> diagnostics_;
    
    // 페일오버 기능
    std::unique_ptr<MqttFailover> failover_;
    
    // 로드밸런싱 기능
    std::unique_ptr<MqttLoadBalancer> load_balancer_;
    
    // 보안 기능 (향후 구현)
    // std::unique_ptr<MqttSecurity> security_;
    
    // 성능 최적화 기능 (향후 구현)
    // std::unique_ptr<MqttPerformance> performance_;

    // =======================================================================
    // 고급 기능 관련 내부 메서드들
    // =======================================================================
    
    /**
     * @brief 고급 기능들에 이벤트 전파
     * @param event_type 이벤트 타입
     * @param data 이벤트 데이터
     */
    void NotifyAdvancedFeatures(const std::string& event_type, const std::string& data = "");
    
    /**
     * @brief 진단 정보에 이벤트 기록
     * @param operation 작업명
     * @param success 성공 여부
     * @param duration_ms 소요 시간
     * @param details 추가 정보
     */
    void RecordDiagnosticEvent(const std::string& operation, bool success, double duration_ms, const std::string& details = "");
    
    /**
     * @brief 페일오버에 연결 상태 변경 알림
     * @param connected 연결 상태
     * @param broker_url 브로커 URL
     * @param reason 원인
     */
    void NotifyConnectionChange(bool connected, const std::string& broker_url, const std::string& reason);
    
    /**
     * @brief 로드밸런서에 메시지 처리 알림
     * @param broker_url 브로커 URL
     * @param topic 토픽명
     * @param success 성공 여부
     * @param processing_time_ms 처리 시간
     */
    void NotifyMessageProcessing(const std::string& broker_url, const std::string& topic, bool success, double processing_time_ms);
    // =======================================================================
    // 내부 메서드들
    // =======================================================================
    
    /**
     * @brief MQTT 특화 통계 카운터들 초기화
     */
    void InitializeMqttCounters();
    
    /**
     * @brief 통계 업데이트
     */
    void UpdateStats(const std::string& operation, bool success, double duration_ms = 0.0);
    
    /**
     * @brief 에러 설정
     */
    void SetError(const std::string& error_message);
    
    /**
     * @brief 로그 메시지 출력
     */
    void LogMessage(const std::string& level, const std::string& message, const std::string& category = "MQTT") const;
    
    /**
     * @brief 패킷 로깅
     */
    void LogPacket(const std::string& direction, const std::string& topic, 
                   const std::string& payload, int qos) const;
    
    /**
     * @brief MQTT 연결 수립
     */
    bool EstablishConnection();
    
    /**
     * @brief MQTT 클라이언트 초기화
     */
    bool InitializeMqttClient();
    
    /**
     * @brief MQTT 클라이언트 정리
     */
    void CleanupMqttClient();
    
    /**
     * @brief 드라이버 설정 파싱
     */
    bool ParseDriverConfig(const DriverConfig& config);
    
    /**
     * @brief 클라이언트 ID 생성
     */
    std::string GenerateClientId() const;
    
    /**
     * @brief 메시지 처리 루프
     */
    void MessageProcessorLoop();
    
    /**
     * @brief 수신된 메시지 처리
     */
    void ProcessReceivedMessage(const std::string& topic, const std::string& payload, int qos);
    
    /**
     * @brief 연결 모니터링 루프
     */
    void ConnectionMonitorLoop();
    
    /**
     * @brief 재연결 필요 여부 확인
     */
    bool ShouldReconnect() const;
    
    /**
     * @brief 구독 복원
     */
    bool RestoreSubscriptions();
    
    /**
     * @brief 연결 끊김 처리
     */
    void HandleConnectionLoss(const std::string& cause);
    // 브로커 전환을 위한 헬퍼 메서드들
    bool SwitchBroker(const std::string& new_broker_url);
    bool CreateMqttClientForBroker(const std::string& broker_url);

};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_MQTT_DRIVER_H