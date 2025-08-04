// =============================================================================
// collector/include/Drivers/Mqtt/MqttDriver.h
// MQTT 프로토콜 드라이버 헤더 - 표준화 완성본 + 확장성 준비
// =============================================================================

#ifndef PULSEONE_DRIVERS_MQTT_DRIVER_H
#define PULSEONE_DRIVERS_MQTT_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Drivers/Common/DriverLogger.h"
#include "Common/DriverError.h"
#include "Common/DriverStatistics.h"  // ✅ 표준 통계 구조
#include "Common/Structs.h"           // ✅ DataPoint, DataValue, TimestampedValue 등
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
// 전방 선언 (기존 + 확장 준비)
// =============================================================================
class MqttCallbackImpl;
class MqttActionListener;

// ✅ 고급 기능 클래스들 전방 선언 (확장성 준비)
class MqttDiagnostics;
class MqttLoadBalancer;
class MqttFailover;
class MqttSecurity;
class MqttPerformance;

// 고급 기능 관련 구조체들
struct QosAnalysis;
struct BrokerStats;
struct FailoverConfig;
struct SecurityConfig;
struct PerformanceConfig;

/**
 * @brief MQTT 프로토콜 드라이버 - 표준화 완성본
 * @details Eclipse Paho C++ 기반 + 표준 DriverStatistics + 확장성 준비
 * 
 * 🎯 주요 개선사항:
 * - ✅ 표준 DriverStatistics 완全 적용
 * - ✅ 중복 통계 필드 제거 (메모리 효율성)
 * - ✅ 고급 기능 확장성 준비 (Facade 패턴)
 * - ✅ ModbusDriver와 동일한 아키텍처 패턴
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
 * 
 * // 향후 고급 기능 (2단계에서 구현)
 * // driver->EnableDiagnostics();
 * // driver->EnableLoadBalancing(broker_urls);
 * // driver->EnableFailover();
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

    // ✅ IProtocolDriver 순수 가상 함수들 구현
    ProtocolType GetProtocolType() const override;
    Structs::DriverStatus GetStatus() const override;
    ErrorInfo GetLastError() const override;
    bool Stop() override;
    void SetError(const std::string& error_message);
    
    // =======================================================================
    // MQTT 핵심 기능 (현재 구현된 기능들 유지)
    // =======================================================================
    
    // 연결 관리
    bool ConnectWithOptions(const std::string& broker_url, const std::string& client_id);
    bool SetConnectionOptions(int keep_alive_seconds, bool clean_session, bool auto_reconnect);
    
    // 발행/구독
    bool Publish(const std::string& topic, const std::string& payload, int qos = 0, bool retain = false);
    bool Subscribe(const std::string& topic, int qos = 0);
    bool Unsubscribe(const std::string& topic);
    bool UnsubscribeAll();
    
    // 메시지 처리
    void SetMessageCallback(std::function<void(const std::string&, const std::string&, int)> callback);
    void SetConnectionCallback(std::function<void(bool, const std::string&)> callback);
    
    // 상태 조회
    std::string GetBrokerUrl() const;
    std::string GetClientId() const;
    std::vector<std::string> GetSubscribedTopics() const;
    bool IsSubscribed(const std::string& topic) const;
    
    // 설정 관리
    void SetQoS(int default_qos);
    void SetKeepAlive(int seconds);
    void SetRetryCount(int count);
    void SetTimeout(int timeout_ms);
    
    // 진단 및 유지보수 (기존 기능 유지)
    std::string GetDiagnosticsJSON() const;
    std::string GetConnectionInfo() const;
    bool TestConnection() const;
    void ResetConnection();
    
    // =======================================================================
    // 고급 기능 확장 준비 (2단계에서 구현 예정)
    // =======================================================================
    
    // 📊 진단 기능 (향후 구현)
    // bool EnableDiagnostics(bool message_tracking = true, bool qos_analysis = true);
    // void DisableDiagnostics();
    // bool IsDiagnosticsEnabled() const;
    // double GetMessageLossRate() const;
    // double GetAverageLatency() const;
    
    // ⚖️ 로드 밸런싱 (향후 구현)
    // bool EnableLoadBalancing(const std::vector<std::string>& broker_urls);
    // void DisableLoadBalancing();
    // std::string GetCurrentBroker() const;
    // BrokerStats GetBrokerStats() const;
    
    // 🔄 페일오버 (향후 구현)
    // bool EnableFailover(int failure_threshold = 3);
    // void AddBackupBroker(const std::string& broker_url);
    // std::vector<std::string> GetBackupBrokers() const;
    
    // 🔐 고급 보안 (향후 구현)
    // bool EnableSecurity(const SecurityConfig& config);
    // bool LoadCertificate(const std::string& cert_path);
    // void SetTokenAuthentication(const std::string& token);
    
    // ⚡ 성능 최적화 (향후 구현)
    // bool EnablePerformanceMode(const PerformanceConfig& config);
    // void SetBatchSize(size_t batch_size);
    // void SetCompressionLevel(int level);

    // =======================================================================
    // 콜백 및 이벤트 처리 (Eclipse Paho 연동)
    // =======================================================================
    void connected(const std::string& cause);
    void connection_lost(const std::string& cause);
    void message_arrived(mqtt::const_message_ptr msg);
    void delivery_complete(mqtt::delivery_token_ptr token);
    void on_failure(const mqtt::token& token);
    void on_success(const mqtt::token& token);

private:
    // =======================================================================
    // Core 멤버 변수들 - 최적화된 구조
    // =======================================================================
    
    // ✅ 표준 통계 (DriverStatistics 사용 - 중복 제거됨)
    DriverStatistics driver_statistics_{"MQTT"};
    
    // 상태 관리
    std::atomic<Structs::DriverStatus> status_;
    std::atomic<bool> is_connected_;
    std::atomic<bool> connection_in_progress_;
    std::atomic<bool> stop_workers_;
    std::atomic<bool> need_reconnect_;
    
    // 에러 정보
    mutable std::mutex error_mutex_;
    ErrorInfo last_error_;
    
    // MQTT 클라이언트 및 콜백
    std::unique_ptr<mqtt::async_client> mqtt_client_;
    std::shared_ptr<MqttCallbackImpl> mqtt_callback_;
    std::shared_ptr<MqttActionListener> mqtt_action_listener_;
    
    // 연결 설정
    std::string broker_url_;
    std::string client_id_;
    int default_qos_;
    int keep_alive_seconds_;
    int retry_count_;
    int timeout_ms_;
    bool clean_session_;
    bool auto_reconnect_;
    
    // 구독 관리
    mutable std::mutex subscriptions_mutex_;
    std::map<std::string, int> subscriptions_;  // topic -> qos
    
    // 메시지 처리
    std::function<void(const std::string&, const std::string&, int)> message_callback_;
    std::function<void(bool, const std::string&)> connection_callback_;
    
    // 스레드 관리
    std::thread connection_monitor_thread_;
    std::atomic<bool> connection_monitor_running_;
    std::condition_variable connection_cv_;
    std::mutex connection_mutex_;
    
    // 메시지 큐 및 처리
    std::queue<std::pair<std::string, std::string>> message_queue_;
    std::mutex message_queue_mutex_;
    std::condition_variable message_queue_cv_;
    std::thread message_processor_thread_;
    std::atomic<bool> message_processor_running_;
    
    // 진단 및 로깅
    std::shared_ptr<DriverLogger> logger_;
    std::atomic<bool> diagnostics_enabled_;
    std::atomic<bool> packet_logging_enabled_;
    std::atomic<bool> console_output_enabled_;
    
    // 시간 추적
    std::chrono::system_clock::time_point last_successful_operation_;
    std::chrono::system_clock::time_point connection_start_time_;
    
    // =======================================================================
    // ❌ 제거된 중복 통계 필드들 (DriverStatistics로 대체됨)
    // =======================================================================
    // mutable std::mutex stats_mutex_;                    // 삭제 - DriverStatistics 내장
    // std::atomic<uint64_t> total_messages_received_;     // 삭제 - statistics_.total_reads 사용
    // std::atomic<uint64_t> total_messages_sent_;         // 삭제 - statistics_.total_writes 사용  
    // std::atomic<uint64_t> total_bytes_received_;        // 삭제 - protocol_counters 사용
    // std::atomic<uint64_t> total_bytes_sent_;            // 삭제 - protocol_counters 사용
    
    // =======================================================================
    // 고급 기능 확장 슬롯 (2단계에서 구현)
    // =======================================================================
    // std::unique_ptr<MqttDiagnostics> diagnostics_;      // 향후 구현
    // std::unique_ptr<MqttLoadBalancer> load_balancer_;   // 향후 구현
    // std::unique_ptr<MqttFailover> failover_;            // 향후 구현
    // std::unique_ptr<MqttSecurity> security_;            // 향후 구현
    // std::unique_ptr<MqttPerformance> performance_;      // 향후 구현
    
    // =======================================================================
    // Private 헬퍼 메서드들
    // =======================================================================
    
    // 초기화 및 정리
    bool InitializeMqttClient();
    void CleanupMqttClient();
    bool ParseDriverConfig(const DriverConfig& config);
    void InitializeMqttCounters();
    
    // 통계 관리 (표준화됨)
    void UpdateStats(const std::string& operation, bool success, double duration_ms = 0.0);
    void UpdateMessageStats(const std::string& operation, size_t payload_size, int qos, bool success);
    void UpdateConnectionStats(bool connected, const std::string& reason = "");
    
    // 연결 관리
    bool EstablishConnection();
    void HandleConnectionLoss(const std::string& cause);
    void ConnectionMonitorLoop();
    bool ShouldReconnect() const;
    
    // 메시지 처리
    void MessageProcessorLoop();
    void ProcessReceivedMessage(const std::string& topic, const std::string& payload, int qos);
    bool SendMessage(const std::string& topic, const std::string& payload, int qos, bool retain);
    
    // 구독 관리
    bool RestoreSubscriptions();
    void ClearSubscriptions();
    
    // 유틸리티
    std::string GenerateClientId() const;
    bool ValidateTopicName(const std::string& topic) const;
    std::string FormatConnectionInfo() const;
    
    // 진단 및 로깅
    void LogMessage(const std::string& level, const std::string& message, const std::string& category = "MQTT") const;
    void LogPacket(const std::string& direction, const std::string& topic, const std::string& payload, int qos) const;
    
    // 확장성 헬퍼 (향후 사용)
    // bool IsAdvancedFeatureEnabled(const std::string& feature_name) const;
    // void NotifyAdvancedFeatures(const std::string& event, const std::map<std::string, std::string>& data);
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_MQTT_DRIVER_H