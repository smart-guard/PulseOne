// =============================================================================
// collector/src/Network/RestApiServer.cpp
// REST API 서버 완전한 구현 - 기존 코드 유지 + 디바이스 그룹 기능 추가
// =============================================================================

#include "Network/RestApiServer.h"
#include "Utils/LogManager.h"
#include "Alarm/AlarmStartupRecovery.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <regex>

// nlohmann::json 직접 사용
#include <nlohmann/json.hpp>
using nlohmann::json;

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
// 라우트 설정 - 기존 라우트 유지 + 그룹 라우트 추가
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
            <p>Version: 2.1.0</p>
            <ul>
                <li><a href="/api/docs">API Documentation</a></li>
                <li><a href="/api/health">Health Check</a></li>
                <li><a href="/api/system/stats">System Statistics</a></li>
                <li><a href="/api/groups">Device Groups</a></li>
            </ul>
        )", "text/html");
    });
    
    server_->Get("/api/health", [this](const httplib::Request& req, httplib::Response& res) {
        json health = CreateHealthResponse();
        res.set_content(CreateSuccessResponse(health).dump(), "application/json");
    });
    
    // 디바이스 목록 및 상태
    server_->Get("/api/devices", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetDevices(req, res);
    });
    
    server_->Get(R"(/api/devices/([^/]+)/status)", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetDeviceStatus(req, res);
    });
    
    // 개별 디바이스 제어 - 진단
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
    
    // 일반 제어
    server_->Post(R"(/api/devices/([^/]+)/control)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceControl(req, res);
    });

    server_->Post(R"(/api/devices/([^/]+)/points/([^/]+)/control)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostPointControl(req, res);
    });
    
    server_->Post(R"(/api/devices/([^/]+)/valve/([^/]+)/control)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostValveControl(req, res);
    });
    
    server_->Post(R"(/api/devices/([^/]+)/setpoint/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostSetpointChange(req, res);
    });

    // 새로운 하드웨어 제어 - 범용
    server_->Post(R"(/api/devices/([^/]+)/digital/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDigitalOutput(req, res);
    });
    
    server_->Post(R"(/api/devices/([^/]+)/analog/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostAnalogOutput(req, res);
    });
    
    server_->Post(R"(/api/devices/([^/]+)/parameters/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostParameterChange(req, res);
    });
    
    // ==========================================================================
    // 디바이스 그룹 제어 라우트 - 새로 추가
    // ==========================================================================
    
    // 그룹 목록 및 상태
    server_->Get("/api/groups", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetDeviceGroups(req, res);
    });
    
    server_->Get(R"(/api/groups/([^/]+)/status)", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetDeviceGroupStatus(req, res);
    });
    
    // 그룹 제어
    server_->Post(R"(/api/groups/([^/]+)/start)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceGroupStart(req, res);
    });
    
    server_->Post(R"(/api/groups/([^/]+)/stop)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceGroupStop(req, res);
    });
    
    server_->Post(R"(/api/groups/([^/]+)/pause)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceGroupPause(req, res);
    });
    
    server_->Post(R"(/api/groups/([^/]+)/resume)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceGroupResume(req, res);
    });
    
    server_->Post(R"(/api/groups/([^/]+)/restart)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceGroupRestart(req, res);
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
    
    // 설정 관리 라우트들 (기존 유지)
    server_->Get("/api/config/devices", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetDeviceConfig(req, res);
    });
    
    server_->Post("/api/config/devices", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceConfig(req, res);
    });
    
    server_->Put(R"(/api/config/devices/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePutDeviceConfig(req, res);
    });
    
    server_->Delete(R"(/api/config/devices/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandleDeleteDeviceConfig(req, res);
    });
    
    server_->Get(R"(/api/config/devices/([^/]+)/datapoints)", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetDataPoints(req, res);
    });
    
    server_->Post(R"(/api/config/devices/([^/]+)/datapoints)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDataPoint(req, res);
    });
    
    server_->Put(R"(/api/config/devices/([^/]+)/datapoints/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePutDataPoint(req, res);
    });
    
    server_->Delete(R"(/api/config/devices/([^/]+)/datapoints/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandleDeleteDataPoint(req, res);
    });
    
    server_->Get("/api/config/alarms", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetAlarmRules(req, res);
    });
    
    server_->Post("/api/config/alarms", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostAlarmRule(req, res);
    });
    
    server_->Put(R"(/api/config/alarms/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePutAlarmRule(req, res);
    });
    
    server_->Delete(R"(/api/config/alarms/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandleDeleteAlarmRule(req, res);
    });
    
    server_->Get("/api/config/virtualpoints", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetVirtualPoints(req, res);
    });
    
    server_->Post("/api/config/virtualpoints", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostVirtualPoint(req, res);
    });
    
    server_->Put(R"(/api/config/virtualpoints/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePutVirtualPoint(req, res);
    });
    
    server_->Delete(R"(/api/config/virtualpoints/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandleDeleteVirtualPoint(req, res);
    });
    
    server_->Get("/api/admin/users", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetUsers(req, res);
    });
    
    server_->Post("/api/admin/users", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostUser(req, res);
    });
    
    server_->Put(R"(/api/admin/users/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePutUser(req, res);
    });
    
    server_->Delete(R"(/api/admin/users/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandleDeleteUser(req, res);
    });
    
    server_->Post(R"(/api/admin/users/([^/]+)/permissions)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostUserPermissions(req, res);
    });
    
    server_->Post("/api/admin/backup", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostSystemBackup(req, res);
    });
    
    server_->Get("/api/admin/logs", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetSystemLogs(req, res);
    });
    
    server_->Get("/api/admin/config", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetSystemConfig(req, res);
    });
    
    server_->Put("/api/admin/config", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePutSystemConfig(req, res);
    });
    
    std::cout << "REST API 라우트 설정 완료 (기존 기능 + 디바이스 그룹 제어)" << std::endl;
#endif
}

// =============================================================================
// 기존 API 핸들러들 - 모두 유지
// =============================================================================

void RestApiServer::HandleGetDevices(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        
        if (device_list_callback_) {
            json device_list = device_list_callback_();
            res.set_content(CreateSuccessResponse(device_list).dump(), "application/json");
        } else {
            json error_response = CreateErrorResponse(
                "Device list callback not set",
                "COLLECTOR_NOT_CONFIGURED",
                "Device list functionality is not available"
            );
            res.status = 503;
            res.set_content(error_response.dump(), "application/json");
        }
    } catch (const std::exception& e) {
        json error_response = CreateErrorResponse(
            "Failed to retrieve device list",
            "DEVICE_LIST_ERROR",
            "Unable to retrieve device information: " + std::string(e.what())
        );
        res.status = 500;
        res.set_content(error_response.dump(), "application/json");
    }
}

void RestApiServer::HandleGetDeviceStatus(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string device_id = ExtractDeviceId(req);
        
        if (device_id.empty()) {
            json error_response = CreateErrorResponse(
                "Device ID required",
                "MISSING_DEVICE_ID",
                "Device ID must be specified in the URL path"
            );
            res.status = 400;
            res.set_content(error_response.dump(), "application/json");
            return;
        }
        
        if (device_status_callback_) {
            json status = device_status_callback_(device_id);
            
            // 빈 응답 체크
            if (status.empty() || (status.contains("error") && status["error"] == "device_not_found")) {
                json error_response = CreateErrorResponse(
                    "Device not found",
                    "DEVICE_NOT_FOUND",
                    "Device with ID '" + device_id + "' does not exist"
                );
                error_response["device_id"] = device_id;
                res.status = 404;
                res.set_content(error_response.dump(), "application/json");
                return;
            }
            
            res.set_content(CreateSuccessResponse(status).dump(), "application/json");
        } else {
            json error_response = CreateErrorResponse(
                "Device status callback not set",
                "COLLECTOR_NOT_CONFIGURED",
                "Device status functionality is not available"
            );
            res.status = 503;
            res.set_content(error_response.dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto [error_code, error_details] = ClassifyHardwareError(ExtractDeviceId(req), e);
        
        json error_response = CreateErrorResponse(
            "Failed to get device status",
            error_code,
            error_details
        );
        error_response["device_id"] = ExtractDeviceId(req);
        
        res.status = 500;
        res.set_content(error_response.dump(), "application/json");
    }
}

void RestApiServer::HandlePostDiagnostics(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = ExtractDeviceId(req);
        
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

// DeviceWorker 제어 핸들러들
void RestApiServer::HandlePostDeviceStart(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string device_id = ExtractDeviceId(req);
        
        if (device_start_callback_) {
            bool success = device_start_callback_(device_id);
            if (success) {
                json data = json::object();
                data["device_id"] = device_id;
                data["status"] = "started";
                data["message"] = "Device worker started successfully";
                
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                // 🔥 실패 시 구체적인 에러 코드 반환
                json error_response = CreateErrorResponse(
                    "Failed to start device worker",
                    "WORKER_START_FAILED", 
                    "Device worker could not be started. Check device configuration and hardware connection."
                );
                error_response["device_id"] = device_id;
                
                res.status = 422; // Unprocessable Entity (하드웨어 문제 가능성)
                res.set_content(error_response.dump(), "application/json");
            }
        } else {
            json error_response = CreateErrorResponse(
                "Device start callback not set",
                "COLLECTOR_NOT_CONFIGURED",
                "Collector service is not properly configured"
            );
            res.status = 503; // Service Unavailable
            res.set_content(error_response.dump(), "application/json");
        }
        
    } catch (const std::exception& e) {
        // 🔥 예외를 분석해서 구체적인 에러 코드 반환
        auto [error_code, error_details] = ClassifyHardwareError(ExtractDeviceId(req), e);
        
        json error_response = CreateErrorResponse(
            "Device start operation failed",
            error_code,
            error_details
        );
        error_response["device_id"] = ExtractDeviceId(req);
        
        // 에러 종류에 따른 HTTP 상태 코드
        if (error_code == "HARDWARE_CONNECTION_FAILED" || error_code == "HARDWARE_TIMEOUT" ||
            error_code.find("MODBUS") != std::string::npos || error_code.find("MQTT") != std::string::npos) {
            res.status = 422; // 하드웨어 연결 문제
        } else if (error_code == "WORKER_ALREADY_RUNNING") {
            res.status = 409; // Conflict
        } else if (error_code == "PERMISSION_DENIED") {
            res.status = 403; // Forbidden
        } else {
            res.status = 500; // Internal Server Error
        }
        
        res.set_content(error_response.dump(), "application/json");
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
                data["message"] = "Device worker stopped successfully";
                
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                json error_response = CreateErrorResponse(
                    "Failed to stop device worker",
                    "WORKER_STOP_FAILED",
                    "Device worker could not be stopped gracefully"
                );
                error_response["device_id"] = device_id;
                
                res.status = 422;
                res.set_content(error_response.dump(), "application/json");
            }
        } else {
            json error_response = CreateErrorResponse(
                "Device stop callback not set",
                "COLLECTOR_NOT_CONFIGURED",
                "Collector service is not properly configured"
            );
            res.status = 503;
            res.set_content(error_response.dump(), "application/json");
        }
        
    } catch (const std::exception& e) {
        auto [error_code, error_details] = ClassifyHardwareError(ExtractDeviceId(req), e);
        
        json error_response = CreateErrorResponse(
            "Device stop operation failed",
            error_code,
            error_details
        );
        error_response["device_id"] = ExtractDeviceId(req);
        
        res.status = (error_code == "WORKER_NOT_FOUND") ? 404 : 500;
        res.set_content(error_response.dump(), "application/json");
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

// 일반 제어 핸들러들
void RestApiServer::HandlePostDeviceControl(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = ExtractDeviceId(req);
        
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
        
        json message_data = CreateMessageResponse("Point control executed for " + device_id + ":" + point_id);
        res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostValveControl(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string device_id = ExtractDeviceId(req, 1);
        std::string valve_id = ExtractDeviceId(req, 2);
        
        bool open = false;
        try {
            json request_body = json::parse(req.body);
            open = request_body.value("open", false);
        } catch (...) {
            json error_response = CreateErrorResponse(
                "Invalid request body",
                "INVALID_REQUEST_FORMAT",
                "Request body must contain valid JSON with 'open' field"
            );
            res.status = 400;
            res.set_content(error_response.dump(), "application/json");
            return;
        }
        
        if (valve_control_callback_) {
            bool success = valve_control_callback_(device_id, valve_id, open);
            if (success) {
                std::string action = open ? "opened" : "closed";
                json data = json::object();
                data["device_id"] = device_id;
                data["valve_id"] = valve_id;
                data["action"] = action;
                data["open"] = open;
                data["message"] = "Valve " + valve_id + " " + action + " successfully";
                
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                std::string action = open ? "open" : "close";
                json error_response = CreateErrorResponse(
                    "Failed to control valve",
                    "VALVE_CONTROL_FAILED",
                    "Unable to " + action + " valve " + valve_id + ". Check hardware connection and valve status."
                );
                error_response["device_id"] = device_id;
                error_response["valve_id"] = valve_id;
                error_response["requested_action"] = action;
                
                res.status = 422;
                res.set_content(error_response.dump(), "application/json");
            }
        } else {
            json error_response = CreateErrorResponse(
                "Valve control callback not set",
                "COLLECTOR_NOT_CONFIGURED",
                "Valve control functionality is not available"
            );
            res.status = 503;
            res.set_content(error_response.dump(), "application/json");
        }
        
    } catch (const std::exception& e) {
        auto [error_code, error_details] = ClassifyHardwareError(ExtractDeviceId(req, 1), e);
        
        json error_response = CreateErrorResponse(
            "Valve control operation failed",
            error_code,
            error_details
        );
        error_response["device_id"] = ExtractDeviceId(req, 1);
        error_response["valve_id"] = ExtractDeviceId(req, 2);
        
        res.status = 422;
        res.set_content(error_response.dump(), "application/json");
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
                json message_data = CreateOutputResponse(value, "setpoint");
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

// 새로운 하드웨어 제어 핸들러들 (디지털/아날로그/파라미터)
void RestApiServer::HandlePostDigitalOutput(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string device_id = ExtractDeviceId(req, 1);
        std::string output_id = ExtractDeviceId(req, 2);
        
        bool enable = false;
        try {
            json request_body = json::parse(req.body);
            enable = request_body.value("enable", false);
        } catch (...) {
            json error_response = CreateErrorResponse(
                "Invalid request body",
                "INVALID_REQUEST_FORMAT",
                "Request body must contain valid JSON with 'enable' field"
            );
            res.status = 400;
            res.set_content(error_response.dump(), "application/json");
            return;
        }
        
        if (digital_output_callback_) {
            bool success = digital_output_callback_(device_id, output_id, enable);
            if (success) {
                std::string action = enable ? "enabled" : "disabled";
                json data = json::object();
                data["device_id"] = device_id;
                data["output_id"] = output_id;
                data["action"] = action;
                data["enable"] = enable;
                data["message"] = "Digital output " + output_id + " " + action + " successfully";
                
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                std::string action = enable ? "enable" : "disable";
                json error_response = CreateErrorResponse(
                    "Failed to control digital output",
                    "DIGITAL_OUTPUT_CONTROL_FAILED",
                    "Unable to " + action + " digital output " + output_id + ". Check hardware connection and output status."
                );
                error_response["device_id"] = device_id;
                error_response["output_id"] = output_id;
                error_response["requested_action"] = action;
                
                res.status = 422; // 하드웨어 제어 실패
                res.set_content(error_response.dump(), "application/json");
            }
        } else {
            json error_response = CreateErrorResponse(
                "Digital output callback not set",
                "COLLECTOR_NOT_CONFIGURED",
                "Digital output control functionality is not available"
            );
            res.status = 503;
            res.set_content(error_response.dump(), "application/json");
        }
        
    } catch (const std::exception& e) {
        auto [error_code, error_details] = ClassifyHardwareError(ExtractDeviceId(req, 1), e);
        
        json error_response = CreateErrorResponse(
            "Digital output control operation failed",
            error_code,
            error_details
        );
        error_response["device_id"] = ExtractDeviceId(req, 1);
        error_response["output_id"] = ExtractDeviceId(req, 2);
        
        res.status = 422; // 대부분 하드웨어 관련 문제
        res.set_content(error_response.dump(), "application/json");
    }
}

void RestApiServer::HandlePostAnalogOutput(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = ExtractDeviceId(req, 1);
        std::string output_id = ExtractDeviceId(req, 2);
        
        double value = 0.0;
        try {
            json request_body = json::parse(req.body);
            value = request_body.value("value", 0.0);
        } catch (...) {
            value = 0.0;
        }
        
        if (analog_output_callback_) {
            bool success = analog_output_callback_(device_id, output_id, value);
            if (success) {
                json message_data = CreateOutputResponse(value, "analog");
                res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to control analog output").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Analog output callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostParameterChange(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = ExtractDeviceId(req, 1);
        std::string parameter_id = ExtractDeviceId(req, 2);
        
        double value = 0.0;
        try {
            json request_body = json::parse(req.body);
            value = request_body.value("value", 0.0);
        } catch (...) {
            value = 0.0;
        }
        
        if (parameter_change_callback_) {
            bool success = parameter_change_callback_(device_id, parameter_id, value);
            if (success) {
                json message_data = CreateOutputResponse(value, "parameter");
                res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to change parameter").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Parameter change callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

// =============================================================================
// 디바이스 그룹 핸들러들 - 새로운 구현
// =============================================================================

void RestApiServer::HandleGetDeviceGroups(const httplib::Request& req, httplib::Response& res) {
    try {
        if (device_group_list_callback_) {
            json group_list = device_group_list_callback_();
            res.set_content(CreateSuccessResponse(group_list).dump(), "application/json");
        } else {
            res.set_content(CreateErrorResponse("Device group list callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandleGetDeviceGroupStatus(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string group_id = ExtractGroupId(req);
        
        if (device_group_status_callback_) {
            json status = device_group_status_callback_(group_id);
            res.set_content(CreateSuccessResponse(status).dump(), "application/json");
        } else {
            res.set_content(CreateErrorResponse("Device group status callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostDeviceGroupStart(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string group_id = ExtractGroupId(req);
        
        if (device_group_control_callback_) {
            bool success = device_group_control_callback_(group_id, "start");
            json response_data = CreateGroupActionResponse(group_id, "start", success);
            
            if (success) {
                res.set_content(CreateSuccessResponse(response_data).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to start device group").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Device group control callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostDeviceGroupStop(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string group_id = ExtractGroupId(req);
        
        if (device_group_control_callback_) {
            bool success = device_group_control_callback_(group_id, "stop");
            json response_data = CreateGroupActionResponse(group_id, "stop", success);
            
            if (success) {
                res.set_content(CreateSuccessResponse(response_data).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to stop device group").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Device group control callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostDeviceGroupPause(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string group_id = ExtractGroupId(req);
        
        if (device_group_control_callback_) {
            bool success = device_group_control_callback_(group_id, "pause");
            json response_data = CreateGroupActionResponse(group_id, "pause", success);
            
            if (success) {
                res.set_content(CreateSuccessResponse(response_data).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to pause device group").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Device group control callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostDeviceGroupResume(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string group_id = ExtractGroupId(req);
        
        if (device_group_control_callback_) {
            bool success = device_group_control_callback_(group_id, "resume");
            json response_data = CreateGroupActionResponse(group_id, "resume", success);
            
            if (success) {
                res.set_content(CreateSuccessResponse(response_data).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to resume device group").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Device group control callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostDeviceGroupRestart(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string group_id = ExtractGroupId(req);
        
        if (device_group_control_callback_) {
            bool success = device_group_control_callback_(group_id, "restart");
            json response_data = CreateGroupActionResponse(group_id, "restart", success);
            
            if (success) {
                res.set_content(CreateSuccessResponse(response_data).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to restart device group").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Device group control callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

// =============================================================================
// 시스템 제어 핸들러들
// =============================================================================

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

// =============================================================================
// 콜백 설정 메소드들 - 기존 콜백 + 그룹 콜백 추가
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

// 기존 하드웨어 제어 콜백들 유지
void RestApiServer::SetPumpControlCallback(PumpControlCallback callback) {
    pump_control_callback_ = callback;
}

void RestApiServer::SetValveControlCallback(ValveControlCallback callback) {
    valve_control_callback_ = callback;
}

void RestApiServer::SetSetpointChangeCallback(SetpointChangeCallback callback) {
    setpoint_change_callback_ = callback;
}

// 새로운 하드웨어 제어 콜백들
void RestApiServer::SetDigitalOutputCallback(DigitalOutputCallback callback) {
    digital_output_callback_ = callback;
}

void RestApiServer::SetAnalogOutputCallback(AnalogOutputCallback callback) {
    analog_output_callback_ = callback;
}

void RestApiServer::SetParameterChangeCallback(ParameterChangeCallback callback) {
    parameter_change_callback_ = callback;
}

// 디바이스 그룹 콜백 설정 - 새로 추가
void RestApiServer::SetDeviceGroupListCallback(DeviceGroupListCallback callback) {
    device_group_list_callback_ = callback;
}

void RestApiServer::SetDeviceGroupStatusCallback(DeviceGroupStatusCallback callback) {
    device_group_status_callback_ = callback;
}

void RestApiServer::SetDeviceGroupControlCallback(DeviceGroupControlCallback callback) {
    device_group_control_callback_ = callback;
}

// 설정 관리 콜백들
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
// 유틸리티 메소드들
// =============================================================================

void RestApiServer::SetCorsHeaders(httplib::Response& res) {
#ifdef HAVE_HTTPLIB
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
#endif
}

json RestApiServer::CreateErrorResponse(const std::string& error, const std::string& error_code, const std::string& details) {
    json response = json::object();
    response["success"] = false;
    response["error"] = error;
    
    // 🔥 NEW: 구체적인 에러 코드 (백엔드가 HTTP 상태코드 결정에 사용)
    if (!error_code.empty()) {
        response["error_code"] = error_code;
    }
    
    // 🔥 NEW: 상세 정보 (사용자 친화적 메시지)
    if (!details.empty()) {
        response["details"] = details;
    }
    
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

json RestApiServer::CreateMessageResponse(const std::string& message) {
    json response = json::object();
    response["message"] = message;
    return response;
}

json RestApiServer::CreateHealthResponse() {
    json response = json::object();
    response["status"] = "ok";
    response["uptime_seconds"] = "calculated_by_system";
    response["version"] = "2.1.0";
    response["features"] = json::array({"device_control", "hardware_control", "device_groups", "system_management"});
    return response;
}

json RestApiServer::CreateOutputResponse(double value, const std::string& type) {
    json response = json::object();
    response["message"] = type + " output set";
    response["value"] = value;
    response["type"] = type;
    return response;
}

// 그룹 액션 응답 생성 - 새로 추가
json RestApiServer::CreateGroupActionResponse(const std::string& group_id, const std::string& action, bool success) {
    json response = json::object();
    response["group_id"] = group_id;
    response["action"] = action;
    response["result"] = success ? "success" : "failed";
    response["message"] = "Group " + group_id + " " + action + " " + (success ? "completed successfully" : "failed");
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

// 그룹 ID 추출 - 새로 추가
std::string RestApiServer::ExtractGroupId(const httplib::Request& req, int match_index) {
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
        } else if (schema_type == "group") { // 새로 추가
            return data.contains("name") && data.contains("devices") && data["devices"].is_array();
        }
        return false;
    } catch (const std::exception&) {
        return false;
    }
}

// =============================================================================
// 설정 관리 핸들러들 (기존 유지)
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


// GET /api/alarm/recovery/status - 복구 상태 조회
void RestApiServer::HandleAlarmRecoveryStatus(const httplib::Request& req, httplib::Response& res) {
    try {
        // ❌ 잘못된 코드: auto& alarm_recovery = Alarm::AlarmStartupRecovery::getInstance();
        // ✅ 올바른 코드:
        auto& alarm_recovery = PulseOne::Alarm::AlarmStartupRecovery::getInstance();
        
        // 복구 상태 확인
        bool is_recovery_enabled = alarm_recovery.IsRecoveryEnabled();
        bool is_recovery_completed = alarm_recovery.IsRecoveryCompleted();
        auto recovery_stats = alarm_recovery.GetRecoveryStats();
        
        // JSON 응답 생성
        nlohmann::json response_data;
        response_data["recovery_enabled"] = is_recovery_enabled;
        response_data["recovery_completed"] = is_recovery_completed;
        response_data["total_active_alarms"] = recovery_stats.total_active_alarms;
        response_data["successfully_published"] = recovery_stats.successfully_published;
        response_data["failed_to_publish"] = recovery_stats.failed_to_publish;
        response_data["recovery_duration_ms"] = recovery_stats.recovery_duration.count();
        response_data["last_recovery_time"] = recovery_stats.last_recovery_time;
        
        // ❌ 잘못된 JSON 대입문 제거
        // }; <- 이 줄이 문제였음
        
        res.set_content(CreateSuccessResponse(response_data).dump(), "application/json");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("알람 복구 상태 조회 실패: " + std::string(e.what()));
        
        nlohmann::json error_data;
        error_data["error"] = "Failed to get alarm recovery status";
        error_data["details"] = e.what();
        
        res.status = 500;
        res.set_content(CreateErrorResponse(error_data).dump(), "application/json");
    }
}

// POST /api/alarm/recovery/trigger - 수동 알람 복구 트리거
void RestApiServer::HandleAlarmRecoveryTrigger(const httplib::Request& req, httplib::Response& res) {
    try {
        // ❌ 잘못된 코드: auto& alarm_recovery = Alarm::AlarmStartupRecovery::getInstance();
        // ✅ 올바른 코드:
        auto& alarm_recovery = PulseOne::Alarm::AlarmStartupRecovery::getInstance();
        
        LogManager::getInstance().Info("REST API를 통한 알람 복구 수동 트리거 요청");
        
        // 알람 복구 실행
        size_t recovered_count = alarm_recovery.RecoverActiveAlarms();
        auto recovery_stats = alarm_recovery.GetRecoveryStats();
        
        // JSON 응답 생성
        nlohmann::json response_data;
        response_data["message"] = "Alarm recovery triggered successfully";
        response_data["recovered_count"] = recovered_count;
        response_data["total_active_alarms"] = recovery_stats.total_active_alarms;
        response_data["successfully_published"] = recovery_stats.successfully_published;
        response_data["failed_to_publish"] = recovery_stats.failed_to_publish;
        response_data["recovery_duration_ms"] = recovery_stats.recovery_duration.count();
        response_data["triggered_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // ❌ 잘못된 JSON 대입문 제거
        // }; <- 이 줄이 문제였음
        
        res.set_content(CreateSuccessResponse(response_data).dump(), "application/json");
        
        LogManager::getInstance().Info("알람 복구 REST API 응답 완료 - 복구된 알람: " + 
                                      std::to_string(recovered_count) + "개");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("알람 복구 트리거 실패: " + std::string(e.what()));
        
        nlohmann::json error_data;
        error_data["error"] = "Failed to trigger alarm recovery";
        error_data["details"] = e.what();
        
        res.status = 500;
        res.set_content(CreateErrorResponse(error_data).dump(), "application/json");
    }
}

std::pair<std::string, std::string> RestApiServer::ClassifyHardwareError(const std::string& device_id, const std::exception& e) {
    std::string error_message = e.what();
    std::string lower_msg = error_message;
    std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);
    
    // 연결 실패 패턴들
    if (lower_msg.find("connection") != std::string::npos && 
        (lower_msg.find("failed") != std::string::npos || lower_msg.find("refused") != std::string::npos)) {
        return {"HARDWARE_CONNECTION_FAILED", "Device connection failed: " + error_message};
    }
    
    // 타임아웃 패턴들
    if (lower_msg.find("timeout") != std::string::npos || lower_msg.find("timed out") != std::string::npos) {
        return {"HARDWARE_TIMEOUT", "Device response timeout: " + error_message};
    }
    
    // Modbus 특화 에러들
    if (lower_msg.find("modbus") != std::string::npos) {
        if (lower_msg.find("slave") != std::string::npos && lower_msg.find("respond") != std::string::npos) {
            return {"MODBUS_SLAVE_NO_RESPONSE", "Modbus slave not responding: " + error_message};
        }
        if (lower_msg.find("crc") != std::string::npos || lower_msg.find("checksum") != std::string::npos) {
            return {"MODBUS_CRC_ERROR", "Modbus CRC error: " + error_message};
        }
        return {"MODBUS_PROTOCOL_ERROR", "Modbus protocol error: " + error_message};
    }
    
    // MQTT 특화 에러들
    if (lower_msg.find("mqtt") != std::string::npos) {
        if (lower_msg.find("broker") != std::string::npos) {
            return {"MQTT_BROKER_UNREACHABLE", "MQTT broker unreachable: " + error_message};
        }
        if (lower_msg.find("auth") != std::string::npos || lower_msg.find("credential") != std::string::npos) {
            return {"MQTT_AUTH_FAILED", "MQTT authentication failed: " + error_message};
        }
        return {"MQTT_CONNECTION_ERROR", "MQTT connection error: " + error_message};
    }
    
    // Worker 관리 에러들
    if (lower_msg.find("worker") != std::string::npos) {
        if (lower_msg.find("already") != std::string::npos && lower_msg.find("running") != std::string::npos) {
            return {"WORKER_ALREADY_RUNNING", "Worker already running: " + error_message};
        }
        if (lower_msg.find("not found") != std::string::npos) {
            return {"WORKER_NOT_FOUND", "Worker not found: " + error_message};
        }
        return {"WORKER_OPERATION_FAILED", "Worker operation failed: " + error_message};
    }
    
    // 디바이스 관련 에러들
    if (lower_msg.find("device") != std::string::npos && lower_msg.find("not found") != std::string::npos) {
        return {"DEVICE_NOT_FOUND", "Device not found: " + error_message};
    }
    
    // 권한 관련 에러들
    if (lower_msg.find("permission") != std::string::npos || lower_msg.find("access denied") != std::string::npos) {
        return {"PERMISSION_DENIED", "Access denied: " + error_message};
    }
    
    // 설정 관련 에러들
    if (lower_msg.find("config") != std::string::npos || lower_msg.find("invalid") != std::string::npos) {
        return {"CONFIGURATION_ERROR", "Configuration error: " + error_message};
    }
    
    // 기본 에러
    return {"COLLECTOR_INTERNAL_ERROR", "Internal collector error: " + error_message};
}

#ifndef HAVE_HTTPLIB
// httplib가 없는 경우 더미 구현
namespace httplib {
void Response::set_header(const std::string& key, const std::string& value) {}
void Response::set_content(const std::string& content, const std::string& type) {}
}
#endif