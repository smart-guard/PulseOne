// =============================================================================
// collector/src/Api/HardwareApiCallbacks.cpp
// 하드웨어 제어 REST API 콜백 - 싱글톤 직접 접근 방식으로 수정
// =============================================================================

#include "Api/HardwareApiCallbacks.h"
#include "Network/RestApiServer.h"
#include "Workers/WorkerManager.h"
#include "Logging/LogManager.h"

using namespace PulseOne::Api;
using namespace PulseOne::Network;
using nlohmann::json;

void HardwareApiCallbacks::Setup(RestApiServer* server) {
    
    if (!server) {
        LogManager::getInstance().Error("HardwareApiCallbacks::Setup - null server parameter");
        return;
    }

    LogManager::getInstance().Info("Setting up hardware control API callbacks...");

    // ==========================================================================
    // 디지털 출력 제어 콜백 (펌프, 모터, 밸브 등 ON/OFF 제어)
    // ==========================================================================
    
    server->SetDigitalOutputCallback([=](const std::string& device_id, 
                                         const std::string& output_id, 
                                         bool enable) -> bool {
        try {
            LogManager::getInstance().Info("API: " + std::string(enable ? "Enable" : "Disable") + 
                        " digital output " + output_id + " on device " + device_id);
            
            // WorkerManager를 통해 실제 하드웨어 제어
            auto& worker_mgr = Workers::WorkerManager::getInstance();
            
            // 디바이스 워커가 실행중인지 확인
            if (!worker_mgr.IsWorkerRunning(device_id)) {
                LogManager::getInstance().Error("Device " + device_id + " is not running");
                return false;
            }
            
            // 실제 디지털 출력 제어 (현재는 더미 구현)
            bool result = worker_mgr.WriteDataPoint(device_id, output_id, enable ? "1" : "0");
            
            if (result) {
                LogManager::getInstance().Info("Digital output " + output_id + " " + 
                            (enable ? "enabled" : "disabled") + " on device " + device_id);
            } else {
                LogManager::getInstance().Error("Failed to control digital output " + output_id);
                return false;
            }
            
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("API: Failed to control digital output " + output_id + 
                         " on device " + device_id + " - " + std::string(e.what()));
            return false;
        }
    });

    // ==========================================================================
    // 아날로그 출력 제어 콜백 (밸브 개도, 모터 속도 등 0-100% 제어)
    // ==========================================================================
    
    server->SetAnalogOutputCallback([=](const std::string& device_id,
                                        const std::string& output_id,
                                        double value) -> bool {
        try {
            LogManager::getInstance().Info("API: Set analog output " + output_id + " = " + 
                        std::to_string(value) + "% on device " + device_id);
            
            // 값 범위 검증
            if (!ValidateAnalogValue(value)) {
                LogManager::getInstance().Error("Analog output value out of range (0-100%): " + 
                                               std::to_string(value));
                return false;
            }
            
            // WorkerManager를 통해 실제 하드웨어 제어
            auto& worker_mgr = Workers::WorkerManager::getInstance();
            
            // 디바이스 워커가 실행중인지 확인
            if (!worker_mgr.IsWorkerRunning(device_id)) {
                LogManager::getInstance().Error("Device " + device_id + " is not running");
                return false;
            }
            
            // 실제 아날로그 출력 제어 (현재는 더미 구현)
            bool result = worker_mgr.WriteDataPoint(device_id, output_id, std::to_string(value));
            
            if (result) {
                LogManager::getInstance().Info("Analog output " + output_id + " set to " + 
                            std::to_string(value) + "% on device " + device_id);
            } else {
                LogManager::getInstance().Error("Failed to control analog output " + output_id);
                return false;
            }
            
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("API: Failed to control analog output " + output_id + 
                         " on device " + device_id + " - " + std::string(e.what()));
            return false;
        }
    });

    // ==========================================================================
    // 파라미터 변경 콜백 (온도/압력 설정값 등)
    // ==========================================================================
    
    server->SetParameterChangeCallback([=](const std::string& device_id,
                                           const std::string& parameter_id,
                                           double value) -> bool {
        try {
            LogManager::getInstance().Info("API: Set parameter " + parameter_id + " = " + 
                        std::to_string(value) + " on device " + device_id);
            
            // 값 범위 검증 (기본 안전 범위)
            if (!ValidateParameterValue(value)) {
                LogManager::getInstance().Error("Parameter value out of safe range: " + 
                                               std::to_string(value));
                return false;
            }
            
            // WorkerManager를 통해 실제 파라미터 변경
            auto& worker_mgr = Workers::WorkerManager::getInstance();
            
            // 디바이스 워커가 실행중인지 확인
            if (!worker_mgr.IsWorkerRunning(device_id)) {
                LogManager::getInstance().Error("Device " + device_id + " is not running");
                return false;
            }
            
            // 실제 파라미터 변경 (현재는 더미 구현)
            bool result = worker_mgr.WriteDataPoint(device_id, parameter_id, std::to_string(value));
            
            if (result) {
                LogManager::getInstance().Info("Parameter " + parameter_id + " set to " + 
                            std::to_string(value) + " on device " + device_id);
            } else {
                LogManager::getInstance().Error("Failed to change parameter " + parameter_id);
                return false;
            }
            
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("API: Failed to change parameter " + parameter_id + 
                         " on device " + device_id + " - " + std::string(e.what()));
            return false;
        }
    });

    LogManager::getInstance().Info("Hardware control API callbacks setup completed");
}

// =============================================================================
// 값 검증 헬퍼 함수들 - 간단하고 실용적인 검증
// =============================================================================

bool HardwareApiCallbacks::ValidateAnalogValue(double value) {
    // 아날로그 출력: 0-100% 범위
    return (value >= 0.0 && value <= 100.0);
}

bool HardwareApiCallbacks::ValidateParameterValue(double value) {
    // 파라미터: 기본 안전 범위 (-10000 ~ 10000)
    // 실제 운영에서는 각 파라미터별로 더 구체적인 범위 설정 필요
    return (value >= -10000.0 && value <= 10000.0);
}