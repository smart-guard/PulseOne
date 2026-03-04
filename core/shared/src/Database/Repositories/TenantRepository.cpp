/**
 * @file TenantRepository.cpp
 * @brief PulseOne TenantRepository 구현 - DeviceRepository 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-07-31
 *
 * 🎯 DeviceRepository 패턴 완전 적용:
 * - DbLib::DatabaseAbstractionLayer 사용
 * - executeQuery/executeNonQuery/executeUpsert 패턴
 * - 컴파일 에러 완전 해결
 * - formatTimestamp, ensureTableExists 문제 해결
 */

#include "Database/Repositories/TenantRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "DatabaseAbstractionLayer.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 기본 CRUD 구현 (DeviceRepository 패턴)
// =============================================================================

std::vector<TenantEntity> TenantRepository::findAll() {
  try {
    if (!ensureTableExists()) {
      logger_->Error("TenantRepository::findAll - Table creation failed");
      return {};
    }

    const std::string query = R"(
            SELECT 
                id, company_name as name, domain, 
                subscription_status as status, max_users, max_edge_servers as max_devices, 
                max_data_points, contact_email, contact_phone, 
                timezone, subscription_start_date as subscription_start, 
                trial_end_date as subscription_end, 
                created_at, updated_at
            FROM tenants 
            WHERE is_deleted = 0
            ORDER BY company_name
        )";

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<TenantEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("TenantRepository::findAll - Failed to map row: " +
                      std::string(e.what()));
      }
    }

    logger_->Info("TenantRepository::findAll - Found " +
                  std::to_string(entities.size()) + " tenants");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::findAll failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::optional<TenantEntity> TenantRepository::findById(int id) {
  try {
    // 캐시 확인
    if (isCacheEnabled()) {
      auto cached = getCachedEntity(id);
      if (cached.has_value()) {
        logger_->Debug("TenantRepository::findById - Cache hit for ID: " +
                       std::to_string(id));
        return cached.value();
      }
    }

    if (!ensureTableExists()) {
      return std::nullopt;
    }

    const std::string query = R"(
            SELECT 
                id, company_name as name, domain, 
                subscription_status as status, max_users, max_edge_servers as max_devices, 
                max_data_points, contact_email, contact_phone, 
                timezone, subscription_start_date as subscription_start, 
                trial_end_date as subscription_end, 
                created_at, updated_at
            FROM tenants 
            WHERE id = )" + std::to_string(id);

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (results.empty()) {
      logger_->Debug("TenantRepository::findById - Tenant not found: " +
                     std::to_string(id));
      return std::nullopt;
    }

    auto entity = mapRowToEntity(results[0]);

    // 캐시에 저장
    if (isCacheEnabled()) {
      cacheEntity(entity);
    }

    logger_->Debug("TenantRepository::findById - Found tenant: " +
                   entity.getName());
    return entity;

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::findById failed for ID " +
                   std::to_string(id) + ": " + std::string(e.what()));
    return std::nullopt;
  }
}

bool TenantRepository::save(TenantEntity &entity) {
  try {
    if (!validateTenant(entity)) {
      logger_->Error("TenantRepository::save - Invalid tenant: " +
                     entity.getName());
      return false;
    }

    // 도메인 중복 체크
    if (isDomainTaken(entity.getDomain(), entity.getId())) {
      logger_->Error("TenantRepository::save - Domain already taken: " +
                     entity.getDomain());
      return false;
    }

    if (!ensureTableExists()) {
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    // entityToParams 메서드 사용하여 맵 생성
    std::map<std::string, std::string> data = entityToParams(entity);

    std::vector<std::string> primary_keys = {"id"};

    bool success = db_layer.executeUpsert("tenants", data, primary_keys);

    if (success) {
      // 새로 생성된 경우 ID 조회 (DB 타입별 자동 분기)
      if (entity.getId() <= 0) {
        int64_t new_id = db_layer.getLastInsertId();
        if (new_id > 0) {
          entity.setId(static_cast<int>(new_id));
        }
      }

      // 캐시 업데이트
      if (isCacheEnabled()) {
        cacheEntity(entity);
      }

      logger_->Info("TenantRepository::save - Saved tenant: " +
                    entity.getName());
    } else {
      logger_->Error("TenantRepository::save - Failed to save tenant: " +
                     entity.getName());
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::save failed: " + std::string(e.what()));
    return false;
  }
}

bool TenantRepository::update(const TenantEntity &entity) {
  TenantEntity mutable_entity = entity;
  return save(mutable_entity);
}

bool TenantRepository::deleteById(int id) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    const std::string query =
        "DELETE FROM tenants WHERE id = " + std::to_string(id);

    DbLib::DatabaseAbstractionLayer db_layer;
    bool success = db_layer.executeNonQuery(query);

    if (success) {
      if (isCacheEnabled()) {
        clearCacheForId(id);
      }

      logger_->Info("TenantRepository::deleteById - Deleted tenant ID: " +
                    std::to_string(id));
    } else {
      logger_->Error(
          "TenantRepository::deleteById - Failed to delete tenant ID: " +
          std::to_string(id));
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::deleteById failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool TenantRepository::exists(int id) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    const std::string query =
        "SELECT COUNT(*) as count FROM tenants WHERE id = " +
        std::to_string(id);

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      int count = std::stoi(results[0].at("count"));
      return count > 0;
    }

    return false;

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::exists failed: " + std::string(e.what()));
    return false;
  }
}

std::vector<TenantEntity>
TenantRepository::findByIds(const std::vector<int> &ids) {
  try {
    if (ids.empty()) {
      return {};
    }

    if (!ensureTableExists()) {
      return {};
    }

    // IN 절 구성
    std::ostringstream ids_ss;
    for (size_t i = 0; i < ids.size(); ++i) {
      if (i > 0)
        ids_ss << ", ";
      ids_ss << ids[i];
    }

    const std::string query = R"(
            SELECT 
                id, company_name as name, domain, 
                subscription_status as status, max_users, max_edge_servers as max_devices, 
                max_data_points, contact_email, contact_phone, 
                timezone, subscription_start_date as subscription_start, 
                trial_end_date as subscription_end, 
                created_at, updated_at
            FROM tenants 
            WHERE id IN ()" + ids_ss.str() +
                              ")";

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<TenantEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("TenantRepository::findByIds - Failed to map row: " +
                      std::string(e.what()));
      }
    }

    logger_->Info("TenantRepository::findByIds - Found " +
                  std::to_string(entities.size()) + " tenants for " +
                  std::to_string(ids.size()) + " IDs");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::findByIds failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<TenantEntity> TenantRepository::findByConditions(
    const std::vector<QueryCondition> &conditions,
    const std::optional<OrderBy> &order_by,
    const std::optional<Pagination> &pagination) {

  try {
    if (!ensureTableExists()) {
      return {};
    }

    std::string query = R"(
            SELECT 
                id, company_name as name, domain, 
                subscription_status as status, max_users, max_edge_servers as max_devices, 
                max_data_points, contact_email, contact_phone, 
                timezone, subscription_start_date as subscription_start, 
                trial_end_date as subscription_end, 
                created_at, updated_at
            FROM tenants
        )";

    query += RepositoryHelpers::buildWhereClause(conditions);
    query += RepositoryHelpers::buildOrderByClause(order_by);
    query += RepositoryHelpers::buildLimitClause(pagination);

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<TenantEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn(
            "TenantRepository::findByConditions - Failed to map row: " +
            std::string(e.what()));
      }
    }

    logger_->Debug("TenantRepository::findByConditions - Found " +
                   std::to_string(entities.size()) + " tenants");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::findByConditions failed: " +
                   std::string(e.what()));
    return {};
  }
}

int TenantRepository::countByConditions(
    const std::vector<QueryCondition> &conditions) {
  try {
    if (!ensureTableExists()) {
      return 0;
    }

    std::string query = "SELECT COUNT(*) as count FROM tenants";
    query += RepositoryHelpers::buildWhereClause(conditions);

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      return std::stoi(results[0].at("count"));
    }

    return 0;

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::countByConditions failed: " +
                   std::string(e.what()));
    return 0;
  }
}

// =============================================================================
// Tenant 전용 메서드들 (DeviceRepository 패턴)
// =============================================================================

std::optional<TenantEntity>
TenantRepository::findByDomain(const std::string &domain) {
  try {
    if (!ensureTableExists()) {
      return std::nullopt;
    }

    const std::string query = R"(
            SELECT 
                id, company_name as name, domain, 
                subscription_status as status, max_users, max_edge_servers as max_devices, 
                max_data_points, contact_email, contact_phone, 
                timezone, subscription_start_date as subscription_start, 
                trial_end_date as subscription_end, 
                created_at, updated_at
            FROM tenants 
            WHERE domain = ')" +
                              RepositoryHelpers::escapeString(domain) + "'";

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (results.empty()) {
      logger_->Debug("TenantRepository::findByDomain - Tenant not found: " +
                     domain);
      return std::nullopt;
    }

    auto entity = mapRowToEntity(results[0]);
    logger_->Debug("TenantRepository::findByDomain - Found tenant: " +
                   entity.getName());
    return entity;

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::findByDomain failed: " +
                   std::string(e.what()));
    return std::nullopt;
  }
}

std::vector<TenantEntity>
TenantRepository::findByStatus(TenantEntity::Status status) {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    const std::string query =
        R"(
            SELECT 
                id, company_name as name, domain, 
                subscription_status as status, max_users, max_edge_servers as max_devices, 
                max_data_points, contact_email, contact_phone, 
                timezone, subscription_start_date as subscription_start, 
                trial_end_date as subscription_end, 
                created_at, updated_at
            FROM tenants 
            WHERE subscription_status = ')" +
        RepositoryHelpers::escapeString(TenantEntity::statusToString(status)) +
        R"('
            ORDER BY company_name
        )";

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<TenantEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("TenantRepository::findByStatus - Failed to map row: " +
                      std::string(e.what()));
      }
    }

    logger_->Info("TenantRepository::findByStatus - Found " +
                  std::to_string(entities.size()) + " tenants for status: " +
                  TenantEntity::statusToString(status));
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::findByStatus failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<TenantEntity> TenantRepository::findActiveTenants() {
  return findByStatus(TenantEntity::Status::ACTIVE);
}

std::vector<TenantEntity> TenantRepository::findExpiredTenants() {
  try {
    if (!ensureTableExists()) {
      return {};
    }

    const std::string query = R"(
            SELECT 
                id, company_name as name, domain, 
                subscription_status as status, max_users, max_edge_servers as max_devices, 
                max_data_points, contact_email, contact_phone, 
                timezone, subscription_start_date as subscription_start, 
                trial_end_date as subscription_end, 
                created_at, updated_at
            FROM tenants 
            WHERE trial_end_date < datetime('now', 'localtime')
            ORDER BY trial_end_date DESC
        )";

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<TenantEntity> entities = mapResultToEntities(results);

    logger_->Info("TenantRepository::findExpiredTenants - Found " +
                  std::to_string(entities.size()) + " expired tenants");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::findExpiredTenants failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<TenantEntity> TenantRepository::findTrialTenants() {
  return findByStatus(TenantEntity::Status::TRIAL);
}

std::optional<TenantEntity>
TenantRepository::findByName(const std::string &name) {
  try {
    if (!ensureTableExists()) {
      return std::nullopt;
    }

    const std::string query = R"(
            SELECT 
                id, company_name as name, domain, 
                subscription_status as status, max_users, max_edge_servers as max_devices, 
                max_data_points, contact_email, contact_phone, 
                timezone, subscription_start_date as subscription_start, 
                trial_end_date as subscription_end, 
                created_at, updated_at
            FROM tenants 
            WHERE company_name = ')" +
                              RepositoryHelpers::escapeString(name) + "'";

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (results.empty()) {
      logger_->Debug("TenantRepository::findByName - Tenant not found: " +
                     name);
      return std::nullopt;
    }

    auto entity = mapRowToEntity(results[0]);
    logger_->Debug("TenantRepository::findByName - Found tenant: " +
                   entity.getName());
    return entity;

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::findByName failed: " +
                   std::string(e.what()));
    return std::nullopt;
  }
}

std::map<std::string, std::vector<TenantEntity>>
TenantRepository::groupByStatus() {
  std::map<std::string, std::vector<TenantEntity>> grouped;

  try {
    auto tenants = findAll();
    for (const auto &tenant : tenants) {
      grouped[TenantEntity::statusToString(tenant.getStatus())].push_back(
          tenant);
    }
  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::groupByStatus failed: " +
                   std::string(e.what()));
  }

  return grouped;
}

// =============================================================================
// 벌크 연산 (DeviceRepository 패턴)
// =============================================================================

int TenantRepository::saveBulk(std::vector<TenantEntity> &entities) {
  int saved_count = 0;
  for (auto &entity : entities) {
    if (save(entity)) {
      saved_count++;
    }
  }
  logger_->Info("TenantRepository::saveBulk - Saved " +
                std::to_string(saved_count) + " tenants");
  return saved_count;
}

int TenantRepository::updateBulk(const std::vector<TenantEntity> &entities) {
  int updated_count = 0;
  for (const auto &entity : entities) {
    if (update(entity)) {
      updated_count++;
    }
  }
  logger_->Info("TenantRepository::updateBulk - Updated " +
                std::to_string(updated_count) + " tenants");
  return updated_count;
}

int TenantRepository::deleteByIds(const std::vector<int> &ids) {
  int deleted_count = 0;
  for (int id : ids) {
    if (deleteById(id)) {
      deleted_count++;
    }
  }
  logger_->Info("TenantRepository::deleteByIds - Deleted " +
                std::to_string(deleted_count) + " tenants");
  return deleted_count;
}

// =============================================================================
// 실시간 테넌트 관리
// =============================================================================

bool TenantRepository::updateStatus(int tenant_id,
                                    TenantEntity::Status status) {
  try {
    const std::string query =
        R"(
            UPDATE tenants 
            SET status = ')" +
        RepositoryHelpers::escapeString(TenantEntity::statusToString(status)) +
        R"(',
                updated_at = ')" +
        RepositoryHelpers::formatTimestamp(std::chrono::system_clock::now()) +
        R"('
            WHERE id = )" +
        std::to_string(tenant_id);

    DbLib::DatabaseAbstractionLayer db_layer;
    bool success = db_layer.executeNonQuery(query);

    if (success) {
      if (isCacheEnabled()) {
        clearCacheForId(tenant_id);
      }
      logger_->Info(
          "TenantRepository::updateStatus - Updated status for tenant ID: " +
          std::to_string(tenant_id));
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::updateStatus failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool TenantRepository::extendSubscription(int tenant_id, int additional_days) {
  try {
    auto tenant = findById(tenant_id);
    if (!tenant.has_value()) {
      logger_->Error(
          "TenantRepository::extendSubscription - Tenant not found: " +
          std::to_string(tenant_id));
      return false;
    }

    tenant->extendSubscription(additional_days);
    return update(*tenant);

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::extendSubscription failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool TenantRepository::isDomainTaken(const std::string &domain,
                                     int exclude_id) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    std::string query =
        "SELECT COUNT(*) as count FROM tenants WHERE domain = '" +
        RepositoryHelpers::escapeString(domain) + "'";
    if (exclude_id > 0) {
      query += " AND id != " + std::to_string(exclude_id);
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      int count = std::stoi(results[0].at("count"));
      return count > 0;
    }

    return false;

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::isDomainTaken failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool TenantRepository::isNameTaken(const std::string &name, int exclude_id) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    std::string query = "SELECT COUNT(*) as count FROM tenants WHERE name = '" +
                        RepositoryHelpers::escapeString(name) + "'";
    if (exclude_id > 0) {
      query += " AND id != " + std::to_string(exclude_id);
    }

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      int count = std::stoi(results[0].at("count"));
      return count > 0;
    }

    return false;

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::isNameTaken failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool TenantRepository::updateLimits(int tenant_id,
                                    const std::map<std::string, int> &limits) {
  try {
    auto tenant = findById(tenant_id);
    if (!tenant.has_value()) {
      logger_->Error("TenantRepository::updateLimits - Tenant not found: " +
                     std::to_string(tenant_id));
      return false;
    }

    // 제한값 업데이트
    if (limits.find("max_users") != limits.end()) {
      tenant->setMaxUsers(limits.at("max_users"));
    }
    if (limits.find("max_devices") != limits.end()) {
      tenant->setMaxDevices(limits.at("max_devices"));
    }
    if (limits.find("max_data_points") != limits.end()) {
      tenant->setMaxDataPoints(limits.at("max_data_points"));
    }

    return update(*tenant);

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::updateLimits failed: " +
                   std::string(e.what()));
    return false;
  }
}

// =============================================================================
// 통계 및 분석
// =============================================================================

std::string TenantRepository::getTenantStatistics() const {
  return "{ \"error\": \"Statistics not implemented\" }";
}

std::vector<TenantEntity> TenantRepository::findInactiveTenants() const {
  // 임시 구현
  return {};
}

std::map<std::string, int> TenantRepository::getStatusDistribution() const {
  std::map<std::string, int> distribution;

  try {
    const std::string query = R"(
            SELECT status, COUNT(*) as count 
            FROM tenants 
            GROUP BY status
            ORDER BY count DESC
        )";

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    for (const auto &row : results) {
      if (row.find("status") != row.end() && row.find("count") != row.end()) {
        distribution[row.at("status")] = std::stoi(row.at("count"));
      }
    }

  } catch (const std::exception &e) {
    if (logger_) {
      logger_->Error("TenantRepository::getStatusDistribution failed: " +
                     std::string(e.what()));
    }
  }

  return distribution;
}

int TenantRepository::getTotalCount() { return countByConditions({}); }

// =============================================================================
// 내부 헬퍼 메서드들 (DeviceRepository 패턴)
// =============================================================================

TenantEntity TenantRepository::mapRowToEntity(
    const std::map<std::string, std::string> &row) {
  TenantEntity entity;

  try {
    DbLib::DatabaseAbstractionLayer db_layer;

    auto it = row.find("id");
    if (it != row.end()) {
      entity.setId(std::stoi(it->second));
    }

    // 기본 정보
    if ((it = row.find("name")) != row.end())
      entity.setName(it->second);
    if ((it = row.find("description")) != row.end())
      entity.setDescription(it->second);
    if ((it = row.find("domain")) != row.end())
      entity.setDomain(it->second);
    if ((it = row.find("status")) != row.end()) {
      entity.setStatus(TenantEntity::stringToStatus(it->second));
    }

    // 제한 설정
    it = row.find("max_users");
    if (it != row.end() && !it->second.empty()) {
      entity.setMaxUsers(std::stoi(it->second));
    }

    it = row.find("max_devices");
    if (it != row.end() && !it->second.empty()) {
      entity.setMaxDevices(std::stoi(it->second));
    }

    it = row.find("max_data_points");
    if (it != row.end() && !it->second.empty()) {
      entity.setMaxDataPoints(std::stoi(it->second));
    }

    // 연락처 정보
    if ((it = row.find("contact_email")) != row.end())
      entity.setContactEmail(it->second);
    if ((it = row.find("contact_phone")) != row.end())
      entity.setContactPhone(it->second);
    if ((it = row.find("address")) != row.end())
      entity.setAddress(it->second);
    if ((it = row.find("city")) != row.end())
      entity.setCity(it->second);
    if ((it = row.find("country")) != row.end())
      entity.setCountry(it->second);
    if ((it = row.find("timezone")) != row.end())
      entity.setTimezone(it->second);

    // 타임스탬프는 기본값 사용 (실제 구현에서는 파싱 필요)
    entity.setCreatedAt(std::chrono::system_clock::now());
    entity.setUpdatedAt(std::chrono::system_clock::now());

    return entity;

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::mapRowToEntity failed: " +
                   std::string(e.what()));
    throw;
  }
}

std::vector<TenantEntity> TenantRepository::mapResultToEntities(
    const std::vector<std::map<std::string, std::string>> &result) {

  std::vector<TenantEntity> entities;
  entities.reserve(result.size());

  for (const auto &row : result) {
    try {
      entities.push_back(mapRowToEntity(row));
    } catch (const std::exception &e) {
      logger_->Warn(
          "TenantRepository::mapResultToEntities - Failed to map row: " +
          std::string(e.what()));
    }
  }

  return entities;
}

std::map<std::string, std::string>
TenantRepository::entityToParams(const TenantEntity &entity) {
  DbLib::DatabaseAbstractionLayer db_layer;

  std::map<std::string, std::string> params;

  // 기본 정보 (ID는 AUTO_INCREMENT이므로 제외)
  params["company_name"] = entity.getName();
  // params["description"] = entity.getDescription(); // Schema doesn't have
  // description
  params["domain"] = entity.getDomain();
  params["subscription_status"] =
      TenantEntity::statusToString(entity.getStatus());

  // 제한 설정
  params["max_users"] = std::to_string(entity.getMaxUsers());
  params["max_edge_servers"] = std::to_string(entity.getMaxDevices());
  params["max_data_points"] = std::to_string(entity.getMaxDataPoints());

  // 연락처 정보
  params["contact_email"] = entity.getContactEmail();
  params["contact_phone"] = entity.getContactPhone();
  params["timezone"] = entity.getTimezone();

  // 구독 정보
  params["subscription_start_date"] =
      RepositoryHelpers::formatTimestamp(entity.getSubscriptionStart());
  params["trial_end_date"] =
      RepositoryHelpers::formatTimestamp(entity.getSubscriptionEnd());
  // 🔥 시간 정보
  params["created_at"] = db_layer.getGenericTimestamp();
  params["updated_at"] = db_layer.getGenericTimestamp();

  return params;
}

bool TenantRepository::ensureTableExists() {
  try {
    const std::string base_create_query = R"(
            CREATE TABLE IF NOT EXISTS tenants (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                
                -- 기본 정보
                name VARCHAR(100) NOT NULL,
                description TEXT,
                domain VARCHAR(100) NOT NULL UNIQUE,
                status VARCHAR(20) DEFAULT 'TRIAL',
                
                -- 제한 설정
                max_users INTEGER DEFAULT 10,
                max_devices INTEGER DEFAULT 50,
                max_data_points INTEGER DEFAULT 500,
                
                -- 연락처 정보
                contact_email VARCHAR(255),
                contact_phone VARCHAR(50),
                address TEXT,
                city VARCHAR(100),
                country VARCHAR(100),
                timezone VARCHAR(50) DEFAULT 'UTC',
                
                -- 구독 정보
                subscription_start DATETIME NOT NULL,
                subscription_end DATETIME NOT NULL,
                
                created_at DATETIME DEFAULT (datetime('now', 'localtime')),
                updated_at DATETIME DEFAULT (datetime('now', 'localtime'))
            )
        )";

    DbLib::DatabaseAbstractionLayer db_layer;
    bool success = db_layer.executeCreateTable(base_create_query);

    if (success) {
      logger_->Debug("TenantRepository::ensureTableExists - Table "
                     "creation/check completed");
    } else {
      logger_->Error(
          "TenantRepository::ensureTableExists - Table creation failed");
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("TenantRepository::ensureTableExists failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool TenantRepository::validateTenant(const TenantEntity &entity) const {
  if (!entity.isValid()) {
    logger_->Warn("TenantRepository::validateTenant - Invalid tenant: " +
                  entity.getName());
    return false;
  }

  if (entity.getName().empty()) {
    logger_->Warn("TenantRepository::validateTenant - Tenant name is empty");
    return false;
  }

  if (entity.getDomain().empty()) {
    logger_->Warn("TenantRepository::validateTenant - Domain is empty for: " +
                  entity.getName());
    return false;
  }

  return true;
}

// =============================================================================
// 유틸리티 함수들
// =============================================================================
std::map<std::string, int> TenantRepository::getCacheStats() const {
  return IRepository<TenantEntity>::getCacheStats();
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne