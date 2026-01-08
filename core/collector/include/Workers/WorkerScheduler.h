#pragma once

#ifndef WORKER_SCHEDULER_H
#define WORKER_SCHEDULER_H

#include <string>
#include <memory>
#include <vector>
#include <future>
#include <mutex>
#include <atomic>
#include "Common/Structs.h"

namespace PulseOne::Storage {
    class RedisDataWriter;
}

namespace PulseOne::Workers {

class WorkerRegistry;
class Monitor;

/**
 * @brief Orchestrates the execution (Start/Stop/Restart) of workers.
 * 
 * Responsibilities:
 * - Starting, Stopping, Pausing, Resuming single workers
 * - Bulk operations (Start All, Stop All)
 * - Redis data initialization upon start
 * - Managing async futures for worker operations
 */
class WorkerScheduler {
public:
    WorkerScheduler(std::shared_ptr<WorkerRegistry> registry, 
                   std::shared_ptr<Storage::RedisDataWriter> redis_writer);
    ~WorkerScheduler();

    // Prevent copying
    WorkerScheduler(const WorkerScheduler&) = delete;
    WorkerScheduler& operator=(const WorkerScheduler&) = delete;

    // Single Worker Operations
    bool StartWorker(const std::string& device_id);
    bool StopWorker(const std::string& device_id);
    bool RestartWorker(const std::string& device_id);
    bool ReloadWorkerSettings(const std::string& device_id);
    bool PauseWorker(const std::string& device_id);
    bool ResumeWorker(const std::string& device_id);

    // Bulk Operations
    int StartAllActiveWorkers();
    void StopAllWorkers();

    // Device Control (Pass-through to Worker)
    bool WriteDataPoint(const std::string& device_id, const std::string& point_id, const std::string& value);
    bool ControlDigitalOutput(const std::string& device_id, const std::string& output_id, bool enable);
    bool ControlAnalogOutput(const std::string& device_id, const std::string& output_id, double value);
    bool ControlDigitalDevice(const std::string& device_id, const std::string& device_id_target, bool enable);

private:
    // Helper Methods
    void InitializeWorkerRedisData(const std::string& device_id);
    int BatchInitializeRedisData(const std::vector<std::string>& device_ids);
    std::vector<Structs::TimestampedValue> LoadCurrentValuesFromDB(const std::string& device_id);
    bool ConvertStringToDataValue(const std::string& str, Structs::DataValue& value);
    
    // Future Management
    void AddPendingFuture(std::future<bool> future);
    void CleanupPendingFutures();

private:
    std::shared_ptr<WorkerRegistry> registry_;
    std::shared_ptr<Storage::RedisDataWriter> redis_writer_;
    
    // Future tracking
    std::mutex futures_mutex_;
    std::vector<std::future<bool>> pending_futures_;
};

} // namespace PulseOne::Workers

#endif // WORKER_SCHEDULER_H
