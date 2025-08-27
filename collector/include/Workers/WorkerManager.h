// =============================================================================
// collector/include/Workers/WorkerManager.h
// 워커 생명주기 관리 및 제어를 담당하는 필수 컴포넌트
// =============================================================================

#ifndef WORKER_MANAGER_H
#define WORKER_MANAGER_H

#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>

// 전방 선언
class LogManager;
class ConfigManager;

namespace PulseOne {
namespace Workers {
    class WorkerFactory;
    class BaseDeviceWorker;
}
namespace Database {
    class RepositoryFactory;
    namespace Repositories {
        class DeviceRepository;
    }
}
}

namespace PulseOne {
namespace Workers {

/**
 * @brief 워커 관리자 - 모든 활성 워커의 생명주기를 관리
 * 
 * 이 클래스가 없으면 REST API에서 디바이스를 제어할 수 없습니다.
 * WorkerFactory는 생성만 하고, 실제 관리는 이 클래스가 담당합니다.
 */
class WorkerManager {
public:
    // ==========================================================================
    // 싱글턴 패턴
    // ==========================================================================
    static WorkerManager& getInstance();
    
    // 복사/이동 방지
    WorkerManager(const WorkerManager&) = delete;
    WorkerManager& operator=(const WorkerManager&) = delete;

    // ==========================================================================
    // 초기화 및 종료
    // ==========================================================================
    
    /**
     * @brief 워커 매니저 초기화
     * @param worker_factory WorkerFactory 인스턴스 (nullptr시 자동으로 싱글턴 사용)
     * @param logger LogManager 인스턴스 (nullptr시 자동으로 싱글턴 사용)
     * @return 성공시 true
     */
    bool Initialize(WorkerFactory* worker_factory = nullptr);

private:
    // ==========================================================================
    // 자동 초기화 관련 (프로젝트 패턴 준수)
    // ==========================================================================
    
    /**
     * @brief 자동 초기화 보장 (thread-safe)
     */
    void ensureInitialized();
    
    /**
     * @brief 실제 초기화 로직
     */
    bool doInitialize();

    WorkerManager() = default;
    ~WorkerManager();
    
    /**
     * @brief 모든 워커 정리 및 종료
     */
    void Shutdown();

    // ==========================================================================
    // 워커 생명주기 제어 - REST API에서 호출됨
    // ==========================================================================
    
    /**
     * @brief 디바이스 워커 시작
     * @param device_id 디바이스 ID (문자열)
     * @return 성공시 true
     */
    bool StartDeviceWorker(const std::string& device_id);
    
    /**
     * @brief 디바이스 워커 중지
     * @param device_id 디바이스 ID
     * @return 성공시 true
     */
    bool StopDeviceWorker(const std::string& device_id);
    
    /**
     * @brief 디바이스 워커 일시정지
     * @param device_id 디바이스 ID
     * @return 성공시 true
     */
    bool PauseDeviceWorker(const std::string& device_id);
    
    /**
     * @brief 디바이스 워커 재개
     * @param device_id 디바이스 ID
     * @return 성공시 true
     */
    bool ResumeDeviceWorker(const std::string& device_id);
    
    /**
     * @brief 디바이스 워커 재시작
     * @param device_id 디바이스 ID
     * @return 성공시 true
     */
    bool RestartDeviceWorker(const std::string& device_id);

    // ==========================================================================
    // 하드웨어 제어 - REST API에서 호출됨
    // ==========================================================================
    
    /**
     * @brief 펌프 제어
     * @param device_id 디바이스 ID
     * @param pump_id 펌프 ID
     * @param enable true=시작, false=정지
     * @return 성공시 true
     */
    bool ControlPump(const std::string& device_id, const std::string& pump_id, bool enable);
    
    /**
     * @brief 밸브 제어
     * @param device_id 디바이스 ID
     * @param valve_id 밸브 ID
     * @param open true=열기, false=닫기
     * @return 성공시 true
     */
    bool ControlValve(const std::string& device_id, const std::string& valve_id, bool open);
    
    /**
     * @brief 설정값 변경
     * @param device_id 디바이스 ID
     * @param setpoint_id 설정값 ID
     * @param value 새로운 값
     * @return 성공시 true
     */
    bool ChangeSetpoint(const std::string& device_id, const std::string& setpoint_id, double value);

    // ==========================================================================
    // 상태 조회 - REST API에서 호출됨
    // ==========================================================================
    
    /**
     * @brief 모든 디바이스 목록 조회
     * @return JSON 배열
     */
    nlohmann::json GetDeviceList();
    
    /**
     * @brief 특정 디바이스 상태 조회
     * @param device_id 디바이스 ID
     * @return JSON 객체
     */
    nlohmann::json GetDeviceStatus(const std::string& device_id);
    
    /**
     * @brief 시스템 통계 조회
     * @return JSON 객체
     */
    nlohmann::json GetSystemStats();

    // ==========================================================================
    // 워커 관리 헬퍼 함수들
    // ==========================================================================
    
    /**
     * @brief 활성 워커 개수 조회
     * @return 워커 개수
     */
    size_t GetActiveWorkerCount() const;
    
    /**
     * @brief 특정 워커가 활성 상태인지 확인
     * @param device_id 디바이스 ID
     * @return 활성 상태면 true
     */
    bool IsWorkerActive(const std::string& device_id) const;

private:
    // ==========================================================================
    // 내부 헬퍼 함수들
    // ==========================================================================
    
    /**
     * @brief 디바이스 ID로 워커 찾기
     * @param device_id 디바이스 ID
     * @return 워커 포인터 (없으면 nullptr)
     */
    std::shared_ptr<BaseDeviceWorker> FindWorker(const std::string& device_id);
    
    /**
     * @brief 새 워커 생성 및 등록
     * @param device_id 디바이스 ID
     * @return 생성된 워커 포인터 (실패시 nullptr)
     */
    std::shared_ptr<BaseDeviceWorker> CreateAndRegisterWorker(const std::string& device_id);
    
    /**
     * @brief 워커 등록 해제
     * @param device_id 디바이스 ID
     */
    void UnregisterWorker(const std::string& device_id);

private:
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // 초기화 상태 및 동기화
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutting_down_{false};
    mutable std::mutex init_mutex_;
    
    // 활성 워커 관리 - 핵심!
    std::unordered_map<std::string, std::shared_ptr<BaseDeviceWorker>> active_workers_;
    mutable std::mutex workers_mutex_;
    
    // 통계
    std::atomic<size_t> total_started_{0};
    std::atomic<size_t> total_stopped_{0};
    std::atomic<size_t> total_failed_{0};
};

} // namespace Workers
} // namespace PulseOne

#endif // WORKER_MANAGER_H