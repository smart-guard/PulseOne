/**
 * @file AlarmSubscriber.cpp
 * @brief Redis Pub/Sub 알람 구독 및 전송 구현 (리팩토링 버전)
 * @author PulseOne Development Team
 * @date 2025-10-23
 * @version 2.0.0
 * 
 * 주요 변경사항:
 * - ❌ CSPGateway 제거
 * - ✅ DynamicTargetManager::getInstance() 사용
 * - ✅ processAlarm() 메서드 수정
 */

#include "Alarm/AlarmSubscriber.h"
#include "Client/RedisClientImpl.h"
#include "Export/ExportTypes.h" 
#include "CSP/DynamicTargetManager.h"
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
    
    LogManager::getInstance().Info("AlarmSubscriber v2.0 초기화 시작");
    LogManager::getInstance().Info("구독 채널: " + std::to_string(config_.subscribe_channels.size()) + "개");
    LogManager::getInstance().Info("워커 스레드: " + std::to_string(config_.worker_thread_count) + "개");
    LogManager::getInstance().Info("최대 큐 크기: " + std::to_string(config_.max_queue_size));
    LogManager::getInstance().Info("✅ DynamicTargetManager 싱글턴 사용");
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
    
    // ✅ 1. DynamicTargetManager 확인 (싱글턴이 초기화되었는지)
    try {
        auto& manager = PulseOne::CSP::DynamicTargetManager::getInstance();
        if (!manager.isRunning()) {
            LogManager::getInstance().Error("DynamicTargetManager가 실행되지 않음");
            return false;
        }
        LogManager::getInstance().Info("✅ DynamicTargetManager 연결 확인");
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("DynamicTargetManager 접근 실패: " + std::string(e.what()));
        return false;
    }
    
    // 2. Redis 연결 초기화
    if (!initializeRedisConnection()) {
        LogManager::getInstance().Error("Redis 연결 초기화 실패");
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
    
    LogManager::getInstance().Info("AlarmSubscriber 시작 완료 ✅");
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
            
            // ✅ 메시지 수신 대기 루프 (실제 메시지 읽기)
            while (!should_stop_.load() && is_connected_.load()) {
                // Redis에서 메시지 읽기 시도 (100ms 타임아웃)
                if (!redis_client_->waitForMessage(100)) {
                    // 타임아웃 - 연결 상태만 체크
                    if (redis_client_ && !redis_client_->isConnected()) {
                        is_connected_ = false;
                        LogManager::getInstance().Warn("Redis 연결 끊김");
                        break;
                    }
                    // 타임아웃은 정상 - 계속 대기
                    continue;
                }
                
                // 메시지 수신 성공 - 콜백이 자동으로 호출됨
                // (waitForMessage 내부에서 message_callback_ 실행)
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
    LogManager::getInstance().Info("워커 스레드 [" + std::to_string(thread_index) + "] 시작");
    
    while (!should_stop_.load()) {
        PulseOne::CSP::AlarmMessage alarm;
        
        // 큐에서 알람 가져오기
        if (!dequeueAlarm(alarm)) {
            // 큐가 비어있으면 대기
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait_for(lock, std::chrono::milliseconds(100), [this]() {
                return should_stop_.load() || !message_queue_.empty();
            });
            continue;
        }
        
        // 알람 처리
        try {
            auto process_start = std::chrono::steady_clock::now();
            
            processAlarm(alarm);
            
            auto process_end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                process_end - process_start).count();
            
            total_processed_.fetch_add(1);
            last_processed_timestamp_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            if (config_.enable_debug_log) {
                LogManager::getInstance().Debug(
                    "워커 [" + std::to_string(thread_index) + "] 처리 완료: " + 
                    alarm.nm + " (" + std::to_string(elapsed) + "ms)");
            }
            
        } catch (const std::exception& e) {
            total_failed_.fetch_add(1);
            LogManager::getInstance().Error(
                "워커 [" + std::to_string(thread_index) + "] 처리 실패: " + 
                alarm.nm + " - " + std::string(e.what()));
        }
    }
    
    LogManager::getInstance().Info("워커 스레드 [" + std::to_string(thread_index) + "] 종료");
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
        // ===================================================================
        // 1단계: JSON 파싱
        // ===================================================================
        LogManager::getInstance().Debug("🔍 [parseAlarm] JSON 파싱 시작");
        LogManager::getInstance().Debug("📄 [parseAlarm] JSON 원본: " + json_str);
        
        auto j = json::parse(json_str);
        
        LogManager::getInstance().Debug("✅ [parseAlarm] JSON 파싱 성공");
        LogManager::getInstance().Debug("📊 [parseAlarm] JSON 키 개수: " + 
            std::to_string(j.size()));
        
        // JSON 키 목록 출력 (디버그)
        std::stringstream keys_ss;
        keys_ss << "🔑 [parseAlarm] 발견된 키: [";
        bool first = true;
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (!first) keys_ss << ", ";
            keys_ss << "\"" << it.key() << "\"";
            first = false;
        }
        keys_ss << "]";
        LogManager::getInstance().Debug(keys_ss.str());
        
        PulseOne::CSP::AlarmMessage alarm;
        
        // ===================================================================
        // 2단계: 필드별 파싱 (icos 형식 우선)
        // ===================================================================
        
        // -------------------------------------------------------------------
        // 1. Building ID (bd → tenant_id → building_id)
        // -------------------------------------------------------------------
        LogManager::getInstance().Debug("🏢 [parseAlarm] Building ID 파싱 시작");
        
        if (j.contains("bd")) {
            alarm.bd = j["bd"].get<int>();
            LogManager::getInstance().Debug("✅ [parseAlarm] bd 필드 발견 → 값: " + 
                std::to_string(alarm.bd));
        } else if (j.contains("tenant_id")) {
            alarm.bd = j["tenant_id"].get<int>();
            LogManager::getInstance().Debug("✅ [parseAlarm] tenant_id 필드 발견 → 값: " + 
                std::to_string(alarm.bd));
        } else if (j.contains("building_id")) {
            alarm.bd = j["building_id"].get<int>();
            LogManager::getInstance().Debug("✅ [parseAlarm] building_id 필드 발견 → 값: " + 
                std::to_string(alarm.bd));
        } else {
            alarm.bd = 0;
            LogManager::getInstance().Warn("⚠️ [parseAlarm] Building ID 필드 없음, 기본값: 0");
        }
        
        // -------------------------------------------------------------------
        // 2. Point Name (nm → point_name → name)
        // -------------------------------------------------------------------
        LogManager::getInstance().Debug("📍 [parseAlarm] Point Name 파싱 시작");
        
        bool nm_found = false;
        
        if (j.contains("nm")) {
            std::string nm_value = j["nm"].get<std::string>();
            alarm.nm = nm_value;
            nm_found = true;
            LogManager::getInstance().Debug("✅ [parseAlarm] nm 필드 발견");
            LogManager::getInstance().Debug("📝 [parseAlarm] nm 원본 값: \"" + nm_value + "\"");
            LogManager::getInstance().Debug("📝 [parseAlarm] alarm.nm 설정: \"" + alarm.nm + "\"");
            LogManager::getInstance().Debug("📏 [parseAlarm] alarm.nm 길이: " + 
                std::to_string(alarm.nm.length()));
        } else if (j.contains("point_name")) {
            std::string pn_value = j["point_name"].get<std::string>();
            alarm.nm = pn_value;
            nm_found = true;
            LogManager::getInstance().Debug("✅ [parseAlarm] point_name 필드 발견 → 값: \"" + 
                alarm.nm + "\"");
        } else if (j.contains("name")) {
            std::string name_value = j["name"].get<std::string>();
            alarm.nm = name_value;
            nm_found = true;
            LogManager::getInstance().Debug("✅ [parseAlarm] name 필드 발견 → 값: \"" + 
                alarm.nm + "\"");
        } else {
            alarm.nm = "";
            LogManager::getInstance().Warn("⚠️ [parseAlarm] Point Name 필드 없음!");
            LogManager::getInstance().Warn("❌ [parseAlarm] bd, point_name, name 모두 없음");
        }
        
        // nm 필드 최종 확인
        LogManager::getInstance().Debug("🔍 [parseAlarm] Point Name 최종 확인:");
        LogManager::getInstance().Debug("   - nm_found: " + std::string(nm_found ? "true" : "false"));
        LogManager::getInstance().Debug("   - alarm.nm: \"" + alarm.nm + "\"");
        LogManager::getInstance().Debug("   - alarm.nm.empty(): " + 
            std::string(alarm.nm.empty() ? "true" : "false"));
        LogManager::getInstance().Debug("   - alarm.nm.length(): " + 
            std::to_string(alarm.nm.length()));
        
        // -------------------------------------------------------------------
        // 3. Value (vl → value → trigger_value)
        // -------------------------------------------------------------------
        LogManager::getInstance().Debug("💰 [parseAlarm] Value 파싱 시작");
        
        if (j.contains("vl")) {
            if (j["vl"].is_number()) {
                alarm.vl = j["vl"].get<double>();
                LogManager::getInstance().Debug("✅ [parseAlarm] vl 필드(숫자) → 값: " + 
                    std::to_string(alarm.vl));
            } else if (j["vl"].is_string()) {
                try {
                    std::string vl_str = j["vl"].get<std::string>();
                    alarm.vl = std::stod(vl_str);
                    LogManager::getInstance().Debug("✅ [parseAlarm] vl 필드(문자열) → 값: " + 
                        std::to_string(alarm.vl));
                } catch (...) {
                    alarm.vl = 0.0;
                    LogManager::getInstance().Warn("⚠️ [parseAlarm] vl 문자열 변환 실패, 기본값: 0.0");
                }
            }
        } else if (j.contains("value")) {
            if (j["value"].is_number()) {
                alarm.vl = j["value"].get<double>();
                LogManager::getInstance().Debug("✅ [parseAlarm] value 필드 → 값: " + 
                    std::to_string(alarm.vl));
            } else if (j["value"].is_string()) {
                try {
                    alarm.vl = std::stod(j["value"].get<std::string>());
                    LogManager::getInstance().Debug("✅ [parseAlarm] value 필드(문자열) → 값: " + 
                        std::to_string(alarm.vl));
                } catch (...) {
                    alarm.vl = 0.0;
                    LogManager::getInstance().Warn("⚠️ [parseAlarm] value 문자열 변환 실패");
                }
            }
        } else if (j.contains("trigger_value")) {
            alarm.vl = j["trigger_value"].get<double>();
            LogManager::getInstance().Debug("✅ [parseAlarm] trigger_value 필드 → 값: " + 
                std::to_string(alarm.vl));
        } else {
            alarm.vl = 0.0;
            LogManager::getInstance().Warn("⚠️ [parseAlarm] Value 필드 없음, 기본값: 0.0");
        }
        
        // -------------------------------------------------------------------
        // 4. Timestamp (tm → timestamp)
        // -------------------------------------------------------------------
        LogManager::getInstance().Debug("⏰ [parseAlarm] Timestamp 파싱 시작");
        
        if (j.contains("tm")) {
            if (j["tm"].is_string()) {
                alarm.tm = j["tm"].get<std::string>();
                LogManager::getInstance().Debug("✅ [parseAlarm] tm 필드(문자열) → 값: \"" + 
                    alarm.tm + "\"");
            } else if (j["tm"].is_number()) {
                int64_t ts = j["tm"].get<int64_t>();
                alarm.tm = std::to_string(ts);
                LogManager::getInstance().Debug("✅ [parseAlarm] tm 필드(숫자) → 값: " + 
                    alarm.tm);
            }
        } else if (j.contains("timestamp")) {
            if (j["timestamp"].is_string()) {
                alarm.tm = j["timestamp"].get<std::string>();
                LogManager::getInstance().Debug("✅ [parseAlarm] timestamp 필드 → 값: \"" + 
                    alarm.tm + "\"");
            } else if (j["timestamp"].is_number()) {
                int64_t ts = j["timestamp"].get<int64_t>();
                alarm.tm = std::to_string(ts);
                LogManager::getInstance().Debug("✅ [parseAlarm] timestamp 필드(숫자) → 값: " + 
                    alarm.tm);
            }
        } else {
            // 현재 시간을 기본값으로
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;
            
            std::stringstream ss;
            ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            ss << "." << std::setfill('0') << std::setw(3) << ms.count();
            alarm.tm = ss.str();
            LogManager::getInstance().Warn("⚠️ [parseAlarm] Timestamp 필드 없음, 현재 시간 사용: " + 
                alarm.tm);
        }
        
        // -------------------------------------------------------------------
        // 5. Alarm Flag (al → alarm_flag → state)
        // -------------------------------------------------------------------
        LogManager::getInstance().Debug("🚨 [parseAlarm] Alarm Flag 파싱 시작");
        
        if (j.contains("al")) {
            alarm.al = j["al"].get<int>();
            LogManager::getInstance().Debug("✅ [parseAlarm] al 필드 → 값: " + 
                std::to_string(alarm.al));
        } else if (j.contains("alarm_flag")) {
            alarm.al = j["alarm_flag"].get<int>();
            LogManager::getInstance().Debug("✅ [parseAlarm] alarm_flag 필드 → 값: " + 
                std::to_string(alarm.al));
        } else if (j.contains("state")) {
            std::string state = j["state"].get<std::string>();
            alarm.al = (state == "active" || state == "ACTIVE") ? 1 : 0;
            LogManager::getInstance().Debug("✅ [parseAlarm] state 필드 → 값: " + 
                std::to_string(alarm.al));
        } else {
            alarm.al = 0;
            LogManager::getInstance().Warn("⚠️ [parseAlarm] Alarm Flag 필드 없음, 기본값: 0");
        }
        
        // -------------------------------------------------------------------
        // 6. Status (st → status)
        // -------------------------------------------------------------------
        LogManager::getInstance().Debug("📊 [parseAlarm] Status 파싱 시작");
        
        if (j.contains("st")) {
            alarm.st = j["st"].get<int>();
            LogManager::getInstance().Debug("✅ [parseAlarm] st 필드 → 값: " + 
                std::to_string(alarm.st));
        } else if (j.contains("status")) {
            alarm.st = j["status"].get<int>();
            LogManager::getInstance().Debug("✅ [parseAlarm] status 필드 → 값: " + 
                std::to_string(alarm.st));
        } else {
            // 기본값: alarm_flag와 동일
            alarm.st = alarm.al;
            LogManager::getInstance().Debug("ℹ️ [parseAlarm] Status 필드 없음, alarm_flag 사용: " + 
                std::to_string(alarm.st));
        }
        
        // -------------------------------------------------------------------
        // 7. Description (des → description → message)
        // -------------------------------------------------------------------
        LogManager::getInstance().Debug("📝 [parseAlarm] Description 파싱 시작");
        
        if (j.contains("des")) {
            alarm.des = j["des"].get<std::string>();
            LogManager::getInstance().Debug("✅ [parseAlarm] des 필드 → 값: \"" + 
                alarm.des + "\"");
        } else if (j.contains("description")) {
            alarm.des = j["description"].get<std::string>();
            LogManager::getInstance().Debug("✅ [parseAlarm] description 필드 → 값: \"" + 
                alarm.des + "\"");
        } else if (j.contains("message")) {
            alarm.des = j["message"].get<std::string>();
            LogManager::getInstance().Debug("✅ [parseAlarm] message 필드 → 값: \"" + 
                alarm.des + "\"");
        } else {
            alarm.des = "";
            LogManager::getInstance().Debug("ℹ️ [parseAlarm] Description 필드 없음");
        }
        
        // ===================================================================
        // 3단계: 파싱 결과 요약
        // ===================================================================
        LogManager::getInstance().Debug("📋 [parseAlarm] ===== 파싱 결과 요약 =====");
        LogManager::getInstance().Debug("   bd (Building ID): " + std::to_string(alarm.bd));
        LogManager::getInstance().Debug("   nm (Point Name): \"" + alarm.nm + "\"");
        LogManager::getInstance().Debug("   vl (Value): " + std::to_string(alarm.vl));
        LogManager::getInstance().Debug("   tm (Timestamp): \"" + alarm.tm + "\"");
        LogManager::getInstance().Debug("   al (Alarm Flag): " + std::to_string(alarm.al));
        LogManager::getInstance().Debug("   st (Status): " + std::to_string(alarm.st));
        LogManager::getInstance().Debug("   des (Description): \"" + alarm.des + "\"");
        LogManager::getInstance().Debug("========================================");
        
        // ===================================================================
        // 4단계: 유효성 검증
        // ===================================================================
        LogManager::getInstance().Debug("🔍 [parseAlarm] 유효성 검증 시작");
        
        // Point Name 검증
        if (alarm.nm.empty()) {
            LogManager::getInstance().Warn("❌ [parseAlarm] 유효성 검증 실패: point_name이 비어있음");
            LogManager::getInstance().Warn("📄 [parseAlarm] 원본 JSON: " + json_str);
            return std::nullopt;
        }
        LogManager::getInstance().Debug("✅ [parseAlarm] Point Name 검증 통과");
        
        // Building ID 검증
        if (alarm.bd <= 0) {
            LogManager::getInstance().Warn("❌ [parseAlarm] 유효성 검증 실패: building_id가 유효하지 않음 (값: " + 
                std::to_string(alarm.bd) + ")");
            LogManager::getInstance().Warn("📄 [parseAlarm] 원본 JSON: " + json_str);
            return std::nullopt;
        }
        LogManager::getInstance().Debug("✅ [parseAlarm] Building ID 검증 통과");
        
        // ===================================================================
        // 5단계: 성공 반환
        // ===================================================================
        LogManager::getInstance().Debug("🎉 [parseAlarm] 알람 파싱 완전 성공!");
        LogManager::getInstance().Debug("✅ [parseAlarm] 최종 AlarmMessage:");
        LogManager::getInstance().Debug("   - Building: " + std::to_string(alarm.bd));
        LogManager::getInstance().Debug("   - Point: \"" + alarm.nm + "\"");
        LogManager::getInstance().Debug("   - Value: " + std::to_string(alarm.vl));
        LogManager::getInstance().Debug("   - Alarm: " + std::to_string(alarm.al));
        
        return alarm;
        
    } catch (const json::parse_error& e) {
        LogManager::getInstance().Error("❌ [parseAlarm] JSON 파싱 예외 (parse_error): " + 
            std::string(e.what()));
        LogManager::getInstance().Error("📄 [parseAlarm] 문제 JSON: " + json_str);
        return std::nullopt;
        
    } catch (const json::type_error& e) {
        LogManager::getInstance().Error("❌ [parseAlarm] JSON 타입 예외 (type_error): " + 
            std::string(e.what()));
        LogManager::getInstance().Error("📄 [parseAlarm] 문제 JSON: " + json_str);
        return std::nullopt;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("❌ [parseAlarm] 알람 메시지 파싱 실패 (일반 예외): " + 
            std::string(e.what()));
        LogManager::getInstance().Error("📄 [parseAlarm] 문제 JSON: " + json_str);
        return std::nullopt;
    }
}


void AlarmSubscriber::processAlarm(const PulseOne::CSP::AlarmMessage& alarm) {
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        // ✅ DynamicTargetManager 싱글턴 사용
        auto& manager = PulseOne::CSP::DynamicTargetManager::getInstance();
        
        // Pre-process 콜백 실행
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (pre_process_callback_) {
                pre_process_callback_(alarm);
            }
        }
        
        // 전송 방식 선택
        std::vector<PulseOne::CSP::TargetSendResult> results;
        
        if (config_.use_parallel_send) {
            // 병렬 전송
            results = manager.sendAlarmToTargets(alarm);
        } else if (config_.max_priority_filter < 1000) {
            // 우선순위 필터 적용
            results = manager.sendAlarmToTargets(alarm);
        } else {
            // 기본 순차 전송
            results = manager.sendAlarmToTargets(alarm);
        }
        
        // 결과 처리
        int success_count = 0;
        int failure_count = 0;
        
        for (const auto& result : results) {
            if (result.success) {
                success_count++;
            } else {
                failure_count++;
                LogManager::getInstance().Warn(
                    "타겟 전송 실패 [" + result.target_name + "]: " + 
                    alarm.nm + " - " + result.error_message);
            }
        }
        
        // Post-process 콜백 실행
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (post_process_callback_) {
                post_process_callback_(alarm);
            }
        }
        
        // 통계 업데이트
        auto end_time = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        
        total_processing_time_ms_.fetch_add(elapsed_ms);
        
        if (failure_count > 0) {
            throw std::runtime_error(
                "일부 타겟 전송 실패: " + std::to_string(failure_count) + "/" + 
                std::to_string(results.size()));
        }
        
        LogManager::getInstance().Info(
            "알람 처리 완료: " + alarm.nm + " (" + 
            std::to_string(success_count) + "개 타겟, " + 
            std::to_string(elapsed_ms) + "ms)");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "알람 처리 실패: " + alarm.nm + " - " + std::string(e.what()));
        throw;
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