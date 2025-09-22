/**
 * @file RetryManager.h
 * @brief 재시도 로직 관리자 - 지수 백오프 및 우선순위 지원 (Shared Library)
 * @author PulseOne Development Team
 * @date 2025-09-22
 * @version 1.0.0
 */

#ifndef PULSEONE_UTILS_RETRY_MANAGER_H
#define PULSEONE_UTILS_RETRY_MANAGER_H

#include <chrono>
#include <functional>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <string>
#include <vector>

namespace PulseOne {
namespace Utils {

/**
 * @brief 재시도 항목 (범용 템플릿)
 */
template<typename T>
struct RetryItem {
    T data;                                              ///< 재시도할 데이터
    int attempt_count = 0;                               ///< 현재 시도 횟수
    int max_attempts = 3;                                ///< 최대 시도 횟수
    std::chrono::system_clock::time_point next_retry_time; ///< 다음 재시도 시간
    std::chrono::milliseconds base_delay{1000};         ///< 기본 지연시간
    std::string last_error;                              ///< 마지막 오류 메시지
    int priority = 5;                                    ///< 우선순위 (1=최고, 10=최저)
    std::string item_id;                                 ///< 아이템 식별자
    
    // 우선순위 큐를 위한 비교 연산자 (우선순위가 높을수록 먼저 처리)
    bool operator<(const RetryItem& other) const {
        if (priority != other.priority) {
            return priority > other.priority; // 낮은 숫자가 높은 우선순위
        }
        return next_retry_time > other.next_retry_time; // 빠른 시간이 높은 우선순위
    }
};

/**
 * @brief 재시도 설정
 */
struct RetryConfig {
    int max_attempts = 3;                                ///< 최대 재시도 횟수
    std::chrono::milliseconds base_delay{1000};         ///< 기본 지연시간 (1초)
    std::chrono::milliseconds max_delay{300000};        ///< 최대 지연시간 (5분)
    double backoff_multiplier = 2.0;                    ///< 지수 백오프 배수
    double jitter_factor = 0.1;                         ///< 지터 팩터 (10%)
    
    // 우선순위별 설정
    std::chrono::milliseconds critical_delay{100};       ///< CRITICAL 우선순위 지연
    std::chrono::milliseconds high_delay{1000};         ///< HIGH 우선순위 지연
    std::chrono::milliseconds medium_delay{5000};       ///< MEDIUM 우선순위 지연
    std::chrono::milliseconds low_delay{10000};         ///< LOW 우선순위 지연
    
    // 큐 크기 제한
    size_t max_queue_size = 10000;                       ///< 최대 큐 크기
    size_t high_priority_reserve = 1000;                 ///< 고우선순위 예약 공간
    
    // 성능 설정
    int worker_thread_count = 1;                         ///< 워커 스레드 개수
    std::chrono::milliseconds worker_sleep_ms{100};     ///< 워커 대기 시간
};

/**
 * @brief 재시도 통계
 */
struct RetryStats {
    std::atomic<size_t> total_items{0};                  ///< 총 재시도 항목 수
    std::atomic<size_t> successful_retries{0};           ///< 성공한 재시도 수
    std::atomic<size_t> failed_retries{0};               ///< 실패한 재시도 수
    std::atomic<size_t> abandoned_retries{0};            ///< 포기한 재시도 수
    std::atomic<size_t> queue_size{0};                   ///< 현재 큐 크기
    
    std::chrono::system_clock::time_point last_retry_time;
    double avg_retry_delay_ms = 0.0;
    
    // 우선순위별 통계
    std::atomic<size_t> critical_retries{0};
    std::atomic<size_t> high_retries{0};
    std::atomic<size_t> medium_retries{0};
    std::atomic<size_t> low_retries{0};
};

/**
 * @brief 재시도 결과
 */
enum class RetryResult {
    SUCCESS,                                             ///< 성공
    FAILED_RETRY,                                        ///< 실패 (재시도 예정)
    FAILED_ABANDON,                                      ///< 실패 (포기)
    QUEUE_FULL,                                          ///< 큐 가득참
    INVALID_DATA                                         ///< 잘못된 데이터
};

/**
 * @brief 재시도 콜백 함수 타입 (범용 템플릿)
 * @param data 재시도할 데이터
 * @param attempt_count 현재 시도 횟수
 * @return 성공 여부 및 오류 메시지
 */
template<typename T>
using RetryCallback = std::function<std::pair<bool, std::string>(const T&, int)>;

/**
 * @brief 우선순위 결정 함수 타입 (범용 템플릿)
 * @param data 우선순위를 결정할 데이터
 * @return 우선순위 (1=최고, 10=최저)
 */
template<typename T>
using PriorityFunction = std::function<int(const T&)>;

/**
 * @brief 재시도 관리자 클래스 (Shared Library - 템플릿)
 * 
 * 특징:
 * - 지수 백오프 (Exponential Backoff)
 * - 지터 (Jitter) 추가로 동시 재시도 방지
 * - 우선순위 기반 처리
 * - 큐 크기 제한
 * - 비동기 처리
 * - 템플릿으로 모든 데이터 타입 지원
 * 
 * 용도: CSP Gateway, Protocol Drivers, Database 연결, 외부 API 호출 등
 */
template<typename T>
class RetryManager {
public:
    /**
     * @brief 생성자
     * @param config 재시도 설정
     * @param callback 재시도 콜백 함수
     * @param priority_func 우선순위 결정 함수 (선택사항)
     */
    explicit RetryManager(const RetryConfig& config, 
                         RetryCallback<T> callback,
                         PriorityFunction<T> priority_func = nullptr);
    
    /**
     * @brief 소멸자
     */
    ~RetryManager();

    // 복사/이동 생성자 비활성화
    RetryManager(const RetryManager&) = delete;
    RetryManager& operator=(const RetryManager&) = delete;
    RetryManager(RetryManager&&) = delete;
    RetryManager& operator=(RetryManager&&) = delete;

    // =======================================================================
    // 재시도 관리 메서드들
    // =======================================================================
    
    /**
     * @brief 재시도 항목 추가
     * @param data 재시도할 데이터
     * @param error_message 오류 메시지
     * @param priority 우선순위 (1=최고, 10=최저, 0=자동결정)
     * @param item_id 아이템 식별자 (선택사항)
     * @return 추가 결과
     */
    RetryResult addRetryItem(const T& data, 
                            const std::string& error_message,
                            int priority = 0,
                            const std::string& item_id = "");
    
    /**
     * @brief 즉시 재시도 (동기식)
     * @param data 재시도할 데이터
     * @param max_attempts 최대 시도 횟수
     * @return 최종 성공 여부
     */
    bool retryImmediate(const T& data, int max_attempts = 3);

    // =======================================================================
    // 서비스 제어
    // =======================================================================
    
    /**
     * @brief 재시도 서비스 시작
     * @return 시작 성공 여부
     */
    bool start();
    
    /**
     * @brief 재시도 서비스 중지
     * @param wait_for_completion 진행 중인 작업 완료 대기 여부
     */
    void stop(bool wait_for_completion = true);
    
    /**
     * @brief 서비스 실행 상태
     * @return 실행 중이면 true
     */
    bool isRunning() const { return is_running_.load(); }

    // =======================================================================
    // 설정 및 상태 조회
    // =======================================================================
    
    /**
     * @brief 설정 업데이트
     * @param new_config 새로운 설정
     */
    void updateConfig(const RetryConfig& new_config);
    
    /**
     * @brief 현재 설정 조회
     * @return 현재 설정
     */
    const RetryConfig& getConfig() const;
    
    /**
     * @brief 통계 정보 조회
     * @return 통계 정보
     */
    RetryStats getStats() const;
    
    /**
     * @brief 통계 초기화
     */
    void resetStats();
    
    /**
     * @brief 큐 크기 조회
     * @return 현재 큐에 있는 항목 수
     */
    size_t getQueueSize() const { return stats_.queue_size.load(); }
    
    /**
     * @brief 큐가 비어있는지 확인
     * @return 비어있으면 true
     */
    bool isQueueEmpty() const { return getQueueSize() == 0; }

    // =======================================================================
    // 유틸리티 메서드들
    // =======================================================================
    
    /**
     * @brief 지연시간 계산 (지수 백오프 + 지터)
     * @param attempt_count 시도 횟수
     * @param base_delay 기본 지연시간
     * @return 계산된 지연시간
     */
    std::chrono::milliseconds calculateDelay(int attempt_count, 
                                            std::chrono::milliseconds base_delay) const;
    
    /**
     * @brief 우선순위별 기본 지연시간 조회
     * @param priority 우선순위
     * @return 기본 지연시간
     */
    std::chrono::milliseconds getPriorityDelay(int priority) const;
    
    /**
     * @brief 큐 정리 (만료된 항목 제거)
     * @return 제거된 항목 수
     */
    size_t cleanExpiredItems();
    
    /**
     * @brief 큐에서 특정 아이템 제거
     * @param item_id 아이템 식별자
     * @return 제거된 항목 수
     */
    size_t removeItems(const std::string& item_id);

private:
    // =======================================================================
    // 내부 구현 메서드들
    // =======================================================================
    
    /**
     * @brief 재시도 워커 스레드
     * @param thread_id 스레드 식별자
     */
    void retryWorkerThread(int thread_id);
    
    /**
     * @brief 재시도 실행
     * @param item 재시도 항목
     * @return 성공 여부
     */
    bool executeRetry(RetryItem<T>& item);
    
    /**
     * @brief 다음 재시도 시간 계산
     * @param item 재시도 항목
     * @return 다음 재시도 시간
     */
    std::chrono::system_clock::time_point calculateNextRetryTime(const RetryItem<T>& item) const;
    
    /**
     * @brief 큐 공간 확보 (필요 시 낮은 우선순위 항목 제거)
     * @param required_priority 필요한 우선순위
     * @return 공간 확보 성공 여부
     */
    bool makeQueueSpace(int required_priority);
    
    /**
     * @brief 지터 추가
     * @param delay 기본 지연시간
     * @return 지터가 추가된 지연시간
     */
    std::chrono::milliseconds addJitter(std::chrono::milliseconds delay) const;
    
    /**
     * @brief 우선순위 결정 (자동)
     * @param data 데이터
     * @return 결정된 우선순위
     */
    int determinePriority(const T& data) const;

    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    RetryConfig config_;                                  ///< 재시도 설정
    mutable RetryStats stats_;                            ///< 통계 정보
    RetryCallback<T> retry_callback_;                     ///< 재시도 콜백
    PriorityFunction<T> priority_function_;               ///< 우선순위 결정 함수
    
    std::atomic<bool> is_running_{false};                ///< 실행 상태
    std::atomic<bool> should_stop_{false};               ///< 중지 플래그
    
    // 스레드 관리
    std::vector<std::unique_ptr<std::thread>> worker_threads_; ///< 워커 스레드들
    
    // 동기화 객체들
    mutable std::mutex config_mutex_;                     ///< 설정 보호
    mutable std::mutex stats_mutex_;                      ///< 통계 보호
    std::mutex queue_mutex_;                              ///< 큐 보호
    std::condition_variable queue_cv_;                    ///< 큐 대기
    
    // 우선순위 큐
    std::priority_queue<RetryItem<T>> retry_queue_;       ///< 재시도 큐
};

// =======================================================================
// 템플릿 구현 (헤더 파일에 포함)
// =======================================================================

template<typename T>
RetryManager<T>::RetryManager(const RetryConfig& config, 
                             RetryCallback<T> callback,
                             PriorityFunction<T> priority_func)
    : config_(config), retry_callback_(callback), priority_function_(priority_func) {
    
    // 설정 검증
    if (!retry_callback_) {
        throw std::invalid_argument("Retry callback cannot be null");
    }
    
    if (config_.max_attempts <= 0) {
        throw std::invalid_argument("Max attempts must be positive");
    }
}

template<typename T>
RetryManager<T>::~RetryManager() {
    stop(false); // 강제 종료
}

template<typename T>
bool RetryManager<T>::start() {
    if (is_running_.load()) {
        return false;
    }
    
    should_stop_.store(false);
    is_running_.store(true);
    
    // 워커 스레드들 시작
    worker_threads_.clear();
    worker_threads_.reserve(config_.worker_thread_count);
    
    for (int i = 0; i < config_.worker_thread_count; ++i) {
        worker_threads_.push_back(
            std::make_unique<std::thread>(&RetryManager::retryWorkerThread, this, i)
        );
    }
    
    return true;
}

template<typename T>
void RetryManager<T>::stop(bool wait_for_completion) {
    if (!is_running_.load()) {
        return;
    }
    
    should_stop_.store(true);
    is_running_.store(false);
    
    // 큐 대기 중인 스레드들 깨우기
    queue_cv_.notify_all();
    
    // 워커 스레드들 종료 대기
    for (auto& thread : worker_threads_) {
        if (thread && thread->joinable()) {
            if (wait_for_completion) {
                thread->join();
            } else {
                thread->detach();
            }
        }
    }
    
    worker_threads_.clear();
}

template<typename T>
RetryResult RetryManager<T>::addRetryItem(const T& data, 
                                         const std::string& error_message,
                                         int priority,
                                         const std::string& item_id) {
    
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    // 큐 크기 확인
    if (retry_queue_.size() >= config_.max_queue_size) {
        if (!makeQueueSpace(priority)) {
            return RetryResult::QUEUE_FULL;
        }
    }
    
    // 우선순위 결정
    if (priority <= 0) {
        priority = determinePriority(data);
    }
    
    // 재시도 항목 생성
    RetryItem<T> item;
    item.data = data;
    item.attempt_count = 1;
    item.max_attempts = config_.max_attempts;
    item.base_delay = getPriorityDelay(priority);
    item.last_error = error_message;
    item.priority = priority;
    item.item_id = item_id;
    item.next_retry_time = calculateNextRetryTime(item);
    
    // 큐에 추가
    retry_queue_.push(item);
    stats_.queue_size.store(retry_queue_.size());
    stats_.total_items++;
    
    // 워커 스레드 깨우기
    queue_cv_.notify_one();
    
    return RetryResult::FAILED_RETRY;
}

template<typename T>
const RetryConfig& RetryManager<T>::getConfig() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

template<typename T>
RetryStats RetryManager<T>::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

template<typename T>
void RetryManager<T>::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.total_items.store(0);
    stats_.successful_retries.store(0);
    stats_.failed_retries.store(0);
    stats_.abandoned_retries.store(0);
    stats_.avg_retry_delay_ms = 0.0;
    stats_.critical_retries.store(0);
    stats_.high_retries.store(0);
    stats_.medium_retries.store(0);
    stats_.low_retries.store(0);
}

template<typename T>
int RetryManager<T>::determinePriority(const T& data) const {
    if (priority_function_) {
        return priority_function_(data);
    }
    return 5; // 기본 우선순위
}

template<typename T>
std::chrono::milliseconds RetryManager<T>::getPriorityDelay(int priority) const {
    switch (priority) {
        case 1: return config_.critical_delay;
        case 2: 
        case 3: return config_.high_delay;
        case 4:
        case 5:
        case 6: return config_.medium_delay;
        default: return config_.low_delay;
    }
}

template<typename T>
std::chrono::milliseconds RetryManager<T>::calculateDelay(int attempt_count, 
                                                         std::chrono::milliseconds base_delay) const {
    
    // 지수 백오프 계산
    double multiplier = std::pow(config_.backoff_multiplier, attempt_count - 1);
    auto delay = std::chrono::milliseconds(static_cast<long long>(base_delay.count() * multiplier));
    
    // 최대 지연시간 제한
    if (delay > config_.max_delay) {
        delay = config_.max_delay;
    }
    
    // 지터 추가
    return addJitter(delay);
}

template<typename T>
std::chrono::milliseconds RetryManager<T>::addJitter(std::chrono::milliseconds delay) const {
    if (config_.jitter_factor <= 0.0) {
        return delay;
    }
    
    // ±jitter_factor만큼 랜덤 변화
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-config_.jitter_factor, config_.jitter_factor);
    
    double jitter = dis(gen);
    long long jittered_delay = static_cast<long long>(delay.count() * (1.0 + jitter));
    
    return std::chrono::milliseconds(std::max(0LL, jittered_delay));
}

} // namespace Utils
} // namespace PulseOne

#endif // PULSEONE_UTILS_RETRY_MANAGER_H