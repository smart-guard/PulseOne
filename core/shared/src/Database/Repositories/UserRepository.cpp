/**
 * @file UserRepository.cpp
 * @brief PulseOne UserRepository 구현 - DeviceRepository 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-07-31
 *
 * 🎯 DeviceRepository 패턴 완전 적용:
 * - DbLib::DatabaseAbstractionLayer 사용
 * - executeQuery/executeNonQuery/executeUpsert 패턴
 * - 컴파일 에러 완전 해결
 * - vtable 에러 해결 (모든 순수 가상 함수 구현)
 */

#include "Database/Repositories/UserRepository.h"
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

std::vector<UserEntity> UserRepository::findAll() {
  try {
    if (!ensureTableExists()) {
      logger_->Error("UserRepository::findAll - Table creation failed");
      return {};
    }

    const std::string query = R"(
            SELECT 
                id, tenant_id, username, email, password_hash, full_name,
                role, is_enabled, phone_number, department, permissions,
                login_count, last_login_at, notes, created_by, created_at, updated_at
            FROM users 
            ORDER BY username
        )";

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<UserEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("UserRepository::findAll - Failed to map row: " +
                      std::string(e.what()));
      }
    }

    logger_->Info("UserRepository::findAll - Found " +
                  std::to_string(entities.size()) + " users");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("UserRepository::findAll failed: " + std::string(e.what()));
    return {};
  }
}

std::optional<UserEntity> UserRepository::findById(int id) {
  try {
    // 캐시 확인
    if (isCacheEnabled()) {
      auto cached = getCachedEntity(id);
      if (cached.has_value()) {
        logger_->Debug("UserRepository::findById - Cache hit for ID: " +
                       std::to_string(id));
        return cached.value();
      }
    }

    if (!ensureTableExists()) {
      return std::nullopt;
    }

    const std::string query = R"(
            SELECT 
                id, tenant_id, username, email, password_hash, full_name,
                role, is_enabled, phone_number, department, permissions,
                login_count, last_login_at, notes, created_by, created_at, updated_at
            FROM users 
            WHERE id = )" + std::to_string(id);

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (results.empty()) {
      logger_->Debug("UserRepository::findById - User not found: " +
                     std::to_string(id));
      return std::nullopt;
    }

    auto entity = mapRowToEntity(results[0]);

    // 캐시에 저장
    if (isCacheEnabled()) {
      cacheEntity(entity);
    }

    logger_->Debug("UserRepository::findById - Found user: " +
                   entity.getUsername());
    return entity;

  } catch (const std::exception &e) {
    logger_->Error("UserRepository::findById failed for ID " +
                   std::to_string(id) + ": " + std::string(e.what()));
    return std::nullopt;
  }
}

bool UserRepository::save(UserEntity &entity) {
  try {
    if (!validateEntity(entity)) {
      logger_->Error("UserRepository::save - Invalid user: " +
                     entity.getUsername());
      return false;
    }

    if (!ensureTableExists()) {
      return false;
    }

    DbLib::DatabaseAbstractionLayer db_layer;

    std::map<std::string, std::string> data = entityToParams(entity);

    std::vector<std::string> primary_keys = {"id"};

    bool success = db_layer.executeUpsert("users", data, primary_keys);

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

      logger_->Info("UserRepository::save - Saved user: " +
                    entity.getUsername());
    } else {
      logger_->Error("UserRepository::save - Failed to save user: " +
                     entity.getUsername());
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("UserRepository::save failed: " + std::string(e.what()));
    return false;
  }
}

bool UserRepository::update(const UserEntity &entity) {
  UserEntity mutable_entity = entity;
  return save(mutable_entity);
}

bool UserRepository::deleteById(int id) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    const std::string query =
        "DELETE FROM users WHERE id = " + std::to_string(id);

    DbLib::DatabaseAbstractionLayer db_layer;
    bool success = db_layer.executeNonQuery(query);

    if (success) {
      if (isCacheEnabled()) {
        clearCacheForId(id);
      }

      logger_->Info("UserRepository::deleteById - Deleted user ID: " +
                    std::to_string(id));
    } else {
      logger_->Error("UserRepository::deleteById - Failed to delete user ID: " +
                     std::to_string(id));
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("UserRepository::deleteById failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool UserRepository::exists(int id) {
  try {
    if (!ensureTableExists()) {
      return false;
    }

    const std::string query =
        "SELECT COUNT(*) as count FROM users WHERE id = " + std::to_string(id);

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      int count = std::stoi(results[0].at("count"));
      return count > 0;
    }

    return false;

  } catch (const std::exception &e) {
    logger_->Error("UserRepository::exists failed: " + std::string(e.what()));
    return false;
  }
}

std::vector<UserEntity> UserRepository::findByIds(const std::vector<int> &ids) {
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
                id, tenant_id, username, email, password_hash, full_name,
                role, is_enabled, phone_number, department, permissions,
                login_count, last_login_at, notes, created_by, created_at, updated_at
            FROM users 
            WHERE id IN ()" + ids_ss.str() +
                              ")";

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<UserEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("UserRepository::findByIds - Failed to map row: " +
                      std::string(e.what()));
      }
    }

    logger_->Info("UserRepository::findByIds - Found " +
                  std::to_string(entities.size()) + " users for " +
                  std::to_string(ids.size()) + " IDs");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("UserRepository::findByIds failed: " +
                   std::string(e.what()));
    return {};
  }
}

std::vector<UserEntity>
UserRepository::findByConditions(const std::vector<QueryCondition> &conditions,
                                 const std::optional<OrderBy> &order_by,
                                 const std::optional<Pagination> &pagination) {

  try {
    if (!ensureTableExists()) {
      return {};
    }

    std::string query = R"(
            SELECT 
                id, tenant_id, username, email, password_hash, full_name,
                role, is_enabled, phone_number, department, permissions,
                login_count, last_login_at, notes, created_by, created_at, updated_at
            FROM users
        )";

    query += RepositoryHelpers::buildWhereClause(conditions);
    query += RepositoryHelpers::buildOrderByClause(order_by);
    query += RepositoryHelpers::buildLimitClause(pagination);

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    std::vector<UserEntity> entities;
    entities.reserve(results.size());

    for (const auto &row : results) {
      try {
        entities.push_back(mapRowToEntity(row));
      } catch (const std::exception &e) {
        logger_->Warn("UserRepository::findByConditions - Failed to map row: " +
                      std::string(e.what()));
      }
    }

    logger_->Debug("UserRepository::findByConditions - Found " +
                   std::to_string(entities.size()) + " users");
    return entities;

  } catch (const std::exception &e) {
    logger_->Error("UserRepository::findByConditions failed: " +
                   std::string(e.what()));
    return {};
  }
}

int UserRepository::countByConditions(
    const std::vector<QueryCondition> &conditions) {
  try {
    if (!ensureTableExists()) {
      return 0;
    }

    std::string query = "SELECT COUNT(*) as count FROM users";
    query += RepositoryHelpers::buildWhereClause(conditions);

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (!results.empty() && results[0].find("count") != results[0].end()) {
      return std::stoi(results[0].at("count"));
    }

    return 0;

  } catch (const std::exception &e) {
    logger_->Error("UserRepository::countByConditions failed: " +
                   std::string(e.what()));
    return 0;
  }
}

// =============================================================================
// User 전용 메서드들 (DeviceRepository 패턴)
// =============================================================================

std::optional<UserEntity>
UserRepository::findByUsername(const std::string &username) {
  try {
    if (!ensureTableExists()) {
      return std::nullopt;
    }

    const std::string query = R"(
            SELECT 
                id, tenant_id, username, email, password_hash, full_name,
                role, is_enabled, phone_number, department, permissions,
                login_count, last_login_at, notes, created_by, created_at, updated_at
            FROM users 
            WHERE username = ')" +
                              RepositoryHelpers::escapeString(username) + "'";

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (results.empty()) {
      return std::nullopt;
    }

    return mapRowToEntity(results[0]);

  } catch (const std::exception &e) {
    logger_->Error("UserRepository::findByUsername failed: " +
                   std::string(e.what()));
    return std::nullopt;
  }
}

std::optional<UserEntity>
UserRepository::findByEmail(const std::string &email) {
  try {
    if (!ensureTableExists()) {
      return std::nullopt;
    }

    const std::string query = R"(
            SELECT 
                id, tenant_id, username, email, password_hash, full_name,
                role, is_enabled, phone_number, department, permissions,
                login_count, last_login_at, notes, created_by, created_at, updated_at
            FROM users 
            WHERE email = ')" +
                              RepositoryHelpers::escapeString(email) + "'";

    DbLib::DatabaseAbstractionLayer db_layer;
    auto results = db_layer.executeQuery(query);

    if (results.empty()) {
      return std::nullopt;
    }

    return mapRowToEntity(results[0]);

  } catch (const std::exception &e) {
    logger_->Error("UserRepository::findByEmail failed: " +
                   std::string(e.what()));
    return std::nullopt;
  }
}

std::vector<UserEntity> UserRepository::findByTenant(int tenant_id) {
  std::vector<QueryCondition> conditions = {
      QueryCondition("tenant_id", "=", std::to_string(tenant_id))};
  return findByConditions(conditions);
}

std::vector<UserEntity> UserRepository::findByRole(const std::string &role) {
  std::vector<QueryCondition> conditions = {QueryCondition("role", "=", role)};
  return findByConditions(conditions);
}

std::vector<UserEntity> UserRepository::findByEnabled(bool enabled) {
  std::vector<QueryCondition> conditions = {
      QueryCondition("is_enabled", "=", enabled ? "1" : "0")};
  return findByConditions(conditions);
}

std::vector<UserEntity>
UserRepository::findByDepartment(const std::string &department) {
  std::vector<QueryCondition> conditions = {
      QueryCondition("department", "=", department)};
  return findByConditions(conditions);
}

// =============================================================================
// 벌크 연산 (DeviceRepository 패턴)
// =============================================================================

int UserRepository::saveBulk(std::vector<UserEntity> &entities) {
  int saved_count = 0;
  for (auto &entity : entities) {
    if (save(entity)) {
      saved_count++;
    }
  }
  logger_->Info("UserRepository::saveBulk - Saved " +
                std::to_string(saved_count) + " users");
  return saved_count;
}

int UserRepository::updateBulk(const std::vector<UserEntity> &entities) {
  int updated_count = 0;
  for (const auto &entity : entities) {
    if (update(entity)) {
      updated_count++;
    }
  }
  logger_->Info("UserRepository::updateBulk - Updated " +
                std::to_string(updated_count) + " users");
  return updated_count;
}

int UserRepository::deleteByIds(const std::vector<int> &ids) {
  int deleted_count = 0;
  for (int id : ids) {
    if (deleteById(id)) {
      deleted_count++;
    }
  }
  logger_->Info("UserRepository::deleteByIds - Deleted " +
                std::to_string(deleted_count) + " users");
  return deleted_count;
}

// =============================================================================
// 통계 메서드들 (DeviceRepository 패턴)
// =============================================================================

int UserRepository::countByTenant(int tenant_id) {
  std::vector<QueryCondition> conditions = {
      QueryCondition("tenant_id", "=", std::to_string(tenant_id))};
  return countByConditions(conditions);
}

int UserRepository::countByRole(const std::string &role) {
  std::vector<QueryCondition> conditions = {QueryCondition("role", "=", role)};
  return countByConditions(conditions);
}

int UserRepository::countActiveUsers() {
  std::vector<QueryCondition> conditions = {
      QueryCondition("is_enabled", "=", "1")};
  return countByConditions(conditions);
}

// =============================================================================
// 인증 관련 메서드들 (DeviceRepository 패턴)
// =============================================================================

bool UserRepository::recordLogin(int user_id) {
  try {
    auto user_opt = findById(user_id);
    if (!user_opt.has_value()) {
      return false;
    }

    UserEntity user = user_opt.value();
    user.updateLastLogin();

    return update(user);

  } catch (const std::exception &e) {
    logger_->Error("UserRepository::recordLogin failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool UserRepository::isUsernameTaken(const std::string &username) {
  return findByUsername(username).has_value();
}

bool UserRepository::isEmailTaken(const std::string &email) {
  return findByEmail(email).has_value();
}

std::map<std::string, int> UserRepository::getCacheStats() const {
  return IRepository<UserEntity>::getCacheStats();
}

int UserRepository::getTotalCount() { return countByConditions({}); }

// =============================================================================
// 내부 헬퍼 메서드들 (DeviceRepository 패턴)
// =============================================================================

UserEntity
UserRepository::mapRowToEntity(const std::map<std::string, std::string> &row) {
  UserEntity entity;

  try {
    DbLib::DatabaseAbstractionLayer db_layer;

    auto it = row.find("id");
    if (it != row.end()) {
      entity.setId(std::stoi(it->second));
    }

    it = row.find("tenant_id");
    if (it != row.end()) {
      entity.setTenantId(std::stoi(it->second));
    }

    if ((it = row.find("username")) != row.end())
      entity.setUsername(it->second);
    if ((it = row.find("email")) != row.end())
      entity.setEmail(it->second);
    if ((it = row.find("full_name")) != row.end())
      entity.setFullName(it->second);
    if ((it = row.find("role")) != row.end())
      entity.setRole(it->second);
    if ((it = row.find("phone_number")) != row.end())
      entity.setPhoneNumber(it->second);
    if ((it = row.find("department")) != row.end())
      entity.setDepartment(it->second);
    if ((it = row.find("notes")) != row.end())
      entity.setNotes(it->second);

    it = row.find("is_enabled");
    if (it != row.end()) {
      entity.setEnabled(db_layer.parseBoolean(it->second));
    }

    it = row.find("login_count");
    if (it != row.end()) {
      entity.setLoginCount(std::stoi(it->second));
    }

    // 권한 파싱 (간단한 구현)
    if ((it = row.find("permissions")) != row.end() && !it->second.empty()) {
      // 간단한 JSON 배열 파싱
      std::vector<std::string> permissions;
      // 실제 구현에서는 JSON 라이브러리 사용
      entity.setPermissions(permissions);
    }

    return entity;

  } catch (const std::exception &e) {
    logger_->Error("UserRepository::mapRowToEntity failed: " +
                   std::string(e.what()));
    throw;
  }
}

std::map<std::string, std::string>
UserRepository::entityToParams(const UserEntity &entity) {
  DbLib::DatabaseAbstractionLayer db_layer;

  std::map<std::string, std::string> params;

  if (entity.getId() > 0) {
    params["id"] = std::to_string(entity.getId());
  }

  params["tenant_id"] = std::to_string(entity.getTenantId());
  params["username"] = entity.getUsername();
  params["email"] = entity.getEmail();
  params["full_name"] = entity.getFullName();
  params["role"] = entity.getRole();
  params["is_enabled"] = db_layer.formatBoolean(entity.isEnabled());
  params["phone_number"] = entity.getPhoneNumber();
  params["department"] = entity.getDepartment();
  params["login_count"] = std::to_string(entity.getLoginCount());
  params["notes"] = entity.getNotes();

  // 권한 JSON 직렬화 (간단한 구현)
  params["permissions"] = "[]"; // 실제로는 JSON 직렬화    // 🔥 시간 정보
  params["created_at"] = db_layer.getGenericTimestamp();
  params["updated_at"] = db_layer.getGenericTimestamp();

  return params;
}

bool UserRepository::ensureTableExists() {
  try {
    const std::string create_query = R"(
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                tenant_id INTEGER NOT NULL,
                username VARCHAR(50) UNIQUE NOT NULL,
                email VARCHAR(100) UNIQUE NOT NULL,
                password_hash VARCHAR(255) NOT NULL,
                full_name VARCHAR(100),
                role VARCHAR(20) NOT NULL DEFAULT 'viewer',
                is_enabled INTEGER DEFAULT 1,
                phone_number VARCHAR(20),
                department VARCHAR(50),
                permissions TEXT DEFAULT '[]',
                login_count INTEGER DEFAULT 0,
                last_login_at DATETIME DEFAULT (datetime('now', 'localtime')),
                notes TEXT,
                created_by INTEGER,
                created_at DATETIME DEFAULT (datetime('now', 'localtime')),
                updated_at DATETIME DEFAULT (datetime('now', 'localtime')),
                
                FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
                FOREIGN KEY (created_by) REFERENCES users(id)
            )
        )";

    DbLib::DatabaseAbstractionLayer db_layer;
    bool success = db_layer.executeCreateTable(create_query);

    if (success) {
      logger_->Debug(
          "UserRepository::ensureTableExists - Table creation/check completed");
    } else {
      logger_->Error(
          "UserRepository::ensureTableExists - Table creation failed");
    }

    return success;

  } catch (const std::exception &e) {
    logger_->Error("UserRepository::ensureTableExists failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool UserRepository::validateEntity(const UserEntity &entity) const {
  return entity.isValid();
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne