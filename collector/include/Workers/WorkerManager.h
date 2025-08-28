// =============================================================================
// collector/include/Workers/WorkerManager.h
// 워커 생명주기 관리자 - 기존 패턴 준수 완성본
// =============================================================================

#ifndef WORKER_MANAGER_H
#define WORKER_MANAGER_H

#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <future>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Workers {
    class BaseDeviceWorker;
    class WorkerFactory;
}
}

namespace PulseOne {
namespace Workers {

/**
 * @brief WorkerManager - 활성 워커 관리 및 REST API 인터페이스
 * 
 * 역할:
 * - 활성 워커들의 생명주기 관리 (시작/중지/일시정지/재개)
 * - REST API 요청 처리
 * - 워커 상태 조회 및 제어
 * - WorkerFactory에서 생성된 워커들의 단일 소유권 관리
 */
class WorkerManager {
public:
    // ==========================================================================
    // 싱글톤 패턴
    // ==========================================================================
    static WorkerManager& getInstance();
    
    WorkerManager(const WorkerManager&) = delete;
    WorkerManager& operator=(const WorkerManager&) = delete;

    // ==========================================================================
    // 초기화 및 종료
    // ==========================================================================
    bool IsInitialized() const { return initialized_.load(); }

    // ==========================================================================
    // 워커 생명주기 관리 (REST API에서 호출되는 메서드들)
    // ==========================================================================
    
    /**
     * @brief 워커 시작 - 없으면 생성 후 시작
     */
    bool StartWorker(const std::string& device_id);
    
    /**
     * @brief 워커 중지
     */
    bool StopWorker(const std::string& device_id);
    
    /**
     * @brief 워커 일시정지
     */
    bool PauseWorker(const std::string& device_id);
    
    /**
     * @brief 워커 재개
     */
    bool ResumeWorker(const std::string& device_id);
    
    /**
     * @brief 워커 재시작 (중지 후 다시 시작)
     */
    bool RestartWorker(const std::string& device_id);

    /**
     * @brief 모든 활성 워커 시작
     */
    int StartAllActiveWorkers();
    
    /**
     * @brief 모든 워커 중지
     */
    void StopAllWorkers();

    // ==========================================================================
    // 디바이스 제어 (REST API에서 호출되는 메서드들)
    // ==========================================================================
    
    /**
     * @brief 디지털 출력 제어
     */
    bool ControlDigitalOutput(const std::string& device_id, 
                             const std::string& output_id, 
                             bool enable);
    
    /**
     * @brief 아날로그 출력 제어
     */
    bool ControlAnalogOutput(const std::string& device_id, 
                           const std::string& output_id, 
                           double value);
    
    /**
     * @brief 파라미터 변경
     */
    bool ChangeParameter(const std::string& device_id, 
                        const std::string& parameter_id, 
                        double value);
    
    /**
     * @brief 데이터포인트 쓰기
     */
    bool WriteDataPoint(const std::string& device_id, 
                       const std::string& point_id, 
                       const std::string& value);

    // ==========================================================================
    // 상태 조회 (REST API에서 호출되는 메서드들)
    // ==========================================================================
    
    /**
     * @brief 특정 워커 상태 조회
     */
    nlohmann::json GetWorkerStatus(const std::string& device_id) const;
    
    /**
     * @brief 모든 워커 목록 조회
     */
    nlohmann::json GetWorkerList() const;
    
    /**
     * @brief 매니저 통계 조회
     */
    nlohmann::json GetManagerStats() const;
    
    /**
     * @brief 활성 워커 개수
     */
    size_t GetActiveWorkerCount() const;
    
    /**
     * @brief 워커 존재 여부 확인
     */
    bool HasWorker(const std::string& device_id) const;
    
    /**
     * @brief 워커 실행 상태 확인
     */
    bool IsWorkerRunning(const std::string& device_id) const;

    // ==========================================================================
    // 워커 등록 관리 (내부 사용 + WorkerFactory 호환성)
    // ==========================================================================
    
    /**
     * @brief 워커 수동 등록 (기존 WorkerFactory와 호환성 유지)
     * @deprecated 사용하지 말 것 - 내부에서만 사용
     */
    void RegisterWorker(const std::string& device_id, 
                       std::shared_ptr<BaseDeviceWorker> worker);
    
    /**
     * @brief 워커 등록 해제
     */
    void UnregisterWorker(const std::string& device_id);

private:
    // ==========================================================================
    // 생성자/소멸자 (싱글톤)
    // ==========================================================================
    WorkerManager() = default;
    ~WorkerManager();

    // ==========================================================================
    // 자동 초기화 (기존 패턴 준수)
    // ==========================================================================
    void ensureInitialized();
    bool doInitialize();
    void Shutdown();

    // ==========================================================================
    // 내부 헬퍼 메서드들
    // ==========================================================================
    
    /**
     * @brief 워커 찾기 (내부 사용)
     */
    std::shared_ptr<BaseDeviceWorker> FindWorker(const std::string& device_id) const;
    
    /**
     * @brief 새 워커 생성 및 등록 (WorkerFactory 사용)
     */
    std::shared_ptr<BaseDeviceWorker> CreateAndRegisterWorker(const std::string& device_id);
    
    /**
     * @brief 활성 디바이스 ID 목록 로드
     */
    std::vector<std::string> LoadActiveDeviceIds();

private:
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // 초기화 상태
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutting_down_{false};
    mutable std::mutex init_mutex_;
    
    // 워커 관리 (핵심 - 단일 소유권)
    std::unordered_map<std::string, std::shared_ptr<BaseDeviceWorker>> active_workers_;
    mutable std::mutex workers_mutex_;
    
    // 통계
    std::atomic<uint64_t> total_started_{0};
    std::atomic<uint64_t> total_stopped_{0};
    std::atomic<uint64_t> total_control_commands_{0};
    std::atomic<uint64_t> total_errors_{0};
};

} // namespace Workers
} // namespace PulseOne

#endif // WORKER_MANAGER_H