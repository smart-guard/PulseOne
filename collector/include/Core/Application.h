#ifndef PULSEONE_APPLICATION_H
#define PULSEONE_APPLICATION_H

/**
 * @file Application.h
 * @brief PulseOne Collector v2.0 Simple Edition
 * @author PulseOne Development Team
 * @date 2025-07-30
 */

#include <memory>
#include <vector>
#include <atomic>
#include <string>

// Forward declarations
class LogManager;
class ConfigManager;
class DatabaseManager;

namespace PulseOne {
    namespace Database {
        class RepositoryFactory;
    }
    
    namespace Workers {
        namespace Base {
            class BaseDeviceWorker;
        }
    }
}

namespace PulseOne {
namespace Core {

/**
 * @brief PulseOne Collector 메인 애플리케이션 클래스 (Simple Edition)
 * @details 간단한 인터페이스로 시스템 관리
 */
class CollectorApplication {
public:
    /**
     * @brief 생성자
     */
    CollectorApplication();
    
    /**
     * @brief 소멸자
     */
    ~CollectorApplication();
    
    /**
     * @brief 애플리케이션 실행 (초기화 + 메인 루프)
     */
    void Run();
    
    /**
     * @brief 애플리케이션 중지
     */
    void Stop();
    
    /**
     * @brief 실행 상태 확인
     * @return 실행 중이면 true
     */
    bool IsRunning() const { return is_running_.load(); }

private:
    /**
     * @brief 시스템 초기화
     * @return 성공 시 true
     */
    bool Initialize();
    
    /**
     * @brief 프로토콜 워커들 초기화
     * @return 성공 시 true
     */
    bool InitializeWorkers();
    
    /**
     * @brief 모든 워커 시작
     */
    void StartWorkers();
    
    /**
     * @brief 모든 워커 중지
     */
    void StopWorkers();
    
    /**
     * @brief 메인 루프
     */
    void MainLoop();
    
    /**
     * @brief 시스템 정리
     */
    void Cleanup();

private:
    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // 실행 상태
    std::atomic<bool> is_running_;
    
    // 시스템 컴포넌트들
    LogManager* logger_;
    ConfigManager* config_manager_;
    DatabaseManager* db_manager_;
    PulseOne::Database::RepositoryFactory* repository_factory_;
    
    // 워커들
    std::vector<std::unique_ptr<PulseOne::Workers::Base::BaseDeviceWorker>> workers_;
};

} // namespace Core
} // namespace PulseOne

#endif // PULSEONE_APPLICATION_H