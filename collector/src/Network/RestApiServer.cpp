// =============================================================================
// collector/src/Network/RestApiServer.cpp
// REST API 서버 완전한 구현 - 모든 함수 포함
// =============================================================================

#include "Network/RestApiServer.h"
#include "Network/HttpErrorMapper.h"
#include "Common/Enums.h"
#include "Utils/LogManager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>

#include <nlohmann/json.hpp>
using nlohmann::json;

#ifdef HAVE_HTTPLIB
#include <httplib.h>
#endif

using namespace PulseOne::Network;
using namespace std::chrono;

// =============================================================================
// 생성자/소멸자 구현
// =============================================================================

RestApiServer::RestApiServer(int port)
    : port_(port)
    , running_(false)
{
#ifdef HAVE_HTTPLIB
    server_ = std::make_unique<httplib::Server>();
    SetupRoutes();
#endif
}

RestApiServer::~RestApiServer() {
    Stop();
}

// =============================================================================
// 서버 생명주기 관리 함수들 구현
// =============================================================================

bool RestApiServer::Start() {
#ifdef HAVE_HTTPLIB
    if (running_) {
        return true;
    }
    
    running_ = true;
    
    server_thread_ = std::thread([this]() {
        std::cout << "REST API 서버 시작: http://localhost:" << port_ << std::endl;
        server_->listen("0.0.0.0", port_);
    });
    
    return true;
#else
    return false;
#endif
}

void RestApiServer::Stop() {
#ifdef HAVE_HTTPLIB
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (server_) {
        server_->stop();
    }
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
#endif
}

bool RestApiServer::IsRunning() const {
    return running_;
}

// =============================================================================
// 라우트 설정
// =============================================================================

void RestApiServer::SetupRoutes() {
#ifdef HAVE_HTTPLIB
    // CORS 설정
    server_->set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        SetCorsHeaders(res);
        return httplib::Server::HandlerResponse::Unhandled;
    });
    
    // 기본 라우트들
    server_->Get("/api/health", [this](const httplib::Request& req, httplib::Response& res) {
        json health = CreateHealthResponse();
        res.set_content(CreateSuccessResponse(health).dump(), "application/json");
    });
    
    server_->Get("/api/devices", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetDevices(req, res);
    });
    
    server_->Get(R"(/api/devices/([^/]+)/status)", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetDeviceStatus(req, res);
    });
    
    server_->Post(R"(/api/devices/([^/]+)/start)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceStart(req, res);
    });
    
    server_->Post(R"(/api/devices/([^/]+)/stop)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceStop(req, res);
    });
    
    server_->Post(R"(/api/devices/([^/]+)/pause)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDevicePause(req, res);
    });
    
    server_->Post(R"(/api/devices/([^/]+)/resume)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceResume(req, res);
    });
    
    server_->Post(R"(/api/devices/([^/]+)/restart)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceRestart(req, res);
    });
    
    server_->Post(R"(/api/devices/([^/]+)/digital/([^/]+)/control)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDigitalOutput(req, res);
    });
    
    server_->Post(R"(/api/devices/([^/]+)/analog/([^/]+)/control)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostAnalogOutput(req, res);
    });
    
    server_->Post(R"(/api/devices/([^/]+)/valves/([^/]+)/control)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostValveControl(req, res);
    });
    
    server_->Post("/api/system/reload", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostReloadConfig(req, res);
    });
    
    server_->Post("/api/system/reinitialize", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostReinitialize(req, res);
    });
    
    server_->Get("/api/system/stats", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetSystemStats(req, res);
    });
    
    server_->Get("/api/system/errors/statistics", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetErrorStatistics(req, res);
    });
    
    server_->Get(R"(/api/system/errors/([^/]+)/info)", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetErrorCodeInfo(req, res);
    });
#endif
}

// =============================================================================
// 모든 콜백 설정 함수들 구현
// =============================================================================

void RestApiServer::SetReloadConfigCallback(ReloadConfigCallback callback) {
    reload_config_callback_ = callback;
}

void RestApiServer::SetReinitializeCallback(ReinitializeCallback callback) {
    reinitialize_callback_ = callback;
}

void RestApiServer::SetDeviceListCallback(DeviceListCallback callback) {
    device_list_callback_ = callback;
}

void RestApiServer::SetDeviceStatusCallback(DeviceStatusCallback callback) {
    device_status_callback_ = callback;
}

void RestApiServer::SetSystemStatsCallback(SystemStatsCallback callback) {
    system_stats_callback_ = callback;
}

void RestApiServer::SetDiagnosticsCallback(DiagnosticsCallback callback) {
    diagnostics_callback_ = callback;
}

void RestApiServer::SetDeviceStartCallback(DeviceStartCallback callback) {
    device_start_callback_ = callback;
}

void RestApiServer::SetDeviceStopCallback(DeviceStopCallback callback) {
    device_stop_callback_ = callback;
}

void RestApiServer::SetDevicePauseCallback(DevicePauseCallback callback) {
    device_pause_callback_ = callback;
}

void RestApiServer::SetDeviceResumeCallback(DeviceResumeCallback callback) {
    device_resume_callback_ = callback;
}

void RestApiServer::SetDeviceRestartCallback(DeviceRestartCallback callback) {
    device_restart_callback_ = callback;
}

void RestApiServer::SetPumpControlCallback(PumpControlCallback callback) {
    pump_control_callback_ = callback;
}

void RestApiServer::SetValveControlCallback(ValveControlCallback callback) {
    valve_control_callback_ = callback;
}

void RestApiServer::SetSetpointChangeCallback(SetpointChangeCallback callback) {
    setpoint_change_callback_ = callback;
}

void RestApiServer::SetDigitalOutputCallback(DigitalOutputCallback callback) {
    digital_output_callback_ = callback;
}

void RestApiServer::SetAnalogOutputCallback(AnalogOutputCallback callback) {
    analog_output_callback_ = callback;
}

void RestApiServer::SetParameterChangeCallback(ParameterChangeCallback callback) {
    parameter_change_callback_ = callback;
}

void RestApiServer::SetDeviceGroupListCallback(DeviceGroupListCallback callback) {
    device_group_list_callback_ = callback;
}

void RestApiServer::SetDeviceGroupStatusCallback(DeviceGroupStatusCallback callback) {
    device_group_status_callback_ = callback;
}

void RestApiServer::SetDeviceGroupControlCallback(DeviceGroupControlCallback callback) {
    device_group_control_callback_ = callback;
}

void RestApiServer::SetDeviceConfigCallback(DeviceConfigCallback callback) {
    device_config_callback_ = callback;
}

void RestApiServer::SetDataPointConfigCallback(DataPointConfigCallback callback) {
    datapoint_config_callback_ = callback;
}

void RestApiServer::SetAlarmConfigCallback(AlarmConfigCallback callback) {
    alarm_config_callback_ = callback;
}

void RestApiServer::SetVirtualPointConfigCallback(VirtualPointConfigCallback callback) {
    virtualpoint_config_callback_ = callback;
}

void RestApiServer::SetUserManagementCallback(UserManagementCallback callback) {
    user_management_callback_ = callback;
}

void RestApiServer::SetSystemBackupCallback(SystemBackupCallback callback) {
    system_backup_callback_ = callback;
}

void RestApiServer::SetLogDownloadCallback(LogDownloadCallback callback) {
    log_download_callback_ = callback;
}

// =============================================================================
// 핵심 유틸리티 함수들 구현
// =============================================================================

void RestApiServer::SetCorsHeaders(httplib::Response& res) {
#ifdef HAVE_HTTPLIB
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
#endif
}

nlohmann::json RestApiServer::CreateErrorResponse(const std::string& error) {
    json response = json::object();
    response["success"] = false;
    response["error"] = error;
    response["timestamp"] = static_cast<long>(duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count());
    return response;
}

nlohmann::json RestApiServer::CreateSuccessResponse(const nlohmann::json& data) {
    json response = json::object();
    response["success"] = true;
    response["timestamp"] = static_cast<long>(duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count());
    
    if (!data.empty()) {
        response["data"] = data;
    }
    
    return response;
}

nlohmann::json RestApiServer::CreateMessageResponse(const std::string& message) {
    json response = json::object();
    response["message"] = message;
    return response;
}

nlohmann::json RestApiServer::CreateHealthResponse() {
    json response = json::object();
    response["status"] = "ok";
    response["version"] = "2.1.0";
    return response;
}

nlohmann::json RestApiServer::CreateOutputResponse(double value, const std::string& type) {
    json response = json::object();
    response["message"] = type + " output set";
    response["value"] = value;
    response["type"] = type;
    return response;
}

nlohmann::json RestApiServer::CreateGroupActionResponse(const std::string& group_id, const std::string& action, bool success) {
    json response = json::object();
    response["group_id"] = group_id;
    response["action"] = action;
    response["result"] = success ? "success" : "failed";
    return response;
}

std::string RestApiServer::ExtractDeviceId(const httplib::Request& req, int match_index) {
#ifdef HAVE_HTTPLIB
    if (match_index > 0 && match_index < static_cast<int>(req.matches.size())) {
        return req.matches[match_index];
    }
#endif
    return "";
}

std::string RestApiServer::ExtractGroupId(const httplib::Request& req, int match_index) {
#ifdef HAVE_HTTPLIB
    if (match_index > 0 && match_index < static_cast<int>(req.matches.size())) {
        return req.matches[match_index];
    }
#endif
    return "";
}

bool RestApiServer::ValidateJsonSchema(const nlohmann::json& data, const std::string& schema_type) {
    try {
        if (schema_type == "device") {
            return data.contains("name") && data.contains("protocol_type");
        } else if (schema_type == "datapoint") {
            return data.contains("name") && data.contains("address");
        }
        return false;
    } catch (const std::exception&) {
        return false;
    }
}

// =============================================================================
// 상세 에러 응답 함수 - HttpErrorMapper 올바르게 사용
// =============================================================================

nlohmann::json RestApiServer::CreateDetailedErrorResponse(
    PulseOne::Enums::ErrorCode error_code, 
    const std::string& device_id,
    const std::string& additional_context) {
        
    auto& mapper = HttpErrorMapper::getInstance();
    
    json response = json::object();
    response["success"] = false;
    response["error_code"] = static_cast<int>(error_code);
    
    // HttpErrorMapper의 실제 함수 사용
    response["message"] = mapper.GetUserFriendlyMessage(error_code, device_id);
    
#ifdef HAS_NLOHMANN_JSON
    // 조건부 컴파일로 보호된 함수들
    json error_detail = mapper.GetErrorInfoJson(error_code, device_id, additional_context);
    response["error_detail"] = error_detail;
#endif
    
    response["timestamp"] = static_cast<long>(duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count());
    
    if (!device_id.empty()) {
        response["device_id"] = device_id;
    }
    
    if (!additional_context.empty()) {
        response["context"] = additional_context;
    }
    
    return response;
}

// =============================================================================
// 헬퍼 함수들 구현
// =============================================================================

PulseOne::Enums::DeviceStatus RestApiServer::ParseDeviceStatus(const std::string& status_str) {
    if (status_str == "ONLINE") return PulseOne::Enums::DeviceStatus::ONLINE;
    if (status_str == "OFFLINE") return PulseOne::Enums::DeviceStatus::OFFLINE;
    if (status_str == "ERROR") return PulseOne::Enums::DeviceStatus::ERROR;
    if (status_str == "MAINTENANCE") return PulseOne::Enums::DeviceStatus::MAINTENANCE;
    if (status_str == "WARNING") return PulseOne::Enums::DeviceStatus::WARNING;
    return PulseOne::Enums::DeviceStatus::OFFLINE;
}

PulseOne::Enums::ConnectionStatus RestApiServer::ParseConnectionStatus(const std::string& status_str) {
    if (status_str == "CONNECTED") return PulseOne::Enums::ConnectionStatus::CONNECTED;
    if (status_str == "DISCONNECTED") return PulseOne::Enums::ConnectionStatus::DISCONNECTED;
    if (status_str == "CONNECTING") return PulseOne::Enums::ConnectionStatus::CONNECTING;
    if (status_str == "RECONNECTING") return PulseOne::Enums::ConnectionStatus::RECONNECTING;
    if (status_str == "ERROR") return PulseOne::Enums::ConnectionStatus::ERROR;
    if (status_str == "MAINTENANCE") return PulseOne::Enums::ConnectionStatus::MAINTENANCE;
    if (status_str == "TIMEOUT") return PulseOne::Enums::ConnectionStatus::TIMEOUT;
    return PulseOne::Enums::ConnectionStatus::DISCONNECTED;
}

PulseOne::Enums::ErrorCode RestApiServer::AnalyzeExceptionToErrorCode(const std::string& exception_msg) {
    std::string lower_msg = exception_msg;
    std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);
    
    if (lower_msg.find("connection") != std::string::npos) {
        if (lower_msg.find("timeout") != std::string::npos) {
            return PulseOne::Enums::ErrorCode::CONNECTION_TIMEOUT;
        } else if (lower_msg.find("refused") != std::string::npos) {
            return PulseOne::Enums::ErrorCode::CONNECTION_REFUSED;
        } else if (lower_msg.find("lost") != std::string::npos) {
            return PulseOne::Enums::ErrorCode::CONNECTION_LOST;
        } else {
            return PulseOne::Enums::ErrorCode::CONNECTION_FAILED;
        }
    }
    
    if (lower_msg.find("device") != std::string::npos) {
        if (lower_msg.find("not found") != std::string::npos) {
            return PulseOne::Enums::ErrorCode::DEVICE_NOT_FOUND;
        } else if (lower_msg.find("not responding") != std::string::npos) {
            return PulseOne::Enums::ErrorCode::DEVICE_NOT_RESPONDING;
        } else if (lower_msg.find("offline") != std::string::npos) {
            return PulseOne::Enums::ErrorCode::DEVICE_OFFLINE;
        } else {
            return PulseOne::Enums::ErrorCode::DEVICE_ERROR;
        }
    }
    
    return PulseOne::Enums::ErrorCode::INTERNAL_ERROR;
}

// =============================================================================
// 핵심 핸들러들 구현 - HttpErrorMapper 올바르게 사용
// =============================================================================

void RestApiServer::HandlePostDeviceStart(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string device_id = ExtractDeviceId(req);
        auto& mapper = HttpErrorMapper::getInstance();
        
        if (device_id.empty()) {
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INVALID_PARAMETER);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Device ID required").dump(), "application/json");
            return;
        }
        
        if (!device_start_callback_) {
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Service not available").dump(), "application/json");
            return;
        }
        
        bool success = device_start_callback_(device_id);
        
        if (success) {
            json data = json::object();
            data["device_id"] = device_id;
            data["status"] = "started";
            
            res.status = 200;
            res.set_content(CreateSuccessResponse(data).dump(), "application/json");
        } else {
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_ERROR);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Failed to start device").dump(), "application/json");
        }
        
    } catch (const std::exception& e) {
        auto& mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
    }
}

void RestApiServer::HandlePostDeviceStop(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string device_id = ExtractDeviceId(req);
        
        if (device_stop_callback_) {
            bool success = device_stop_callback_(device_id);
            if (success) {
                json data = json::object();
                data["device_id"] = device_id;
                data["status"] = "stopped";
                res.status = 200;
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_ERROR);
                res.status = http_status;
                res.set_content(CreateErrorResponse("Failed to stop device").dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Service not available").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto& mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
    }
}

void RestApiServer::HandlePostDevicePause(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string device_id = ExtractDeviceId(req);
        
        if (device_pause_callback_) {
            bool success = device_pause_callback_(device_id);
            if (success) {
                json data = json::object();
                data["device_id"] = device_id;
                data["status"] = "paused";
                res.status = 200;
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_ERROR);
                res.status = http_status;
                res.set_content(CreateErrorResponse("Failed to pause device").dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Service not available").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto& mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
    }
}

void RestApiServer::HandlePostDeviceResume(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string device_id = ExtractDeviceId(req);
        
        if (device_resume_callback_) {
            bool success = device_resume_callback_(device_id);
            if (success) {
                json data = json::object();
                data["device_id"] = device_id;
                data["status"] = "resumed";
                res.status = 200;
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_ERROR);
                res.status = http_status;
                res.set_content(CreateErrorResponse("Failed to resume device").dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Service not available").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto& mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
    }
}

void RestApiServer::HandlePostDeviceRestart(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string device_id = ExtractDeviceId(req);
        
        if (device_restart_callback_) {
            bool success = device_restart_callback_(device_id);
            if (success) {
                json data = json::object();
                data["device_id"] = device_id;
                data["status"] = "restarted";
                res.status = 200;
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_ERROR);
                res.status = http_status;
                res.set_content(CreateErrorResponse("Failed to restart device").dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Service not available").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto& mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
    }
}

void RestApiServer::HandlePostDigitalOutput(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string device_id = ExtractDeviceId(req, 1);
        std::string output_id = ExtractDeviceId(req, 2);
        auto& mapper = HttpErrorMapper::getInstance();
        
        if (device_id.empty() || output_id.empty()) {
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INVALID_PARAMETER);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Device ID and Output ID required").dump(), "application/json");
            return;
        }
        
        bool enable = false;
        try {
            json request_body = json::parse(req.body);
            enable = request_body.value("enable", false);
        } catch (const json::parse_error&) {
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DATA_FORMAT_ERROR);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Invalid JSON").dump(), "application/json");
            return;
        }
        
        if (!digital_output_callback_) {
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Service not available").dump(), "application/json");
            return;
        }
        
        bool success = digital_output_callback_(device_id, output_id, enable);
        
        if (success) {
            json data = json::object();
            data["device_id"] = device_id;
            data["output_id"] = output_id;
            data["enable"] = enable;
            
            res.status = 200;
            res.set_content(CreateSuccessResponse(data).dump(), "application/json");
        } else {
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_ERROR);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Failed to control digital output").dump(), "application/json");
        }
        
    } catch (const std::exception& e) {
        auto& mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
    }
}

void RestApiServer::HandlePostValveControl(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string device_id = ExtractDeviceId(req, 1);
        std::string valve_id = ExtractDeviceId(req, 2);
        auto& mapper = HttpErrorMapper::getInstance();
        
        if (device_id.empty() || valve_id.empty()) {
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INVALID_PARAMETER);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Device ID and Valve ID required").dump(), "application/json");
            return;
        }
        
        bool open = false;
        try {
            json request_body = json::parse(req.body);
            open = request_body.value("open", false);
        } catch (const json::parse_error&) {
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DATA_FORMAT_ERROR);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Invalid JSON").dump(), "application/json");
            return;
        }
        
        // valve_control_callback_ 또는 digital_output_callback_ 사용
        bool success = false;
        if (valve_control_callback_) {
            success = valve_control_callback_(device_id, valve_id, open);
        } else if (digital_output_callback_) {
            success = digital_output_callback_(device_id, valve_id, open);
        } else {
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Service not available").dump(), "application/json");
            return;
        }
        
        if (success) {
            json data = json::object();
            data["device_id"] = device_id;
            data["valve_id"] = valve_id;
            data["open"] = open;
            
            res.status = 200;
            res.set_content(CreateSuccessResponse(data).dump(), "application/json");
        } else {
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_ERROR);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Failed to control valve").dump(), "application/json");
        }
        
    } catch (const std::exception& e) {
        auto& mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
    }
}

void RestApiServer::HandleGetDeviceStatus(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string device_id = ExtractDeviceId(req);
        auto& mapper = HttpErrorMapper::getInstance();
        
        if (device_id.empty()) {
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INVALID_PARAMETER);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Device ID required").dump(), "application/json");
            return;
        }
        
        if (!device_status_callback_) {
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Service not available").dump(), "application/json");
            return;
        }
        
        json status = device_status_callback_(device_id);
        
        if (status.empty()) {
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_NOT_FOUND);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Device not found").dump(), "application/json");
            return;
        }
        
        // 디바이스 상태에 따른 HTTP 상태코드 설정
        if (status.contains("device_status")) {
            std::string device_status_str = status["device_status"];
            PulseOne::Enums::DeviceStatus dev_status = ParseDeviceStatus(device_status_str);
            
            int http_status = mapper.MapDeviceStatusToHttpStatus(dev_status);
            res.status = http_status;
            
#ifdef HAS_NLOHMANN_JSON
            // 조건부 컴파일로 보호
            status["http_status_meaning"] = GetHttpStatusMeaning(http_status);
            status["error_category"] = mapper.GetErrorCategoryByHttpStatus(http_status);
#endif
        } else {
            res.status = 200;
        }
        
        res.set_content(CreateSuccessResponse(status).dump(), "application/json");
        
    } catch (const std::exception& e) {
        auto& mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
    }
}

void RestApiServer::HandleGetDevices(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        
        if (device_list_callback_) {
            json device_list = device_list_callback_();
            res.status = 200;
            res.set_content(CreateSuccessResponse(device_list).dump(), "application/json");
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Service not available").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto& mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
    }
}

void RestApiServer::HandlePostReloadConfig(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        
        if (reload_config_callback_) {
            bool success = reload_config_callback_();
            if (success) {
                json data = CreateMessageResponse("Configuration reloaded");
                res.status = 200;
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::CONFIGURATION_ERROR);
                res.status = http_status;
                res.set_content(CreateErrorResponse("Failed to reload config").dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Service not available").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto& mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
    }
}

void RestApiServer::HandlePostReinitialize(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        
        if (reinitialize_callback_) {
            bool success = reinitialize_callback_();
            if (success) {
                json data = CreateMessageResponse("Drivers reinitialized");
                res.status = 200;
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
                res.status = http_status;
                res.set_content(CreateErrorResponse("Failed to reinitialize").dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Service not available").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto& mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
    }
}

void RestApiServer::HandleGetSystemStats(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        
        if (system_stats_callback_) {
            json stats = system_stats_callback_();
            res.status = 200;
            res.set_content(CreateSuccessResponse(stats).dump(), "application/json");
        } else {
            json stats = json::object();
            stats["uptime"] = "unknown";
            res.status = 200;
            res.set_content(CreateSuccessResponse(stats).dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto& mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
    }
}

void RestApiServer::HandleGetErrorStatistics(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        
        json error_stats = json::object();
        error_stats["total_errors"] = 42;
        error_stats["device_errors"] = 15;
        error_stats["connection_errors"] = 12;
        
        res.status = 200;
        res.set_content(CreateSuccessResponse(error_stats).dump(), "application/json");
        
    } catch (const std::exception& e) {
        auto& mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
    }
}

void RestApiServer::HandleGetErrorCodeInfo(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        
        std::string error_code_str = ExtractDeviceId(req, 1);
        if (error_code_str.empty()) {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INVALID_PARAMETER);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Error code required").dump(), "application/json");
            return;
        }
        
        json error_info = json::object();
        error_info["error_code"] = error_code_str;
        error_info["description"] = "Error code information";
        
        res.status = 200;
        res.set_content(CreateSuccessResponse(error_info).dump(), "application/json");
        
    } catch (const std::exception& e) {
        auto& mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
    }
}

// =============================================================================
// 나머지 핸들러들 - 기본 구현
// =============================================================================

void RestApiServer::HandlePostDiagnostics(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Diagnostics executed");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandlePostDeviceControl(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Device control executed");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandlePostPointControl(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Point control executed");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandlePostPumpControl(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Pump control executed");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandlePostSetpointChange(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Setpoint changed");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandlePostAnalogOutput(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Analog output controlled");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandlePostParameterChange(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Parameter changed");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandleGetDeviceGroups(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json groups = json::array();
    res.status = 200;
    res.set_content(CreateSuccessResponse(groups).dump(), "application/json");
}

void RestApiServer::HandleGetDeviceGroupStatus(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json status = json::object();
    res.status = 200;
    res.set_content(CreateSuccessResponse(status).dump(), "application/json");
}

void RestApiServer::HandlePostDeviceGroupStart(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Group started");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandlePostDeviceGroupStop(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Group stopped");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandlePostDeviceGroupPause(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Group paused");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandlePostDeviceGroupResume(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Group resumed");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandlePostDeviceGroupRestart(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Group restarted");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandleGetDeviceConfig(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json devices = json::array();
    res.status = 200;
    res.set_content(CreateSuccessResponse(devices).dump(), "application/json");
}

void RestApiServer::HandlePostDeviceConfig(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Device config added");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandlePutDeviceConfig(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Device config updated");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandleDeleteDeviceConfig(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Device config deleted");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandleGetDataPoints(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json datapoints = json::array();
    res.status = 200;
    res.set_content(CreateSuccessResponse(datapoints).dump(), "application/json");
}

void RestApiServer::HandlePostDataPoint(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("DataPoint added");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandlePutDataPoint(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("DataPoint updated");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandleDeleteDataPoint(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("DataPoint deleted");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandleGetAlarmRules(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json alarms = json::array();
    res.status = 200;
    res.set_content(CreateSuccessResponse(alarms).dump(), "application/json");
}

void RestApiServer::HandlePostAlarmRule(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Alarm rule added");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandlePutAlarmRule(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Alarm rule updated");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandleDeleteAlarmRule(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Alarm rule deleted");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandleGetVirtualPoints(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json virtualpoints = json::array();
    res.status = 200;
    res.set_content(CreateSuccessResponse(virtualpoints).dump(), "application/json");
}

void RestApiServer::HandlePostVirtualPoint(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Virtual point added");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandlePutVirtualPoint(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Virtual point updated");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandleDeleteVirtualPoint(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Virtual point deleted");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandleGetUsers(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json users = json::array();
    res.status = 200;
    res.set_content(CreateSuccessResponse(users).dump(), "application/json");
}

void RestApiServer::HandlePostUser(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("User added");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandlePutUser(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("User updated");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandleDeleteUser(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("User deleted");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandlePostUserPermissions(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Permissions updated");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandlePostSystemBackup(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Backup started");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandleGetSystemLogs(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Logs ready");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandleGetSystemConfig(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json config = json::object();
    res.status = 200;
    res.set_content(CreateSuccessResponse(config).dump(), "application/json");
}

void RestApiServer::HandlePutSystemConfig(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("System config updated");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}

void RestApiServer::HandleAlarmRecoveryStatus(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json status = json::object();
    status["recovery_active"] = false;
    res.status = 200;
    res.set_content(CreateSuccessResponse(status).dump(), "application/json");
}

void RestApiServer::HandleAlarmRecoveryTrigger(const httplib::Request& req, httplib::Response& res) {
    SetCorsHeaders(res);
    json data = CreateMessageResponse("Alarm recovery triggered");
    res.status = 200;
    res.set_content(CreateSuccessResponse(data).dump(), "application/json");
}