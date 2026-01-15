// =============================================================================
// collector/src/Workers/WorkerManager.cpp - Facade Implementation
// =============================================================================

#include "Workers/WorkerManager.h"
#include "Workers/WorkerRegistry.h"
#include "Workers/WorkerScheduler.h"
#include "Workers/WorkerMonitor.h"
#include "Workers/Base/BaseDeviceWorker.h"
#include "Workers/Protocol/BACnetDiscoveryService.h"
#include "Database/RepositoryFactory.h"
#include "Storage/RedisDataWriter.h"
#include "Logging/LogManager.h"

namespace PulseOne::Workers {

using nlohmann::json;

// =============================================================================
// Singleton & Initialization
// =============================================================================

WorkerManager& WorkerManager::getInstance() {
    static WorkerManager instance;
    return instance;
}

WorkerManager::WorkerManager() {
    try {
        // Initialize Components
        redis_writer_ = std::make_shared<Storage::RedisDataWriter>();
        registry_ = std::make_shared<WorkerRegistry>();
        scheduler_ = std::make_shared<WorkerScheduler>(registry_, redis_writer_);
        monitor_ = std::make_shared<WorkerMonitor>(registry_);

        // Initialize BACnet Discovery Service
        auto& repo_factory = Database::RepositoryFactory::getInstance();
        bacnet_discovery_service_ = std::make_shared<BACnetDiscoveryService>(
            repo_factory.getDeviceRepository(),
            repo_factory.getDataPointRepository(),
            repo_factory.getCurrentValueRepository(),
            repo_factory.getDeviceSettingsRepository(),
            nullptr // WorkerFactory will be passed if needed, or nullptr for now
        );
        // Set Schedule Repository
        bacnet_discovery_service_->SetDeviceScheduleRepository(repo_factory.getDeviceScheduleRepository());
        
        LogManager::getInstance().Info("WorkerManager (Facade) initialized with sub-components");
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("WorkerManager initialization failed: " + std::string(e.what()));
    }
}

WorkerManager::~WorkerManager() {
    // Components destructors will handle cleanup
    // specifically Scheduler will StopAllWorkers
}

// =============================================================================
// Delegations to Scheduler
// =============================================================================

bool WorkerManager::StartWorker(const std::string& device_id) {
    if (scheduler_) return scheduler_->StartWorker(device_id);
    return false;
}

bool WorkerManager::StopWorker(const std::string& device_id) {
    if (scheduler_) return scheduler_->StopWorker(device_id);
    return false;
}

bool WorkerManager::RestartWorker(const std::string& device_id) {
    if (scheduler_) return scheduler_->RestartWorker(device_id);
    return false;
}

bool WorkerManager::PauseWorker(const std::string& device_id) {
    if (scheduler_) return scheduler_->PauseWorker(device_id);
    return false;
}

bool WorkerManager::ResumeWorker(const std::string& device_id) {
    if (scheduler_) return scheduler_->ResumeWorker(device_id);
    return false;
}

bool WorkerManager::ReloadWorkerSettings(const std::string& device_id) {
    if (scheduler_) return scheduler_->ReloadWorkerSettings(device_id);
    return false;
}

bool WorkerManager::ReloadWorker(const std::string& device_id) {
    // Original ReloadWorker updated factory protocols then restarted.
    // Registry handles factory reload.
    if (registry_) registry_->ReloadFactoryProtocols();
    return RestartWorker(device_id);
}

int WorkerManager::StartAllActiveWorkers() {
    if (scheduler_) return scheduler_->StartAllActiveWorkers();
    return 0;
}

void WorkerManager::StopAllWorkers() {
    if (scheduler_) scheduler_->StopAllWorkers();
}

bool WorkerManager::WriteDataPoint(const std::string& device_id, const std::string& point_id, const std::string& value) {
    if (scheduler_) return scheduler_->WriteDataPoint(device_id, point_id, value);
    return false;
}

bool WorkerManager::ControlDigitalOutput(const std::string& device_id, const std::string& output_id, bool enable) {
    if (scheduler_) return scheduler_->ControlDigitalOutput(device_id, output_id, enable);
    return false;
}

bool WorkerManager::ControlAnalogOutput(const std::string& device_id, const std::string& output_id, double value) {
    if (scheduler_) return scheduler_->ControlAnalogOutput(device_id, output_id, value);
    return false;
}

bool WorkerManager::ControlOutput(const std::string& device_id, const std::string& output_id, bool enable) {
    return ControlDigitalOutput(device_id, output_id, enable);
}

bool WorkerManager::ControlDigitalDevice(const std::string& device_id, const std::string& device_id_target, bool enable) {
    if (scheduler_) return scheduler_->ControlDigitalDevice(device_id, device_id_target, enable);
    return false;
}

// =============================================================================
// Delegations to Registry/Worker directly (Discovery)
// =============================================================================

std::vector<PulseOne::Structs::DataPoint> WorkerManager::DiscoverDevicePoints(const std::string& device_id) {
    if (!registry_) return {};
    auto worker = registry_->GetWorker(device_id);
    if (!worker) return {};
    return worker->DiscoverDataPoints();
}

bool WorkerManager::StartNetworkScan(const std::string& protocol, const std::string& range, int timeout_ms) {
    if (protocol == "BACNET" || protocol == "BACnet") {
        if (bacnet_discovery_service_) {
            LogManager::getInstance().Info("WorkerManager: Starting BACnet network scan");
            return bacnet_discovery_service_->StartNetworkScan(range);
        } else {
            LogManager::getInstance().Error("WorkerManager: BACnet Discovery Service not initialized");
            return false;
        }
    } else if (protocol == "MODBUS_TCP" || protocol == "MODBUS_RTU") {
        LogManager::getInstance().Info("WorkerManager: Starting stubbed Modbus network scan for protocol: " + protocol);
        // 🔥 Phase 3: Modbus Network Scan Stub
        // In real implementation, this would spawn a ModbusScanner worker
        return true; 
    }
    
    LogManager::getInstance().Warn("WorkerManager: Unsupported partial scan protocol: " + protocol);
    return false;
}

void WorkerManager::StopNetworkScan(const std::string& protocol) {
    if (protocol == "BACNET" || protocol == "BACnet") {
        if (bacnet_discovery_service_) {
            bacnet_discovery_service_->StopNetworkScan();
        }
    } else if (protocol == "MODBUS_TCP" || protocol == "MODBUS_RTU") {
        LogManager::getInstance().Info("WorkerManager: Stopping stubbed Modbus network scan");
    }
}

// =============================================================================
// Delegations to Monitor
// =============================================================================

nlohmann::json WorkerManager::GetWorkerStatus(const std::string& device_id) const {
    if (monitor_) return monitor_->GetWorkerStatus(device_id);
    return {{"error", "Monitor not initialized"}};
}

nlohmann::json WorkerManager::GetWorkerList() const {
    if (monitor_) return monitor_->GetWorkerList();
    return json::array();
}

nlohmann::json WorkerManager::GetManagerStats() const {
    if (monitor_) return monitor_->GetDetailedStatistics();
    return json::object();
}

size_t WorkerManager::GetActiveWorkerCount() const {
    if (registry_) return registry_->GetWorkerCount();
    return 0;
}

bool WorkerManager::HasWorker(const std::string& device_id) const {
    if (registry_) return registry_->HasWorker(device_id);
    return false;
}

bool WorkerManager::IsWorkerRunning(const std::string& device_id) const {
    if (!registry_) return false;
    auto worker = registry_->GetWorker(device_id);
    if (!worker) return false;
    return worker->GetState() == WorkerState::RUNNING;
}

} // namespace PulseOne::Workers
