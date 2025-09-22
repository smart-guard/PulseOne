/**
 * @file WorkerThreadManager.h
 * @brief 워커 스레드 생명주기 관리 컴포넌트
 * @details 읽기/쓰기/제어 스레드의 시작, 정지, 일시정지, 재개를 관리
 * @author PulseOne Development Team
 * @date 2025-01-20
 * @version 1.0.0
 */

#ifndef COMPONENTS_WORKER_THREAD_MANAGER_H
#define COMPONENTS_WORKER_THREAD_MANAGER_H

#include "Utils/LogManager.h"
#include <thread>
#include <atomic>
#include <memory>
#include <functional>
#include <chrono>
#include <vector>
#include <string>

namespace PulseOne {
namespace Components {

/**
 * @brief 스레드 상태 열거형
 */
enum class ThreadState {
    STOPPED = 0,        ///< 정지됨
    STARTING = 1,       ///< 시작 중
    RUNNING = 2,        ///< 실행 중
    PAUSED = 3,         ///< 일시정지됨
    STOPPING = 4        ///< 정지 중
};

/**
 * @brief 관리되는 스레드 정보
 */
struct ManagedThread {
    std::string name;                                    ///< 스레드 이름
    std::unique_ptr<std::thread> thread;                 ///< 스레드 객체
    std::function<void()> thread_function;               ///< 스레드 함수
    std::atomic<ThreadState> state{ThreadState::STOPPED}; ///< 스레드 상태
    std::chrono::system_clock::time_point start_time;   ///< 시작 시간
    std::atomic<uint64_t> iteration_count{0};           ///< 반복 횟수
    std::atomic<bool> should_pause{false};              ///< 일시정지 요청
    std::atomic<bool> should_stop{false};               ///< 정지 요청
    
    /**
     * @brief 기본 생성자
     */
    ManagedThread() = default;
    
    /**
     * @brief 생성자
     * @param thread_name 스레드 이름
     * @param func 스레드 함수
     */
    ManagedThread(const std::string& thread_name, std::function<void()> func)
        : name(thread_name), thread_function(std::move(func)) {}
};

/**
 * @brief 워커 스레드 관리자 클래스
 * @details 여러 워커 스레드의 생명주기를 통합 관리
 */
class WorkerThreadManager {
public:
    /**
     * @brief 생성자
     * @param logger 로거 (선택사항)
     */
    explicit WorkerThreadManager(std::shared_ptr<LogManager> logger = nullptr);
    
    /**
     * @brief 소멸자
     */
    ~WorkerThreadManager();
    
    // 복사/이동 방지
    WorkerThreadManager(const WorkerThreadManager&) = delete;
    WorkerThreadManager& operator=(const WorkerThreadManager&) = delete;
    WorkerThreadManager(WorkerThreadManager&&) = delete;
    WorkerThreadManager& operator=(WorkerThreadManager&&) = delete;
    
    // =============================================================================
    // 스레드 등록 및 제거
    // =============================================================================
    
    /**
     * @brief 관리할 스레드 등록
     * @param name 스레드 이름
     * @param thread_function 실행할 함수
     * @return 성공 시 true
     */
    bool RegisterThread(const std::string& name, std::function<void()> thread_function);
    
    /**
     * @brief 스레드 등록 해제
     * @param name 스레드 이름
     * @return 성공 시 true
     */
    bool UnregisterThread(const std::string& name);
    
    /**
     * @brief 등록된 모든 스레드 제거
     */
    void ClearAllThreads();
    
    // =============================================================================
    // 스레드 제어
    // =============================================================================
    
    /**
     * @brief 모든 스레드 시작
     * @return 성공 시 true
     */
    bool StartAllThreads();
    
    /**
     * @brief 특정 스레드 시작
     * @param name 스레드 이름
     * @return 성공 시 true
     */
    bool StartThread(const std::string& name);
    
    /**
     * @brief 모든 스레드 정지
     * @param timeout_ms 타임아웃 (밀리초)
     * @return 성공 시 true
     */
    bool StopAllThreads(int timeout_ms = 5000);
    
    /**
     * @brief 특정 스레드 정지
     * @param name 스레드 이름
     * @param timeout_ms 타임아웃 (밀리초)
     * @return 성공 시 true
     */
    bool StopThread(const std::string& name, int timeout_ms = 5000);
    
    /**
     * @brief 모든 스레드 일시정지
     * @return 성공 시 true
     */
    bool PauseAllThreads();
    
    /**
     * @brief 특정 스레드 일시정지
     * @param name 스레드 이름
     * @return 성공 시 true
     */
    bool PauseThread(const std::string& name);
    
    /**
     * @brief 모든 스레드 재개
     * @return 성공 시 true
     */
    bool ResumeAllThreads();
    
    /**
     * @brief 특정 스레드 재개
     * @param name 스레드 이름
     * @return 성공 시 true
     */
    bool ResumeThread(const std::string& name);
    
    // =============================================================================
    // 상태 조회
    // =============================================================================
    
    /**
     * @brief 특정 스레드 상태 조회
     * @param name 스레드 이름
     * @return 스레드 상태
     */
    ThreadState GetThreadState(const std::string& name) const;
    
    /**
     * @brief 모든 스레드가 실행 중인지 확인
     * @return 모두 실행 중이면 true
     */
    bool AreAllThreadsRunning() const;
    
    /**
     * @brief 실행 중인 스레드 개수 조회
     * @return 실행 중인 스레드 개수
     */
    size_t GetRunningThreadCount() const;
    
    /**
     * @brief 등록된 스레드 개수 조회
     * @return 등록된 스레드 개수
     */
    size_t GetTotalThreadCount() const;
    
    /**
     * @brief 스레드 목록 조회
     * @return 스레드 이름 벡터
     */
    std::vector<std::string> GetThreadNames() const;
    
    /**
     * @brief 스레드 상태 정보 JSON 반환
     * @return JSON 형태의 상태 정보
     */
    std::string GetStatusJson() const;
    
    // =============================================================================
    // 모니터링 및 진단
    // =============================================================================
    
    /**
     * @brief 특정 스레드 실행 통계 조회
     * @param name 스레드 이름
     * @return JSON 형태의 통계 정보
     */
    std::string GetThreadStats(const std::string& name) const;
    
    /**
     * @brief 모든 스레드 성능 통계 조회
     * @return JSON 형태의 성능 통계
     */
    std::string GetPerformanceStats() const;
    
    /**
     * @brief 스레드 건강 상태 체크
     * @return 모든 스레드가 정상이면 true
     */
    bool CheckThreadHealth() const;

protected:
    // =============================================================================
    // 내부 유틸리티 함수들
    // =============================================================================
    
    /**
     * @brief 스레드를 안전하게 랩핑하는 함수
     * @param managed_thread 관리되는 스레드 정보
     */
    void ThreadWrapper(ManagedThread* managed_thread);
    
    /**
     * @brief 스레드 상태를 문자열로 변환
     * @param state 스레드 상태
     * @return 상태 문자열
     */
    std::string ThreadStateToString(ThreadState state) const;
    
    /**
     * @brief 로그 메시지 출력
     * @param level 로그 레벨
     * @param message 메시지
     */
    void LogMessage(LogLevel level, const std::string& message) const;

private:
    // =============================================================================
    // 내부 데이터 멤버
    // =============================================================================
    
    std::shared_ptr<LogManager> logger_;                     ///< 로거
    
    mutable std::mutex threads_mutex_;                       ///< 스레드 맵 뮤텍스
    std::map<std::string, std::unique_ptr<ManagedThread>> managed_threads_; ///< 관리되는 스레드들
    
    std::atomic<bool> manager_shutdown_requested_{false};    ///< 매니저 종료 요청
    
    // =============================================================================
    // 통계 정보
    // =============================================================================
    
    struct ManagerStatistics {
        std::atomic<uint64_t> total_threads_created{0};      ///< 생성된 총 스레드 수
        std::atomic<uint64_t> total_threads_destroyed{0};    ///< 삭제된 총 스레드 수
        std::atomic<uint64_t> successful_starts{0};          ///< 성공한 시작 횟수
        std::atomic<uint64_t> failed_starts{0};              ///< 실패한 시작 횟수
        std::atomic<uint64_t> successful_stops{0};           ///< 성공한 정지 횟수
        std::atomic<uint64_t> failed_stops{0};               ///< 실패한 정지 횟수
        std::chrono::system_clock::time_point manager_start_time; ///< 매니저 시작 시간
    } statistics_;
    
    // =============================================================================
    // 내부 메서드들
    // =============================================================================
    
    /**
     * @brief 스레드 찾기 (내부용)
     * @param name 스레드 이름
     * @return 스레드 포인터 (없으면 nullptr)
     */
    ManagedThread* FindThread(const std::string& name) const;
    
    /**
     * @brief 스레드가 정상적으로 정지될 때까지 대기
     * @param managed_thread 관리되는 스레드
     * @param timeout_ms 타임아웃 (밀리초)
     * @return 정상 정지 시 true
     */
    bool WaitForThreadStop(ManagedThread* managed_thread, int timeout_ms) const;
    
    /**
     * @brief 통계 업데이트
     * @param operation 작업 유형 ("start", "stop", "create", "destroy")
     * @param success 성공 여부
     */
    void UpdateStatistics(const std::string& operation, bool success);
};

} // namespace Components
} // namespace PulseOne

#endif // COMPONENTS_WORKER_THREAD_MANAGER_H