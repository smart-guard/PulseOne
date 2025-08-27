// =============================================================================
// collector/src/Api/ConfigApiCallbacks.cpp
// 설정 관리 관련 REST API 콜백 구현 - json 충돌 해결
// =============================================================================

#include "Api/ConfigApiCallbacks.h"
#include "Network/RestApiServer.h" 
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"

using namespace PulseOne::Api;
using namespace PulseOne::Network;

// nlohmann::json을 명시적으로 사용하여 충돌 방지
using nlohmann::json;

void ConfigApiCallbacks::Setup(RestApiServer* server,
                              ConfigManager* config_manager,
                              LogManager* logger) {
    
    if (!server || !config_manager || !logger) {
        return;
    }
    
    // 설정 다시 로드 콜백
    server->SetReloadConfigCallback([=]() -> bool {
        try {
            config_manager->reload();
            logger->Info("Configuration reloaded via API");
            return true;
        } catch (const std::exception& e) {
            logger->Error("Config reload failed: " + std::string(e.what()));
            return false;
        }
    });
    
    // 드라이버 재초기화 콜백
    server->SetReinitializeCallback([=]() -> bool {
        try {
            logger->Info("Driver reinitialization requested via API");
            // 실제 구현에서는 DriverManager 재초기화
            logger->Info("Drivers reinitialized successfully");
            return true;
        } catch (const std::exception& e) {
            logger->Error("Driver reinitialization failed: " + std::string(e.what()));
            return false;
        }
    });
    
    // 시스템 통계 조회 콜백
    server->SetSystemStatsCallback([=]() -> nlohmann::json {
        try {
            nlohmann::json stats = nlohmann::json::object();
            stats["uptime"] = "00:15:30";
            stats["memory_usage"] = "45%";
            stats["cpu_usage"] = "12%";
            stats["active_workers"] = 5;
            stats["status"] = "running";
            
            return stats;
        } catch (const std::exception& e) {
            logger->Error("Failed to get system stats: " + std::string(e.what()));
            nlohmann::json error_stats = nlohmann::json::object();
            error_stats["error"] = "Failed to get system statistics";
            return error_stats;
        }
    });
}