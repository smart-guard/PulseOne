// =============================================================================
// collector/include/Drivers/Mqtt/MqttDiagnostics.h
// MQTT 진단 및 모니터링 기능 - 상세한 통계와 분석 제공
// =============================================================================

#ifndef PULSEONE_MQTT_DIAGNOSTICS_H
#define PULSEONE_MQTT_DIAGNOSTICS_H

#include <memory>
#include <string>
#include <map>
#include <deque>
#include <atomic>
#include <mutex>
#include <chrono>
#include <vector>
#include <unordered_map>

#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

namespace PulseOne {
namespace Drivers {

// 전방 선언
class MqttDriver;

// =============================================================================
// 진단 관련 구조체들
// =============================================================================

/**
 * @brief QoS별 성능 분석 데이터
 */
struct QosAnalysis {
    uint64_t total_messages = 0;
    uint64_t successful_deliveries = 0;
    uint64_t failed_deliveries = 0;
    double success_rate = 0.0;
    double avg_latency_ms = 0.0;
    double max_latency_ms = 0.0;
    double min_latency_ms = 999999.0;
    
    void UpdateLatency(double latency_ms) {
        if (latency_ms > max_latency_ms) max_latency_ms = latency_ms;
        if (latency_ms < min_latency_ms) min_latency_ms = latency_ms;
        avg_latency_ms = (avg_latency_ms + latency_ms) / 2.0;
    }
    
    void CalculateSuccessRate() {
        if (total_messages > 0) {
            success_rate = (double)successful_deliveries / total_messages * 100.0;
        }
    }
};

/**
 * @brief 토픽별 통계 정보
 */
struct TopicStats {
    std::string topic_name;
    uint64_t publish_count = 0;
    uint64_t subscribe_count = 0;
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    std::chrono::system_clock::time_point last_activity;
    
    TopicStats() : last_activity(std::chrono::system_clock::now()) {}
};

/**
 * @brief 연결 품질 메트릭
 */
struct ConnectionQuality {
    double uptime_percentage = 0.0;
    uint64_t reconnection_count = 0;
    double avg_reconnection_time_ms = 0.0;
    uint64_t message_loss_count = 0;
    double message_loss_rate = 0.0;
    std::chrono::system_clock::time_point last_quality_check;
    
    ConnectionQuality() : last_quality_check(std::chrono::system_clock::now()) {}
};

/**
 * @brief MQTT 진단 및 모니터링 클래스
 * @details MqttDriver에서 선택적으로 활성화되는 고급 진단 기능
 * 
 * 🎯 주요 기능:
 * - QoS별 성능 분석
 * - 토픽별 상세 통계
 * - 메시지 손실률 추적
 * - 연결 품질 모니터링
 * - 레이턴시 히스토그램
 * - 실시간 진단 데이터 제공
 */
class MqttDiagnostics {
public:
    // =======================================================================
    // 생성자/소멸자
    // =======================================================================
    
    /**
     * @brief MqttDiagnostics 생성자
     * @param parent_driver 부모 MqttDriver 포인터
     */
    explicit MqttDiagnostics(MqttDriver* parent_driver);
    
    /**
     * @brief 소멸자
     */
    ~MqttDiagnostics();
    
    // 복사/이동 비활성화
    MqttDiagnostics(const MqttDiagnostics&) = delete;
    MqttDiagnostics& operator=(const MqttDiagnostics&) = delete;
    
    // =======================================================================
    // 진단 설정 및 제어
    // =======================================================================
    
    /**
     * @brief 메시지 추적 활성화/비활성화
     * @param enable 활성화 여부
     */
    void EnableMessageTracking(bool enable);
    
    /**
     * @brief QoS 분석 활성화/비활성화
     * @param enable 활성화 여부
     */
    void EnableQosAnalysis(bool enable);
    
    /**
     * @brief 패킷 로깅 활성화/비활성화
     * @param enable 활성화 여부
     */
    void EnablePacketLogging(bool enable);
    
    /**
     * @brief 히스토리 최대 크기 설정
     * @param max_size 최대 보관할 히스토리 항목 수
     */
    void SetMaxHistorySize(size_t max_size);
    
    /**
     * @brief 진단 샘플링 간격 설정
     * @param interval_ms 샘플링 간격 (밀리초)
     */
    void SetSamplingInterval(int interval_ms);
    
    // =======================================================================
    // 진단 정보 수집 (MqttDriver에서 호출)
    // =======================================================================
    
    /**
     * @brief 연결 이벤트 기록
     * @param connected 연결 상태 (true: 연결, false: 끊김)
     * @param broker_url 브로커 URL
     * @param duration_ms 연결/재연결에 걸린 시간
     */
    void RecordConnectionEvent(bool connected, const std::string& broker_url, double duration_ms = 0.0);
    
    /**
     * @brief 메시지 발행 기록
     * @param topic 토픽명
     * @param payload_size 메시지 크기
     * @param qos QoS 레벨
     * @param success 성공 여부
     * @param latency_ms 레이턴시
     */
    void RecordPublish(const std::string& topic, size_t payload_size, int qos, bool success, double latency_ms);
    
    /**
     * @brief 메시지 수신 기록
     * @param topic 토픽명
     * @param payload_size 메시지 크기
     * @param qos QoS 레벨
     */
    void RecordReceive(const std::string& topic, size_t payload_size, int qos);
    
    /**
     * @brief 구독 이벤트 기록
     * @param topic 토픽명
     * @param qos QoS 레벨
     * @param success 성공 여부
     */
    void RecordSubscription(const std::string& topic, int qos, bool success);
    
    /**
     * @brief 메시지 손실 기록
     * @param topic 토픽명
     * @param qos QoS 레벨
     * @param reason 손실 원인
     */
    void RecordMessageLoss(const std::string& topic, int qos, const std::string& reason);
    
    /**
     * @brief 일반 작업 성능 기록
     * @param operation 작업명 (connect, disconnect, subscribe 등)
     * @param success 성공 여부
     * @param duration_ms 소요 시간
     */
    void RecordOperation(const std::string& operation, bool success, double duration_ms);
    
    // =======================================================================
    // 진단 정보 조회
    // =======================================================================
    
    /**
     * @brief 전체 진단 정보를 JSON으로 반환
     * @return JSON 형태의 진단 정보
     */
    std::string GetDiagnosticsJSON() const;
    
    /**
     * @brief 메시지 손실률 조회
     * @return 손실률 (0.0 ~ 100.0)
     */
    double GetMessageLossRate() const;
    
    /**
     * @brief 평균 레이턴시 조회
     * @return 평균 레이턴시 (밀리초)
     */
    double GetAverageLatency() const;
    
    /**
     * @brief QoS별 분석 데이터 조회
     * @return QoS별 성능 분석 맵
     */
    std::map<int, QosAnalysis> GetQosAnalysis() const;
    
    /**
     * @brief 토픽별 통계 조회
     * @return 토픽별 통계 맵
     */
    std::map<std::string, TopicStats> GetTopicStatistics() const;
    
    /**
     * @brief 연결 품질 메트릭 조회
     * @return 연결 품질 정보
     */
    ConnectionQuality GetConnectionQuality() const;
    
    /**
     * @brief 레이턴시 히스토그램 조회
     * @return 레이턴시 분포 히스토그램
     */
    std::vector<std::pair<double, uint64_t>> GetLatencyHistogram() const;
    
    /**
     * @brief 최근 에러 목록 조회
     * @param max_count 최대 조회 개수
     * @return 최근 에러 목록
     */
    std::vector<std::string> GetRecentErrors(size_t max_count = 10) const;
    
    // =======================================================================
    // 유틸리티
    // =======================================================================
    
    /**
     * @brief 진단 데이터 초기화
     */
    void Reset();
    
    /**
     * @brief 진단 기능 활성화 여부 확인
     * @return 활성화 상태
     */
    bool IsEnabled() const;

private:
    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // 부모 드라이버
    MqttDriver* parent_driver_;
    
    // 진단 설정
    std::atomic<bool> message_tracking_enabled_{true};
    std::atomic<bool> qos_analysis_enabled_{true};
    std::atomic<bool> packet_logging_enabled_{false};
    std::atomic<bool> enabled_{true};
    
    size_t max_history_size_{1000};
    int sampling_interval_ms_{1000};
    
    // 통계 수집
    mutable std::mutex stats_mutex_;
    
    // QoS별 분석
    std::map<int, QosAnalysis> qos_stats_;
    
    // 토픽별 통계
    std::unordered_map<std::string, TopicStats> topic_stats_;
    
    // 레이턴시 히스토리
    std::deque<double> latency_history_;
    
    // 에러 히스토리
    std::deque<std::pair<std::chrono::system_clock::time_point, std::string>> error_history_;
    
    // 연결 품질 추적
    ConnectionQuality connection_quality_;
    std::chrono::system_clock::time_point connection_start_time_;
    std::chrono::system_clock::time_point last_disconnect_time_;
    
    // 카운터들
    std::atomic<uint64_t> total_messages_{0};
    std::atomic<uint64_t> lost_messages_{0};
    std::atomic<uint64_t> total_bytes_sent_{0};
    std::atomic<uint64_t> total_bytes_received_{0};
    
    // =======================================================================
    // 내부 유틸리티 메서드들
    // =======================================================================
    
    /**
     * @brief 오래된 히스토리 데이터 정리
     */
    void CleanOldHistory();
    
    /**
     * @brief 레이턴시 히스토그램 업데이트
     * @param latency_ms 레이턴시
     */
    void UpdateLatencyHistogram(double latency_ms);
    
    /**
     * @brief 연결 품질 메트릭 업데이트
     */
    void UpdateConnectionQuality();
    
    /**
     * @brief 에러 기록
     * @param error_message 에러 메시지
     */
    void RecordError(const std::string& error_message);
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_MQTT_DIAGNOSTICS_H