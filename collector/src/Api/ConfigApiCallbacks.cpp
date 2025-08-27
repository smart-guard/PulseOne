// =============================================================================
// collector/src/Api/ConfigApiCallbacks.cpp
// 설정 관련 REST API 콜백 구현
// =============================================================================

#include "Api/ConfigApiCallbacks.h"
#include "Network/RestApiServer.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"

#include <map>
#include <string>

using namespace PulseOne::Api;
using namespace PulseOne::Network;

void ConfigApiCallbacks::Setup(RestApiServer* server, 
                              ConfigManager* config_mgr, 
                              LogManager* logger) {
    
    if (!server || !config_mgr || !logger) {
        return;
    }
    
    // 설정 다시 로드 콜백
    server->SetReloadConfigCallback([=]() -> bool {
        try {
            logger->Info("API: Configuration reload requested");
            config_mgr->reload();
            logger->Info("API: Configuration reloaded successfully");
            return true;
        } catch (const std::exception& e) {
            logger->Error("API: Failed to reload config - " + std::string(e.what()));
            return false;
        }
    });
    
    // 시스템 통계 콜백 (설정 정보 포함)
    server->SetSystemStatsCallback([=]() -> json {
        json stats = json::object();
        
        try {
            // 기본 설정 정보
            stats["config_directory"] = config_mgr->get("CONFIG_DIR", "unknown");
            stats["data_directory"] = config_mgr->getDataDirectory();
            
            // 설정 파일 상태
            auto all_configs = config_mgr->listAll();
            stats["loaded_config_count"] = static_cast<int>(all_configs.size());
            
            // 주요 설정 값들 (민감하지 않은 것들만)
            json config_info = json::object();
            config_info["log_level"] = config_mgr->get("LOG_LEVEL", "INFO");
            config_info["database_type"] = config_mgr->get("DATABASE_TYPE", "sqlite");
            config_info["redis_host"] = config_mgr->get("REDIS_HOST", "localhost");
            config_info["mqtt_enabled"] = config_mgr->getBool("MQTT_ENABLED", false);
            
            stats["config_info"] = config_info;
            
            // 설정 파일 체크 상태
            auto file_status = config_mgr->checkAllConfigFiles();
            json files_status = json::object();
            for (const auto& [filename, exists] : file_status) {
                files_status[filename] = exists;
            }
            stats["config_files_status"] = files_status;
            
        } catch (const std::exception& e) {
            logger->Error("API: Error gathering config stats - " + std::string(e.what()));
            stats["error"] = "Failed to gather config statistics";
        }
        
        return stats;
    });
}