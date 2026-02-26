/**
 * @file Application.cpp
 * @brief PulseOne Collector v2.0 - 실제 메서드명과 일치하도록 수정
 */

#include "Core/Application.h"

#include "Alarm/AlarmStartupRecovery.h"
#include "Database/Entities/EdgeServerEntity.h"
#include "Database/Entities/TenantEntity.h"
#include "Database/Repositories/EdgeServerRepository.h"
#include "Database/Repositories/SystemSettingsRepository.h"
#include "Database/Repositories/TenantRepository.h"
#include "Database/RepositoryFactory.h"
#include "DatabaseManager.hpp"
#include "Drivers/Common/PluginLoader.h"
#include "Logging/LogManager.h"
#include "Pipeline/DataProcessingService.h"
#include "Pipeline/PipelineManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/RedisManager.h"
#include "Workers/WorkerManager.h"

#include <nlohmann/json.hpp>

#if HAS_HTTPLIB
#include "Api/ConfigApiCallbacks.h"
#include "Api/DeviceApiCallbacks.h"
#include "Api/HardwareApiCallbacks.h"
#include "Api/LogApiCallbacks.h"
#endif

#include <chrono>
#include <iostream>
#include <thread>

// 크로스플랫폼 소켓 (IP 자동 감지용)
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace PulseOne {
namespace Core {

CollectorApplication::CollectorApplication() : is_running_(false) {
  LogManager::getInstance().Info("CollectorApplication initialized");
}

CollectorApplication::~CollectorApplication() {
  Cleanup();
  LogManager::getInstance().Info("CollectorApplication destroyed");
}

void CollectorApplication::Run() {
  LogManager::getInstance().Info("PulseOne Collector v2.0 starting...");

  try {
    if (!Initialize()) {
      LogManager::getInstance().Error("Initialization failed");
      return;
    }

    is_running_.store(true);
    LogManager::getInstance().Info("PulseOne Collector started successfully");

    UpdateHeartbeat(); // First heartbeat
    MainLoop();

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Runtime error: " + std::string(e.what()));
  }

  LogManager::getInstance().Info("PulseOne Collector shutdown complete");
}

void CollectorApplication::Stop() {
  LogManager::getInstance().Info("Shutdown requested");
  is_running_.store(false);

  // Fast shutdown: notify the condition variable
  {
    std::lock_guard<std::mutex> lock(stop_mutex_);
    stop_cv_.notify_all();
  }
}

bool CollectorApplication::Initialize() {
  // 드라이버 및 플러그인 동적 로드
  LogManager::getInstance().Info("Loading protocol driver plugins...");
  try {
    // 실행 파일 기준 ./drivers 디렉토리에서 드라이버 로드
    std::string exe_dir = Platform::Path::GetExecutableDirectory();
    std::string plugin_path = Platform::Path::Join(exe_dir, "drivers");

    size_t loaded =
        Drivers::PluginLoader::GetInstance().LoadPlugins(plugin_path);
    LogManager::getInstance().Info("Loaded " + std::to_string(loaded) +
                                   " drivers from " + plugin_path);

#ifdef HAS_HTTP_DRIVER
    // HttpDriver는 내장형일 수 있으므로(Worker 포함 여부에 따라) 별도 처리 가능
    // 여기서는 일단 플러그인화가 기본이므로 로드되지 않았을 경우만 예외적
    // 처리하거나 생략
#endif
  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Plugin loading failed: " +
                                    std::string(e.what()));
  }

  try {
    LogManager::getInstance().Info("=== SYSTEM INITIALIZATION STARTING ===");

    // 1. 설정 관리자 초기화
    try {
      LogManager::getInstance().Info("Step 1/5: Initializing ConfigManager...");
      if (!ConfigManager::getInstance().isInitialized()) {
        ConfigManager::getInstance().initialize();
      }
      LogManager::getInstance().Info(
          "✓ ConfigManager initialized successfully");
    } catch (const std::exception &e) {
      LogManager::getInstance().Error(
          "✗ ConfigManager initialization failed: " + std::string(e.what()));
      return false;
    }

    // 2. Repository 팩토리 초기화 (데이터베이스 연결 포함)
    LogManager::getInstance().Info(
        "Step 2/5: Initializing RepositoryFactory & Database...");
    if (!Database::RepositoryFactory::getInstance().initialize()) {
      LogManager::getInstance().Error(
          "✗ RepositoryFactory initialization failed");
      return false;
    }
    LogManager::getInstance().Info(
        "✓ RepositoryFactory initialized successfully");

    // ── 시작 시 system_settings DB에서 로깅 설정 전체 로드 ─────────────────
    try {
      auto settings_repo = Database::RepositoryFactory::getInstance()
                               .getSystemSettingsRepository();
      auto &lm = LogManager::getInstance();

      // 1. log_level
      std::string saved_level = settings_repo->getValue("log_level", "INFO");
      LogLevel lvl = LogLevel::INFO;
      if (saved_level == "TRACE")
        lvl = LogLevel::TRACE;
      else if (saved_level == "DEBUG")
        lvl = LogLevel::DEBUG;
      else if (saved_level == "WARN")
        lvl = LogLevel::WARN;
      else if (saved_level == "ERROR")
        lvl = LogLevel::LOG_ERROR;
      else if (saved_level == "FATAL")
        lvl = LogLevel::LOG_FATAL;
      lm.setLogLevel(lvl);

      // 2. enable_debug_logging (true면 DEBUG로 오버라이드)
      if (settings_repo->getValue("enable_debug_logging", "false") == "true") {
        lm.setLogLevel(LogLevel::DEBUG);
      }

      // 3. log_to_file
      bool log_to_file =
          settings_repo->getValue("log_to_file", "true") == "true";
      lm.setFileOutput(log_to_file);

      // 4. max_log_file_size_mb
      try {
        int max_mb =
            std::stoi(settings_repo->getValue("max_log_file_size_mb", "100"));
        if (max_mb > 0)
          lm.setMaxLogSizeMB(static_cast<size_t>(max_mb));
      } catch (...) {
      }

      // 5. packet_logging_enabled (COMMUNICATION 카테고리 레벨 제어)
      bool pkt =
          settings_repo->getValue("packet_logging_enabled", "false") == "true";
      lm.setCategoryLogLevel(DriverLogCategory::COMMUNICATION,
                             pkt ? LogLevel::TRACE : LogLevel::INFO);

      // 6. log_retention_days & log_disk_min_free_mb
      try {
        log_retention_days_ =
            std::stoi(settings_repo->getValue("log_retention_days", "30"));
      } catch (...) {
        log_retention_days_ = 30;
      }
      try {
        log_disk_min_free_mb_ = static_cast<size_t>(
            std::stoul(settings_repo->getValue("log_disk_min_free_mb", "500")));
      } catch (...) {
        log_disk_min_free_mb_ = 500;
      }

      lm.Info("📋 Logging settings loaded from DB: log_level=" + saved_level +
              ", log_to_file=" + (log_to_file ? "true" : "false") +
              ", packet_logging=" + (pkt ? "true" : "false") +
              ", retention_days=" + std::to_string(log_retention_days_) +
              ", disk_min_free_mb=" + std::to_string(log_disk_min_free_mb_));

      // 시작 시 즉시 오래된 로그 정리
      lm.cleanupOldLogs(log_retention_days_);
      last_log_cleanup_time_ = std::chrono::steady_clock::now();

    } catch (const std::exception &e) {
      LogManager::getInstance().Warn(
          "⚠️ Could not load logging settings from DB, using defaults: " +
          std::string(e.what()));
    }
    // ──────────────────────────────────────────────────────────────────────────

    // 🔥 3a. Collector Identity Verification
    LogManager::getInstance().Info(
        "Step 3a/7: Verifying Collector Identity...");

    int collector_id = ResolveCollectorId();

    if (collector_id <= 0) {
      return false;
    }

    // 1. Edge Server(Collector) 정보 확인
    auto edge_repo =
        Database::RepositoryFactory::getInstance().getEdgeServerRepository();
    auto edge_server = edge_repo->findById(collector_id);
    if (!edge_server.has_value()) {
      LogManager::getInstance().Error("✗ Collector ID " +
                                      std::to_string(collector_id) +
                                      " not found in database.");
      return false;
    }

    if (!edge_server->isEnabled()) {
      LogManager::getInstance().Error("✗ Collector ID " +
                                      std::to_string(collector_id) +
                                      " is DISABLED in database.");
      return false;
    }

    // ✅ [자동 자기등록] Collector 시작 시 자신의 IP를 자동 감지 → DB 저장
    // - COLLECTOR_HOST 설정됨 (Docker): 해당 값 사용 (서비스명 "collector")
    // - COLLECTOR_HOST 미설정 (네이티브): UDP 소켓으로 주 outbound IP 자동 감지
    //   → 설정파일에 자기 IP를 넣을 필요 없음
    {
      const char *env_host = std::getenv("COLLECTOR_HOST");
      const char *env_port = std::getenv("COLLECTOR_API_PORT");

      std::string my_ip;
      if (env_host && std::strlen(env_host) > 0) {
        my_ip = std::string(env_host); // Docker: "collector"
      } else {
        my_ip = DetectOutboundIP(); // 자동 감지: 192.168.x.x 또는 127.0.0.1
      }

      // 멀티 인스턴스: BASE_PORT + (collector_id - 1)
      // ID 1 → 8501, ID 2 → 8502, ID 3 → 8503, ...
      int base_port =
          (env_port && std::strlen(env_port) > 0) ? std::atoi(env_port) : 8501;
      int my_port = base_port + (collector_id - 1);

      bool ip_changed = (edge_server->getIpAddress() != my_ip);
      bool port_changed = (edge_server->getPort() != my_port);

      if (ip_changed || port_changed) {
        edge_server->setIpAddress(my_ip);
        edge_server->setPort(my_port);
        if (edge_repo->update(*edge_server)) {
          LogManager::getInstance().Info("📍 [Self-Register] edge_server[" +
                                         std::to_string(collector_id) + "] → " +
                                         my_ip + ":" + std::to_string(my_port));
        } else {
          LogManager::getInstance().Warn(
              "⚠️  [Self-Register] DB update failed — using existing value");
        }
      } else {
        LogManager::getInstance().Info("📍 [Self-Register] IP unchanged: " +
                                       my_ip + ":" + std::to_string(my_port));
      }
    }

    // 2. 테넌트 정보 확인 및 이름 조회
    std::string tenant_name = "Unknown Tenant";
    auto tenant_repo =
        Database::RepositoryFactory::getInstance().getTenantRepository();
    auto tenant = tenant_repo->findById(edge_server->getTenantId());

    if (!tenant.has_value()) {
      LogManager::getInstance().Error(
          "✗ Tenant ID " + std::to_string(edge_server->getTenantId()) +
          " not found for Collector " + std::to_string(collector_id));
      return false;
    }
    tenant_name = tenant->getName();

    // 3. 식별된 정체성 로깅 (계획된 이모지 포맷 반영)
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
      strncpy(hostname, "unknown", sizeof(hostname));
    }

    LogManager::getInstance().Info(
        "🆔 Verified Identity: " + edge_server->getName() +
        " (Tenant: " + tenant_name + ")");
    LogManager::getInstance().Info(
        "🌐 Collector Info: ID=" + std::to_string(collector_id) +
        ", Hostname=" + std::string(hostname) +
        ", InstanceKey=" + ConfigManager::getInstance().getInstanceKey());

    // 메모리에 세션 정보 저장
    ConfigManager::getInstance().setTenantId(edge_server->getTenantId());

    // 4. REST API 서버 초기화
    LogManager::getInstance().Info("Step 4/7: Initializing REST API Server...");
    if (!InitializeRestApiServer()) {
      LogManager::getInstance().Warn("Continuing without REST API server...");
    } else {
      LogManager::getInstance().Info(
          "✓ REST API Server initialized successfully");
    }

    // ✅ 5. 포인트 값 복구 (Warm Startup 핵심 로직)
    LogManager::getInstance().Info(
        "Step 5/7: Recovering latest point values (RDB -> Redis -> RAM)...");
    try {
      auto &alarm_recovery = Alarm::AlarmStartupRecovery::getInstance();
      size_t point_count = alarm_recovery.RecoverLatestPointValues();
      LogManager::getInstance().Info("✓ Successfully recovered " +
                                     std::to_string(point_count) +
                                     " point values");
    } catch (const std::exception &e) {
      LogManager::getInstance().Error("✗ Point value recovery failed: " +
                                      std::string(e.what()));
    }

    // ✅ 6. 활성 알람 복구
    LogManager::getInstance().Info("Step 6/7: Recovering active alarms...");
    try {
      auto &alarm_recovery = Alarm::AlarmStartupRecovery::getInstance();
      if (alarm_recovery.IsRecoveryEnabled()) {
        size_t recovered_count = alarm_recovery.RecoverActiveAlarms();
        LogManager::getInstance().Info("✓ Successfully recovered " +
                                       std::to_string(recovered_count) +
                                       " active alarms");
      }
    } catch (const std::exception &e) {
      LogManager::getInstance().Error("✗ Alarm recovery failed: " +
                                      std::string(e.what()));
    }

    // 🔗 6.5. 데이터 파이프라인 초기화 및 시작
    LogManager::getInstance().Info("Step 6.5/7: Initializing Data Pipeline...");
    try {
      // PipelineManager (큐) 시작
      if (!Pipeline::PipelineManager::getInstance().initialize()) {
        LogManager::getInstance().Error(
            "✗ PipelineManager initialization failed");
        return false;
      }

      // DataProcessingService (처리기) 시작
      data_processing_service_ =
          std::make_unique<Pipeline::DataProcessingService>();

      // InfluxDB 저장 주기 설정 로드 (기본 0: 즉시 저장)
      try {
        auto settings_repo = Database::RepositoryFactory::getInstance()
                                 .getSystemSettingsRepository();
        int influx_interval = std::stoi(
            settings_repo->getValue("influxdb_storage_interval", "0"));
        data_processing_service_->SetInfluxDbStorageInterval(influx_interval);
      } catch (...) {
        data_processing_service_->SetInfluxDbStorageInterval(0);
      }

      if (!data_processing_service_->Start()) {
        LogManager::getInstance().Error(
            "✗ DataProcessingService failed to start");
        return false;
      }
      LogManager::getInstance().Info(
          "✓ Data Pipeline (Manager & Service) started successfully");
    } catch (const std::exception &e) {
      LogManager::getInstance().Error(
          "✗ Data Pipeline initialization failed: " + std::string(e.what()));
      return false;
    }

    // 7. WorkerManager를 통한 활성 워커들 시작
    LogManager::getInstance().Info("Step 7/7: Starting active workers...");
    try {
      auto &worker_manager = Workers::WorkerManager::getInstance();
      int started_count = worker_manager.StartAllActiveWorkers();
      LogManager::getInstance().Info(
          "✓ Started " + std::to_string(started_count) + " active workers");
    } catch (const std::exception &e) {
      LogManager::getInstance().Error("✗ Worker startup failed: " +
                                      std::string(e.what()));
    }

    // ✅ 8. C2 Command Subscriber 시작
    LogManager::getInstance().Info("Step 8: Starting C2 Command Subscriber...");
    try {
      PulseOne::Event::EventSubscriberConfig event_config;
      event_config.redis_host = ConfigManager::getInstance().getOrDefault(
          "REDIS_PRIMARY_HOST", "localhost");
      event_config.redis_port =
          ConfigManager::getInstance().getInt("REDIS_PRIMARY_PORT", 6379);
      event_config.redis_password =
          ConfigManager::getInstance().getSecret("REDIS_PASSWORD_FILE");
      event_config.worker_thread_count = 1;

      command_subscriber_ = std::make_unique<CommandSubscriber>(event_config);
      if (command_subscriber_->start()) {
        LogManager::getInstance().Info(
            "✓ C2 Command Subscriber started successfully");
      } else {
        LogManager::getInstance().Warn(
            "✗ Failed to start C2 Command Subscriber");
      }
    } catch (const std::exception &e) {
      LogManager::getInstance().Error(
          "✗ CommandSubscriber initialization failed: " +
          std::string(e.what()));
    }

    LogManager::getInstance().Info("=== SYSTEM INITIALIZATION COMPLETED ===");
    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("=== INITIALIZATION FAILED ===");
    LogManager::getInstance().Error("Exception: " + std::string(e.what()));
    return false;
  }
}

// ---------------------------------------------------------------------------
// DetectOutboundIP: UDP 라우팅 트릭으로 주 outbound IP 자동 감지
// 실제 패킷 미전송. Windows/Linux/macOS/Raspberry Pi 전부 동작.
// ---------------------------------------------------------------------------
std::string CollectorApplication::DetectOutboundIP() {
#ifdef _WIN32
  WSADATA wsa;
  WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

  std::string result = "127.0.0.1"; // fallback

  int sock = static_cast<int>(socket(AF_INET, SOCK_DGRAM, 0));
  if (sock < 0)
    return result;

  struct sockaddr_in remote{};
  remote.sin_family = AF_INET;
  remote.sin_port = htons(53);
  inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);

  if (connect(sock, reinterpret_cast<struct sockaddr *>(&remote),
              sizeof(remote)) == 0) {
    struct sockaddr_in local{};
    socklen_t len = sizeof(local);
    if (getsockname(sock, reinterpret_cast<struct sockaddr *>(&local), &len) ==
        0) {
      char buf[INET_ADDRSTRLEN];
      if (inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf))) {
        result = std::string(buf);
      }
    }
  }

#ifdef _WIN32
  closesocket(sock);
  WSACleanup();
#else
  close(sock);
#endif

  LogManager::getInstance().Info("🌐 [Self-Register] Detected outbound IP: " +
                                 result);
  return result;
}

int CollectorApplication::ResolveCollectorId() {
  auto &config = ConfigManager::getInstance();
  auto &db_mgr = DbLib::DatabaseManager::getInstance();

  std::string instance_key = config.getInstanceKey();
  int explicit_id = config.getCollectorId();

  // 1. Explicit ID provided via env/cmd
  if (explicit_id > 0) {
    LogManager::getInstance().Info("🆔 Using explicit Collector ID: " +
                                   std::to_string(explicit_id));
    // Claim the slot with instance_key for consistency
    std::string update_query =
        "UPDATE edge_servers SET instance_key = '" + instance_key +
        "', last_seen = CURRENT_TIMESTAMP WHERE id = " +
        std::to_string(explicit_id) + " AND server_type = 'collector'";
    db_mgr.executeNonQuery(update_query);
    config.setCollectorId(explicit_id);
    return explicit_id;
  }

  LogManager::getInstance().Info(
      "🔍 No COLLECTOR_ID set. Attempting Zero-Config auto-resolution...");
  LogManager::getInstance().Info("🔑 Instance Key: " + instance_key);

  // 2. Check if already claimed
  std::vector<std::vector<std::string>> results;
  std::string find_query =
      "SELECT id FROM edge_servers WHERE instance_key = '" + instance_key +
      "' AND server_type = 'collector' LIMIT 1";
  if (db_mgr.executeQuery(find_query, results) && !results.empty()) {
    int id = std::stoi(results[0][0]);
    LogManager::getInstance().Info(
        "✅ Found existing slot for this instance: ID=" + std::to_string(id));
    config.setCollectorId(id);
    return id;
  }

  // 3. Try to claim an available slot (instance_key IS NULL or stale heartbeat
  // > 5 min)
  std::string claim_query =
      "UPDATE edge_servers SET instance_key = '" + instance_key +
      "', last_seen = CURRENT_TIMESTAMP "
      "WHERE id = (SELECT id FROM edge_servers WHERE server_type = 'collector' "
      "AND ((instance_key IS NULL OR "
      "instance_key = '') "
      "OR (last_heartbeat < CURRENT_TIMESTAMP - INTERVAL '5 minutes')) LIMIT "
      "1)";

  if (db_mgr.executeNonQuery(claim_query)) {
    // Re-check which ID we got
    if (db_mgr.executeQuery(find_query, results) && !results.empty()) {
      int id = std::stoi(results[0][0]);
      LogManager::getInstance().Info(
          "✨ Successfully claimed available slot: ID=" + std::to_string(id));
      config.setCollectorId(id);
      return id;
    }
  }

  // 4. No slot available?
  LogManager::getInstance().Error(
      "❌ No available slots in edge_servers table. Please add more "
      "collectors in the dashboard.");
  return -1;
}

void CollectorApplication::MainLoop() {
  LogManager::getInstance().Info("Main loop started - production mode");

  auto last_health_check = std::chrono::steady_clock::now();
  auto last_stats_report = std::chrono::steady_clock::now();

  const auto health_check_interval = std::chrono::minutes(5);
  const auto stats_report_interval = std::chrono::hours(1);

  while (is_running_.load()) {
    try {
      auto now = std::chrono::steady_clock::now();
      auto &worker_manager = Workers::WorkerManager::getInstance();

      // 주기적 헬스체크 (5분마다)
      if (now - last_health_check >= health_check_interval) {
        size_t active_count = worker_manager.GetActiveWorkerCount();

        LogManager::getInstance().Info("=== Health Check ===");
        LogManager::getInstance().Info("Active workers: " +
                                       std::to_string(active_count));

        // 디스크 여유 공간 체크
        auto &lm = LogManager::getInstance();
        size_t free_mb = lm.getAvailableDiskSpaceMB();
        lm.Info("💾 Disk free: " + std::to_string(free_mb) + " MB");
        if (free_mb < log_disk_min_free_mb_) {
          lm.Warn("⚠️ Disk space low (" + std::to_string(free_mb) + " MB < " +
                  std::to_string(log_disk_min_free_mb_) +
                  " MB)! Emergency log cleanup triggered.");
          lm.emergencyCleanupLogs(log_disk_min_free_mb_);
        }

        last_health_check = now;
      }

      // 24시간마다 보관 기간 초과 로그 파일 삭제
      if (now - last_log_cleanup_time_ >= std::chrono::hours(24)) {
        LogManager::getInstance().cleanupOldLogs(log_retention_days_);
        last_log_cleanup_time_ = now;
      }

      // 주기적 통계 리포트 (1시간마다)
      if (now - last_stats_report >= stats_report_interval) {
        LogManager::getInstance().Info("=== HOURLY STATISTICS REPORT ===");

        try {
          // WorkerManager 통계
          auto &worker_manager = Workers::WorkerManager::getInstance();
          size_t active_workers = worker_manager.GetActiveWorkerCount();

          LogManager::getInstance().Info("WorkerManager Statistics:");
          LogManager::getInstance().Info("  Active Workers: " +
                                         std::to_string(active_workers));

          // 시스템 리소스 정보
          LogManager::getInstance().Info("System Status:");

          // DbLib::DatabaseManager 연결 상태 확인 (enum class 사용)
          bool db_connected = false;
          try {
            db_connected =
                DbLib::DatabaseManager::getInstance().isConnected(
                    DbLib::DatabaseManager::DatabaseType::POSTGRESQL) ||
                DbLib::DatabaseManager::getInstance().isConnected(
                    DbLib::DatabaseManager::DatabaseType::SQLITE);
          } catch (...) {
            db_connected = false;
          }
          LogManager::getInstance().Info(
              "  Database: " +
              std::string(db_connected ? "Connected" : "Disconnected"));

          // RepositoryFactory 상태
          LogManager::getInstance().Info(
              "  Repositories: " +
              std::string(
                  Database::RepositoryFactory::getInstance().isInitialized()
                      ? "Ready"
                      : "Not Ready"));

#if HAS_HTTPLIB
          // API 서버 상태
          LogManager::getInstance().Info(
              "  REST API: " +
              std::string((api_server_ && api_server_->IsRunning())
                              ? "Running"
                              : "Stopped"));
#else
          LogManager::getInstance().Info("  REST API: Disabled");
#endif

        } catch (const std::exception &e) {
          LogManager::getInstance().Error("Error generating statistics: " +
                                          std::string(e.what()));
        }

        LogManager::getInstance().Info("=== END STATISTICS REPORT ===");
        last_stats_report = now;
      }

      // 콜렉터 하트비트 업데이트 (매 30초)
      UpdateHeartbeat();

      // 메인 루프 간격 (30초) - Fast shutdown 대응 (wait_for 사용)
      std::unique_lock<std::mutex> lock(stop_mutex_);
      if (stop_cv_.wait_for(lock, std::chrono::seconds(30),
                            [this]() { return !is_running_.load(); })) {
        LogManager::getInstance().Info(
            "MainLoop exiting due to shutdown signal");
        break;
      }

    } catch (const std::exception &e) {
      LogManager::getInstance().Error("Exception in MainLoop: " +
                                      std::string(e.what()));
      // 예외 발생시 잠시 대기 후 계속 실행
      std::unique_lock<std::mutex> lock(stop_mutex_);
      stop_cv_.wait_for(lock, std::chrono::seconds(10),
                        [this]() { return !is_running_.load(); });
    }
  }

  LogManager::getInstance().Info("Main loop ended");
}

void CollectorApplication::Cleanup() {
  LogManager::getInstance().Info("=== SYSTEM CLEANUP STARTING ===");

  try {
    is_running_.store(false);

    // 1. REST API 서버 정리
#if HAS_HTTPLIB
    if (api_server_) {
      LogManager::getInstance().Info("Step 1/3: Stopping REST API server...");
      api_server_->Stop();
      api_server_.reset();
      LogManager::getInstance().Info("✓ REST API server stopped");
    }
#endif

    // 2. WorkerManager를 통한 모든 워커 중지
    LogManager::getInstance().Info("Step 2/3: Stopping all workers...");
    try {
      Workers::WorkerManager::getInstance().StopAllWorkers();
      LogManager::getInstance().Info("✓ All workers stopped");
    } catch (const std::exception &e) {
      LogManager::getInstance().Error("Error stopping workers: " +
                                      std::string(e.what()));
    }

    // 2.5. 데이터 파이프라인 정리
    if (data_processing_service_) {
      LogManager::getInstance().Info(
          "Step 2.5/3: Stopping Data Processing Service...");
      data_processing_service_->Stop();
      data_processing_service_.reset();
      LogManager::getInstance().Info("✓ Data Processing Service stopped");
    }

    Pipeline::PipelineManager::getInstance().Shutdown();
    LogManager::getInstance().Info("✓ PipelineManager shut down");

    // 2.7. CommandSubscriber 정리
    if (command_subscriber_) {
      LogManager::getInstance().Info(
          "Step 2.7/3: Stopping Command Subscriber...");
      command_subscriber_->stop();
      command_subscriber_.reset();
      LogManager::getInstance().Info("✓ Command Subscriber stopped");
    }

    // 3. 데이터베이스 연결 정리 (자동으로 소멸자에서 처리됨)
    LogManager::getInstance().Info("Step 3/3: Database cleanup...");
    try {
      // DbLib::DatabaseManager와 RepositoryFactory는 싱글톤이므로 명시적 정리
      // 불필요 소멸자에서 자동으로 연결 해제됨
      LogManager::getInstance().Info("✓ Database cleanup completed");
    } catch (const std::exception &e) {
      LogManager::getInstance().Error("Error in database cleanup: " +
                                      std::string(e.what()));
    }

    LogManager::getInstance().Info("=== SYSTEM CLEANUP COMPLETED ===");

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Exception in Cleanup: " +
                                    std::string(e.what()));
  }
}

bool CollectorApplication::InitializeRestApiServer() {
#if HAS_HTTPLIB
  try {
    // ConfigManager에서 API 포트 읽기
    // 멀티 인스턴스: COLLECTOR_API_PORT(기본 8501) + (collector_id - 1)
    // ID 1 → 8501, ID 2 → 8502, ID 3 → 8503, ...
    int api_port = 8501;
    int resolved_id = ResolveCollectorId();
    try {
      const char *env_port = std::getenv("COLLECTOR_API_PORT");
      int base_port =
          (env_port && std::strlen(env_port) > 0) ? std::atoi(env_port) : 8501;
      api_port = (resolved_id > 0) ? base_port + (resolved_id - 1) : base_port;
    } catch (const std::exception &e) {
      LogManager::getInstance().Warn(
          "Could not read API port from config, using default 8501: " +
          std::string(e.what()));
    }

    api_server_ = std::make_unique<Network::RestApiServer>(api_port);

    // API 콜백들 등록 - 싱글톤 직접 접근 방식
    LogManager::getInstance().Info("Registering API callbacks...");

    // 설정 관련 API 콜백 등록
    PulseOne::Api::ConfigApiCallbacks::Setup(api_server_.get());
    LogManager::getInstance().Info("✓ ConfigApiCallbacks registered");

    // 디바이스 제어 관련 API 콜백 등록
    PulseOne::Api::DeviceApiCallbacks::Setup(api_server_.get());
    LogManager::getInstance().Info("✓ DeviceApiCallbacks registered");

    PulseOne::Api::HardwareApiCallbacks::Setup(api_server_.get());
    LogManager::getInstance().Info("✓ HardwareApiCallbacks registered");

    PulseOne::Api::LogApiCallbacks::Setup(api_server_.get());
    LogManager::getInstance().Info("✓ LogApiCallbacks registered");
    // API 서버 시작
    if (api_server_->Start()) {
      LogManager::getInstance().Info("✓ REST API Server started on port " +
                                     std::to_string(api_port));
      LogManager::getInstance().Info("  API Documentation: http://localhost:" +
                                     std::to_string(api_port) + "/api/docs");
      LogManager::getInstance().Info("  Health Check: http://localhost:" +
                                     std::to_string(api_port) + "/api/health");
      return true;
    } else {
      LogManager::getInstance().Error(
          "✗ Failed to start REST API Server on port " +
          std::to_string(api_port));
      return false;
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("Exception in InitializeRestApiServer: " +
                                    std::string(e.what()));
    return false;
  }
#else
  LogManager::getInstance().Info(
      "REST API Server disabled - HTTP library not available");
  LogManager::getInstance().Info("To enable REST API, compile with "
                                 "-DHAS_HTTPLIB and link against httplib");
  return true;
#endif
}

void CollectorApplication::UpdateHeartbeat() {
  try {
    int collector_id = ConfigManager::getInstance().getCollectorId();
    if (collector_id <= 0)
      return;

    auto &db_mgr = DbLib::DatabaseManager::getInstance();
    std::string query =
        "UPDATE edge_servers SET last_seen = CURRENT_TIMESTAMP, "
        "last_heartbeat = CURRENT_TIMESTAMP WHERE id = " +
        std::to_string(collector_id);

    db_mgr.executeNonQuery(query);

    // Redis 하트비트 추가
    auto redis = PulseOne::Utils::RedisManager::getInstance().getClient();
    if (redis && redis->isConnected()) {
      nlohmann::json status_json;
      status_json["status"] = "online";
      status_json["lastSeen"] = std::chrono::system_clock::to_time_t(
          std::chrono::system_clock::now());
      status_json["gatewayId"] = collector_id;
      status_json["hostname"] =
          "docker-container"; // Hostname retrieval is complex cross-platform

      // Backend EdgeServerService expects collector:status:{id} for collectors
      std::string key = "collector:status:" + std::to_string(collector_id);

      // 90 seconds TTL (Backend refreshes often, but this allows for some
      // missed beats)
      redis->setex(key, status_json.dump(), 90);
    }

  } catch (const std::exception &e) {
    LogManager::getInstance().Warn("Heartbeat update failed: " +
                                   std::string(e.what()));
  }
}

} // namespace Core
} // namespace PulseOne
