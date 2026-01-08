#include "Workers/WorkerMonitor.h"
#include "Workers/WorkerRegistry.h"
#include "Workers/Base/BaseDeviceWorker.h"
#include "Database/RepositoryFactory.h"
#include "Logging/LogManager.h"
#include "Database/Repositories/DeviceSettingsRepository.h"
#include <chrono>

namespace PulseOne::Workers {

using nlohmann::json;

WorkerMonitor::WorkerMonitor(std::shared_ptr<WorkerRegistry> registry)
    : registry_(std::move(registry)) {
}

nlohmann::json WorkerMonitor::GetWorkerStatus(const std::string& device_id) const {
    if (!registry_) return {{"error", "Registry not initialized"}};

    auto worker = registry_->GetWorker(device_id);
    if (!worker) {
        return {
            {"device_id", device_id},
            {"error", "Worker not found"}
        };
    }

    try {
        auto state = worker->GetState();
        bool is_connected = worker->CheckConnection();
        
        json result = {
            {"device_id", device_id},
            {"state", WorkerStateToString(state)},
            {"connected", is_connected},
            {"auto_reconnect_active", true},
            {"description", GetWorkerStateDescription(state, is_connected)}
        };
        
        // Add metadata
        try {
            json metadata;
            
            // Get latest settings from DB
            int device_int_id = 0;
            try { device_int_id = std::stoi(device_id); } catch (...) {}

            if (device_int_id > 0) {
                auto& repo_factory = Database::RepositoryFactory::getInstance();
                auto settings_repo = repo_factory.getDeviceSettingsRepository();
                
                if (settings_repo) {
                    auto settings_opt = settings_repo->findById(device_int_id);
                    if (settings_opt.has_value()) {
                        const auto& settings = settings_opt.value();
                        
                        metadata["timeout_ms"] = settings.getReadTimeoutMs();
                        metadata["retry_interval_ms"] = settings.getRetryIntervalMs();
                        metadata["backoff_time_ms"] = settings.getBackoffTimeMs();
                        metadata["keep_alive_enabled"] = settings.isKeepAliveEnabled();
                        metadata["polling_interval_ms"] = settings.getPollingIntervalMs();
                        metadata["max_retry_count"] = settings.getMaxRetryCount();
                        
                        metadata["captured_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                    }
                }
            }
            
            if (!metadata.empty()) {
                result["metadata"] = metadata;
            }
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Warn("WorkerMonitor - Failed to generate metadata for " + device_id + ": " + e.what());
        }
        
        return result;
        
    } catch (const std::exception& e) {
        return {
            {"device_id", device_id},
            {"error", "Status query failed: " + std::string(e.what())}
        };
    }
}

nlohmann::json WorkerMonitor::GetWorkerList() const {
    if (!registry_) return json::array();

    json list = json::array();
    
    registry_->ForEachWorker([&](const std::string& id, std::shared_ptr<BaseDeviceWorker> worker) {
        if (worker) {
            list.push_back({
                {"device_id", id},
                {"state", WorkerStateToString(worker->GetState())},
                {"connected", worker->CheckConnection()}
            });
        }
    });

    return list;
}

nlohmann::json WorkerMonitor::GetAllWorkersStatus() const {
    if (!registry_) return json::array();

    json list = json::array();
    
    registry_->ForEachWorker([&](const std::string& id, std::shared_ptr<BaseDeviceWorker> worker) {
        // Reuse GetWorkerStatus for detailed info
        list.push_back(GetWorkerStatus(id));
    });

    return list;
}

nlohmann::json WorkerMonitor::GetDetailedStatistics() const {
    size_t active_count = registry_ ? registry_->GetWorkerCount() : 0;
    
    return {
        {"active_workers", active_count},
        {"total_started", total_started_.load()},
        {"total_stopped", total_stopped_.load()},
        {"total_errors", total_errors_.load()}
    };
}

void WorkerMonitor::IncrementTotalStarted() {
    total_started_++;
}

void WorkerMonitor::IncrementTotalStopped() {
    total_stopped_++;
}

void WorkerMonitor::IncrementTotalErrors() {
    total_errors_++;
}

std::string WorkerMonitor::WorkerStateToString(WorkerState state) const {
    switch (state) {
        case WorkerState::RUNNING: return "RUNNING";
        case WorkerState::STOPPED: return "STOPPED";
        case WorkerState::RECONNECTING: return "RECONNECTING";
        case WorkerState::DEVICE_OFFLINE: return "DEVICE_OFFLINE";
        case WorkerState::WORKER_ERROR: return "ERROR";
        case WorkerState::STARTING: return "STARTING";
        case WorkerState::STOPPING: return "STOPPING";
        case WorkerState::PAUSED: return "PAUSED";
        case WorkerState::MAINTENANCE: return "MAINTENANCE";
        case WorkerState::SIMULATION: return "SIMULATION";
        case WorkerState::CALIBRATION: return "CALIBRATION";
        case WorkerState::COMMISSIONING: return "COMMISSIONING";
        case WorkerState::COMMUNICATION_ERROR: return "COMMUNICATION_ERROR";
        case WorkerState::DATA_INVALID: return "DATA_INVALID";
        case WorkerState::SENSOR_FAULT: return "SENSOR_FAULT";
        case WorkerState::MANUAL_OVERRIDE: return "MANUAL_OVERRIDE";
        case WorkerState::EMERGENCY_STOP: return "EMERGENCY_STOP";
        case WorkerState::BYPASS_MODE: return "BYPASS_MODE";
        case WorkerState::DIAGNOSTIC_MODE: return "DIAGNOSTIC_MODE";
        case WorkerState::WAITING_RETRY: return "WAITING_RETRY";
        case WorkerState::MAX_RETRIES_EXCEEDED: return "MAX_RETRIES_EXCEEDED";
        case WorkerState::UNKNOWN: 
        default: return "UNKNOWN";
    }
}

std::string WorkerMonitor::GetWorkerStateDescription(WorkerState state, bool connected) const {
    switch (state) {
        case WorkerState::RUNNING:
            return connected ? "정상 동작 중" : "데이터 수집 중 (일시적 연결 끊김)";
        case WorkerState::RECONNECTING:
            return "자동 재연결 시도 중";
        case WorkerState::DEVICE_OFFLINE:
            return "디바이스 오프라인 (재연결 대기 중)";
        case WorkerState::WORKER_ERROR:
            return "오류 상태 (복구 시도 중)";
        case WorkerState::STOPPED:
            return "중지됨";
        case WorkerState::STARTING:
            return "시작 중";
        case WorkerState::STOPPING:
            return "중지 중";
        case WorkerState::PAUSED:
            return "일시정지됨";
        case WorkerState::MAINTENANCE:
            return "점검 모드";
        case WorkerState::SIMULATION:
            return "시뮬레이션 모드";
        case WorkerState::CALIBRATION:
            return "교정 모드";
        case WorkerState::COMMISSIONING:
            return "시운전 모드";
        case WorkerState::COMMUNICATION_ERROR:
            return "통신 오류 (복구 시도 중)";
        case WorkerState::DATA_INVALID:
            return "데이터 이상 감지";
        case WorkerState::SENSOR_FAULT:
            return "센서 고장";
        case WorkerState::MANUAL_OVERRIDE:
            return "수동 제어 모드";
        case WorkerState::EMERGENCY_STOP:
            return "비상 정지";
        case WorkerState::BYPASS_MODE:
            return "우회 모드";
        case WorkerState::DIAGNOSTIC_MODE:
            return "진단 모드";
        case WorkerState::WAITING_RETRY:
            return "재시도 대기 중";
        case WorkerState::MAX_RETRIES_EXCEEDED:
            return "최대 재시도 횟수 초과";
        case WorkerState::UNKNOWN:
        default:
            return "상태 확인 중";
    }
}

} // namespace PulseOne::Workers
