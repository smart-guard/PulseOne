/**
 * @file main.cpp - Export Gateway v2.0
 * @brief GatewayService 기반 통합 아키텍처
 * @author PulseOne Development Team
 * @date 2025-10-31
 * @version 3.2.0 - 테스트 헬퍼 분리 완료 (cli_tests.cpp)
 *
 * 크로스 플랫폼 대상:
 *   - Linux 네이티브 (systemd 데몬)
 *   - Windows 네이티브 (WinSW 서비스, MinGW 크로스컴파일)
 *   - Docker 컨테이너 (docker compose)
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// ✅ v2.0 헤더
// ✅ v2.0 헤더 (CSP Removed)
#include "Export/ExportLogService.h"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"

// ✅ CLI 테스트 헬퍼 (--test-* 플래그용, 분리됨)
#include "../tests/manual/cli_tests.h"

// ✅ Refactored Services
#include "Client/RedisClientImpl.h"
#include "Gateway/Service/GatewayContext.h"
#include "Gateway/Service/GatewayService.h"
#include "Gateway/Service/TargetRegistry.h"
#include "Gateway/Service/TargetRunner.h"
#include "Schedule/ScheduledExporter.h"

// ✅ 데이터베이스 및 레포지토리 팩토리
#include "Database/RepositoryFactory.h"
#include "DatabaseManager.hpp"

using namespace PulseOne;
// using namespace PulseOne;

// [v3.0.0] Local Configuration Structure (Migrated from ExportCoordinator.h)
struct ExportCoordinatorConfig {
  // Database Configuration (Centralized) - No hardcoded defaults
  std::string database_type = "";
  std::string database_path = "";

  // Dynamic settings (Must be provided via Env or Config)
  std::string db_host = "";
  int db_port = 0;
  std::string db_name = "";
  std::string db_user = "";
  std::string db_pass = "";

  std::string redis_host = "localhost";
  int redis_port = 6379;
  std::string redis_password = "";
  int redis_db_index = 0;
  std::vector<std::string> alarm_channels = {"alarms:all"};
  std::vector<std::string> alarm_patterns;
  int alarm_worker_threads = 4;
  size_t alarm_max_queue_size = 10000;
  int schedule_check_interval_seconds = 60;
  int schedule_reload_interval_seconds = 300;
  int schedule_batch_size = 100;
  bool enable_debug_log = false;
  int log_retention_days = 30;
  int max_concurrent_exports = 50;
  int export_timeout_seconds = 30;

  // Batching Configuration
  bool enable_alarm_batching = false;
  int alarm_batch_latency_ms = 1000;
  int alarm_batch_max_size = 100;

  // Subscription Mode
  std::string subscription_mode = "all"; // "all" or "selective"
}; // Removed
// using namespace PulseOne::CSP;         // Removed

// 전역 종료 플래그
std::atomic<bool> g_shutdown_requested{false};

// =============================================================================
// 시그널 핸들러
// =============================================================================

void signal_handler(int signal) {
  std::cout << "\nReceived signal " << signal << ". Shutting down gracefully..."
            << std::endl;
  LogManager::getInstance().Info("Shutdown signal received: " +
                                 std::to_string(signal));
  g_shutdown_requested.store(true);
}

// =============================================================================
// 배너 / 사용법
// =============================================================================

void print_banner() {
  std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║                    PulseOne Export Gateway                   ║
║                        Version 3.2.0                        ║
║     Linux / Windows / Docker 크로스 플랫폼 지원             ║
╚══════════════════════════════════════════════════════════════╝
)" << std::endl;
}

void print_usage(const char *program_name) {
  std::cout << "사용법: " << program_name << " [옵션]\n\n";
  std::cout << "옵션:\n";
  std::cout << "  --help              도움말 출력\n";
  std::cout << "  --version           버전 정보 출력\n";
  std::cout << "  --config=PATH       설정 파일 경로 지정\n\n";
  std::cout << "테스트 옵션:\n";
  std::cout << "  --test-alarm        단일 테스트 알람 전송\n";
  std::cout << "  --test-targets      타겟 목록 출력\n";
  std::cout << "  --test-schedule     스케줄 실행 테스트\n";
  std::cout << "  --test-connection   연결 테스트\n";
  std::cout << "  --test-all          모든 기능 테스트\n\n";
  std::cout << "서비스 옵션:\n";
  std::cout << "  --daemon            데몬 모드로 실행 (기본값)\n";
  std::cout << "  --interactive       대화형 모드로 실행\n";
  std::cout
      << "  --list-gateways     DB에서 활성 게이트웨이 ID 목록만 출력\n\n";
}

// =============================================================================
// 설정 로드
// =============================================================================

/**
 * @brief ConfigManager에서 ExportCoordinatorConfig 로드
 * 플랫폼 무관: 환경변수/파일에서 읽어 ConfigManager가 처리
 */
ExportCoordinatorConfig loadCoordinatorConfig() {
  ExportCoordinatorConfig config;

  try {
    auto &cfg_mgr = ConfigManager::getInstance();

    // 데이터베이스 설정
    config.database_type = cfg_mgr.getOrDefault("DB_TYPE", "SQLITE");
    config.database_path =
        cfg_mgr.getOrDefault("SQLITE_PATH", "/app/data/db/pulseone.db");

    config.db_host = cfg_mgr.getOrDefault("DB_PRIMARY_HOST", "localhost");
    config.db_port = std::stoi(cfg_mgr.getOrDefault("DB_PRIMARY_PORT", "5432"));
    config.db_name = cfg_mgr.getOrDefault("DB_PRIMARY_NAME", "pulseone");
    config.db_user = cfg_mgr.getOrDefault("DB_PRIMARY_USER", "pulseone");
    config.db_pass = cfg_mgr.getOrDefault("DB_PRIMARY_PASS", "");

    // Redis 설정
    config.redis_host = cfg_mgr.getOrDefault("REDIS_HOST", "localhost");
    config.redis_port = std::stoi(cfg_mgr.getOrDefault("REDIS_PORT", "6379"));
    config.redis_password = cfg_mgr.getOrDefault("REDIS_PASSWORD", "");

    // AlarmSubscriber 설정
    config.alarm_worker_threads =
        std::stoi(cfg_mgr.getOrDefault("ALARM_WORKER_THREADS", "4"));

    config.alarm_max_queue_size =
        std::stoi(cfg_mgr.getOrDefault("ALARM_MAX_QUEUE_SIZE", "10000"));

    // 구독 채널 설정
    std::string channels = cfg_mgr.getOrDefault("ALARM_CHANNELS", "");
    if (!channels.empty()) {
      config.alarm_channels.clear();
      std::stringstream ss(channels);
      std::string channel;
      while (std::getline(ss, channel, ',')) {
        config.alarm_channels.push_back(channel);
      }
    }

    // ScheduledExporter 설정
    config.schedule_check_interval_seconds =
        std::stoi(cfg_mgr.getOrDefault("SCHEDULE_CHECK_INTERVAL", "10"));

    config.schedule_reload_interval_seconds =
        std::stoi(cfg_mgr.getOrDefault("SCHEDULE_RELOAD_INTERVAL", "60"));

    config.schedule_batch_size =
        std::stoi(cfg_mgr.getOrDefault("SCHEDULE_BATCH_SIZE", "100"));

    // 공통 설정
    config.enable_debug_log =
        (cfg_mgr.getOrDefault("ENABLE_DEBUG_LOG", "false") == "true");

    config.log_retention_days =
        std::stoi(cfg_mgr.getOrDefault("LOG_RETENTION_DAYS", "30"));

    // 성능 설정
    config.max_concurrent_exports =
        std::stoi(cfg_mgr.getOrDefault("MAX_CONCURRENT_EXPORTS", "50"));

    config.export_timeout_seconds =
        std::stoi(cfg_mgr.getOrDefault("EXPORT_TIMEOUT_SECONDS", "30"));

  } catch (const std::exception &e) {
    LogManager::getInstance().Error("설정 로드 실패, 기본값 사용: " +
                                    std::string(e.what()));
    config.alarm_channels = {"alarms:all"};
    config.alarm_worker_threads = 2;
    config.alarm_max_queue_size = 5000;
  }

  return config;
}

void logLoadedConfig(const ExportCoordinatorConfig &config) {
  std::cout << "\n========================================\n";
  std::cout << "Export Coordinator 설정:\n";
  std::cout << "========================================\n";
  std::cout << "데이터베이스: " << config.database_path << "\n";
  std::cout << "Redis: " << config.redis_host << ":" << config.redis_port
            << "\n";
  std::cout << "\n[AlarmSubscriber 설정]\n";
  std::cout << "구독 채널 (" << config.alarm_channels.size() << "개):\n";
  for (const auto &channel : config.alarm_channels) {
    std::cout << "  - " << channel << "\n";
  }
  std::cout << "워커 스레드: " << config.alarm_worker_threads << "개\n";
  std::cout << "최대 큐 크기: " << config.alarm_max_queue_size << "\n";
  std::cout << "\n[ScheduledExporter 설정]\n";
  std::cout << "체크 간격: " << config.schedule_check_interval_seconds
            << "초\n";
  std::cout << "리로드 간격: " << config.schedule_reload_interval_seconds
            << "초\n";
  std::cout << "배치 크기: " << config.schedule_batch_size << "\n";
  std::cout << "\n[공통 설정]\n";
  std::cout << "디버그 로그: "
            << (config.enable_debug_log ? "활성화" : "비활성화") << "\n";
  std::cout << "로그 보관 기간: " << config.log_retention_days << "일\n";
  std::cout << "최대 동시 Export: " << config.max_concurrent_exports << "\n";
  std::cout << "Export 타임아웃: " << config.export_timeout_seconds << "초\n";
  std::cout << "========================================\n\n";
}

// =============================================================================
// 데몬 모드 (프로덕션 실행 경로)
// =============================================================================

/**
 * @brief 데몬 모드 실행 - Linux/Windows/Docker 모두 이 경로로 실행
 */
void runDaemonMode(PulseOne::Gateway::Service::GatewayService &service) {
  LogManager::getInstance().Info("데몬 모드 시작");
  std::cout << "데몬 모드로 실행 중...\n";
  std::cout << "종료하려면 Ctrl+C를 누르세요.\n\n";

  int stats_counter = 0;
  const int stats_interval = 60; // 60초마다 통계 출력

  auto &runner = service.getContext().getRunner();

  while (!g_shutdown_requested.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    stats_counter++;
    if (stats_counter >= stats_interval) {
      printStatistics(runner);
      stats_counter = 0;
    }
  }

  LogManager::getInstance().Info("데몬 모드 종료");
}

// =============================================================================
// main()
// =============================================================================

/**
 * @brief 메인 함수 - v3.2.0 아키텍처
 *
 * 실행 순서:
 *  1. 환경변수/파일에서 설정 로드 (ConfigManager)
 *  2. DatabaseManager + RepositoryFactory 초기화
 *  3. CSP DynamicTargetManager 시작 (DB에서 타겟 로드)
 *  4. ExportLogService 시작
 *  5. TargetRegistry + TargetRunner 생성
 *  6. GatewayContext + GatewayService 시작 (Redis 구독)
 *  7. ScheduledExporter 시작
 *  8. 데몬 루프 (또는 테스트/인터랙티브 모드)
 */
int main(int argc, char **argv) {
  std::cout << "🔥🔥🔥 GATEWAY BINARY EXECUTING 🔥🔥🔥" << std::endl;
  try {
    std::string config_path = "";
    std::string gateway_id = "default";
    bool interactive = false;
    bool test_alarm = false;
    bool test_targets = false;
    bool test_schedule = false;
    bool test_connection = false;
    bool test_all = false;
    bool list_gateways = false;

    // 아규먼트 파싱
    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--help") {
        print_usage(argv[0]);
        return 0;
      } else if (arg == "--version") {
        std::cout << "PulseOne Export Gateway v3.2.0\n";
        return 0;
      } else if (arg == "--interactive") {
        interactive = true;
      } else if (arg == "--list-gateways") {
        list_gateways = true;
      } else if (arg.find("--config=") == 0) {
        config_path = arg.substr(9);
      } else if (arg == "--id" && i + 1 < argc) {
        gateway_id = argv[++i];
      } else if (arg.find("--id=") == 0) {
        gateway_id = arg.substr(5);
      } else if (arg == "--test-alarm") {
        test_alarm = true;
      } else if (arg == "--test-targets") {
        test_targets = true;
      } else if (arg == "--test-schedule") {
        test_schedule = true;
      } else if (arg == "--test-connection") {
        test_connection = true;
      } else if (arg == "--test-all") {
        test_all = true;
      }
    }

    std::cout << "===========================================\n";
    std::cout << "PulseOne Export Gateway v3.2.0\n";
    std::cout << "Instance ID: " << gateway_id << "\n";
    std::cout << "===========================================\n\n";

    // 시그널 핸들러 등록
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // 1. 설정 로드
    if (!config_path.empty()) {
      ConfigManager::getInstance().load(config_path);
    }
    auto config = loadCoordinatorConfig();

    LogManager::getInstance().Info("Export Gateway 시작 (ID: " + gateway_id +
                                   ")");

    // 2. DatabaseManager + RepositoryFactory 초기화
    {
      auto &db_manager = DbLib::DatabaseManager::getInstance();
      DbLib::DatabaseConfig db_config;
      db_config.type = config.database_type;
      db_config.sqlite_path = config.database_path;
      db_config.pg_host = config.db_host;
      db_config.pg_port = config.db_port;
      db_config.pg_db = config.db_name;
      db_config.pg_user = config.db_user;
      db_config.pg_pass = config.db_pass;
      db_config.use_redis = false;

      if (!db_manager.initialize(db_config)) {
        LogManager::getInstance().Error("DatabaseManager 초기화 실패");
        return 1;
      }

      if (!PulseOne::Database::RepositoryFactory::getInstance().initialize()) {
        LogManager::getInstance().Error("RepositoryFactory 초기화 실패");
        return 1;
      }

      // 🚀 Discovery Mode: 게이트웨이 목록 출력 후 종료
      if (list_gateways) {
        std::string query = "SELECT id FROM edge_servers WHERE server_type = "
                            "'gateway' AND is_deleted = 0";
        std::vector<std::vector<std::string>> results;
        if (db_manager.executeQuery(query, results)) {
          for (const auto &row : results) {
            if (!row.empty())
              std::cout << row[0] << " ";
          }
          std::cout << std::endl;
        }
        return 0;
      }

      LogManager::getInstance().Info(
          "✅ 전역 데이터베이스 및 레포지토리 초기화 완료");
    }

    // 3. CSP DynamicTargetManager 시작 (Deprecated & Removed)
    // GatewayService::TargetRunner가 기능을 대체함.
    // {
    //   auto &csp_manager = PulseOne::CSP::DynamicTargetManager::getInstance();
    //   ...
    //   if (!csp_manager.start()) ...
    // }

    // 4. ExportLogService 시작
    PulseOne::Export::ExportLogService::getInstance().start();

    // 5. TargetRegistry + TargetRunner 생성
    int gw_id = 0;
    try {
      if (gateway_id != "default")
        gw_id = std::stoi(gateway_id);
    } catch (...) {
    }

    auto registry =
        std::make_unique<PulseOne::Gateway::Service::TargetRegistry>(gw_id);
    if (!registry->loadFromDatabase()) {
      LogManager::getInstance().Error("TargetRegistry 로드 실패");
      return 1;
    }

    auto runner =
        std::make_unique<PulseOne::Gateway::Service::TargetRunner>(*registry);

    // 단독 테스트 모드들 (cli_tests.cpp 함수 사용)
    if (test_alarm) {
      testSingleAlarm(*runner);
      return 0;
    }
    if (test_targets) {
      testTargets(*registry);
      return 0;
    }
    if (test_connection) {
      testConnection(*runner, *registry);
      return 0;
    }

    // 6. GatewayContext + GatewayService 시작
    auto redis_client = std::make_unique<RedisClientImpl>();
    redis_client->connect(config.redis_host, config.redis_port,
                          config.redis_password);
    redis_client->select(0);

    auto context = std::make_unique<PulseOne::Gateway::Service::GatewayContext>(
        gw_id, std::move(redis_client), std::move(registry), std::move(runner));

    PulseOne::Gateway::Service::GatewayService service(std::move(context));

    if (!service.start()) {
      std::cerr << "GatewayService 시작 실패\n";
      return 1;
    }

    // 7. ScheduledExporter 시작
    PulseOne::Schedule::ScheduledExporterConfig schedule_config;
    schedule_config.redis_host = config.redis_host;
    schedule_config.redis_port = config.redis_port;
    schedule_config.redis_password = config.redis_password;
    auto &scheduled_exporter =
        PulseOne::Schedule::ScheduledExporter::getInstance(schedule_config);
    scheduled_exporter.start();

    std::cout << "GatewayService 시작 완료 ✅ (ID: " << gateway_id << ")\n\n";

    // 8. 데몬 / 인터랙티브 / 테스트 모드
    if (interactive) {
      runInteractiveMode(service.getContext().getRunner(),
                         service.getContext().getRegistry());
    } else if (test_all) {
      testSingleAlarm(service.getContext().getRunner());
      testTargets(service.getContext().getRegistry());
      testConnection(service.getContext().getRunner(),
                     service.getContext().getRegistry());
      testSchedule();
    } else {
      runDaemonMode(service);
    }

    // 종료 순서
    scheduled_exporter.stop();
    service.stop();
    PulseOne::Export::ExportLogService::getInstance().stop();
    std::cout << "\nExport Gateway 종료 완료 (ID: " << gateway_id << ")\n";

    return 0;
  } catch (const std::exception &e) {
    std::cerr << "심각한 에러: " << e.what() << "\n";
    return 1;
  }
}