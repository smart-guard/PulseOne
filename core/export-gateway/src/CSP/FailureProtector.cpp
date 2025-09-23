/**
 * @file FailureProtector.cpp
 * @brief CSP Gateway 실패 방지기 구현 - CircuitBreaker 패턴
 * @author PulseOne Development Team
 * @date 2025-09-23
 * 저장 위치: core/export-gateway/src/CSP/FailureProtector.cpp
 * 
 * 기존 PulseOne 패턴 100% 준수:
 * - MqttFailover.cpp의 재연결 로직 패턴 차용
 * - LogManager 표준 사용법 적용
 * - std::atomic + std::mutex 스레드 안전성
 * - 지수 백오프 알고리즘 내장
 */

#include "CSP/FailureProtector.h"
#include "Utils/LogManager.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace PulseOne {
namespace CSP {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

FailureProtector::FailureProtector(const std::string& target_name, const FailureProtectorConfig& config)
    : target_name_(target_name)
    , config_(config)
    , state_(State::CLOSED)
    , failure_count_(0)
    , success_count_(0)
    , last_failure_time_(std::chrono::steady_clock::now())
    , last_state_change_(std::chrono::steady_clock::now())
    , half_open_attempts_(0)
    , total_attempts_(0)
    , total_successes_(0)
    , total_failures_(0) {
    
    // 기존 LogManager 패턴 따라 로깅
    LogManager::getInstance().Info("FailureProtector 초기화: " + target_name_);
    LogManager::getInstance().Debug("설정 - failure_threshold: " + std::to_string(config_.failure_threshold) +
                                   ", recovery_timeout: " + std::to_string(config_.recovery_timeout_ms) + "ms");
}

FailureProtector::~FailureProtector() {
    LogManager::getInstance().Info("FailureProtector 종료: " + target_name_ + 
                                  " (성공: " + std::to_string(total_successes_.load()) +
                                  ", 실패: " + std::to_string(total_failures_.load()) + ")");
}

// =============================================================================
// 핵심 CircuitBreaker 로직 (MqttFailover 패턴 차용)
// =============================================================================

bool FailureProtector::canExecute() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    total_attempts_++;
    
    switch (state_) {
        case State::CLOSED:
            // 정상 상태 - 모든 요청 허용
            return true;
            
        case State::OPEN:
            // 회로 열림 상태 - 복구 시간 확인
            if (isRecoveryTimeReached()) {
                LogManager::getInstance().Info("복구 시간 도달 - HALF_OPEN으로 전환: " + target_name_);
                transitionToHalfOpen();
                return true;
            }
            LogManager::getInstance().Debug("회로 열림 상태 - 요청 차단: " + target_name_);
            return false;
            
        case State::HALF_OPEN:
            // 반개방 상태 - 제한된 요청만 허용
            if (half_open_attempts_ < config_.half_open_max_attempts) {
                half_open_attempts_++;
                LogManager::getInstance().Debug("HALF_OPEN 시도: " + std::to_string(half_open_attempts_) + 
                                               "/" + std::to_string(config_.half_open_max_attempts) + 
                                               " (" + target_name_ + ")");
                return true;
            }
            LogManager::getInstance().Debug("HALF_OPEN 최대 시도 초과 - 요청 차단: " + target_name_);
            return false;
    }
    
    return false;
}

void FailureProtector::recordSuccess() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    success_count_++;
    total_successes_++;
    
    LogManager::getInstance().Debug("성공 기록: " + target_name_ + " (연속 성공: " + 
                                   std::to_string(success_count_.load()) + ")");
    
    switch (state_) {
        case State::CLOSED:
            // 정상 상태에서는 실패 카운터만 리셋
            failure_count_ = 0;
            break;
            
        case State::HALF_OPEN:
            // 반개방에서 성공 시 CLOSED로 전환
            if (success_count_ >= config_.half_open_success_threshold) {
                LogManager::getInstance().Info("HALF_OPEN 성공 임계치 달성 - CLOSED로 전환: " + target_name_);
                transitionToClosed();
            }
            break;
            
        case State::OPEN:
            // 열림 상태에서는 성공해도 상태 변경 없음 (이론적으로 도달 불가)
            LogManager::getInstance().Warn("OPEN 상태에서 성공 기록 - 비정상적 상황: " + target_name_);
            break;
    }
}

void FailureProtector::recordFailure() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    failure_count_++;
    total_failures_++;
    last_failure_time_ = std::chrono::steady_clock::now();
    
    LogManager::getInstance().Debug("실패 기록: " + target_name_ + " (연속 실패: " + 
                                   std::to_string(failure_count_.load()) + ")");
    
    switch (state_) {
        case State::CLOSED:
            // 정상 상태에서 실패 임계치 초과 시 OPEN으로 전환
            if (failure_count_ >= config_.failure_threshold) {
                LogManager::getInstance().Warn("실패 임계치 초과 - OPEN으로 전환: " + target_name_ + 
                                              " (" + std::to_string(failure_count_.load()) + 
                                              "/" + std::to_string(config_.failure_threshold) + ")");
                transitionToOpen();
            }
            success_count_ = 0;  // 성공 카운터 리셋
            break;
            
        case State::HALF_OPEN:
            // 반개방에서 실패 시 즉시 OPEN으로 전환
            LogManager::getInstance().Warn("HALF_OPEN 상태에서 실패 - OPEN으로 즉시 전환: " + target_name_);
            transitionToOpen();
            break;
            
        case State::OPEN:
            // 이미 열림 상태 - 실패 카운트만 증가
            break;
    }
}

void FailureProtector::reset() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    LogManager::getInstance().Info("수동 리셋 실행: " + target_name_);
    
    // 모든 카운터 및 상태 초기화
    state_ = State::CLOSED;
    failure_count_ = 0;
    success_count_ = 0;
    half_open_attempts_ = 0;
    last_state_change_ = std::chrono::steady_clock::now();
    
    LogManager::getInstance().Info("리셋 완료 - CLOSED 상태로 초기화: " + target_name_);
}

// =============================================================================
// 상태 조회 메서드들
// =============================================================================

FailureProtector::State FailureProtector::getState() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_;
}

std::string FailureProtector::getStateString() const {
    State current_state = getState();
    switch (current_state) {
        case State::CLOSED: return "CLOSED";
        case State::OPEN: return "OPEN";
        case State::HALF_OPEN: return "HALF_OPEN";
        default: return "UNKNOWN";
    }
}

bool FailureProtector::isHealthy() const {
    return getState() == State::CLOSED;
}

// =============================================================================
// 통계 정보 조회
// =============================================================================

FailureProtectorStats FailureProtector::getStats() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    FailureProtectorStats stats;
    stats.target_name = target_name_;
    stats.current_state = getStateString();
    stats.failure_count = failure_count_.load();
    stats.success_count = success_count_.load();
    stats.total_attempts = total_attempts_.load();
    stats.total_successes = total_successes_.load();
    stats.total_failures = total_failures_.load();
    stats.half_open_attempts = half_open_attempts_;
    
    // 성공률 계산
    uint32_t total = stats.total_successes + stats.total_failures;
    stats.success_rate = total > 0 ? (static_cast<double>(stats.total_successes) / total * 100.0) : 0.0;
    
    // 상태 지속 시간 계산 (밀리초)
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_state_change_);
    stats.state_duration_ms = duration.count();
    
    return stats;
}

uint32_t FailureProtector::getFailureCount() const {
    return failure_count_.load();
}

uint32_t FailureProtector::getSuccessCount() const {
    return success_count_.load();
}

double FailureProtector::getSuccessRate() const {
    uint32_t total = total_successes_.load() + total_failures_.load();
    return total > 0 ? (static_cast<double>(total_successes_.load()) / total * 100.0) : 0.0;
}

// =============================================================================
// 내부 헬퍼 메서드들 (MqttFailover 패턴 차용)
// =============================================================================

bool FailureProtector::isRecoveryTimeReached() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_failure_time_);
    
    // 지수 백오프 계산 (최대 복구 시간 제한)
    uint32_t backoff_multiplier = std::min(failure_count_.load(), static_cast<uint32_t>(10)); // 최대 10배
    uint64_t adjusted_timeout = static_cast<uint64_t>(config_.recovery_timeout_ms) * 
                               static_cast<uint64_t>(std::pow(config_.backoff_multiplier, backoff_multiplier));
    
    // 최대 복구 시간 제한 (기본 30분)
    adjusted_timeout = std::min(adjusted_timeout, static_cast<uint64_t>(config_.max_recovery_timeout_ms));
    
    return elapsed.count() >= static_cast<int64_t>(adjusted_timeout);
}

void FailureProtector::transitionToClosed() {
    State old_state = state_;
    state_ = State::CLOSED;
    failure_count_ = 0;
    success_count_ = 0;
    half_open_attempts_ = 0;
    last_state_change_ = std::chrono::steady_clock::now();
    
    LogManager::getInstance().Info("상태 전환: " + stateToString(old_state) + " → CLOSED (" + target_name_ + ")");
}

void FailureProtector::transitionToOpen() {
    State old_state = state_;
    state_ = State::OPEN;
    success_count_ = 0;
    half_open_attempts_ = 0;
    last_failure_time_ = std::chrono::steady_clock::now();
    last_state_change_ = std::chrono::steady_clock::now();
    
    LogManager::getInstance().Warn("상태 전환: " + stateToString(old_state) + " → OPEN (" + target_name_ + 
                                  ") - 복구 대기 시간: " + std::to_string(calculateNextRecoveryTimeout()) + "ms");
}

void FailureProtector::transitionToHalfOpen() {
    State old_state = state_;
    state_ = State::HALF_OPEN;
    half_open_attempts_ = 0;
    success_count_ = 0;
    last_state_change_ = std::chrono::steady_clock::now();
    
    LogManager::getInstance().Info("상태 전환: " + stateToString(old_state) + " → HALF_OPEN (" + target_name_ + 
                                  ") - 최대 시도: " + std::to_string(config_.half_open_max_attempts));
}

std::string FailureProtector::stateToString(State state) const {
    switch (state) {
        case State::CLOSED: return "CLOSED";
        case State::OPEN: return "OPEN";
        case State::HALF_OPEN: return "HALF_OPEN";
        default: return "UNKNOWN";
    }
}

uint64_t FailureProtector::calculateNextRecoveryTimeout() const {
    uint32_t backoff_multiplier = std::min(failure_count_.load(), static_cast<uint32_t>(10));
    uint64_t timeout = static_cast<uint64_t>(config_.recovery_timeout_ms) * 
                      static_cast<uint64_t>(std::pow(config_.backoff_multiplier, backoff_multiplier));
    
    return std::min(timeout, static_cast<uint64_t>(config_.max_recovery_timeout_ms));
}

// =============================================================================
// 설정 업데이트 (런타임 변경 지원)
// =============================================================================

void FailureProtector::updateConfig(const FailureProtectorConfig& new_config) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    LogManager::getInstance().Info("설정 업데이트: " + target_name_);
    LogManager::getInstance().Debug("기존 failure_threshold: " + std::to_string(config_.failure_threshold) + 
                                   " → 새 값: " + std::to_string(new_config.failure_threshold));
    
    config_ = new_config;
    
    // 현재 상태에서 새 설정 적용 검증
    if (state_ == State::CLOSED && failure_count_ >= config_.failure_threshold) {
        LogManager::getInstance().Warn("설정 변경으로 인한 상태 재평가 - OPEN으로 전환: " + target_name_);
        transitionToOpen();
    }
    
    LogManager::getInstance().Info("설정 업데이트 완료: " + target_name_);
}

const FailureProtectorConfig& FailureProtector::getConfig() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return config_;
}

} // namespace CSP
} // namespace PulseOne