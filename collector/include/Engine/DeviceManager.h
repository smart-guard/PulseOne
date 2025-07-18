// collector/include/Engine/DeviceManager.h
// 전체 시스템의 디바이스들을 관리하는 매니저 클래스
#pragma once

#include <memory>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <shared_mutex>
#include <chrono>

#include "Engine/DeviceWorker.h"
#include "Engine/DeviceControlHandler.h"
#include "Engine/VirtualPointEngine.h"
#include "Engine/AlarmEngine.h"
#include "Drivers/CommonTypes.h"
#include "Database/DatabaseManager.h"
#include "RedisClient.h"
#include "Utils/LogManager.h"

namespace PulseOne {
namespace Engine {

/**
 * @brief 시스템 전체 통계 구조체
 */
struct SystemStatistics {
    uint32_t total_devices = 0;            ///< 총 디바이스 수
    uint32_t connected_devices = 0;        ///< 연결된 디바이스 수
    uint32_t running_devices = 0;          ///< 실행 중인 디바이스 수
    uint32_t paused_devices = 0;           ///< 일시정지된 디바이스 수
    uint32_t error_devices = 0;            ///< 에러 상태 디바이스 수
    
    uint64_t total_read_count = 0;         ///< 총 읽기 횟수
    uint64_t total_write_count = 0;        ///< 총 쓰기 횟수
    uint64_t total_error_count = 0;        ///< 총 에러 횟수
    
    double overall_success_rate = 0.0;     ///< 전체 성공률
    uint32_t avg_response_time_ms = 0;     ///< 평균 응답 시간
    
    Timestamp last_statistics_time;        ///< 마지막 통계 업데이트 시간
    uint64_t uptime_seconds = 0;           ///< 시스템 가동 시간
    
    uint32_t memory_usage_mb = 0;          ///< 메모리 사용량 (MB)
    double cpu_usage_percent = 0.0;       ///< CPU 사용률
};

/**
 * @brief 디바이스 매니저 설정 구조체
 */
struct DeviceManagerConfig {
    int max_concurrent_devices = 100;           ///< 최대 동시 디바이스 수
    int statistics_interval_seconds = 30;       ///< 통계 업데이트 간격
    int health_check_interval_seconds = 60;     ///< 상태 체크 간격
    int cleanup_interval_seconds = 300;         ///< 정리 작업 간격
    
    bool auto_restart_failed_devices = true;    ///< 실패한 디바이스 자동 재시작
    int max_restart_attempts = 3;               ///< 최대 재시작 시도 횟수
    int restart_delay_seconds = 30;             ///< 재시작 지연 시간
    
    bool enable_performance_monitoring = true;  ///< 성능 모니터링 활성화
    bool enable_memory_monitoring = true;       ///< 메모리 모니터링 활성화
    
    std::string redis_status_prefix = "pulseone:device_status:";  ///< Redis 상태 키 접두사
    std::string redis_command_prefix = "pulseone:device_command:"; ///< Redis 명령 키 접두사
};

/**
 * @brief 전체 시스템의 디바이스들을 관리하는 매니저 클래스
 * @details 모든 DeviceWorker 인스턴스를 생성, 관리, 제어하는 중앙 관리자
 * 
 * 주요 기능:
 * - 디바이스 워커 생명주기 관리
 * - 웹에서 오는 제어 명령 처리
 * - 시스템 전체 모니터링 및 통계
 * - 장애 감지 및 자동 복구
 * - 확장성 관리 (동적 디바이스 추가/제거)
 */
class DeviceManager {
private:
    // 디바이스 워커들
    std::unordered_map<UUID, std::unique_ptr<DeviceWorker>> device_workers_;
    mutable std::shared_mutex workers_mutex_;
    
    // 재시작 시도 추적
    std::unordered_map<UUID, int> restart_attempts_;
    std::unordered_map<UUID, Timestamp> last_restart_time_;
    mutable std::mutex restart_tracking_mutex_;
    
    // 스레드 관리
    std::atomic<bool> running_{false};
    std::thread manager_thread_;
    std::thread statistics_thread_;
    std::thread health_monitor_thread_;
    std::thread cleanup_thread_;
    
    // 하위 엔진들
    std::unique_ptr<DeviceControlHandler> control_handler_;
    std::unique_ptr<VirtualPointEngine> virtual_point_engine_;
    std::unique_ptr<AlarmEngine> alarm_engine_;
    
    // 외부 의존성
    std::shared_ptr<DatabaseManager> db_manager_;
    std::shared_ptr<RedisClient> redis_client_;
    LogManager& logger_;
    
    // 설정
    DeviceManagerConfig config_;
    
    // 통계 및 모니터링
    SystemStatistics system_stats_;
    mutable std::mutex stats_mutex_;
    Timestamp start_time_;
    std::atomic<uint64_t> total_read_count_{0};
    std::atomic<uint64_t> total_write_count_{0};
    std::atomic<uint64_t> total_error_count_{0};

public:
    /**
     * @brief 디바이스 매니저 생성자
     * @param config 매니저 설정
     */
    explicit DeviceManager(const DeviceManagerConfig& config = DeviceManagerConfig{});
    
    ~DeviceManager();
    
    // 복사/이동 생성자 비활성화
    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;
    DeviceManager(DeviceManager&&) = delete;
    DeviceManager& operator=(DeviceManager&&) = delete;
    
    // =================================================================
    // 매니저 생명주기 관리
    // =================================================================
    
    /**
     * @brief 매니저 초기화
     * @return 성공 시 true
     */
    bool Initialize();
    
    /**
     * @brief 매니저 시작
     * @return 성공 시 true
     */
    bool Start();
    
    /**
     * @brief 매니저 중지
     */
    void Stop();
    
    /**
     * @brief 실행 중 여부 확인
     */
    bool IsRunning() const { return running_.load(); }
    
    // =================================================================
    // 디바이스 관리 (CRUD)
    // =================================================================
    
    /**
     * @brief 데이터베이스에서 모든 디바이스 로드
     * @return 로드된 디바이스 수
     */
    int LoadAllDevicesFromDatabase();
    
    /**
     * @brief 특정 프로젝트의 디바이스들만 로드
     * @param project_id 프로젝트 ID
     * @return 로드된 디바이스 수
     */
    int LoadDevicesFromProject(const UUID& project_id);
    
    /**
     * @brief 특정 디바이스 추가
     * @param device_info 디바이스 정보
     * @param data_points 데이터 포인트 목록
     * @return 성공 시 true
     */
    bool AddDevice(const DeviceInfo& device_info, const std::vector<DataPoint>& data_points);
    
    /**
     * @brief 특정 디바이스 제거
     * @param device_id 디바이스 ID
     * @param force_remove 강제 제거 여부
     * @return 성공 시 true
     */
    bool RemoveDevice(const UUID& device_id, bool force_remove = false);
    
    /**
     * @brief 디바이스 정보 업데이트
     * @param device_id 디바이스 ID
     * @param new_device_info 새로운 디바이스 정보
     * @return 성공 시 true
     */
    bool UpdateDevice(const UUID& device_id, const DeviceInfo& new_device_info);
    
    /**
     * @brief 디바이스 데이터 포인트 업데이트
     * @param device_id 디바이스 ID
     * @param new_data_points 새로운 데이터 포인트들
     * @return 성공 시 true
     */
    bool UpdateDeviceDataPoints(const UUID& device_id, const std::vector<DataPoint>& new_data_points);
    
    // =================================================================
    // 디바이스 제어 (웹에서 호출)
    // =================================================================
    
    /**
     * @brief 특정 디바이스 시작
     * @param device_id 디바이스 ID
     * @return 성공 시 true
     */
    bool StartDevice(const UUID& device_id);
    
    /**
     * @brief 특정 디바이스 중지
     * @param device_id 디바이스 ID
     * @return 성공 시 true
     */
    bool StopDevice(const UUID& device_id);
    
    /**
     * @brief 특정 디바이스 일시정지
     * @param device_id 디바이스 ID
     * @return 성공 시 true
     */
    bool PauseDevice(const UUID& device_id);
    
    /**
     * @brief 특정 디바이스 재개
     * @param device_id 디바이스 ID
     * @return 성공 시 true
     */
    bool ResumeDevice(const UUID& device_id);
    
    /**
     * @brief 특정 디바이스 재시작
     * @param device_id 디바이스 ID
     * @return 성공 시 true
     */
    bool RestartDevice(const UUID& device_id);
    
    /**
     * @brief 모든 디바이스 시작
     * @return 시작된 디바이스 수
     */
    int StartAllDevices();
    
    /**
     * @brief 모든 디바이스 중지
     * @return 중지된 디바이스 수
     */
    int StopAllDevices();
    
    /**
     * @brief 특정 프로젝트의 모든 디바이스 제어
     * @param project_id 프로젝트 ID
     * @param command 명령 (start, stop, pause, resume)
     * @return 처리된 디바이스 수
     */
    int ControlProjectDevices(const UUID& project_id, const std::string& command);
    
    // =================================================================
    // 데이터 처리
    // =================================================================
    
    /**
     * @brief 디바이스에 쓰기 요청
     * @param device_id 디바이스 ID
     * @param request 쓰기 요청
     * @return 성공 시 true
     */
    bool WriteToDevice(const UUID& device_id, const WriteRequest& request);
    
    /**
     * @brief 디바이스에서 즉시 데이터 읽기
     * @param device_id 디바이스 ID
     * @return 읽은 데이터 값들
     */
    std::vector<DataValue> ReadFromDevice(const UUID& device_id);
    
    /**
     * @brief 특정 포인트에 즉시 쓰기
     * @param device_id 디바이스 ID
     * @param point_id 포인트 ID
     * @param value 쓸 값
     * @return 성공 시 true
     */
    bool WriteToPoint(const UUID& device_id, const UUID& point_id, double value);
    
    // =================================================================
    // 상태 조회 및 모니터링
    // =================================================================
    
    /**
     * @brief 모든 디바이스 상태 조회
     * @return 디바이스 상태 목록
     */
    std::vector<DeviceStatus> GetAllDeviceStatus() const;
    
    /**
     * @brief 특정 디바이스 상태 조회
     * @param device_id 디바이스 ID
     * @return 디바이스 상태 (존재하지 않으면 빈 옵션)
     */
    std::optional<DeviceStatus> GetDeviceStatus(const UUID& device_id) const;
    
    /**
     * @brief 시스템 전체 통계 조회
     * @return 시스템 통계
     */
    SystemStatistics GetSystemStatistics() const;
    
    /**
     * @brief 특정 디바이스 통계 조회
     * @param device_id 디바이스 ID
     * @return 디바이스 통계 (존재하지 않으면 빈 옵션)
     */
    std::optional<DeviceStatistics> GetDeviceStatistics(const UUID& device_id) const;
    
    /**
     * @brief 특정 프로젝트의 통계 조회
     * @param project_id 프로젝트 ID
     * @return 프로젝트 통계
     */
    SystemStatistics GetProjectStatistics(const UUID& project_id) const;
    
    /**
     * @brief 활성 디바이스 목록 조회
     * @return 활성 디바이스 ID 목록
     */
    std::vector<UUID> GetActiveDeviceIds() const;
    
    /**
     * @brief 에러 상태 디바이스 목록 조회
     * @return 에러 상태 디바이스 ID 목록
     */
    std::vector<UUID> GetErrorDeviceIds() const;
    
    /**
     * @brief 성능 메트릭 조회 (웹 모니터링용)
     * @return JSON 형태의 성능 데이터
     */
    nlohmann::json GetPerformanceMetrics() const;
    
    // =================================================================
    // 설정 및 구성
    // =================================================================
    
    /**
     * @brief 매니저 설정 업데이트
     * @param new_config 새로운 설정
     */
    void UpdateConfig(const DeviceManagerConfig& new_config);
    
    /**
     * @brief 현재 설정 조회
     * @return 현재 설정
     */
    DeviceManagerConfig GetConfig() const { return config_; }
    
    /**
     * @brief 가상 포인트 엔진 설정 업데이트
     * @return 성공 시 true
     */
    bool UpdateVirtualPointConfiguration();
    
    /**
     * @brief 알람 엔진 설정 업데이트
     * @return 성공 시 true
     */
    bool UpdateAlarmConfiguration();
    
    // =================================================================
    // 고급 기능
    // =================================================================
    
    /**
     * @brief 디바이스 그룹별 일괄 제어
     * @param group_id 디바이스 그룹 ID
     * @param command 명령
     * @return 처리된 디바이스 수
     */
    int ControlDeviceGroup(const UUID& group_id, const std::string& command);
    
    /**
     * @brief 조건부 디바이스 제어
     * @param condition 조건 함수
     * @param command 명령
     * @return 처리된 디바이스 수
     */
    int ControlDevicesWhere(std::function<bool(const DeviceStatus&)> condition, 
                           const std::string& command);
    
    /**
     * @brief 시스템 자원 최적화
     * @return 최적화 결과 메시지
     */
    std::string OptimizeSystemResources();
    
    /**
     * @brief 백업 및 복원
     * @param backup_path 백업 파일 경로
     * @return 성공 시 true
     */
    bool BackupConfiguration(const std::string& backup_path);
    bool RestoreConfiguration(const std::string& backup_path);
    
    // =================================================================
    // 이벤트 및 콜백
    // =================================================================
    
    /**
     * @brief 디바이스 이벤트 리스너 등록
     * @param listener 이벤트 리스너
     */
    void RegisterEventListener(std::shared_ptr<IDeviceWorkerEventListener> listener);
    
    /**
     * @brief 디바이스 이벤트 리스너 제거
     * @param listener 제거할 리스너
     */
    void UnregisterEventListener(std::shared_ptr<IDeviceWorkerEventListener> listener);

private:
    // =================================================================
    // 스레드 함수들
    // =================================================================
    
    /**
     * @brief 매니저 메인 스레드 함수
     */
    void ManagerThreadFunction();
    
    /**
     * @brief 통계 수집 스레드 함수
     */
    void StatisticsThreadFunction();
    
    /**
     * @brief 상태 모니터링 스레드 함수
     */
    void HealthMonitorThreadFunction();
    
    /**
     * @brief 정리 작업 스레드 함수
     */
    void CleanupThreadFunction();
    
    // =================================================================
    // 내부 로직 메서드들
    // =================================================================
    
    /**
     * @brief 데이터베이스에서 디바이스 정보 조회
     * @param where_clause WHERE 절 (선택사항)
     * @return 디바이스 정보 목록
     */
    std::vector<DeviceInfo> QueryDevicesFromDB(const std::string& where_clause = "");
    
    /**
     * @brief 데이터베이스에서 데이터 포인트 조회
     * @param device_id 디바이스 ID
     * @return 데이터 포인트 목록
     */
    std::vector<DataPoint> QueryDataPointsFromDB(const UUID& device_id);
    
    /**
     * @brief 디바이스 워커 생성 및 시작
     * @param device_info 디바이스 정보
     * @param data_points 데이터 포인트들
     * @return 성공 시 true
     */
    bool CreateAndStartWorker(const DeviceInfo& device_info, 
                             const std::vector<DataPoint>& data_points);
    
    /**
     * @brief 비활성 디바이스 정리
     */
    void CleanupInactiveDevices();
    
    /**
     * @brief 디바이스 상태 모니터링
     */
    void MonitorDeviceHealth();
    
    /**
     * @brief 통계 정보 업데이트
     */
    void UpdateStatistics();
    
    /**
     * @brief 시스템 리소스 모니터링
     */
    void MonitorSystemResources();
    
    /**
     * @brief 실패한 디바이스 자동 재시작
     */
    void AutoRestartFailedDevices();
    
    /**
     * @brief Redis에 시스템 상태 발행
     */
    void PublishSystemStatus();
    
    /**
     * @brief 디바이스 재시작 시도 추적
     * @param device_id 디바이스 ID
     * @return 재시작 가능 여부
     */
    bool CanAttemptRestart(const UUID& device_id);
    
    /**
     * @brief 재시작 시도 기록
     * @param device_id 디바이스 ID
     */
    void RecordRestartAttempt(const UUID& device_id);
    
    /**
     * @brief 재시작 기록 초기화
     * @param device_id 디바이스 ID
     */
    void ResetRestartAttempts(const UUID& device_id);
    
    /**
     * @brief 디바이스 존재 여부 확인
     * @param device_id 디바이스 ID
     * @return 존재하면 true
     */
    bool DeviceExists(const UUID& device_id) const;
    
    /**
     * @brief 스레드 안전한 워커 검색
     * @param device_id 디바이스 ID
     * @return 워커 포인터 (존재하지 않으면 nullptr)
     */
    DeviceWorker* FindWorker(const UUID& device_id) const;
    
    /**
     * @brief 안전한 스레드 종료
     * @param thread 종료할 스레드
     * @param thread_name 스레드 이름
     */
    void SafeStopThread(std::thread& thread, const std::string& thread_name);
    
    // =================================================================
    // 이벤트 처리
    // =================================================================
    std::vector<std::weak_ptr<IDeviceWorkerEventListener>> event_listeners_;
    mutable std::mutex listeners_mutex_;
    
    void NotifyDeviceStateChanged(const UUID& device_id, DeviceWorkerState old_state, DeviceWorkerState new_state);
    void NotifyDeviceError(const UUID& device_id, const std::string& error_message, ErrorSeverity severity);
};

/**
 * @brief 디바이스 매니저 싱글턴
 * @details 전역에서 하나의 DeviceManager 인스턴스에 접근
 */
class DeviceManagerSingleton {
private:
    static std::unique_ptr<DeviceManager> instance_;
    static std::mutex instance_mutex_;

public:
    /**
     * @brief 싱글턴 인스턴스 생성
     * @param config 매니저 설정
     * @return 생성된 인스턴스
     */
    static DeviceManager& CreateInstance(const DeviceManagerConfig& config = DeviceManagerConfig{});
    
    /**
     * @brief 싱글턴 인스턴스 조회
     * @return 인스턴스 참조
     * @throws std::runtime_error 인스턴스가 생성되지 않은 경우
     */
    static DeviceManager& GetInstance();
    
    /**
     * @brief 싱글턴 인스턴스 해제
     */
    static void DestroyInstance();
    
    /**
     * @brief 인스턴스 존재 여부 확인
     */
    static bool HasInstance();
};

} // namespace Engine
} // namespace PulseOne