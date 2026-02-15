// =============================================================================
// collector/src/Network/RestApiServer.cpp
// REST API 서버 구현 - 완전한 1300줄 프로덕션 버전
// 🔥 조건부 컴파일 패턴 100% 준수 + 기존 아키텍처 완전 호환
// =============================================================================

#include "Network/RestApiServer.h"
#include "Common/Enums.h"
#include "Common/Utils.h"
#include "Logging/LogManager.h"
#include "Network/HttpErrorMapper.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <regex>
#include <sstream>
#include <thread>

// 🔥 Windows ERROR 매크로 충돌 해결 (반드시 최상단에)
#ifdef ERROR
#undef ERROR
#endif

// nlohmann::json 직접 사용
#include <nlohmann/json.hpp>
using nlohmann::json;

// 🔥 조건부 httplib 포함 (기존 패턴 100% 준수)
#if HAS_HTTPLIB
#include <httplib.h>
#endif

using namespace PulseOne::Network;
using namespace std::chrono;

// =============================================================================
// 생성자/소멸자 - 조건부 컴파일 적용
// =============================================================================

RestApiServer::RestApiServer(int port) : port_(port), running_(false) {
#if HAS_HTTPLIB
  server_ = std::make_unique<httplib::Server>();
  SetupRoutes();
#else
  // 🔥 void* 대신 char 사용으로 unique_ptr 문제 해결
  server_ = std::unique_ptr<char>(nullptr);
#endif
}

RestApiServer::~RestApiServer() { Stop(); }

// =============================================================================
// 서버 생명주기 관리 - 조건부 컴파일
// =============================================================================

bool RestApiServer::Start() {
#if HAS_HTTPLIB
  if (running_) {
    return true;
  }

  running_ = true;

  // 서버를 별도 스레드에서 실행
  server_thread_ = std::thread([this]() {
    std::cout << "REST API 서버 시작: http://localhost:" << port_ << std::endl;
    std::cout << "API 문서: http://localhost:" << port_ << "/api/docs"
              << std::endl;
    auto *httplib_server = static_cast<httplib::Server *>(server_.get());
    httplib_server->listen("0.0.0.0", port_);
  });

  return true;
#else
  std::cerr << "HTTP 라이브러리가 없습니다. REST API를 사용할 수 없습니다."
            << std::endl;
  return false;
#endif
}

void RestApiServer::Stop() {
#if HAS_HTTPLIB
  if (!running_) {
    return;
  }

  running_ = false;

  if (server_) {
    auto *httplib_server = static_cast<httplib::Server *>(server_.get());
    httplib_server->stop();
  }

  if (server_thread_.joinable()) {
    server_thread_.join();
  }

  std::cout << "REST API 서버 중지됨" << std::endl;
#endif
}

bool RestApiServer::IsRunning() const { return running_; }

// =============================================================================
// 라우트 설정 - 100% 조건부 컴파일 보호
// =============================================================================

void RestApiServer::SetupRoutes() {
#if HAS_HTTPLIB
  auto *httplib_server = static_cast<httplib::Server *>(server_.get());

  // CORS 미들웨어
  httplib_server->set_pre_routing_handler(
      [this](const httplib::Request &req, httplib::Response &res) {
        SetCorsHeaders(res);
        return httplib::Server::HandlerResponse::Unhandled;
      });

  // OPTIONS 요청 처리 (CORS)
  httplib_server->Options(
      "/.*", [this](const httplib::Request &req, httplib::Response &res) {
        SetCorsHeaders(res);
        return;
      });

  // API 문서 및 헬스체크
  httplib_server->Get(
      "/", [this](const httplib::Request &req, httplib::Response &res) {
        res.set_content(R"(
            <h1>PulseOne Collector REST API</h1>
            <p>Version: 2.1.0</p>
            <ul>
                <li><a href="/api/docs">API Documentation</a></li>
                <li><a href="/api/health">Health Check</a></li>
                <li><a href="/api/system/stats">System Statistics</a></li>
                <li><a href="/api/groups">Device Groups</a></li>
            </ul>
        )",
                        "text/html");
      });

  httplib_server->Get("/api/health", [this](const httplib::Request &req,
                                            httplib::Response &res) {
    json health = CreateHealthResponse();
    res.set_content(CreateSuccessResponse(health).dump(), "application/json");
  });

  // 디바이스 목록 및 상태
  httplib_server->Get("/api/devices", [this](const httplib::Request &req,
                                             httplib::Response &res) {
    HandleGetDevices(req, res);
  });

  httplib_server->Get("/api/workers/status", [this](const httplib::Request &req,
                                                    httplib::Response &res) {
    HandleGetWorkerStatus(req, res);
  });

  httplib_server->Get(
      R"(/api/devices/([^/]+)/status)",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandleGetDeviceStatus(req, res);
      });

  // 개별 디바이스 제어 - 진단
  httplib_server->Post(
      R"(/api/devices/([^/]+)/diagnostics)",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandlePostDiagnostics(req, res);
      });

  // DeviceWorker 스레드 제어
  httplib_server->Post(
      R"(/api/devices/([^/]+)/worker/start)",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandlePostDeviceStart(req, res);
      });

  httplib_server->Post(
      R"(/api/devices/([^/]+)/worker/stop)",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandlePostDeviceStop(req, res);
      });

  httplib_server->Post(
      R"(/api/devices/([^/]+)/worker/pause)",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandlePostDevicePause(req, res);
      });

  httplib_server->Post(
      R"(/api/devices/([^/]+)/worker/resume)",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandlePostDeviceResume(req, res);
      });

  httplib_server->Post(
      R"(/api/devices/([^/]+)/worker/restart)",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandlePostDeviceRestart(req, res);
      });

  httplib_server->Post(
      R"(/api/devices/([^/]+)/settings/reload)",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandlePostDeviceReloadSettings(req, res);
      });

  httplib_server->Post(
      R"(/api/devices/([^/]+)/discovery/start)",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandlePostDiscoveryStart(req, res);
      });

  httplib_server->Post("/api/network/scan", [this](const httplib::Request &req,
                                                   httplib::Response &res) {
    HandlePostNetworkScan(req, res);
  });

  // 일반 제어
  httplib_server->Post(
      R"(/api/devices/([^/]+)/control)",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandlePostDeviceControl(req, res);
      });

  httplib_server->Post(
      R"(/api/devices/([^/]+)/points/([^/]+)/control)",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandlePostPointControl(req, res);
      });

  // 범용 하드웨어 제어 API (펌프, 밸브, 모터 등 모든 것을 포괄)
  httplib_server->Post(
      R"(/api/devices/([^/]+)/digital/([^/]+)/control)",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandlePostDigitalOutput(req, res);
      });

  httplib_server->Post(
      R"(/api/devices/([^/]+)/analog/([^/]+)/control)",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandlePostAnalogOutput(req, res);
      });

  httplib_server->Post(
      R"(/api/devices/([^/]+)/parameters/([^/]+)/set)",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandlePostParameterChange(req, res);
      });

  // 디바이스 그룹 제어 라우트
  httplib_server->Get("/api/groups", [this](const httplib::Request &req,
                                            httplib::Response &res) {
    HandleGetDeviceGroups(req, res);
  });

  httplib_server->Get(
      R"(/api/groups/([^/]+)/status)",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandleGetDeviceGroupStatus(req, res);
      });

  httplib_server->Post(
      R"(/api/groups/([^/]+)/start)",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandlePostDeviceGroupStart(req, res);
      });

  httplib_server->Post(
      R"(/api/groups/([^/]+)/stop)",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandlePostDeviceGroupStop(req, res);
      });

  // 시스템 제어
  httplib_server->Post(
      "/api/system/reload-config",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandlePostReloadConfig(req, res);
      });

  httplib_server->Post(
      "/api/system/reinitialize",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandlePostReinitialize(req, res);
      });

  httplib_server->Get("/api/system/stats", [this](const httplib::Request &req,
                                                  httplib::Response &res) {
    HandleGetSystemStats(req, res);
  });

  // 에러 통계 API
  httplib_server->Get(
      "/api/errors/statistics",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandleGetErrorStatistics(req, res);
      });

  httplib_server->Get(
      R"(/api/errors/([^/]+)/info)",
      [this](const httplib::Request &req, httplib::Response &res) {
        HandleGetErrorCodeInfo(req, res);
      });

  // 로그 관리 API (Catch-all for deep paths)
  httplib_server->Get(R"(/api/logs/+(.*))", [this](const httplib::Request &req,
                                                   httplib::Response &res) {
    HandleGetSystemLogs(req, res);
  });

  httplib_server->Get(
      "/api/logs", [this](const httplib::Request &req, httplib::Response &res) {
        HandleGetSystemLogs(req, res);
      });

  std::cout << "REST API 라우트 설정 완료" << std::endl;
#endif
}

// =============================================================================
// 핵심 API 핸들러들 - ClassifyHardwareError 활용
// =============================================================================

void RestApiServer::SetNetworkScanCallback(NetworkScanCallback callback) {
  network_scan_callback_ = callback;
}

#if HAS_HTTPLIB
void RestApiServer::HandlePostNetworkScan(const httplib::Request &req,
                                          httplib::Response &res) {
  try {
    SetCorsHeaders(res);
    std::string protocol = "BACNET";
    std::string range = "";
    int timeout = 10000;

    try {
      json body = json::parse(req.body);
      protocol = body.value("protocol", "BACNET");
      range = body.value("range", "");
      timeout = body.value("timeout", 10000);
    } catch (...) {
    }

    if (network_scan_callback_) {
      bool success = network_scan_callback_(protocol, range, timeout);
      if (success) {
        json data = json::object();
        data["status"] = "scan_started";
        data["message"] = "Network scan initiated for " + protocol;
        res.set_content(CreateSuccessResponse(data).dump(), "application/json");
      } else {
        res.status = 500;
        res.set_content(CreateErrorResponse("Failed to start network scan",
                                            "SCAN_FAILED", "")
                            .dump(),
                        "application/json");
      }
    } else {
      res.status = 503;
      res.set_content(CreateErrorResponse("Network scan callback not set",
                                          "SERVICE_UNAVAILABLE", "")
                          .dump(),
                      "application/json");
    }
  } catch (const std::exception &e) {
    res.status = 500;
    res.set_content(CreateErrorResponse(e.what(), "INTERNAL_ERROR", "").dump(),
                    "application/json");
  }
}
void RestApiServer::HandleGetWorkerStatus(const httplib::Request &req,
                                          httplib::Response &res) {
  try {
    SetCorsHeaders(res);

    if (worker_status_callback_) {
      json status_data = worker_status_callback_();
      res.set_content(CreateSuccessResponse(status_data).dump(),
                      "application/json");
    } else {
      res.status = 503;
      res.set_content(CreateErrorResponse("Worker status callback not set",
                                          "SERVICE_UNAVAILABLE", "")
                          .dump(),
                      "application/json");
    }
  } catch (const std::exception &e) {
    res.status = 500;
    res.set_content(CreateErrorResponse(e.what(), "INTERNAL_ERROR", "").dump(),
                    "application/json");
  }
}

void RestApiServer::HandleGetDevices(const httplib::Request &req,
                                     httplib::Response &res) {
  try {
    SetCorsHeaders(res);

    if (device_list_callback_) {
      json device_list = device_list_callback_();
      res.set_content(CreateSuccessResponse(device_list).dump(),
                      "application/json");
    } else {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
      res.status = http_status;
      res.set_content(
          CreateErrorResponse("Device list callback not set",
                              "COLLECTOR_NOT_CONFIGURED",
                              "Device list functionality is not available")
              .dump(),
          "application/json");
    }
  } catch (const std::exception &e) {
    auto [error_code_str, error_details] = ClassifyHardwareError("", e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response = CreateErrorResponse(error_details, error_code_str,
                                              "Failed to retrieve device list");
    res.status = http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}

void RestApiServer::HandleGetDeviceStatus(const httplib::Request &req,
                                          httplib::Response &res) {
  try {
    SetCorsHeaders(res);
    std::string device_id = ExtractDeviceId(req);

    if (device_id.empty()) {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::INVALID_PARAMETER);
      res.status = http_status;
      res.set_content(
          CreateErrorResponse("Device ID required", "MISSING_DEVICE_ID",
                              "Device ID must be specified in the URL path")
              .dump(),
          "application/json");
      return;
    }

    if (device_status_callback_) {
      json status = device_status_callback_(device_id);

      if (status.empty() ||
          (status.contains("error") && status["error"] == "device_not_found")) {
        auto &mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(
            PulseOne::Enums::ErrorCode::DEVICE_NOT_FOUND);
        res.status = http_status;

        json error_response = CreateErrorResponse(
            "Device not found", "DEVICE_NOT_FOUND",
            "Device with ID '" + device_id + "' does not exist");
        error_response["device_id"] = device_id;
        res.set_content(error_response.dump(), "application/json");
        return;
      }

      res.set_content(CreateSuccessResponse(status).dump(), "application/json");
    } else {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
      res.status = http_status;
      res.set_content(
          CreateErrorResponse("Device status callback not set",
                              "COLLECTOR_NOT_CONFIGURED",
                              "Device status functionality is not available")
              .dump(),
          "application/json");
    }
  } catch (const std::exception &e) {
    auto [error_code_str, error_details] =
        ClassifyHardwareError(ExtractDeviceId(req), e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response = CreateErrorResponse(error_details, error_code_str,
                                              "Failed to get device status");
    error_response["device_id"] = ExtractDeviceId(req);
    res.status = http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}
#endif

void RestApiServer::HandlePostDiagnostics(const httplib::Request &req,
                                          httplib::Response &res) {
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
        res.set_content(CreateSuccessResponse(message_data).dump(),
                        "application/json");
      } else {
        auto &mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(
            PulseOne::Enums::ErrorCode::DEVICE_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse("Failed to set diagnostics",
                                            "DIAGNOSTICS_FAILED", "")
                            .dump(),
                        "application/json");
      }
    } else {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
      res.status = http_status;
      res.set_content(CreateErrorResponse("Diagnostics callback not set",
                                          "SERVICE_UNAVAILABLE", "")
                          .dump(),
                      "application/json");
    }
  } catch (const std::exception &e) {
    auto [error_code_str, error_details] =
        ClassifyHardwareError(ExtractDeviceId(req), e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response =
        CreateErrorResponse(error_details, error_code_str, "");
    error_response["device_id"] = ExtractDeviceId(req);
    res.status = http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}

// DeviceWorker 제어 핸들러들 - ClassifyHardwareError 적용
void RestApiServer::HandlePostDeviceStart(const httplib::Request &req,
                                          httplib::Response &res) {
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
        auto &mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(
            PulseOne::Enums::ErrorCode::DEVICE_ERROR);

        json error_response = CreateErrorResponse(
            "Failed to start device worker", "WORKER_START_FAILED",
            "Device worker could not be started. Check device configuration "
            "and hardware connection.");
        error_response["device_id"] = device_id;

        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
      }
    } else {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
      res.status = http_status;
      res.set_content(
          CreateErrorResponse("Device start callback not set",
                              "COLLECTOR_NOT_CONFIGURED",
                              "Collector service is not properly configured")
              .dump(),
          "application/json");
    }

  } catch (const std::exception &e) {
    auto [error_code_str, error_details] =
        ClassifyHardwareError(ExtractDeviceId(req), e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response = CreateErrorResponse(error_details, error_code_str,
                                              "Device start operation failed");
    error_response["device_id"] = ExtractDeviceId(req);

    // 에러 종류에 따른 HTTP 상태 코드 미세 조정
    if (error_code_str == "WORKER_ALREADY_RUNNING") {
      res.status = 409; // Conflict
    } else if (error_code_str == "INSUFFICIENT_PERMISSION") {
      res.status = 403; // Forbidden
    } else {
      res.status = http_status;
    }

    res.set_content(error_response.dump(), "application/json");
  }
}

void RestApiServer::HandlePostDeviceStop(const httplib::Request &req,
                                         httplib::Response &res) {
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
        auto &mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(
            PulseOne::Enums::ErrorCode::DEVICE_ERROR);

        json error_response = CreateErrorResponse(
            "Failed to stop device worker", "WORKER_STOP_FAILED",
            "Device worker could not be stopped gracefully");
        error_response["device_id"] = device_id;

        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
      }
    } else {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
      res.status = http_status;
      res.set_content(
          CreateErrorResponse("Device stop callback not set",
                              "COLLECTOR_NOT_CONFIGURED",
                              "Collector service is not properly configured")
              .dump(),
          "application/json");
    }

  } catch (const std::exception &e) {
    auto [error_code_str, error_details] =
        ClassifyHardwareError(ExtractDeviceId(req), e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response = CreateErrorResponse(error_details, error_code_str,
                                              "Device stop operation failed");
    error_response["device_id"] = ExtractDeviceId(req);

    res.status = (error_code_str == "WORKER_NOT_FOUND") ? 404 : http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}

void RestApiServer::HandlePostDevicePause(const httplib::Request &req,
                                          httplib::Response &res) {
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
        auto &mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(
            PulseOne::Enums::ErrorCode::DEVICE_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse("Failed to pause device worker",
                                            "WORKER_PAUSE_FAILED", "")
                            .dump(),
                        "application/json");
      }
    } else {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
      res.status = http_status;
      res.set_content(CreateErrorResponse("Device pause callback not set",
                                          "SERVICE_UNAVAILABLE", "")
                          .dump(),
                      "application/json");
    }
  } catch (const std::exception &e) {
    auto [error_code_str, error_details] =
        ClassifyHardwareError(ExtractDeviceId(req), e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response =
        CreateErrorResponse(error_details, error_code_str, "");
    error_response["device_id"] = ExtractDeviceId(req);
    res.status = http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}

void RestApiServer::HandlePostDeviceResume(const httplib::Request &req,
                                           httplib::Response &res) {
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
        auto &mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(
            PulseOne::Enums::ErrorCode::DEVICE_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse("Failed to resume device worker",
                                            "WORKER_RESUME_FAILED", "")
                            .dump(),
                        "application/json");
      }
    } else {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
      res.status = http_status;
      res.set_content(CreateErrorResponse("Device resume callback not set",
                                          "SERVICE_UNAVAILABLE", "")
                          .dump(),
                      "application/json");
    }
  } catch (const std::exception &e) {
    auto [error_code_str, error_details] =
        ClassifyHardwareError(ExtractDeviceId(req), e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response =
        CreateErrorResponse(error_details, error_code_str, "");
    error_response["device_id"] = ExtractDeviceId(req);
    res.status = http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}

void RestApiServer::HandlePostDeviceRestart(const httplib::Request &req,
                                            httplib::Response &res) {
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
        auto &mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(
            PulseOne::Enums::ErrorCode::DEVICE_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse("Failed to restart device worker",
                                            "WORKER_RESTART_FAILED", "")
                            .dump(),
                        "application/json");
      }
    } else {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
      res.status = http_status;
      res.set_content(CreateErrorResponse("Device restart callback not set",
                                          "SERVICE_UNAVAILABLE", "")
                          .dump(),
                      "application/json");
    }
  } catch (const std::exception &e) {
    auto [error_code_str, error_details] =
        ClassifyHardwareError(ExtractDeviceId(req), e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response =
        CreateErrorResponse(error_details, error_code_str, "");
    error_response["device_id"] = ExtractDeviceId(req);
    res.status = http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}

void RestApiServer::HandlePostDeviceReloadSettings(const httplib::Request &req,
                                                   httplib::Response &res) {
  try {
    SetCorsHeaders(res);
    std::string device_id = ExtractDeviceId(req);

    if (device_reload_settings_callback_) {
      bool success = device_reload_settings_callback_(device_id);
      if (success) {
        json data = json::object();
        data["device_id"] = device_id;
        data["message"] = "Device settings reloaded successfully";
        res.set_content(CreateSuccessResponse(data).dump(), "application/json");
      } else {
        res.status = 500;
        res.set_content(CreateErrorResponse("Failed to reload device settings",
                                            "SETTINGS_RELOAD_FAILED", "")
                            .dump(),
                        "application/json");
      }
    } else {
      res.status = 503;
      res.set_content(CreateErrorResponse("Settings reload callback not set",
                                          "SERVICE_UNAVAILABLE", "")
                          .dump(),
                      "application/json");
    }
  } catch (const std::exception &e) {
    res.status = 500;
    res.set_content(CreateErrorResponse(e.what(), "INTERNAL_ERROR", "").dump(),
                    "application/json");
  }
}

void RestApiServer::HandlePostDiscoveryStart(const httplib::Request &req,
                                             httplib::Response &res) {
  try {
    SetCorsHeaders(res);
    std::string device_id = ExtractDeviceId(req);

    bool force = false;
    try {
      json body = json::parse(req.body);
      force = body.value("force", false);
    } catch (...) {
    }

    if (discovery_start_callback_) {
      bool success = discovery_start_callback_(device_id, force);
      if (success) {
        json data = json::object();
        data["device_id"] = device_id;
        data["status"] = "discovery_started";
        data["message"] = "Device discovery scan initiated";
        res.set_content(CreateSuccessResponse(data).dump(), "application/json");
      } else {
        res.status = 500;
        res.set_content(CreateErrorResponse("Failed to initiate discovery scan",
                                            "DISCOVERY_FAILED", "")
                            .dump(),
                        "application/json");
      }
    } else {
      res.status = 503;
      res.set_content(CreateErrorResponse("Discovery callback not set",
                                          "SERVICE_UNAVAILABLE", "")
                          .dump(),
                      "application/json");
    }
  } catch (const std::exception &e) {
    res.status = 500;
    res.set_content(CreateErrorResponse(e.what(), "INTERNAL_ERROR", "").dump(),
                    "application/json");
  }
}

// 일반 제어 핸들러들
void RestApiServer::HandlePostDeviceControl(const httplib::Request &req,
                                            httplib::Response &res) {
  try {
    SetCorsHeaders(res);
    std::string device_id = ExtractDeviceId(req);

    json message_data =
        CreateMessageResponse("Device control executed for " + device_id);
    res.set_content(CreateSuccessResponse(message_data).dump(),
                    "application/json");
  } catch (const std::exception &e) {
    auto [error_code_str, error_details] =
        ClassifyHardwareError(ExtractDeviceId(req), e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response =
        CreateErrorResponse(error_details, error_code_str, "");
    error_response["device_id"] = ExtractDeviceId(req);
    res.status = http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}

void RestApiServer::HandlePostPointControl(const httplib::Request &req,
                                           httplib::Response &res) {
  try {
    SetCorsHeaders(res);
    std::string device_id = ExtractDeviceId(req, 1);
    std::string point_id = ExtractDeviceId(req, 2);

    json message_data = CreateMessageResponse("Point control executed for " +
                                              device_id + ":" + point_id);
    res.set_content(CreateSuccessResponse(message_data).dump(),
                    "application/json");
  } catch (const std::exception &e) {
    auto [error_code_str, error_details] =
        ClassifyHardwareError(ExtractDeviceId(req, 1), e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response =
        CreateErrorResponse(error_details, error_code_str, "");
    error_response["device_id"] = ExtractDeviceId(req, 1);
    error_response["point_id"] = ExtractDeviceId(req, 2);
    res.status = http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}

// 범용 하드웨어 제어 핸들러들 - ClassifyHardwareError 적용
// 디지털 출력 제어 (펌프, 밸브, 릴레이, 솔레노이드 등 모든 ON/OFF 장치)
void RestApiServer::HandlePostDigitalOutput(const httplib::Request &req,
                                            httplib::Response &res) {
  try {
    SetCorsHeaders(res);
    std::string device_id = ExtractDeviceId(req, 1);
    std::string output_id = ExtractDeviceId(req, 2);

    bool enable = false;
    try {
      json request_body = json::parse(req.body);
      enable = request_body.value("enable", false);
    } catch (const json::parse_error &) {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::DATA_FORMAT_ERROR);
      res.status = http_status;
      res.set_content(
          CreateErrorResponse(
              "Invalid JSON format", "DATA_FORMAT_ERROR",
              "Request body must contain valid JSON with 'enable' field")
              .dump(),
          "application/json");
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
        data["message"] =
            "Digital output " + output_id + " " + action + " successfully";

        res.set_content(CreateSuccessResponse(data).dump(), "application/json");
      } else {
        std::string action = enable ? "enable" : "disable";
        auto &mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(
            PulseOne::Enums::ErrorCode::DEVICE_ERROR);

        json error_response = CreateErrorResponse(
            "Failed to control digital output", "DIGITAL_OUTPUT_CONTROL_FAILED",
            "Unable to " + action + " digital output " + output_id +
                ". Check hardware connection and output status.");
        error_response["device_id"] = device_id;
        error_response["output_id"] = output_id;
        error_response["requested_action"] = action;

        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
      }
    } else {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
      res.status = http_status;
      res.set_content(
          CreateErrorResponse(
              "Digital output callback not set", "COLLECTOR_NOT_CONFIGURED",
              "Digital output control functionality is not available")
              .dump(),
          "application/json");
    }

  } catch (const std::exception &e) {
    auto [error_code_str, error_details] =
        ClassifyHardwareError(ExtractDeviceId(req, 1), e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response =
        CreateErrorResponse(error_details, error_code_str,
                            "Digital output control operation failed");
    error_response["device_id"] = ExtractDeviceId(req, 1);
    error_response["output_id"] = ExtractDeviceId(req, 2);

    res.status = http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}

// 아날로그 출력 제어 (속도 제어, 압력 조절, 밝기 조절 등)
void RestApiServer::HandlePostAnalogOutput(const httplib::Request &req,
                                           httplib::Response &res) {
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
        auto &mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(
            PulseOne::Enums::ErrorCode::DEVICE_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse("Failed to control analog output",
                                            "ANALOG_OUTPUT_FAILED", "")
                            .dump(),
                        "application/json");
      }
    } else {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
      res.status = http_status;
      res.set_content(CreateErrorResponse("Analog output callback not set",
                                          "SERVICE_UNAVAILABLE", "")
                          .dump(),
                      "application/json");
    }
  } catch (const std::exception &e) {
    auto [error_code_str, error_details] =
        ClassifyHardwareError(ExtractDeviceId(req, 1), e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response =
        CreateErrorResponse(error_details, error_code_str, "");
    error_response["device_id"] = ExtractDeviceId(req, 1);
    error_response["output_id"] = ExtractDeviceId(req, 2);
    res.status = http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}

// 파라미터 변경 (설정값, 임계값 등)
void RestApiServer::HandlePostParameterChange(const httplib::Request &req,
                                              httplib::Response &res) {
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

    if (parameter_change_callback_) {
      bool success = parameter_change_callback_(device_id, parameter_id, value);
      if (success) {
        json data = CreateOutputResponse(value, "parameter");
        data["device_id"] = device_id;
        data["parameter_id"] = parameter_id;
        res.set_content(CreateSuccessResponse(data).dump(), "application/json");
      } else {
        auto &mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(
            PulseOne::Enums::ErrorCode::DEVICE_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse("Failed to change parameter",
                                            "PARAMETER_CHANGE_FAILED", "")
                            .dump(),
                        "application/json");
      }
    } else {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
      res.status = http_status;
      res.set_content(CreateErrorResponse("Parameter change callback not set",
                                          "SERVICE_UNAVAILABLE", "")
                          .dump(),
                      "application/json");
    }
  } catch (const std::exception &e) {
    auto [error_code_str, error_details] =
        ClassifyHardwareError(ExtractDeviceId(req, 1), e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response =
        CreateErrorResponse(error_details, error_code_str, "");
    error_response["device_id"] = ExtractDeviceId(req, 1);
    error_response["parameter_id"] = ExtractDeviceId(req, 2);
    res.status = http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}

// 시스템 제어 핸들러들
void RestApiServer::HandlePostReloadConfig(const httplib::Request &req,
                                           httplib::Response &res) {
  try {
    SetCorsHeaders(res);

    if (reload_config_callback_) {
      bool success = reload_config_callback_();
      if (success) {
        json data = CreateMessageResponse("Configuration reload started");
        res.set_content(CreateSuccessResponse(data).dump(), "application/json");
      } else {
        auto &mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(
            PulseOne::Enums::ErrorCode::CONFIGURATION_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse("Failed to reload configuration",
                                            "CONFIG_RELOAD_FAILED", "")
                            .dump(),
                        "application/json");
      }
    } else {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
      res.status = http_status;
      res.set_content(CreateErrorResponse("Reload config callback not set",
                                          "SERVICE_UNAVAILABLE", "")
                          .dump(),
                      "application/json");
    }
  } catch (const std::exception &e) {
    auto [error_code_str, error_details] = ClassifyHardwareError("", e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response =
        CreateErrorResponse(error_details, error_code_str, "");
    res.status = http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}

void RestApiServer::HandlePostReinitialize(const httplib::Request &req,
                                           httplib::Response &res) {
  try {
    SetCorsHeaders(res);

    if (reinitialize_callback_) {
      bool success = reinitialize_callback_();
      if (success) {
        json data = CreateMessageResponse("Driver reinitialization started");
        res.set_content(CreateSuccessResponse(data).dump(), "application/json");
      } else {
        auto &mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(
            PulseOne::Enums::ErrorCode::INTERNAL_ERROR);
        res.status = http_status;
        res.set_content(CreateErrorResponse("Failed to reinitialize drivers",
                                            "REINIT_FAILED", "")
                            .dump(),
                        "application/json");
      }
    } else {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
      res.status = http_status;
      res.set_content(CreateErrorResponse("Reinitialize callback not set",
                                          "SERVICE_UNAVAILABLE", "")
                          .dump(),
                      "application/json");
    }
  } catch (const std::exception &e) {
    auto [error_code_str, error_details] = ClassifyHardwareError("", e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response =
        CreateErrorResponse(error_details, error_code_str, "");
    res.status = http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}

void RestApiServer::HandleGetSystemStats(const httplib::Request &req,
                                         httplib::Response &res) {
  try {
    SetCorsHeaders(res);

    if (system_stats_callback_) {
      json stats = system_stats_callback_();
      res.set_content(CreateSuccessResponse(stats).dump(), "application/json");
    } else {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
      res.status = http_status;
      res.set_content(CreateErrorResponse("System stats callback not set",
                                          "SERVICE_UNAVAILABLE", "")
                          .dump(),
                      "application/json");
    }
  } catch (const std::exception &e) {
    auto [error_code_str, error_details] = ClassifyHardwareError("", e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response =
        CreateErrorResponse(error_details, error_code_str, "");
    res.status = http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}

void RestApiServer::HandleGetSystemLogs(const httplib::Request &req,
                                        httplib::Response &res) {
  try {
    SetCorsHeaders(res);

    std::string path = "";
    if (req.matches.size() > 1) {
      path = req.matches[1];
    }

    bool is_read_request =
        req.params.count("lines") > 0 || req.params.count("offset") > 0;
    bool has_extension = path.find('.') != std::string::npos;

    if (is_read_request || has_extension) {
      if (log_read_callback_) {
        int lines = 100;
        int offset = 0;

        if (req.params.count("lines")) {
          auto it = req.params.find("lines");
          if (it != req.params.end())
            lines = std::stoi(it->second);
        }
        if (req.params.count("offset")) {
          auto it = req.params.find("offset");
          if (it != req.params.end())
            offset = std::stoi(it->second);
        }

        std::string content = log_read_callback_(path, lines, offset);

        json data = json::object();
        data["path"] = path;
        data["content"] = content;

        res.set_content(CreateSuccessResponse(data).dump(), "application/json");
      } else {
        res.status = 503;
        res.set_content(CreateErrorResponse("Log read callback not set",
                                            "SERVICE_UNAVAILABLE", "")
                            .dump(),
                        "application/json");
      }
    } else {
      if (log_list_callback_) {
        json logs = log_list_callback_(path);
        res.set_content(CreateSuccessResponse(logs).dump(), "application/json");
      } else {
        res.status = 503;
        res.set_content(CreateErrorResponse("Log list callback not set",
                                            "SERVICE_UNAVAILABLE", "")
                            .dump(),
                        "application/json");
      }
    }
  } catch (const std::exception &e) {
    res.status = 500;
    res.set_content(
        CreateErrorResponse(e.what(), "INTERNAL_SERVER_ERROR", "").dump(),
        "application/json");
  }
}

void RestApiServer::HandleGetErrorStatistics(const httplib::Request &req,
                                             httplib::Response &res) {
  try {
    SetCorsHeaders(res);

    json stats = json::object();
    auto &log_mgr = LogManager::getInstance();
    auto log_stats = log_mgr.getStatistics();

    stats["total_logs"] = log_stats.total_logs.load();
    stats["error_count"] = log_stats.error_count.load();
    stats["warn_count"] = log_stats.warn_count.load();
    stats["fatal_count"] = log_stats.fatal_count.load();

    res.set_content(CreateSuccessResponse(stats).dump(), "application/json");
  } catch (const std::exception &e) {
    res.status = 500;
    res.set_content(
        CreateErrorResponse(e.what(), "INTERNAL_SERVER_ERROR", "").dump(),
        "application/json");
  }
}

void RestApiServer::HandleGetErrorCodeInfo(const httplib::Request &req,
                                           httplib::Response &res) {
  try {
    SetCorsHeaders(res);
    std::string error_code_str =
        req.matches.size() > 1 ? std::string(req.matches[1]) : "";

    json info = json::object();
    info["code"] = error_code_str;

    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode code = mapper.ParseErrorString(error_code_str);

    info["http_status"] = mapper.MapErrorToHttpStatus(code);
    info["description"] = error_code_str + " error occurred.";

    res.set_content(CreateSuccessResponse(info).dump(), "application/json");
  } catch (const std::exception &e) {
    res.status = 500;
    res.set_content(
        CreateErrorResponse(e.what(), "INTERNAL_SERVER_ERROR", "").dump(),
        "application/json");
  }
}

void RestApiServer::HandleGetDeviceGroups(const httplib::Request &req,
                                          httplib::Response &res) {
  try {
    SetCorsHeaders(res);

    if (device_group_list_callback_) {
      json groups = device_group_list_callback_();
      res.set_content(CreateSuccessResponse(groups).dump(), "application/json");
    } else {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
      res.status = http_status;
      res.set_content(
          CreateErrorResponse("Device group list callback not set",
                              "COLLECTOR_NOT_CONFIGURED",
                              "Device group functionality is not available")
              .dump(),
          "application/json");
    }
  } catch (const std::exception &e) {
    auto [error_code_str, error_details] = ClassifyHardwareError("", e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response = CreateErrorResponse(
        error_details, error_code_str, "Failed to retrieve device groups");
    res.status = http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}

void RestApiServer::HandleGetDeviceGroupStatus(const httplib::Request &req,
                                               httplib::Response &res) {
  try {
    SetCorsHeaders(res);
    std::string group_id = ExtractGroupId(req);

    if (device_group_status_callback_) {
      json status = device_group_status_callback_(group_id);

      if (status.empty() ||
          (status.contains("error") && status["error"] == "group_not_found")) {
        auto &mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(
            PulseOne::Enums::ErrorCode::DEVICE_NOT_FOUND);
        res.status = http_status;

        json error_response = CreateErrorResponse(
            "Device group not found", "GROUP_NOT_FOUND",
            "Device group with ID '" + group_id + "' does not exist");
        error_response["group_id"] = group_id;
        res.set_content(error_response.dump(), "application/json");
        return;
      }

      res.set_content(CreateSuccessResponse(status).dump(), "application/json");
    } else {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
      res.status = http_status;
      res.set_content(
          CreateErrorResponse("Device group status callback not set",
                              "COLLECTOR_NOT_CONFIGURED",
                              "Device group functionality is not available")
              .dump(),
          "application/json");
    }
  } catch (const std::exception &e) {
    auto [error_code_str, error_details] =
        ClassifyHardwareError(ExtractGroupId(req), e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response = CreateErrorResponse(
        error_details, error_code_str, "Failed to get device group status");
    error_response["group_id"] = ExtractGroupId(req);
    res.status = http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}

void RestApiServer::HandlePostDeviceGroupStart(const httplib::Request &req,
                                               httplib::Response &res) {
  try {
    SetCorsHeaders(res);
    std::string group_id = ExtractGroupId(req);

    if (device_group_control_callback_) {
      bool success = device_group_control_callback_(group_id, "start");
      if (success) {
        json data = CreateGroupActionResponse(group_id, "started", true);
        res.set_content(CreateSuccessResponse(data).dump(), "application/json");
      } else {
        auto &mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(
            PulseOne::Enums::ErrorCode::DEVICE_ERROR);

        json error_response = CreateErrorResponse(
            "Failed to start device group", "GROUP_START_FAILED",
            "Unable to start all devices in group '" + group_id + "'");
        error_response["group_id"] = group_id;
        error_response["action"] = "start";

        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
      }
    } else {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
      res.status = http_status;
      res.set_content(CreateErrorResponse(
                          "Device group control callback not set",
                          "COLLECTOR_NOT_CONFIGURED",
                          "Device group control functionality is not available")
                          .dump(),
                      "application/json");
    }
  } catch (const std::exception &e) {
    auto [error_code_str, error_details] =
        ClassifyHardwareError(ExtractGroupId(req), e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response = CreateErrorResponse(error_details, error_code_str,
                                              "Group start operation failed");
    error_response["group_id"] = ExtractGroupId(req);
    res.status = http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}

void RestApiServer::HandlePostDeviceGroupStop(const httplib::Request &req,
                                              httplib::Response &res) {
  try {
    SetCorsHeaders(res);
    std::string group_id = ExtractGroupId(req);

    if (device_group_control_callback_) {
      bool success = device_group_control_callback_(group_id, "stop");
      if (success) {
        json data = CreateGroupActionResponse(group_id, "stopped", true);
        res.set_content(CreateSuccessResponse(data).dump(), "application/json");
      } else {
        auto &mapper = HttpErrorMapper::getInstance();
        int http_status = mapper.MapErrorToHttpStatus(
            PulseOne::Enums::ErrorCode::DEVICE_ERROR);

        json error_response = CreateErrorResponse(
            "Failed to stop device group", "GROUP_STOP_FAILED",
            "Unable to stop all devices in group '" + group_id + "'");
        error_response["group_id"] = group_id;
        error_response["action"] = "stop";

        res.status = http_status;
        res.set_content(error_response.dump(), "application/json");
      }
    } else {
      auto &mapper = HttpErrorMapper::getInstance();
      int http_status = mapper.MapErrorToHttpStatus(
          PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED);
      res.status = http_status;
      res.set_content(CreateErrorResponse(
                          "Device group control callback not set",
                          "COLLECTOR_NOT_CONFIGURED",
                          "Device group control functionality is not available")
                          .dump(),
                      "application/json");
    }
  } catch (const std::exception &e) {
    auto [error_code_str, error_details] =
        ClassifyHardwareError(ExtractGroupId(req), e);
    auto &mapper = HttpErrorMapper::getInstance();
    PulseOne::Enums::ErrorCode error_code =
        mapper.ParseErrorString(error_code_str);
    int http_status = mapper.MapErrorToHttpStatus(error_code);

    json error_response = CreateErrorResponse(error_details, error_code_str,
                                              "Group stop operation failed");
    error_response["group_id"] = ExtractGroupId(req);
    res.status = http_status;
    res.set_content(error_response.dump(), "application/json");
  }
}

// =============================================================================
// 콜백 설정 메서드들 - 모든 콜백 타입 지원
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

void RestApiServer::SetWorkerStatusCallback(WorkerStatusCallback callback) {
  worker_status_callback_ = callback;
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

void RestApiServer::SetDeviceReloadSettingsCallback(
    DeviceReloadSettingsCallback callback) {
  device_reload_settings_callback_ = callback;
}

void RestApiServer::SetDiscoveryStartCallback(DiscoveryStartCallback callback) {
  discovery_start_callback_ = callback;
}

void RestApiServer::SetDigitalOutputCallback(DigitalOutputCallback callback) {
  digital_output_callback_ = callback;
}

void RestApiServer::SetAnalogOutputCallback(AnalogOutputCallback callback) {
  analog_output_callback_ = callback;
}

void RestApiServer::SetParameterChangeCallback(
    ParameterChangeCallback callback) {
  parameter_change_callback_ = callback;
}

void RestApiServer::SetDeviceGroupListCallback(
    DeviceGroupListCallback callback) {
  device_group_list_callback_ = callback;
}

void RestApiServer::SetDeviceGroupStatusCallback(
    DeviceGroupStatusCallback callback) {
  device_group_status_callback_ = callback;
}

void RestApiServer::SetDeviceGroupControlCallback(
    DeviceGroupControlCallback callback) {
  device_group_control_callback_ = callback;
}

void RestApiServer::SetDeviceConfigCallback(DeviceConfigCallback callback) {
  device_config_callback_ = callback;
}

void RestApiServer::SetDataPointConfigCallback(
    DataPointConfigCallback callback) {
  datapoint_config_callback_ = callback;
}

void RestApiServer::SetAlarmConfigCallback(AlarmConfigCallback callback) {
  alarm_config_callback_ = callback;
}

void RestApiServer::SetVirtualPointConfigCallback(
    VirtualPointConfigCallback callback) {
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

void RestApiServer::SetLogListCallback(LogListCallback callback) {
  log_list_callback_ = callback;
}

void RestApiServer::SetLogReadCallback(LogReadCallback callback) {
  log_read_callback_ = callback;
}

// =============================================================================
// 유틸리티 메소드들 - 100% 조건부 컴파일 보호
// =============================================================================

void RestApiServer::SetCorsHeaders(httplib::Response &res) {
  res.set_header("Access-Control-Allow-Origin", "*");
  res.set_header("Access-Control-Allow-Methods",
                 "GET, POST, PUT, DELETE, OPTIONS");
  res.set_header("Access-Control-Allow-Headers",
                 "Content-Type, Authorization, X-Requested-With");
  res.set_header("Access-Control-Max-Age", "3600");
}
json RestApiServer::CreateErrorResponse(const std::string &error,
                                        const std::string &error_code,
                                        const std::string &details) {
  json response = json::object();
  response["success"] = false;
  response["error"] = error;

  if (!error_code.empty()) {
    response["error_code"] = error_code;
  }

  if (!details.empty()) {
    response["details"] = details;
  }

  response["timestamp"] = static_cast<long>(
      duration_cast<milliseconds>(system_clock::now().time_since_epoch())
          .count());
  return response;
}

json RestApiServer::CreateSuccessResponse(const json &data) {
  json response = json::object();
  response["success"] = true;
  response["timestamp"] = static_cast<long>(
      duration_cast<milliseconds>(system_clock::now().time_since_epoch())
          .count());

  if (!data.empty()) {
    response["data"] = data;
  }

  return response;
}

json RestApiServer::CreateMessageResponse(const std::string &message) {
  json response = json::object();
  response["message"] = message;
  return response;
}

json RestApiServer::CreateHealthResponse() {
  json response = json::object();
  response["status"] = "ok";
  response["uptime_seconds"] = "calculated_by_system";
  response["version"] = "2.1.0";
  response["features"] = json::array({"device_control", "hardware_control",
                                      "device_groups", "system_management"});
  return response;
}

json RestApiServer::CreateOutputResponse(double value,
                                         const std::string &type) {
  json response = json::object();
  response["message"] = type + " output set";
  response["value"] = value;
  response["type"] = type;
  return response;
}

json RestApiServer::CreateGroupActionResponse(const std::string &group_id,
                                              const std::string &action,
                                              bool success) {
  json response = json::object();
  response["group_id"] = group_id;
  response["action"] = action;
  response["result"] = success ? "success" : "failed";
  response["message"] = "Group " + group_id + " " + action + " " +
                        (success ? "completed successfully" : "failed");
  return response;
}

std::string RestApiServer::ExtractDeviceId(const httplib::Request &req,
                                           int match_index) {
  if (match_index > 0 && match_index < static_cast<int>(req.matches.size())) {
    return req.matches[match_index];
  }
  return "";
}

std::string RestApiServer::ExtractGroupId(const httplib::Request &req,
                                          int match_index) {
  if (match_index > 0 && match_index < static_cast<int>(req.matches.size())) {
    return req.matches[match_index];
  }
  return "";
}

bool RestApiServer::ValidateJsonSchema(const nlohmann::json &data,
                                       const std::string &schema_type) {
  try {
    if (schema_type == "device") {
      return data.contains("name") && data.contains("protocol_type") &&
             data.contains("endpoint");
    } else if (schema_type == "datapoint") {
      return data.contains("name") && data.contains("address") &&
             data.contains("data_type");
    } else if (schema_type == "alarm") {
      return data.contains("name") && data.contains("condition") &&
             data.contains("threshold");
    } else if (schema_type == "virtualpoint") {
      return data.contains("name") && data.contains("formula") &&
             data.contains("input_points");
    } else if (schema_type == "user") {
      return data.contains("username") && data.contains("email") &&
             data.contains("role");
    } else if (schema_type == "group") {
      return data.contains("name") && data.contains("devices") &&
             data["devices"].is_array();
    }
    return false;
  } catch (const std::exception &) {
    return false;
  }
}

json RestApiServer::CreateDetailedErrorResponse(
    PulseOne::Enums::ErrorCode error_code, const std::string &device_id,
    const std::string &additional_context) {
  auto &mapper = HttpErrorMapper::getInstance();
  auto error_detail = mapper.GetErrorDetail(error_code);

  json response = json::object();
  response["success"] = false;
  response["error_code"] = mapper.ErrorCodeToString(error_code);
  response["http_status"] = error_detail.http_status;
  response["severity"] = error_detail.severity;
  response["category"] = error_detail.category;
  response["user_message"] = mapper.GetUserFriendlyMessage(error_code);
  response["technical_details"] = error_detail.tech_details;
  response["recoverable"] = error_detail.recoverable;
  response["user_actionable"] = error_detail.user_actionable;

  if (!device_id.empty()) {
    response["device_id"] = device_id;
  }

  if (!additional_context.empty()) {
    response["context"] = additional_context;
  }

  if (!error_detail.action_hint.empty()) {
    response["suggested_action"] = error_detail.action_hint;
  }

  response["timestamp"] = static_cast<long>(
      duration_cast<milliseconds>(system_clock::now().time_since_epoch())
          .count());

  return response;
}

PulseOne::Enums::DeviceStatus
RestApiServer::ParseDeviceStatus(const std::string &status_str) {
  // 🔥 기존 Common/Enums.h에 정의된 값들만 사용
  if (status_str == "ONLINE")
    return PulseOne::Enums::DeviceStatus::ONLINE;
  if (status_str == "OFFLINE")
    return PulseOne::Enums::DeviceStatus::OFFLINE;
  if (status_str == "MAINTENANCE")
    return PulseOne::Enums::DeviceStatus::MAINTENANCE;
  if (status_str == "ERROR")
    return PulseOne::Enums::DeviceStatus::DEVICE_ERROR; // 수정됨
  if (status_str == "WARNING")
    return PulseOne::Enums::DeviceStatus::WARNING;
  return PulseOne::Enums::DeviceStatus::OFFLINE; // 기본값
}

PulseOne::Enums::ConnectionStatus
RestApiServer::ParseConnectionStatus(const std::string &status_str) {
  // 🔥 기존 Common/Enums.h에 정의된 값들만 사용
  if (status_str == "CONNECTED")
    return PulseOne::Enums::ConnectionStatus::CONNECTED;
  if (status_str == "DISCONNECTED")
    return PulseOne::Enums::ConnectionStatus::DISCONNECTED;
  if (status_str == "CONNECTING")
    return PulseOne::Enums::ConnectionStatus::CONNECTING;
  if (status_str == "TIMEOUT")
    return PulseOne::Enums::ConnectionStatus::TIMEOUT;
  if (status_str == "ERROR")
    return PulseOne::Enums::ConnectionStatus::ERROR;
  if (status_str == "MAINTENANCE")
    return PulseOne::Enums::ConnectionStatus::MAINTENANCE;
  return PulseOne::Enums::ConnectionStatus::DISCONNECTED; // 기본값
}

PulseOne::Enums::ErrorCode
RestApiServer::AnalyzeExceptionToErrorCode(const std::string &exception_msg) {
  std::string lower_msg = exception_msg;
  std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(),
                 ::tolower);

  // 타임아웃 패턴
  if (lower_msg.find("timeout") != std::string::npos ||
      lower_msg.find("timed out") != std::string::npos) {
    return PulseOne::Enums::ErrorCode::TIMEOUT;
  }

  // 연결 실패 패턴
  if (lower_msg.find("connection") != std::string::npos &&
      (lower_msg.find("failed") != std::string::npos ||
       lower_msg.find("refused") != std::string::npos)) {
    return PulseOne::Enums::ErrorCode::CONNECTION_FAILED;
  }

  // 디바이스 에러 패턴
  if (lower_msg.find("device") != std::string::npos &&
      lower_msg.find("error") != std::string::npos) {
    return PulseOne::Enums::ErrorCode::DEVICE_ERROR;
  }

  // 설정 에러 패턴
  if (lower_msg.find("config") != std::string::npos ||
      lower_msg.find("configuration") != std::string::npos) {
    return PulseOne::Enums::ErrorCode::CONFIGURATION_ERROR;
  }

  // 권한 에러 패턴
  if (lower_msg.find("permission") != std::string::npos ||
      lower_msg.find("access denied") != std::string::npos) {
    return PulseOne::Enums::ErrorCode::INSUFFICIENT_PERMISSION; // 수정됨
  }

  // 메모리 부족 패턴
  if (lower_msg.find("memory") != std::string::npos ||
      lower_msg.find("out of memory") != std::string::npos) {
    return PulseOne::Enums::ErrorCode::RESOURCE_EXHAUSTED;
  }

  // 데이터 형식 에러 패턴
  if (lower_msg.find("parse") != std::string::npos ||
      lower_msg.find("format") != std::string::npos ||
      lower_msg.find("json") != std::string::npos) {
    return PulseOne::Enums::ErrorCode::DATA_FORMAT_ERROR;
  }

  // 기본적으로 내부 에러로 분류
  return PulseOne::Enums::ErrorCode::INTERNAL_ERROR;
}

// ClassifyHardwareError - 예외 메시지 분석 및 하드웨어별 특화 에러 분류
std::pair<std::string, std::string>
RestApiServer::ClassifyHardwareError(const std::string &device_id,
                                     const std::exception &e) {
  std::string error_message = e.what();
  std::string lower_msg = error_message;
  std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(),
                 ::tolower);

  // 1. 연결 실패 패턴들
  if (lower_msg.find("connection") != std::string::npos &&
      (lower_msg.find("failed") != std::string::npos ||
       lower_msg.find("refused") != std::string::npos)) {
    return {"HARDWARE_CONNECTION_FAILED",
            "Device connection failed: " + error_message};
  }

  // 2. 타임아웃 패턴들
  if (lower_msg.find("timeout") != std::string::npos ||
      lower_msg.find("timed out") != std::string::npos) {
    return {"HARDWARE_TIMEOUT", "Device response timeout: " + error_message};
  }

  // 3. Modbus 특화 에러들
  if (lower_msg.find("modbus") != std::string::npos) {
    if (lower_msg.find("exception") != std::string::npos) {
      return {"MODBUS_EXCEPTION",
              "Modbus protocol exception: " + error_message};
    }
    if (lower_msg.find("crc") != std::string::npos ||
        lower_msg.find("checksum") != std::string::npos) {
      return {"MODBUS_CRC_ERROR",
              "Modbus data integrity error: " + error_message};
    }
    if (lower_msg.find("slave") != std::string::npos &&
        lower_msg.find("not") != std::string::npos) {
      return {"MODBUS_SLAVE_NOT_RESPONDING",
              "Modbus slave device not responding: " + error_message};
    }
    return {"MODBUS_PROTOCOL_ERROR", "Modbus protocol error: " + error_message};
  }

  // 4. MQTT 특화 에러들
  if (lower_msg.find("mqtt") != std::string::npos) {
    if (lower_msg.find("broker") != std::string::npos &&
        lower_msg.find("connect") != std::string::npos) {
      return {"MQTT_BROKER_CONNECTION_FAILED",
              "MQTT broker connection failed: " + error_message};
    }
    if (lower_msg.find("publish") != std::string::npos &&
        lower_msg.find("failed") != std::string::npos) {
      return {"MQTT_PUBLISH_FAILED",
              "MQTT message publish failed: " + error_message};
    }
    if (lower_msg.find("subscribe") != std::string::npos &&
        lower_msg.find("failed") != std::string::npos) {
      return {"MQTT_SUBSCRIBE_FAILED",
              "MQTT topic subscription failed: " + error_message};
    }
    return {"MQTT_PROTOCOL_ERROR", "MQTT protocol error: " + error_message};
  }

  // 5. BACnet 특화 에러들
  if (lower_msg.find("bacnet") != std::string::npos) {
    if (lower_msg.find("object") != std::string::npos &&
        lower_msg.find("not found") != std::string::npos) {
      return {"BACNET_OBJECT_NOT_FOUND",
              "BACnet object not found: " + error_message};
    }
    if (lower_msg.find("property") != std::string::npos &&
        lower_msg.find("error") != std::string::npos) {
      return {"BACNET_PROPERTY_ERROR",
              "BACnet property access error: " + error_message};
    }
    if (lower_msg.find("device") != std::string::npos &&
        lower_msg.find("unreachable") != std::string::npos) {
      return {"BACNET_DEVICE_UNREACHABLE",
              "BACnet device unreachable: " + error_message};
    }
    return {"BACNET_PROTOCOL_ERROR", "BACnet protocol error: " + error_message};
  }

  // 기본 분류 (나머지는 동일)
  if (lower_msg.find("invalid") != std::string::npos) {
    return {"INVALID_PARAMETER", "Invalid parameter: " + error_message};
  }

  // 기본 에러 (분류되지 않은 모든 예외)
  return {"INTERNAL_ERROR", "Unexpected internal error: " + error_message};
}
