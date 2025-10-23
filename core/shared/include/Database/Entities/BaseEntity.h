#ifndef PULSEONE_BASE_ENTITY_H
#define PULSEONE_BASE_ENTITY_H

/**
 * @file BaseEntity.h
 * @brief PulseOne 기본 엔티티 템플릿 - 크로스플랫폼 버전
 * @author PulseOne Development Team
 * @date 2025-07-31
 */
#include "Platform/PlatformCompat.h"
#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include "Common/BasicTypes.h"
#include "Common/Enums.h"
#include "Common/Structs.h"
#include "Common/DriverStatistics.h"
#include <string>
#include <optional>
#include <chrono>
#include <map>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace PulseOne {
namespace Database {

namespace Repositories {
    class DeviceRepository;
    class DeviceSettingsRepository;
    class DataPointRepository;
    class CurrentValueRepository;
    class VirtualPointRepository;
    class SiteRepository;
    class TenantRepository;
    class UserRepository;
    class AlarmConfigRepository;
    class ExportLogRepository;
    class ExportTargetRepository;
    class ExportTargetMappingRepository;
    class ExportScheduleRepository;
    class PayloadTemplateRepository;
}

/**
 * @brief 엔티티 상태 열거형
 */
enum class EntityState {
    NEW,
    LOADED,
    MODIFIED,
    DELETED,
    ENTITY_ERROR
};

/**
 * @brief 모든 엔티티의 기본 클래스 (템플릿)
 */
template<typename DerivedType>
class BaseEntity {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    BaseEntity() 
        : id_(0)
        , state_(EntityState::NEW)
        , created_at_(std::chrono::system_clock::now())
        , updated_at_(std::chrono::system_clock::now())
        , db_manager_(&DatabaseManager::getInstance())
        , config_manager_(&ConfigManager::getInstance())
        , logger_(&LogManager::getInstance()) {}
    
    explicit BaseEntity(int id) : BaseEntity() {
        id_ = id;
        if (id > 0) {
            state_ = EntityState::LOADED;
        }
    }
    
    virtual ~BaseEntity() = default;

    // =======================================================================
    // 복사/이동 생성자 및 할당 연산자
    // =======================================================================
    
    BaseEntity(const BaseEntity& other) 
        : id_(other.id_)
        , state_(other.state_)
        , created_at_(other.created_at_)
        , updated_at_(other.updated_at_)
        , db_manager_(&DatabaseManager::getInstance())
        , config_manager_(&ConfigManager::getInstance())
        , logger_(&LogManager::getInstance()) {}
    
    BaseEntity(BaseEntity&& other) noexcept
        : id_(other.id_)
        , state_(other.state_)
        , created_at_(other.created_at_)
        , updated_at_(other.updated_at_)
        , db_manager_(&DatabaseManager::getInstance())
        , config_manager_(&ConfigManager::getInstance())
        , logger_(&LogManager::getInstance()) {}

    BaseEntity& operator=(const BaseEntity& other) {
        if (this != &other) {
            id_ = other.id_;
            state_ = other.state_;
            created_at_ = other.created_at_;
            updated_at_ = other.updated_at_;
        }
        return *this;
    }
    
    BaseEntity& operator=(BaseEntity&& other) noexcept {
        if (this != &other) {
            id_ = other.id_;
            state_ = other.state_;
            created_at_ = other.created_at_;
            updated_at_ = other.updated_at_;
        }
        return *this;
    }

    // =======================================================================
    // Repository 패턴 지원
    // =======================================================================

protected:
    template<typename EntityType = DerivedType>
    auto getRepository() -> decltype(getRepositoryImpl(static_cast<EntityType*>(nullptr))) {
        return getRepositoryImpl(static_cast<EntityType*>(nullptr));
    }

    bool saveViaRepository() {
        try {
            auto repo = getRepository();
            if (repo) {
                DerivedType& derived = static_cast<DerivedType&>(*this);
                bool success = repo->save(derived);
                if (success) {
                    markSaved();
                    logger_->Info("Saved " + getEntityTypeName());
                }
                return success;
            }
            return false;
        } catch (const std::exception& e) {
            logger_->Error("Save failed: " + std::string(e.what()));
            markError();
            return false;
        }
    }

    bool updateViaRepository() {
        try {
            auto repo = getRepository();
            if (repo) {
                const DerivedType& derived = static_cast<const DerivedType&>(*this);
                bool success = repo->update(derived);
                if (success) {
                    markSaved();
                    logger_->Info("Updated " + getEntityTypeName());
                }
                return success;
            }
            return false;
        } catch (const std::exception& e) {
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
                    logger_->Info("Deleted " + getEntityTypeName());
                }
                return success;
            }
            return false;
        } catch (const std::exception& e) {
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
                    DerivedType& derived = static_cast<DerivedType&>(*this);
                    derived = loaded.value();
                    markSaved();
                    logger_->Info("Loaded " + getEntityTypeName());
                    return true;
                }
            }
            return false;
        } catch (const std::exception& e) {
            logger_->Error("Load failed: " + std::string(e.what()));
            markError();
            return false;
        }
    }

private:
    // Repository 타입별 특화
    std::shared_ptr<Repositories::DeviceRepository> getRepositoryImpl(class DeviceEntity*);
    std::shared_ptr<Repositories::DeviceSettingsRepository> getRepositoryImpl(class DeviceSettingsEntity*);
    std::shared_ptr<Repositories::DataPointRepository> getRepositoryImpl(class DataPointEntity*);
    std::shared_ptr<Repositories::CurrentValueRepository> getRepositoryImpl(class CurrentValueEntity*);
    std::shared_ptr<Repositories::VirtualPointRepository> getRepositoryImpl(class VirtualPointEntity*);
    std::shared_ptr<Repositories::SiteRepository> getRepositoryImpl(class SiteEntity*);
    std::shared_ptr<Repositories::TenantRepository> getRepositoryImpl(class TenantEntity*);
    std::shared_ptr<Repositories::UserRepository> getRepositoryImpl(class UserEntity*);
    std::shared_ptr<Repositories::AlarmConfigRepository> getRepositoryImpl(class AlarmConfigEntity*);
    std::shared_ptr<Repositories::ExportLogRepository> getRepositoryImpl(class ExportLogEntity*);
    std::shared_ptr<Repositories::ExportTargetRepository> getRepositoryImpl(class ExportTargetEntity*);
    std::shared_ptr<Repositories::ExportTargetMappingRepository> getRepositoryImpl(class ExportTargetMappingEntity*);
    std::shared_ptr<Repositories::ExportScheduleRepository> getRepositoryImpl(class ExportScheduleEntity*);
    std::shared_ptr<Repositories::PayloadTemplateRepository> getRepositoryImpl(class PayloadTemplateEntity*);

    int getEntityId() const {
        if constexpr (std::is_same_v<DerivedType, class DeviceSettingsEntity>) {
            return static_cast<const DerivedType&>(*this).getDeviceId();
        } else {
            return id_;
        }
    }

    virtual std::string getEntityTypeName() const {
        if constexpr (std::is_same_v<DerivedType, class DeviceEntity>) {
            return "DeviceEntity";
        } else if constexpr (std::is_same_v<DerivedType, class DeviceSettingsEntity>) {
            return "DeviceSettingsEntity";
        } else if constexpr (std::is_same_v<DerivedType, class DataPointEntity>) {
            return "DataPointEntity";
        } else if constexpr (std::is_same_v<DerivedType, class CurrentValueEntity>) {
            return "CurrentValueEntity";
        } else if constexpr (std::is_same_v<DerivedType, class VirtualPointEntity>) {
            return "VirtualPointEntity";
        } else if constexpr (std::is_same_v<DerivedType, class SiteEntity>) {
            return "SiteEntity";
        } else if constexpr (std::is_same_v<DerivedType, class TenantEntity>) {
            return "TenantEntity";
        } else if constexpr (std::is_same_v<DerivedType, class UserEntity>) {
            return "UserEntity";
        } else if constexpr (std::is_same_v<DerivedType, class AlarmConfigEntity>) {
            return "AlarmConfigEntity";
        } else if constexpr (std::is_same_v<DerivedType, class ExportLogEntity>) {
            return "ExportLogEntity";
        } else if constexpr (std::is_same_v<DerivedType, class ExportTargetEntity>) {
            return "ExportTargetEntity";
        } else if constexpr (std::is_same_v<DerivedType, class ExportTargetMappingEntity>) {
            return "ExportTargetMappingEntity";
        } else if constexpr (std::is_same_v<DerivedType, class ExportScheduleEntity>) {
            return "ExportScheduleEntity";
        } else if constexpr (std::is_same_v<DerivedType, class PayloadTemplateEntity>) {
            return "PayloadTemplateEntity";
        }else {
            return "UnknownEntity";
        }
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
    virtual bool fromJson(const json& data) = 0;
    virtual std::string toString() const = 0;

    // =======================================================================
    // 접근자
    // =======================================================================
    
    int getId() const { return id_; }
    void setId(int id) { 
        id_ = id; 
        markModified();
    }
    
    EntityState getState() const { return state_; }
    std::chrono::system_clock::time_point getCreatedAt() const { return created_at_; }
    std::chrono::system_clock::time_point getUpdatedAt() const { return updated_at_; }

    // =======================================================================
    // 상태 관리
    // =======================================================================
    
    bool isLoadedFromDb() const { return state_ == EntityState::LOADED; }
    bool isNew() const { return state_ == EntityState::NEW; }
    bool isModified() const { return state_ == EntityState::MODIFIED; }
    bool isDeleted() const { return state_ == EntityState::DELETED; }
    bool isError() const { return state_ == EntityState::ENTITY_ERROR; }
    
    virtual bool isValid() const {
        return state_ != EntityState::ENTITY_ERROR && id_ >= 0;
    }

    void markModified() {
        if (state_ == EntityState::LOADED) {
            state_ = EntityState::MODIFIED;
            updated_at_ = std::chrono::system_clock::now();
        }
    }
    
    void markDeleted() {
        state_ = EntityState::DELETED;
        updated_at_ = std::chrono::system_clock::now();
    }
    
    void markError() {
        state_ = EntityState::ENTITY_ERROR;
        updated_at_ = std::chrono::system_clock::now();
    }
    
    void markSaved() {
        state_ = EntityState::LOADED;
        updated_at_ = std::chrono::system_clock::now();
    }

protected:
    // =======================================================================
    // 크로스플랫폼 DB 쿼리 실행
    // =======================================================================
    
    /**
     * @brief 통합 쿼리 실행 - 조건부 컴파일로 DB 타입별 처리
     */
    std::vector<std::map<std::string, std::string>> executeUnifiedQuery(const std::string& sql) {
        try {
            std::string db_type = config_manager_->getOrDefault("DATABASE_TYPE", "SQLITE");
            
#ifdef HAS_POSTGRESQL
            if (db_type == "POSTGRESQL" || db_type == "POSTGRES") {
                return executePostgresQuery(sql);
            }
#endif

#ifdef HAS_MYSQL  
            if (db_type == "MYSQL" || db_type == "MARIADB") {
                return executeMySQLQuery(sql);
            }
#endif

            // 기본값: SQLite (항상 지원)
            return executeSQLiteQuery(sql);
            
        } catch (const std::exception& e) {
            logger_->Error("Query failed: " + std::string(e.what()));
            markError();
            return {};
        }
    }
    
    /**
     * @brief 통합 비쿼리 실행 - 조건부 컴파일로 DB 타입별 처리
     */
    bool executeUnifiedNonQuery(const std::string& sql) {
        try {
            std::string db_type = config_manager_->getOrDefault("DATABASE_TYPE", "SQLITE");
            
#ifdef HAS_POSTGRESQL
            if (db_type == "POSTGRESQL" || db_type == "POSTGRES") {
                return db_manager_->executeNonQueryPostgres(sql);
            }
#endif

#ifdef HAS_MYSQL
            if (db_type == "MYSQL" || db_type == "MARIADB") {
                std::vector<std::vector<std::string>> dummy;
                return db_manager_->executeQueryMySQL(sql, dummy);
            }
#endif

            // 기본값: SQLite (항상 지원)
            return db_manager_->executeNonQuerySQLite(sql);
            
        } catch (const std::exception& e) {
            logger_->Error("NonQuery failed: " + std::string(e.what()));
            markError();
            return false;
        }
    }
    
    virtual std::string getTableName() const = 0;
    
    std::string escapeString(const std::string& str) const {
        std::string escaped = str;
        size_t pos = 0;
        while ((pos = escaped.find("'", pos)) != std::string::npos) {
            escaped.replace(pos, 1, "''");
            pos += 2;
        }
        return escaped;
    }
    
    std::string timestampToString(const std::chrono::system_clock::time_point& timestamp) const {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        std::stringstream ss;
        std::tm tm_buf{};
#ifdef _WIN32
        gmtime_s(&tm_buf, &time_t);
#else
        gmtime_r(&time_t, &tm_buf);
#endif
        ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

private:
    // =======================================================================
    // PostgreSQL 쿼리 (조건부)
    // =======================================================================
#ifdef HAS_POSTGRESQL
    std::vector<std::map<std::string, std::string>> executePostgresQuery(const std::string& sql) {
        std::vector<std::map<std::string, std::string>> results;
        try {
            auto pg_result = db_manager_->executeQueryPostgres(sql);
            for (const auto& row : pg_result) {
                std::map<std::string, std::string> row_map;
                for (size_t i = 0; i < row.size(); ++i) {
                    std::string column_name = pg_result.column_name(i);
                    std::string value = row[static_cast<int>(i)].is_null() ? 
                        "" : row[static_cast<int>(i)].as<std::string>();
                    row_map[column_name] = value;
                }
                results.push_back(row_map);
            }
        } catch (const std::exception& e) {
            logger_->Error("PostgreSQL query failed: " + std::string(e.what()));
        }
        return results;
    }
#endif

    // =======================================================================
    // MySQL 쿼리 (조건부)
    // =======================================================================
#ifdef HAS_MYSQL
    std::vector<std::map<std::string, std::string>> executeMySQLQuery(const std::string& sql) {
        std::vector<std::map<std::string, std::string>> results;
        std::vector<std::vector<std::string>> raw_results;
        
        if (db_manager_->executeQueryMySQL(sql, raw_results)) {
            // MySQL은 컬럼명이 없으므로 인덱스 기반으로 처리
            for (const auto& row : raw_results) {
                std::map<std::string, std::string> row_map;
                for (size_t i = 0; i < row.size(); ++i) {
                    row_map["col_" + std::to_string(i)] = row[i];
                }
                results.push_back(row_map);
            }
        }
        return results;
    }
#endif
    
    // =======================================================================
    // SQLite 쿼리 (항상 포함)
    // =======================================================================
    std::vector<std::map<std::string, std::string>> executeSQLiteQuery(const std::string& sql) {
        std::vector<std::map<std::string, std::string>> results;
        
        auto callback = [](void* data, int argc, char** argv, char** col_names) -> int {
            auto* results_ptr = static_cast<std::vector<std::map<std::string, std::string>>*>(data);
            std::map<std::string, std::string> row;
            
            for (int i = 0; i < argc; i++) {
                std::string value = argv[i] ? argv[i] : "";
                row[col_names[i]] = value;
            }
            
            results_ptr->push_back(row);
            return 0;
        };
        
        bool success = db_manager_->executeQuerySQLite(sql, callback, &results);
        if (!success) {
            logger_->Error("SQLite query failed: " + sql);
            results.clear();
        }
        
        return results;
    }

protected:
    // =======================================================================
    // 멤버 변수
    // =======================================================================
    
    int id_;
    EntityState state_;
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point updated_at_;
    
    DatabaseManager* db_manager_;
    ConfigManager* config_manager_;
    LogManager* logger_;
};

} // namespace Database
} // namespace PulseOne

#endif // PULSEONE_BASE_ENTITY_H