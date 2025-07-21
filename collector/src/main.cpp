#include <iostream>
#include <signal.h>
#include <memory>
#include "Core/Application.h"

using namespace PulseOne::Core;

std::unique_ptr<CollectorApplication> g_app;

void SignalHandler(int signal_num) {
    std::cout << "\n🛑 종료 신호 받음" << std::endl;
    if (g_app) {
        g_app->Stop();
    }
}

int main() {
    try {
        signal(SIGINT, SignalHandler);
        
        std::cout << R"(
🚀 PulseOne Collector v2.0 (Simple Edition)
Enterprise Industrial Data Collection System
)" << std::endl;
        
        g_app = std::make_unique<CollectorApplication>();
        g_app->Run();
        
        std::cout << "✅ 정상 종료" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "💥 오류: " << e.what() << std::endl;
        return -1;
    }
}
