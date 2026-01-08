#pragma once

#ifndef WORKER_REGISTRY_H
#define WORKER_REGISTRY_H

#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>
#include <vector>
#include <functional>

namespace PulseOne::Workers {

class BaseDeviceWorker;
class WorkerFactory;

/**
 * @brief Manages the lifecycle and storage of Worker instances.
 * 
 * Responsibilities:
 * - Thread-safe storage of worker instances (Map)
 * - Worker creation via WorkerFactory
 * - Worker retrieval and deletion
 */
class WorkerRegistry {
public:
    WorkerRegistry();
    ~WorkerRegistry();

    // Prevent copying
    WorkerRegistry(const WorkerRegistry&) = delete;
    WorkerRegistry& operator=(const WorkerRegistry&) = delete;

    /**
     * @brief Creates and registers a new worker for the given device ID.
     * @return Shared pointer to the created worker, or nullptr if creation failed.
     */
    std::shared_ptr<BaseDeviceWorker> CreateAndRegisterWorker(const std::string& device_id);

    /**
     * @brief Registers an existing worker instance (useful for testing).
     */
    void RegisterWorker(const std::string& device_id, std::shared_ptr<BaseDeviceWorker> worker);

    /**
     * @brief Unregisters and removes a worker.
     */
    void UnregisterWorker(const std::string& device_id);

    /**
     * @brief Retrieves a worker by device ID.
     * @return Shared pointer to the worker, or nullptr if not found.
     */
    std::shared_ptr<BaseDeviceWorker> GetWorker(const std::string& device_id) const;

    /**
     * @brief Checks if a worker exists.
     */
    bool HasWorker(const std::string& device_id) const;

    /**
     * @brief Retrieves all registered workers.
     * Use with caution as the list may be large.
     */
    std::vector<std::shared_ptr<BaseDeviceWorker>> GetAllWorkers() const;

    /**
     * @brief Reloads protocols in the factory.
     */
    void ReloadFactoryProtocols();
    
    /**
     * @brief Iterates over all workers safely with a callback.
     */
    void ForEachWorker(std::function<void(const std::string&, std::shared_ptr<BaseDeviceWorker>)> callback);

    /**
     * @brief Returns the number of registered workers.
     */
    size_t GetWorkerCount() const;

private:
    mutable std::mutex registry_mutex_;
    std::unordered_map<std::string, std::shared_ptr<BaseDeviceWorker>> workers_;
    std::unique_ptr<WorkerFactory> worker_factory_;
};

} // namespace PulseOne::Workers

#endif // WORKER_REGISTRY_H
