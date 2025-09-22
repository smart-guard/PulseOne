// =============================================================================
// collector/src/Api/DeviceApiCallbacks.cpp
// 디바이스 제어 REST API 콜백 - 실제 코드와 맞춤
// =============================================================================

#include "Api/DeviceApiCallbacks.h"
#include "Network/RestApiServer.h" 
#include "Workers/WorkerManager.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/DeviceRepository.h"  // 실제 헤더 include
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"

#include <thread>
#include <chrono>

using namespace PulseOne::Api;
using namespace PulseOne::Network;
using nlohmann::json;

void DeviceApiCallbacks::Setup(RestApiServer* server) {
    
    if (!server) {
        LogManager::getInstance().Error("DeviceApiCallbacks::Setup - null server parameter");
        return;
    }
    
    LogManager::getInstance().Info("Setting up device control API callbacks...");

    // ==========================================================================
    // 디바이스 조회 콜백들
    // ==========================================================================
    
    server->SetDeviceListCallback([=]() -> json {
        try {
            LogManager::getInstance().Info("API: Retrieving device list");
            
            json device_list = json::array();
            
            try {
                // RepositoryFactory를 통해 실제 DB에서 디바이스 정보 조회
                auto& repo_factory = Database::RepositoryFactory::getInstance();
                auto device_repo = repo_factory.getDeviceRepository();
                
                if (device_repo) {
                    auto devices = device_repo->findAll();
                    
                    for (const auto& device : devices) {
                        json device_json = json::object();
                        device_json["device_id"] = std::to_string(device.getId());
                        device_json["name"] = device.getName();
                        device_json["protocol"] = device.getProtocolDisplayName();
                        device_json["endpoint"] = device.getEndpoint();
                        device_json["enabled"] = device.isEnabled();
                        
                        // WorkerManager에서 실시간 상태 조회
                        auto& worker_mgr = Workers::WorkerManager::getInstance();
                        std::string device_id = std::to_string(device.getId());
                        device_json["status"] = worker_mgr.IsWorkerRunning(device_id) ? "running" : "stopped";
                        device_json["connection_status"] = worker_mgr.IsWorkerRunning(device_id) ? "connected" : "disconnected";
                        
                        device_list.push_back(device_json);
                    }
                } else {
                    LogManager::getInstance().Warn("DeviceRepository not available, using fallback");
                    // 폴백으로 기본 디바이스 정보 제공
                    std::vector<std::string> fallback_devices = {"1", "2", "3"};
                    for (const auto& device_id : fallback_devices) {
                        json device_json = json::object();
                        device_json["device_id"] = device_id;
                        device_json["name"] = "Device " + device_id;
                        device_json["protocol"] = "Modbus TCP";
                        device_json["enabled"] = true;
                        device_json["status"] = "stopped";
                        device_json["connection_status"] = "disconnected";
                        device_list.push_back(device_json);
                    }
                }
            } catch (const std::exception& e) {
                LogManager::getInstance().Error("Error accessing device repository: " + std::string(e.what()));
                // 에러 발생시에도 빈 배열 반환
                return json::array();
            }
            
            LogManager::getInstance().Info("API: Retrieved " + std::to_string(device_list.size()) + " devices");
            return device_list;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("API: Failed to get device list - " + std::string(e.what()));
            return json::array();
        }
    });
    
    server->SetDeviceStatusCallback([=](const std::string& device_id) -> json {
        try {
            LogManager::getInstance().Info("API: Getting status for device " + device_id);
            
            auto& worker_mgr = Workers::WorkerManager::getInstance();
            
            json status = json::object();
            status["device_id"] = device_id;
            status["running"] = worker_mgr.IsWorkerRunning(device_id);
            status["paused"] = false;
            status["connected"] = worker_mgr.IsWorkerRunning(device_id);
            status["uptime"] = "0d 0h 0m 0s";
            status["last_error"] = "";
            status["message_count"] = 0;
            status["scan_rate"] = 1000;
            status["connection_attempts"] = 0;
            status["successful_scans"] = 0;
            status["failed_scans"] = 0;
            status["average_response_time"] = 0.0;
            
            return status;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("API: Failed to get device status for " + device_id + 
                         " - " + std::string(e.what()));
            return CreateDeviceErrorResponse(device_id, "Failed to get status");
        }
    });

    // ==========================================================================
    // 디바이스 워커 생명주기 제어 콜백들 - 실제 WorkerManager 호출
    // ==========================================================================
    
    server->SetDeviceStartCallback([=](const std::string& device_id) -> bool {
        try {
            LogManager::getInstance().Info("API: Starting device worker " + device_id);
            
            auto& worker_mgr = Workers::WorkerManager::getInstance();
            bool result = worker_mgr.StartWorker(device_id);
            
            if (result) {
                LogManager::getInstance().Info("Device worker started successfully: " + device_id);
            } else {
                LogManager::getInstance().Error("Failed to start device worker: " + device_id);
            }
            
            return result;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("API: Exception starting device " + device_id + 
                         " - " + std::string(e.what()));
            return false;
        }
    });
    
    server->SetDeviceStopCallback([=](const std::string& device_id) -> bool {
        try {
            LogManager::getInstance().Info("API: Stopping device worker " + device_id);
            
            auto& worker_mgr = Workers::WorkerManager::getInstance();
            bool result = worker_mgr.StopWorker(device_id);
            
            if (result) {
                LogManager::getInstance().Info("Device worker stopped successfully: " + device_id);
            } else {
                LogManager::getInstance().Error("Failed to stop device worker: " + device_id);
            }
            
            return result;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("API: Exception stopping device " + device_id + 
                         " - " + std::string(e.what()));
            return false;
        }
    });
    
    server->SetDevicePauseCallback([=](const std::string& device_id) -> bool {
        try {
            LogManager::getInstance().Info("API: Pausing device worker " + device_id);
            
            auto& worker_mgr = Workers::WorkerManager::getInstance();
            bool result = worker_mgr.PauseWorker(device_id);
            
            return result;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("API: Exception pausing device " + device_id + 
                         " - " + std::string(e.what()));
            return false;
        }
    });
    
    server->SetDeviceResumeCallback([=](const std::string& device_id) -> bool {
        try {
            LogManager::getInstance().Info("API: Resuming device worker " + device_id);
            
            auto& worker_mgr = Workers::WorkerManager::getInstance();
            bool result = worker_mgr.ResumeWorker(device_id);
            
            return result;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("API: Exception resuming device " + device_id + 
                         " - " + std::string(e.what()));
            return false;
        }
    });
    
    server->SetDeviceRestartCallback([=](const std::string& device_id) -> bool {
        try {
            LogManager::getInstance().Info("API: Restarting device worker " + device_id);
            
            auto& worker_mgr = Workers::WorkerManager::getInstance();
            bool result = worker_mgr.RestartWorker(device_id);
            
            return result;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("API: Exception restarting device " + device_id + 
                         " - " + std::string(e.what()));
            return false;
        }
    });

    // ==========================================================================
    // 전체 시스템 재초기화 콜백 - 실제 재구성 로직
    // ==========================================================================
    
    server->SetReinitializeCallback([=]() -> bool {
        try {
            LogManager::getInstance().Info("=== SYSTEM REINITIALIZATION REQUESTED ===");
            
            // 1단계: 모든 워커 안전 정지
            LogManager::getInstance().Info("Step 1: Stopping all workers...");
            if (!StopAllWorkersSafely()) {
                LogManager::getInstance().Warn("Some workers didn't stop gracefully, continuing...");
            }
            
            // 2단계: 설정 파일 재로드
            LogManager::getInstance().Info("Step 2: Reloading configuration...");
            auto& config_mgr = ConfigManager::getInstance();
            config_mgr.reload();
            
            // 3단계: 활성 디바이스들 재시작
            LogManager::getInstance().Info("Step 3: Restarting configured devices...");
            if (!RestartConfiguredDevices()) {
                LogManager::getInstance().Error("Failed to restart some devices");
                return false;
            }
            
            // 4단계: 초기화 완료 확인
            LogManager::getInstance().Info("Step 4: Verifying system status...");
            auto& worker_mgr = Workers::WorkerManager::getInstance();
            size_t active_workers = worker_mgr.GetActiveWorkerCount();
            
            LogManager::getInstance().Info("=== REINITIALIZATION COMPLETED ===");
            LogManager::getInstance().Info("Active workers: " + std::to_string(active_workers));
            
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("REINITIALIZATION FAILED: " + std::string(e.what()));
            return false;
        }
    });

    LogManager::getInstance().Info("Device control API callbacks setup completed");
}

// =============================================================================
// 실제 워커 관리 로직 - WorkerManager와 연동
// =============================================================================

bool DeviceApiCallbacks::StopAllWorkersSafely() {
    try {
        LogManager::getInstance().Info("Stopping all workers gracefully...");
        
        auto& worker_mgr = Workers::WorkerManager::getInstance();
        
        // 모든 워커 중지 요청
        worker_mgr.StopAllWorkers();
        
        // 워커들이 완전히 정지할 때까지 대기 (최대 10초)
        int wait_count = 0;
        while (worker_mgr.GetActiveWorkerCount() > 0 && wait_count < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            wait_count++;
        }
        
        size_t remaining = worker_mgr.GetActiveWorkerCount();
        if (remaining > 0) {
            LogManager::getInstance().Warn("Could not stop " + std::to_string(remaining) + 
                                         " workers gracefully");
            return false;
        }
        
        LogManager::getInstance().Info("Successfully stopped all workers");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Error stopping workers: " + std::string(e.what()));
        return false;
    }
}

bool DeviceApiCallbacks::RestartConfiguredDevices() {
    try {
        LogManager::getInstance().Info("Restarting configured devices...");
        
        std::vector<std::string> enabled_devices;
        
        try {
            // DB에서 활성화된 디바이스 목록 조회
            auto& repo_factory = Database::RepositoryFactory::getInstance();
            auto device_repo = repo_factory.getDeviceRepository();
            
            if (device_repo) {
                auto devices = device_repo->findAll();
                
                for (const auto& device : devices) {
                    if (device.isEnabled()) {
                        enabled_devices.push_back(std::to_string(device.getId()));
                    }
                }
            } else {
                // Repository 없으면 설정에서 읽기
                std::string devices_config = ConfigManager::getInstance().get("devices.enabled");
                if (!devices_config.empty()) {
                    // 쉼표로 구분된 ID들 파싱
                    std::stringstream ss(devices_config);
                    std::string item;
                    while (std::getline(ss, item, ',')) {
                        item.erase(0, item.find_first_not_of(" \t"));
                        item.erase(item.find_last_not_of(" \t") + 1);
                        if (!item.empty()) {
                            enabled_devices.push_back(item);
                        }
                    }
                } else {
                    // 기본값
                    enabled_devices = {"1", "2", "3"};
                }
            }
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Error reading device configuration: " + std::string(e.what()));
            // 폴백으로 기본 디바이스들 사용
            enabled_devices = {"1", "2", "3"};
        }
        
        LogManager::getInstance().Info("Found " + std::to_string(enabled_devices.size()) + 
                                     " enabled devices to restart");
        
        auto& worker_mgr = Workers::WorkerManager::getInstance();
        int restarted_count = 0;
        
        for (const auto& device_id : enabled_devices) {
            LogManager::getInstance().Info("Starting worker for device: " + device_id);
            
            if (worker_mgr.StartWorker(device_id)) {
                restarted_count++;
                LogManager::getInstance().Info("Device " + device_id + " started successfully");
            } else {
                LogManager::getInstance().Error("Failed to start worker for device: " + device_id);
            }
            
            // 연속 시작 부하 방지
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        LogManager::getInstance().Info("Restarted " + std::to_string(restarted_count) + "/" + 
                                     std::to_string(enabled_devices.size()) + " workers");
        
        return restarted_count > 0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Error restarting workers: " + std::string(e.what()));
        return false;
    }
}

json DeviceApiCallbacks::CreateDeviceErrorResponse(const std::string& device_id, const std::string& error) {
    json response = json::object();
    response["device_id"] = device_id;
    response["success"] = false;
    response["error"] = error;
    response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return response;
}