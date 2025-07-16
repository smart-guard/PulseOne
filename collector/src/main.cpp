#include <iostream>
#include <thread>   // 추가: std::this_thread
#include <chrono>   // 추가: std::chrono
#include "Config/ConfigManager.h"
#include "Database/DatabaseManager.h"
#include "Utils/LogManager.h"

int main() {
    LogManager::getInstance().log(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "🚀 PulseOne Collector 시작");

    ConfigManager::getInstance().initialize();
    if (!DatabaseManager::getInstance().initialize()) {
        LogManager::getInstance().log(LOG_MODULE_DATABASE, LOG_LEVEL_ERROR, "❌ 데이터베이스 초기화 실패");
        return 1;
    }

    LogManager::getInstance().log(LOG_MODULE_DATABASE, LOG_LEVEL_INFO, "✅ 데이터베이스 초기화 완료");

    // 메인 루프
    while (true) {
        // TODO: 실제 수집 로직
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}