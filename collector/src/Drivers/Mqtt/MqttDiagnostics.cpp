// =============================================================================
// collector/src/Drivers/Mqtt/MqttDiagnostics.cpp
// MQTT 진단 및 모니터링 기능 구현
// =============================================================================

#include "Drivers/Mqtt/MqttDiagnostics.h"
#include "Drivers/Mqtt/MqttDriver.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

namespace PulseOne {
namespace Drivers {

using namespace std::chrono;

// =============================================================================
// 생성자/소멸자
// =============================================================================

MqttDiagnostics::MqttDiagnostics(MqttDriver* parent_driver)
    : parent_driver_(parent_driver)
    , connection_start_time_(system_clock::now())
    , last_disconnect_time_(system_clock::now())
{
    // QoS 0, 1, 2에 대한 기본 분석 데이터 초기화
    for (int qos = 0; qos <= 2; ++qos) {
        qos_stats_[qos] = QosAnalysis{};
    }
    
    connection_quality_.last_quality_check = system_clock::now();
}

MqttDiagnostics::~MqttDiagnostics() {
    // 정리 작업 (필요시)
}

// =============================================================================
// 진단 설정 및 제어
// =============================================================================

void MqttDiagnostics::EnableMessageTracking(bool enable) {
    message_tracking_enabled_ = enable;
}

void MqttDiagnostics::EnableQosAnalysis(bool enable) {
    qos_analysis_enabled_ = enable;
}

void MqttDiagnostics::EnablePacketLogging(bool enable) {
    packet_logging_enabled_ = enable;
}

void MqttDiagnostics::SetMaxHistorySize(size_t max_size) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    max_history_size_ = max_size;
    CleanOldHistory();
}

void MqttDiagnostics::SetSamplingInterval(int interval_ms) {
    sampling_interval_ms_ = interval_ms;
}

// =============================================================================
// 진단 정보 수집
// =============================================================================

void MqttDiagnostics::RecordConnectionEvent(bool connected, const std::string& broker_url, double duration_ms) {
    if (!enabled_.load()) return;
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (connected) {
        connection_start_time_ = system_clock::now();
        connection_quality_.reconnection_count++;
        
        if (duration_ms > 0) {
            // 재연결 시간 평균 업데이트
            double current_avg = connection_quality_.avg_reconnection_time_ms;
            connection_quality_.avg_reconnection_time_ms = 
                (current_avg + duration_ms) / 2.0;
        }
    } else {
        last_disconnect_time_ = system_clock::now();
        
        // 업타임 계산
        if (connection_start_time_ < last_disconnect_time_) {
            auto uptime = duration_cast<milliseconds>(last_disconnect_time_ - connection_start_time_);
            auto total_time = duration_cast<milliseconds>(system_clock::now() - connection_start_time_);
            
            if (total_time.count() > 0) {
                connection_quality_.uptime_percentage = 
                    (double)uptime.count() / total_time.count() * 100.0;
            }
        }
    }
    
    UpdateConnectionQuality();
}

void MqttDiagnostics::RecordPublish(const std::string& topic, size_t payload_size, int qos, bool success, double latency_ms) {
    if (!enabled_.load()) return;
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    // 전체 메시지 카운터 업데이트
    total_messages_.fetch_add(1);
    total_bytes_sent_.fetch_add(payload_size);
    
    // QoS 분석 업데이트
    if (qos_analysis_enabled_ && qos_stats_.find(qos) != qos_stats_.end()) {
        auto& qos_stat = qos_stats_[qos];
        qos_stat.total_messages++;
        
        if (success) {
            qos_stat.successful_deliveries++;
            qos_stat.UpdateLatency(latency_ms);
        } else {
            qos_stat.failed_deliveries++;
        }
        
        qos_stat.CalculateSuccessRate();
    }
    
    // 토픽별 통계 업데이트
    if (message_tracking_enabled_) {
        auto& topic_stat = topic_stats_[topic];
        topic_stat.topic_name = topic;
        topic_stat.publish_count++;
        topic_stat.bytes_sent += payload_size;
        topic_stat.last_activity = system_clock::now();
    }
    
    // 레이턴시 히스토리 업데이트
    if (success && latency_ms > 0) {
        UpdateLatencyHistogram(latency_ms);
    }
    
    // 실패한 경우 에러 기록
    if (!success) {
        RecordError("Message publish failed for topic: " + topic + " (QoS " + std::to_string(qos) + ")");
    }
}

void MqttDiagnostics::RecordReceive(const std::string& topic, size_t payload_size, int qos) {
    if (!enabled_.load()) return;
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    // 전체 바이트 카운터 업데이트
    total_bytes_received_.fetch_add(payload_size);
    
    // QoS 분석 업데이트
    if (qos_analysis_enabled_ && qos_stats_.find(qos) != qos_stats_.end()) {
        auto& qos_stat = qos_stats_[qos];
        qos_stat.total_messages++;
        qos_stat.successful_deliveries++; // 수신은 항상 성공으로 간주
        qos_stat.CalculateSuccessRate();
    }
    
    // 토픽별 통계 업데이트
    if (message_tracking_enabled_) {
        auto& topic_stat = topic_stats_[topic];
        topic_stat.topic_name = topic;
        topic_stat.subscribe_count++;
        topic_stat.bytes_received += payload_size;
        topic_stat.last_activity = system_clock::now();
    }
}

void MqttDiagnostics::RecordSubscription(const std::string& topic, int qos, bool success) {
    if (!enabled_.load()) return;
    
    if (!success) {
        RecordError("Subscription failed for topic: " + topic + " (QoS " + std::to_string(qos) + ")");
    }
}

void MqttDiagnostics::RecordMessageLoss(const std::string& topic, int qos, const std::string& reason) {
    if (!enabled_.load()) return;
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    lost_messages_.fetch_add(1);
    connection_quality_.message_loss_count++;
    
    // 손실률 계산
    if (total_messages_.load() > 0) {
        connection_quality_.message_loss_rate = 
            (double)lost_messages_.load() / total_messages_.load() * 100.0;
    }
    
    RecordError("Message lost - Topic: " + topic + ", QoS: " + std::to_string(qos) + ", Reason: " + reason);
}

void MqttDiagnostics::RecordOperation(const std::string& operation, bool success, double duration_ms) {
    if (!enabled_.load()) return;
    
    if (!success) {
        RecordError("Operation failed: " + operation + " (Duration: " + std::to_string(duration_ms) + "ms)");
    }
    
    // 레이턴시 히스토리에 추가 (성공한 경우만)
    if (success && duration_ms > 0) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        UpdateLatencyHistogram(duration_ms);
    }
}

// =============================================================================
// 진단 정보 조회
// =============================================================================

std::string MqttDiagnostics::GetDiagnosticsJSON() const {
#ifdef HAS_NLOHMANN_JSON
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    try {
        json diagnostics;
        
        // 기본 정보
        diagnostics["enabled"] = enabled_.load();
        diagnostics["message_tracking"] = message_tracking_enabled_.load();
        diagnostics["qos_analysis"] = qos_analysis_enabled_.load();
        diagnostics["packet_logging"] = packet_logging_enabled_.load();
        
        // 전체 통계
        json stats;
        stats["total_messages"] = total_messages_.load();
        stats["lost_messages"] = lost_messages_.load();
        stats["total_bytes_sent"] = total_bytes_sent_.load();
        stats["total_bytes_received"] = total_bytes_received_.load();
        stats["message_loss_rate"] = GetMessageLossRate();
        stats["average_latency_ms"] = GetAverageLatency();
        diagnostics["statistics"] = stats;
        
        // QoS 분석
        json qos_analysis;
        for (const auto& [qos, analysis] : qos_stats_) {
            json qos_data;
            qos_data["total_messages"] = analysis.total_messages;
            qos_data["successful_deliveries"] = analysis.successful_deliveries;
            qos_data["failed_deliveries"] = analysis.failed_deliveries;
            qos_data["success_rate"] = analysis.success_rate;
            qos_data["avg_latency_ms"] = analysis.avg_latency_ms;
            qos_data["max_latency_ms"] = analysis.max_latency_ms;
            qos_data["min_latency_ms"] = analysis.min_latency_ms;
            qos_analysis["qos_" + std::to_string(qos)] = qos_data;
        }
        diagnostics["qos_analysis"] = qos_analysis;
        
        // 토픽 통계 (상위 10개만)
        json topics;
        std::vector<std::pair<std::string, TopicStats>> sorted_topics;
        for (const auto& [topic, stats] : topic_stats_) {
            sorted_topics.emplace_back(topic, stats);
        }
        
        // 메시지 수 기준으로 정렬
        std::sort(sorted_topics.begin(), sorted_topics.end(),
                 [](const auto& a, const auto& b) {
                     return (a.second.publish_count + a.second.subscribe_count) >
                            (b.second.publish_count + b.second.subscribe_count);
                 });
        
        for (size_t i = 0; i < std::min<size_t>(10, sorted_topics.size()); ++i) {
            const auto& [topic, topic_stat] = sorted_topics[i];
            json topic_data;
            topic_data["publish_count"] = topic_stat.publish_count;
            topic_data["subscribe_count"] = topic_stat.subscribe_count;
            topic_data["bytes_sent"] = topic_stat.bytes_sent;
            topic_data["bytes_received"] = topic_stat.bytes_received;
            topics[topic] = topic_data;
        }
        diagnostics["top_topics"] = topics;
        
        // 연결 품질
        json quality;
        quality["uptime_percentage"] = connection_quality_.uptime_percentage;
        quality["reconnection_count"] = connection_quality_.reconnection_count;
        quality["avg_reconnection_time_ms"] = connection_quality_.avg_reconnection_time_ms;
        quality["message_loss_count"] = connection_quality_.message_loss_count;
        quality["message_loss_rate"] = connection_quality_.message_loss_rate;
        diagnostics["connection_quality"] = quality;
        
        // 최근 에러들
        auto recent_errors = GetRecentErrors(5);
        diagnostics["recent_errors"] = recent_errors;
        
        return diagnostics.dump(2);
        
    } catch (const std::exception& e) {
        return "{\"error\":\"Failed to generate diagnostics JSON: " + std::string(e.what()) + "\"}";
    }
#else
    // JSON 라이브러리가 없는 경우 간단한 문자열 반환
    std::ostringstream oss;
    oss << "{"
        << "\"enabled\":" << (enabled_.load() ? "true" : "false") << ","
        << "\"total_messages\":" << total_messages_.load() << ","
        << "\"lost_messages\":" << lost_messages_.load() << ","
        << "\"message_loss_rate\":" << GetMessageLossRate() << ","
        << "\"average_latency_ms\":" << GetAverageLatency()
        << "}";
    return oss.str();
#endif
}

double MqttDiagnostics::GetMessageLossRate() const {
    uint64_t total = total_messages_.load();
    uint64_t lost = lost_messages_.load();
    
    if (total == 0) return 0.0;
    return (double)lost / total * 100.0;
}

double MqttDiagnostics::GetAverageLatency() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (latency_history_.empty()) return 0.0;
    
    double sum = 0.0;
    for (double latency : latency_history_) {
        sum += latency;
    }
    
    return sum / latency_history_.size();
}

std::map<int, QosAnalysis> MqttDiagnostics::GetQosAnalysis() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return qos_stats_;
}

std::map<std::string, TopicStats> MqttDiagnostics::GetTopicStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    std::map<std::string, TopicStats> result;
    for (const auto& [topic, stats] : topic_stats_) {
        result[topic] = stats;
    }
    
    return result;
}

ConnectionQuality MqttDiagnostics::GetConnectionQuality() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return connection_quality_;
}

std::vector<std::pair<double, uint64_t>> MqttDiagnostics::GetLatencyHistogram() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    // 레이턴시를 구간별로 분류 (0-10ms, 10-50ms, 50-100ms, 100-500ms, 500ms+)
    std::vector<std::pair<double, uint64_t>> histogram = {
        {10.0, 0},    // 0-10ms
        {50.0, 0},    // 10-50ms
        {100.0, 0},   // 50-100ms
        {500.0, 0},   // 100-500ms
        {999999.0, 0} // 500ms+
    };
    
    for (double latency : latency_history_) {
        for (auto& [threshold, count] : histogram) {
            if (latency <= threshold) {
                count++;
                break;
            }
        }
    }
    
    return histogram;
}

std::vector<std::string> MqttDiagnostics::GetRecentErrors(size_t max_count) const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    std::vector<std::string> recent_errors;
    
    size_t count = 0;
    for (auto it = error_history_.rbegin(); it != error_history_.rend() && count < max_count; ++it, ++count) {
        const auto& [timestamp, error] = *it;
        
        // 타임스탬프를 문자열로 변환
        auto time_t = system_clock::to_time_t(timestamp);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        
        recent_errors.push_back("[" + oss.str() + "] " + error);
    }
    
    return recent_errors;
}

// =============================================================================
// 유틸리티
// =============================================================================

void MqttDiagnostics::Reset() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    // 모든 통계 초기화
    total_messages_ = 0;
    lost_messages_ = 0;
    total_bytes_sent_ = 0;
    total_bytes_received_ = 0;
    
    // QoS 분석 초기화
    for (auto& [qos, analysis] : qos_stats_) {
        analysis = QosAnalysis{};
    }
    
    // 토픽 통계 초기화
    topic_stats_.clear();
    
    // 히스토리 초기화
    latency_history_.clear();
    error_history_.clear();
    
    // 연결 품질 초기화
    connection_quality_ = ConnectionQuality{};
    connection_start_time_ = system_clock::now();
}

bool MqttDiagnostics::IsEnabled() const {
    return enabled_.load();
}

// =============================================================================
// 내부 유틸리티 메서드들
// =============================================================================

void MqttDiagnostics::CleanOldHistory() {
    // 레이턴시 히스토리 정리
    while (latency_history_.size() > max_history_size_) {
        latency_history_.pop_front();
    }
    
    // 에러 히스토리 정리
    while (error_history_.size() > max_history_size_) {
        error_history_.pop_front();
    }
}

void MqttDiagnostics::UpdateLatencyHistogram(double latency_ms) {
    latency_history_.push_back(latency_ms);
    CleanOldHistory();
}

void MqttDiagnostics::UpdateConnectionQuality() {
    connection_quality_.last_quality_check = system_clock::now();
    
    // 메시지 손실률 업데이트
    if (total_messages_.load() > 0) {
        connection_quality_.message_loss_rate = GetMessageLossRate();
    }
}

void MqttDiagnostics::RecordError(const std::string& error_message) {
    error_history_.emplace_back(system_clock::now(), error_message);
    CleanOldHistory();
}

} // namespace Drivers
} // namespace PulseOne