// =============================================================================
// collector/src/Api/DeviceApiCallbacks.cpp
// 디바이스 제어 관련 REST API 콜백 구현
// =============================================================================

#include "Api/DeviceApiCallbacks.h"
#include "Network/RestApiServer.h"
#include "Workers/WorkerFactory.h"
#include "Utils/LogManager.h"
#include "Common/UUID.h"

using namespace PulseOne::Api;
using namespace PulseOne::Network;
using namespace PulseOne::Workers;

void DeviceApiCallbacks::Setup(RestApiServer* server,
                              WorkerFactory* worker_factory,
                              LogManager* logger) {
    
    if (!server || !worker_factory || !logger) {
        return;
    }
    
    // 디바이스 목록 조회 콜백
    server->SetDeviceListCallback([=]() -> json {
        try {
            json device_list = json::array();
            
            // WorkerFactory에서 활성 디바이스 정보 가져오기
            auto active_workers = worker_factory->GetActiveWorkers();
            
            for (const auto& [device_id, worker] : active_workers) {
                json device_info = json::object();
                device_info["device_id"] = device_id.ToString();
                device_info["status"] = worker->IsRunning() ? "running" : "stopped";
                device_info["protocol"] = "modbus"; // 실제로는 worker에서 가져와야 함
                device_list.push_back(device_info);
            }
            
            return device_list;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to get device list - " + std::string(e.what()));
            return json::array();
        }
    });
    
    // 특정 디바이스 상태 조회 콜백
    server->SetDeviceStatusCallback([=](const std::string& device_id) -> json {
        try {
            json status = json::object();
            
            UUID uuid = UUID::FromString(device_id);
            auto worker = worker_factory->GetWorkerById(uuid);
            
            if (worker) {
                status["device_id"] = device_id;
                status["running"] = worker->IsRunning();
                status["uptime"] = "0";  // 실제로는 worker에서 계산
                status["last_error"] = "";
                status["message_count"] = 0;
            } else {
                status["device_id"] = device_id;
                status["running"] = false;
                status["error"] = "Device not found";
            }
            
            return status;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to get device status for " + device_id + " - " + std::string(e.what()));
            json error_status = json::object();
            error_status["device_id"] = device_id;
            error_status["error"] = "Failed to get status";
            return error_status;
        }
    });
    
    // 디바이스 시작 콜백
    server->SetDeviceStartCallback([=](const std::string& device_id) -> bool {
        try {
            logger->Info("API: Starting device " + device_id);
            
            UUID uuid = UUID::FromString(device_id);
            auto worker = worker_factory->GetWorkerById(uuid);
            
            if (worker) {
                if (!worker->IsRunning()) {
                    worker->Start();
                    logger->Info("API: Device " + device_id + " started successfully");
                    return true;
                } else {
                    logger->Warning("API: Device " + device_id + " is already running");
                    return true;
                }
            } else {
                logger->Error("API: Device " + device_id + " not found");
                return false;
            }
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to start device " + device_id + " - " + std::string(e.what()));
            return false;
        }
    });
    
    // 디바이스 중지 콜백
    server->SetDeviceStopCallback([=](const std::string& device_id) -> bool {
        try {
            logger->Info("API: Stopping device " + device_id);
            
            UUID uuid = UUID::FromString(device_id);
            auto worker = worker_factory->GetWorkerById(uuid);
            
            if (worker) {
                if (worker->IsRunning()) {
                    worker->Stop();
                    logger->Info("API: Device " + device_id + " stopped successfully");
                    return true;
                } else {
                    logger->Warning("API: Device " + device_id + " is already stopped");
                    return true;
                }
            } else {
                logger->Error("API: Device " + device_id + " not found");
                return false;
            }
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to stop device " + device_id + " - " + std::string(e.what()));
            return false;
        }
    });
    
    // 디바이스 일시정지 콜백
    server->SetDevicePauseCallback([=](const std::string& device_id) -> bool {
        try {
            logger->Info("API: Pausing device " + device_id);
            
            UUID uuid = UUID::FromString(device_id);
            auto worker = worker_factory->GetWorkerById(uuid);
            
            if (worker) {
                worker->Pause();
                logger->Info("API: Device " + device_id + " paused successfully");
                return true;
            } else {
                logger->Error("API: Device " + device_id + " not found");
                return false;
            }
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to pause device " + device_id + " - " + std::string(e.what()));
            return false;
        }
    });
    
    // 디바이스 재개 콜백
    server->SetDeviceResumeCallback([=](const std::string& device_id) -> bool {
        try {
            logger->Info("API: Resuming device " + device_id);
            
            UUID uuid = UUID::FromString(device_id);
            auto worker = worker_factory->GetWorkerById(uuid);
            
            if (worker) {
                worker->Resume();
                logger->Info("API: Device " + device_id + " resumed successfully");
                return true;
            } else {
                logger->Error("API: Device " + device_id + " not found");
                return false;
            }
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to resume device " + device_id + " - " + std::string(e.what()));
            return false;
        }
    });
    
    // 디바이스 재시작 콜백
    server->SetDeviceRestartCallback([=](const std::string& device_id) -> bool {
        try {
            logger->Info("API: Restarting device " + device_id);
            
            UUID uuid = UUID::FromString(device_id);
            auto worker = worker_factory->GetWorkerById(uuid);
            
            if (worker) {
                worker->Stop();
                // 잠시 대기
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                worker->Start();
                logger->Info("API: Device " + device_id + " restarted successfully");
                return true;
            } else {
                logger->Error("API: Device " + device_id + " not found");
                return false;
            }
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to restart device " + device_id + " - " + std::string(e.what()));
            return false;
        }
    });
    
    // 진단 모드 설정 콜백
    server->SetDiagnosticsCallback([=](const std::string& device_id, bool enabled) -> bool {
        try {
            logger->Info("API: Setting diagnostics for device " + device_id + " to " + (enabled ? "enabled" : "disabled"));
            
            UUID uuid = UUID::FromString(device_id);
            auto worker = worker_factory->GetWorkerById(uuid);
            
            if (worker) {
                // 실제로는 worker에 진단 모드 설정 메서드가 있어야 함
                // worker->SetDiagnosticsEnabled(enabled);
                logger->Info("API: Diagnostics " + (enabled ? "enabled" : "disabled") + " for device " + device_id);
                return true;
            } else {
                logger->Error("API: Device " + device_id + " not found for diagnostics");
                return false;
            }
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to set diagnostics for device " + device_id + " - " + std::string(e.what()));
            return false;
        }
    });
}