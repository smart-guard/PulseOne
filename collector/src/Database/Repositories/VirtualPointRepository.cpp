// =============================================================================
// collector/src/Database/Repositories/VirtualPointRepository.cpp
// PulseOne VirtualPointRepository - DB 스키마와 완전 동기화
// =============================================================================

#include "Database/Repositories/VirtualPointRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Database/ExtendedSQLQueries.h"
#include "Database/SQLQueries.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// 가장 중요한 수정: DB 행을 엔티티로 변환 (모든 스키마 필드 포함)
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
        if (row.find("scope_type") != row.end()) {
            entity.setScopeType(row.at("scope_type"));
        }
        
        // Optional 필드들 (NULL 체크 필요)
        if (row.find("site_id") != row.end() && !row.at("site_id").empty() && row.at("site_id") != "NULL") {
            entity.setSiteId(std::stoi(row.at("site_id")));
        }
        if (row.find("device_id") != row.end() && !row.at("device_id").empty() && row.at("device_id") != "NULL") {
            entity.setDeviceId(std::stoi(row.at("device_id")));
        }
        
        // 필수 문자열 필드들
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
        
        // 계산 설정
        if (row.find("calculation_interval") != row.end()) {
            entity.setCalculationInterval(std::stoi(row.at("calculation_interval")));
        }
        if (row.find("calculation_trigger") != row.end()) {
            entity.setCalculationTrigger(row.at("calculation_trigger"));
        }
        
        // 활성화 여부
        if (row.find("is_enabled") != row.end()) {
            entity.setIsEnabled(row.at("is_enabled") == "1" || row.at("is_enabled") == "true");
        }
        
        // 분류 필드들
        if (row.find("category") != row.end()) {
            entity.setCategory(row.at("category"));
        }
        if (row.find("tags") != row.end()) {
            entity.setTags(row.at("tags"));
        }
        
        // v3.0.0 확장 필드들 - 실제 DB 스키마 필드들
        if (row.find("execution_type") != row.end()) {
            entity.setExecutionType(row.at("execution_type"));
        }
        if (row.find("dependencies") != row.end()) {
            entity.setDependencies(row.at("dependencies"));
        }
        if (row.find("cache_duration_ms") != row.end()) {
            entity.setCacheDurationMs(std::stoi(row.at("cache_duration_ms")));
        }
        if (row.find("error_handling") != row.end()) {
            entity.setErrorHandling(row.at("error_handling"));
        }
        if (row.find("last_error") != row.end()) {
            entity.setLastError(row.at("last_error"));
        }
        
        // 실행 통계 필드들
        if (row.find("execution_count") != row.end()) {
            entity.setExecutionCount(std::stoi(row.at("execution_count")));
        }
        if (row.find("avg_execution_time_ms") != row.end()) {
            entity.setAvgExecutionTimeMs(std::stod(row.at("avg_execution_time_ms")));
        }
        
        // 스크립트 라이브러리 연동 - 실제 DB 스키마에 존재
        if (row.find("script_library_id") != row.end() && !row.at("script_library_id").empty() && row.at("script_library_id") != "NULL") {
            entity.setScriptLibraryId(std::stoi(row.at("script_library_id")));
        }
        
        // 성능 추적 설정 - 실제 DB 스키마에 존재
        if (row.find("performance_tracking_enabled") != row.end()) {
            entity.setPerformanceTrackingEnabled(row.at("performance_tracking_enabled") == "1");
        }
        if (row.find("log_calculations") != row.end()) {
            entity.setLogCalculations(row.at("log_calculations") == "1");
        }
        if (row.find("log_errors") != row.end()) {
            entity.setLogErrors(row.at("log_errors") == "1");
        }
        
        // 알람 연동 - 실제 DB 스키마에 존재
        if (row.find("alarm_enabled") != row.end()) {
            entity.setAlarmEnabled(row.at("alarm_enabled") == "1");
        }
        if (row.find("high_limit") != row.end() && !row.at("high_limit").empty() && row.at("high_limit") != "NULL") {
            entity.setHighLimit(std::stod(row.at("high_limit")));
        }
        if (row.find("low_limit") != row.end() && !row.at("low_limit").empty() && row.at("low_limit") != "NULL") {
            entity.setLowLimit(std::stod(row.at("low_limit")));
        }
        if (row.find("deadband") != row.end()) {
            entity.setDeadband(std::stod(row.at("deadband")));
        }
        
        // 감사 필드들 - 실제 DB 스키마와 일치
        if (row.find("created_by") != row.end() && !row.at("created_by").empty() && row.at("created_by") != "NULL") {
            entity.setCreatedBy(std::stoi(row.at("created_by")));
        }
        
        // 타임스탬프는 Entity 내부에서 자동 관리되므로 여기서는 처리하지 않음
        
        entity.markSaved();
        return entity;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::mapRowToEntity failed: " + std::string(e.what()));
        throw;
    }
}

// =============================================================================
// 가장 중요한 수정: 엔티티를 DB 파라미터로 변환 (모든 스키마 필드 포함)
// =============================================================================

std::map<std::string, std::string> VirtualPointRepository::entityToParams(const VirtualPointEntity& entity) {
    std::map<std::string, std::string> params;
    
    // 기본 필드
    params["tenant_id"] = std::to_string(entity.getTenantId());
    params["scope_type"] = entity.getScopeType();
    
    // Optional 필드들 처리
    if (entity.getSiteId().has_value()) {
        params["site_id"] = std::to_string(entity.getSiteId().value());
    } else {
        params["site_id"] = "NULL";
    }
    
    if (entity.getDeviceId().has_value()) {
        params["device_id"] = std::to_string(entity.getDeviceId().value());
    } else {
        params["device_id"] = "NULL";
    }
    
    // 필수 문자열 필드들
    params["name"] = entity.getName();
    params["description"] = entity.getDescription();
    params["formula"] = entity.getFormula();
    params["data_type"] = entity.getDataType();
    params["unit"] = entity.getUnit();
    
    // 계산 설정
    params["calculation_interval"] = std::to_string(entity.getCalculationInterval());
    params["calculation_trigger"] = entity.getCalculationTrigger();
    params["is_enabled"] = entity.getIsEnabled() ? "1" : "0";
    
    // 분류 필드들
    params["category"] = entity.getCategory();
    params["tags"] = entity.getTags();
    
    // v3.0.0 확장 필드들 - 실제 DB 스키마 필드들
    params["execution_type"] = entity.getExecutionType();
    params["dependencies"] = entity.getDependencies();
    params["cache_duration_ms"] = std::to_string(entity.getCacheDurationMs());
    params["error_handling"] = entity.getErrorHandling();
    params["last_error"] = entity.getLastError();
    params["execution_count"] = std::to_string(entity.getExecutionCount());
    params["avg_execution_time_ms"] = std::to_string(entity.getAvgExecutionTimeMs());
    
    // 스크립트 라이브러리 연동
    if (entity.getScriptLibraryId().has_value()) {
        params["script_library_id"] = std::to_string(entity.getScriptLibraryId().value());
    } else {
        params["script_library_id"] = "NULL";
    }
    
    // 성능 추적 설정
    params["performance_tracking_enabled"] = entity.getPerformanceTrackingEnabled() ? "1" : "0";
    params["log_calculations"] = entity.getLogCalculations() ? "1" : "0";
    params["log_errors"] = entity.getLogErrors() ? "1" : "0";
    
    // 알람 연동
    params["alarm_enabled"] = entity.getAlarmEnabled() ? "1" : "0";
    
    if (entity.getHighLimit().has_value()) {
        params["high_limit"] = std::to_string(entity.getHighLimit().value());
    } else {
        params["high_limit"] = "NULL";
    }
    
    if (entity.getLowLimit().has_value()) {
        params["low_limit"] = std::to_string(entity.getLowLimit().value());
    } else {
        params["low_limit"] = "NULL";
    }
    
    params["deadband"] = std::to_string(entity.getDeadband());
    
    // 감사 필드
    if (entity.getCreatedBy().has_value()) {
        params["created_by"] = std::to_string(entity.getCreatedBy().value());
    } else {
        params["created_by"] = "NULL";
    }
    
    return params;
}

// =============================================================================
// 수정된 검증 메서드 - 실제 DB 제약조건 반영
// =============================================================================

bool VirtualPointRepository::validateVirtualPoint(const VirtualPointEntity& entity) {
    // 필수 필드 검증
    if (entity.getName().empty()) {
        LogManager::getInstance().Error("VirtualPointRepository::validateVirtualPoint - Name cannot be empty");
        return false;
    }
    
    if (entity.getFormula().empty()) {
        LogManager::getInstance().Error("VirtualPointRepository::validateVirtualPoint - Formula cannot be empty");
        return false;
    }
    
    if (entity.getTenantId() <= 0) {
        LogManager::getInstance().Error("VirtualPointRepository::validateVirtualPoint - Invalid tenant_id");
        return false;
    }
    
    // scope_type 검증 (DB 제약조건)
    const std::vector<std::string> valid_scope_types = {"tenant", "site", "device"};
    if (std::find(valid_scope_types.begin(), valid_scope_types.end(), entity.getScopeType()) == valid_scope_types.end()) {
        LogManager::getInstance().Error("VirtualPointRepository::validateVirtualPoint - Invalid scope_type: " + entity.getScopeType());
        return false;
    }
    
    // data_type 검증 (DB 제약조건)
    const std::vector<std::string> valid_data_types = {"bool", "int", "float", "double", "string"};
    if (std::find(valid_data_types.begin(), valid_data_types.end(), entity.getDataType()) == valid_data_types.end()) {
        LogManager::getInstance().Error("VirtualPointRepository::validateVirtualPoint - Invalid data_type: " + entity.getDataType());
        return false;
    }
    
    // calculation_trigger 검증 (DB 제약조건)
    const std::vector<std::string> valid_triggers = {"timer", "onchange", "manual", "event"};
    if (std::find(valid_triggers.begin(), valid_triggers.end(), entity.getCalculationTrigger()) == valid_triggers.end()) {
        LogManager::getInstance().Error("VirtualPointRepository::validateVirtualPoint - Invalid calculation_trigger: " + entity.getCalculationTrigger());
        return false;
    }
    
    // execution_type 검증 (DB 제약조건)
    const std::vector<std::string> valid_execution_types = {"javascript", "formula", "aggregation", "external"};
    if (std::find(valid_execution_types.begin(), valid_execution_types.end(), entity.getExecutionType()) == valid_execution_types.end()) {
        LogManager::getInstance().Error("VirtualPointRepository::validateVirtualPoint - Invalid execution_type: " + entity.getExecutionType());
        return false;
    }
    
    // error_handling 검증 (DB 제약조건)
    const std::vector<std::string> valid_error_handling = {"return_null", "return_zero", "return_previous", "throw_error"};
    if (std::find(valid_error_handling.begin(), valid_error_handling.end(), entity.getErrorHandling()) == valid_error_handling.end()) {
        LogManager::getInstance().Error("VirtualPointRepository::validateVirtualPoint - Invalid error_handling: " + entity.getErrorHandling());
        return false;
    }
    
    // scope 일관성 검증 (DB 제약조건)
    if (entity.getScopeType() == "tenant" && (entity.getSiteId().has_value() || entity.getDeviceId().has_value())) {
        LogManager::getInstance().Error("VirtualPointRepository::validateVirtualPoint - Tenant scope cannot have site_id or device_id");
        return false;
    }
    
    if (entity.getScopeType() == "site" && (!entity.getSiteId().has_value() || entity.getDeviceId().has_value())) {
        LogManager::getInstance().Error("VirtualPointRepository::validateVirtualPoint - Site scope must have site_id but no device_id");
        return false;
    }
    
    if (entity.getScopeType() == "device" && (!entity.getSiteId().has_value() || !entity.getDeviceId().has_value())) {
        LogManager::getInstance().Error("VirtualPointRepository::validateVirtualPoint - Device scope must have both site_id and device_id");
        return false;
    }
    
    // 계산 간격은 양수여야 함
    if (entity.getCalculationInterval() <= 0) {
        LogManager::getInstance().Error("VirtualPointRepository::validateVirtualPoint - Calculation interval must be positive");
        return false;
    }
    
    return true;
}

// =============================================================================
// 기존 메서드들은 동일하게 유지 (IRepository 인터페이스 구현)
// =============================================================================

std::vector<VirtualPointEntity> VirtualPointRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            LogManager::getInstance().Error("VirtualPointRepository::findAll - Table creation failed");
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::VirtualPoint::FIND_ALL);
        
        std::vector<VirtualPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().Warn("VirtualPointRepository::findAll - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().Info("VirtualPointRepository::findAll - Found " + std::to_string(entities.size()) + " virtual points");
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::findAll failed: " + std::string(e.what()));
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
                LogManager::getInstance().Debug("VirtualPointRepository::findById - Cache hit for ID: " + std::to_string(id));
                return cached;
            }
        }
        
        DatabaseAbstractionLayer db_layer;
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
        LogManager::getInstance().Error("VirtualPointRepository::findById failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool VirtualPointRepository::save(VirtualPointEntity& entity) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        if (!validateVirtualPoint(entity)) {
            LogManager::getInstance().Error("VirtualPointRepository::save - Invalid virtual point data");
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto params = entityToParams(entity);
        std::string query = RepositoryHelpers::replaceParametersInOrder(SQL::VirtualPoint::INSERT, params);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success && entity.getId() <= 0) {
            // 새로 생성된 ID 가져오기
            auto id_results = db_layer.executeQuery(SQL::Common::GET_LAST_INSERT_ID);
            if (!id_results.empty() && id_results[0].find("id") != id_results[0].end()) {
                entity.setId(std::stoll(id_results[0].at("id")));
            }
            
            LogManager::getInstance().Info("VirtualPointRepository::save - Saved virtual point: " + entity.getName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::save failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::update(const VirtualPointEntity& entity) {
    try {
        if (entity.getId() <= 0 || !ensureTableExists()) {
            return false;
        }
        
        if (!validateVirtualPoint(entity)) {
            LogManager::getInstance().Error("VirtualPointRepository::update - Invalid virtual point data");
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto params = entityToParams(entity);
        params["id"] = std::to_string(entity.getId());
        std::string query = RepositoryHelpers::replaceParametersInOrder(SQL::VirtualPoint::UPDATE_BY_ID, params);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(static_cast<int>(entity.getId()));
            }
            
            LogManager::getInstance().Info("VirtualPointRepository::update - Updated virtual point ID: " + std::to_string(entity.getId()));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::update failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::deleteById(int id) {
    try {
        if (id <= 0 || !ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        std::string query = RepositoryHelpers::replaceParameter(SQL::VirtualPoint::DELETE_BY_IDS, std::to_string(id));
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시에서 제거
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            LogManager::getInstance().Info("VirtualPointRepository::deleteById - Deleted virtual point ID: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::exists(int id) {
    try {
        if (id <= 0 || !ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        std::string query = RepositoryHelpers::replaceParameter(SQL::VirtualPoint::EXISTS_BY_ID, std::to_string(id));
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count")) > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::exists failed: " + std::string(e.what()));
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
        LogManager::getInstance().Error("VirtualPointRepository::getTotalCount failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// VirtualPoint 특화 메서드들 완전 구현
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
                LogManager::getInstance().Warn("VirtualPointRepository::findByTenant - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().Info("VirtualPointRepository::findByTenant - Found " + std::to_string(entities.size()) + " virtual points for tenant " + std::to_string(tenant_id));
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::findByTenant failed: " + std::string(e.what()));
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
                LogManager::getInstance().Warn("VirtualPointRepository::findBySite - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().Info("VirtualPointRepository::findBySite - Found " + std::to_string(entities.size()) + " virtual points for site " + std::to_string(site_id));
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::findBySite failed: " + std::string(e.what()));
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
                LogManager::getInstance().Warn("VirtualPointRepository::findByDevice - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().Info("VirtualPointRepository::findByDevice - Found " + std::to_string(entities.size()) + " virtual points for device " + std::to_string(device_id));
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::findByDevice failed: " + std::string(e.what()));
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
                LogManager::getInstance().Warn("VirtualPointRepository::findEnabled - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().Info("VirtualPointRepository::findEnabled - Found " + std::to_string(entities.size()) + " enabled virtual points");
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::findEnabled failed: " + std::string(e.what()));
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
                LogManager::getInstance().Warn("VirtualPointRepository::findByCategory - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().Info("VirtualPointRepository::findByCategory - Found " + std::to_string(entities.size()) + " virtual points for category: " + category);
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::findByCategory failed: " + std::string(e.what()));
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
                LogManager::getInstance().Warn("VirtualPointRepository::findByExecutionType - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().Info("VirtualPointRepository::findByExecutionType - Found " + std::to_string(entities.size()) + " virtual points for execution_type: " + execution_type);
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::findByExecutionType failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByTag(const std::string& tag) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // JSON 배열에서 태그 검색하는 쿼리
        std::string query = "SELECT * FROM virtual_points WHERE tags LIKE '%" + tag + "%' ORDER BY name";
        auto results = db_layer.executeQuery(query);
        
        std::vector<VirtualPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                auto entity = mapRowToEntity(row);
                // 실제로 해당 태그를 가지고 있는지 확인 (LIKE 검색의 false positive 방지)
                if (entity.hasTag(tag)) {
                    entities.push_back(entity);
                }
            } catch (const std::exception& e) {
                LogManager::getInstance().Warn("VirtualPointRepository::findByTag - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().Info("VirtualPointRepository::findByTag - Found " + std::to_string(entities.size()) + " virtual points for tag: " + tag);
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::findByTag failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<VirtualPointEntity> VirtualPointRepository::findDependents(int point_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 수식에서 특정 포인트 ID를 참조하는 가상포인트들 찾기
        std::string search_pattern = "%" + std::to_string(point_id) + "%";
        std::string query = "SELECT * FROM virtual_points WHERE formula LIKE '" + search_pattern + "' OR dependencies LIKE '" + search_pattern + "' ORDER BY name";
        auto results = db_layer.executeQuery(query);
        
        std::vector<VirtualPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().Warn("VirtualPointRepository::findDependents - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().Info("VirtualPointRepository::findDependents - Found " + std::to_string(entities.size()) + " dependents for point " + std::to_string(point_id));
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::findDependents failed: " + std::string(e.what()));
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
        params["execution_time_ms"] = std::to_string(execution_time_ms);
        params["id"] = std::to_string(id);
        
        // execution_count 증가 및 avg_execution_time_ms 업데이트
        std::string query = "UPDATE virtual_points SET "
                           "execution_count = execution_count + 1, "
                           "avg_execution_time_ms = (avg_execution_time_ms * execution_count + " + params["execution_time_ms"] + ") / (execution_count + 1), "
                           "last_execution_time = CURRENT_TIMESTAMP "
                           "WHERE id = " + params["id"];
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            LogManager::getInstance().Debug("VirtualPointRepository::updateExecutionStats - Updated stats for ID: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::updateExecutionStats failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::updateLastError(int id, const std::string& error_message) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // SQL injection 방지를 위한 간단한 이스케이핑
        std::string escaped_error = error_message;
        std::replace(escaped_error.begin(), escaped_error.end(), '\'', '\"');
        
        std::string query = "UPDATE virtual_points SET last_error = '" + escaped_error + "' WHERE id = " + std::to_string(id);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            LogManager::getInstance().Debug("VirtualPointRepository::updateLastError - Updated error for ID: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::updateLastError failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::updateLastValue(int id, double last_value) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        // 주의: last_value는 실제로는 virtual_point_values 테이블에 저장됨
        // 이 메서드는 호환성을 위해 제공되지만, 실제 구현에서는 virtual_point_values 테이블을 사용해야 함
        LogManager::getInstance().Warn("VirtualPointRepository::updateLastValue - This method should update virtual_point_values table instead");
        
        // 임시로 execution_count만 업데이트
        DatabaseAbstractionLayer db_layer;
        std::string query = "UPDATE virtual_points SET execution_count = execution_count + 1, last_execution_time = CURRENT_TIMESTAMP WHERE id = " + std::to_string(id);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::updateLastValue failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::setEnabled(int id, bool enabled) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        std::string query = "UPDATE virtual_points SET is_enabled = " + std::string(enabled ? "1" : "0") + " WHERE id = " + std::to_string(id);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            LogManager::getInstance().Info("VirtualPointRepository::setEnabled - Set virtual point " + std::to_string(id) + " to " + (enabled ? "enabled" : "disabled"));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::setEnabled failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointRepository::resetExecutionStats(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        std::string query = "UPDATE virtual_points SET "
                           "execution_count = 0, "
                           "avg_execution_time_ms = 0.0, "
                           "last_error = '', "
                           "last_execution_time = NULL "
                           "WHERE id = " + std::to_string(id);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            LogManager::getInstance().Info("VirtualPointRepository::resetExecutionStats - Reset stats for virtual point " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::resetExecutionStats failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 벌크 연산 메서드들 완전 구현
// =============================================================================

std::vector<VirtualPointEntity> VirtualPointRepository::findByIds(const std::vector<int>& ids) {
    if (ids.empty()) {
        LogManager::getInstance().Debug("VirtualPointRepository::findByIds - Empty ID list, returning empty result");
        return {};
    }
    
    try {
        LogManager::getInstance().Debug("VirtualPointRepository::findByIds - Bulk query for " + std::to_string(ids.size()) + " IDs");
        
        DatabaseAbstractionLayer db_layer;
        
        std::string query = SQL::VirtualPoint::FIND_BY_IDS;
        std::string in_clause = RepositoryHelpers::buildInClause(ids);
        RepositoryHelpers::replaceStringPlaceholder(query, "%IN_CLAUSE%", in_clause);
        
        auto results = db_layer.executeQuery(query);
        
        std::vector<VirtualPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().Warn("VirtualPointRepository::findByIds - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().Info("VirtualPointRepository::findByIds - Found " + std::to_string(entities.size()) + "/" + std::to_string(ids.size()) + " virtual points");
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::findByIds failed: " + std::string(e.what()));
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
        
        LogManager::getInstance().Debug("VirtualPointRepository::findByConditions - Search with " + std::to_string(conditions.size()) + " conditions");
        
        DatabaseAbstractionLayer db_layer;
        std::string query = SQL::VirtualPoint::FIND_ALL;
        
        if (!conditions.empty()) {
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
                } else if (condition.operation == "IN") {
                    where_clause += condition.field + " IN (" + condition.value + ")";
                } else {
                    where_clause += condition.field + " = '" + condition.value + "'";
                }
                
                first = false;
            }
            
            size_t order_pos = query.find("ORDER BY");
            if (order_pos != std::string::npos) {
                query.insert(order_pos, where_clause + " ");
            } else {
                query += where_clause;
            }
        }
        
        if (order_by.has_value()) {
            size_t order_pos = query.find("ORDER BY");
            if (order_pos != std::string::npos) {
                query = query.substr(0, order_pos);
            }
            query += " ORDER BY " + order_by->field + " " + (order_by->ascending ? "ASC" : "DESC");
        }
        
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
                LogManager::getInstance().Warn("VirtualPointRepository::findByConditions - Failed to map row: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().Debug("VirtualPointRepository::findByConditions - Found " + std::to_string(entities.size()) + " virtual points");
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::findByConditions failed: " + std::string(e.what()));
        return {};
    }
}

int VirtualPointRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        LogManager::getInstance().Debug("VirtualPointRepository::countByConditions - Count with " + std::to_string(conditions.size()) + " conditions");
        
        DatabaseAbstractionLayer db_layer;
        std::string query = SQL::VirtualPoint::COUNT_ALL;
        
        if (!conditions.empty()) {
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
            
            size_t from_pos = query.find("FROM virtual_points");
            if (from_pos != std::string::npos) {
                size_t insert_pos = from_pos + strlen("FROM virtual_points");
                query.insert(insert_pos, " " + where_clause);
            }
        }
        
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            int count = std::stoi(results[0].at("count"));
            LogManager::getInstance().Debug("VirtualPointRepository::countByConditions - Count: " + std::to_string(count));
            return count;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::countByConditions failed: " + std::string(e.what()));
        return 0;
    }
}

int VirtualPointRepository::saveBulk(std::vector<VirtualPointEntity>& entities) {
    if (entities.empty()) {
        LogManager::getInstance().Debug("VirtualPointRepository::saveBulk - Empty entity list, returning 0");
        return 0;
    }
    
    try {
        LogManager::getInstance().Info("VirtualPointRepository::saveBulk - Bulk save for " + std::to_string(entities.size()) + " entities");
        
        DatabaseAbstractionLayer db_layer;
        
        if (!db_layer.executeNonQuery("BEGIN TRANSACTION")) {
            LogManager::getInstance().Error("VirtualPointRepository::saveBulk - Failed to start transaction");
            return 0;
        }
        
        int success_count = 0;
        
        try {
            for (auto& entity : entities) {
                if (validateVirtualPoint(entity)) {
                    auto params = entityToParams(entity);
                    std::string query = RepositoryHelpers::replaceParametersInOrder(SQL::VirtualPoint::INSERT, params);
                    
                    if (db_layer.executeNonQuery(query)) {
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
            
            if (!db_layer.executeNonQuery("COMMIT")) {
                LogManager::getInstance().Error("VirtualPointRepository::saveBulk - Failed to commit transaction");
                return 0;
            }
            
        } catch (const std::exception& e) {
            db_layer.executeNonQuery("ROLLBACK");
            throw;
        }
        
        LogManager::getInstance().Info("VirtualPointRepository::saveBulk - Successfully saved " + std::to_string(success_count) + " entities (with transaction)");
        return success_count;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::saveBulk failed: " + std::string(e.what()));
        return 0;
    }
}

int VirtualPointRepository::updateBulk(const std::vector<VirtualPointEntity>& entities) {
    if (entities.empty()) {
        LogManager::getInstance().Debug("VirtualPointRepository::updateBulk - Empty entity list, returning 0");
        return 0;
    }
    
    try {
        LogManager::getInstance().Info("VirtualPointRepository::updateBulk - Bulk update for " + std::to_string(entities.size()) + " entities");
        
        DatabaseAbstractionLayer db_layer;
        
        if (!db_layer.executeNonQuery("BEGIN TRANSACTION")) {
            LogManager::getInstance().Error("VirtualPointRepository::updateBulk - Failed to start transaction");
            return 0;
        }
        
        int success_count = 0;
        
        try {
            for (const auto& entity : entities) {
                if (entity.getId() > 0 && validateVirtualPoint(entity)) {
                    auto params = entityToParams(entity);
                    params["id"] = std::to_string(entity.getId());
                    std::string query = RepositoryHelpers::replaceParametersInOrder(SQL::VirtualPoint::UPDATE_BY_ID, params);
                    
                    if (db_layer.executeNonQuery(query)) {
                        success_count++;
                        
                        if (isCacheEnabled()) {
                            clearCacheForId(static_cast<int>(entity.getId()));
                        }
                    }
                }
            }
            
            if (!db_layer.executeNonQuery("COMMIT")) {
                LogManager::getInstance().Error("VirtualPointRepository::updateBulk - Failed to commit transaction");
                return 0;
            }
            
        } catch (const std::exception& e) {
            db_layer.executeNonQuery("ROLLBACK");
            throw;
        }
        
        LogManager::getInstance().Info("VirtualPointRepository::updateBulk - Successfully updated " + std::to_string(success_count) + " entities (with transaction)");
        return success_count;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::updateBulk failed: " + std::string(e.what()));
        return 0;
    }
}

int VirtualPointRepository::deleteByIds(const std::vector<int>& ids) {
    if (ids.empty()) {
        LogManager::getInstance().Debug("VirtualPointRepository::deleteByIds - Empty ID list, returning 0");
        return 0;
    }
    
    try {
        LogManager::getInstance().Info("VirtualPointRepository::deleteByIds - Bulk delete for " + std::to_string(ids.size()) + " IDs");
        
        DatabaseAbstractionLayer db_layer;
        
        std::string query = SQL::VirtualPoint::DELETE_BY_IDS;
        std::string in_clause = RepositoryHelpers::buildInClause(ids);
        RepositoryHelpers::replaceStringPlaceholder(query, "%IN_CLAUSE%", in_clause);
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                for (int id : ids) {
                    clearCacheForId(id);
                }
            }
            
            LogManager::getInstance().Info("VirtualPointRepository::deleteByIds - Successfully deleted " + std::to_string(ids.size()) + " entities");
            return ids.size();
        }
        
        LogManager::getInstance().Warn("VirtualPointRepository::deleteByIds - Delete failed");
        return 0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::deleteByIds failed: " + std::string(e.what()));
        return 0;
    }
}

bool VirtualPointRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        return db_layer.executeNonQuery(SQL::VirtualPoint::CREATE_TABLE);
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("VirtualPointRepository::ensureTableExists failed: " + std::string(e.what()));
        return false;
    }
}

// 캐시 관련 메서드들은 IRepository 구현 위임...
void VirtualPointRepository::clearCache() {
    IRepository<VirtualPointEntity>::clearCache();
    LogManager::getInstance().Info("VirtualPointRepository::clearCache - All cache cleared");
}

std::map<std::string, int> VirtualPointRepository::getCacheStats() const {
    return IRepository<VirtualPointEntity>::getCacheStats();
}

bool VirtualPointRepository::isCacheEnabled() const {
    return IRepository<VirtualPointEntity>::isCacheEnabled();
}

std::optional<VirtualPointEntity> VirtualPointRepository::getCachedEntity(int id) {
    return std::nullopt; // 기본 구현에서는 캐시 미사용
}

void VirtualPointRepository::cacheEntity(int id, const VirtualPointEntity& entity) {
    // IRepository의 기본 캐시 구현 사용
}

void VirtualPointRepository::clearCacheForId(int id) {
    IRepository<VirtualPointEntity>::clearCacheForId(id);
}

std::string VirtualPointRepository::generateCacheKey(int id) const {
    return "virtualpoint:" + std::to_string(id);
}

} // namespace Repositories
} // namespace Database  
} // namespace PulseOne