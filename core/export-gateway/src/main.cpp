/**
 * @file main.cpp - Export Gateway v2.0
 * @brief ExportCoordinator 기반 통합 아키텍처
 * @author PulseOne Development Team
 * @date 2025-10-31
 * @version 3.1.0 - Factory 기반 핸들러 등록 완료
 *
 * 🔧 주요 수정사항:
 * - ❌ sendAlarmToAllTargets() → ✅ sendAlarmToTargets()
 * - ❌ getTargetNames() → ✅ getAllTargets()를 사용하여 이름 추출
 * - ❌ testAllConnections() → ✅ healthCheck() + 테스트 알람 전송
 * - ✅ 미사용 파라미터 경고 제거 (argc, argv)
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// ✅ v2.0 헤더
#include "CSP/AlarmMessage.h"
#include "CSP/DynamicTargetManager.h"
#include "CSP/ExportCoordinator.h"
#include "Export/ExportLogService.h"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"

// ✅ Refactored Services
#include "Client/RedisClientImpl.h"
#include "Gateway/Service/GatewayContext.h"
#include "Gateway/Service/GatewayService.h"
#include "Gateway/Service/TargetRegistry.h"
#include "Gateway/Service/TargetRunner.h"
#include "Schedule/ScheduledExporter.h"

// ✅ 데이터베이스 및 레포지토리 팩토리 (추가)
#include "Database/RepositoryFactory.h"
#include "DatabaseManager.hpp"

using namespace PulseOne;
using namespace PulseOne::Coordinator;
using namespace PulseOne::CSP;

// 전역 종료 플래그
std::atomic<bool> g_shutdown_requested{false};

// 시그널 핸들러
void signal_handler(int signal) {
  std::cout << "\nReceived signal " << signal << ". Shutting down gracefully..."
            << std::endl;
  LogManager::getInstance().Info("Shutdown signal received: " +
                                 std::to_string(signal));
  g_shutdown_requested.store(true);
}

void print_banner() {
  std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║                    PulseOne Export Gateway                   ║
║                        Version 2.0.1                        ║
║          Coordinator + DynamicTargetManager + Templates     ║
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

std::vector<int> parseBuildingIds(const std::string &building_ids_str) {
  std::vector<int> building_ids;
  std::stringstream ss(building_ids_str);
  std::string item;

  while (std::getline(ss, item, ',')) {
    item.erase(0, item.find_first_not_of(" \t"));
    item.erase(item.find_last_not_of(" \t") + 1);

    try {
      int building_id = std::stoi(item);
      if (building_id > 0) {
        building_ids.push_back(building_id);
      }
    } catch (const std::exception &e) {
      std::cout << "경고: 잘못된 빌딩 ID 무시: " << item << "\n";
    }
  }

  if (building_ids.empty()) {
    std::cout << "경고: 유효한 빌딩 ID가 없어 기본값(1001) 사용\n";
    building_ids.push_back(1001);
  }

  return building_ids;
}

/**
 * @brief ConfigManager에서 ExportCoordinatorConfig 로드
 */
PulseOne::Coordinator::ExportCoordinatorConfig loadCoordinatorConfig() {
  PulseOne::Coordinator::ExportCoordinatorConfig config;

  try {
    auto &cfg_mgr = ConfigManager::getInstance();

    // 데이터베이스 설정
    config.database_type = cfg_mgr.getOrDefault("DB_TYPE", "SQLITE");
    config.database_path =
        cfg_mgr.getOrDefault("SQLITE_DB_PATH", "/app/data/db/pulseone.db");

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

    // 기본값 설정
    config.alarm_channels = {"alarms:all"};
    config.alarm_worker_threads = 2;
    config.alarm_max_queue_size = 5000;
  }

  return config;
}

void logLoadedConfig(
    const PulseOne::Coordinator::ExportCoordinatorConfig &config) {
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

/**
 * @brief 테스트: 단일 알람 전송
 */
void testSingleAlarm(PulseOne::Gateway::Service::ITargetRunner &runner) {
  std::cout << "\n=== 단일 알람 테스트 ===\n";

  try {
    // 테스트 알람 생성
    PulseOne::Gateway::Model::AlarmMessage alarm;
    alarm.bd = 1001;
    alarm.nm = "TEST_POINT_001";
    alarm.vl = 85.5;
    alarm.al = 1;
    alarm.st = 1;

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    alarm.tm = ss.str();

    LogManager::getInstance().Info("테스트 알람 전송: " + alarm.nm);

    // ✅ 수정: TargetRunner를 직접 사용
    auto results = runner.sendAlarm(alarm);

    std::cout << "전송 결과:\n";
    std::cout << "  총 타겟 수: " << results.size() << "\n";

    int success_count = 0;
    int failure_count = 0;

    for (const auto &result : results) {
      if (result.success) {
        success_count++;
        std::cout << "  ✅ " << result.target_name << " (" << result.target_type
                  << ") - 성공\n";
      } else {
        failure_count++;
        std::cout << "  ❌ " << result.target_name << " (" << result.target_type
                  << ") - 실패: " << result.error_message << "\n";
      }
    }

    std::cout << "\n성공: " << success_count << " / 실패: " << failure_count
              << "\n\n";

  } catch (const std::exception &e) {
    std::cerr << "테스트 실패: " << e.what() << "\n";
    LogManager::getInstance().Error("Test alarm failed: " +
                                    std::string(e.what()));
  }
}

/**
 * @brief 테스트: 타겟 목록 출력
 */
void testTargets(PulseOne::Gateway::Service::ITargetRegistry &registry) {
  std::cout << "\n=== 타겟 목록 ===\n";

  try {
    // ✅ 수정: TargetRegistry를 직접 사용
    auto targets = registry.getAllTargets();

    std::cout << "총 타겟 수: " << targets.size() << "\n\n";

    for (size_t i = 0; i < targets.size(); ++i) {
      const auto &target = targets[i];

      std::cout << (i + 1) << ". " << target.name;
      std::cout << " (" << target.type << ")";
      std::cout << " - " << (target.enabled ? "활성화" : "비활성화");
      std::cout << "\n";
    }

    std::cout << "\n";

  } catch (const std::exception &e) {
    std::cerr << "타겟 목록 조회 실패: " << e.what() << "\n";
  }
}

/**
 * @brief 테스트: 연결 테스트
 */
void testConnection(PulseOne::Gateway::Service::ITargetRunner &runner,
                    PulseOne::Gateway::Service::ITargetRegistry &registry) {
  std::cout << "\n=== 연결 테스트 ===\n";

  try {
    auto targets = registry.getAllTargets();

    std::cout << "총 타겟 수: " << targets.size() << "\n";

    // 개별 타겟 테스트를 위한 테스트 알람 전송
    PulseOne::Gateway::Model::AlarmMessage test_alarm;
    test_alarm.bd = 1001;
    test_alarm.nm = "CONNECTION_TEST";
    test_alarm.vl = 1.0;
    test_alarm.al = 0;
    test_alarm.st = 0;

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    test_alarm.tm = ss.str();

    std::cout << "개별 타겟 연결 테스트:\n";
    auto results = runner.sendAlarm(test_alarm);

    int success_count = 0;
    for (const auto &result : results) {
      std::cout << (result.success ? "✅" : "❌") << " " << result.target_name
                << " (" << result.target_type << ")";
      if (!result.success) {
        std::cout << " (" << result.error_message << ")";
      }
      std::cout << "\n";

      if (result.success)
        success_count++;
    }

    std::cout << "\n연결 성공: " << success_count << " / " << results.size()
              << "\n\n";

  } catch (const std::exception &e) {
    std::cerr << "연결 테스트 실패: " << e.what() << "\n";
  }
}

/**
 * @brief 테스트: 스케줄 실행
 */
void testSchedule() {
  std::cout << "\n========================================\n";
  std::cout << "스케줄 Export 테스트\n";
  std::cout << "========================================\n";

  try {
    // ScheduledExporter 직접 확인
    auto &scheduled_exporter =
        PulseOne::Schedule::ScheduledExporter::getInstance();

    std::cout << "ScheduledExporter 상태: "
              << (scheduled_exporter.isRunning() ? "실행 중" : "중지됨")
              << "\n";

    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "스케줄 실행 대기 중...\n";

  } catch (const std::exception &e) {
    std::cout << "스케줄 테스트 실패: " << e.what() << "\n";
  }
}

/**
 * @brief 통계 출력
 */
void printStatistics(PulseOne::Gateway::Service::ITargetRunner &runner) {
  std::cout << "\n========================================\n";
  std::cout << "Export 통계 (Gateway V2)\n";
  std::cout << "========================================\n";

  try {
    auto stats = runner.getStats();

    std::cout << "전체 Export: " << stats.total_exports << "\n";
    std::cout << "성공: " << stats.successful_exports << "\n";
    std::cout << "실패: " << stats.failed_exports << "\n";

    if (stats.total_exports > 0) {
      double success_rate =
          (double)stats.successful_exports / stats.total_exports * 100.0;
      std::cout << "성공률: " << std::fixed << std::setprecision(2)
                << success_rate << "%\n";
    }

    std::cout << "\n알람 Export: " << stats.alarm_exports << "\n";
    std::cout << "스케줄 실행: " << stats.schedule_executions << "\n";
    std::cout << "스케줄 Export: " << stats.schedule_exports << "\n";

    std::cout << "\n평균 처리 시간: " << std::fixed << std::setprecision(2)
              << stats.avg_processing_time_ms << "ms\n";

    auto now = std::chrono::system_clock::now();
    auto uptime =
        std::chrono::duration_cast<std::chrono::seconds>(now - stats.start_time)
            .count();
    std::cout << "Uptime: " << uptime << " seconds\n";

    std::cout << "========================================\n";

  } catch (const std::exception &e) {
    std::cout << "통계 조회 실패: " << e.what() << "\n";
  }
}

/**
 * @brief 데몬 모드 실행
 */
void runDaemonMode(PulseOne::Gateway::Service::GatewayService &service) {
  LogManager::getInstance().Info("데몬 모드 시작");
  std::cout << "데몬 모드로 실행 중...\n";
  std::cout << "종료하려면 Ctrl+C를 누르세요.\n\n";

  // 통계 출력 간격 (60초)
  int stats_counter = 0;
  const int stats_interval = 60;

  auto &runner = service.getContext().getRunner();

  while (!g_shutdown_requested.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 통계 출력
    stats_counter++;
    if (stats_counter >= stats_interval) {
      printStatistics(runner);
      stats_counter = 0;
    }
  }

  LogManager::getInstance().Info("데몬 모드 종료");
}

/**
 * @brief 대화형 모드 실행
 */
void runInteractiveMode(PulseOne::Gateway::Service::ITargetRunner &runner,
                        PulseOne::Gateway::Service::ITargetRegistry &registry) {
  LogManager::getInstance().Info("대화형 모드 시작");
  std::cout << "대화형 모드로 실행 중...\n";
  std::cout << "명령어: status, test, targets, schedule, quit\n\n";

  std::string command;
  while (!g_shutdown_requested.load()) {
    std::cout << "> ";
    std::getline(std::cin, command);

    if (command == "quit" || command == "exit") {
      break;
    } else if (command == "status") {
      printStatistics(runner);
    } else if (command == "test") {
      testSingleAlarm(runner);
    } else if (command == "targets") {
      testTargets(registry);
    } else if (command == "schedule") {
      testSchedule();
    } else if (command == "connection") {
      testConnection(runner, registry);
    } else if (command == "help") {
      std::cout << "명령어:\n";
      std::cout << "  status      - 통계 출력\n";
      std::cout << "  test        - 테스트 알람 전송\n";
      std::cout << "  targets     - 타겟 목록\n";
      std::cout << "  schedule    - 스케줄 실행\n";
      std::cout << "  connection  - 연결 테스트\n";
      std::cout << "  quit/exit   - 종료\n";
    } else {
      std::cout << "알 수 없는 명령어. 'help' 입력\n";
    }
  }

  LogManager::getInstance().Info("대화형 모드 종료");
}

/**
 * @brief 메인 함수 - v2.0 아키텍처
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
        std::cout << "PulseOne Export Gateway v3.1.0\n";
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
    std::cout << "PulseOne Export Gateway v2.0.2\n";
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

    // 게이트웨이 ID를 로그 태그로 사용 (LogManager 기능에 따라 다름)
    LogManager::getInstance().Info("Export Gateway 시작 (ID: " + gateway_id +
                                   ")");

    // 1.1 데이터베이스 및 레포지토리 팩토리 초기화 (비동기 서비스용)
    {
      auto &db_manager = DbLib::DatabaseManager::getInstance();
      DbLib::DatabaseConfig db_config;
      db_config.type = config.database_type;
      db_config.sqlite_path = config.database_path;

      // PostgreSQL 설정
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
        auto &db_manager = DbLib::DatabaseManager::getInstance();
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

    // 1.5 ExportLogService 시작 (비동기 로그 저장)
    PulseOne::Export::ExportLogService::getInstance().start();

    // 2. Gateway Service Components Initialization
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

    // 단독 테스트 모드들
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

    // ScheduledExporter (Legacy support/integration)
    PulseOne::Schedule::ScheduledExporterConfig schedule_config;
    schedule_config.redis_host = config.redis_host;
    schedule_config.redis_port = config.redis_port;
    schedule_config.redis_password = config.redis_password;
    auto &scheduled_exporter =
        PulseOne::Schedule::ScheduledExporter::getInstance(schedule_config);
    scheduled_exporter.start();

    std::cout << "GatewayService 시작 완료 ✅ (ID: " << gateway_id << ")\n\n";

    if (interactive) {
      runInteractiveMode(service.getContext().getRunner(),
                         service.getContext().getRegistry());
    } else {
      runDaemonMode(service);
    }

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