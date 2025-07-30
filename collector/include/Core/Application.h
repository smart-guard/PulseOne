#ifndef PULSEONE_APPLICATION_H
#define PULSEONE_APPLICATION_H

/**
 * @file Application.h
 * @brief PulseOne Collector v2.0 - 데이터베이스 기반 멀티테넌트 워커 관리 (최종 완성본)
 * @author PulseOne Development Team
 * @date 2025-07-30
 */

#include <memory>
#include <vector>
#include <atomic>
#include <string>
#include <chrono>
#include "Common/Structs.h"

// Forward declarations (컴파일 속도 향상 및 의존성 최소화)
class LogManager;
class ConfigManager;
class DatabaseManager;

namespace PulseOne {

namespace Database {
    class RepositoryFactory;
}

namespace Workers {
    class BaseDeviceWorker;
    class WorkerFactory;
}

namespace Core {

/**
 * @brief PulseOne Collector 메인 애플리케이션 클래스 (최종 완성본)
 * 
 * 주요 기능:
 * - 멀티테넌트 환경에서 모든 활성 디바이스 자동 인식
 * - 프로토콜별 워커 자동 생성 (MODBUS_TCP, MQTT, BACNET 등)
 * - 데이터베이스 기반 동적 워커 관리
 * - 실시간 상태 모니터링 및 통계
 * - 안전한 시스템 시작/종료 관리
 */
class CollectorApplication {
public:
    /**
     * @brief 생성자
     */
    CollectorApplication();
    
    /**
     * @brief 소멸자 - 안전한 정리 보장
     */
    ~CollectorApplication();
    
    /**
     * @brief 애플리케이션 메인 실행
     * 
     * 실행 순서:
     * 1. 시스템 초기화 (Config, DB, Repository, WorkerFactory)
     * 2. 데이터베이스에서 모든 활성 디바이스 로드
     * 3. 프로토콜별 워커 자동 생성
     * 4. 워커들 시작
     * 5. 메인 루프 실행 (상태 모니터링)
     */
    void Run();
    
    /**
     * @brief 애플리케이션 안전 종료
     */
    void Stop();
    
    /**
     * @brief 실행 상태 확인
     * @return 실행 중이면 true
     */
    bool IsRunning() const { return is_running_.load(); }

private:
    // =======================================================================
    // 초기화 관련 메서드들
    // =======================================================================
    
    /**
     * @brief 시스템 전체 초기화
     * - ConfigManager, DatabaseManager 초기화
     * - RepositoryFactory 초기화
     * - WorkerFactory 초기화 및 의존성 주입
     * @return 성공 시 true
     */
    bool Initialize();
    
    /**
     * @brief WorkerFactory 초기화 및 의존성 주입
     * - Repository들을 WorkerFactory에 주입
     * - 데이터베이스 클라이언트들(Redis, InfluxDB) 주입
     * @return 성공 시 true
     */
    bool InitializeWorkerFactory();
    
    // =======================================================================
    // 워커 관리 메서드들
    // =======================================================================
    
    /**
     * @brief 데이터베이스에서 워커들 로드 및 생성
     * 
     * 동작:
     * 1. 모든 활성 테넌트의 활성 디바이스 조회
     * 2. 각 디바이스의 protocol_type에 따라 워커 생성
     * 3. 각 디바이스의 데이터포인트들 자동 로드
     * 4. 생성된 워커들을 컨테이너에 추가
     * @return 성공 시 true
     */
    bool LoadWorkersFromDatabase();
    
    /**
     * @brief 모든 워커 시작
     * - 각 워커별로 개별 시작
     * - 시작 성공/실패 통계 출력
     */
    void StartWorkers();
    
    /**
     * @brief 모든 워커 안전 중지
     * - 각 워커별로 개별 중지
     * - 중지 성공/실패 통계 출력
     */
    void StopWorkers();
    
    // =======================================================================
    // 모니터링 및 통계 메서드들
    // =======================================================================
    
    /**
     * @brief 메인 루프 - 시스템 모니터링
     * - 30초마다 워커 상태 확인
     * - 5분마다 상세 통계 출력
     * - 시스템 헬스체크
     */
    void MainLoop();
    
    /**
     * @brief 워커 로드 완료 통계 출력
     * - 테넌트별 워커 분포
     * - 프로토콜별 워커 분포
     * - 총 워커 수
     */
    void PrintLoadStatistics();
    
    /**
     * @brief 실시간 시스템 상태 출력
     * @param loop_count 루프 카운터
     * @param start_time 시스템 시작 시간
     */
    void PrintRuntimeStatistics(int loop_count, 
                               const std::chrono::steady_clock::time_point& start_time);
    
    /**
     * @brief 상세 시스템 통계 출력 (5분 주기)
     * - WorkerFactory 통계
     * - Repository 캐시 통계
     * - 성능 지표
     */
    void PrintDetailedStatistics();
    
    /**
     * @brief 시스템 정리
     * - 모든 워커 안전 중지
     * - 리소스 해제
     * - 로그 기록
     */
    void Cleanup();

private:
    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // 실행 상태 (스레드 안전)
    std::atomic<bool> is_running_;
    
    // 시스템 컴포넌트들 (싱글톤 참조)
    LogManager* logger_;
    ConfigManager* config_manager_;
    DatabaseManager* db_manager_;
    PulseOne::Database::RepositoryFactory* repository_factory_;
    PulseOne::Workers::WorkerFactory* worker_factory_;
    
    // 워커 컨테이너 (데이터베이스에서 동적으로 생성된 워커들)
    // 멀티테넌트 환경의 모든 활성 디바이스 워커들을 포함
    std::vector<std::unique_ptr<PulseOne::Workers::BaseDeviceWorker>> workers_;
};

} // namespace Core
} // namespace PulseOne

#endif // PULSEONE_APPLICATION_H