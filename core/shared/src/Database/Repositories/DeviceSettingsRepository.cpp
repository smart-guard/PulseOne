/**
 * @file DeviceSettingsRepository.cpp - SQLQueries.h 상수 100% 적용
 * @brief PulseOne DeviceSettingsRepository 구현 - 완전한 중앙 집중식 쿼리 관리
 * @author PulseOne Development Team
 * @date 2025-08-07
 *
 * 🎯 SQLQueries.h 상수 완전 적용:
 * - 모든 하드코딩된 쿼리를 SQL::DeviceSettings:: 상수로 교체
 * - 동적 파라미터 처리 개선
 * - DeviceSettingsRepository 패턴 100% 준수
 * - DbLib::DatabaseAbstractionLayer 완전 활용
 */

#include "Database/Repositories/DeviceSettingsRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/SQLQueries.h"
#include "DatabaseAbstractionLayer.hpp"
#include "Logging/LogManager.h"
#include <algorithm>
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 기본 CRUD 구현 (SQLQueries.h 상수 사용)
// =============================================================================

std::vector<DeviceSettingsEntity> DeviceSettingsRepository::findAll() {
  try {
    if (!ensureTableExists()) {
      logger_->Error(
          "DeviceSettingsRepository::findAll - Table creation failed");
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 SQLQueries.h 상수 사용
    auto results = db_layer.executeQuery(SQL::DeviceSettings::FIND_ALL);

    std::vector<DeviceSettingsEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn(
            "DeviceSettingsRepository::findAll - Failed to map row: " +
            std::string(e.what()));
      }
    }

    logger_->Info("DeviceSettingsRepository::findAll - Found " +
                  std::to_string(entities.size()) + " settings");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("DeviceSettingsRepository::findAll failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::optional<DeviceSettingsEntity>
DeviceSettingsRepository::findById(int device_id) {
  try {
    // 캐시 확인
    if (isCacheEnabled()) {
      auto cached = getCachedEntity(device_id);
      if (cached.has_value()) {
        logger_->Debug(
            "DeviceSettingsRepository::findById - Cache hit for device_id: " +
            std::to_string(device_id));
        return cached.value();
      }
    }

    if (!ensureTableExists()) {
      return std::nullopt;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 SQLQueries.h 상수 사용 + 동적 파라미터 처리
    std::string query = RepositoryHelpers::replaceParameter(
        SQL::DeviceSettings::FIND_BY_ID, std::to_string(device_id));

    auto results = db_layer.executeQuery(query);

    if (results.empty()) {
      logger_->Debug("DeviceSettingsRepository::findById - No settings found "
                     "for device_id: " +
                     std::to_string(device_id));
      return std::nullopt;
    }

    auto entity = mapRowToEntity(results[0]);

    // 캐시에 저장
    if (isCacheEnabled()) {
      cacheEntity(entity);
    }

    logger_->Debug(
        "DeviceSettingsRepository::findById - Found settings for device_id: " +
        std::to_string(device_id));
    return entity;

  } catch (const std::exception &e) {
    logger_->Error("DeviceSettingsRepository::findById failed for device_id " +
                   std::to_string(device_id) + ": " + std::string(e.what()));
    return std::nullopt;
  }
}

bool DeviceSettingsRepository::save(DeviceSettingsEntity &entity) {
  try {
    if (!validateSettings(entity)) {
      logger_->Error(
          "DeviceSettingsRepository::save - Invalid settings for device_id: " +
          std::to_string(entity.getDeviceId()));
      return false;
    }

    if (!ensureTableExists()) {
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    std::map<std::string, std::string> data = entityToParams(entity);
    std::vector<std::string> primary_keys = {"device_id"};

    bool success =
        db_layer.executeUpsert("device_settings", data, primary_keys);

    if (success) {
      // 캐시 업데이트
      if (isCacheEnabled()) {
        cacheEntity(entity);
      }

      logger_->Info(
          "DeviceSettingsRepository::save - Saved settings for device_id: " +
          std::to_string(entity.getDeviceId()));
    } else {
      logger_->Error("DeviceSettingsRepository::save - Failed to save settings "
                     "for device_id: " +
                     std::to_string(entity.getDeviceId()));
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("DeviceSettingsRepository::save failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool DeviceSettingsRepository::update(const DeviceSettingsEntity &entity) {
  DeviceSettingsEntity mutable_entity = entity;
  return save(mutable_entity);
}

bool DeviceSettingsRepository::deleteById(int device_id) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 SQLQueries.h 상수 사용
    std::string query = RepositoryHelpers::replaceParameter(
        SQL::DeviceSettings::DELETE_BY_ID, std::to_string(device_id));

    bool success = db_layer.executeNonQuery(query);

    if (success) {
      if (isCacheEnabled()) {
        clearCacheForId(device_id);
      }

      logger_->Info("DeviceSettingsRepository::deleteById - Deleted settings "
                    "for device_id: " +
                    std::to_string(device_id));
    } else {
      logger_->Error("DeviceSettingsRepository::deleteById - Failed to delete "
                     "settings for device_id: " +
                     std::to_string(device_id));
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("DeviceSettingsRepository::deleteById failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool DeviceSettingsRepository::exists(int device_id) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 SQLQueries.h 상수 사용
    std::string query = RepositoryHelpers::replaceParameter(
        SQL::DeviceSettings::EXISTS_BY_ID, std::to_string(device_id));

    auto results = db_layer.executeQuery(query);

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      int count = std::stoi(results[0].at("count"));
      return count > 0;
    }

    return false;

  } catch (const std::exception &e) {
    logger_->Error("DeviceSettingsRepository::exists failed: " +
                   std::string(e.what()));
    return false;
  }
}

std::vector<DeviceSettingsEntity>
DeviceSettingsRepository::findByIds(const std::vector<int> &device_ids) {
  try {
    if (device_ids.empty()) {
      return {};
    }

    if (!ensureTableExists()) {
      return {};
    }

    // IN 절 구성
    std::ostringstream ids_ss;
    for (size_t i = 0; i < device_ids.size(); ++i) {
      if (i > 0)
        ids_ss << ", ";
      ids_ss << device_ids[i];
    }

    // 🎯 기본 쿼리에 IN 절 추가 (SQLQueries.h 기반)
    std::string query = SQL::DeviceSettings::FIND_ALL;

    // ORDER BY 전에 WHERE 절 삽입
    size_t order_pos = query.find("ORDER BY");
    if (order_pos != std::string::npos) {
      query.insert(order_pos, "WHERE device_id IN (" + ids_ss.str() + ") ");
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<DeviceSettingsEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn(
            "DeviceSettingsRepository::findByIds - Failed to map row: " +
            std::string(e.what()));
      }
    }

    logger_->Info("DeviceSettingsRepository::findByIds - Found " +
                  std::to_string(entities.size()) + " settings for " +
                  std::to_string(device_ids.size()) + " devices");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("DeviceSettingsRepository::findByIds failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<DeviceSettingsEntity> DeviceSettingsRepository::findByConditions(
    const std::vector<QueryCondition> & /* conditions */,
    const std::optional<OrderBy> & /* order_by */,
    const std::optional<Pagination> & /* pagination */) {

  // 임시 구현
  logger_->Info("DeviceSettingsRepository::findByConditions called - returning "
                "all for now");
  return findAll();
}

// =============================================================================
// DeviceSettings 전용 메서드들 (SQLQueries.h 상수 사용)
// =============================================================================

std::vector<DeviceSettingsEntity>
DeviceSettingsRepository::findByProtocol(const std::string &protocol_type) {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 SQLQueries.h 상수 사용
    std::string query = RepositoryHelpers::replaceParameterWithQuotes(
        SQL::DeviceSettings::FIND_BY_PROTOCOL, protocol_type);

    auto results = db_layer.executeQuery(query);

    std::vector<DeviceSettingsEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn(
            "DeviceSettingsRepository::findByProtocol - Failed to map row: " +
            std::string(e.what()));
      }
    }

    logger_->Info("DeviceSettingsRepository::findByProtocol - Found " +
                  std::to_string(entities.size()) +
                  " settings for protocol: " + protocol_type);
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("DeviceSettingsRepository::findByProtocol failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<DeviceSettingsEntity>
DeviceSettingsRepository::findActiveDeviceSettings() {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 SQLQueries.h 상수 사용
    auto results =
        db_layer.executeQuery(SQL::DeviceSettings::FIND_ACTIVE_DEVICES);

    std::vector<DeviceSettingsEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("DeviceSettingsRepository::findActiveDeviceSettings - "
                      "Failed to map row: " +
                      std::string(e.what()));
      }
    }

    logger_->Info(
        "DeviceSettingsRepository::findActiveDeviceSettings - Found " +
        std::to_string(entities.size()) + " active device settings");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error(
        "DeviceSettingsRepository::findActiveDeviceSettings failed: " +
        std::string(e.what()));
    return {};
  }
}

// =============================================================================
// 내부 헬퍼 메서드들 (DeviceSettingsRepository 패턴)
// =============================================================================

DeviceSettingsEntity DeviceSettingsRepository::mapRowToEntity(
    const std::map<std::string, std::string> &row) {
  DeviceSettingsEntity entity;

  try {
    // 🎯 내부에서 DbLib::DatabaseAbstractionLayer 생성해서 사용
    DbLib::DatabaseAbstractionLayer db_layer;

    auto it = row.find("device_id");
    if (it != row.end()) {
      entity.setDeviceId(std::stoi(it->second));
    }

    it = row.find("polling_interval_ms");
    if (it != row.end()) {
      entity.setPollingIntervalMs(std::stoi(it->second));
    }

    it = row.find("connection_timeout_ms");
    if (it != row.end()) {
      entity.setConnectionTimeoutMs(std::stoi(it->second));
    }

    it = row.find("max_retry_count");
    if (it != row.end()) {
      entity.setMaxRetryCount(std::stoi(it->second));
    }

    it = row.find("retry_interval_ms");
    if (it != row.end()) {
      entity.setRetryIntervalMs(std::stoi(it->second));
    }

    it = row.find("backoff_time_ms");
    if (it != row.end()) {
      entity.setBackoffTimeMs(std::stoi(it->second));
    }

    // 🎯 BOOLEAN 값은 추상화 레이어가 처리
    it = row.find("keep_alive_enabled");
    if (it != row.end()) {
      entity.setKeepAliveEnabled(db_layer.parseBoolean(it->second));
    }

    it = row.find("keep_alive_interval_s");
    if (it != row.end()) {
      entity.setKeepAliveIntervalS(std::stoi(it->second));
    }

    it = row.find("scan_rate_override");
    if (it != row.end() && !it->second.empty() && it->second != "NULL") {
      entity.setScanRateOverride(std::stoi(it->second));
    }

    it = row.find("read_timeout_ms");
    if (it != row.end()) {
      entity.setReadTimeoutMs(std::stoi(it->second));
    }

    it = row.find("write_timeout_ms");
    if (it != row.end()) {
      entity.setWriteTimeoutMs(std::stoi(it->second));
    }

    it = row.find("backoff_multiplier");
    if (it != row.end()) {
      entity.setBackoffMultiplier(std::stod(it->second));
    }

    it = row.find("max_backoff_time_ms");
    if (it != row.end()) {
      entity.setMaxBackoffTimeMs(std::stoi(it->second));
    }

    it = row.find("keep_alive_timeout_s");
    if (it != row.end()) {
      entity.setKeepAliveTimeoutS(std::stoi(it->second));
    }

    it = row.find("data_validation_enabled");
    if (it != row.end()) {
      entity.setDataValidationEnabled(db_layer.parseBoolean(it->second));
    }

    it = row.find("performance_monitoring_enabled");
    if (it != row.end()) {
      entity.setPerformanceMonitoringEnabled(db_layer.parseBoolean(it->second));
    }

    it = row.find("diagnostic_mode_enabled");
    if (it != row.end()) {
      entity.setDiagnosticModeEnabled(db_layer.parseBoolean(it->second));
    }

    it = row.find("auto_registration_enabled");
    if (it != row.end()) {
      entity.setAutoRegistrationEnabled(db_layer.parseBoolean(it->second));
    }

    return entity;

  } catch (const std::exception &e) {
    logger_->Error("DeviceSettingsRepository::mapRowToEntity failed: " +
                   std::string(e.what()));
    throw;
  }
}

std::map<std::string, std::string>
DeviceSettingsRepository::entityToParams(const DeviceSettingsEntity &entity) {
  DbLib::DatabaseAbstractionLayer db_layer;

  std::map<std::string, std::string> params;

  params["device_id"] = std::to_string(entity.getDeviceId());
  params["polling_interval_ms"] = std::to_string(entity.getPollingIntervalMs());
  params["connection_timeout_ms"] =
      std::to_string(entity.getConnectionTimeoutMs());
  params["max_retry_count"] = std::to_string(entity.getMaxRetryCount());
  params["retry_interval_ms"] = std::to_string(entity.getRetryIntervalMs());
  params["backoff_time_ms"] = std::to_string(entity.getBackoffTimeMs());
  params["keep_alive_enabled"] =
      db_layer.formatBoolean(entity.isKeepAliveEnabled());
  params["keep_alive_interval_s"] =
      std::to_string(entity.getKeepAliveIntervalS());

  if (entity.getScanRateOverride().has_value()) {
    params["scan_rate_override"] =
        std::to_string(entity.getScanRateOverride().value());
  } else {
    params["scan_rate_override"] = "NULL";
  }

  params["read_timeout_ms"] = std::to_string(entity.getReadTimeoutMs());
  params["write_timeout_ms"] = std::to_string(entity.getWriteTimeoutMs());
  params["backoff_multiplier"] = std::to_string(entity.getBackoffMultiplier());
  params["max_backoff_time_ms"] = std::to_string(entity.getMaxBackoffTimeMs());
  params["keep_alive_timeout_s"] =
      std::to_string(entity.getKeepAliveTimeoutS());
  params["data_validation_enabled"] =
      db_layer.formatBoolean(entity.isDataValidationEnabled());
  params["performance_monitoring_enabled"] =
      db_layer.formatBoolean(entity.isPerformanceMonitoringEnabled());
  params["diagnostic_mode_enabled"] =
      db_layer.formatBoolean(entity.isDiagnosticModeEnabled());
  params["auto_registration_enabled"] = db_layer.formatBoolean(
      entity.isAutoRegistrationEnabled()); // 🔥 시간 정보
  params["created_at"] = db_layer.getGenericTimestamp();
  params["updated_at"] = db_layer.getGenericTimestamp();

  return params;
}

bool DeviceSettingsRepository::ensureTableExists() {
  try {
    DbLib::DatabaseAbstractionLayer db_layer;

    // 🎯 SQLQueries.h 상수 사용
    bool success =
        db_layer.executeCreateTable(SQL::DeviceSettings::CREATE_TABLE);

    if (success) {
      logger_->Debug("DeviceSettingsRepository::ensureTableExists - Table "
                     "creation/check completed");
    } else {
      logger_->Error("DeviceSettingsRepository::ensureTableExists - Table "
                     "creation failed");
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("DeviceSettingsRepository::ensureTableExists failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool DeviceSettingsRepository::validateSettings(
    const DeviceSettingsEntity &entity) const {
  if (!entity.isValid()) {
    logger_->Warn("DeviceSettingsRepository::validateSettings - Invalid "
                  "settings for device_id: " +
                  std::to_string(entity.getDeviceId()));
    return false;
  }

  if (entity.getPollingIntervalMs() < 100) {
    logger_->Warn("DeviceSettingsRepository::validateSettings - Polling "
                  "interval too short (< 100ms) for device_id: " +
                  std::to_string(entity.getDeviceId()));
    return false;
  }

  if (entity.getConnectionTimeoutMs() < entity.getPollingIntervalMs()) {
    logger_->Warn(
        "DeviceSettingsRepository::validateSettings - Connection timeout "
        "should be greater than polling interval for device_id: " +
        std::to_string(entity.getDeviceId()));
    return false;
  }

  return true;
}

// =============================================================================
// 비즈니스 로직 메서드들 (SQLQueries.h 상수 적용)
// =============================================================================

bool DeviceSettingsRepository::createOrUpdateSettings(
    int device_id, const DeviceSettingsEntity &settings) {
  DeviceSettingsEntity mutable_settings = settings;
  mutable_settings.setDeviceId(device_id);
  return save(mutable_settings);
}

bool DeviceSettingsRepository::createDefaultSettings(int device_id) {
  try {
    if (exists(device_id)) {
      logger_->Info("DeviceSettingsRepository::createDefaultSettings - "
                    "Settings already exist for device_id: " +
                    std::to_string(device_id));
      return true;
    }

    DeviceSettingsEntity default_settings(device_id);
    default_settings.resetToIndustrialDefaults();

    bool success = save(default_settings);

    if (success) {
      logger_->Info("DeviceSettingsRepository::createDefaultSettings - Created "
                    "default settings for device_id: " +
                    std::to_string(device_id));
    } else {
      logger_->Error("DeviceSettingsRepository::createDefaultSettings - Failed "
                     "to create default settings for device_id: " +
                     std::to_string(device_id));
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("DeviceSettingsRepository::createDefaultSettings failed: " +
                   std::string(e.what()));
    return false;
  }
}

// 실시간 설정 변경 메서드들
bool DeviceSettingsRepository::updatePollingInterval(int device_id,
                                                     int polling_interval_ms) {
  try {
    auto settings = findById(device_id);
    if (!settings.has_value()) {
      if (!createDefaultSettings(device_id)) {
        return false;
      }
      settings = findById(device_id);
    }

    if (settings.has_value()) {
      auto old_settings = settings.value();
      settings.value().setPollingIntervalMs(polling_interval_ms);

      bool success = save(settings.value());
      if (success) {
        logSettingsChange(device_id, old_settings, settings.value());
        logger_->Info("DeviceSettingsRepository::updatePollingInterval - "
                      "Updated polling interval for device_id " +
                      std::to_string(device_id) + " to " +
                      std::to_string(polling_interval_ms) + "ms");
      }
      return success;
    }

    return false;

  } catch (const std::exception &e) {
    logger_->Error("DeviceSettingsRepository::updatePollingInterval failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool DeviceSettingsRepository::updateConnectionTimeout(int device_id,
                                                       int timeout_ms) {
  try {
    auto settings = findById(device_id);
    if (!settings.has_value()) {
      if (!createDefaultSettings(device_id)) {
        return false;
      }
      settings = findById(device_id);
    }

    if (settings.has_value()) {
      auto old_settings = settings.value();
      settings.value().setConnectionTimeoutMs(timeout_ms);

      bool success = save(settings.value());
      if (success) {
        logSettingsChange(device_id, old_settings, settings.value());
        logger_->Info("DeviceSettingsRepository::updateConnectionTimeout - "
                      "Updated connection timeout for device_id " +
                      std::to_string(device_id) + " to " +
                      std::to_string(timeout_ms) + "ms");
      }
      return success;
    }

    return false;

  } catch (const std::exception &e) {
    logger_->Error(
        "DeviceSettingsRepository::updateConnectionTimeout failed: " +
        std::string(e.what()));
    return false;
  }
}

bool DeviceSettingsRepository::updateRetrySettings(int device_id,
                                                   int max_retry_count,
                                                   int retry_interval_ms) {
  try {
    auto settings = findById(device_id);
    if (!settings.has_value()) {
      if (!createDefaultSettings(device_id)) {
        return false;
      }
      settings = findById(device_id);
    }

    if (settings.has_value()) {
      auto old_settings = settings.value();
      settings.value().setMaxRetryCount(max_retry_count);
      settings.value().setRetryIntervalMs(retry_interval_ms);

      bool success = save(settings.value());
      if (success) {
        logSettingsChange(device_id, old_settings, settings.value());
        logger_->Info("DeviceSettingsRepository::updateRetrySettings - Updated "
                      "retry settings for device_id " +
                      std::to_string(device_id) +
                      ": max_retry=" + std::to_string(max_retry_count) +
                      ", interval=" + std::to_string(retry_interval_ms) + "ms");
      }
      return success;
    }

    return false;

  } catch (const std::exception &e) {
    logger_->Error("DeviceSettingsRepository::updateRetrySettings failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool DeviceSettingsRepository::updateAutoRegistrationEnabled(int device_id,
                                                             bool enabled) {
  try {
    auto settings = findById(device_id);
    if (!settings.has_value()) {
      if (!createDefaultSettings(device_id)) {
        return false;
      }
      settings = findById(device_id);
    }

    if (settings.has_value()) {
      auto old_settings = settings.value();
      settings.value().setAutoRegistrationEnabled(enabled);

      bool success = save(settings.value());
      if (success) {
        logSettingsChange(device_id, old_settings, settings.value());
        logger_->Info("DeviceSettingsRepository::updateAutoRegistrationEnabled "
                      "- Updated auto registration for device_id " +
                      std::to_string(device_id) + " to " +
                      (enabled ? "enabled" : "disabled"));
      }
      return success;
    }

    return false;

  } catch (const std::exception &e) {
    logger_->Error(
        "DeviceSettingsRepository::updateAutoRegistrationEnabled failed: " +
        std::string(e.what()));
    return false;
  }
}

void DeviceSettingsRepository::logSettingsChange(
    int device_id, const DeviceSettingsEntity &old_settings,
    const DeviceSettingsEntity &new_settings) {
  try {
    std::stringstream change_log;
    change_log << "DeviceSettings changed for device_id " << device_id << ": ";

    if (old_settings.getPollingIntervalMs() !=
        new_settings.getPollingIntervalMs()) {
      change_log << "polling_interval_ms: "
                 << old_settings.getPollingIntervalMs() << " -> "
                 << new_settings.getPollingIntervalMs() << "; ";
    }

    if (old_settings.getConnectionTimeoutMs() !=
        new_settings.getConnectionTimeoutMs()) {
      change_log << "connection_timeout_ms: "
                 << old_settings.getConnectionTimeoutMs() << " -> "
                 << new_settings.getConnectionTimeoutMs() << "; ";
    }

    if (old_settings.getMaxRetryCount() != new_settings.getMaxRetryCount()) {
      change_log << "max_retry_count: " << old_settings.getMaxRetryCount()
                 << " -> " << new_settings.getMaxRetryCount() << "; ";
    }

    if (old_settings.isKeepAliveEnabled() !=
        new_settings.isKeepAliveEnabled()) {
      change_log << "keep_alive_enabled: "
                 << (old_settings.isKeepAliveEnabled() ? "true" : "false")
                 << " -> "
                 << (new_settings.isKeepAliveEnabled() ? "true" : "false")
                 << "; ";
    }

    if (old_settings.isAutoRegistrationEnabled() !=
        new_settings.isAutoRegistrationEnabled()) {
      change_log << "auto_registration_enabled: "
                 << (old_settings.isAutoRegistrationEnabled() ? "true"
                                                              : "false")
                 << " -> "
                 << (new_settings.isAutoRegistrationEnabled() ? "true"
                                                              : "false")
                 << "; ";
    }

    logger_->Info(change_log.str());

  } catch (const std::exception &e) {
    logger_->Error("DeviceSettingsRepository::logSettingsChange failed: " +
                   std::string(e.what()));
  }
}

// 설정 프리셋 관리 메서드들
bool DeviceSettingsRepository::applyIndustrialDefaults(int device_id) {
  try {
    DeviceSettingsEntity settings(device_id);
    settings.resetToIndustrialDefaults();

    bool success = save(settings);

    if (success) {
      logger_->Info("DeviceSettingsRepository::applyIndustrialDefaults - "
                    "Applied industrial defaults to device_id: " +
                    std::to_string(device_id));
    } else {
      logger_->Error("DeviceSettingsRepository::applyIndustrialDefaults - "
                     "Failed to apply industrial defaults to device_id: " +
                     std::to_string(device_id));
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error(
        "DeviceSettingsRepository::applyIndustrialDefaults failed: " +
        std::string(e.what()));
    return false;
  }
}

bool DeviceSettingsRepository::applyHighSpeedMode(int device_id) {
  try {
    DeviceSettingsEntity settings(device_id);
    settings.setHighSpeedMode();

    bool success = save(settings);

    if (success) {
      logger_->Info("DeviceSettingsRepository::applyHighSpeedMode - Applied "
                    "high speed mode to device_id: " +
                    std::to_string(device_id));
    } else {
      logger_->Error("DeviceSettingsRepository::applyHighSpeedMode - Failed to "
                     "apply high speed mode to device_id: " +
                     std::to_string(device_id));
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("DeviceSettingsRepository::applyHighSpeedMode failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool DeviceSettingsRepository::applyStabilityMode(int device_id) {
  try {
    DeviceSettingsEntity settings(device_id);
    settings.setStabilityMode();

    bool success = save(settings);

    if (success) {
      logger_->Info("DeviceSettingsRepository::applyStabilityMode - Applied "
                    "stability mode to device_id: " +
                    std::to_string(device_id));
    } else {
      logger_->Error("DeviceSettingsRepository::applyStabilityMode - Failed to "
                     "apply stability mode to device_id: " +
                     std::to_string(device_id));
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("DeviceSettingsRepository::applyStabilityMode failed: " +
                   std::string(e.what()));
    return false;
  }
}

// 기타 메서드들은 기본 구현
int DeviceSettingsRepository::applyPresetToProtocol(
    const std::string & /* protocol_type */,
    const std::string & /* preset_mode */) {
  logger_->Info("DeviceSettingsRepository::applyPresetToProtocol - Not fully "
                "implemented yet");
  return 0;
}

std::string DeviceSettingsRepository::getSettingsStatistics() const {
  return "{ \"error\": \"Statistics not implemented\" }";
}

std::vector<DeviceSettingsEntity>
DeviceSettingsRepository::findAbnormalSettings() const {
  return {};
}

int DeviceSettingsRepository::bulkUpdateSettings(
    const std::map<int, DeviceSettingsEntity> &settings_map) {
  int updated_count = 0;
  for (const auto &[device_id, settings] : settings_map) {
    DeviceSettingsEntity mutable_settings = settings;
    mutable_settings.setDeviceId(device_id);
    if (save(mutable_settings)) {
      updated_count++;
    }
  }
  return updated_count;
}

std::map<int, std::vector<DeviceSettingsEntity>>
DeviceSettingsRepository::groupByPollingInterval() {
  auto all_settings = findActiveDeviceSettings();
  std::map<int, std::vector<DeviceSettingsEntity>> grouped_settings;

  for (const auto &settings : all_settings) {
    int polling_interval = settings.getPollingIntervalMs();
    grouped_settings[polling_interval].push_back(settings);
  }

  return grouped_settings;
}

DeviceSettingsEntity
DeviceSettingsRepository::createPresetEntity(const std::string &preset_mode,
                                             int device_id) const {
  DeviceSettingsEntity entity(device_id);

  if (preset_mode == "industrial") {
    entity.resetToIndustrialDefaults();
  } else if (preset_mode == "highspeed") {
    entity.setHighSpeedMode();
  } else if (preset_mode == "stability") {
    entity.setStabilityMode();
  } else {
    entity.resetToIndustrialDefaults();
    logger_->Warn(
        "DeviceSettingsRepository::createPresetEntity - Unknown preset mode '" +
        preset_mode + "', using industrial defaults");
  }

  return entity;
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne