/**
 * @file DeviceWorker.h
 * @brief 개별 디바이스를 담당하는 워커 클래스 정의
 * @details 멀티스레드 동작으로 읽기/쓰기/제어를 분리하여 처리하며 웹에서 완전 제어 가능
 * @author PulseOne Development Team
 * @date 2025-01-17
 * @version 2.0.0
 */

#ifndef ENGINE_DEVICE_WORKER_H
#define ENGINE_DEVICE_WORKER_H

#include "Drivers/IProtocolDriver.h"
#include "Drivers/CommonTypes.h"
#include "Utils/LogManager.h"
#include "Config/ConfigManager.h"
#include "Client/RedisClient.h"
#include "InfluxClient.h"
#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <future>
#include <map>
#include <functional>

namespace PulseOne {
namespace Engine {

// =============================================================================
// 전방 선언 및 타입 정의
// =============================================================================

class MaintenanceManager;
class AlarmContextEngine;

/**
 * @brief 워커 상태 열거형
 */
enum class WorkerState {
    STOPPED = 0,        ///< 정지됨
    STARTING = 1,       ///< 시작 중
    RUNNING = 2,        ///< 실행 중
    PAUSED = 3,         ///< 일시정지됨
    STOPPING = 4,       ///< 정지 중
    ERROR = 5           ///< 오류 상태
};

/**
 * @brief 워커 제어 명령
 */
enum class WorkerCommand {
    START = 0,          ///< 시작
    STOP = 1,           ///< 정지
    PAUSE = 2,          ///< 일시정지
    RESUME = 3,         ///< 재개
    RESTART = 4,        ///< 재시작
    SHUTDOWN = 5        ///< 완전 종료
};

/**
 * @brief 하드웨어 제어 명령
 */
struct HardwareControlRequest {
    UUID point_id;                              ///< 대상 포인트 ID
    Drivers::DataValue value;                   ///< 제어값
    std::string command_type;                   ///< 명령 타입 ("pump_on", "valve_open" 등)
    std::map<std::string, std::string> params; ///< 추가 매개변수
    std::chrono::system_clock::time_point timeout; ///< 타임아웃 시간
    std::promise<bool> result_promise;          ///< 결과 반환용
};

/**
 * @brief 가상 포인트 계산 규칙
 */
struct VirtualPointRule {
    UUID virtual_point_id;                      ///< 가상 포인트 ID
    std::string formula;                        ///< 계산 수식 ("temp + pressure * 0.1")
    std::vector<UUID> input_point_ids;          ///< 입력 포인트들
    std::chrono::milliseconds calc_interval;    ///< 계산 주기
    bool is_enabled;                            ///< 활성화 여부
};

/**
 * @brief 알람 규칙
 */
struct AlarmRule {
    UUID alarm_id;                              ///< 알람 고유 ID
    UUID point_id;                              ///< 대상 포인트 ID
    std::string condition;                      ///< 조건 ("value > 80", "delta > 5")
    double threshold;                           ///< 임계값
    std::string severity;                       ///< 심각도 ("critical", "warning", "info")
    std::chrono::seconds delay;                 ///< 지연 시간 (채터링 방지)
    bool is_enabled;                            ///< 활성화 여부
    bool maintenance_suppress;                  ///< 점검 모드에서 억제 여부
};

/**
 * @brief 워커 통계 정보
 */
struct WorkerStatistics {
    std::atomic<uint64_t> total_reads{0};           ///< 총 읽기 횟수
    std::atomic<uint64_t> successful_reads{0};      ///< 성공한 읽기 횟수
    std::atomic<uint64_t> total_writes{0};          ///< 총 쓰기 횟수
    std::atomic<uint64_t> successful_writes{0};     ///< 성공한 쓰기 횟수
    std::atomic<uint64_t> hardware_commands{0};     ///< 하드웨어 제어 횟수
    std::atomic<uint64_t> alarms_triggered{0};      ///< 발생한 알람 수
    std::atomic<uint64_t> virtual_calculations{0};  ///< 가상 포인트 계산 횟수
    std::atomic<uint32_t> avg_response_time_ms{0};  ///< 평균 응답 시간
    std::chrono::system_clock::time_point start_time;     ///< 시작 시간
    std::chrono::system_clock::time_point last_read_time; ///< 마지막 읽기 시간
};

// =============================================================================
// DeviceWorker 클래스 정의
// =============================================================================

/**
 * @brief 개별 디바이스 워커 클래스
 * @details 하나의 물리적 디바이스를 담당하며 멀티스레드로 안전하게 동작
 * 
 * 주요 특징:
 * - 웹에서 완전 제어 가능 (시작/정지/일시정지/재시작)
 * - 3개 워커 스레드 (읽기/쓰기/하드웨어제어) 분리 운영
 * - Redis 상태 발행으로 실시간 모니터링
 * - 자동 재연결 및 장애 복구
 * - 가상 포인트 계산 및 알람 처리
 * - 점검 모드 지원 (알람 억제)
 */
class DeviceWorker {
public:
    // =============================================================================
    // 생성자 및 소멸자
    // =============================================================================
    
    /**
     * @brief 생성자
     * @param device_info 디바이스 정보
     * @param driver 프로토콜 드라이버
     * @param redis_client Redis 클라이언트
     * @param influx_client InfluxDB 클라이언트
     * @param logger 로거
     */
    DeviceWorker(const PulseOne::DeviceInfo& device_info,
                 std::unique_ptr<Drivers::IProtocolDriver> driver,
                 std::shared_ptr<RedisClient> redis_client,
                 std::shared_ptr<InfluxClient> influx_client,
                 std::shared_ptr<LogManager> logger);
    
    /**
     * @brief 소멸자
     */
    ~DeviceWorker();
    
    // 복사/이동 방지
    DeviceWorker(const DeviceWorker&) = delete;
    DeviceWorker& operator=(const DeviceWorker&) = delete;
    DeviceWorker(DeviceWorker&&) = delete;
    DeviceWorker& operator=(DeviceWorker&&) = delete;
    
    // =============================================================================
    // 기본 제어 인터페이스 (웹에서 호출)
    // =============================================================================
    
    /**
     * @brief 워커 시작 (비동기)
     * @return Future<bool> 시작 결과
     */
    std::future<bool> Start();
    
    /**
     * @brief 워커 정지 (비동기)
     * @return Future<bool> 정지 결과
     */
    std::future<bool> Stop();
    
    /**
     * @brief 워커 일시정지
     * @return Future<bool> 일시정지 결과
     */
    std::future<bool> Pause();
    
    /**
     * @brief 워커 재개
     * @return Future<bool> 재개 결과
     */
    std::future<bool> Resume();
    
    /**
     * @brief 워커 재시작
     * @return Future<bool> 재시작 결과
     */
    std::future<bool> Restart();
    
    /**
     * @brief 워커 완전 종료 (리소스 해제)
     */
    void Shutdown();
    
    // =============================================================================
    // 하드웨어 제어 인터페이스
    // =============================================================================
    
    /**
     * @brief 하드웨어 제어 명령 실행
     * @param point_id 대상 포인트 ID
     * @param value 제어값
     * @param command_type 명령 타입
     * @param params 추가 매개변수
     * @param timeout_ms 타임아웃 (밀리초)
     * @return Future<bool> 제어 결과
     */
    std::future<bool> ExecuteHardwareControl(
        const UUID& point_id,
        const Drivers::DataValue& value,
        const std::string& command_type = "write",
        const std::map<std::string, std::string>& params = {},
        int timeout_ms = 5000);
    
    /**
     * @brief 펌프 제어
     * @param pump_id 펌프 ID
     * @param enable true=시작, false=정지
     * @return Future<bool> 제어 결과
     */
    std::future<bool> ControlPump(const UUID& pump_id, bool enable);
    
    /**
     * @brief 밸브 제어
     * @param valve_id 밸브 ID
     * @param position 밸브 위치 (0-100%)
     * @return Future<bool> 제어 결과
     */
    std::future<bool> ControlValve(const UUID& valve_id, double position);
    
    /**
     * @brief 설정값 변경
     * @param setpoint_id 설정값 ID
     * @param value 새로운 설정값
     * @return Future<bool> 변경 결과
     */
    std::future<bool> ChangeSetpoint(const UUID& setpoint_id, double value);
    
    // =============================================================================
    // 점검 모드 제어
    // =============================================================================
    
    /**
     * @brief 점검 모드 활성화
     * @param reason 점검 사유
     * @param estimated_duration 예상 점검 시간
     * @return 성공 시 true
     */
    bool EnableMaintenanceMode(const std::string& reason, 
                              std::chrono::minutes estimated_duration = std::chrono::minutes(60));
    
    /**
     * @brief 점검 모드 비활성화
     * @return 성공 시 true
     */
    bool DisableMaintenanceMode();
    
    /**
     * @brief 점검 모드 상태 확인
     * @return 점검 모드 여부
     */
    bool IsInMaintenanceMode() const;
    
    // =============================================================================
    // 설정 관리
    // =============================================================================
    
    /**
     * @brief 데이터 포인트 추가
     * @param point 데이터 포인트 정보
     * @return 성공 시 true
     */
    bool AddDataPoint(const PulseOne::DataPoint& point);
    
    /**
     * @brief 데이터 포인트 제거
     * @param point_id 포인트 ID
     * @return 성공 시 true
     */
    bool RemoveDataPoint(const UUID& point_id);
    
    /**
     * @brief 가상 포인트 규칙 추가
     * @param rule 가상 포인트 규칙
     * @return 성공 시 true
     */
    bool AddVirtualPointRule(const VirtualPointRule& rule);
    
    /**
     * @brief 알람 규칙 추가
     * @param rule 알람 규칙
     * @return 성공 시 true
     */
    bool AddAlarmRule(const AlarmRule& rule);
    
    /**
     * @brief 폴링 간격 변경
     * @param interval_ms 새로운 폴링 간격 (밀리초)
     */
    void SetPollingInterval(int interval_ms);
    
    // =============================================================================
    // 상태 조회
    // =============================================================================
    
    /**
     * @brief 현재 워커 상태
     * @return 워커 상태
     */
    WorkerState GetState() const { return current_state_.load(); }
    
    /**
     * @brief 디바이스 연결 상태
     * @return 연결 상태
     */
    Drivers::ConnectionStatus GetConnectionStatus() const;
    
    /**
     * @brief 워커 통계 정보
     * @return 통계 구조체
     */
    WorkerStatistics GetStatistics() const { return statistics_; }
    
    /**
     * @brief 디바이스 정보
     * @return 디바이스 정보 구조체
     */
    const PulseOne::DeviceInfo& GetDeviceInfo() const { return device_info_; }
    
    /**
     * @brief 현재 데이터 포인트 목록
     * @return 데이터 포인트들
     */
    std::vector<PulseOne::DataPoint> GetDataPoints() const;
    
    /**
     * @brief 상태 정보를 JSON으로 반환
     * @return JSON 형태의 상태 정보
     */
    std::string GetStatusJson() const;

private:
    // =============================================================================
    // 내부 데이터 멤버
    // =============================================================================
    
    // 기본 정보
    PulseOne::DeviceInfo device_info_;
    std::unique_ptr<Drivers::IProtocolDriver> driver_;
    std::shared_ptr<RedisClient> redis_client_;
    std::shared_ptr<InfluxClient> influx_client_;
    std::shared_ptr<LogManager> logger_;
    
    // 상태 관리
    mutable std::atomic<WorkerState> current_state_{WorkerState::STOPPED};
    mutable std::mutex state_mutex_;
    
    // 스레드 관리
    std::unique_ptr<std::thread> read_thread_;
    std::unique_ptr<std::thread> write_thread_;
    std::unique_ptr<std::thread> control_thread_;
    std::atomic<bool> shutdown_requested_{false};
    
    // 데이터 저장
    mutable std::mutex data_points_mutex_;
    std::map<UUID, PulseOne::DataPoint> data_points_;
    std::map<UUID, PulseOne::TimestampedValue> latest_values_;
    
    // 가상 포인트 및 알람
    mutable std::mutex virtual_points_mutex_;
    std::map<UUID, VirtualPointRule> virtual_point_rules_;
    mutable std::mutex alarm_rules_mutex_;
    std::map<UUID, AlarmRule> alarm_rules_;
    
    // 하드웨어 제어 큐
    mutable std::mutex control_queue_mutex_;
    std::queue<HardwareControlRequest> control_queue_;
    std::condition_variable control_queue_cv_;
    
    // 설정
    std::atomic<int> polling_interval_ms_{1000};
    std::atomic<bool> auto_reconnect_enabled_{true};
    
    // 점검 모드
    std::unique_ptr<MaintenanceManager> maintenance_manager_;
    
    // 알람 엔진
    std::unique_ptr<AlarmContextEngine> alarm_engine_;
    
    // 통계
    mutable WorkerStatistics statistics_;
    
    // Redis 채널명들
    std::string status_channel_;
    std::string command_channel_;
    std::string alarm_channel_;
    
    // =============================================================================
    // 내부 스레드 함수들
    // =============================================================================
    
    /**
     * @brief 읽기 스레드 메인 함수
     */
    void ReadThreadMain();
    
    /**
     * @brief 쓰기 스레드 메인 함수
     */
    void WriteThreadMain();
    
    /**
     * @brief 제어 스레드 메인 함수
     */
    void ControlThreadMain();
    
    // =============================================================================
    // 내부 유틸리티 함수들
    // =============================================================================
    
    /**
     * @brief 상태 변경 및 Redis 발행
     * @param new_state 새로운 상태
     */
    void ChangeState(WorkerState new_state);
    
    /**
     * @brief Redis에 상태 발행
     */
    void PublishStatusToRedis();
    
    /**
     * @brief 데이터를 InfluxDB에 저장
     * @param point_id 포인트 ID
     * @param value 값
     */
    void SaveToInfluxDB(const UUID& point_id, const PulseOne::TimestampedValue& value);
    
    /**
     * @brief 가상 포인트 계산
     */
    void CalculateVirtualPoints();
    
    /**
     * @brief 알람 검사
     * @param point_id 포인트 ID
     * @param value 값
     */
    void CheckAlarms(const UUID& point_id, const PulseOne::TimestampedValue& value);
    
    /**
     * @brief 드라이버 재연결 시도
     * @return 성공 시 true
     */
    bool AttemptReconnection();
    
    /**
     * @brief 명령 실행 결과를 Redis에 발행
     * @param command 실행된 명령
     * @param success 성공 여부
     * @param message 메시지
     */
    void PublishCommandResult(const std::string& command, bool success, const std::string& message);
};

} // namespace Engine
} // namespace PulseOne

#endif // ENGINE_DEVICE_WORKER_H