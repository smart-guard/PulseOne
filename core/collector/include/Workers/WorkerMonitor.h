#pragma once

#ifndef WORKER_MONITOR_H
#define WORKER_MONITOR_H

#include <string>
#include <memory>
#include <atomic>
#include <nlohmann/json.hpp>
#include "Workers/Base/BaseDeviceWorker.h" // For WorkerState enum and class

namespace PulseOne::Workers {

class WorkerRegistry;

/**
 * @brief Monitors the health and status of workers.
 * 
 * Responsibilities:
 * - Aggregating status and statistics from workers
 * - Providing JSON formatted status reports for the API
 * - Tracking global worker statistics (start/stop/error counts)
 */
class WorkerMonitor {
public:
    explicit WorkerMonitor(std::shared_ptr<WorkerRegistry> registry);
    ~WorkerMonitor() = default;

    // Prevent copying
    WorkerMonitor(const WorkerMonitor&) = delete;
    WorkerMonitor& operator=(const WorkerMonitor&) = delete;

    /**
     * @brief Gets the status of a specific worker.
     */
    nlohmann::json GetWorkerStatus(const std::string& device_id) const;

    /**
     * @brief Gets a summary list of all workers.
     */
    nlohmann::json GetWorkerList() const;

    /**
     * @brief Gets detailed status of all workers.
     */
    nlohmann::json GetAllWorkersStatus() const;

    /**
     * @brief Gets global manager statistics.
     */
    nlohmann::json GetDetailedStatistics() const;

    /**
     * @brief Increments the total started counter.
     */
    void IncrementTotalStarted();

    /**
     * @brief Increments the total stopped counter.
     */
    void IncrementTotalStopped();

    /**
     * @brief Increments the total error counter.
     */
    void IncrementTotalErrors();

    // Helper functions for state strings (moved from WorkerManager)
    std::string WorkerStateToString(WorkerState state) const;
    std::string GetWorkerStateDescription(WorkerState state, bool connected) const;

private:
    std::shared_ptr<WorkerRegistry> registry_;

    // Statistics
    std::atomic<int> total_started_{0};
    std::atomic<int> total_stopped_{0};
    std::atomic<int> total_errors_{0};
};

} // namespace PulseOne::Workers

#endif // WORKER_MONITOR_H
