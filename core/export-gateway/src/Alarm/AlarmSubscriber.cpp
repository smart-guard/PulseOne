/**
 * @file AlarmSubscriber.cpp
 * @brief Redis Pub/Sub 알람 구독 및 전송 구현
 * @author PulseOne Development Team
 * @date 2025-10-22
 * @version 1.0.0
 */

#include "Alarm/AlarmSubscriber.h"
#include "Client/RedisClientImpl.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <iomanip>

namespace PulseOne {
namespace Alarm {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

AlarmSubscriber::AlarmSubscriber(const AlarmSubscriberConfig& config)
    : config_(config) {
    
    LogManager::getInstance().Info("AlarmSubscriber 초기화 시작");
    LogManager::getInstance().Info("구독 채널: " + std::to_string(config_.subscribe_channels.size()) + "개");
    LogManager::getInstance().Info("워커 스레드: " + std::to_string(config_.worker_thread_count) + "개");
    LogManager::getInstance().Info("최대 큐 크기: " + std::to_string(config_.max_queue_size));
}

AlarmSubscriber::~AlarmSubscriber() {
    stop();
    LogManager::getInstance().Info("AlarmSubscriber 종료 완료");
}

// =============================================================================
// 라이프사이클 관리
// =============================================================================

bool AlarmSubscriber::start() {
    if (is_running_.load()) {
        LogManager::getInstance().Warn("AlarmSubscriber가 이미 실행 중입니다");
        return false;
    }
    
    LogManager::getInstance().Info("AlarmSubscriber 시작 중...");
    
    // 1. Redis 연결 초기화
    if (!initializeRedisConnection()) {
        LogManager::getInstance().Error("Redis 연결 초기화 실패");
        return false;
    }
    
    // 2. CSPGateway 초기화
    if (!initializeCSPGateway()) {
        LogManager::getInstance().Error("CSPGateway 초기화 실패");
        return false;
    }
    
    // 3. 구독 채널 복사
    {
        std::lock_guard<std::mutex> lock(channel_mutex_);
        subscribed_channels_ = config_.subscribe_channels;
        subscribed_patterns_ = config_.subscribe_patterns;
    }
    
    // 4. 스레드 시작
    should_stop_ = false;
    is_running_ = true;
    
    // 4.1 구독 스레드
    subscribe_thread_ = std::make_unique<std::thread>(
        &AlarmSubscriber::subscribeLoop, this);
    
    // 4.2 워커 스레드들
    for (int i = 0; i < config_.worker_thread_count; ++i) {
        worker_threads_.emplace_back(
            std::make_unique<std::thread>(
                &AlarmSubscriber::workerLoop, this, i));
    }
    
    // 4.3 재연결 스레드 (옵션)
    if (config_.auto_reconnect) {
        reconnect_thread_ = std::make_unique<std::thread>(
            &AlarmSubscriber::reconnectLoop, this);
    }
    
    LogManager::getInstance().Info("AlarmSubscriber 시작 완료");
    return true;
}

void AlarmSubscriber::stop() {
    if (!is_running_.load()) {
        return;
    }
    
    LogManager::getInstance().Info("AlarmSubscriber 중지 중...");
    
    should_stop_ = true;
    is_connected_ = false;
    
    // 모든 대기 중인 스레드 깨우기
    queue_cv_.notify_all();
    
    // 구독 스레드 종료
    if (subscribe_thread_ && subscribe_thread_->joinable()) {
        subscribe_thread_->join();
    }
    
    // 워커 스레드들 종료
    for (auto& thread : worker_threads_) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }
    worker_threads_.clear();
    
    // 재연결 스레드 종료
    if (reconnect_thread_ && reconnect_thread_->joinable()) {
        reconnect_thread_->join();
    }
    
    is_running_ = false;
    LogManager::getInstance().Info("AlarmSubscriber 중지 완료");
}

// =============================================================================
// 채널 관리
// =============================================================================

bool AlarmSubscriber::subscribeChannel(const std::string& channel) {
    std::lock_guard<std::mutex> lock(channel_mutex_);
    
    auto it = std::find(subscribed_channels_.begin(), 
                       subscribed_channels_.end(), 
                       channel);
    if (it == subscribed_channels_.end()) {
        subscribed_channels_.push_back(channel);
        
        // 실시간 구독 (실행 중일 때)
        if (is_running_.load() && redis_client_ && redis_client_->isConnected()) {
            redis_client_->subscribe(channel);
        }
        
        LogManager::getInstance().Info("채널 구독 추가: " + channel);
        return true;
    }
    
    return false;
}

bool AlarmSubscriber::unsubscribeChannel(const std::string& channel) {
    std::lock_guard<std::mutex> lock(channel_mutex_);
    
    auto it = std::remove(subscribed_channels_.begin(), 
                         subscribed_channels_.end(), 
                         channel);
    if (it != subscribed_channels_.end()) {
        subscribed_channels_.erase(it, subscribed_channels_.end());
        
        // 실시간 구독 해제
        if (is_running_.load() && redis_client_ && redis_client_->isConnected()) {
            redis_client_->unsubscribe(channel);
        }
        
        LogManager::getInstance().Info("채널 구독 해제: " + channel);
        return true;
    }
    
    return false;
}

bool AlarmSubscriber::subscribePattern(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(channel_mutex_);
    
    auto it = std::find(subscribed_patterns_.begin(), 
                       subscribed_patterns_.end(), 
                       pattern);
    if (it == subscribed_patterns_.end()) {
        subscribed_patterns_.push_back(pattern);
        
        // 실시간 구독
        if (is_running_.load() && redis_client_ && redis_client_->isConnected()) {
            redis_client_->psubscribe(pattern);
        }
        
        LogManager::getInstance().Info("패턴 구독 추가: " + pattern);
        return true;
    }
    
    return false;
}

bool AlarmSubscriber::unsubscribePattern(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(channel_mutex_);
    
    auto it = std::remove(subscribed_patterns_.begin(), 
                         subscribed_patterns_.end(), 
                         pattern);
    if (it != subscribed_patterns_.end()) {
        subscribed_patterns_.erase(it, subscribed_patterns_.end());
        
        // 실시간 구독 해제
        if (is_running_.load() && redis_client_ && redis_client_->isConnected()) {
            redis_client_->punsubscribe(pattern);
        }
        
        LogManager::getInstance().Info("패턴 구독 해제: " + pattern);
        return true;
    }
    
    return false;
}

std::vector<std::string> AlarmSubscriber::getSubscribedChannels() const {
    std::lock_guard<std::mutex> lock(channel_mutex_);
    return subscribed_channels_;
}

std::vector<std::string> AlarmSubscriber::getSubscribedPatterns() const {
    std::lock_guard<std::mutex> lock(channel_mutex_);
    return subscribed_patterns_;
}

// =============================================================================
// 콜백 설정
// =============================================================================

void AlarmSubscriber::setPreProcessCallback(AlarmCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    pre_process_callback_ = callback;
    LogManager::getInstance().Info("Pre-process 콜백 설정됨");
}

void AlarmSubscriber::setPostProcessCallback(AlarmCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    post_process_callback_ = callback;
    LogManager::getInstance().Info("Post-process 콜백 설정됨");
}

// =============================================================================
// 통계
// =============================================================================

SubscriptionStats AlarmSubscriber::getStatistics() const {
    SubscriptionStats stats;
    
    stats.total_received = total_received_.load();
    stats.total_processed = total_processed_.load();
    stats.total_failed = total_failed_.load();
    stats.last_received_timestamp = last_received_timestamp_.load();
    stats.last_processed_timestamp = last_processed_timestamp_.load();
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stats.queue_size = message_queue_.size();
    }
    
    stats.max_queue_size_reached = max_queue_size_reached_.load();
    
    // 평균 처리 시간 계산
    size_t processed = total_processed_.load();
    if (processed > 0) {
        stats.avg_processing_time_ms = 
            static_cast<double>(total_processing_time_ms_.load()) / processed;
    } else {
        stats.avg_processing_time_ms = 0.0;
    }
    
    return stats;
}

void AlarmSubscriber::resetStatistics() {
    total_received_ = 0;
    total_processed_ = 0;
    total_failed_ = 0;
    total_filtered_ = 0;
    max_queue_size_reached_ = 0;
    last_received_timestamp_ = 0;
    last_processed_timestamp_ = 0;
    total_processing_time_ms_ = 0;
    
    LogManager::getInstance().Info("통계 초기화 완료");
}

json AlarmSubscriber::getDetailedStatistics() const {
    auto stats = getStatistics();
    json j = stats.to_json();
    
    j["is_running"] = is_running_.load();
    j["is_connected"] = is_connected_.load();
    j["worker_thread_count"] = config_.worker_thread_count;
    j["max_queue_size"] = config_.max_queue_size;
    j["total_filtered"] = total_filtered_.load();
    
    // 성공률
    size_t received = total_received_.load();
    if (received > 0) {
        j["success_rate"] = 
            static_cast<double>(total_processed_.load()) / received * 100.0;
    } else {
        j["success_rate"] = 0.0;
    }
    
    // 채널 정보
    {
        std::lock_guard<std::mutex> lock(channel_mutex_);
        j["subscribed_channels"] = subscribed_channels_;
        j["subscribed_patterns"] = subscribed_patterns_;
    }
    
    return j;
}

// =============================================================================
// 내부 메서드
// =============================================================================

void AlarmSubscriber::subscribeLoop() {
    LogManager::getInstance().Info("구독 루프 시작");
    
    while (!should_stop_.load()) {
        try {
            if (!redis_client_ || !redis_client_->isConnected()) {
                LogManager::getInstance().Warn("Redis 연결 대기 중...");
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            
            // Redis 메시지 콜백 설정
            redis_client_->setMessageCallback(
                [this](const std::string& channel, const std::string& message) {
                    handleMessage(channel, message);
                });
            
            // 모든 채널 구독
            if (!subscribeAllChannels()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            
            is_connected_ = true;
            LogManager::getInstance().Info("Redis Pub/Sub 구독 시작됨");
            
            // 메시지 수신 대기 (블로킹)
            while (!should_stop_.load() && is_connected_.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // 연결 상태 체크
                if (redis_client_ && !redis_client_->isConnected()) {
                    is_connected_ = false;
                    LogManager::getInstance().Warn("Redis 연결 끊김");
                    break;
                }
            }
            
        } catch (const std::exception& e) {
            is_connected_ = false;
            LogManager::getInstance().Error("구독 루프 에러: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    LogManager::getInstance().Info("구독 루프 종료");
}

void AlarmSubscriber::workerLoop(int thread_index) {
    LogManager::getInstance().Info("워커 " + std::to_string(thread_index) + " 시작");
    
    while (!should_stop_.load()) {
        PulseOne::CSP::AlarmMessage alarm;
        
        // 큐에서 알람 가져오기
        if (!dequeueAlarm(alarm)) {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait_for(lock, std::chrono::milliseconds(100), 
                [this] { return !message_queue_.empty() || should_stop_.load(); });
            continue;
        }
        
        // 처리
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            // Pre-process 콜백
            {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                if (pre_process_callback_) {
                    pre_process_callback_(alarm);
                }
            }
            
            // 알람 처리
            processAlarm(alarm);
            
            // Post-process 콜백
            {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                if (post_process_callback_) {
                    post_process_callback_(alarm);
                }
            }
            
            // 통계 업데이트
            total_processed_.fetch_add(1);
            
            auto now = std::chrono::system_clock::now();
            last_processed_timestamp_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            
            if (config_.enable_debug_log) {
                LogManager::getInstance().Debug(
                    "워커 " + std::to_string(thread_index) + " 알람 처리: " + alarm.nm);
            }
            
        } catch (const std::exception& e) {
            total_failed_.fetch_add(1);
            LogManager::getInstance().Error(
                "워커 " + std::to_string(thread_index) + 
                " 알람 처리 실패: " + std::string(e.what()));
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        total_processing_time_ms_.fetch_add(duration.count());
    }
    
    LogManager::getInstance().Info("워커 " + std::to_string(thread_index) + " 종료");
}

void AlarmSubscriber::reconnectLoop() {
    LogManager::getInstance().Info("재연결 루프 시작");
    
    while (!should_stop_.load()) {
        std::this_thread::sleep_for(
            std::chrono::seconds(config_.reconnect_interval_seconds));
        
        if (should_stop_.load()) break;
        
        // 연결 끊김 확인
        if (!is_connected_.load() && is_running_.load()) {
            LogManager::getInstance().Info("Redis 재연결 시도...");
            
            try {
                if (initializeRedisConnection()) {
                    LogManager::getInstance().Info("Redis 재연결 성공");
                } else {
                    LogManager::getInstance().Warn("Redis 재연결 실패");
                }
            } catch (const std::exception& e) {
                LogManager::getInstance().Error(
                    "재연결 실패: " + std::string(e.what()));
            }
        }
    }
    
    LogManager::getInstance().Info("재연결 루프 종료");
}

void AlarmSubscriber::handleMessage(const std::string& channel, 
                                    const std::string& message) {
    // 통계 업데이트
    total_received_.fetch_add(1);
    
    auto now = std::chrono::system_clock::now();
    last_received_timestamp_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    if (config_.enable_debug_log) {
        LogManager::getInstance().Debug("메시지 수신 [" + channel + "]: " + message);
    }
    
    // JSON 파싱
    auto alarm_opt = parseAlarmMessage(message);
    if (!alarm_opt.has_value()) {
        total_failed_.fetch_add(1);
        LogManager::getInstance().Warn("알람 메시지 파싱 실패");
        return;
    }
    
    auto alarm = alarm_opt.value();
    
    // 필터링
    if (!filterAlarm(alarm)) {
        total_filtered_.fetch_add(1);
        if (config_.enable_debug_log) {
            LogManager::getInstance().Debug("알람 필터링됨: " + alarm.nm);
        }
        return;
    }
    
    // 큐에 추가
    if (!enqueueAlarm(alarm)) {
        total_failed_.fetch_add(1);
        LogManager::getInstance().Warn("알람 큐 가득참, 메시지 버림: " + alarm.nm);
    }
}

std::optional<PulseOne::CSP::AlarmMessage> 
AlarmSubscriber::parseAlarmMessage(const std::string& json_str) {
    try {
        auto j = json::parse(json_str);
        
        PulseOne::CSP::AlarmMessage alarm;
        
        // Redis 알람 구조에서 AlarmMessage로 매핑
        // tenant_id → building_id
        alarm.bd = j.value("tenant_id", j.value("building_id", 0));
        
        // point_name
        alarm.nm = j.value("point_name", j.value("name", ""));
        
        // value를 double로 변환
        if (j.contains("value")) {
            if (j["value"].is_number()) {
                alarm.vl = j["value"].get<double>();
            } else if (j["value"].is_string()) {
                try {
                    std::string value_str = j["value"].get<std::string>();
                    alarm.vl = std::stod(value_str);
                } catch (...) {
                    alarm.vl = 0.0;
                }
            } else {
                alarm.vl = 0.0;
            }
        } else if (j.contains("trigger_value")) {
            alarm.vl = j["trigger_value"].get<double>();
        }
        
        // timestamp
        if (j.contains("timestamp")) {
            if (j["timestamp"].is_string()) {
                alarm.tm = j["timestamp"].get<std::string>();
            } else {
                // milliseconds -> string
                int64_t ts = j["timestamp"].get<int64_t>();
                alarm.tm = std::to_string(ts);
            }
        }
        
        // alarm flag (state가 "active"면 1, 아니면 0)
        if (j.contains("state")) {
            std::string state = j["state"].get<std::string>();
            alarm.al = (state == "active" || state == "ACTIVE") ? 1 : 0;
            alarm.st = alarm.al;
        } else if (j.contains("alarm_flag")) {
            alarm.al = j["alarm_flag"].get<int>();
            alarm.st = alarm.al;
        }
        
        // description/message
        alarm.des = j.value("message", j.value("description", ""));
        
        return alarm;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "알람 메시지 파싱 실패: " + std::string(e.what()) + 
            " - JSON: " + json_str);
        return std::nullopt;
    }
}

void AlarmSubscriber::processAlarm(const PulseOne::CSP::AlarmMessage& alarm) {
    if (!csp_gateway_) {
        throw std::runtime_error("CSPGateway가 초기화되지 않음");
    }
    
    // CSPGateway를 통해 전송
    auto result = csp_gateway_->taskAlarmSingle(alarm);
    
    if (!result.success) {
        LogManager::getInstance().Warn(
            "알람 전송 실패: " + alarm.nm + " - " + result.error_message);
        throw std::runtime_error("알람 전송 실패: " + result.error_message);
    }
}

bool AlarmSubscriber::filterAlarm(const PulseOne::CSP::AlarmMessage& alarm) const {
    // 심각도 필터링
    if (config_.filter_by_severity && !config_.allowed_severities.empty()) {
        // TODO: AlarmMessage에 severity 필드가 추가되면 구현
        // 현재는 모두 통과
    }
    
    // 기본 유효성 검사
    if (alarm.nm.empty()) {
        return false;
    }
    
    return true;
}

bool AlarmSubscriber::initializeRedisConnection() {
    try {
        LogManager::getInstance().Info("Redis 연결 초기화 중...");
        
        redis_client_ = std::make_shared<RedisClientImpl>();
        
        if (!redis_client_->isConnected()) {
            LogManager::getInstance().Error("Redis 자동 연결 실패");
            return false;
        }
        
        LogManager::getInstance().Info("Redis 연결 성공");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Redis 연결 실패: " + std::string(e.what()));
        return false;
    }
}

bool AlarmSubscriber::initializeCSPGateway() {
    try {
        LogManager::getInstance().Info("CSPGateway 초기화 중...");
        
        csp_gateway_ = std::make_unique<PulseOne::CSP::CSPGateway>(
            config_.gateway_config);
        
        LogManager::getInstance().Info("CSPGateway 초기화 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "CSPGateway 초기화 실패: " + std::string(e.what()));
        return false;
    }
}

bool AlarmSubscriber::subscribeAllChannels() {
    bool success = true;
    
    try {
        std::lock_guard<std::mutex> lock(channel_mutex_);
        
        // 채널 구독
        for (const auto& channel : subscribed_channels_) {
            if (!redis_client_->subscribe(channel)) {
                LogManager::getInstance().Error("채널 구독 실패: " + channel);
                success = false;
            } else {
                LogManager::getInstance().Info("채널 구독 성공: " + channel);
            }
        }
        
        // 패턴 구독
        for (const auto& pattern : subscribed_patterns_) {
            if (!redis_client_->psubscribe(pattern)) {
                LogManager::getInstance().Error("패턴 구독 실패: " + pattern);
                success = false;
            } else {
                LogManager::getInstance().Info("패턴 구독 성공: " + pattern);
            }
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("채널 구독 중 예외: " + std::string(e.what()));
        return false;
    }
    
    return success;
}

bool AlarmSubscriber::enqueueAlarm(const PulseOne::CSP::AlarmMessage& alarm) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    // 큐 크기 체크
    if (message_queue_.size() >= config_.max_queue_size) {
        // 최대 크기 기록
        size_t current_size = message_queue_.size();
        size_t max_reached = max_queue_size_reached_.load();
        if (current_size > max_reached) {
            max_queue_size_reached_ = current_size;
        }
        return false;
    }
    
    message_queue_.push(alarm);
    queue_cv_.notify_one();
    
    return true;
}

bool AlarmSubscriber::dequeueAlarm(PulseOne::CSP::AlarmMessage& alarm) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    if (message_queue_.empty()) {
        return false;
    }
    
    alarm = message_queue_.front();
    message_queue_.pop();
    
    return true;
}

} // namespace Alarm
} // namespace PulseOne