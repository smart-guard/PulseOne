/**
 * @file PayloadTemplateRepository.cpp
 * @brief Payload Template Repository 구현
 * @version 1.0.0
 * 저장 위치: core/shared/src/Database/Repositories/PayloadTemplateRepository.cpp
 */

#include "Database/Repositories/PayloadTemplateRepository.h"
#include "Database/ExportSQLQueries.h"
#include "Database/DatabaseAbstractionLayer.h"

namespace PulseOne {
namespace Database {
namespace Repositories {

using namespace SQL::PayloadTemplate;

// =============================================================================
// IRepository 인터페이스 구현
// =============================================================================

std::vector<PayloadTemplateEntity> PayloadTemplateRepository::findAll() {
    try {
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(FIND_ALL);
        
        std::vector<PayloadTemplateEntity> entities;
        for (const auto& row : results) {
            entities.push_back(mapRowToEntity(row));
        }
        
        if (logger_) {
            logger_->Debug("Found " + std::to_string(entities.size()) + " payload templates");
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("PayloadTemplateRepository::findAll failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::optional<PayloadTemplateEntity> PayloadTemplateRepository::findById(int id) {
    try {
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(FIND_BY_ID, {std::to_string(id)});
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        return mapRowToEntity(results[0]);
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("PayloadTemplateRepository::findById failed: " + std::string(e.what()));
        }
        return std::nullopt;
    }
}

bool PayloadTemplateRepository::save(PayloadTemplateEntity& entity) {
    try {
        DatabaseAbstractionLayer db_layer;
        auto params = entityToParams(entity);
        
        bool success = db_layer.executeNonQuery(INSERT, {
            params["name"],
            params["system_type"],
            params["description"],
            params["template_json"],
            params["is_active"]
        });
        
        if (success) {
            int new_id = db_layer.getLastInsertId();
            entity.setId(new_id);
            
            if (logger_) {
                logger_->Info("Saved PayloadTemplate: " + entity.getName());
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("PayloadTemplateRepository::save failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool PayloadTemplateRepository::update(const PayloadTemplateEntity& entity) {
    try {
        DatabaseAbstractionLayer db_layer;
        auto params = entityToParams(entity);
        
        bool success = db_layer.executeNonQuery(UPDATE, {
            params["name"],
            params["system_type"],
            params["description"],
            params["template_json"],
            params["is_active"],
            params["id"]
        });
        
        if (success && logger_) {
            logger_->Info("Updated PayloadTemplate: " + entity.getName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("PayloadTemplateRepository::update failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool PayloadTemplateRepository::deleteById(int id) {
    try {
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(DELETE_BY_ID, {std::to_string(id)});
        
        if (success && logger_) {
            logger_->Info("Deleted PayloadTemplate ID: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("PayloadTemplateRepository::deleteById failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool PayloadTemplateRepository::exists(int id) {
    try {
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(EXISTS_BY_ID, {std::to_string(id)});
        
        if (!results.empty()) {
            auto it = results[0].find("count");
            if (it != results[0].end()) {
                return std::stoi(it->second) > 0;
            }
        }
        return false;
        
    } catch (const std::exception&) {
        return false;
    }
}

// =============================================================================
// 벌크 연산
// =============================================================================

std::vector<PayloadTemplateEntity> PayloadTemplateRepository::findByIds(const std::vector<int>& ids) {
    std::vector<PayloadTemplateEntity> entities;
    for (int id : ids) {
        auto entity = findById(id);
        if (entity.has_value()) {
            entities.push_back(entity.value());
        }
    }
    return entities;
}

std::vector<PayloadTemplateEntity> PayloadTemplateRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    // TODO: 조건 기반 쿼리 구현
    return findAll();
}

int PayloadTemplateRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(COUNT_ALL);
        
        if (!results.empty()) {
            auto it = results[0].find("count");
            if (it != results[0].end()) {
                return std::stoi(it->second);
            }
        }
        return 0;
    } catch (const std::exception&) {
        return 0;
    }
}

bool PayloadTemplateRepository::bulkSave(std::vector<PayloadTemplateEntity>& entities) {
    bool all_success = true;
    for (auto& entity : entities) {
        if (!save(entity)) {
            all_success = false;
        }
    }
    return all_success;
}

bool PayloadTemplateRepository::bulkUpdate(const std::vector<PayloadTemplateEntity>& entities) {
    bool all_success = true;
    for (const auto& entity : entities) {
        if (!update(entity)) {
            all_success = false;
        }
    }
    return all_success;
}

bool PayloadTemplateRepository::bulkDelete(const std::vector<int>& ids) {
    bool all_success = true;
    for (int id : ids) {
        if (!deleteById(id)) {
            all_success = false;
        }
    }
    return all_success;
}

// =============================================================================
// 커스텀 쿼리
// =============================================================================

std::optional<PayloadTemplateEntity> PayloadTemplateRepository::findByName(const std::string& name) {
    try {
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(FIND_BY_NAME, {name});
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        return mapRowToEntity(results[0]);
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findByName failed: " + std::string(e.what()));
        }
        return std::nullopt;
    }
}

std::vector<PayloadTemplateEntity> PayloadTemplateRepository::findBySystemType(const std::string& system_type) {
    try {
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(FIND_BY_SYSTEM_TYPE, {system_type});
        
        std::vector<PayloadTemplateEntity> entities;
        for (const auto& row : results) {
            entities.push_back(mapRowToEntity(row));
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findBySystemType failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::vector<PayloadTemplateEntity> PayloadTemplateRepository::findActive() {
    try {
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(FIND_ACTIVE);
        
        std::vector<PayloadTemplateEntity> entities;
        for (const auto& row : results) {
            entities.push_back(mapRowToEntity(row));
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findActive failed: " + std::string(e.what()));
        }
        return {};
    }
}

// =============================================================================
// 헬퍼
// =============================================================================

bool PayloadTemplateRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        return db_layer.executeNonQuery(CREATE_TABLE);
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("ensureTableExists failed: " + std::string(e.what()));
        }
        return false;
    }
}

PayloadTemplateEntity PayloadTemplateRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    PayloadTemplateEntity entity;
    
    try {
        auto it = row.find("id");
        if (it != row.end() && !it->second.empty()) {
            entity.setId(std::stoi(it->second));
        }
        
        it = row.find("name");
        if (it != row.end()) {
            entity.setName(it->second);
        }
        
        it = row.find("system_type");
        if (it != row.end()) {
            entity.setSystemType(it->second);
        }
        
        it = row.find("description");
        if (it != row.end()) {
            entity.setDescription(it->second);
        }
        
        it = row.find("template_json");
        if (it != row.end()) {
            entity.setTemplateJson(it->second);
        }
        
        it = row.find("is_active");
        if (it != row.end() && !it->second.empty()) {
            entity.setActive(std::stoi(it->second) != 0);
        }
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to map row to PayloadTemplateEntity: " + std::string(e.what()));
    }
    
    return entity;
}

std::map<std::string, std::string> PayloadTemplateRepository::entityToParams(const PayloadTemplateEntity& entity) {
    std::map<std::string, std::string> params;
    
    params["id"] = std::to_string(entity.getId());
    params["name"] = entity.getName();
    params["system_type"] = entity.getSystemType();
    params["description"] = entity.getDescription();
    params["template_json"] = entity.getTemplateJson();
    params["is_active"] = entity.isActive() ? "1" : "0";
    
    return params;
}

// =============================================================================
// 벌크 연산 (UserRepository/DeviceRepository 패턴)
// =============================================================================

int PayloadTemplateRepository::saveBulk(std::vector<PayloadTemplateEntity>& entities) {
    int saved_count = 0;
    
    for (auto& entity : entities) {
        if (save(entity)) {
            saved_count++;
        }
    }
    
    if (logger_) {
        logger_->Info("PayloadTemplateRepository::saveBulk - Saved " + 
                     std::to_string(saved_count) + " templates");
    }
    
    return saved_count;
}

int PayloadTemplateRepository::updateBulk(const std::vector<PayloadTemplateEntity>& entities) {
    int updated_count = 0;
    
    for (const auto& entity : entities) {
        if (update(entity)) {
            updated_count++;
        }
    }
    
    if (logger_) {
        logger_->Info("PayloadTemplateRepository::updateBulk - Updated " + 
                     std::to_string(updated_count) + " templates");
    }
    
    return updated_count;
}

int PayloadTemplateRepository::deleteByIds(const std::vector<int>& ids) {
    int deleted_count = 0;
    
    for (int id : ids) {
        if (deleteById(id)) {
            deleted_count++;
        }
    }
    
    if (logger_) {
        logger_->Info("PayloadTemplateRepository::deleteByIds - Deleted " + 
                     std::to_string(deleted_count) + " templates");
    }
    
    return deleted_count;
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne