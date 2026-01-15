#include "Workers/WorkerRegistry.h"
#include "Workers/WorkerFactory.h"
#include "Workers/Base/BaseDeviceWorker.h"
#include "Logging/LogManager.h"

namespace PulseOne::Workers {

WorkerRegistry::WorkerRegistry() {
    try {
        worker_factory_ = std::make_unique<WorkerFactory>();
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("WorkerRegistry - Failed to create WorkerFactory: " + std::string(e.what()));
    }
}

WorkerRegistry::~WorkerRegistry() {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    workers_.clear();
}

std::shared_ptr<BaseDeviceWorker> WorkerRegistry::CreateAndRegisterWorker(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    // Check if already exists
    auto it = workers_.find(device_id);
    if (it != workers_.end()) {
        LogManager::getInstance().Warn("WorkerRegistry - Worker already exists: " + device_id);
        return it->second;
    }

    if (!worker_factory_) {
        LogManager::getInstance().Error("WorkerRegistry - WorkerFactory not initialized");
        return nullptr;
    }

    try {
        int device_int_id = std::stoi(device_id);
        auto worker = worker_factory_->CreateWorkerById(device_int_id);
        
        if (worker) {
            // 🔥 Load and set data points for the worker
            auto points = worker_factory_->LoadDeviceDataPoints(device_int_id);
            worker->ReloadDataPoints(points);
            LogManager::getInstance().Info("WorkerRegistry - Worker created with " + 
                                         std::to_string(points.size()) + " data points: " + device_id);
            
            workers_[device_id] = std::move(worker);
            return workers_[device_id];
        } else {
            LogManager::getInstance().Error("WorkerRegistry - Factory failed to create worker: " + device_id);
            return nullptr;
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("WorkerRegistry - Exception creating worker " + device_id + ": " + e.what());
        return nullptr;
    }
}

void WorkerRegistry::RegisterWorker(const std::string& device_id, std::shared_ptr<BaseDeviceWorker> worker) {
    if (!worker) return;

    std::lock_guard<std::mutex> lock(registry_mutex_);
    workers_[device_id] = worker;
    LogManager::getInstance().Debug("WorkerRegistry - Manually registered worker: " + device_id);
}

void WorkerRegistry::UnregisterWorker(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    
    auto it = workers_.find(device_id);
    if (it != workers_.end()) {
        workers_.erase(it);
        LogManager::getInstance().Debug("WorkerRegistry - Unregistered worker: " + device_id);
    } else {
        LogManager::getInstance().Warn("WorkerRegistry - Attempted to unregister non-existent worker: " + device_id);
    }
}

std::shared_ptr<BaseDeviceWorker> WorkerRegistry::GetWorker(const std::string& device_id) const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    
    auto it = workers_.find(device_id);
    if (it != workers_.end()) {
        return it->second;
    }
    return nullptr;
}

bool WorkerRegistry::HasWorker(const std::string& device_id) const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return workers_.find(device_id) != workers_.end();
}

std::vector<std::shared_ptr<BaseDeviceWorker>> WorkerRegistry::GetAllWorkers() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    
    std::vector<std::shared_ptr<BaseDeviceWorker>> result;
    result.reserve(workers_.size());
    
    for (const auto& [id, worker] : workers_) {
        result.push_back(worker);
    }
    
    return result;
}

void WorkerRegistry::ReloadFactoryProtocols() {
    if (worker_factory_) {
        worker_factory_->ReloadProtocols();
    }
}

void WorkerRegistry::ForEachWorker(std::function<void(const std::string&, std::shared_ptr<BaseDeviceWorker>)> callback) {
    // Collect workers first to avoid holding lock during callback execution
    // which could lead to deadlocks if callback calls back into registry
    
    std::vector<std::pair<std::string, std::shared_ptr<BaseDeviceWorker>>> snapshot;
    
    {
        std::lock_guard<std::mutex> lock(registry_mutex_);
        snapshot.reserve(workers_.size());
        for (const auto& pair : workers_) {
            snapshot.push_back(pair);
        }
    }
    
    for (const auto& [id, worker] : snapshot) {
        callback(id, worker);
    }
}

size_t WorkerRegistry::GetWorkerCount() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return workers_.size();
}

} // namespace PulseOne::Workers
