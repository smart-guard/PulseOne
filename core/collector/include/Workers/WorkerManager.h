// =============================================================================
// collector/include/Workers/WorkerManager.h - Facade for Worker Components
// =============================================================================
#pragma once

#ifndef WORKER_MANAGER_H
#define WORKER_MANAGER_H

#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// Forward declarations for Facade components
namespace PulseOne::Workers {
    class WorkerRegistry;
    class WorkerRegistry;
    class WorkerScheduler;
    class WorkerMonitor;
    class WorkerMonitor;
    class BaseDeviceWorker; // For completeness if needed in public API return types
    class BACnetDiscoveryService;
}

namespace PulseOne::Storage {
    class RedisDataWriter;
}

namespace PulseOne::Structs {
    struct DataPoint;
}

namespace PulseOne::Workers {
    
class WorkerManager {
public:
    // ==========================================================================
    // Singleton Access
    // ==========================================================================
    static WorkerManager& getInstance();
    
    // Prevent copying
    WorkerManager(const WorkerManager&) = delete;
    WorkerManager& operator=(const WorkerManager&) = delete;

    // ==========================================================================
    // Worker Lifecycle (Delegated to Scheduler)
    // ==========================================================================
    bool StartWorker(const std::string& device_id);
    bool StopWorker(const std::string& device_id);
    bool RestartWorker(const std::string& device_id);
    bool PauseWorker(const std::string& device_id);
    bool ResumeWorker(const std::string& device_id);
    bool ReloadWorkerSettings(const std::string& device_id);
    bool ReloadWorker(const std::string& device_id);
    
    // ==========================================================================
    // Bulk Operations (Delegated to Scheduler)
    // ==========================================================================
    int StartAllActiveWorkers();
    void StopAllWorkers();
    
    // ==========================================================================
    // Device Control (Delegated to Scheduler/Worker)
    // ==========================================================================
    bool WriteDataPoint(const std::string& device_id, const std::string& point_id, const std::string& value);
    bool ControlDigitalOutput(const std::string& device_id, const std::string& output_id, bool enable);
    bool ControlAnalogOutput(const std::string& device_id, const std::string& output_id, double value);
    bool ControlOutput(const std::string& device_id, const std::string& output_id, bool enable);
    bool ControlDigitalDevice(const std::string& device_id, const std::string& device_id_target, bool enable);
    
    // ==========================================================================
    // Discovery (Delegated to Registry -> Worker)
    // ==========================================================================
    std::vector<PulseOne::Structs::DataPoint> DiscoverDevicePoints(const std::string& device_id);
    
    // Network Scan
    bool StartNetworkScan(const std::string& protocol, const std::string& range, int timeout_ms);
    void StopNetworkScan(const std::string& protocol);

    // ==========================================================================
    // Status & Statistics (Delegated to Monitor)
    // ==========================================================================
    nlohmann::json GetWorkerStatus(const std::string& device_id) const;
    nlohmann::json GetWorkerList() const;
    nlohmann::json GetManagerStats() const; // Maps to GetDetailedStatistics
    size_t GetActiveWorkerCount() const;
    bool HasWorker(const std::string& device_id) const;
    bool IsWorkerRunning(const std::string& device_id) const;

private:
    WorkerManager();
    ~WorkerManager();

    // Components
    std::shared_ptr<Storage::RedisDataWriter> redis_writer_;
    std::shared_ptr<WorkerRegistry> registry_;
    std::shared_ptr<WorkerScheduler> scheduler_;
    std::shared_ptr<WorkerMonitor> monitor_;
    std::shared_ptr<BACnetDiscoveryService> bacnet_discovery_service_;
};

} // namespace PulseOne::Workers

#endif // WORKER_MANAGER_H