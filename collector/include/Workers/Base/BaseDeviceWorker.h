/**
 * @file BaseDeviceWorker.h
 * @brief 모든 프로토콜 워커의 공통 기반 클래스 (재연결 로직 포함)
 * @details 프로토콜에 독립적인 핵심 인터페이스와 공통 기능 제공
 * @author PulseOne Development Team
 * @date 2025-01-20
 * @version 2.0.0
 */

#ifndef WORKERS_BASE_DEVICE_WORKER_H
#define WORKERS_BASE_DEVICE_WORKER_H

#include "Common/Structs.h"
#include "Common/Enums.h"
#include "Utils/LogManager.h"
#include "Pipeline/PipelineManager.h"  // 🔥 전역 파이프라인 매니저
#include <memory>
#include <future>
#include <atomic>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <mutex>

namespace PulseOne {
namespace Workers {

// 워커 상태 열거형 (현장 운영 상황 반영)
enum class WorkerState {
    STOPPED = 0,                ///< 정지됨
    STARTING = 1,               ///< 시작 중
    RUNNING = 2,                ///< 정상 실행 중
    PAUSED = 3,                 ///< 일시정지됨 (소프트웨어)
    STOPPING = 4,               ///< 정지 중
    ERROR = 5,                  ///< 오류 상태
    
    // 현장 운영 상태들
    MAINTENANCE = 10,           ///< 점검 모드 (하드웨어 점검 중)
    SIMULATION = 11,            ///< 시뮬레이션 모드 (테스트 데이터)
    CALIBRATION = 12,           ///< 교정 모드 (센서 교정 중)
    COMMISSIONING = 13,         ///< 시운전 모드 (초기 설정 중)
    
    // 장애 상황들
    DEVICE_OFFLINE = 20,        ///< 디바이스 오프라인
    COMMUNICATION_ERROR = 21,   ///< 통신 오류
    DATA_INVALID = 22,          ///< 데이터 이상 (범위 벗어남)
    SENSOR_FAULT = 23,          ///< 센서 고장
    
    // 특수 운영 모드들
    MANUAL_OVERRIDE = 30,       ///< 수동 제어 모드
    EMERGENCY_STOP = 31,        ///< 비상 정지
    BYPASS_MODE = 32,           ///< 우회 모드 (백업 시스템 사용)
    DIAGNOSTIC_MODE = 33,       ///< 진단 모드 (상세 로깅)
    
    // 재연결 관련 상태들
    RECONNECTING = 40,          ///< 재연결 시도 중
    WAITING_RETRY = 41,         ///< 재시도 대기 중
    MAX_RETRIES_EXCEEDED = 42   ///< 최대 재시도 횟수 초과
};

/**
 * @brief 재연결 설정 구조체 (데이터베이스에서 로드)
 */
struct ReconnectionSettings {
    bool auto_reconnect_enabled = true;         ///< 자동 재연결 활성화
    int retry_interval_ms = 5000;               ///< 재시도 간격 (밀리초)
    int max_retries_per_cycle = 10;             ///< 사이클당 최대 재시도 횟수
    int wait_time_after_max_retries_ms = 60000; ///< 최대 재시도 후 대기 시간 (밀리초)
    bool keep_alive_enabled = true;             ///< Keep-alive 활성화
    int keep_alive_interval_seconds = 30;       ///< Keep-alive 간격 (초)
    int connection_timeout_seconds = 10;        ///< 연결 타임아웃 (초)
    
    /**
     * @brief 데이터베이스에서 JSON으로 저장/로드
     * @return JSON 문자열
     */
    std::string ToJson() const;
    
    /**
     * @brief JSON에서 설정 로드
     * @param json_str JSON 문자열
     * @return 성공 시 true
     */
    bool FromJson(const std::string& json_str);
};

/**
 * @brief 재연결 통계 정보
 */
struct ReconnectionStats {
    std::atomic<uint64_t> total_connections{0};           ///< 총 연결 횟수
    std::atomic<uint64_t> successful_connections{0};      ///< 성공한 연결 횟수
    std::atomic<uint64_t> failed_connections{0};          ///< 실패한 연결 횟수
    std::atomic<uint64_t> reconnection_cycles{0};         ///< 재연결 사이클 횟수
    std::atomic<uint64_t> wait_cycles{0};                 ///< 대기 사이클 횟수
    std::atomic<uint64_t> keep_alive_sent{0};             ///< 전송한 Keep-alive 횟수
    std::atomic<uint64_t> keep_alive_failed{0};           ///< 실패한 Keep-alive 횟수
    std::atomic<double> avg_connection_duration_seconds{0.0}; ///< 평균 연결 지속 시간
    
    std::chrono::system_clock::time_point first_connection_time; ///< 첫 연결 시간
    std::chrono::system_clock::time_point last_successful_connection; ///< 마지막 성공 연결 시간
    std::chrono::system_clock::time_point last_failure_time;     ///< 마지막 실패 시간
};

/**
 * @brief 모든 디바이스 워커의 기반 클래스
 * @details 재연결, Keep-alive, 상태 관리 등 공통 기능을 제공
 */
class BaseDeviceWorker {
public:
    /**
     * @brief 생성자
     * @param device_info 디바이스 정보
     */
    explicit BaseDeviceWorker(const PulseOne::DeviceInfo& device_info); 
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~BaseDeviceWorker();
    
    // 복사/이동 방지
    BaseDeviceWorker(const BaseDeviceWorker&) = delete;
    BaseDeviceWorker& operator=(const BaseDeviceWorker&) = delete;    
    // =============================================================================
    // 순수 가상 함수들 (파생 클래스에서 구현 필수)
    // =============================================================================
    virtual std::future<bool> Start() = 0;
    virtual std::future<bool> Stop() = 0;
    /**
     * @brief 프로토콜별 연결 수립 (파생 클래스에서 구현)
     * @return 성공 시 true
     */
    virtual bool EstablishConnection() = 0;
    
    /**
     * @brief 프로토콜별 연결 해제 (파생 클래스에서 구현)
     * @return 성공 시 true
     */
    virtual bool CloseConnection() = 0;
    
    /**
     * @brief 프로토콜별 연결 상태 확인 (파생 클래스에서 구현)
     * @return 연결 상태
     */
    virtual bool CheckConnection() = 0;
    
    /**
     * @brief 프로토콜별 Keep-alive 전송 (파생 클래스에서 구현)
     * @return 성공 시 true
     */
    virtual bool SendKeepAlive() { return true; } // 기본 구현 (선택사항)
    
    // =============================================================================
    // 공통 인터페이스 (기본 구현 제공)
    // =============================================================================
    
    /**
     * @brief 현재 워커 상태 조회
     * @return 워커 상태
     */
    virtual WorkerState GetState() const { return current_state_.load(); }
    
    /**
     * @brief 워커 일시정지
     * @return Future<bool> 일시정지 결과
     */
    virtual std::future<bool> Pause();
    
    /**
     * @brief 워커 재개
     * @return Future<bool> 재개 결과
     */
    virtual std::future<bool> Resume();
    
    /**
     * @brief 데이터 포인트 추가
     * @param point 추가할 데이터 포인트
     * @return 성공 시 true
     */
    virtual bool AddDataPoint(const PulseOne::DataPoint& point);
    
    /**
     * @brief 현재 등록된 데이터 포인트 목록 조회
     * @return 데이터 포인트 벡터
     */
    virtual std::vector<PulseOne::DataPoint> GetDataPoints() const;
    
    // =============================================================================
    // 재연결 관리 (공통 기능 - 모든 프로토콜에서 사용)
    // =============================================================================
    
    /**
     * @brief 재연결 설정 업데이트 (데이터베이스에서 로드 또는 웹에서 변경)
     * @param settings 새로운 재연결 설정
     * @return 성공 시 true
     */
    bool UpdateReconnectionSettings(const ReconnectionSettings& settings);
    
    /**
     * @brief 현재 재연결 설정 조회
     * @return 재연결 설정
     */
    ReconnectionSettings GetReconnectionSettings() const;
    
    /**
     * @brief 강제 재연결 시도 (엔지니어 수동 명령)
     * @return Future<bool> 재연결 결과
     */
    std::future<bool> ForceReconnect();
    
    /**
     * @brief 재연결 통계 조회
     * @return JSON 형태의 통계 정보
     */
    std::string GetReconnectionStats() const;
    
    /**
     * @brief 재시도 카운터 및 대기 상태 리셋
     */
    void ResetReconnectionState();
    WorkerState GetCurrentState() const { return current_state_.load(); }
    /**
     * @brief 연결 상태 조회
     * @return 연결 상태
     */
    bool IsConnected() const { return is_connected_.load(); }
    
    // =============================================================================
    // 상태 관리 및 모니터링
    // =============================================================================
    
    /**
     * @brief 워커 상태 정보 JSON 반환
     * @return JSON 형태의 상태 정보
     */
    virtual std::string GetStatusJson() const;
    
    /**
     * @brief 디바이스 정보 조회
     * @return 디바이스 정보
     */
    const PulseOne::DeviceInfo& GetDeviceInfo() const { return device_info_; }
    // ==========================================================================
    // 🔥 파이프라인 연결 메서드들 (임시 비활성화)
    // ==========================================================================
    
    /**
     * @brief 스캔된 데이터를 전역 파이프라인에 전송
     * @param values 스캔된 데이터 값들
     * @param priority 우선순위 (0: 일반, 1: 높음, 2: 긴급)
     * @return 성공 시 true
     */
    bool SendDataToPipeline(const std::vector<PulseOne::TimestampedValue>& values, 
                           uint32_t priority = 0);
    
    /**
     * @brief Worker ID 조회
     */
    const std::string& GetWorkerId() const { return worker_id_; }
    
 protected:
    // =============================================================================
    // 파생 클래스에서 사용할 수 있는 보호된 메서드들
    // =============================================================================
    /**
     * @brief 워커 상태 변경
     * @param new_state 새로운 상태
     */
    void ChangeState(WorkerState new_state);   
    /**
     * @brief 연결 상태 설정 (재연결 로직에서 사용)
     * @param connected 새로운 연결 상태
     */
    void SetConnectionState(bool connected);
    
    /**
     * @brief 네트워크 오류 처리 (재연결 트리거)
     * @param error_message 오류 메시지
     */
    void HandleConnectionError(const std::string& error_message);
    
    /**
     * @brief 로그 메시지 출력
     * @param level 로그 레벨
     * @param message 메시지
     */
    void LogMessage(LogLevel level, const std::string& message) const;
    // =============================================================================
    // WorkerState 유틸리티 함수들 (네임스페이스 레벨)
    // =============================================================================

    /**
     * @brief WorkerState를 문자열로 변환
     * @param state 워커 상태
     * @return 상태 문자열
     */
    std::string WorkerStateToString(WorkerState state) const;

    /**
     * @brief 활성 상태인지 확인
     * @param state 워커 상태
     * @return 활성 상태이면 true
     */
    bool IsActiveState(WorkerState state);

    /**
     * @brief 에러 상태인지 확인
     * @param state 워커 상태
     * @return 에러 상태이면 true
     */
    bool IsErrorState(WorkerState state);

    PulseOne::DeviceInfo device_info_;                    ///< 디바이스 정보
    std::string worker_id_;

private:
    // =============================================================================
    // 내부 데이터 멤버
    // =============================================================================
    
    std::atomic<WorkerState> current_state_{WorkerState::STOPPED}; ///< 현재 상태
    std::atomic<bool> is_connected_{false};              ///< 연결 상태
    
    // =============================================================================
    // 재연결 관리
    // =============================================================================
    
    mutable std::mutex settings_mutex_;                  ///< 설정 뮤텍스
    ReconnectionSettings reconnection_settings_;         ///< 재연결 설정
    ReconnectionStats reconnection_stats_;               ///< 재연결 통계
    
    std::atomic<int> current_retry_count_{0};            ///< 현재 재시도 횟수
    std::atomic<bool> in_wait_cycle_{false};             ///< 대기 사이클 중인지
    std::chrono::system_clock::time_point wait_start_time_; ///< 대기 시작 시간
    std::chrono::system_clock::time_point last_keep_alive_time_; ///< 마지막 Keep-alive 시간
    
    // =============================================================================
    // 백그라운드 스레드 관리
    // =============================================================================
    
    std::unique_ptr<std::thread> reconnection_thread_;   ///< 재연결 관리 스레드
    std::atomic<bool> thread_running_{false};           ///< 스레드 실행 상태
    
    std::string status_channel_;                         ///< Redis 상태 채널
    std::string reconnection_channel_;                   ///< Redis 재연결 채널
    
    // =============================================================================
    // 데이터 포인트 관리
    // =============================================================================
    
    mutable std::mutex data_points_mutex_;               ///< 데이터 포인트 뮤텍스
    std::vector<PulseOne::DataPoint> data_points_;        ///< 등록된 데이터 포인트들
    
    // =============================================================================
    // 내부 메서드들
    // =============================================================================
    
    /**
     * @brief 재연결 관리 스레드 메인 함수
     */
    void ReconnectionThreadMain();
    
    /**
     * @brief 재연결 시도 로직
     * @return 성공 시 true
     */
    bool AttemptReconnection();
    
    /**
     * @brief 대기 사이클 관리
     * @return 대기 완료 시 true
     */
    bool HandleWaitCycle();
    
    /**
     * @brief Keep-alive 관리
     */
    void HandleKeepAlive();
    
    /**
     * @brief Redis 채널명 초기화
     */
    void InitializeRedisChannels();
    
    /**
     * @brief 재연결 통계 업데이트
     * @param connection_successful 연결 성공 여부
     */
    void UpdateReconnectionStats(bool connection_successful);

    std::string GetWorkerIdString() const;

    
};



} // namespace Workers
} // namespace PulseOne

#endif // WORKERS_BASE_DEVICE_WORKER_H