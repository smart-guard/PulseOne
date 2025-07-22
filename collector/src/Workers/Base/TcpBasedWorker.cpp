/**
 * @file TcpBasedWorker.cpp
 * @brief TCP 기반 프로토콜 워커의 구현
 * @author PulseOne Development Team
 * @date 2025-01-20
 * @version 1.0.0
 */

#include "Workers/Base/TcpBasedWorker.h"
#include "Utils/LogManager.h"
#include <chrono>
#include <sstream>
#include <iomanip>

using namespace std::chrono;

namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

TcpBasedWorker::TcpBasedWorker(const Drivers::DeviceInfo& device_info,
                               std::shared_ptr<RedisClient> redis_client,
                               std::shared_ptr<InfluxClient> influx_client,
                               std::shared_ptr<LogManager> logger)
    : BaseDeviceWorker(device_info, redis_client, influx_client, logger)
    , ip_address_("127.0.0.1")
    , port_(502)
    , connection_timeout_seconds_(5)
    , retry_interval_ms_(5000)
    , max_retries_(0)
    , keep_alive_enabled_(true)
    , keep_alive_interval_seconds_(30) {
    
    // 통계 시작 시간 설정
    connection_stats_.first_connection_time = system_clock::now();
    last_keep_alive_ = system_clock::now();
    
    if (logger_) {
        logger_->Info("TcpBasedWorker created for device: " + device_info.device_name);
    }
}

TcpBasedWorker::~TcpBasedWorker() {
    // 모니터링 스레드 정리
    monitor_thread_running_ = false;
    if (connection_monitor_thread_ && connection_monitor_thread_->joinable()) {
        connection_monitor_thread_->join();
    }
    
    if (logger_) {
        logger_->Info("TcpBasedWorker destroyed");
    }
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현
// =============================================================================

std::future<bool> TcpBasedWorker::Start() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    std::thread([this, promise]() {
        try {
            if (GetState() == WorkerState::RUNNING) {
                logger_->Warn("TcpBasedWorker already running");
                promise->set_value(true);
                return;
            }
            
            // TCP 연결 시도
            if (!AttemptTcpConnection()) {
                logger_->Error("Failed to establish TCP connection");
                promise->set_value(false);
                return;
            }
            
            // 프로토콜별 연결 수립
            if (!EstablishProtocolConnection()) {
                logger_->Error("Failed to establish protocol connection");
                promise->set_value(false);
                return;
            }
            
            // 연결 모니터링 스레드 시작
            monitor_thread_running_ = true;
            connection_monitor_thread_ = std::make_unique<std::thread>(
                &TcpBasedWorker::ConnectionMonitorThreadMain, this);
            
            SetConnectionState(true);
            ChangeState(WorkerState::RUNNING);
            
            logger_->Info("TcpBasedWorker started successfully");
            promise->set_value(true);
            
        } catch (const std::exception& e) {
            logger_->Error("Exception in TcpBasedWorker::Start: " + std::string(e.what()));
            promise->set_value(false);
        }
    }).detach();
    
    return future;
}

std::future<bool> TcpBasedWorker::Stop() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    std::thread([this, promise]() {
        try {
            if (GetState() == WorkerState::STOPPED) {
                promise->set_value(true);
                return;
            }
            
            ChangeState(WorkerState::STOPPING);
            
            // 모니터링 스레드 정지
            monitor_thread_running_ = false;
            if (connection_monitor_thread_ && connection_monitor_thread_->joinable()) {
                connection_monitor_thread_->join();
            }
            
            // 프로토콜별 연결 해제
            CloseProtocolConnection();
            
            SetConnectionState(false);
            ChangeState(WorkerState::STOPPED);
            
            logger_->Info("TcpBasedWorker stopped successfully");
            promise->set_value(true);
            
        } catch (const std::exception& e) {
            logger_->Error("Exception in TcpBasedWorker::Stop: " + std::string(e.what()));
            promise->set_value(false);
        }
    }).detach();
    
    return future;
}

WorkerState TcpBasedWorker::GetState() const {
    return current_state_.load();
}

// =============================================================================
// TCP 설정 메서드들
// =============================================================================

void TcpBasedWorker::ConfigureTcpConnection(const std::string& ip, 
                                           uint16_t port,
                                           int timeout_seconds) {
    ip_address_ = ip;
    port_ = port;
    connection_timeout_seconds_ = timeout_seconds;
    
    if (logger_) {
        logger_->Info("TCP connection configured - " + ip + ":" + std::to_string(port));
    }
}

void TcpBasedWorker::ConfigureAutoReconnect(bool enabled, 
                                           int retry_interval_ms,
                                           int max_retries) {
    auto_reconnect_enabled_ = enabled;
    retry_interval_ms_ = retry_interval_ms;
    max_retries_ = max_retries;
    current_retry_count_ = 0;
    
    if (logger_) {
        std::string msg = "Auto-reconnect configured: " + 
                         std::string(enabled ? "enabled" : "disabled");
        if (enabled) {
            msg += " (interval: " + std::to_string(retry_interval_ms) + "ms, " +
                   "max retries: " + (max_retries == 0 ? "unlimited" : std::to_string(max_retries)) + ")";
        }
        logger_->Info(msg);
    }
}

void TcpBasedWorker::ConfigureKeepAlive(bool enabled, int interval_seconds) {
    keep_alive_enabled_ = enabled;
    keep_alive_interval_seconds_ = interval_seconds;
    
    if (logger_) {
        std::string msg = "Keep-alive configured: " + 
                         std::string(enabled ? "enabled" : "disabled");
        if (enabled) {
            msg += " (interval: " + std::to_string(interval_seconds) + "s)";
        }
        logger_->Info(msg);
    }
}

// =============================================================================
// 런타임 설정 관리 메서드들
// =============================================================================

std::string TcpBasedWorker::GetReconnectSettings() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"enabled\": " << (auto_reconnect_enabled_.load() ? "true" : "false") << ",\n";
    ss << "  \"interval_ms\": " << retry_interval_ms_ << ",\n";
    ss << "  \"max_retries\": " << max_retries_ << ",\n";
    ss << "  \"current_retry_count\": " << current_retry_count_.load() << ",\n";
    ss << "  \"description\": \"Auto-reconnect configuration\"\n";
    ss << "}";
    return ss.str();
}

bool TcpBasedWorker::UpdateReconnectSettings(const std::string& settings_json) {
    try {
        // 간단한 JSON 파싱 (실제로는 JSON 라이브러리 사용 권장)
        // 여기서는 기본적인 문자열 파싱으로 구현
        
        bool enabled = auto_reconnect_enabled_.load();
        int interval_ms = retry_interval_ms_;
        int max_retries = max_retries_;
        
        // "enabled": true/false 파싱
        size_t enabled_pos = settings_json.find("\"enabled\":");
        if (enabled_pos != std::string::npos) {
            size_t value_start = settings_json.find(":", enabled_pos) + 1;
            size_t value_end = settings_json.find_first_of(",}", value_start);
            std::string value_str = settings_json.substr(value_start, value_end - value_start);
            
            // 공백 제거
            value_str.erase(0, value_str.find_first_not_of(" \t"));
            value_str.erase(value_str.find_last_not_of(" \t") + 1);
            
            enabled = (value_str == "true");
        }
        
        // "interval_ms": 숫자 파싱
        size_t interval_pos = settings_json.find("\"interval_ms\":");
        if (interval_pos != std::string::npos) {
            size_t value_start = settings_json.find(":", interval_pos) + 1;
            size_t value_end = settings_json.find_first_of(",}", value_start);
            std::string value_str = settings_json.substr(value_start, value_end - value_start);
            
            value_str.erase(0, value_str.find_first_not_of(" \t"));
            value_str.erase(value_str.find_last_not_of(" \t") + 1);
            
            interval_ms = std::stoi(value_str);
        }
        
        // "max_retries": 숫자 파싱
        size_t retries_pos = settings_json.find("\"max_retries\":");
        if (retries_pos != std::string::npos) {
            size_t value_start = settings_json.find(":", retries_pos) + 1;
            size_t value_end = settings_json.find_first_of(",}", value_start);
            std::string value_str = settings_json.substr(value_start, value_end - value_start);
            
            value_str.erase(0, value_str.find_first_not_of(" \t"));
            value_str.erase(value_str.find_last_not_of(" \t") + 1);
            
            max_retries = std::stoi(value_str);
        }
        
        // 유효성 검사
        if (interval_ms < 1000) {
            if (logger_) {
                logger_->Warn("Reconnect interval too short, minimum 1000ms");
            }
            interval_ms = 1000;
        }
        
        if (interval_ms > 300000) { // 5분 최대
            if (logger_) {
                logger_->Warn("Reconnect interval too long, maximum 300000ms");
            }
            interval_ms = 300000;
        }
        
        if (max_retries < 0) {
            max_retries = 0; // 0 = 무제한
        }
        
        // 설정 업데이트
        ConfigureAutoReconnect(enabled, interval_ms, max_retries);
        
        if (logger_) {
            logger_->Info("Reconnect settings updated: enabled=" + std::string(enabled ? "true" : "false") +
                         ", interval=" + std::to_string(interval_ms) + "ms" +
                         ", max_retries=" + std::to_string(max_retries));
        }
        
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Failed to update reconnect settings: " + std::string(e.what()));
        }
        return false;
    }
}

bool TcpBasedWorker::UpdateKeepAliveSettings(const std::string& settings_json) {
    try {
        bool enabled = keep_alive_enabled_;
        int interval_seconds = keep_alive_interval_seconds_;
        
        // "enabled": true/false 파싱
        size_t enabled_pos = settings_json.find("\"enabled\":");
        if (enabled_pos != std::string::npos) {
            size_t value_start = settings_json.find(":", enabled_pos) + 1;
            size_t value_end = settings_json.find_first_of(",}", value_start);
            std::string value_str = settings_json.substr(value_start, value_end - value_start);
            
            value_str.erase(0, value_str.find_first_not_of(" \t"));
            value_str.erase(value_str.find_last_not_of(" \t") + 1);
            
            enabled = (value_str == "true");
        }
        
        // "interval_seconds": 숫자 파싱
        size_t interval_pos = settings_json.find("\"interval_seconds\":");
        if (interval_pos != std::string::npos) {
            size_t value_start = settings_json.find(":", interval_pos) + 1;
            size_t value_end = settings_json.find_first_of(",}", value_start);
            std::string value_str = settings_json.substr(value_start, value_end - value_start);
            
            value_str.erase(0, value_str.find_first_not_of(" \t"));
            value_str.erase(value_str.find_last_not_of(" \t") + 1);
            
            interval_seconds = std::stoi(value_str);
        }
        
        // 유효성 검사
        if (interval_seconds < 10) {
            if (logger_) {
                logger_->Warn("Keep-alive interval too short, minimum 10 seconds");
            }
            interval_seconds = 10;
        }
        
        if (interval_seconds > 3600) { // 1시간 최대
            if (logger_) {
                logger_->Warn("Keep-alive interval too long, maximum 3600 seconds");
            }
            interval_seconds = 3600;
        }
        
        // 설정 업데이트
        ConfigureKeepAlive(enabled, interval_seconds);
        
        if (logger_) {
            logger_->Info("Keep-alive settings updated: enabled=" + std::string(enabled ? "true" : "false") +
                         ", interval=" + std::to_string(interval_seconds) + "s");
        }
        
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Failed to update keep-alive settings: " + std::string(e.what()));
        }
        return false;
    }
}

std::future<bool> TcpBasedWorker::ForceReconnect() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    std::thread([this, promise]() {
        try {
            if (logger_) {
                logger_->Info("Force reconnect requested by engineer");
            }
            
            // 현재 연결 강제 종료
            if (is_connected_.load()) {
                CloseProtocolConnection();
                SetConnectionState(false);
                
                // 잠시 대기 (연결 정리 시간)
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            
            // 재시도 카운터 리셋
            ResetRetryCounter();
            
            // 즉시 재연결 시도
            bool success = false;
            if (AttemptTcpConnection()) {
                if (EstablishProtocolConnection()) {
                    SetConnectionState(true);
                    success = true;
                    if (logger_) {
                        logger_->Info("Force reconnect successful");
                    }
                }
            }
            
            if (!success && logger_) {
                logger_->Error("Force reconnect failed");
            }
            
            promise->set_value(success);
            
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->Error("Exception in force reconnect: " + std::string(e.what()));
            }
            promise->set_value(false);
        }
    }).detach();
    
    return future;
}

void TcpBasedWorker::ResetRetryCounter() {
    int old_count = current_retry_count_.exchange(0);
    if (logger_ && old_count > 0) {
        logger_->Info("Retry counter reset from " + std::to_string(old_count) + " to 0");
    }
}

// =============================================================================
// 상태 조회 메서드들
// =============================================================================

std::chrono::system_clock::time_point TcpBasedWorker::GetLastConnectedTime() const {
    return last_connected_time_;
}

std::string TcpBasedWorker::GetConnectionStats() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"tcp_connection\": {\n";
    ss << "    \"endpoint\": \"" << ip_address_ << ":" << port_ << "\",\n";
    ss << "    \"connected\": " << (is_connected_.load() ? "true" : "false") << ",\n";
    ss << "    \"auto_reconnect\": " << (auto_reconnect_enabled_.load() ? "true" : "false") << ",\n";
    ss << "    \"keep_alive\": " << (keep_alive_enabled_ ? "true" : "false") << "\n";
    ss << "  },\n";
    ss << "  \"statistics\": {\n";
    ss << "    \"total_connections\": " << connection_stats_.total_connections.load() << ",\n";
    ss << "    \"successful_connections\": " << connection_stats_.successful_connections.load() << ",\n";
    ss << "    \"failed_connections\": " << connection_stats_.failed_connections.load() << ",\n";
    ss << "    \"reconnections\": " << connection_stats_.reconnections.load() << ",\n";
    ss << "    \"connection_drops\": " << connection_stats_.connection_drops.load() << ",\n";
    ss << "    \"keep_alive_sent\": " << connection_stats_.keep_alive_sent.load() << ",\n";
    ss << "    \"avg_connection_duration_seconds\": " << connection_stats_.avg_connection_duration_seconds.load() << ",\n";
    ss << "    \"current_retry_count\": " << current_retry_count_.load() << "\n";
    ss << "  }\n";
    ss << "}";
    
    return ss.str();
}

// =============================================================================
// 보호된 메서드들
// =============================================================================

void TcpBasedWorker::HandleNetworkError(const std::string& error_message, bool should_reconnect) {
    last_error_time_ = system_clock::now();
    
    if (logger_) {
        logger_->Error("Network error: " + error_message);
    }
    
    SetConnectionState(false);
    connection_stats_.connection_drops.fetch_add(1);
    
    if (should_reconnect && auto_reconnect_enabled_.load()) {
        if (logger_) {
            logger_->Info("Scheduling reconnection attempt...");
        }
        // 재연결은 모니터링 스레드에서 처리됨
    }
}

void TcpBasedWorker::SetConnectionState(bool connected) {
    bool old_state = is_connected_.exchange(connected);
    
    if (old_state != connected) {
        if (connected) {
            last_connected_time_ = system_clock::now();
            current_retry_count_ = 0;
            if (logger_) {
                logger_->Info("Connection established to " + ip_address_ + ":" + std::to_string(port_));
            }
        } else {
            if (logger_) {
                logger_->Warn("Connection lost to " + ip_address_ + ":" + std::to_string(port_));
            }
        }
        
        UpdateConnectionStats(connected);
    }
}

int TcpBasedWorker::CheckConnectionQuality() {
    if (!is_connected_.load()) {
        return 0; // 연결되지 않음
    }
    
    // 간단한 품질 계산 로직
    auto now = system_clock::now();
    auto connection_duration = duration_cast<seconds>(now - last_connected_time_).count();
    
    // 기본 점수 (연결 상태)
    int score = 50;
    
    // 연결 지속 시간에 따른 보너스 (최대 30점)
    if (connection_duration > 300) score += 30;        // 5분 이상
    else if (connection_duration > 60) score += 20;    // 1분 이상
    else if (connection_duration > 10) score += 10;    // 10초 이상
    
    // 오류 발생 시간에 따른 감점
    auto time_since_error = duration_cast<seconds>(now - last_error_time_).count();
    if (time_since_error < 60) score -= 20;            // 1분 이내 오류
    else if (time_since_error < 300) score -= 10;      // 5분 이내 오류
    
    // Keep-alive 상태에 따른 보너스
    if (keep_alive_enabled_) {
        auto time_since_keepalive = duration_cast<seconds>(now - last_keep_alive_).count();
        if (time_since_keepalive < keep_alive_interval_seconds_ * 2) {
            score += 20;
        }
    }
    
    return std::max(0, std::min(100, score));
}

// =============================================================================
// 내부 메서드들
// =============================================================================

void TcpBasedWorker::ConnectionMonitorThreadMain() {
    if (logger_) {
        logger_->Info("Connection monitor thread started");
    }
    
    while (monitor_thread_running_.load()) {
        try {
            // 연결 상태 확인
            if (is_connected_.load()) {
                bool protocol_connected = CheckProtocolConnection();
                if (!protocol_connected) {
                    HandleNetworkError("Protocol connection check failed", true);
                }
                
                // Keep-alive 전송
                if (keep_alive_enabled_) {
                    auto now = system_clock::now();
                    auto time_since_keepalive = duration_cast<seconds>(now - last_keep_alive_).count();
                    
                    if (time_since_keepalive >= keep_alive_interval_seconds_) {
                        SendKeepAlive();
                    }
                }
            } else {
                // 재연결 시도
                if (auto_reconnect_enabled_.load()) {
                    AttemptReconnection();
                }
            }
            
            // 모니터링 간격 (1초)
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->Error("Exception in connection monitor thread: " + std::string(e.what()));
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    if (logger_) {
        logger_->Info("Connection monitor thread stopped");
    }
}

bool TcpBasedWorker::AttemptTcpConnection() {
    connection_stats_.total_connections.fetch_add(1);
    
    try {
        // 실제 TCP 연결 로직은 파생 클래스에서 구현
        // 여기서는 기본적인 유효성 검사만
        if (ip_address_.empty() || port_ == 0) {
            throw std::runtime_error("Invalid TCP configuration");
        }
        
        if (logger_) {
            logger_->Info("Attempting TCP connection to " + ip_address_ + ":" + std::to_string(port_));
        }
        
        // 기본적으로 성공으로 간주 (파생 클래스에서 실제 구현)
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("TCP connection failed: " + std::string(e.what()));
        }
        return false;
    }
}

void TcpBasedWorker::AttemptReconnection() {
    // 최대 재시도 횟수 확인
    if (max_retries_ > 0 && current_retry_count_.load() >= max_retries_) {
        if (logger_) {
            logger_->Warn("Maximum retry count reached (" + std::to_string(max_retries_) + ")");
        }
        return;
    }
    
    // 재시도 간격 확인
    static auto last_retry_time = system_clock::now();
    auto now = system_clock::now();
    auto time_since_retry = duration_cast<milliseconds>(now - last_retry_time).count();
    
    if (time_since_retry < retry_interval_ms_) {
        return; // 아직 재시도 시간이 안됨
    }
    
    last_retry_time = now;
    current_retry_count_.fetch_add(1);
    connection_stats_.reconnections.fetch_add(1);
    
    if (logger_) {
        logger_->Info("Attempting reconnection (attempt " + 
                     std::to_string(current_retry_count_.load()) + ")");
    }
    
    // TCP 연결 시도
    if (AttemptTcpConnection()) {
        // 프로토콜별 연결 수립 시도
        if (EstablishProtocolConnection()) {
            SetConnectionState(true);
            if (logger_) {
                logger_->Info("Reconnection successful");
            }
        } else {
            if (logger_) {
                logger_->Error("Protocol connection failed during reconnection");
            }
        }
    }
}

void TcpBasedWorker::SendKeepAlive() {
    try {
        // 실제 Keep-alive 구현은 파생 클래스에서
        // 여기서는 기본적인 타이밍 업데이트만
        last_keep_alive_ = system_clock::now();
        connection_stats_.keep_alive_sent.fetch_add(1);
        
        if (logger_) {
            logger_->Debug("Keep-alive sent");
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Keep-alive failed: " + std::string(e.what()));
        }
    }
}

void TcpBasedWorker::UpdateConnectionStats(bool connection_successful) {
    if (connection_successful) {
        connection_stats_.successful_connections.fetch_add(1);
    } else {
        connection_stats_.failed_connections.fetch_add(1);
    }
    
    // 평균 연결 지속 시간 계산 (간단한 구현)
    static auto last_connection_start = system_clock::now();
    if (connection_successful) {
        last_connection_start = system_clock::now();
    } else {
        auto duration = duration_cast<seconds>(system_clock::now() - last_connection_start).count();
        double current_avg = connection_stats_.avg_connection_duration_seconds.load();
        double new_avg = (current_avg + duration) / 2.0; // 간단한 이동 평균
        connection_stats_.avg_connection_duration_seconds.store(new_avg);
    }
}

} // namespace Workers
} // namespace PulseOne