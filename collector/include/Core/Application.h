/**
 * @file Application.h
 * @brief PulseOne Collector v2.0 - 간단 수정본
 * @author PulseOne Development Team
 * @date 2025-07-30
 */

#ifndef PULSEONE_APPLICATION_H
#define PULSEONE_APPLICATION_H

#include <memory>
#include <vector>
#include <atomic>
#include <string>
#include <chrono>
#include "Common/Structs.h"

// 🔧 간단한 전방 선언
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
}

namespace PulseOne {
namespace Core {

/**
 * @brief PulseOne Collector 메인 애플리케이션 클래스
 */
class CollectorApplication {
public:
    CollectorApplication();
    ~CollectorApplication();
    
    void Run();
    void Stop();
    bool IsRunning() const { return is_running_.load(); }

private:
    bool Initialize();
    bool InitializeWorkerFactory();
    void MainLoop();
    void PrintRuntimeStatistics(int loop_count, const std::chrono::steady_clock::time_point& start_time);
    void Cleanup();

private:
    // 실행 상태
    std::atomic<bool> is_running_{false};
    
    // 🔧 전역 클래스들 - PulseOne:: 불필요
    LogManager* logger_;
    ConfigManager* config_manager_;
    DatabaseManager* db_manager_;
    Database::RepositoryFactory* repository_factory_;
    Workers::WorkerFactory* worker_factory_;
};

} // namespace Core
} // namespace PulseOne

#endif // PULSEONE_APPLICATION_H