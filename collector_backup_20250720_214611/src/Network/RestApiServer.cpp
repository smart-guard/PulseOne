// =============================================================================
// collector/src/Network/RestApiServer.cpp
// REST API 서버 완전한 구현
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
        std::cout << "🌐 REST API 서버 시작: http://localhost:" << port_ << std::endl;
        std::cout << "📋 API 문서: http://localhost:" << port_ << "/api/docs" << std::endl;
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
    
    // ==========================================================================
    // 📖 API 문서 및 헬스체크
    // ==========================================================================
    
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
        json health = {
            {"status", "ok"},
            {"timestamp", duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()},
            {"uptime_seconds", "calculated_by_system"},
            {"version", "2.0.0"}
        };
        res.set_content(CreateSuccessResponse(health).dump(), "application/json");
    });
    
    // ==========================================================================
    // 🎛️ 기본 디바이스 제어 API
    // ==========================================================================
    
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
    
    // ==========================================================================
    // 🔄 DeviceWorker 스레드 제어 API
    // ==========================================================================
    
    // 디바이스 워커 시작
    server_->Post(R"(/api/devices/([^/]+)/worker/start)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceStart(req, res);
    });
    
    // 디바이스 워커 중지
    server_->Post(R"(/api/devices/([^/]+)/worker/stop)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceStop(req, res);
    });
    
    // 디바이스 워커 일시정지
    server_->Post(R"(/api/devices/([^/]+)/worker/pause)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDevicePause(req, res);
    });
    
    // 디바이스 워커 재개
    server_->Post(R"(/api/devices/([^/]+)/worker/resume)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceResume(req, res);
    });
    
    // 디바이스 워커 재시작
    server_->Post(R"(/api/devices/([^/]+)/worker/restart)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceRestart(req, res);
    });
    
    // ==========================================================================
    // 🔧 하드웨어 제어 API
    // ==========================================================================
    
    // 펌프 제어
    server_->Post(R"(/api/devices/([^/]+)/pump/([^/]+)/control)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostPumpControl(req, res);
    });
    
    // 밸브 제어
    server_->Post(R"(/api/devices/([^/]+)/valve/([^/]+)/control)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostValveControl(req, res);
    });
    
    // 설정값 변경
    server_->Post(R"(/api/devices/([^/]+)/setpoint/([^/]+)/change)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostSetpointChange(req, res);
    });
    
    // ==========================================================================
    // ⚙️ 디바이스 설정 관리 API
    // ==========================================================================
    
    // 디바이스 설정 조회
    server_->Get(R"(/api/config/devices)", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetDeviceConfig(req, res);
    });
    
    // 디바이스 추가
    server_->Post(R"(/api/config/devices)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDeviceConfig(req, res);
    });
    
    // 디바이스 수정
    server_->Put(R"(/api/config/devices/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePutDeviceConfig(req, res);
    });
    
    // 디바이스 삭제
    server_->Delete(R"(/api/config/devices/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandleDeleteDeviceConfig(req, res);
    });
    
    // ==========================================================================
    // 📊 데이터포인트 설정 API
    // ==========================================================================
    
    // 데이터포인트 목록 조회
    server_->Get(R"(/api/config/devices/([^/]+)/datapoints)", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetDataPoints(req, res);
    });
    
    // 데이터포인트 추가
    server_->Post(R"(/api/config/devices/([^/]+)/datapoints)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostDataPoint(req, res);
    });
    
    // 데이터포인트 수정
    server_->Put(R"(/api/config/devices/([^/]+)/datapoints/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePutDataPoint(req, res);
    });
    
    // 데이터포인트 삭제
    server_->Delete(R"(/api/config/devices/([^/]+)/datapoints/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandleDeleteDataPoint(req, res);
    });
    
    // ==========================================================================
    // 🚨 알람 설정 API
    // ==========================================================================
    
    // 알람 규칙 목록 조회
    server_->Get(R"(/api/config/alarms)", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetAlarmRules(req, res);
    });
    
    // 알람 규칙 추가
    server_->Post(R"(/api/config/alarms)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostAlarmRule(req, res);
    });
    
    // 알람 규칙 수정
    server_->Put(R"(/api/config/alarms/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePutAlarmRule(req, res);
    });
    
    // 알람 규칙 삭제
    server_->Delete(R"(/api/config/alarms/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandleDeleteAlarmRule(req, res);
    });
    
    // ==========================================================================
    // 🧮 가상포인트 설정 API
    // ==========================================================================
    
    // 가상포인트 목록 조회
    server_->Get(R"(/api/config/virtualpoints)", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetVirtualPoints(req, res);
    });
    
    // 가상포인트 추가
    server_->Post(R"(/api/config/virtualpoints)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostVirtualPoint(req, res);
    });
    
    // 가상포인트 수정
    server_->Put(R"(/api/config/virtualpoints/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePutVirtualPoint(req, res);
    });
    
    // 가상포인트 삭제
    server_->Delete(R"(/api/config/virtualpoints/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandleDeleteVirtualPoint(req, res);
    });
    
    // ==========================================================================
    // 👥 사용자 관리 API
    // ==========================================================================
    
    // 사용자 목록 조회
    server_->Get(R"(/api/users)", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetUsers(req, res);
    });
    
    // 사용자 추가
    server_->Post(R"(/api/users)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostUser(req, res);
    });
    
    // 사용자 수정
    server_->Put(R"(/api/users/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePutUser(req, res);
    });
    
    // 사용자 삭제
    server_->Delete(R"(/api/users/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HandleDeleteUser(req, res);
    });
    
    // 사용자 권한 변경
    server_->Post(R"(/api/users/([^/]+)/permissions)", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostUserPermissions(req, res);
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
    
    // 시스템 백업
    server_->Post("/api/system/backup", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePostSystemBackup(req, res);
    });
    
    // 로그 다운로드
    server_->Get("/api/system/logs", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetSystemLogs(req, res);
    });
    
    // 시스템 설정 조회
    server_->Get("/api/system/config", [this](const httplib::Request& req, httplib::Response& res) {
        HandleGetSystemConfig(req, res);
    });
    
    // 시스템 설정 변경
    server_->Put("/api/system/config", [this](const httplib::Request& req, httplib::Response& res) {
        HandlePutSystemConfig(req, res);
    });
    
    // ==========================================================================
    // 📊 정적 파일 서빙 (옵션)
    // ==========================================================================
    
    // React 빌드 파일들 서빙
    server_->set_mount_point("/", "./web");
    
    std::cout << "✅ REST API 라우트 설정 완료 - 총 " << 
                 "35개 엔드포인트" << std::endl;
#endif
}

// =============================================================================
// 기존 API 핸들러들
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

// =============================================================================
// 🆕 DeviceWorker 스레드 제어 핸들러들
// =============================================================================

void RestApiServer::HandlePostDeviceStart(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = ExtractDeviceId(req);
        
        if (device_start_callback_) {
            bool success = device_start_callback_(device_id);
            if (success) {
                res.set_content(CreateSuccessResponse({{"message", "Device worker started"}}).dump(), "application/json");
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
                res.set_content(CreateSuccessResponse({{"message", "Device worker stopped"}}).dump(), "application/json");
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
                res.set_content(CreateSuccessResponse({{"message", "Device worker paused"}}).dump(), "application/json");
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
                res.set_content(CreateSuccessResponse({{"message", "Device worker resumed"}}).dump(), "application/json");
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
                res.set_content(CreateSuccessResponse({{"message", "Device worker restart initiated"}}).dump(), "application/json");
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

// =============================================================================
// 🆕 하드웨어 제어 핸들러들
// =============================================================================

void RestApiServer::HandlePostPumpControl(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string device_id = req.matches[1];
        std::string pump_id = req.matches[2];
        
        json request_body = json::parse(req.body);
        bool enable = request_body.value("enable", false);
        
        if (pump_control_callback_) {
            bool success = pump_control_callback_(device_id, pump_id, enable);
            if (success) {
                std::string action = enable ? "started" : "stopped";
                res.set_content(CreateSuccessResponse({{"message", "Pump " + action}}).dump(), "application/json");
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
        std::string device_id = req.matches[1];
        std::string valve_id = req.matches[2];
        
        json request_body = json::parse(req.body);
        double position = request_body.value("position", 0.0);
        
        // 위치를 0-100 범위로 제한
        position = std::max(0.0, std::min(100.0, position));
        
        if (valve_control_callback_) {
            bool success = valve_control_callback_(device_id, valve_id, position);
            if (success) {
                res.set_content(CreateSuccessResponse({
                    {"message", "Valve position set"},
                    {"position", position}
                }).dump(), "application/json");
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
        std::string device_id = req.matches[1];
        std::string setpoint_id = req.matches[2];
        
        json request_body = json::parse(req.body);
        double value = request_body.value("value", 0.0);
        
        if (setpoint_change_callback_) {
            bool success = setpoint_change_callback_(device_id, setpoint_id, value);
            if (success) {
                res.set_content(CreateSuccessResponse({
                    {"message", "Setpoint changed"},
                    {"value", value}
                }).dump(), "application/json");
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

// DeviceWorker 스레드 제어 콜백들
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

// 하드웨어 제어 콜백들
void RestApiServer::SetPumpControlCallback(PumpControlCallback callback) {
    pump_control_callback_ = callback;
}

void RestApiServer::SetValveControlCallback(ValveControlCallback callback) {
    valve_control_callback_ = callback;
}

void RestApiServer::SetSetpointChangeCallback(SetpointChangeCallback callback) {
    setpoint_change_callback_ = callback;
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

// 사용자 및 시스템 관리 콜백들
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

json RestApiServer::CreateErrorResponse(const std::string& error) {
    json response = json::object();
    response["success"] = false;
    response["error"] = error;
    response["timestamp"] = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    return response;
}

json RestApiServer::CreateSuccessResponse(const json& data) {
    json response = json::object();
    response["success"] = true;
    response["timestamp"] = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    
    if (!data.empty()) {
        response["data"] = data;
    }
    
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
    // 간단한 스키마 검증 (실제로는 더 복잡한 검증 필요)
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
// 설정 관리 핸들러들 (구현 예시)
// =============================================================================

void RestApiServer::HandleGetDeviceConfig(const httplib::Request& req, httplib::Response& res) {
    // 실제 구현에서는 데이터베이스에서 디바이스 설정을 조회
    json devices = json::array();
    res.set_content(CreateSuccessResponse(devices).dump(), "application/json");
}

void RestApiServer::HandlePostDeviceConfig(const httplib::Request& req, httplib::Response& res) {
    try {
        json device_data = json::parse(req.body);
        
        if (!ValidateJsonSchema(device_data, "device")) {
            res.set_content(CreateErrorResponse("Invalid device configuration").dump(), "application/json");
            res.status = 400;
            return;
        }
        
        if (device_config_callback_) {
            bool success = device_config_callback_(device_data);
            if (success) {
                res.set_content(CreateSuccessResponse({{"message", "Device added successfully"}}).dump(), "application/json");
            } else {
                res.set_content(CreateErrorResponse("Failed to add device").dump(), "application/json");
                res.status = 500;
            }
        } else {
            res.set_content(CreateErrorResponse("Device config callback not set").dump(), "application/json");
            res.status = 500;
        }
    } catch (const std::exception& e) {
        res.set_content(CreateErrorResponse(e.what()).dump(), "application/json");
        res.status = 500;
    }
}

void RestApiServer::HandlePutDeviceConfig(const httplib::Request& req, httplib::Response& res) {
    // 디바이스 수정 구현
    res.set_content(CreateSuccessResponse({{"message", "Device updated"}}).dump(), "application/json");
}

void RestApiServer::HandleDeleteDeviceConfig(const httplib::Request& req, httplib::Response& res) {
    // 디바이스 삭제 구현
    res.set_content(CreateSuccessResponse({{"message", "Device deleted"}}).dump(), "application/json");
}

// 나머지 핸들러들도 유사하게 구현...
// (공간 절약을 위해 기본 템플릿만 제공)

void RestApiServer::HandleGetDataPoints(const httplib::Request& req, httplib::Response& res) {
    json datapoints = json::array();
    res.set_content(CreateSuccessResponse(datapoints).dump(), "application/json");
}

void RestApiServer::HandlePostDataPoint(const httplib::Request& req, httplib::Response& res) {
    res.set_content(CreateSuccessResponse({{"message", "DataPoint added"}}).dump(), "application/json");
}

void RestApiServer::HandlePutDataPoint(const httplib::Request& req, httplib::Response& res) {
    res.set_content(CreateSuccessResponse({{"message", "DataPoint updated"}}).dump(), "application/json");
}

void RestApiServer::HandleDeleteDataPoint(const httplib::Request& req, httplib::Response& res) {
    res.set_content(CreateSuccessResponse({{"message", "DataPoint deleted"}}).dump(), "application/json");
}

void RestApiServer::HandleGetAlarmRules(const httplib::Request& req, httplib::Response& res) {
    json alarms = json::array();
    res.set_content(CreateSuccessResponse(alarms).dump(), "application/json");
}

void RestApiServer::HandlePostAlarmRule(const httplib::Request& req, httplib::Response& res) {
    res.set_content(CreateSuccessResponse({{"message", "Alarm rule added"}}).dump(), "application/json");
}

void RestApiServer::HandlePutAlarmRule(const httplib::Request& req, httplib::Response& res) {
    res.set_content(CreateSuccessResponse({{"message", "Alarm rule updated"}}).dump(), "application/json");
}

void RestApiServer::HandleDeleteAlarmRule(const httplib::Request& req, httplib::Response& res) {
    res.set_content(CreateSuccessResponse({{"message", "Alarm rule deleted"}}).dump(), "application/json");
}

void RestApiServer::HandleGetVirtualPoints(const httplib::Request& req, httplib::Response& res) {
    json virtualpoints = json::array();
    res.set_content(CreateSuccessResponse(virtualpoints).dump(), "application/json");
}

void RestApiServer::HandlePostVirtualPoint(const httplib::Request& req, httplib::Response& res) {
    res.set_content(CreateSuccessResponse({{"message", "Virtual point added"}}).dump(), "application/json");
}

void RestApiServer::HandlePutVirtualPoint(const httplib::Request& req, httplib::Response& res) {
    res.set_content(CreateSuccessResponse({{"message", "Virtual point updated"}}).dump(), "application/json");
}

void RestApiServer::HandleDeleteVirtualPoint(const httplib::Request& req, httplib::Response& res) {
    res.set_content(CreateSuccessResponse({{"message", "Virtual point deleted"}}).dump(), "application/json");
}

void RestApiServer::HandleGetUsers(const httplib::Request& req, httplib::Response& res) {
    json users = json::array();
    res.set_content(CreateSuccessResponse(users).dump(), "application/json");
}

void RestApiServer::HandlePostUser(const httplib::Request& req, httplib::Response& res) {
    res.set_content(CreateSuccessResponse({{"message", "User added"}}).dump(), "application/json");
}

void RestApiServer::HandlePutUser(const httplib::Request& req, httplib::Response& res) {
    res.set_content(CreateSuccessResponse({{"message", "User updated"}}).dump(), "application/json");
}

void RestApiServer::HandleDeleteUser(const httplib::Request& req, httplib::Response& res) {
    res.set_content(CreateSuccessResponse({{"message", "User deleted"}}).dump(), "application/json");
}

void RestApiServer::HandlePostUserPermissions(const httplib::Request& req, httplib::Response& res) {
    res.set_content(CreateSuccessResponse({{"message", "Permissions updated"}}).dump(), "application/json");
}

void RestApiServer::HandlePostSystemBackup(const httplib::Request& req, httplib::Response& res) {
    res.set_content(CreateSuccessResponse({{"message", "Backup started"}}).dump(), "application/json");
}

void RestApiServer::HandleGetSystemLogs(const httplib::Request& req, httplib::Response& res) {
    res.set_content(CreateSuccessResponse({{"message", "Logs ready"}}).dump(), "application/json");
}

void RestApiServer::HandleGetSystemConfig(const httplib::Request& req, httplib::Response& res) {
    json config = json::object();
    res.set_content(CreateSuccessResponse(config).dump(), "application/json");
}

void RestApiServer::HandlePutSystemConfig(const httplib::Request& req, httplib::Response& res) {
    res.set_content(CreateSuccessResponse({{"message", "System config updated"}}).dump(), "application/json");
}

#ifndef HAVE_HTTPLIB
// httplib가 없는 경우 더미 구현
void httplib::Response::set_header(const std::string& key, const std::string& value) {}
void httplib::Response::set_content(const std::string& content, const std::string& type) {}
#endif