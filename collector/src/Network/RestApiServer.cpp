// =============================================================================
// collector/src/Network/RestApiServer.cpp
// REST API 서버 구현 - 완성본 (Part 1/2)
// =============================================================================

#include "Network/RestApiServer.h"
#include "Network/HttpErrorMapper.h"
#include "Utils/LogManager.h"
#include "Alarm/AlarmStartupRecovery.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <regex>
#include <algorithm>
#include <cctype>

// nlohmann::json 직접 사용
#include <nlohmann/json.hpp>
using nlohmann::json;

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
    
    // 범용 하드웨어 제어 API (펌프, 밸브, 모터 등 모든 것을 포괄)
    server_->Post(R"(/api/devices/([^/]+)/digital/([^/]+)/control)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDigitalOutput(req, res);
    });
    
    server_->Post(R"(/api/devices/([^/]+)/analog/([^/]+)/control)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostAnalogOutput(req, res);
    });
    
    server_->Post(R"(/api/devices/([^/]+)/parameters/([^/]+)/set)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostParameterChange(req, res);
    });
    
    // 디바이스 그룹 제어 라우트
    server_->Get("/api/groups", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetDeviceGroups(req, res);
    });
    
    server_->Get(R"(/api/groups/([^/]+)/status)", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetDeviceGroupStatus(req, res);
    });
    
    server_->Post(R"(/api/groups/([^/]+)/start)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceGroupStart(req, res);
    });
    
    server_->Post(R"(/api/groups/([^/]+)/stop)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceGroupStop(req, res);
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
    
    // 에러 통계 API
    server_->Get("/api/errors/statistics", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetErrorStatistics(req, res);
    });
    
    server_->Get(R"(/api/errors/([^/]+)/info)", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetErrorCodeInfo(req, res);
    });
    
    std::cout << "REST API 라우트 설정 완료" << std::endl;
#endif
}

// =============================================================================
// 핵심 API 핸들러들 - ClassifyHardwareError 활용
// =============================================================================

void RestApiServer::HandleGetDevices(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        
        if (device_list_callback_) {
            json device_list = device_list_callback_();
            res.set_content(CreateSuccessResponse(device_list).dump(), "application/json");
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Device list callback not set", "COLLECTOR_NOT_CONFIGURED", 
                                               "Device list functionality is not available").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError("", e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "Failed to retrieve device list");
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
    }
}

void RestApiServer::HandleGetDeviceStatus(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string device_id = ExtractDeviceId(req);
        
        if (device_id.empty()) {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INVALID_PARAMETER);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Device ID required", "MISSING_DEVICE_ID", 
                                               "Device ID must be specified in the URL path").dump(), "application/json");
            return;
        }
        
        if (device_status_callback_) {
            json status = device_status_callback_(device_id);
            
            if (status.empty() || (status.contains("error") && status["error"] == "device_not_found")) {
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_NOT_FOUND);
                res.status = http_status;
                
                json error_response = CreateErrorResponse("Device not found", "DEVICE_NOT_FOUND", 
                                                         "Device with ID '" + device_id + "' does not exist");
                error_response["device_id"] = device_id;
                res.set_content(error_response.dump(), "application/json");
                return;
            }
            
            res.set_content(CreateSuccessResponse(status).dump(), "application/json");
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Device status callback not set", "COLLECTOR_NOT_CONFIGURED", 
                                               "Device status functionality is not available").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError(ExtractDeviceId(req), e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "Failed to get device status");
        error_response["device_id"] = ExtractDeviceId(req);
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
    }
}

void RestApiServer::HandlePostDiagnostics(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
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
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_ERROR);
                res.status = http_status;
                res.set_content(CreateErrorResponse("Failed to set diagnostics", "DIAGNOSTICS_FAILED", "").dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Diagnostics callback not set", "SERVICE_UNAVAILABLE", "").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError(ExtractDeviceId(req), e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "");
        error_response["device_id"] = ExtractDeviceId(req);
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
    }
}

// DeviceWorker 제어 핸들러들 - ClassifyHardwareError 적용
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
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_ERROR);
                
                json error_response = CreateErrorResponse("Failed to start device worker", "WORKER_START_FAILED", 
                                                         "Device worker could not be started. Check device configuration and hardware connection.");
                error_response["device_id"] = device_id;
                
                res.status = http_status;
                res.set_content(error_response.dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Device start callback not set", "COLLECTOR_NOT_CONFIGURED", 
                                               "Collector service is not properly configured").dump(), "application/json");
        }
        
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError(ExtractDeviceId(req), e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "Device start operation failed");
        error_response["device_id"] = ExtractDeviceId(req);
        
        // 에러 종류에 따른 HTTP 상태 코드 미세 조정
        if (error_code_str == "WORKER_ALREADY_RUNNING") {
            res.status = 409; // Conflict
        } else if (error_code_str == "PERMISSION_DENIED") {
            res.status = 403; // Forbidden
        } else {
            res.status = http_status;
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
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_ERROR);
                
                json error_response = CreateErrorResponse("Failed to stop device worker", "WORKER_STOP_FAILED", 
                                                         "Device worker could not be stopped gracefully");
                error_response["device_id"] = device_id;
                
                res.status = http_status;
                res.set_content(error_response.dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Device stop callback not set", "COLLECTOR_NOT_CONFIGURED", 
                                               "Collector service is not properly configured").dump(), "application/json");
        }
        
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError(ExtractDeviceId(req), e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "Device stop operation failed");
        error_response["device_id"] = ExtractDeviceId(req);
        
        res.status = (error_code_str == "WORKER_NOT_FOUND") ? 404 : http_status;
        res.set_content(error_response.dump(), "application/json");
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
                data["message"] = "Device worker paused successfully";
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_ERROR);
                res.status = http_status;
                res.set_content(CreateErrorResponse("Failed to pause device worker", "WORKER_PAUSE_FAILED", "").dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Device pause callback not set", "SERVICE_UNAVAILABLE", "").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError(ExtractDeviceId(req), e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "");
        error_response["device_id"] = ExtractDeviceId(req);
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
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
                data["message"] = "Device worker resumed successfully";
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_ERROR);
                res.status = http_status;
                res.set_content(CreateErrorResponse("Failed to resume device worker", "WORKER_RESUME_FAILED", "").dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Device resume callback not set", "SERVICE_UNAVAILABLE", "").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError(ExtractDeviceId(req), e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "");
        error_response["device_id"] = ExtractDeviceId(req);
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
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
                data["message"] = "Device worker restart initiated";
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_ERROR);
                res.status = http_status;
                res.set_content(CreateErrorResponse("Failed to restart device worker", "WORKER_RESTART_FAILED", "").dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Device restart callback not set", "SERVICE_UNAVAILABLE", "").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError(ExtractDeviceId(req), e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "");
        error_response["device_id"] = ExtractDeviceId(req);
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
    }
}

// 일반 제어 핸들러들
void RestApiServer::HandlePostDeviceControl(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string device_id = ExtractDeviceId(req);
        
        json message_data = CreateMessageResponse("Device control executed for " + device_id);
        res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError(ExtractDeviceId(req), e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "");
        error_response["device_id"] = ExtractDeviceId(req);
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
    }
}

void RestApiServer::HandlePostPointControl(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string device_id = ExtractDeviceId(req, 1);
        std::string point_id = ExtractDeviceId(req, 2);
        
        json message_data = CreateMessageResponse("Point control executed for " + device_id + ":" + point_id);
        res.set_content(CreateSuccessResponse(message_data).dump(), "application/json");
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError(ExtractDeviceId(req, 1), e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "");
        error_response["device_id"] = ExtractDeviceId(req, 1);
        error_response["point_id"] = ExtractDeviceId(req, 2);
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
    }
}

// 범용 하드웨어 제어 핸들러들 - ClassifyHardwareError 적용
// 디지털 출력 제어 (펌프, 밸브, 릴레이, 솔레노이드 등 모든 ON/OFF 장치)
void RestApiServer::HandlePostDigitalOutput(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string device_id = ExtractDeviceId(req, 1);
        std::string output_id = ExtractDeviceId(req, 2);
        
        bool enable = false;
        try {
            json request_body = json::parse(req.body);
            enable = request_body.value("enable", false);
        } catch (const json::parse_error&) {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DATA_FORMAT_ERROR);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Invalid JSON format", "DATA_FORMAT_ERROR", 
                                               "Request body must contain valid JSON with 'enable' field").dump(), "application/json");
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
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_ERROR);
                
                json error_response = CreateErrorResponse("Failed to control digital output", "DIGITAL_OUTPUT_CONTROL_FAILED", 
                                                         "Unable to " + action + " digital output " + output_id + ". Check hardware connection and output status.");
                error_response["device_id"] = device_id;
                error_response["output_id"] = output_id;
                error_response["requested_action"] = action;
                
                res.status = http_status;
                res.set_content(error_response.dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Digital output callback not set", "COLLECTOR_NOT_CONFIGURED", 
                                               "Digital output control functionality is not available").dump(), "application/json");
        }
        
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError(ExtractDeviceId(req, 1), e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "Digital output control operation failed");
        error_response["device_id"] = ExtractDeviceId(req, 1);
        error_response["output_id"] = ExtractDeviceId(req, 2);
        
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
    }
}

// 아날로그 출력 제어 (속도 제어, 압력 조절, 밝기 조절 등)
void RestApiServer::HandlePostAnalogOutput(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
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
                json data = CreateOutputResponse(value, "analog");
                data["device_id"] = device_id;
                data["output_id"] = output_id;
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_ERROR);
                res.status = http_status;
                res.set_content(CreateErrorResponse("Failed to control analog output", "ANALOG_OUTPUT_FAILED", "").dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Analog output callback not set", "SERVICE_UNAVAILABLE", "").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError(ExtractDeviceId(req, 1), e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "");
        error_response["device_id"] = ExtractDeviceId(req, 1);
        error_response["output_id"] = ExtractDeviceId(req, 2);
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
    }
}

// 파라미터 변경 (설정값, 임계값 등)
void RestApiServer::HandlePostParameterChange(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string device_id = ExtractDeviceId(req, 1);
        std::string parameter_id = ExtractDeviceId(req, 2);
        
        double value = 0.0;
        try {
            json request_body = json::parse(req.body);
            value = request_body.value("value", 0.0);
        } catch (...) {
            value = 0.0;
        }
        
        // 수정: setpoint_change_callback_ → parameter_change_callback_
        if (parameter_change_callback_) {
            bool success = parameter_change_callback_(device_id, parameter_id, value);
            if (success) {
                json data = CreateOutputResponse(value, "parameter");
                data["device_id"] = device_id;
                data["parameter_id"] = parameter_id;
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_ERROR);
                res.status = http_status;
                res.set_content(CreateErrorResponse("Failed to change parameter", "PARAMETER_CHANGE_FAILED", "").dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Parameter change callback not set", "SERVICE_UNAVAILABLE", "").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError(ExtractDeviceId(req, 1), e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "");
        error_response["device_id"] = ExtractDeviceId(req, 1);
        error_response["parameter_id"] = ExtractDeviceId(req, 2);
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
    }
}

// 시스템 제어 핸들러들
void RestApiServer::HandlePostReloadConfig(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        
        if (reload_config_callback_) {
            bool success = reload_config_callback_();
            if (success) {
                json data = CreateMessageResponse("Configuration reload started");
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::CONFIGURATION_ERROR);
                res.status = http_status;
                res.set_content(CreateErrorResponse("Failed to reload configuration", "CONFIG_RELOAD_FAILED", "").dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Reload config callback not set", "SERVICE_UNAVAILABLE", "").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError("", e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "");
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
    }
}

void RestApiServer::HandlePostReinitialize(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        
        if (reinitialize_callback_) {
            bool success = reinitialize_callback_();
            if (success) {
                json data = CreateMessageResponse("Driver reinitialization started");
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
                res.status = http_status;
                res.set_content(CreateErrorResponse("Failed to reinitialize drivers", "REINIT_FAILED", "").dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Reinitialize callback not set", "SERVICE_UNAVAILABLE", "").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError("", e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "");
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
    }
}

void RestApiServer::HandleGetSystemStats(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        
        if (system_stats_callback_) {
            json stats = system_stats_callback_();
            res.set_content(CreateSuccessResponse(stats).dump(), "application/json");
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("System stats callback not set", "SERVICE_UNAVAILABLE", "").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError("", e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "");
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
    }
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
    
    if (!error_code.empty()) {
        response["error_code"] = error_code;
    }
    
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
        } else if (schema_type == "group") {
            return data.contains("name") && data.contains("devices") && data["devices"].is_array();
        }
        return false;
    } catch (const std::exception&) {
        return false;
    }
}

// ClassifyHardwareError - 예외 메시지 분석
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

void RestApiServer::SetDigitalOutputCallback(DigitalOutputCallback callback) {
    digital_output_callback_ = callback;
}

void RestApiServer::SetAnalogOutputCallback(AnalogOutputCallback callback) {
    analog_output_callback_ = callback;
}

void RestApiServer::SetParameterChangeCallback(ParameterChangeCallback callback) {
    parameter_change_callback_ = callback;
}

void RestApiServer::HandleGetDeviceGroups(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        
        if (device_group_list_callback_) {
            json groups = device_group_list_callback_();
            res.set_content(CreateSuccessResponse(groups).dump(), "application/json");
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Device group list callback not set", "COLLECTOR_NOT_CONFIGURED", 
                                               "Device group functionality is not available").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError("", e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "Failed to retrieve device groups");
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
    }
}

void RestApiServer::HandleGetDeviceGroupStatus(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string group_id = ExtractGroupId(req);
        
        if (device_group_status_callback_) {
            json status = device_group_status_callback_(group_id);
            
            if (status.empty() || (status.contains("error") && status["error"] == "group_not_found")) {
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_NOT_FOUND);
                res.status = http_status;
                
                json error_response = CreateErrorResponse("Device group not found", "GROUP_NOT_FOUND", 
                                                         "Device group with ID '" + group_id + "' does not exist");
                error_response["group_id"] = group_id;
                res.set_content(error_response.dump(), "application/json");
                return;
            }
            
            res.set_content(CreateSuccessResponse(status).dump(), "application/json");
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Device group status callback not set", "COLLECTOR_NOT_CONFIGURED", 
                                               "Device group functionality is not available").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError(ExtractGroupId(req), e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "Failed to get device group status");
        error_response["group_id"] = ExtractGroupId(req);
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
    }
}

void RestApiServer::HandlePostDeviceGroupStart(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string group_id = ExtractGroupId(req);
        
        if (device_group_control_callback_) {
            bool success = device_group_control_callback_(group_id, "start");
            if (success) {
                json data = CreateGroupActionResponse(group_id, "started", true);
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_ERROR);
                
                json error_response = CreateErrorResponse("Failed to start device group", "GROUP_START_FAILED", 
                                                         "Unable to start all devices in group '" + group_id + "'");
                error_response["group_id"] = group_id;
                error_response["action"] = "start";
                
                res.status = http_status;
                res.set_content(error_response.dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Device group control callback not set", "COLLECTOR_NOT_CONFIGURED", 
                                               "Device group control functionality is not available").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError(ExtractGroupId(req), e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "Group start operation failed");
        error_response["group_id"] = ExtractGroupId(req);
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
    }
}

void RestApiServer::HandlePostDeviceGroupStop(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string group_id = ExtractGroupId(req);
        
        if (device_group_control_callback_) {
            bool success = device_group_control_callback_(group_id, "stop");
            if (success) {
                json data = CreateGroupActionResponse(group_id, "stopped", true);
                res.set_content(CreateSuccessResponse(data).dump(), "application/json");
            } else {
                auto& mapper = HttpErrorMapper::getInstance();
                int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::DEVICE_ERROR);
                
                json error_response = CreateErrorResponse("Failed to stop device group", "GROUP_STOP_FAILED", 
                                                         "Unable to stop all devices in group '" + group_id + "'");
                error_response["group_id"] = group_id;
                error_response["action"] = "stop";
                
                res.status = http_status;
                res.set_content(error_response.dump(), "application/json");
            }
        } else {
            auto& mapper = HttpErrorMapper::getInstance();
            int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
            res.status = http_status;
            res.set_content(CreateErrorResponse("Device group control callback not set", "COLLECTOR_NOT_CONFIGURED", 
                                               "Device group control functionality is not available").dump(), "application/json");
        }
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError(ExtractGroupId(req), e);
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode error_code = mapper.ParseErrorString(error_code_str);
        int http_status = mapper.MapErrorToHttpStatus(error_code);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "Group stop operation failed");
        error_response["group_id"] = ExtractGroupId(req);
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
    }
}

void RestApiServer::HandleGetErrorStatistics(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        
        json stats = json::object();
        stats["total_errors"] = 0;
        stats["error_types"] = json::object();
        stats["device_errors"] = json::object();
        stats["last_24_hours"] = json::array();
        
        res.set_content(CreateSuccessResponse(stats).dump(), "application/json");
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError("", e);
        auto& mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "Failed to get error statistics");
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
    }
}

void RestApiServer::HandleGetErrorCodeInfo(const httplib::Request& req, httplib::Response& res) {
    try {
        SetCorsHeaders(res);
        std::string error_code = ExtractDeviceId(req, 1);  // Reusing method for path parameter
        
        auto& mapper = HttpErrorMapper::getInstance();
        PulseOne::Enums::ErrorCode enum_code = mapper.ParseErrorString(error_code);
        
        // GetErrorDetail 메서드를 사용하여 ErrorDetail 구조체 가져오기
        auto error_detail = mapper.GetErrorDetail(enum_code);
        
        json info = json::object();
        info["error_code"] = error_code;
        info["http_status"] = error_detail.http_status;
        info["severity"] = error_detail.severity;
        info["category"] = error_detail.category;
        info["error_type"] = error_detail.error_type;
        info["recoverable"] = error_detail.recoverable;
        info["user_actionable"] = error_detail.user_actionable;
        info["action_hint"] = error_detail.action_hint;
        info["tech_details"] = error_detail.tech_details;
        info["user_message"] = mapper.GetUserFriendlyMessage(enum_code);
        
        res.set_content(CreateSuccessResponse(info).dump(), "application/json");
    } catch (const std::exception& e) {
        auto [error_code_str, error_details] = ClassifyHardwareError("", e);
        auto& mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
        
        json error_response = CreateErrorResponse(error_details, error_code_str, "Failed to get error code information");
        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
    }
}

#ifndef HAVE_HTTPLIB
// httplib가 없는 경우 더미 구현
namespace httplib {
void Response::set_header(const std::string& key, const std::string& value) {}
void Response::set_content(const std::string& content, const std::string& type) {}
}
#endif