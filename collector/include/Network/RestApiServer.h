// =============================================================================
// collector/include/Network/RestApiServer.h
// 웹 클라이언트용 REST API 서버
// =============================================================================

#ifndef PULSEONE_REST_API_SERVER_H
#define PULSEONE_REST_API_SERVER_H

#include <memory>
#include <functional>
#include <string>
#include <nlohmann/json.hpp>

// REST 프레임워크 (예: cpp-httplib 사용)
#ifdef HAVE_HTTPLIB
#include <httplib.h>
#endif

namespace PulseOne {
namespace Network {

using json = nlohmann::json;

/**
 * @brief CollectorApplication을 제어하는 REST API 서버
 */
class RestApiServer {
public:
    // 콜백 함수 타입들
    using ReloadConfigCallback = std::function<bool()>;
    using ReinitializeCallback = std::function<bool()>;
    using DeviceControlCallback = std::function<bool(const std::string&)>;
    using DeviceListCallback = std::function<json()>;
    using DeviceStatusCallback = std::function<json(const std::string&)>;
    using SystemStatsCallback = std::function<json()>;
    using DiagnosticsCallback = std::function<bool(const std::string&, bool)>;

public:
    RestApiServer(int port = 8080);
    ~RestApiServer();

    // 서버 제어
    bool Start();
    void Stop();
    bool IsRunning() const;

    // 콜백 등록 (CollectorApplication에서 호출)
    void SetReloadConfigCallback(ReloadConfigCallback callback);
    void SetReinitializeCallback(ReinitializeCallback callback);
    void SetStartDeviceCallback(DeviceControlCallback callback);
    void SetStopDeviceCallback(DeviceControlCallback callback);
    void SetRestartDeviceCallback(DeviceControlCallback callback);
    void SetDeviceListCallback(DeviceListCallback callback);
    void SetDeviceStatusCallback(DeviceStatusCallback callback);
    void SetSystemStatsCallback(SystemStatsCallback callback);
    void SetDiagnosticsCallback(DiagnosticsCallback callback);

private:
    void SetupRoutes();
    
    // API 엔드포인트 핸들러들
    void HandleGetDevices(const httplib::Request& req, httplib::Response& res);
    void HandleGetDeviceStatus(const httplib::Request& req, httplib::Response& res);
    void HandlePostDeviceConnect(const httplib::Request& req, httplib::Response& res);
    void HandlePostDeviceDisconnect(const httplib::Request& req, httplib::Response& res);
    void HandlePostDeviceRestart(const httplib::Request& req, httplib::Response& res);
    void HandlePostReloadConfig(const httplib::Request& req, httplib::Response& res);
    void HandlePostReinitialize(const httplib::Request& req, httplib::Response& res);
    void HandleGetSystemStats(const httplib::Request& req, httplib::Response& res);
    void HandlePostDiagnostics(const httplib::Request& req, httplib::Response& res);
    
    // 유틸리티
    void SetCorsHeaders(httplib::Response& res);
    json CreateErrorResponse(const std::string& error);
    json CreateSuccessResponse(const json& data = json::object());

private:
    int port_;
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    std::atomic<bool> running_;
    
    // 콜백들
    ReloadConfigCallback reload_config_callback_;
    ReinitializeCallback reinitialize_callback_;
    DeviceControlCallback start_device_callback_;
    DeviceControlCallback stop_device_callback_;
    DeviceControlCallback restart_device_callback_;
    DeviceListCallback device_list_callback_;
    DeviceStatusCallback device_status_callback_;
    SystemStatsCallback system_stats_callback_;
    DiagnosticsCallback diagnostics_callback_;
};

} // namespace Network
} // namespace PulseOne

#endif // PULSEONE_REST_API_SERVER_H

// =============================================================================
// collector/src/Network/RestApiServer.cpp
// REST API 서버 구현
// =============================================================================

#include "Network/RestApiServer.h"
#include <iostream>
#include <thread>

using namespace PulseOne::Network;

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

bool RestApiServer::Start() {
#ifdef HAVE_HTTPLIB
    if (running_) {
        return true;
    }
    
    running_ = true;
    
    // 서버를 별도 스레드에서 실행
    server_thread_ = std::thread([this]() {
        std::cout << "🌐 REST API 서버 시작: http://localhost:" << port_ << std::endl;
        server_->listen("0.0.0.0", port_);
    });
    
    return true;
#else
    std::cerr << "❌ HTTP 라이브러리가 없습니다. REST API를 사용할 수 없습니다." << std::endl;
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
    
    std::cout << "🌐 REST API 서버 중지됨" << std::endl;
#endif
}

bool RestApiServer::IsRunning() const {
    return running_;
}

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
    
    // ==========================================================================
    // 🎛️ 디바이스 제어 API
    // ==========================================================================
    
    // 디바이스 목록 조회
    server_->Get("/api/devices", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetDevices(req, res);
    });
    
    // 특정 디바이스 상태 조회
    server_->Get(R"(/api/devices/([^/]+)/status)", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetDeviceStatus(req, res);
    });
    
    // 디바이스 연결
    server_->Post(R"(/api/devices/([^/]+)/connect)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceConnect(req, res);
    });
    
    // 디바이스 연결 해제
    server_->Post(R"(/api/devices/([^/]+)/disconnect)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceDisconnect(req, res);
    });
    
    // 디바이스 재시작
    server_->Post(R"(/api/devices/([^/]+)/restart)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceRestart(req, res);
    });
    
    // 진단 기능 제어
    server_->Post(R"(/api/devices/([^/]+)/diagnostics)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDiagnostics(req, res);
    });
    
    // ==========================================================================
    // 🔧 시스템 제어 API
    // ==========================================================================
    
    // 설정 다시 로드
    server_->Post("/api/system/reload-config", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostReloadConfig(req, res);
    });
    
    // 드라이버 재초기화
    server_->Post("/api/system/reinitialize", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostReinitialize(req, res);
    });
    
    // 시스템 통계
    server_->Get("/api/system/stats", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetSystemStats(req, res);
    });
    
    // ==========================================================================
    // 📊 정적 파일 서빙 (옵션)
    // ==========================================================================
    
    // React 빌드 파일들 서빙
    server_->set_mount_point("/", "./web");
    
    std::cout << "✅ REST API 라우트 설정 완료" << std::endl;
#endif
}

// ==========================================================================
// API 핸들러 구현들
// ==========================================================================

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
        std::string device_id = req.matches[1];
        
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

void RestApiServer::HandlePostDeviceConnect(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = req.matches[1];
        
        if (start_device_callback_) {
            bool success = start_device_callback_(device_id);
            if (success) {
                res.set_content(CreateSuccessResponse({{"message", "Device connection started"}}).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to start device").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Start device callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostDeviceDisconnect(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = req.matches[1];
        
        if (stop_device_callback_) {
            bool success = stop_device_callback_(device_id);
            if (success) {
                res.set_content(CreateSuccessResponse({{"message", "Device disconnection started"}}).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to stop device").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Stop device callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostDeviceRestart(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = req.matches[1];
        
        if (restart_device_callback_) {
            bool success = restart_device_callback_(device_id);
            if (success) {
                res.set_content(CreateSuccessResponse({{"message", "Device restart started"}}).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to restart device").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Restart device callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePostDiagnostics(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = req.matches[1];
        
        // 요청 본문에서 enabled 플래그 파싱
        json request_body = json::parse(req.body);
        bool enabled = request_body.value("enabled", false);
        
        if (diagnostics_callback_) {
            bool success = diagnostics_callback_(device_id, enabled);
            if (success) {
                std::string action = enabled ? "enabled" : "disabled";
                res.set_content(CreateSuccessResponse({{"message", "Diagnostics " + action}}).dump(), "application/json");
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
                res.set_content(CreateSuccessResponse({{"message", "Configuration reload started"}}).dump(), "application/json");
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
                res.set_content(CreateSuccessResponse({{"message", "Driver reinitialization started"}}).dump(), "application/json");
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

// ==========================================================================
// 콜백 설정 메소드들
// ==========================================================================

void RestApiServer::SetReloadConfigCallback(ReloadConfigCallback callback) {
    reload_config_callback_ = callback;
}

void RestApiServer::SetReinitializeCallback(ReinitializeCallback callback) {
    reinitialize_callback_ = callback;
}

void RestApiServer::SetStartDeviceCallback(DeviceControlCallback callback) {
    start_device_callback_ = callback;
}

void RestApiServer::SetStopDeviceCallback(DeviceControlCallback callback) {
    stop_device_callback_ = callback;
}

void RestApiServer::SetRestartDeviceCallback(DeviceControlCallback callback) {
    restart_device_callback_ = callback;
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

// ==========================================================================
// 유틸리티 메소드들
// ==========================================================================

void RestApiServer::SetCorsHeaders(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

json RestApiServer::CreateErrorResponse(const std::string& error) {
    return {
        {"success", false},
        {"error", error},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
}

json RestApiServer::CreateSuccessResponse(const json& data) {
    json response = {
        {"success", true},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
    
    if (!data.empty()) {
        response["data"] = data;
    }
    
    return response;
}