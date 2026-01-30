/**
 * @file RepositoryFactory.cpp
 * @brief PulseOne Repository 팩토리 구현 - Export Repository 추가
 * @author PulseOne Development Team
 * @date 2025-10-15
 */

#include "Database/RepositoryFactory.h"

// ✅ 완전한 타입 정의를 위한 실제 헤더들 include
#include "DatabaseManager.hpp" // DbLib header
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"

// ✅ Repository 구현 클래스들 include (전방 선언 해결)
#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Database/Repositories/AlarmRuleRepository.h"
#include "Database/Repositories/CurrentValueRepository.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DeviceSettingsRepository.h"
#include "Database/Repositories/EdgeServerRepository.h"
#include "Database/Repositories/ProtocolRepository.h"
#include "Database/Repositories/ScriptLibraryRepository.h"
#include "Database/Repositories/SiteRepository.h"
#include "Database/Repositories/TenantRepository.h"
#include "Database/Repositories/UserRepository.h"
#include "Database/Repositories/VirtualPointRepository.h"

// 🆕 Export 시스템 Repository들
#include "Database/Repositories/DeviceScheduleRepository.h"
#include "Database/Repositories/ExportLogRepository.h"
#include "Database/Repositories/ExportScheduleRepository.h"
#include "Database/Repositories/ExportTargetMappingRepository.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/PayloadTemplateRepository.h"

// ✅ 필수 STL 헤더들
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>

namespace PulseOne {
namespace Database {

// Adapter for DbLib Logger
class PulseOneDbLogger : public DbLib::IDbLogger {
public:
  static std::atomic<bool> enabled;
  void log([[maybe_unused]] const std::string &category, int level,
           const std::string &message) override {
    if (!enabled.load())
      return;
    // Simple mapping: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
    auto &logger = LogManager::getInstance();
    switch (level) {
    case 0:
      logger.Debug(message);
      break;
    case 1:
      logger.Info(message);
      break;
    case 2:
      logger.Warn(message);
      break;
    case 3:
      logger.Error(message);
      break;
    default:
      logger.Info(message);
      break;
    }
  }
};
std::atomic<bool> PulseOneDbLogger::enabled(true);
static PulseOneDbLogger db_logger_adapter;

// =============================================================================
// 싱글톤 구현
// =============================================================================

RepositoryFactory &RepositoryFactory::getInstance() {
  static RepositoryFactory instance;
  return instance;
}

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

RepositoryFactory::RepositoryFactory()
    : db_manager_(
          &DbLib::DatabaseManager::getInstance()) // ✅ DbLib 네임스페이스
      ,
      config_manager_(&::ConfigManager::getInstance()),
      logger_(&::LogManager::getInstance()), global_cache_enabled_(true),
      cache_ttl_seconds_(300), max_cache_size_(1000) {

  logger_->Info("🏭 RepositoryFactory created");
}

RepositoryFactory::~RepositoryFactory() { shutdown(); }

// =============================================================================
// 초기화 및 종료
// =============================================================================

bool RepositoryFactory::initialize() {
  std::lock_guard<std::mutex> lock(factory_mutex_);

  if (initialized_.load()) {
    logger_->Warn("RepositoryFactory already initialized");
    return true;
  }

  try {
    logger_->Info("🔧 RepositoryFactory initializing...");

    // 1. DatabaseManager 초기화 확인
    bool any_db_connected =
        db_manager_->isConnected(DbLib::DatabaseManager::DatabaseType::SQLITE);

    if (db_manager_->isConnected(
            DbLib::DatabaseManager::DatabaseType::POSTGRESQL))
      any_db_connected = true;
    if (db_manager_->isConnected(DbLib::DatabaseManager::DatabaseType::MYSQL))
      any_db_connected = true;
    if (db_manager_->isConnected(DbLib::DatabaseManager::DatabaseType::MSSQL))
      any_db_connected = true;

    if (!any_db_connected) {
      logger_->Error(
          "DatabaseManager not connected - attempting initialization");

      // Configure DbLib
      DbLib::DatabaseConfig db_config;
      db_config.type = config_manager_->getOrDefault("DATABASE_TYPE", "SQLITE");
      logger_->Info("DEBUG: Configured DATABASE_TYPE: " + db_config.type);
      db_config.sqlite_path = config_manager_->getSQLiteDbPath();

      db_config.pg_host =
          config_manager_->getOrDefault("POSTGRES_HOST", "localhost");
      db_config.pg_port = config_manager_->getInt("POSTGRES_PORT", 5432);
      db_config.pg_db =
          config_manager_->getOrDefault("POSTGRES_DB", "pulseone");
      db_config.pg_user =
          config_manager_->getOrDefault("POSTGRES_USER", "pulseone");
      db_config.pg_pass =
          config_manager_->getOrDefault("POSTGRES_PASSWORD", "");

      db_config.mysql_host =
          config_manager_->getOrDefault("MYSQL_HOST", "localhost");
      db_config.mysql_user =
          config_manager_->getOrDefault("MYSQL_USER", "root");
      db_config.mysql_pass =
          config_manager_->getOrDefault("MYSQL_PASSWORD", "");
      db_config.mysql_db =
          config_manager_->getOrDefault("MYSQL_DATABASE", "pulseone");
      db_config.mysql_port = config_manager_->getInt("MYSQL_PORT", 3306);

      db_config.use_redis = true; // Always enable Redis support if available
      db_config.redis_host =
          config_manager_->getOrDefault("REDIS_HOST", "localhost");
      db_config.redis_port = config_manager_->getInt("REDIS_PORT", 6379);
      db_config.redis_pass =
          config_manager_->getOrDefault("REDIS_PASSWORD", "");

      db_config.use_influx =
          (config_manager_->getOrDefault("USE_INFLUXDB", "false") == "true");
      db_config.influx_url =
          config_manager_->getOrDefault("INFLUX_URL", "http://localhost:8086");
      db_config.influx_token =
          config_manager_->getOrDefault("INFLUX_TOKEN", "");
      db_config.influx_org =
          config_manager_->getOrDefault("INFLUX_ORG", "pulseone");
      db_config.influx_bucket =
          config_manager_->getOrDefault("INFLUX_BUCKET", "history");

      // static PulseOneDbLogger db_logger_adapter; // Moved to global scope

      if (!db_manager_->initialize(db_config, &db_logger_adapter)) {
        logger_->Error("Failed to initialize DatabaseManager");
        error_count_.fetch_add(1);
        return false;
      }
    }

    // 2. Repository 인스턴스 생성
    if (!createRepositoryInstances()) {
      logger_->Error("Failed to create repository instances");
      error_count_.fetch_add(1);
      return false;
    }

    connectRepositoryDependencies();

    // 3. 의존성 주입
    if (!injectDependencies()) {
      logger_->Error("Failed to inject dependencies");
      error_count_.fetch_add(1);
      return false;
    }

    // 4. Repository별 설정 적용
    applyRepositoryConfigurations();

    initialized_.store(true);
    logger_->Info("✅ RepositoryFactory initialized successfully");

    return true;

  } catch (const std::exception &e) {
    logger_->Error("RepositoryFactory::initialize failed: " +
                   std::string(e.what()));
    error_count_.fetch_add(1);
    return false;
  }
}

void RepositoryFactory::shutdown() {
  std::lock_guard<std::mutex> lock(factory_mutex_);

  if (!initialized_.load()) {
    return;
  }

  try {
    // [RAII 패턴 적용]
    // 명시적인 로그 출력이나 reset() 호출 없이 스마트 포인터의 소멸자에
    // 의존합니다. 종료 시점(Static Destruction)에 Logger나 다른 객체 사용 시
    // 데드락/Hang 위험이 있습니다.

    initialized_.store(false);
    PulseOneDbLogger::enabled.store(false);

  } catch (...) {
    // 모든 예외 무시
  }
}

// =============================================================================
// 캐싱 제어
// =============================================================================

void RepositoryFactory::setCacheEnabled(bool enabled) {
  std::lock_guard<std::mutex> lock(factory_mutex_);

  global_cache_enabled_ = enabled;

  logger_->Info("Global cache " +
                std::string(enabled ? "enabled" : "disabled"));
}

void RepositoryFactory::clearAllCaches() {
  std::lock_guard<std::mutex> lock(factory_mutex_);
  clearAllCachesInternal();
}

void RepositoryFactory::clearAllCachesInternal() {
  logger_->Info("Clearing all repository caches...");

  int total_cleared = 0;

  // 기본 Repository
  if (device_repository_)
    device_repository_->clearCache();
  if (data_point_repository_)
    data_point_repository_->clearCache();
  if (device_settings_repository_)
    device_settings_repository_->clearCache();
  if (user_repository_)
    user_repository_->clearCache();
  if (tenant_repository_)
    tenant_repository_->clearCache();
  if (alarm_rule_repository_)
    alarm_rule_repository_->clearCache();
  if (site_repository_)
    site_repository_->clearCache();
  if (virtual_point_repository_)
    virtual_point_repository_->clearCache();
  if (current_value_repository_)
    current_value_repository_->clearCache();
  if (alarm_occurrence_repository_)
    alarm_occurrence_repository_->clearCache();
  if (script_library_repository_)
    script_library_repository_->clearCache();
  if (protocol_repository_)
    protocol_repository_->clearCache();
  if (edge_server_repository_)
    edge_server_repository_->clearCache();

  // Export Repository
  if (export_target_repository_)
    export_target_repository_->clearCache();
  if (export_target_mapping_repository_)
    export_target_mapping_repository_->clearCache();
  if (export_log_repository_)
    export_log_repository_->clearCache();
  if (export_schedule_repository_)
    export_schedule_repository_->clearCache();
  if (payload_template_repository_)
    payload_template_repository_->clearCache();
  if (device_schedule_repository_)
    device_schedule_repository_->clearCache();

  logger_->Info("All repository caches cleared");
}

void RepositoryFactory::setCacheTTL(int ttl_seconds) {
  std::lock_guard<std::mutex> lock(factory_mutex_);

  cache_ttl_seconds_ = ttl_seconds;

  logger_->Info("Cache TTL set to " + std::to_string(ttl_seconds) + " seconds");
}

void RepositoryFactory::setMaxCacheSize(int max_size) {
  std::lock_guard<std::mutex> lock(factory_mutex_);

  max_cache_size_ = max_size;

  logger_->Info("Max cache size set to " + std::to_string(max_size) +
                " entries");
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

bool RepositoryFactory::createRepositoryInstances() {
  try {
    logger_->Info("Creating repository instances...");

    // ✅ 기존 Repository 생성
    device_repository_ = std::make_shared<Repositories::DeviceRepository>();
    if (!device_repository_) {
      logger_->Error("Failed to create DeviceRepository");
      return false;
    }

    device_settings_repository_ =
        std::make_shared<Repositories::DeviceSettingsRepository>();
    if (!device_settings_repository_) {
      logger_->Error("Failed to create DeviceSettingsRepository");
      return false;
    }

    data_point_repository_ =
        std::make_shared<Repositories::DataPointRepository>();
    if (!data_point_repository_) {
      logger_->Error("Failed to create DataPointRepository");
      return false;
    }

    user_repository_ = std::make_shared<Repositories::UserRepository>();
    if (!user_repository_) {
      logger_->Error("Failed to create UserRepository");
      return false;
    }

    tenant_repository_ = std::make_shared<Repositories::TenantRepository>();
    if (!tenant_repository_) {
      logger_->Error("Failed to create TenantRepository");
      return false;
    }

    site_repository_ = std::make_shared<Repositories::SiteRepository>();
    if (!site_repository_) {
      logger_->Error("Failed to create SiteRepository");
      return false;
    }

    virtual_point_repository_ =
        std::make_shared<Repositories::VirtualPointRepository>();
    if (!virtual_point_repository_) {
      logger_->Error("Failed to create VirtualPointRepository");
      return false;
    }

    current_value_repository_ =
        std::make_shared<Repositories::CurrentValueRepository>();
    if (!current_value_repository_) {
      logger_->Error("Failed to create CurrentValueRepository");
      return false;
    }

    alarm_rule_repository_ =
        std::make_shared<Repositories::AlarmRuleRepository>();
    if (!alarm_rule_repository_) {
      logger_->Error("Failed to create AlarmRuleRepository");
      return false;
    }

    alarm_occurrence_repository_ =
        std::make_shared<Repositories::AlarmOccurrenceRepository>();
    if (!alarm_occurrence_repository_) {
      logger_->Error("Failed to create AlarmOccurrenceRepository");
      return false;
    }

    protocol_repository_ = std::make_shared<Repositories::ProtocolRepository>();
    if (!protocol_repository_) {
      logger_->Error("Failed to create ProtocolRepository");
      return false;
    }

    edge_server_repository_ =
        std::make_shared<Repositories::EdgeServerRepository>();
    if (!edge_server_repository_) {
      logger_->Error("Failed to create EdgeServerRepository");
      return false;
    }

    // 🆕 Export 시스템 Repository 생성
    export_target_repository_ =
        std::make_shared<Repositories::ExportTargetRepository>();
    if (!export_target_repository_) {
      logger_->Error("Failed to create ExportTargetRepository");
      return false;
    }
    logger_->Info("✅ ExportTargetRepository created");

    export_target_mapping_repository_ =
        std::make_shared<Repositories::ExportTargetMappingRepository>();
    if (!export_target_mapping_repository_) {
      logger_->Error("Failed to create ExportTargetMappingRepository");
      return false;
    }
    logger_->Info("✅ ExportTargetMappingRepository created");

    export_log_repository_ =
        std::make_shared<Repositories::ExportLogRepository>();
    if (!export_log_repository_) {
      logger_->Error("Failed to create ExportLogRepository");
      return false;
    }
    logger_->Info("✅ ExportLogRepository created");

    export_schedule_repository_ =
        std::make_shared<Repositories::ExportScheduleRepository>();
    if (!export_schedule_repository_) {
      logger_->Error("Failed to create ExportScheduleRepository");
      return false;
    }
    logger_->Info("✅ ExportScheduleRepository created");

    payload_template_repository_ =
        std::make_shared<Repositories::PayloadTemplateRepository>();
    if (!payload_template_repository_) {
      logger_->Error("Failed to create PayloadTemplateRepository");
      return false;
    }
    logger_->Info("✅ PayloadTemplateRepository created");

    device_schedule_repository_ =
        std::make_shared<Repositories::DeviceScheduleRepository>();
    if (!device_schedule_repository_) {
      logger_->Error("Failed to create DeviceScheduleRepository");
      return false;
    }
    logger_->Info("✅ DeviceScheduleRepository created");

    logger_->Info("All repository instances created successfully");
    return true;

  } catch (const std::exception &e) {
    logger_->Error("RepositoryFactory::createRepositoryInstances failed: " +
                   std::string(e.what()));
    error_count_.fetch_add(1);
    return false;
  }
}

void RepositoryFactory::applyRepositoryConfigurations() {
  try {
    logger_->Info("Applying repository configurations...");

    logger_->Info("Step 1: Checking global cache settings");

    // 전역 캐시 설정 적용 (안전하게)
    try {
      logger_->Info("Step 2: Applying cache settings");
      if (global_cache_enabled_) {
        logger_->Info("Cache enabled globally");
      } else {
        logger_->Info("Cache disabled globally");
      }
      logger_->Info("Step 3: Cache settings applied");
    } catch (const std::exception &cache_e) {
      logger_->Warn("Cache setting failed but continuing: " +
                    std::string(cache_e.what()));
    }

    logger_->Info("Step 4: Configuration complete");
    logger_->Info("✅ Repository configurations applied successfully");

  } catch (const std::exception &e) {
    logger_->Error("RepositoryFactory::applyRepositoryConfigurations failed: " +
                   std::string(e.what()));
    error_count_.fetch_add(1);
  }
}

bool RepositoryFactory::injectDependencies() {
  try {
    logger_->Info("Injecting dependencies into repositories...");

    // 현재는 Repository들이 생성자에서 싱글톤 참조를 받아오므로 별도 주입
    // 불필요 향후 DI 컨테이너 패턴으로 발전시킬 때 이 메서드에서 의존성 주입
    // 수행

    logger_->Info("Dependencies injected successfully");
    return true;

  } catch (const std::exception &e) {
    logger_->Error("RepositoryFactory::injectDependencies failed: " +
                   std::string(e.what()));
    error_count_.fetch_add(1);
    return false;
  }
}

void RepositoryFactory::connectRepositoryDependencies() {
  try {
    logger_->Info("Connecting repository dependencies...");

    // 🔥 핵심: DataPointRepository에 CurrentValueRepository 자동 주입
    if (data_point_repository_ && current_value_repository_) {
      data_point_repository_->setCurrentValueRepository(
          current_value_repository_);
      logger_->Info(
          "✅ CurrentValueRepository connected to DataPointRepository");
    }

    // 🆕 Export 시스템 의존성 연결 (필요 시)
    // if (export_target_repository_ && export_log_repository_) {
    //     export_target_repository_->setLogRepository(export_log_repository_);
    //     logger_->Info("✅ ExportLogRepository connected to
    //     ExportTargetRepository");
    // }

    logger_->Info("✅ Repository dependencies connected successfully");

  } catch (const std::exception &e) {
    logger_->Error("Failed to connect repository dependencies: " +
                   std::string(e.what()));
    error_count_.fetch_add(1);
  }
}

} // namespace Database
} // namespace PulseOne