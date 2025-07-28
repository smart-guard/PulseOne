#ifndef APPLICATION_H
#define APPLICATION_H

/**
 * @file Application.h  
 * @brief PulseOne Collector 애플리케이션 헤더 (완성본)
 */

#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Database/DatabaseManager.h"
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Workers/WorkerFactory.h"
#include "Workers/Base/BaseDeviceWorker.h"
#include "DBClient.h"
#include "RedisClient.h"
#include "InfluxClient.h"
#include <string>
#include <atomic>
#include <chrono>
#include <memory>
#include <vector>

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

private:
    // =======================================================================
    // 초기화 메서드들
    // =======================================================================
    
    bool Initialize();
    bool InitializeConfigManager();
    bool InitializeDatabaseManager();
    bool InitializeRepositoryFactory();
    bool InitializeDatabaseClients();
    bool InitializeWorkerFactory();
    bool InitializeWorkers();
    
    // =======================================================================
    // 메인 루프 및 관리 메서드들
    // =======================================================================
    
    void MainLoop();
    void MonitorWorkers();
    void PrintStatus();
    void Cleanup();
    void StopAllWorkers();

    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // 기존 멤버들
    PulseOne::LogManager& logger_;
    ConfigManager* config_ = nullptr;
    std::atomic<bool> running_{false};
    std::chrono::system_clock::time_point start_time_;
    const std::string version_ = "1.0.0";
    
    // 🔥 새로 추가된 멤버들
    
    // 데이터베이스 관련
    PulseOne::Database::DatabaseManager* db_manager_ = nullptr;
    std::shared_ptr<PulseOne::Database::Repositories::DeviceRepository> device_repo_;
    std::shared_ptr<PulseOne::Database::Repositories::DataPointRepository> datapoint_repo_;
    
    // 데이터베이스 클라이언트들
    std::shared_ptr<RedisClient> redis_client_;
    std::shared_ptr<InfluxClient> influx_client_;
    
    // Worker 관련
    PulseOne::Workers::WorkerFactory* worker_factory_ = nullptr;
    std::vector<std::unique_ptr<BaseDeviceWorker>> running_workers_;
};

} // namespace Core
} // namespace PulseOne

#endif // APPLICATION_H