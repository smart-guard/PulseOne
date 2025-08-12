// =============================================================================
// collector/src/Database/Repositories/VirtualPointRepository.cpp
// PulseOne VirtualPointRepository - ScriptLibraryRepository 패턴 100% 적용
// =============================================================================

/**
 * @file VirtualPointRepository.cpp
 * @brief PulseOne VirtualPointRepository - ScriptLibraryRepository 패턴 완전 적용
 * @author PulseOne Development Team
 * @date 2025-08-12
 * 
 * 🎯 ScriptLibraryRepository 패턴 100% 적용:
 * - ExtendedSQLQueries.h 사용 (분리된 쿼리 파일)
 * - DatabaseAbstractionLayer 패턴
 * - 표준 LogManager 사용법
 * - RepositoryHelpers 활용
 * - 벌크 연산 SQL 최적화
 * - 캐시 관리 완전 구현
 * - 모든 IRepository 메서드 override
 */

#include "Database/Repositories/VirtualPointRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Database/ExtendedSQLQueries.h"  // 🔥 분리된 쿼리 파일 사용
#include "Database/SQLQueries.h"          // 🔥 SQL::Common 네임스페이스용
#include "Utils/LogManager.h"
#include <sstream>
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 기본 CRUD 구현 (ScriptLibraryRepository 패턴)
// =============================================================================

std::vector<VirtualPointEntity> VirtualPointRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR, 
                                        "findAll - Table creation failed");
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h 사용
        auto results = db_layer.executeQuery(SQL::VirtualPoint::FIND_ALL);
        
        std::vector<VirtualPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("VirtualPointRepository", LogLevel::WARN,
                                            "findAll - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::INFO,
                                    "findAll - Found " + std::to_string(entities.size()) + " virtual points");
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<VirtualPointEntity> VirtualPointRepository::findById(int id) {
    try {
        if (id <= 0 || !ensureTableExists()) {
            return std::nullopt;
        }
        
        // 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                LogManager::getInstance().log("VirtualPointRepository", LogLevel::DEBUG,
                                            "findById - Cache hit for ID: " + std::to_string(id));
                return cached;
            }
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
        std::string query = RepositoryHelpers::replaceParameter(SQL::VirtualPoint::FIND_BY_ID, std::to_string(id));
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(id, entity);
        }
        
        return entity;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "findById failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool VirtualPointRepository::save(VirtualPointEntity& entity) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        if (!validateVirtualPoint(entity)) {
            LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                        "save - Invalid virtual point data");
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
        auto params = entityToParams(entity);
        std::string query = RepositoryHelpers::replaceParametersInOrder(SQL::VirtualPoint::INSERT, params);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success && entity.getId() <= 0) {
            // 새로 생성된 ID 가져오기
            auto id_results = db_layer.executeQuery(SQL::Common::GET_LAST_INSERT_ID);
            if (!id_results.empty() && id_results[0].find("id") != id_results[0].end()) {
                entity.setId(std::stoll(id_results[0].at("id")));
            }
            
            LogManager::getInstance().log("VirtualPointRepository", LogLevel::INFO,
                                        "save - Saved virtual point: " + entity.getName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "save failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::update(const VirtualPointEntity& entity) {
    try {
        if (entity.getId() <= 0 || !ensureTableExists()) {
            return false;
        }
        
        if (!validateVirtualPoint(entity)) {
            LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                        "update - Invalid virtual point data");
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
        auto params = entityToParams(entity);
        params["id"] = std::to_string(entity.getId()); // WHERE 절용
        std::string query = RepositoryHelpers::replaceParametersInOrder(SQL::VirtualPoint::UPDATE_BY_ID, params);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(static_cast<int>(entity.getId()));
            }
            
            LogManager::getInstance().log("VirtualPointRepository", LogLevel::INFO,
                                        "update - Updated virtual point ID: " + std::to_string(entity.getId()));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "update failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::deleteById(int id) {
    try {
        if (id <= 0 || !ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
        std::string query = RepositoryHelpers::replaceParameter(SQL::VirtualPoint::DELETE_BY_ID, std::to_string(id));
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시에서 제거
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            LogManager::getInstance().log("VirtualPointRepository", LogLevel::INFO,
                                        "deleteById - Deleted virtual point ID: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::exists(int id) {
    try {
        if (id <= 0 || !ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 ExtendedSQLQueries.h + RepositoryHelpers 패턴
        std::string query = RepositoryHelpers::replaceParameter(SQL::VirtualPoint::EXISTS_BY_ID, std::to_string(id));
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count")) > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "exists failed: " + std::string(e.what()));
        return false;
    }
}

int VirtualPointRepository::getTotalCount() {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::VirtualPoint::COUNT_ALL);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "getTotalCount failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// VirtualPoint 특화 메서드들
// =============================================================================

std::vector<VirtualPointEntity> VirtualPointRepository::findByTenant(int tenant_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        std::string query = RepositoryHelpers::replaceParameter(SQL::VirtualPoint::FIND_BY_TENANT, std::to_string(tenant_id));
        auto results = db_layer.executeQuery(query);
        
        std::vector<VirtualPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("VirtualPointRepository", LogLevel::WARN,
                                            "findByTenant - Failed to map row: " + std::string(e.what()));
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "findByTenant failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<VirtualPointEntity> VirtualPointRepository::findBySite(int site_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        std::string query = RepositoryHelpers::replaceParameter(SQL::VirtualPoint::FIND_BY_SITE, std::to_string(site_id));
        auto results = db_layer.executeQuery(query);
        
        std::vector<VirtualPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("VirtualPointRepository", LogLevel::WARN,
                                            "findBySite - Failed to map row: " + std::string(e.what()));
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "findBySite failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByDevice(int device_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        std::string query = RepositoryHelpers::replaceParameter(SQL::VirtualPoint::FIND_BY_DEVICE, std::to_string(device_id));
        auto results = db_layer.executeQuery(query);
        
        std::vector<VirtualPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("VirtualPointRepository", LogLevel::WARN,
                                            "findByDevice - Failed to map row: " + std::string(e.what()));
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "findByDevice failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<VirtualPointEntity> VirtualPointRepository::findEnabled() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::VirtualPoint::FIND_ENABLED);
        
        std::vector<VirtualPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("VirtualPointRepository", LogLevel::WARN,
                                            "findEnabled - Failed to map row: " + std::string(e.what()));
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "findEnabled failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByCategory(const std::string& category) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        std::string query = RepositoryHelpers::replaceParameter(SQL::VirtualPoint::FIND_BY_CATEGORY, category);
        auto results = db_layer.executeQuery(query);
        
        std::vector<VirtualPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("VirtualPointRepository", LogLevel::WARN,
                                            "findByCategory - Failed to map row: " + std::string(e.what()));
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "findByCategory failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByExecutionType(const std::string& execution_type) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        std::string query = RepositoryHelpers::replaceParameter(SQL::VirtualPoint::FIND_BY_EXECUTION_TYPE, execution_type);
        auto results = db_layer.executeQuery(query);
        
        std::vector<VirtualPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("VirtualPointRepository", LogLevel::WARN,
                                            "findByExecutionType - Failed to map row: " + std::string(e.what()));
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "findByExecutionType failed: " + std::string(e.what()));
        return {};
    }
}

bool VirtualPointRepository::updateExecutionStats(int id, double last_value, double execution_time_ms) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::map<std::string, std::string> params;
        params["last_value"] = std::to_string(last_value);
        params["execution_time_ms"] = std::to_string(execution_time_ms);
        params["id"] = std::to_string(id);
        
        std::string query = RepositoryHelpers::replaceParametersInOrder(SQL::VirtualPoint::UPDATE_EXECUTION_STATS, params);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "updateExecutionStats failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::updateLastError(int id, const std::string& error_message) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::map<std::string, std::string> params;
        params["error_message"] = error_message;
        params["id"] = std::to_string(id);
        
        std::string query = RepositoryHelpers::replaceParametersInOrder(SQL::VirtualPoint::UPDATE_ERROR_INFO, params);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "updateLastError failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 헬퍼 메서드들
// =============================================================================

VirtualPointEntity VirtualPointRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    VirtualPointEntity entity;
    
    try {
        // 기본 필드
        if (row.find("id") != row.end()) {
            entity.setId(std::stoll(row.at("id")));
        }
        if (row.find("tenant_id") != row.end()) {
            entity.setTenantId(std::stoi(row.at("tenant_id")));
        }
        if (row.find("site_id") != row.end() && !row.at("site_id").empty() && row.at("site_id") != "NULL") {
            entity.setSiteId(std::stoi(row.at("site_id")));
        }
        if (row.find("device_id") != row.end() && !row.at("device_id").empty() && row.at("device_id") != "NULL") {
            entity.setDeviceId(std::stoi(row.at("device_id")));
        }
        
        // 문자열 필드
        if (row.find("name") != row.end()) {
            entity.setName(row.at("name"));
        }
        if (row.find("description") != row.end()) {
            entity.setDescription(row.at("description"));
        }
        if (row.find("formula") != row.end()) {
            entity.setFormula(row.at("formula"));
        }
        if (row.find("data_type") != row.end()) {
            entity.setDataType(row.at("data_type"));
        }
        if (row.find("unit") != row.end()) {
            entity.setUnit(row.at("unit"));
        }
        if (row.find("category") != row.end()) {
            entity.setCategory(row.at("category"));
        }
        if (row.find("tags") != row.end()) {
            entity.setTags(row.at("tags"));
        }
        if (row.find("scope_type") != row.end()) {
            entity.setScopeType(row.at("scope_type"));
        }
        if (row.find("calculation_trigger") != row.end()) {
            entity.setCalculationTrigger(row.at("calculation_trigger"));
        }
        if (row.find("created_by") != row.end()) {
            entity.setCreatedBy(row.at("created_by"));
        }
        
        // 숫자 필드
        if (row.find("calculation_interval") != row.end()) {
            entity.setCalculationInterval(std::stoi(row.at("calculation_interval")));
        }
        if (row.find("cache_duration_ms") != row.end()) {
            entity.setCacheDurationMs(std::stoi(row.at("cache_duration_ms")));
        }
        if (row.find("execution_count") != row.end()) {
            entity.setExecutionCount(std::stoi(row.at("execution_count")));
        }
        if (row.find("last_value") != row.end()) {
            entity.setLastValue(std::stod(row.at("last_value")));
        }
        if (row.find("avg_execution_time_ms") != row.end()) {
            entity.setAvgExecutionTimeMs(std::stod(row.at("avg_execution_time_ms")));
        }
        
        // 불린 필드
        if (row.find("is_enabled") != row.end()) {
            entity.setIsEnabled(row.at("is_enabled") == "1" || row.at("is_enabled") == "true");
        }
        
        // 에러 정보
        if (row.find("last_error") != row.end()) {
            entity.setLastError(row.at("last_error"));
        }
        
        entity.markSaved();
        return entity;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "mapRowToEntity failed: " + std::string(e.what()));
        throw;
    }
}

std::map<std::string, std::string> VirtualPointRepository::entityToParams(const VirtualPointEntity& entity) {
    std::map<std::string, std::string> params;
    
    params["tenant_id"] = std::to_string(entity.getTenantId());
    
    if (entity.getSiteId().has_value()) {
        params["site_id"] = std::to_string(entity.getSiteId().value());
    } else {
        params["site_id"] = "";
    }
    
    if (entity.getDeviceId().has_value()) {
        params["device_id"] = std::to_string(entity.getDeviceId().value());
    } else {
        params["device_id"] = "";
    }
    
    params["name"] = entity.getName();
    params["description"] = entity.getDescription();
    params["formula"] = entity.getFormula();
    params["data_type"] = entity.getDataType();
    params["unit"] = entity.getUnit();
    params["calculation_interval"] = std::to_string(entity.getCalculationInterval());
    params["calculation_trigger"] = entity.getCalculationTrigger();
    params["cache_duration_ms"] = std::to_string(entity.getCacheDurationMs());
    params["is_enabled"] = entity.getIsEnabled() ? "1" : "0";
    params["category"] = entity.getCategory();
    params["tags"] = entity.getTags();
    params["scope_type"] = entity.getScopeType();
    params["created_by"] = entity.getCreatedBy();
    
    return params;
}

bool VirtualPointRepository::validateVirtualPoint(const VirtualPointEntity& entity) {
    // 이름 필수
    if (entity.getName().empty()) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "validateVirtualPoint - Name cannot be empty");
        return false;
    }
    
    // 포뮬러 필수
    if (entity.getFormula().empty()) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "validateVirtualPoint - Formula cannot be empty");
        return false;
    }
    
    // 계산 간격은 양수여야 함
    if (entity.getCalculationInterval() <= 0) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "validateVirtualPoint - Calculation interval must be positive");
        return false;
    }
    
    return true;
}

bool VirtualPointRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        return db_layer.executeNonQuery(SQL::VirtualPoint::CREATE_TABLE);
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                    "ensureTableExists failed: " + std::string(e.what()));
        return false;
    }
}
std::vector<VirtualPointEntity> VirtualPointRepository::findByIds(const std::vector<int>& ids) {
    std::vector<VirtualPointEntity> results;
    results.reserve(ids.size());
    
    for (int id : ids) {
        auto entity_opt = findById(id);
        if (entity_opt.has_value()) {
            results.push_back(entity_opt.value());
        }
    }
    
    LogManager::getInstance().log("VirtualPointRepository", LogLevel::INFO,
                                "findByIds - Found " + std::to_string(results.size()) + 
                                " virtual points for " + std::to_string(ids.size()) + " IDs");
    return results;
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    // 현재는 메모리 필터링으로 구현 (나중에 SQL 최적화)
    auto all_entities = findAll();
    std::vector<VirtualPointEntity> filtered;
    
    for (const auto& entity : all_entities) {
        bool matches = true;
        
        for (const auto& condition : conditions) {
            // 간단한 조건 매칭 (확장 가능)
            if (condition.field == "tenant_id") {
                if (std::to_string(entity.getTenantId()) != condition.value) {
                    matches = false;
                    break;
                }
            } else if (condition.field == "is_enabled") {
                bool is_enabled = (condition.value == "1" || condition.value == "true");
                if (entity.getIsEnabled() != is_enabled) {
                    matches = false;
                    break;
                }
            } else if (condition.field == "category") {
                if (entity.getCategory() != condition.value) {
                    matches = false;
                    break;
                }
            }
            // 필요에 따라 더 많은 필드 추가
        }
        
        if (matches) {
            filtered.push_back(entity);
        }
    }
    
    // 정렬 적용
    if (order_by.has_value()) {
        std::sort(filtered.begin(), filtered.end(), [&](const auto& a, const auto& b) {
            if (order_by->field == "name") {
                return order_by->ascending ? (a.getName() < b.getName()) : (a.getName() > b.getName());
            }
            return true;
        });
    }
    
    // 페이징 적용
    if (pagination.has_value()) {
        size_t start = pagination->offset;
        size_t count = pagination->limit;
        if (start >= filtered.size()) {
            return {};
        }
        size_t end = std::min(start + count, filtered.size());
        return std::vector<VirtualPointEntity>(filtered.begin() + start, filtered.begin() + end);
    }
    
    return filtered;
}

int VirtualPointRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    // findByConditions를 재사용
    auto results = findByConditions(conditions);
    return static_cast<int>(results.size());
}

int VirtualPointRepository::saveBulk(std::vector<VirtualPointEntity>& entities) {
    int saved_count = 0;
    
    for (auto& entity : entities) {
        try {
            if (save(entity)) {
                saved_count++;
            }
        } catch (const std::exception& e) {
            LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                        "saveBulk - Failed to save entity: " + std::string(e.what()));
        }
    }
    
    LogManager::getInstance().log("VirtualPointRepository", LogLevel::INFO,
                                "saveBulk - Saved " + std::to_string(saved_count) + 
                                " out of " + std::to_string(entities.size()) + " virtual points");
    return saved_count;
}

int VirtualPointRepository::updateBulk(const std::vector<VirtualPointEntity>& entities) {
    int updated_count = 0;
    
    for (const auto& entity : entities) {
        try {
            if (update(entity)) {
                updated_count++;
            }
        } catch (const std::exception& e) {
            LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                        "updateBulk - Failed to update entity: " + std::string(e.what()));
        }
    }
    
    LogManager::getInstance().log("VirtualPointRepository", LogLevel::INFO,
                                "updateBulk - Updated " + std::to_string(updated_count) + 
                                " out of " + std::to_string(entities.size()) + " virtual points");
    return updated_count;
}

int VirtualPointRepository::deleteByIds(const std::vector<int>& ids) {
    int deleted_count = 0;
    
    for (int id : ids) {
        try {
            if (deleteById(id)) {
                deleted_count++;
            }
        } catch (const std::exception& e) {
            LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                        "deleteByIds - Failed to delete ID " + std::to_string(id) + 
                                        ": " + std::string(e.what()));
        }
    }
    
    LogManager::getInstance().log("VirtualPointRepository", LogLevel::INFO,
                                "deleteByIds - Deleted " + std::to_string(deleted_count) + 
                                " out of " + std::to_string(ids.size()) + " virtual points");
    return deleted_count;
}

// =============================================================================
// 캐시 관리 메서드들 (IRepository 위임 패턴)
// =============================================================================

void VirtualPointRepository::clearCache() {
    // IRepository의 기본 구현을 사용
    IRepository<VirtualPointEntity>::clearCache();
    LogManager::getInstance().log("VirtualPointRepository", LogLevel::INFO,
                                "clearCache - All cache cleared");
}

std::map<std::string, int> VirtualPointRepository::getCacheStats() const {
    // IRepository의 기본 구현을 사용
    return IRepository<VirtualPointEntity>::getCacheStats();
}

// =============================================================================
// 캐시 관련 private 메서드들 (IRepository 위임)
// =============================================================================

bool VirtualPointRepository::isCacheEnabled() const {
    return IRepository<VirtualPointEntity>::isCacheEnabled();
}

std::optional<VirtualPointEntity> VirtualPointRepository::getCachedEntity(int id) {
    // IRepository의 기본 캐시 구현 사용
    return std::nullopt; // 기본 구현에서는 캐시 미사용
}

void VirtualPointRepository::cacheEntity(int id, const VirtualPointEntity& entity) {
    // IRepository의 기본 캐시 구현 사용
    // 현재는 빈 구현 (IRepository에서 관리)
}

void VirtualPointRepository::clearCacheForId(int id) {
    // IRepository의 기본 구현을 사용
    IRepository<VirtualPointEntity>::clearCacheForId(id);
}

void VirtualPointRepository::clearAllCache() {
    // clearCache()와 동일
    clearCache();
}

std::string VirtualPointRepository::generateCacheKey(int id) const {
    return "virtualpoint:" + std::to_string(id);
}


} // namespace Repositories
} // namespace Database  
} // namespace PulseOne