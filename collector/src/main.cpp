// main.cpp - PulseOne Collector 진입점

#include "DatabaseManager.h"
#include "LogManager.h"

int main() {
    LogManager::getInstance().log(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "🚀 PulseOne Collector 시작");

    // DB 초기화
    if (!DatabaseManager::getInstance().initialize()) {
        LogManager::getInstance().log(LOG_MODULE_DATABASE, LOG_LEVEL_ERROR, "❌ 데이터베이스 초기화 실패");
        return 1;
    }

    LogManager::getInstance().log(LOG_MODULE_DATABASE, LOG_LEVEL_INFO, "✅ 데이터베이스 초기화 완료");

    // TODO: 드라이버 로드, ConfigManager 호출, VirtualPointEngine 시작 등 구현 예정

    // 임시 대기 루프
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}
