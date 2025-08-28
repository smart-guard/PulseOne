// =============================================================================
// collector/include/Workers/WorkerManager.h - 간단한 싱글톤 Worker 관리자
// =============================================================================
#pragma once

#ifndef WORKER_MANAGER_H
#define WORKER_MANAGER_H

#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>

namespace PulseOne::Workers {

class BaseDeviceWorker;
class WorkerFactory;

/**
 * @brief 간단한 Worker 관리자 (싱글톤)
 * 
 * 핵심 기능:
 * - 활성 Worker들의 생명주기 관리
 * - REST API에서 호출되는 메서드들
 * - Worker 상태 조회 및 제어
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
    // Worker 생명주기 관리 (REST API에서 호출)
    // ==========================================================================
    
    /**
     * @brief Worker 시작
     */
    bool StartWorker(const std::string& device_id);
    
    /**
     * @brief Worker 중지
     */
    bool StopWorker(const std::string& device_id);
    
    /**
     * @brief Worker 재시작
     */
    bool RestartWorker(const std::string& device_id);
    
    /**
     * @brief Worker 일시정지
     */
    bool PauseWorker(const std::string& device_id);
    
    /**
     * @brief Worker 재개
     */
    bool ResumeWorker(const std::string& device_id);
    
    /**
     * @brief Worker 설정 리로드
     */
    bool ReloadWorker(const std::string& device_id);
    
    // ==========================================================================
    // 대량 작업
    // ==========================================================================
    
    /**
     * @brief 활성화된 모든 Worker 시작
     */
    int StartAllActiveWorkers();
    
    /**
     * @brief 모든 Worker 중지
     */
    void StopAllWorkers();
    
    // ==========================================================================
    // 디바이스 제어 (REST API에서 호출)
    // ==========================================================================
    
    /**
     * @brief 데이터 포인트 쓰기
     */
    bool WriteDataPoint(const std::string& device_id, const std::string& point_id, const std::string& value);
    
    /**
     * @brief 출력 제어 (펌프/밸브)
     */
    bool ControlOutput(const std::string& device_id, const std::string& output_id, bool enable);
    
    // ==========================================================================
    // 상태 조회 (REST API에서 호출)
    // ==========================================================================
    
    /**
     * @brief 특정 Worker 상태 조회
     */
    nlohmann::json GetWorkerStatus(const std::string& device_id) const;
    
    /**
     * @brief 모든 Worker 목록 조회
     */
    nlohmann::json GetWorkerList() const;
    
    /**
     * @brief 매니저 통계 조회
     */
    nlohmann::json GetManagerStats() const;
    
    /**
     * @brief 활성 Worker 개수
     */
    size_t GetActiveWorkerCount() const;
    
    /**
     * @brief Worker 존재 여부 확인
     */
    bool HasWorker(const std::string& device_id) const;
    
    /**
     * @brief Worker 실행 상태 확인
     */
    bool IsWorkerRunning(const std::string& device_id) const;

private:
    // ==========================================================================
    // 생성자/소멸자 (싱글톤)
    // ==========================================================================
    WorkerManager() = default;
    ~WorkerManager();

    // ==========================================================================
    // 내부 헬퍼 메서드들
    // ==========================================================================
    
    /**
     * @brief Worker 찾기
     */
    std::shared_ptr<BaseDeviceWorker> FindWorker(const std::string& device_id) const;
    
    /**
     * @brief 새 Worker 생성 및 등록
     */
    std::shared_ptr<BaseDeviceWorker> CreateAndRegisterWorker(const std::string& device_id);
    
    /**
     * @brief Worker 등록
     */
    void RegisterWorker(const std::string& device_id, std::shared_ptr<BaseDeviceWorker> worker);
    
    /**
     * @brief Worker 등록 해제
     */
    void UnregisterWorker(const std::string& device_id);

private:
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // Worker 관리 (핵심)
    mutable std::mutex workers_mutex_;
    std::unordered_map<std::string, std::shared_ptr<BaseDeviceWorker>> workers_;
    
    // WorkerFactory (Worker 생성용)
    std::unique_ptr<WorkerFactory> worker_factory_;
    
    // 통계
    std::atomic<int> total_started_{0};
    std::atomic<int> total_stopped_{0};
    std::atomic<int> total_errors_{0};
};

} // namespace PulseOne::Workers

#endif // WORKER_MANAGER_H