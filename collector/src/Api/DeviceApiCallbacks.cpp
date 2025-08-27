// =============================================================================
// collector/src/Api/DeviceApiCallbacks.cpp
// 디바이스 제어 및 워커 관리 전용 REST API 콜백 구현 - 그룹 기능 추가
// =============================================================================

#include "Api/DeviceApiCallbacks.h"
#include "Network/RestApiServer.h" 
#include "Workers/WorkerFactory.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Database/RepositoryFactory.h"

#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <set>

using namespace PulseOne::Api;
using namespace PulseOne::Network;
using namespace PulseOne::Workers;
using nlohmann::json;

// 전역 참조 (워커 재초기화에 필요)
static ConfigManager* g_config_manager = nullptr;

void DeviceApiCallbacks::Setup(RestApiServer* server,
                              WorkerFactory* worker_factory,
                              LogManager* logger) {
    
    if (!server || !worker_factory || !logger) {
        logger->Error("DeviceApiCallbacks::Setup - null parameters");
        return;
    }
    
    logger->Info("Setting up device control API callbacks (with group support)...");

    // ==========================================================================
    // 디바이스 조회 콜백들
    // ==========================================================================
    
    server->SetDeviceListCallback([=]() -> nlohmann::json {
        try {
            logger->Info("API: Retrieving device list");
            
            json device_list = json::array();
            
            // WorkerFactory에서 활성 디바이스 목록 조회
            auto active_devices = worker_factory->GetActiveDeviceList();
            
            for (const auto& device_info : active_devices) {
                json device = json::object();
                device["device_id"] = device_info.device_id;
                device["name"] = device_info.name;
                device["protocol"] = device_info.protocol;
                device["status"] = device_info.status; // "running", "stopped", "paused", "error"
                device["connection_status"] = device_info.connection_status; // "connected", "disconnected", "connecting"
                device["last_scan"] = device_info.last_scan_time;
                device["uptime"] = device_info.uptime_seconds;
                device["error_count"] = device_info.error_count;
                device["scan_rate"] = device_info.scan_rate_ms;
                
                // 그룹 정보 추가
                device["groups"] = GetDeviceGroups(device_info.device_id);
                
                device_list.push_back(device);
            }
            
            logger->Info("API: Retrieved " + std::to_string(device_list.size()) + " devices");
            return device_list;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to get device list - " + std::string(e.what()));
            return json::array();
        }
    });
    
    server->SetDeviceStatusCallback([=](const std::string& device_id) -> nlohmann::json {
        try {
            logger->Info("API: Getting status for device " + device_id);
            
            auto device_status = worker_factory->GetDeviceStatus(device_id);
            
            json status = json::object();
            status["device_id"] = device_id;
            status["running"] = device_status.is_running;
            status["paused"] = device_status.is_paused;
            status["connected"] = device_status.is_connected;
            status["uptime"] = device_status.uptime_formatted;
            status["last_error"] = device_status.last_error;
            status["message_count"] = device_status.total_messages;
            status["scan_rate"] = device_status.scan_rate_ms;
            status["connection_attempts"] = device_status.connection_attempts;
            status["successful_scans"] = device_status.successful_scans;
            status["failed_scans"] = device_status.failed_scans;
            status["average_response_time"] = device_status.avg_response_time_ms;
            status["groups"] = GetDeviceGroups(device_id);
            
            return status;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to get device status for " + device_id + 
                         " - " + std::string(e.what()));
            json error_status = json::object();
            error_status["device_id"] = device_id;
            error_status["error"] = "Failed to get status";
            error_status["running"] = false;
            return error_status;
        }
    });

    // ==========================================================================
    // 디바이스 워커 생명주기 제어 콜백들 - Worker에 위임
    // ==========================================================================
    
    server->SetDeviceStartCallback([=](const std::string& device_id) -> bool {
        try {
            logger->Info("API: Starting device " + device_id);
            logger->Info("   Action: Connect to device + Start polling thread");
            
            // WorkerFactory를 통해 워커 시작 (연결부터 새로 시작)
            bool result = worker_factory->StartWorker(device_id);
            
            if (result) {
                logger->Info("API: Device " + device_id + " started successfully");
            } else {
                logger->Error("API: Failed to start device " + device_id);
            }
            
            return result;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to start device " + device_id + 
                         " - " + std::string(e.what()));
            return false;
        }
    });
    
    server->SetDeviceStopCallback([=](const std::string& device_id) -> bool {
        try {
            logger->Info("API: Stopping device " + device_id);
            logger->Info("   Action: Stop polling + Disconnect from device");
            
            // WorkerFactory를 통해 워커 정지 (연결 완전 끊기)
            bool result = worker_factory->StopWorker(device_id);
            
            if (result) {
                logger->Info("API: Device " + device_id + " stopped successfully");
            } else {
                logger->Error("API: Failed to stop device " + device_id);
            }
            
            return result;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to stop device " + device_id + 
                         " - " + std::string(e.what()));
            return false;
        }
    });
    
    server->SetDevicePauseCallback([=](const std::string& device_id) -> bool {
        try {
            logger->Info("API: Pausing device " + device_id);
            logger->Info("   Action: Pause polling schedule (keep connection)");
            
            // WorkerFactory를 통해 워커 일시정지 (폴링만 중지, 연결 유지)
            bool result = worker_factory->PauseWorker(device_id);
            
            if (result) {
                logger->Info("API: Device " + device_id + " paused successfully");
            } else {
                logger->Error("API: Failed to pause device " + device_id);
            }
            
            return result;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to pause device " + device_id + 
                         " - " + std::string(e.what()));
            return false;
        }
    });
    
    server->SetDeviceResumeCallback([=](const std::string& device_id) -> bool {
        try {
            logger->Info("API: Resuming device " + device_id);
            logger->Info("   Action: Resume polling schedule (connection already exists)");
            
            // WorkerFactory를 통해 워커 재개 (폴링 재시작, 연결은 이미 있음)
            bool result = worker_factory->ResumeWorker(device_id);
            
            if (result) {
                logger->Info("API: Device " + device_id + " resumed successfully");
            } else {
                logger->Error("API: Failed to resume device " + device_id);
            }
            
            return result;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to resume device " + device_id + 
                         " - " + std::string(e.what()));
            return false;
        }
    });
    
    server->SetDeviceRestartCallback([=](const std::string& device_id) -> bool {
        try {
            logger->Info("API: Restarting device " + device_id);
            logger->Info("   Action: Stop polling + Disconnect + Reconnect + Start polling");
            
            // WorkerFactory를 통해 워커 재시작 (완전 재시작)
            bool result = worker_factory->RestartWorker(device_id);
            
            if (result) {
                logger->Info("API: Device " + device_id + " restarted successfully");
            } else {
                logger->Error("API: Failed to restart device " + device_id);
            }
            
            return result;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to restart device " + device_id + 
                         " - " + std::string(e.what()));
            return false;
        }
    });

    // ==========================================================================
    // 디바이스 그룹 제어 콜백들 - 새로 추가
    // ==========================================================================
    
    server->SetDeviceGroupListCallback([=]() -> nlohmann::json {
        try {
            logger->Info("API: Retrieving device groups");
            
            json groups_list = json::array();
            auto groups = GetConfiguredGroups();
            
            for (const auto& [group_id, group_info] : groups) {
                json group = json::object();
                group["group_id"] = group_id;
                group["name"] = group_info.name;
                group["description"] = group_info.description;
                group["device_count"] = group_info.devices.size();
                group["devices"] = json::array();
                
                // 그룹 내 디바이스들의 상태 정보
                int running_count = 0;
                int paused_count = 0;
                int stopped_count = 0;
                int error_count = 0;
                
                for (const auto& device_id : group_info.devices) {
                    json device_status = json::object();
                    device_status["device_id"] = device_id;
                    
                    try {
                        auto status = worker_factory->GetDeviceStatus(device_id);
                        device_status["running"] = status.is_running;
                        device_status["paused"] = status.is_paused;
                        device_status["connected"] = status.is_connected;
                        device_status["error"] = !status.last_error.empty();
                        
                        // 상태별 카운트
                        if (!status.last_error.empty()) {
                            error_count++;
                        } else if (status.is_running) {
                            running_count++;
                        } else if (status.is_paused) {
                            paused_count++;
                        } else {
                            stopped_count++;
                        }
                        
                    } catch (...) {
                        device_status["running"] = false;
                        device_status["paused"] = false;
                        device_status["connected"] = false;
                        device_status["error"] = true;
                        error_count++;
                    }
                    
                    group["devices"].push_back(device_status);
                }
                
                // 그룹 전체 상태 계산
                group["summary"] = json::object();
                group["summary"]["running"] = running_count;
                group["summary"]["paused"] = paused_count;
                group["summary"]["stopped"] = stopped_count;
                group["summary"]["error"] = error_count;
                
                // 그룹 상태 결정
                if (error_count > 0) {
                    group["status"] = "error";
                } else if (running_count == group_info.devices.size()) {
                    group["status"] = "running";
                } else if (stopped_count == group_info.devices.size()) {
                    group["status"] = "stopped";
                } else if (paused_count == group_info.devices.size()) {
                    group["status"] = "paused";
                } else {
                    group["status"] = "mixed";
                }
                
                groups_list.push_back(group);
            }
            
            logger->Info("API: Retrieved " + std::to_string(groups_list.size()) + " device groups");
            return groups_list;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to get device groups - " + std::string(e.what()));
            return json::array();
        }
    });
    
    server->SetDeviceGroupStatusCallback([=](const std::string& group_id) -> nlohmann::json {
        try {
            logger->Info("API: Getting status for device group " + group_id);
            
            auto groups = GetConfiguredGroups();
            if (groups.find(group_id) == groups.end()) {
                logger->Error("Group not found: " + group_id);
                return CreateGroupErrorResponse(group_id, "Group not found");
            }
            
            const auto& group_info = groups[group_id];
            json status = json::object();
            status["group_id"] = group_id;
            status["name"] = group_info.name;
            status["description"] = group_info.description;
            status["devices"] = json::array();
            
            // 각 디바이스 상태 조회
            for (const auto& device_id : group_info.devices) {
                json device_status = json::object();
                device_status["device_id"] = device_id;
                
                try {
                    auto device_info = worker_factory->GetDeviceStatus(device_id);
                    device_status["running"] = device_info.is_running;
                    device_status["paused"] = device_info.is_paused;
                    device_status["connected"] = device_info.is_connected;
                    device_status["uptime"] = device_info.uptime_formatted;
                    device_status["last_error"] = device_info.last_error;
                } catch (...) {
                    device_status["running"] = false;
                    device_status["paused"] = false;
                    device_status["connected"] = false;
                    device_status["uptime"] = "0";
                    device_status["last_error"] = "Status unavailable";
                }
                
                status["devices"].push_back(device_status);
            }
            
            return status;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to get group status for " + group_id + 
                         " - " + std::string(e.what()));
            return CreateGroupErrorResponse(group_id, "Failed to get status");
        }
    });
    
    server->SetDeviceGroupControlCallback([=](const std::string& group_id, const std::string& action) -> bool {
        try {
            logger->Info("API: Performing " + action + " on device group " + group_id);
            
            auto groups = GetConfiguredGroups();
            if (groups.find(group_id) == groups.end()) {
                logger->Error("Group not found: " + group_id);
                return false;
            }
            
            const auto& group_info = groups[group_id];
            logger->Info("Group " + group_id + " contains " + std::to_string(group_info.devices.size()) + " devices");
            
            int success_count = 0;
            int total_count = group_info.devices.size();
            
            // 각 디바이스에 대해 액션 수행
            for (const auto& device_id : group_info.devices) {
                logger->Info("Performing " + action + " on device " + device_id);
                
                bool device_result = false;
                try {
                    if (action == "start") {
                        device_result = worker_factory->StartWorker(device_id);
                    } else if (action == "stop") {
                        device_result = worker_factory->StopWorker(device_id);
                    } else if (action == "pause") {
                        device_result = worker_factory->PauseWorker(device_id);
                    } else if (action == "resume") {
                        device_result = worker_factory->ResumeWorker(device_id);
                    } else if (action == "restart") {
                        device_result = worker_factory->RestartWorker(device_id);
                    } else {
                        logger->Error("Unknown action: " + action);
                        continue;
                    }
                    
                    if (device_result) {
                        success_count++;
                        logger->Info("Device " + device_id + " " + action + " successful");
                    } else {
                        logger->Error("Device " + device_id + " " + action + " failed");
                    }
                    
                } catch (const std::exception& e) {
                    logger->Error("Error performing " + action + " on device " + device_id + 
                                 ": " + std::string(e.what()));
                }
                
                // 디바이스 간 작업 간격 (동시 연결 부하 방지)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            logger->Info("Group " + group_id + " " + action + " completed: " + 
                        std::to_string(success_count) + "/" + std::to_string(total_count) + " devices");
            
            // 과반수 성공시 true 반환
            return success_count > (total_count / 2);
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to perform " + action + " on group " + group_id + 
                         " - " + std::string(e.what()));
            return false;
        }
    });

    // ==========================================================================
    // 진단 모드 설정 콜백
    // ==========================================================================
    
    server->SetDiagnosticsCallback([=](const std::string& device_id, bool enabled) -> bool {
        try {
            std::string status = enabled ? "enabled" : "disabled";
            logger->Info("API: Setting diagnostics for device " + device_id + " to " + status);
            
            // WorkerFactory를 통해 진단 모드 설정
            bool result = worker_factory->SetDiagnosticsMode(device_id, enabled);
            
            if (result) {
                logger->Info("API: Diagnostics " + status + " for device " + device_id);
            }
            
            return result;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to set diagnostics for device " + device_id + 
                         " - " + std::string(e.what()));
            return false;
        }
    });

    // ==========================================================================
    // 드라이버 재초기화 콜백 (ConfigApiCallbacks에서 이동)
    // ==========================================================================
    
    server->SetReinitializeCallback([=]() -> bool {
        try {
            logger->Info("=== Driver reinitialization requested via API ===");
            
            // 1단계: 모든 워커 안전하게 정지
            logger->Info("Step 1: Stopping all workers gracefully...");
            if (!StopAllWorkersSafely(worker_factory, logger)) {
                logger->Warning("Some workers didn't stop gracefully, continuing...");
            }
            
            // 2단계: WorkerFactory 재초기화
            logger->Info("Step 2: Reinitializing worker factory...");
            if (!worker_factory->Reinitialize()) {
                logger->Error("WorkerFactory reinitialization failed");
                return false;
            }
            
            // 3단계: 설정된 디바이스들 재시작
            logger->Info("Step 3: Restarting configured devices...");
            if (!RestartConfiguredDevices(worker_factory, logger)) {
                logger->Error("Failed to restart some devices");
                return false;
            }
            
            logger->Info("Driver reinitialization completed successfully");
            return true;
            
        } catch (const std::exception& e) {
            logger->Error("Driver reinitialization failed: " + std::string(e.what()));
            return false;
        }
    });

    logger->Info("Device control API callbacks setup completed (with group support)");
}

void DeviceApiCallbacks::SetGlobalReferences(ConfigManager* config_manager) {
    g_config_manager = config_manager;
}

// =============================================================================
// 워커 관리 헬퍼 함수들
// =============================================================================

bool DeviceApiCallbacks::StopAllWorkersSafely(WorkerFactory* worker_factory, 
                                             LogManager* logger) {
    try {
        logger->Info("Stopping all workers gracefully...");
        
        // WorkerFactory에서 활성 워커 목록 조회
        auto active_workers = worker_factory->GetActiveWorkerList();
        logger->Info("Found " + std::to_string(active_workers.size()) + " active workers");
        
        int stopped_count = 0;
        
        // 모든 워커에게 정지 신호 전송
        for (const auto& worker_info : active_workers) {
            logger->Info("Stopping worker: " + worker_info.device_id);
            if (worker_factory->StopWorker(worker_info.device_id)) {
                stopped_count++;
            } else {
                logger->Warning("Failed to stop worker: " + worker_info.device_id);
            }
        }
        
        // 워커들이 완전히 정지할 때까지 대기 (최대 10초)
        int wait_count = 0;
        while (worker_factory->GetActiveWorkerCount() > 0 && wait_count < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            wait_count++;
        }
        
        int remaining = worker_factory->GetActiveWorkerCount();
        if (remaining > 0) {
            logger->Warning("Could not stop " + std::to_string(remaining) + " workers gracefully");
            return false;
        }
        
        logger->Info("Successfully stopped " + std::to_string(stopped_count) + " workers");
        return true;
        
    } catch (const std::exception& e) {
        logger->Error("Error stopping workers: " + std::string(e.what()));
        return false;
    }
}

bool DeviceApiCallbacks::RestartConfiguredDevices(WorkerFactory* worker_factory,
                                                 LogManager* logger) {
    try {
        logger->Info("Restarting configured devices...");
        
        // 설정에서 활성화된 디바이스 목록 읽기
        std::vector<std::string> enabled_devices;
        if (g_config_manager) {
            enabled_devices = g_config_manager->getStringList("devices.enabled", {});
        } else {
            // 설정 매니저가 없으면 기본 디바이스들
            enabled_devices = {"1", "2", "3"};
        }
        
        logger->Info("Found " + std::to_string(enabled_devices.size()) + 
                    " devices configured for restart");
        
        int restarted_count = 0;
        for (const auto& device_id : enabled_devices) {
            logger->Info("Starting worker for device: " + device_id);
            
            if (worker_factory->StartWorker(device_id)) {
                restarted_count++;
                logger->Info("Device " + device_id + " started successfully");
            } else {
                logger->Error("Failed to start worker for device: " + device_id);
            }
            
            // 워커 간 시작 간격 (연결 부하 방지)
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        logger->Info("Restarted " + std::to_string(restarted_count) + "/" + 
                    std::to_string(enabled_devices.size()) + " workers");
        
        return restarted_count > 0;
        
    } catch (const std::exception& e) {
        logger->Error("Error restarting workers: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 디바이스 그룹 관리 함수들 - 새로 추가
// =============================================================================

DeviceApiCallbacks::DeviceGroupInfo::DeviceGroupInfo(const std::string& n, const std::string& d, const std::vector<std::string>& devs)
    : name(n), description(d), devices(devs) {}

std::map<std::string, DeviceApiCallbacks::DeviceGroupInfo> DeviceApiCallbacks::GetConfiguredGroups() {
    std::map<std::string, DeviceGroupInfo> groups;
    
    if (g_config_manager) {
        // 설정 파일에서 그룹 정보 읽기
        try {
            // groups.production_line_a.name = "Production Line A"
            // groups.production_line_a.description = "Main production line devices"  
            // groups.production_line_a.devices = ["1", "2", "3"]
            
            auto group_names = g_config_manager->getStringList("groups.list", {});
            for (const auto& group_id : group_names) {
                std::string name = g_config_manager->getString("groups." + group_id + ".name", group_id);
                std::string description = g_config_manager->getString("groups." + group_id + ".description", "");
                auto devices = g_config_manager->getStringList("groups." + group_id + ".devices", {});
                
                if (!devices.empty()) {
                    groups[group_id] = DeviceGroupInfo(name, description, devices);
                }
            }
            
        } catch (const std::exception& e) {
            // 설정 파일에 그룹이 없으면 기본 그룹들 사용
        }
    }
    
    // 설정에 그룹이 없으면 기본 그룹들 생성
    if (groups.empty()) {
        groups["production_line_a"] = DeviceGroupInfo(
            "Production Line A", 
            "Main production line devices", 
            {"1", "2", "3"}
        );
        
        groups["utility_systems"] = DeviceGroupInfo(
            "Utility Systems",
            "HVAC, Power, and monitoring systems",
            {"4", "5"}
        );
        
        groups["quality_control"] = DeviceGroupInfo(
            "Quality Control",
            "Inspection and testing equipment",
            {"6"}
        );
    }
    
    return groups;
}

std::vector<std::string> DeviceApiCallbacks::GetDeviceGroups(const std::string& device_id) {
    std::vector<std::string> device_groups;
    auto groups = GetConfiguredGroups();
    
    for (const auto& [group_id, group_info] : groups) {
        if (std::find(group_info.devices.begin(), group_info.devices.end(), device_id) != group_info.devices.end()) {
            device_groups.push_back(group_id);
        }
    }
    
    return device_groups;
}

// =============================================================================
// 디바이스 설정 관리 헬퍼 함수들 (추가 기능)
// =============================================================================

bool DeviceApiCallbacks::ValidateDeviceId(const std::string& device_id, LogManager* logger) {
    if (device_id.empty()) {
        logger->Error("Empty device ID provided");
        return false;
    }
    
    // 디바이스 ID 형식 검증 (숫자 또는 영숫자 조합)
    if (device_id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos) {
        logger->Error("Invalid device ID format: " + device_id);
        return false;
    }
    
    return true;
}

json DeviceApiCallbacks::CreateDeviceErrorResponse(const std::string& device_id, 
                                                   const std::string& error) {
    json response = json::object();
    response["device_id"] = device_id;
    response["success"] = false;
    response["error"] = error;
    response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return response;
}

json DeviceApiCallbacks::CreateGroupErrorResponse(const std::string& group_id, 
                                                  const std::string& error) {
    json response = json::object();
    response["group_id"] = group_id;
    response["success"] = false;
    response["error"] = error;
    response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return response;
}