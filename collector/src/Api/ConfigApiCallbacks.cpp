// =============================================================================
// collector/src/Api/ConfigApiCallbacks.cpp
// 설정 관리 전용 REST API 콜백 구현 - 설정 리로드/검증만 담당
// =============================================================================

#include "Api/ConfigApiCallbacks.h"
#include "Network/RestApiServer.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"

#include <vector>
#include <string>

using namespace PulseOne::Api;
using namespace PulseOne::Network;
using nlohmann::json;

void ConfigApiCallbacks::Setup(RestApiServer* server,
                              ConfigManager* config_manager,
                              LogManager* logger) {
    
    if (!server || !config_manager || !logger) {
        logger->Error("ConfigApiCallbacks::Setup - null parameters");
        return;
    }

    logger->Info("Setting up configuration API callbacks...");

    // ==========================================================================
    // 설정 리로드 콜백 - 설정 파일만 다시 로드
    // ==========================================================================
    server->SetReloadConfigCallback([=]() -> bool {
        try {
            logger->Info("=== Configuration reload requested via API ===");
            
            // 1단계: 새 설정 파일 로드
            logger->Info("Loading new configuration from files...");
            config_manager->reload();
            
            // 2단계: 설정 유효성 검증
            logger->Info("Validating new configuration...");
            if (!ValidateReloadedConfig(config_manager, logger)) {
                logger->Error("New configuration validation failed");
                return false;
            }
            
            // 3단계: 설정 변경 알림 (로그 레벨 등 즉시 적용 가능한 것들만)
            logger->Info("Applying immediate configuration changes...");
            ApplyImmediateConfigChanges(config_manager, logger);
            
            logger->Info("Configuration reload completed successfully");
            return true;
            
        } catch (const std::exception& e) {
            logger->Error("Config reload failed: " + std::string(e.what()));
            return false;
        }
    });

    logger->Info("Configuration API callbacks setup completed");
}

// =============================================================================
// 설정 검증 및 즉시 적용 함수들
// =============================================================================

bool ConfigApiCallbacks::ValidateReloadedConfig(ConfigManager* config_manager, 
                                               LogManager* logger) {
    try {
        // 필수 설정 키 존재 여부 확인
        std::vector<std::string> required_keys = {
            "database.host",
            "database.port", 
            "redis.host",
            "redis.port",
            "log.level",
            "api.port"
        };
        
        for (const auto& key : required_keys) {
            if (!config_manager->hasKey(key)) {
                logger->Error("Required config key missing: " + key);
                return false;
            }
        }
        
        // 포트 번호 유효성 확인
        int db_port = config_manager->getInt("database.port", -1);
        if (db_port <= 0 || db_port > 65535) {
            logger->Error("Invalid database port: " + std::to_string(db_port));
            return false;
        }
        
        int redis_port = config_manager->getInt("redis.port", -1);
        if (redis_port <= 0 || redis_port > 65535) {
            logger->Error("Invalid Redis port: " + std::to_string(redis_port));
            return false;
        }
        
        int api_port = config_manager->getInt("api.port", -1);
        if (api_port <= 0 || api_port > 65535) {
            logger->Error("Invalid API port: " + std::to_string(api_port));
            return false;
        }
        
        // 로그 레벨 유효성 확인
        std::string log_level = config_manager->getString("log.level", "");
        if (log_level != "debug" && log_level != "info" && 
            log_level != "warning" && log_level != "error") {
            logger->Error("Invalid log level: " + log_level);
            return false;
        }
        
        logger->Info("Configuration validation passed");
        return true;
        
    } catch (const std::exception& e) {
        logger->Error("Configuration validation error: " + std::string(e.what()));
        return false;
    }
}

void ConfigApiCallbacks::ApplyImmediateConfigChanges(ConfigManager* config_manager, 
                                                    LogManager* logger) {
    try {
        // 로그 레벨 즉시 적용
        std::string new_log_level = config_manager->getString("log.level", "info");
        logger->setLevel(new_log_level);
        logger->Info("Log level updated to: " + new_log_level);
        
        // 기타 즉시 적용 가능한 설정들...
        // (디바이스나 시스템 재시작이 필요한 것들은 다른 콜백에서 처리)
        
        logger->Info("Immediate configuration changes applied");
        
    } catch (const std::exception& e) {
        logger->Error("Error applying immediate config changes: " + std::string(e.what()));
    }
}