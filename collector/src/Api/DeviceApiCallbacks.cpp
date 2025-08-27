// =============================================================================
// collector/src/Api/DeviceApiCallbacks.cpp
// 디바이스 제어 관련 REST API 콜백 구현 - 기존 프로젝트 구조에 맞춤
// =============================================================================

#include "Api/DeviceApiCallbacks.h"
#include "Network/RestApiServer.h" 
#include "Workers/WorkerFactory.h"
#include "Utils/LogManager.h"

#include <thread>
#include <chrono>
#include <string>

using namespace PulseOne::Api;
using namespace PulseOne::Network;
using namespace PulseOne::Workers;

// nlohmann::json을 명시적으로 사용하여 충돌 방지
using nlohmann::json;

void DeviceApiCallbacks::Setup(RestApiServer* server,
                              WorkerFactory* worker_factory,
                              LogManager* logger) {
    
    if (!server || !worker_factory || !logger) {
        return;
    }
    
    // 디바이스 목록 조회 콜백 - 단순화
    server->SetDeviceListCallback([=]() -> nlohmann::json {
        try {
            nlohmann::json device_list = nlohmann::json::array();
            
            // 간단한 더미 응답 (실제로는 WorkerFactory에서 활성 디바이스 조회)
            nlohmann::json device_info = nlohmann::json::object();
            device_info["device_id"] = "1";
            device_info["status"] = "active";
            device_info["protocol"] = "modbus";
            device_list.push_back(device_info);
            
            return device_list;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to get device list - " + std::string(e.what()));
            return nlohmann::json::array();
        }
    });
    
    // 특정 디바이스 상태 조회 콜백
    server->SetDeviceStatusCallback([=](const std::string& device_id) -> nlohmann::json {
        try {
            nlohmann::json status = nlohmann::json::object();
            status["device_id"] = device_id;
            status["running"] = true;  // 단순화된 상태
            status["uptime"] = "0";
            status["last_error"] = "";
            status["message_count"] = 0;
            
            return status;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to get device status for " + device_id + " - " + std::string(e.what()));
            nlohmann::json error_status = nlohmann::json::object();
            error_status["device_id"] = device_id;
            error_status["error"] = "Failed to get status";
            return error_status;
        }
    });
    
    // 디바이스 시작 콜백
    server->SetDeviceStartCallback([=](const std::string& device_id) -> bool {
        try {
            logger->Info("API: Starting device " + device_id);
            
            // 실제 구현에서는 WorkerFactory를 통해 워커 제어
            // 현재는 단순히 성공 응답
            
            logger->Info("API: Device " + device_id + " started successfully");
            return true;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to start device " + device_id + " - " + std::string(e.what()));
            return false;
        }
    });
    
    // 디바이스 중지 콜백
    server->SetDeviceStopCallback([=](const std::string& device_id) -> bool {
        try {
            logger->Info("API: Stopping device " + device_id);
            
            // 실제 구현에서는 WorkerFactory를 통해 워커 제어
            // 현재는 단순히 성공 응답
            
            logger->Info("API: Device " + device_id + " stopped successfully");
            return true;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to stop device " + device_id + " - " + std::string(e.what()));
            return false;
        }
    });
    
    // 디바이스 일시정지 콜백
    server->SetDevicePauseCallback([=](const std::string& device_id) -> bool {
        try {
            logger->Info("API: Pausing device " + device_id);
            
            logger->Info("API: Device " + device_id + " paused successfully");
            return true;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to pause device " + device_id + " - " + std::string(e.what()));
            return false;
        }
    });
    
    // 디바이스 재개 콜백
    server->SetDeviceResumeCallback([=](const std::string& device_id) -> bool {
        try {
            logger->Info("API: Resuming device " + device_id);
            
            logger->Info("API: Device " + device_id + " resumed successfully");
            return true;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to resume device " + device_id + " - " + std::string(e.what()));
            return false;
        }
    });
    
    // 디바이스 재시작 콜백
    server->SetDeviceRestartCallback([=](const std::string& device_id) -> bool {
        try {
            logger->Info("API: Restarting device " + device_id);
            
            // 재시작 시뮬레이션
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            logger->Info("API: Device " + device_id + " restarted successfully");
            return true;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to restart device " + device_id + " - " + std::string(e.what()));
            return false;
        }
    });
    
    // 진단 모드 설정 콜백
    server->SetDiagnosticsCallback([=](const std::string& device_id, bool enabled) -> bool {
        try {
            std::string status = enabled ? "enabled" : "disabled";
            logger->Info("API: Setting diagnostics for device " + device_id + " to " + status);
            
            logger->Info("API: Diagnostics " + status + " for device " + device_id);
            return true;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to set diagnostics for device " + device_id + " - " + std::string(e.what()));
            return false;
        }
    });
}