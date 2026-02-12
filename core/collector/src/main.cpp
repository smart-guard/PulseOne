#include "Core/Application.h"
#include "Utils/ConfigManager.h"
#include <iostream>
#include <memory>
#include <signal.h>

using namespace PulseOne::Core;

std::unique_ptr<CollectorApplication> g_app;

void SignalHandler(int signal_num) {
  std::cout << "\n🛑 종료 신호 받음 (Signal: " << signal_num << ")"
            << std::endl;
  if (g_app) {
    g_app->Stop();
  }
}

int main(int argc, char *argv[]) {
  try {
    signal(SIGINT, SignalHandler);

    std::cout << R"(
🚀 PulseOne Collector v2.0 (Simple Edition)
Enterprise Industrial Data Collection System
)" << std::endl;

    // Command line argument parsing
    int collector_id = -1;
    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if ((arg == "--id" || arg == "-i") && i + 1 < argc) {
        try {
          collector_id = std::stoi(argv[++i]);
        } catch (...) {
          std::cerr << "❌ Error: Invalid ID format" << std::endl;
          return -1;
        }
      } else if (arg == "--help" || arg == "-h") {
        std::cout << "Usage: " << argv[0] << " [--id <number>]" << std::endl;
        return 0;
      }
    }

    if (collector_id != -1) {
      std::cout << "🆔 Manually assigned Collector ID: " << collector_id
                << std::endl;
      ConfigManager::getInstance().setCollectorId(collector_id);
    }

    g_app = std::make_unique<CollectorApplication>();
    g_app->Run();

    std::cout << "✅ 정상 종료" << std::endl;
    return 0;

  } catch (const std::exception &e) {
    std::cerr << "💥 오류: " << e.what() << std::endl;
    return -1;
  }
}
