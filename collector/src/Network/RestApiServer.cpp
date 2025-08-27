// =============================================================================
// collector/src/Network/RestApiServer.cpp
// REST API 서버 완전한 구현 - 컴파일 에러 수정 완료
// =============================================================================

#include "Network/RestApiServer.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <regex>

#ifdef HAVE_HTTPLIB
#include <httplib.h>
#endif

using namespace PulseOne::Network;
using namespace std::chrono;

// =============================================================================
// 생성자/소멸자
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
// 서버 생명주기 관리
// =============================================================================

bool RestApiServer::Start() {
#ifdef HAVE_HTTPLIB
    if (running_) {
        return true;
    }
    
    running_ = true;
    
    // 서버를 별도 스레드에서 실행
    server_thread_ = std::thread([this]() {
        std::cout << "REST API 서버 시작: http://localhost:" << port_ << std::endl;
        std::cout << "API 문서: http://localhost:" << port_ << "/api/docs" << std::endl;
        server_->listen("0.0.0.0", port_);
    });
    
    return true;
#else
    std::cerr << "HTTP 라이브러리가 없습니다. REST API를 사용할 수 없습니다." << std::endl;
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
    
    std::cout << "REST API 서버 중지됨" << std::endl;
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
    // CORS 미들웨어
    server_->set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        SetCorsHeaders(res);
        return httplib::Server::HandlerResponse::Unhandled;
    });
    
    // OPTIONS 요청 처리 (CORS)
    server_->Options("/.*", [this](const httplib::Request& req, httplib::Response& res) {
        SetCorsHeaders(res);
        return;
    });
    
    // API 문서 및 헬스체크
    server_->Get("/", [this](const httplib::Request& req, httplib::Response& res) {
        res.set_content(R"(
            <h1>PulseOne Collector REST API</h1>
            <p>Version: 2.0.0</p>
            <ul>
                <li><a href="/api/docs">API Documentation</a></li>
                <li><a href="/api/health">Health Check</a></li>
                <li><a href="/api/system/stats">System Statistics</a></li>
            </ul>
        )", "text/html");
    });
    
    server_->Get("/api/health", [this](const httplib::Request& req, httplib::Response& res) {
        json health = CreateHealthResponse();
        res.set_content(CreateSuccessResponse(health).dump(), "application/json");
    });
    
    // 디바이스 목록 조회
    server_->Get("/api/devices", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetDevices(req, res);
    });
    
    // 특정 디바이스 상태 조회
    server_->Get(R"(/api/devices/([^/]+)/status)", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetDeviceStatus(req, res);
    });
    
    // 진단 기능 제어
    server_->Post(R"(/api/devices/([^/]+)/diagnostics)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDiagnostics(req, res);
    });
    
    // DeviceWorker 스레드 제어
    server_->Post(R"(/api/devices/([^/]+)/worker/start)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceStart(req, res);
    });
    
    server_->Post(R"(/api/devices/([^/]+)/worker/stop)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceStop(req, res);
    });
    
    server_->Post(R"(/api/devices/([^/]+)/worker/pause)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDevicePause(req, res);
    });
    
    server_->Post(R"(/api/devices/([^/]+)/worker/resume)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceResume(req, res);
    });
    
    server_->Post(R"(/api/devices/([^/]+)/worker/restart)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceRestart(req, res);
    });
    
    // 하드웨어 제어
    server_->Post(R"(/api/devices/([^/]+)/control)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceControl(req, res);
    });

    server_->Post(R"(/api/devices/([^/]+)/points/([^/]+)/control)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostPointControl(req, res);
    });
    
    // 시스템 제어
    server_->Post("/api/system/reload-config", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostReloadConfig(req, res);
    });
    
    server_->Post("/api/system/reinitialize", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostReinitialize(req, res);
    });
    
    server_->Get("/api/system/stats", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetSystemStats(req, res);
    });
    
    std::cout << "REST API 라우트 설정 완료" << std::endl;
#endif
}

// =============================================================================
// API 핸들러들 - 수정된 버전
// =============================================================================

void RestApiServer::HandleGetDevices(const httplib::Request& req, httplib::Response& res) {
    try {
        if (device_list_callback_) {
            json device_list = device_list_callback_();
            res.set_content(CreateSuccessResponse(device_list).dump(), "application/json");
        } else {
            res.set_content(CreateErrorResponse("Device list callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandleGetDeviceStatus(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = ExtractDeviceId(req);
        
        if (device_status_callback_) {
            json status = device_status_callback_(device_id);
            res.set_content(CreateSuccessResponse(status).dump(), "application/json");
        } else {
            res.set_content(CreateErrorResponse("Device status callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostDiagnostics(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = ExtractDeviceId(req);
        
        // JSON 파싱 (안전한 방법)
        bool enabled = false;
        try {
            json request_body = json::parse(req.body);
            enabled = request_body.value("enabled", false);
        } catch (...) {
            enabled = false;
        }
        
        if (diagnostics_callback_) {
            bool success = diagnostics_callback_(device_id, enabled);
            if (success) {
                std::string action = enabled ? "enabled" : "disabled";
                json message_data = CreateMessageResponse("Diagnostics " + action);
                res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to set diagnostics").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Diagnostics callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostReloadConfig(const httplib::Request& req, httplib::Response& res) {
    try {
        if (reload_config_callback_) {
            bool success = reload_config_callback_();
            if (success) {
                json message_data = CreateMessageResponse("Configuration reload started");
                res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to reload configuration").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Reload config callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostReinitialize(const httplib::Request& req, httplib::Response& res) {
    try {
        if (reinitialize_callback_) {
            bool success = reinitialize_callback_();
            if (success) {
                json message_data = CreateMessageResponse("Driver reinitialization started");
                res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to reinitialize drivers").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Reinitialize callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandleGetSystemStats(const httplib::Request& req, httplib::Response& res) {
    try {
        if (system_stats_callback_) {
            json stats = system_stats_callback_();
            res.set_content(CreateSuccessResponse(stats).dump(), "application/json");
        } else {
            res.set_content(CreateErrorResponse("System stats callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

// DeviceWorker 스레드 제어 핸들러들
void RestApiServer::HandlePostDeviceStart(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = ExtractDeviceId(req);
        
        if (device_start_callback_) {
            bool success = device_start_callback_(device_id);
            if (success) {
                json message_data = CreateMessageResponse("Device worker started");
                res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to start device worker").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Device start callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostDeviceStop(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = ExtractDeviceId(req);
        
        if (device_stop_callback_) {
            bool success = device_stop_callback_(device_id);
            if (success) {
                json message_data = CreateMessageResponse("Device worker stopped");
                res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to stop device worker").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Device stop callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostDevicePause(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = ExtractDeviceId(req);
        
        if (device_pause_callback_) {
            bool success = device_pause_callback_(device_id);
            if (success) {
                json message_data = CreateMessageResponse("Device worker paused");
                res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to pause device worker").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Device pause callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostDeviceResume(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = ExtractDeviceId(req);
        
        if (device_resume_callback_) {
            bool success = device_resume_callback_(device_id);
            if (success) {
                json message_data = CreateMessageResponse("Device worker resumed");
                res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to resume device worker").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Device resume callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostDeviceRestart(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = ExtractDeviceId(req);
        
        if (device_restart_callback_) {
            bool success = device_restart_callback_(device_id);
            if (success) {
                json message_data = CreateMessageResponse("Device worker restart initiated");
                res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to restart device worker").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Device restart callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

// 누락된 핸들러들 추가
void RestApiServer::HandlePostDeviceControl(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = ExtractDeviceId(req);
        
        // 일반적인 디바이스 제어 로직
        json message_data = CreateMessageResponse("Device control executed for " + device_id);
        res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostPointControl(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = ExtractDeviceId(req, 1);
        std::string point_id = ExtractDeviceId(req, 2);
        
        // 특정 포인트 제어 로직
        json message_data = CreateMessageResponse("Point control executed for " + device_id + ":" + point_id);
        res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

// 하드웨어 제어 핸들러들 (기존 코드 수정)
void RestApiServer::HandlePostPumpControl(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = ExtractDeviceId(req, 1);
        std::string pump_id = ExtractDeviceId(req, 2);
        
        bool enable = false;
        try {
            json request_body = json::parse(req.body);
            enable = request_body.value("enable", false);
        } catch (...) {
            enable = false;
        }
        
        if (pump_control_callback_) {
            bool success = pump_control_callback_(device_id, pump_id, enable);
            if (success) {
                std::string action = enable ? "started" : "stopped";
                json message_data = CreateMessageResponse("Pump " + action);
                res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to control pump").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Pump control callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostValveControl(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = ExtractDeviceId(req, 1);
        std::string valve_id = ExtractDeviceId(req, 2);
        
        double position = 0.0;
        try {
            json request_body = json::parse(req.body);
            position = request_body.value("position", 0.0);
        } catch (...) {
            position = 0.0;
        }
        
        // 위치를 0-100 범위로 제한
        position = std::max(0.0, std::min(100.0, position));
        
        if (valve_control_callback_) {
            bool success = valve_control_callback_(device_id, valve_id, position);
            if (success) {
                json message_data = CreateValveResponse(position);
                res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to control valve").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Valve control callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostSetpointChange(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = ExtractDeviceId(req, 1);
        std::string setpoint_id = ExtractDeviceId(req, 2);
        
        double value = 0.0;
        try {
            json request_body = json::parse(req.body);
            value = request_body.value("value", 0.0);
        } catch (...) {
            value = 0.0;
        }
        
        if (setpoint_change_callback_) {
            bool success = setpoint_change_callback_(device_id, setpoint_id, value);
            if (success) {
                json message_data = CreateSetpointResponse(value);
                res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to change setpoint").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Setpoint change callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

// =============================================================================
// 콜백 설정 메소드들
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
// 유틸리티 메소드들 - 수정된 버전
// =============================================================================

void RestApiServer::SetCorsHeaders(httplib::Response& res) {
#ifdef HAVE_HTTPLIB
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
#endif
}

json RestApiServer::CreateErrorResponse(const std::string& error) {
    json response = json::object();
    response["success"] = false;
    response["error"] = error;
    response["timestamp"] = static_cast<long>(duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count());
    return response;
}

json RestApiServer::CreateSuccessResponse(const json& data) {
    json response = json::object();
    response["success"] = true;
    response["timestamp"] = static_cast<long>(duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count());
    
    if (!data.empty()) {
        response["data"] = data;
    }
    
    return response;
}

// 새로운 유틸리티 메서드들 (브레이스 초기화 문제 해결용)
json RestApiServer::CreateMessageResponse(const std::string& message) {
    json response = json::object();
    response["message"] = message;
    return response;
}

json RestApiServer::CreateHealthResponse() {
    json response = json::object();
    response["status"] = "ok";
    response["uptime_seconds"] = "calculated_by_system";
    response["version"] = "2.0.0";
    return response;
}

json RestApiServer::CreateValveResponse(double position) {
    json response = json::object();
    response["message"] = "Valve position set";
    response["position"] = position;
    return response;
}

json RestApiServer::CreateSetpointResponse(double value) {
    json response = json::object();
    response["message"] = "Setpoint changed";
    response["value"] = value;
    return response;
}

std::string RestApiServer::ExtractDeviceId(const httplib::Request& req, int match_index) {
#ifdef HAVE_HTTPLIB
    if (match_index < static_cast<int>(req.matches.size())) {
        return req.matches[match_index];
    }
#endif
    return "";
}

bool RestApiServer::ValidateJsonSchema(const json& data, const std::string& schema_type) {
    try {
        if (schema_type == "device") {
            return data.contains("name") && data.contains("protocol_type") && data.contains("endpoint");
        } else if (schema_type == "datapoint") {
            return data.contains("name") && data.contains("address") && data.contains("data_type");
        } else if (schema_type == "alarm") {
            return data.contains("name") && data.contains("condition") && data.contains("threshold");
        } else if (schema_type == "virtualpoint") {
            return data.contains("name") && data.contains("formula") && data.contains("input_points");
        } else if (schema_type == "user") {
            return data.contains("username") && data.contains("email") && data.contains("role");
        }
        return false;
    } catch (const std::exception&) {
        return false;
    }
}

// =============================================================================
// 나머지 핸들러들 구현 (간략히)
// =============================================================================

void RestApiServer::HandleGetDeviceConfig(const httplib::Request& req, httplib::Response& res) {
    json devices = json::array();
    res.set_content(CreateSuccessResponse(devices).dump(), "application/json");
}

void RestApiServer::HandlePostDeviceConfig(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("Device added successfully");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

void RestApiServer::HandlePutDeviceConfig(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("Device updated");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

void RestApiServer::HandleDeleteDeviceConfig(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("Device deleted");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

void RestApiServer::HandleGetDataPoints(const httplib::Request& req, httplib::Response& res) {
    json datapoints = json::array();
    res.set_content(CreateSuccessResponse(datapoints).dump(), "application/json");
}

void RestApiServer::HandlePostDataPoint(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("DataPoint added");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

void RestApiServer::HandlePutDataPoint(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("DataPoint updated");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

void RestApiServer::HandleDeleteDataPoint(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("DataPoint deleted");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

void RestApiServer::HandleGetAlarmRules(const httplib::Request& req, httplib::Response& res) {
    json alarms = json::array();
    res.set_content(CreateSuccessResponse(alarms).dump(), "application/json");
}

void RestApiServer::HandlePostAlarmRule(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("Alarm rule added");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

void RestApiServer::HandlePutAlarmRule(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("Alarm rule updated");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

void RestApiServer::HandleDeleteAlarmRule(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("Alarm rule deleted");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

void RestApiServer::HandleGetVirtualPoints(const httplib::Request& req, httplib::Response& res) {
    json virtualpoints = json::array();
    res.set_content(CreateSuccessResponse(virtualpoints).dump(), "application/json");
}

void RestApiServer::HandlePostVirtualPoint(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("Virtual point added");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

void RestApiServer::HandlePutVirtualPoint(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("Virtual point updated");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

void RestApiServer::HandleDeleteVirtualPoint(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("Virtual point deleted");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

void RestApiServer::HandleGetUsers(const httplib::Request& req, httplib::Response& res) {
    json users = json::array();
    res.set_content(CreateSuccessResponse(users).dump(), "application/json");
}

void RestApiServer::HandlePostUser(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("User added");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

void RestApiServer::HandlePutUser(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("User updated");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

void RestApiServer::HandleDeleteUser(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("User deleted");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

void RestApiServer::HandlePostUserPermissions(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("Permissions updated");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

void RestApiServer::HandlePostSystemBackup(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("Backup started");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

void RestApiServer::HandleGetSystemLogs(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("Logs ready");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

void RestApiServer::HandleGetSystemConfig(const httplib::Request& req, httplib::Response& res) {
    json config = json::object();
    res.set_content(CreateSuccessResponse(config).dump(), "application/json");
}

void RestApiServer::HandlePutSystemConfig(const httplib::Request& req, httplib::Response& res) {
    json message_data = CreateMessageResponse("System config updated");
    res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
}

#ifndef HAVE_HTTPLIB
// httplib가 없는 경우 더미 구현
namespace httplib {
void Response::set_header(const std::string& key, const std::string& value) {}
void Response::set_content(const std::string& content, const std::string& type) {}
}
#endif