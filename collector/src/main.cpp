// =============================================================================
// collector/src/main.cpp
// 깔끔한 메인 - 모든 로직은 CollectorApplication 클래스에서 처리
// =============================================================================

#include <iostream>
#include <signal.h>
#include <memory>
#include "Core/CollectorApplication.h"

using namespace PulseOne::Core;

// 전역 애플리케이션 인스턴스
std::unique_ptr<CollectorApplication> g_app;

// 시그널 핸들러 - 깔끔하게 종료
void SignalHandler(int signal) {
    std::cout << "\n🛑 종료 신호 받음 (Signal: " << signal << ")" << std::endl;
    if (g_app) {
        g_app->Stop();
    }
}

int main(int argc, char* argv[]) {
    try {
        // 시그널 핸들러 등록
        signal(SIGINT, SignalHandler);   // Ctrl+C
        signal(SIGTERM, SignalHandler);  // 종료 신호
        
        std::cout << R"(
🚀 PulseOne Collector v2.0
Enterprise Industrial Data Collection System
)" << std::endl;
        
        // 애플리케이션 생성 및 실행
        g_app = std::make_unique<CollectorApplication>();
        g_app->Run();  // 여기서 모든 것이 처리됨!
        
        std::cout << "✅ 정상 종료" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "💥 치명적 오류: " << e.what() << std::endl;
        return -1;
    }
}