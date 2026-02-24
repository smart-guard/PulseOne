#include "Core/Application.h"
#include "DatabaseManager.hpp"
#include "Utils/ConfigManager.h"
#include "Utils/ProcessSupervisor.h"
#include <iostream>
#include <memory>
#include <signal.h>

using namespace PulseOne::Core;

std::unique_ptr<CollectorApplication> g_app;
PulseOne::Utils::ProcessSupervisor *g_supervisor = nullptr;

void SignalHandler(int signal_num) {
  std::cout << "\n🛑 종료 신호 받음 (Signal: " << signal_num << ")"
            << std::endl;
  if (g_supervisor) {
    g_supervisor->requestShutdown();
  }
  if (g_app) {
    g_app->Stop();
  }
}

int main(int argc, char *argv[]) {
  try {
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    std::cout << R"(
🚀 PulseOne Collector v2.0 (Simple Edition)
Enterprise Industrial Data Collection System
)" << std::endl;

    // Command line argument parsing
    int collector_id = -1;
    std::string config_path;
    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if ((arg == "--id" || arg == "-i") && i + 1 < argc) {
        try {
          collector_id = std::stoi(argv[++i]);
        } catch (...) {
          std::cerr << "❌ Error: Invalid ID format" << std::endl;
          return -1;
        }
      } else if (arg.find("--id=") == 0) {
        try {
          collector_id = std::stoi(arg.substr(5));
        } catch (...) {
          std::cerr << "❌ Error: Invalid ID format" << std::endl;
          return -1;
        }
      } else if (arg.find("--config=") == 0) {
        config_path = arg.substr(9);
      } else if (arg == "--help" || arg == "-h") {
        std::cout << "Usage: " << argv[0] << " [--id <number>]" << std::endl;
        std::cout << "  --id 없이 실행하면 supervisor 모드 (DB 기반 자동 spawn)"
                  << std::endl;
        return 0;
      }
    }

    // ── Supervisor 모드: --id 없이 실행 ──
    if (collector_id == -1) {
      std::cout << "📡 Supervisor 모드 진입 (--id 미지정)" << std::endl;

      // Config/DB 초기화
      if (!config_path.empty()) {
#if PULSEONE_WINDOWS
        _putenv_s("PULSEONE_CONFIG_DIR", config_path.c_str());
#else
        setenv("PULSEONE_CONFIG_DIR", config_path.c_str(), 1);
#endif
      }
      ConfigManager::getInstance().initialize();

      DbLib::DatabaseConfig db_config;
      db_config.type =
          ConfigManager::getInstance().getOrDefault("DATABASE_TYPE", "SQLITE");
      db_config.sqlite_path = ConfigManager::getInstance().getSQLiteDbPath();
      if (!DbLib::DatabaseManager::getInstance().initialize(db_config)) {
        std::cerr << "❌ DB 초기화 실패" << std::endl;
        return -1;
      }

      PulseOne::Utils::ProcessSupervisor supervisor("collector", argv[0],
                                                    config_path);
      g_supervisor = &supervisor;
      supervisor.run();
      g_supervisor = nullptr;
      return 0;
    }

    // ── Worker 모드: --id=N으로 실행 ──
    std::cout << "🆔 Manually assigning Collector ID: " << collector_id
              << std::endl;
    ConfigManager::getInstance().setCollectorId(collector_id);
    std::cout << "🆔 Verification - getCollectorId() returns: "
              << ConfigManager::getInstance().getCollectorId() << std::endl;

    g_app = std::make_unique<CollectorApplication>();
    g_app->Run();

    std::cout << "✅ 정상 종료" << std::endl;
    return 0;

  } catch (const std::exception &e) {
    std::cerr << "💥 오류: " << e.what() << std::endl;
    return -1;
  }
}
