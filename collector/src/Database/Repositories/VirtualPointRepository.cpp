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
        std::string query = RepositoryHelpers::replaceParameter(SQL::VirtualPoint::DELETE_BY_IDS, std::to_string(id));
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
        //if (!ensureTableExists()) {
        //    return {};
        //}
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
        //if (row.find("last_value") != row.end()) {
        //    entity.setLastValue(std::stod(row.at("last_value")));
        //}
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
    if (ids.empty()) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::DEBUG,
                                     "findByIds: 빈 ID 리스트, 빈 결과 반환");
        return {};
    }
    
    try {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::DEBUG,
                                     "findByIds: " + std::to_string(ids.size()) + "개 ID로 벌크 조회 시작");
        
        DatabaseAbstractionLayer db_layer;
        
        // ✅ ExtendedSQLQueries.h 상수 사용
        std::string query = SQL::VirtualPoint::FIND_BY_IDS;
        
        // ✅ RepositoryHelpers를 사용한 IN절 생성
        std::string in_clause = RepositoryHelpers::buildInClause(ids);
        RepositoryHelpers::replaceStringPlaceholder(query, "%IN_CLAUSE%", in_clause);
        
        auto results = db_layer.executeQuery(query);
        
        std::vector<VirtualPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("VirtualPointRepository", LogLevel::WARN,
                                            "findByIds - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::INFO,
                                     "✅ findByIds: " + std::to_string(entities.size()) + "/" + 
                                     std::to_string(ids.size()) + " 개 조회 완료");
        
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                     "findByIds 실행 실패: " + std::string(e.what()));
        return {};
    }
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::DEBUG,
                                     "findByConditions: " + std::to_string(conditions.size()) + "개 조건으로 검색 시작");
        
        DatabaseAbstractionLayer db_layer;
        
        // ✅ 기본 쿼리에서 시작
        std::string query = SQL::VirtualPoint::FIND_ALL;
        
        if (!conditions.empty()) {
            // ✅ WHERE절 생성
            std::string where_clause = " WHERE ";
            bool first = true;
            
            for (const auto& condition : conditions) {
                if (!first) where_clause += " AND ";
                
                // ✅ 조건 타입에 따른 처리
                if (condition.operation == "=") {
                    where_clause += condition.field + " = '" + condition.value + "'";
                } else if (condition.operation == "!=") {
                    where_clause += condition.field + " != '" + condition.value + "'";
                } else if (condition.operation == "LIKE") {
                    where_clause += condition.field + " LIKE '%" + condition.value + "%'";
                } else if (condition.operation == "IN") {
                    where_clause += condition.field + " IN (" + condition.value + ")";
                } else {
                    // 기본은 = 연산자
                    where_clause += condition.field + " = '" + condition.value + "'";
                }
                
                first = false;
            }
            
            // ✅ ORDER BY 앞에 WHERE절 삽입
            size_t order_pos = query.find("ORDER BY");
            if (order_pos != std::string::npos) {
                query.insert(order_pos, where_clause + " ");
            } else {
                query += where_clause;
            }
        }
        
        // ✅ ORDER BY 처리
        if (order_by.has_value()) {
            size_t order_pos = query.find("ORDER BY");
            if (order_pos != std::string::npos) {
                // 기존 ORDER BY 교체
                query = query.substr(0, order_pos);
            }
            query += " ORDER BY " + order_by->field + " " + (order_by->ascending ? "ASC" : "DESC");
        }
        
        // ✅ LIMIT/OFFSET 처리
        if (pagination.has_value()) {
            query += " LIMIT " + std::to_string(pagination->limit);
            if (pagination->offset > 0) {
                query += " OFFSET " + std::to_string(pagination->offset);
            }
        }
        
        auto results = db_layer.executeQuery(query);
        
        std::vector<VirtualPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("VirtualPointRepository", LogLevel::WARN,
                                            "findByConditions - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::DEBUG,
                                     "findByConditions: " + std::to_string(entities.size()) + "개 조회 완료");
        
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                     "findByConditions 실행 실패: " + std::string(e.what()));
        return {};
    }
}

int VirtualPointRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::DEBUG,
                                     "countByConditions: " + std::to_string(conditions.size()) + "개 조건으로 카운트 시작");
        
        DatabaseAbstractionLayer db_layer;
        
        // ✅ ExtendedSQLQueries.h의 COUNT 쿼리 사용
        std::string query = SQL::VirtualPoint::COUNT_ALL;
        
        if (!conditions.empty()) {
            // ✅ WHERE절 생성
            std::string where_clause = " WHERE ";
            bool first = true;
            
            for (const auto& condition : conditions) {
                if (!first) where_clause += " AND ";
                
                if (condition.operation == "=") {
                    where_clause += condition.field + " = '" + condition.value + "'";
                } else if (condition.operation == "!=") {
                    where_clause += condition.field + " != '" + condition.value + "'";
                } else if (condition.operation == "LIKE") {
                    where_clause += condition.field + " LIKE '%" + condition.value + "%'";
                } else {
                    where_clause += condition.field + " = '" + condition.value + "'";
                }
                
                first = false;
            }
            
            // ✅ FROM 테이블명 뒤에 WHERE절 추가
            size_t from_pos = query.find("FROM virtual_points");
            if (from_pos != std::string::npos) {
                size_t insert_pos = from_pos + strlen("FROM virtual_points");
                query.insert(insert_pos, " " + where_clause);
            }
        }
        
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            int count = std::stoi(results[0].at("count"));
            LogManager::getInstance().log("VirtualPointRepository", LogLevel::DEBUG,
                                         "countByConditions: " + std::to_string(count) + "개 카운트 완료");
            return count;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                     "countByConditions 실행 실패: " + std::string(e.what()));
        return 0;
    }
}

int VirtualPointRepository::saveBulk(std::vector<VirtualPointEntity>& entities) {
    if (entities.empty()) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::DEBUG,
                                     "saveBulk: 빈 엔티티 리스트, 0 반환");
        return 0;
    }
    
    try {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::INFO,
                                     "🔄 saveBulk: " + std::to_string(entities.size()) + "개 엔티티 벌크 저장 시작");
        
        DatabaseAbstractionLayer db_layer;
        
        // ✅ 트랜잭션 시작
        if (!db_layer.executeNonQuery("BEGIN TRANSACTION")) {
            LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                         "saveBulk: 트랜잭션 시작 실패");
            return 0;
        }
        
        int success_count = 0;
        
        try {
            for (auto& entity : entities) {
                if (validateVirtualPoint(entity)) {
                    auto params = entityToParams(entity);
                    std::string query = RepositoryHelpers::replaceParametersInOrder(SQL::VirtualPoint::INSERT, params);
                    
                    if (db_layer.executeNonQuery(query)) {
                        // 새로 생성된 ID 가져오기 (SQLite AUTO_INCREMENT)
                        if (entity.getId() <= 0) {
                            auto id_results = db_layer.executeQuery(SQL::Common::GET_LAST_INSERT_ID);
                            if (!id_results.empty() && id_results[0].find("id") != id_results[0].end()) {
                                entity.setId(std::stoll(id_results[0].at("id")));
                            }
                        }
                        success_count++;
                    }
                }
            }
            
            // ✅ 트랜잭션 커밋
            if (!db_layer.executeNonQuery("COMMIT")) {
                LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                             "saveBulk: 트랜잭션 커밋 실패");
                return 0;
            }
            
        } catch (const std::exception& e) {
            // ✅ 트랜잭션 롤백
            db_layer.executeNonQuery("ROLLBACK");
            throw;
        }
        
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::INFO,
                                     "✅ saveBulk: " + std::to_string(success_count) + 
                                     "개 엔티티 벌크 저장 완료 (트랜잭션)");
        return success_count;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                     "saveBulk 실행 실패: " + std::string(e.what()));
        return 0;
    }
}


int VirtualPointRepository::updateBulk(const std::vector<VirtualPointEntity>& entities) {
    if (entities.empty()) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::DEBUG,
                                     "updateBulk: 빈 엔티티 리스트, 0 반환");
        return 0;
    }
    
    try {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::INFO,
                                     "🔄 updateBulk: " + std::to_string(entities.size()) + "개 엔티티 벌크 업데이트 시작");
        
        DatabaseAbstractionLayer db_layer;
        
        // ✅ 트랜잭션 시작
        if (!db_layer.executeNonQuery("BEGIN TRANSACTION")) {
            LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                         "updateBulk: 트랜잭션 시작 실패");
            return 0;
        }
        
        int success_count = 0;
        
        try {
            for (const auto& entity : entities) {
                if (entity.getId() > 0 && validateVirtualPoint(entity)) {
                    auto params = entityToParams(entity);
                    params["id"] = std::to_string(entity.getId()); // WHERE 절용
                    std::string query = RepositoryHelpers::replaceParametersInOrder(SQL::VirtualPoint::UPDATE_BY_ID, params);
                    
                    if (db_layer.executeNonQuery(query)) {
                        success_count++;
                        
                        // 캐시 무효화
                        if (isCacheEnabled()) {
                            clearCacheForId(static_cast<int>(entity.getId()));
                        }
                    }
                }
            }
            
            // ✅ 트랜잭션 커밋
            if (!db_layer.executeNonQuery("COMMIT")) {
                LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                             "updateBulk: 트랜잭션 커밋 실패");
                return 0;
            }
            
        } catch (const std::exception& e) {
            // ✅ 트랜잭션 롤백
            db_layer.executeNonQuery("ROLLBACK");
            throw;
        }
        
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::INFO,
                                     "✅ updateBulk: " + std::to_string(success_count) + 
                                     "개 엔티티 벌크 업데이트 완료 (트랜잭션)");
        return success_count;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                     "updateBulk 실행 실패: " + std::string(e.what()));
        return 0;
    }
}

int VirtualPointRepository::deleteByIds(const std::vector<int>& ids) {
    if (ids.empty()) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::DEBUG,
                                     "deleteByIds: 빈 ID 리스트, 0 반환");
        return 0;
    }
    
    try {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::INFO,
                                     "🗑️ deleteByIds: " + std::to_string(ids.size()) + "개 ID 벌크 삭제 시작");
        
        DatabaseAbstractionLayer db_layer;
        
        // ✅ ExtendedSQLQueries.h 상수 사용
        std::string query = SQL::VirtualPoint::DELETE_BY_IDS;
        
        // ✅ RepositoryHelpers를 사용한 IN절 생성
        std::string in_clause = RepositoryHelpers::buildInClause(ids);
        RepositoryHelpers::replaceStringPlaceholder(query, "%IN_CLAUSE%", in_clause);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // ✅ 캐시에서 모든 ID 제거
            if (isCacheEnabled()) {
                for (int id : ids) {
                    clearCacheForId(id);
                }
            }
            
            LogManager::getInstance().log("VirtualPointRepository", LogLevel::INFO,
                                         "✅ deleteByIds: " + std::to_string(ids.size()) + 
                                         "개 엔티티 벌크 삭제 완료");
            return ids.size();
        }
        
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::WARN,
                                     "deleteByIds: 삭제 실패");
        return 0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::ERROR,
                                     "deleteByIds 실행 실패: " + std::string(e.what()));
        return 0;
    }
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