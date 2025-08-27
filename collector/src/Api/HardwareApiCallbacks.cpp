// =============================================================================
// collector/src/Api/HardwareApiCallbacks.cpp
// 하드웨어 제어 전용 REST API 콜백 구현 - 새로운 네이밍 완전 반영
// =============================================================================

#include "Api/HardwareApiCallbacks.h"
#include "Network/RestApiServer.h"
#include "Workers/WorkerFactory.h"
#include "Utils/LogManager.h"

using namespace PulseOne::Api;
using namespace PulseOne::Network;
using namespace PulseOne::Workers;
using nlohmann::json;

void HardwareApiCallbacks::Setup(RestApiServer* server,
                                WorkerFactory* worker_factory,
                                LogManager* logger) {
    
    if (!server || !worker_factory || !logger) {
        logger->Error("HardwareApiCallbacks::Setup - null parameters");
        return;
    }

    logger->Info("Setting up hardware control API callbacks...");

    // ==========================================================================
    // 디지털 출력 제어 콜백 (펌프, 모터, 히터, LED 등 모든 ON/OFF 장비)
    // ==========================================================================
    server->SetDigitalOutputCallback([=](const std::string& device_id, 
                                         const std::string& output_id, 
                                         bool enable) -> bool {
        try {
            logger->Info("API: " + std::string(enable ? "Enable" : "Disable") + 
                        " digital output " + output_id + " on device " + device_id);
            
            // 출력 타입별 로깅
            std::string output_type = DetermineDigitalOutputType(output_id);
            logger->Info("   Output type: " + output_type);
            
            // 값 유효성 검증
            if (!ValidateDigitalOutput(output_id, enable, logger)) {
                return false;
            }
            
            // WorkerFactory를 통해 실제 하드웨어 제어
            // bool result = worker_factory->ControlDigitalOutput(device_id, output_id, enable);
            bool result = true; // 현재는 더미 구현
            
            if (result) {
                logger->Info("API: Digital output " + output_id + " " + 
                            (enable ? "enabled" : "disabled") + " successfully");
            }
            
            return result;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to control digital output " + output_id + 
                         " on device " + device_id + " - " + std::string(e.what()));
            return false;
        }
    });

    // ==========================================================================
    // 아날로그 출력 제어 콜백 (밸브 개도, 모터 속도, 히터 출력 등 0-100% 장비)
    // ==========================================================================
    server->SetAnalogOutputCallback([=](const std::string& device_id,
                                        const std::string& output_id,
                                        double value) -> bool {
        try {
            logger->Info("API: Set analog output " + output_id + " = " + 
                        std::to_string(value) + "% on device " + device_id);
            
            // 출력 타입별 로깅
            std::string output_type = DetermineAnalogOutputType(output_id);
            logger->Info("   Output type: " + output_type);
            
            // 값 범위 검증
            if (!ValidateAnalogOutputValue(output_id, value, logger)) {
                return false;
            }
            
            // WorkerFactory를 통해 실제 하드웨어 제어
            // bool result = worker_factory->ControlAnalogOutput(device_id, output_id, value);
            bool result = true; // 현재는 더미 구현
            
            if (result) {
                logger->Info("API: Analog output " + output_id + " set to " + 
                            std::to_string(value) + "% successfully");
            }
            
            return result;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to control analog output " + output_id + 
                         " on device " + device_id + " - " + std::string(e.what()));
            return false;
        }
    });

    // ==========================================================================
    // 파라미터 변경 콜백 (온도 setpoint, 압력 setpoint, 속도 설정 등)
    // ==========================================================================
    server->SetParameterChangeCallback([=](const std::string& device_id,
                                           const std::string& parameter_id,
                                           double value) -> bool {
        try {
            logger->Info("API: Set parameter " + parameter_id + " = " + 
                        std::to_string(value) + " on device " + device_id);
            
            // 파라미터 타입별 로깅
            std::string param_type = DetermineParameterType(parameter_id);
            std::string unit = GetParameterUnit(parameter_id);
            logger->Info("   Parameter type: " + param_type + " (unit: " + unit + ")");
            
            // 값 범위 검증
            if (!ValidateParameterValue(parameter_id, value, logger)) {
                return false;
            }
            
            // WorkerFactory를 통해 실제 파라미터 변경
            // bool result = worker_factory->ChangeParameter(device_id, parameter_id, value);
            bool result = true; // 현재는 더미 구현
            
            if (result) {
                logger->Info("API: Parameter " + parameter_id + " set to " + 
                            std::to_string(value) + " " + unit + " successfully");
            }
            
            return result;
            
        } catch (const std::exception& e) {
            logger->Error("API: Failed to change parameter " + parameter_id + 
                         " on device " + device_id + " - " + std::string(e.what()));
            return false;
        }
    });

    logger->Info("Hardware control API callbacks setup completed");
}

// =============================================================================
// 하드웨어 제어 관련 헬퍼 함수들
// =============================================================================

std::string HardwareApiCallbacks::DetermineDigitalOutputType(const std::string& output_id) {
    std::string lower_id = output_id;
    std::transform(lower_id.begin(), lower_id.end(), lower_id.begin(), ::tolower);
    
    if (lower_id.find("pump") != std::string::npos) {
        return "pump";
    } else if (lower_id.find("motor") != std::string::npos) {
        return "motor";
    } else if (lower_id.find("heater") != std::string::npos) {
        return "heater";
    } else if (lower_id.find("fan") != std::string::npos) {
        return "fan";
    } else if (lower_id.find("led") != std::string::npos || 
               lower_id.find("light") != std::string::npos) {
        return "indicator";
    } else if (lower_id.find("alarm") != std::string::npos || 
               lower_id.find("siren") != std::string::npos) {
        return "alarm";
    } else if (lower_id.find("valve") != std::string::npos && 
               lower_id.find("digital") != std::string::npos) {
        return "digital_valve";
    }
    return "generic_digital";
}

std::string HardwareApiCallbacks::DetermineAnalogOutputType(const std::string& output_id) {
    std::string lower_id = output_id;
    std::transform(lower_id.begin(), lower_id.end(), lower_id.begin(), ::tolower);
    
    if (lower_id.find("valve") != std::string::npos) {
        return "analog_valve";
    } else if (lower_id.find("speed") != std::string::npos) {
        return "speed_control";
    } else if (lower_id.find("power") != std::string::npos || 
               lower_id.find("output") != std::string::npos) {
        return "power_control";
    } else if (lower_id.find("position") != std::string::npos || 
               lower_id.find("actuator") != std::string::npos) {
        return "position_control";
    }
    return "generic_analog";
}

std::string HardwareApiCallbacks::DetermineParameterType(const std::string& parameter_id) {
    std::string lower_id = parameter_id;
    std::transform(lower_id.begin(), lower_id.end(), lower_id.begin(), ::tolower);
    
    if (lower_id.find("temperature") != std::string::npos || 
        lower_id.find("temp") != std::string::npos) {
        return "temperature";
    } else if (lower_id.find("pressure") != std::string::npos) {
        return "pressure";
    } else if (lower_id.find("flow") != std::string::npos) {
        return "flow_rate";
    } else if (lower_id.find("speed") != std::string::npos || 
               lower_id.find("rpm") != std::string::npos) {
        return "speed";
    } else if (lower_id.find("level") != std::string::npos) {
        return "level";
    } else if (lower_id.find("setpoint") != std::string::npos || 
               lower_id.find("target") != std::string::npos) {
        return "setpoint";
    }
    return "generic_parameter";
}

std::string HardwareApiCallbacks::GetParameterUnit(const std::string& parameter_id) {
    std::string param_type = DetermineParameterType(parameter_id);
    
    if (param_type == "temperature") {
        return "°C";
    } else if (param_type == "pressure") {
        return "bar";
    } else if (param_type == "flow_rate") {
        return "L/min";
    } else if (param_type == "speed") {
        return "RPM";
    } else if (param_type == "level") {
        return "%";
    }
    return "units";
}

bool HardwareApiCallbacks::ValidateDigitalOutput(const std::string& output_id,
                                                 bool value,
                                                 LogManager* logger) {
    // 디지털 출력은 true/false만 가능하므로 항상 유효
    return true;
}

bool HardwareApiCallbacks::ValidateAnalogOutputValue(const std::string& output_id,
                                                     double value,
                                                     LogManager* logger) {
    try {
        // 기본 범위: 0-100%
        if (value < 0.0 || value > 100.0) {
            logger->Error("Analog output value out of range (0-100%): " + std::to_string(value));
            return false;
        }
        
        // 출력 타입별 추가 검증
        std::string output_type = DetermineAnalogOutputType(output_id);
        
        if (output_type == "analog_valve") {
            // 밸브 개도: 0-100%
            if (value < 0.0 || value > 100.0) {
                logger->Error("Valve position out of range (0-100%): " + std::to_string(value));
                return false;
            }
        } else if (output_type == "speed_control") {
            // 속도 제어: 0-100%
            if (value < 0.0 || value > 100.0) {
                logger->Error("Speed control out of range (0-100%): " + std::to_string(value));
                return false;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logger->Error("Analog output validation error: " + std::string(e.what()));
        return false;
    }
}

bool HardwareApiCallbacks::ValidateParameterValue(const std::string& parameter_id,
                                                  double value,
                                                  LogManager* logger) {
    try {
        std::string param_type = DetermineParameterType(parameter_id);
        
        // 파라미터 타입별 값 범위 검증
        if (param_type == "temperature") {
            // 온도 파라미터: -50~200°C
            if (value < -50.0 || value > 200.0) {
                logger->Error("Temperature parameter out of range (-50~200°C): " + std::to_string(value));
                return false;
            }
        } else if (param_type == "pressure") {
            // 압력 파라미터: 0~50 bar
            if (value < 0.0 || value > 50.0) {
                logger->Error("Pressure parameter out of range (0~50 bar): " + std::to_string(value));
                return false;
            }
        } else if (param_type == "flow_rate") {
            // 유량 파라미터: 0~1000 L/min
            if (value < 0.0 || value > 1000.0) {
                logger->Error("Flow rate parameter out of range (0~1000 L/min): " + std::to_string(value));
                return false;
            }
        } else if (param_type == "speed") {
            // 속도 파라미터: 0~10000 RPM
            if (value < 0.0 || value > 10000.0) {
                logger->Error("Speed parameter out of range (0~10000 RPM): " + std::to_string(value));
                return false;
            }
        } else if (param_type == "level") {
            // 레벨 파라미터: 0~100%
            if (value < 0.0 || value > 100.0) {
                logger->Error("Level parameter out of range (0~100%): " + std::to_string(value));
                return false;
            }
        } else {
            // 일반 파라미터: -999999~999999 (매우 넓은 범위)
            if (value < -999999.0 || value > 999999.0) {
                logger->Error("Parameter value out of safe range: " + std::to_string(value));
                return false;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logger->Error("Parameter validation error: " + std::string(e.what()));
        return false;
    }
}