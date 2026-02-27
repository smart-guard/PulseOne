#ifndef PULSEONE_BASE_ENTITY_H
#define PULSEONE_BASE_ENTITY_H

/**
 * @file BaseEntity.h
 * @brief PulseOne 기본 엔티티 템플릿 - 크로스플랫폼 버전
 * @author PulseOne Development Team
 * @date 2025-07-31
 */
#include "Common/BasicTypes.h"
#include "Common/DriverStatistics.h"
#include "Common/Enums.h"
#include "Common/Structs.h"
#include "CoreEntity.hpp"
#include "Database/RepositoryFactory.h"
#include "DatabaseAbstractionLayer.hpp"
#include "DatabaseManager.hpp"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"
#include <chrono>
#include <iomanip>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>

using json = nlohmann::json;

namespace PulseOne {
namespace Database {

// Use DbLib's EntityState
using EntityState = DbLib::EntityState;

namespace Entities {
// Basic types used in template signatures if needed,
// but most are now handled in derived classes.
}

/**
 * @brief 모든 엔티티의 기본 클래스 (템플릿)
 */
template <typename DerivedType>
class BaseEntity : public DbLib::CoreEntity<DerivedType> {
public:
  using Base = DbLib::CoreEntity<DerivedType>;

  BaseEntity()
      : Base(), db_manager_(&DbLib::DatabaseManager::getInstance()),
        config_manager_(&ConfigManager::getInstance()),
        logger_(&LogManager::getInstance()) {}

  explicit BaseEntity(int id) : Base(id) {
    db_manager_ = &DbLib::DatabaseManager::getInstance();
    config_manager_ = &ConfigManager::getInstance();
    logger_ = &LogManager::getInstance();
  }

  virtual ~BaseEntity() = default;

  // Use base class getters
  using Base::getCreatedAt;
  using Base::getId;
  using Base::getState;
  using Base::getUpdatedAt;
  using Base::isDeleted;
  using Base::isError;
  using Base::isLoaded;
  using Base::isModified;
  using Base::isNew;
  using Base::markDeleted;
  using Base::markError;
  using Base::markModified;
  using Base::markSaved;
  using Base::setId;

  // =======================================================================
  // Repository 패턴 지원
  // =======================================================================

protected:
  /**
   * @brief DerivedType의 getRepository() 호출 (CRTP)
   */
  auto getRepository() const {
    return static_cast<const DerivedType *>(this)->getRepository();
  }

  bool saveViaRepository() {
    try {
      auto repo = getRepository();
      if (repo) {
        // derived call for save
        bool success = repo->save(static_cast<DerivedType &>(*this));
        if (success) {
          markSaved();
        }
        return success;
      }
      return false;
    } catch (const std::exception &e) {
      if (logger_)
        logger_->Error("Save failed: " + std::string(e.what()));
      markError();
      return false;
    }
  }

  bool updateViaRepository() {
    try {
      auto repo = getRepository();
      if (repo) {
        bool success = repo->update(static_cast<const DerivedType &>(*this));
        if (success) {
          markSaved();
        }
        return success;
      }
      return false;
    } catch (const std::exception &e) {
      if (logger_)
        logger_->Error("Update failed: " + std::string(e.what()));
      markError();
      return false;
    }
  }

  bool deleteViaRepository() {
    try {
      auto repo = getRepository();
      if (repo) {
        bool success = repo->deleteById(getEntityId());
        if (success) {
          markDeleted();
        }
        return success;
      }
      return false;
    } catch (const std::exception &e) {
      if (logger_)
        logger_->Error("Delete failed: " + std::string(e.what()));
      markError();
      return false;
    }
  }

  bool loadViaRepository() {
    try {
      auto repo = getRepository();
      if (repo) {
        auto loaded = repo->findById(getEntityId());
        if (loaded.has_value()) {
          static_cast<DerivedType &>(*this) = loaded.value();
          markSaved();
          return true;
        }
      }
      return false;
    } catch (const std::exception &e) {
      if (logger_)
        logger_->Error("Load failed: " + std::string(e.what()));
      markError();
      return false;
    }
  }

  int getEntityId() const { return this->id_; }

  virtual std::string getEntityTypeName() const {
    return "BaseEntity"; // Derived should override if needed for logging
  }

public:
  // =======================================================================
  // 공통 DB 연산 (순수 가상 함수)
  // =======================================================================

  virtual bool loadFromDatabase() = 0;
  virtual bool saveToDatabase() = 0;
  virtual bool updateToDatabase() = 0;
  virtual bool deleteFromDatabase() = 0;

  // =======================================================================
  // JSON 직렬화
  // =======================================================================

  virtual json toJson() const = 0;
  virtual bool fromJson(const json &data) = 0;
  virtual std::string toString() const = 0;

  virtual bool isValid() const {
    return this->state_ != EntityState::ERROR && this->id_ >= 0;
  }

protected:
  // =======================================================================
  // 크로스플랫폼 DB 쿼리 실행
  // =======================================================================

  /**
   * @brief 통합 쿼리 실행 - DbLib Abstraction Layer 사용
   */
  std::vector<std::map<std::string, std::string>>
  executeUnifiedQuery(const std::string &sql) {
    try {
      DbLib::DatabaseAbstractionLayer dal;
      return dal.executeQuery(sql);
    } catch (const std::exception &e) {
      logger_->Error("Query failed: " + std::string(e.what()));
      markError();
      return {};
    }
  }

  /**
   * @brief 통합 비쿼리 실행 - DbLib Abstraction Layer 사용
   */
  bool executeUnifiedNonQuery(const std::string &sql) {
    try {
      DbLib::DatabaseAbstractionLayer dal;
      return dal.executeNonQuery(sql);
    } catch (const std::exception &e) {
      logger_->Error("NonQuery failed: " + std::string(e.what()));
      markError();
      return false;
    }
  }

  virtual std::string getTableName() const = 0;

  std::string escapeString(const std::string &str) const {
    return DbLib::DatabaseManager::getInstance().isConnected(
               DbLib::DatabaseManager::DatabaseType::SQLITE)
               ? "''"
               : "''"; // Placeholder - better to use dal.escapeString if
                       // possible
  }

  std::string timestampToString(
      const std::chrono::system_clock::time_point &timestamp) const {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream ss;
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t);
#else
    localtime_r(&time_t, &tm_buf);
#endif
    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return ss.str();
  }

protected:
  // =======================================================================
  // 멤버 변수
  // =======================================================================

  DbLib::DatabaseManager *db_manager_;
  ConfigManager *config_manager_;
  LogManager *logger_;
};

} // namespace Database
} // namespace PulseOne

#endif // PULSEONE_BASE_ENTITY_H