/**
 * @file Application.h
 * @brief PulseOne Collector v2.0 - WorkerManager 통합 버전
 * @author PulseOne Development Team
 * @date 2025-08-28
 */

#ifndef PULSEONE_APPLICATION_H
#define PULSEONE_APPLICATION_H

#include <memory>
#include <atomic>
#include <string>
#include <chrono>

#include "Common/Structs.h"

#ifdef HAVE_HTTPLIB
#include "Network/RestApiServer.h"
#include "Api/ConfigApiCallbacks.h"
#include "Api/DeviceApiCallbacks.h"
#endif

// Forward declarations
class LogManager;
class ConfigManager;
#include "DatabaseManager.hpp"

namespace PulseOne {
namespace Database {
    class RepositoryFactory;
}
namespace Workers {
    class WorkerFactory;
    class WorkerManager;  // 추가
}
}

namespace PulseOne {
namespace Core {

/**
 * @brief PulseOne Collector 메인 애플리케이션 클래스
 * 
 * 전체 시스템의 생명주기를 관리하며, 다음 구성요소들을 초기화하고 운영합니다:
 * - 설정 관리 (ConfigManager)
 * - 데이터베이스 연결 (DbLib::DatabaseManager) 
 * - 데이터 저장소 (RepositoryFactory)
 * - 워커 팩토리 (WorkerFactory) - 생성 전담
 * - 워커 관리자 (WorkerManager) - 관리 전담
 * - REST API 서버 (RestApiServer)
 */
class CollectorApplication {
public:
    // ==========================================================================
    // 생성자/소멸자
    // ==========================================================================
    CollectorApplication();
    ~CollectorApplication();

    // ==========================================================================
    // 애플리케이션 생명주기 관리
    // ==========================================================================
    
    /**
     * @brief 애플리케이션 메인 실행
     * @details 초기화 -> 메인루프 -> 정리 순서로 실행
     */
    void Run();
    
    /**
     * @brief 애플리케이션 종료 요청
     * @details 안전한 종료를 위해 is_running_ 플래그를 false로 설정
     */
    void Stop();
    
    /**
     * @brief 실행 상태 확인
     * @return true if running, false if stopped
     */
    bool IsRunning() const { return is_running_.load(); }

private:
    // ==========================================================================
    // 초기화 및 설정 메서드들
    // ==========================================================================
    
    /**
     * @brief 전체 시스템 초기화
     * @return true 성공, false 실패
     */
    bool Initialize();
    
    /**
     * @brief WorkerFactory 초기화 및 의존성 주입
     * @return true 성공, false 실패
     */
    bool InitializeWorkerFactory();
    
    /**
     * @brief REST API 서버 초기화 (WorkerManager 콜백 등록)
     * @return true 성공, false 실패
     */
    bool InitializeRestApiServer();

    // ==========================================================================
    // 런타임 메서드들
    // ==========================================================================
    
    /**
     * @brief 메인 실행 루프
     * @details 
     * - 5분마다 워커 헬스체크 (WorkerManager 통계 사용)
     * - 1시간마다 통계 리포트 출력
     * - 30초 간격으로 루프 실행
     */
    void MainLoop();
    
    /**
     * @brief 시스템 정리 및 리소스 해제
     * @details WorkerManager 종료, API 서버 중지
     */
    void Cleanup();

private:
    // ==========================================================================
    // REST API 서버 (조건부 컴파일)
    // ==========================================================================
#ifdef HAVE_HTTPLIB
    std::unique_ptr<Network::RestApiServer> api_server_;
#endif

    // ==========================================================================
    // 실행 상태 관리
    // ==========================================================================
    std::atomic<bool> is_running_{false};

};

} // namespace Core
} // namespace PulseOne

#endif // PULSEONE_APPLICATION_H