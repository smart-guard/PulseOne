/**
 * @file DeviceRepository.cpp - 스키마 기반 깔끔한 구현
 * @brief DeviceSettingsRepository 패턴을 따라 완전히 재작성
 * @author PulseOne Development Team
 * @date 2025-07-31
 */

#include "Database/Repositories/DeviceRepository.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// 🎯 간단하고 깔끔한 구현 - DeviceSettingsRepository 패턴 준수
// =============================================================================

std::vector<DeviceEntity> DeviceRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            logger_->Error("DeviceRepository::findAll - Table creation failed");
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, device_group_id, edge_server_id,
                name, description, device_type, manufacturer, model, serial_number,
                protocol_type, endpoint, config, polling_interval, timeout, retry_count,
                is_enabled, installation_date, last_maintenance, created_by,
                created_at, updated_at
            FROM devices 
            ORDER BY name
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<DeviceEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("DeviceRepository::findAll - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("DeviceRepository::findAll - Found " + std::to_string(entities.size()) + " devices");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<DeviceEntity> DeviceRepository::findById(int id) {
    try {
        // 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                logger_->Debug("DeviceRepository::findById - Cache hit for id: " + std::to_string(id));
                return cached.value();
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, device_group_id, edge_server_id,
                name, description, device_type, manufacturer, model, serial_number,
                protocol_type, endpoint, config, polling_interval, timeout, retry_count,
                is_enabled, installation_date, last_maintenance, created_by,
                created_at, updated_at
            FROM devices 
            WHERE id = )" + std::to_string(id);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            logger_->Debug("DeviceRepository::findById - Device not found: " + std::to_string(id));
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        logger_->Debug("DeviceRepository::findById - Found device: " + entity.getName());
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::findById failed for id " + std::to_string(id) + ": " + std::string(e.what()));
        return std::nullopt;
    }
}

bool DeviceRepository::save(DeviceEntity& entity) {
    try {
        if (!validateDevice(entity)) {
            logger_->Error("DeviceRepository::save - Invalid device data for: " + entity.getName());
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::map<std::string, std::string> data;
        
        // ID가 있으면 UPDATE, 없으면 INSERT
        if (entity.getId() > 0) {
            data["id"] = std::to_string(entity.getId());
        }
        
        // 스키마 기반 데이터 매핑
        data["tenant_id"] = std::to_string(entity.getTenantId());
        data["site_id"] = std::to_string(entity.getSiteId());
        
        if (entity.getDeviceGroupId().has_value()) {
            data["device_group_id"] = std::to_string(entity.getDeviceGroupId().value());
        }
        if (entity.getEdgeServerId().has_value()) {
            data["edge_server_id"] = std::to_string(entity.getEdgeServerId().value());
        }
        
        data["name"] = entity.getName();
        data["description"] = entity.getDescription();
        data["device_type"] = entity.getDeviceType();
        data["manufacturer"] = entity.getManufacturer();
        data["model"] = entity.getModel();
        data["serial_number"] = entity.getSerialNumber();
        data["protocol_type"] = entity.getProtocolType();
        data["endpoint"] = entity.getEndpoint();
        data["config"] = entity.getConfig();
        data["polling_interval"] = std::to_string(entity.getPollingInterval());
        data["timeout"] = std::to_string(entity.getTimeout());
        data["retry_count"] = std::to_string(entity.getRetryCount());
        data["is_enabled"] = db_layer.formatBoolean(entity.isEnabled());
        
        if (entity.getCreatedBy().has_value()) {
            data["created_by"] = std::to_string(entity.getCreatedBy().value());
        }
        
        data["updated_at"] = db_layer.getCurrentTimestamp();
        
        std::vector<std::string> primary_keys = {"id"};
        
        bool success = db_layer.executeUpsert("devices", data, primary_keys);
        
        if (success) {
            // ID가 새로 생성된 경우 업데이트
            if (entity.getId() == 0) {
                // 새로 생성된 ID 조회
                auto new_device = findByEndpoint(entity.getEndpoint());
                if (new_device.has_value()) {
                    entity.setId(new_device.value().getId());
                }
            }
            
            // 캐시 업데이트
            if (isCacheEnabled()) {
                cacheEntity(entity);
            }
            
            logger_->Info("DeviceRepository::save - Saved device: " + entity.getName());
        } else {
            logger_->Error("DeviceRepository::save - Failed to save device: " + entity.getName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::save failed: " + std::string(e.what()));
        return false;
    }
}

bool DeviceRepository::update(const DeviceEntity& entity) {
    DeviceEntity mutable_entity = entity;
    return save(mutable_entity);
}

bool DeviceRepository::deleteById(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        const std::string query = "DELETE FROM devices WHERE id = " + std::to_string(id);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            logger_->Info("DeviceRepository::deleteById - Deleted device id: " + std::to_string(id));
        } else {
            logger_->Error("DeviceRepository::deleteById - Failed to delete device id: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool DeviceRepository::exists(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        const std::string query = "SELECT COUNT(*) as count FROM devices WHERE id = " + std::to_string(id);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            int count = std::stoi(results[0].at("count"));
            return count > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::exists failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<DeviceEntity> DeviceRepository::findByIds(const std::vector<int>& ids) {
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
            if (i > 0) ids_ss << ", ";
            ids_ss << ids[i];
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, device_group_id, edge_server_id,
                name, description, device_type, manufacturer, model, serial_number,
                protocol_type, endpoint, config, polling_interval, timeout, retry_count,
                is_enabled, installation_date, last_maintenance, created_by,
                created_at, updated_at
            FROM devices 
            WHERE id IN ()" + ids_ss.str() + ")";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<DeviceEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("DeviceRepository::findByIds - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("DeviceRepository::findByIds - Found " + std::to_string(entities.size()) + " devices for " + std::to_string(ids.size()) + " ids");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::findByIds failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<DeviceEntity> DeviceRepository::findByConditions(
    const std::vector<QueryCondition>& /* conditions */,
    const std::optional<OrderBy>& /* order_by */,
    const std::optional<Pagination>& /* pagination */) {
    
    // 🎯 임시 구현: 모든 활성 디바이스 반환
    logger_->Info("DeviceRepository::findByConditions called - returning enabled devices for now");
    return findAllEnabled();
}

int DeviceRepository::countByConditions(const std::vector<QueryCondition>& /* conditions */) {
    // 🎯 임시 구현
    return getTotalCount();
}

std::optional<DeviceEntity> DeviceRepository::findFirstByConditions(
    const std::vector<QueryCondition>& conditions) {
    
    auto results = findByConditions(conditions, std::nullopt, std::nullopt);
    return results.empty() ? std::nullopt : std::make_optional(results[0]);
}

int DeviceRepository::saveBulk(std::vector<DeviceEntity>& entities) {
    int saved_count = 0;
    for (auto& entity : entities) {
        if (save(entity)) {
            saved_count++;
        }
    }
    logger_->Info("DeviceRepository::saveBulk - Saved " + std::to_string(saved_count) + " devices");
    return saved_count;
}

int DeviceRepository::updateBulk(const std::vector<DeviceEntity>& entities) {
    int updated_count = 0;
    for (const auto& entity : entities) {
        if (update(entity)) {
            updated_count++;
        }
    }
    logger_->Info("DeviceRepository::updateBulk - Updated " + std::to_string(updated_count) + " devices");
    return updated_count;
}

int DeviceRepository::deleteByIds(const std::vector<int>& ids) {
    int deleted_count = 0;
    for (int id : ids) {
        if (deleteById(id)) {
            deleted_count++;
        }
    }
    logger_->Info("DeviceRepository::deleteByIds - Deleted " + std::to_string(deleted_count) + " devices");
    return deleted_count;
}

// =============================================================================
// 캐시 관리 (IRepository 위임)
// =============================================================================

void DeviceRepository::setCacheEnabled(bool enabled) {
    IRepository<DeviceEntity>::setCacheEnabled(enabled);
    logger_->Info("DeviceRepository cache " + std::string(enabled ? "enabled" : "disabled"));
}

bool DeviceRepository::isCacheEnabled() const {
    return IRepository<DeviceEntity>::isCacheEnabled();
}

void DeviceRepository::clearCache() {
    IRepository<DeviceEntity>::clearCache();
    logger_->Info("DeviceRepository cache cleared");
}

void DeviceRepository::clearCacheForId(int id) {
    IRepository<DeviceEntity>::clearCacheForId(id);
    logger_->Debug("DeviceRepository cache cleared for ID: " + std::to_string(id));
}

std::map<std::string, int> DeviceRepository::getCacheStats() const {
    return IRepository<DeviceEntity>::getCacheStats();
}

int DeviceRepository::getTotalCount() {
    try {
        const std::string query = "SELECT COUNT(*) as count FROM devices";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::getTotalCount failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// Device 전용 메서드들
// =============================================================================

std::vector<DeviceEntity> DeviceRepository::findAllEnabled() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, device_group_id, edge_server_id,
                name, description, device_type, manufacturer, model, serial_number,
                protocol_type, endpoint, config, polling_interval, timeout, retry_count,
                is_enabled, installation_date, last_maintenance, created_by,
                created_at, updated_at
            FROM devices 
            WHERE is_enabled = )" + std::string("1") + R"(
            ORDER BY name
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<DeviceEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("DeviceRepository::findAllEnabled - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("DeviceRepository::findAllEnabled - Found " + std::to_string(entities.size()) + " enabled devices");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::findAllEnabled failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<DeviceEntity> DeviceRepository::findByProtocol(const std::string& protocol_type) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, device_group_id, edge_server_id,
                name, description, device_type, manufacturer, model, serial_number,
                protocol_type, endpoint, config, polling_interval, timeout, retry_count,
                is_enabled, installation_date, last_maintenance, created_by,
                created_at, updated_at
            FROM devices 
            WHERE protocol_type = ')" + escapeString(protocol_type) + R"('
            ORDER BY name
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<DeviceEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("DeviceRepository::findByProtocol - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("DeviceRepository::findByProtocol - Found " + std::to_string(entities.size()) + " " + protocol_type + " devices");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::findByProtocol failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<DeviceEntity> DeviceRepository::findByTenant(int tenant_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, device_group_id, edge_server_id,
                name, description, device_type, manufacturer, model, serial_number,
                protocol_type, endpoint, config, polling_interval, timeout, retry_count,
                is_enabled, installation_date, last_maintenance, created_by,
                created_at, updated_at
            FROM devices 
            WHERE tenant_id = )" + std::to_string(tenant_id) + R"(
            ORDER BY name
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<DeviceEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("DeviceRepository::findByTenant - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("DeviceRepository::findByTenant - Found " + std::to_string(entities.size()) + " devices for tenant " + std::to_string(tenant_id));
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::findByTenant failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<DeviceEntity> DeviceRepository::findBySite(int site_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, device_group_id, edge_server_id,
                name, description, device_type, manufacturer, model, serial_number,
                protocol_type, endpoint, config, polling_interval, timeout, retry_count,
                is_enabled, installation_date, last_maintenance, created_by,
                created_at, updated_at
            FROM devices 
            WHERE site_id = )" + std::to_string(site_id) + R"(
            ORDER BY name
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<DeviceEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("DeviceRepository::findBySite - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("DeviceRepository::findBySite - Found " + std::to_string(entities.size()) + " devices for site " + std::to_string(site_id));
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::findBySite failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<DeviceEntity> DeviceRepository::findByEndpoint(const std::string& endpoint) {
    try {
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, device_group_id, edge_server_id,
                name, description, device_type, manufacturer, model, serial_number,
                protocol_type, endpoint, config, polling_interval, timeout, retry_count,
                is_enabled, installation_date, last_maintenance, created_by,
                created_at, updated_at
            FROM devices 
            WHERE endpoint = ')" + escapeString(endpoint) + R"('
            LIMIT 1
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            logger_->Debug("DeviceRepository::findByEndpoint - Device not found: " + endpoint);
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        logger_->Debug("DeviceRepository::findByEndpoint - Found device: " + entity.getName());
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::findByEndpoint failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<DeviceEntity> DeviceRepository::findByNamePattern(const std::string& name_pattern) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, device_group_id, edge_server_id,
                name, description, device_type, manufacturer, model, serial_number,
                protocol_type, endpoint, config, polling_interval, timeout, retry_count,
                is_enabled, installation_date, last_maintenance, created_by,
                created_at, updated_at
            FROM devices 
            WHERE name LIKE '%)" + escapeString(name_pattern) + R"(%'
            ORDER BY name
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<DeviceEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("DeviceRepository::findByNamePattern - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("DeviceRepository::findByNamePattern - Found " + std::to_string(entities.size()) + " devices matching: " + name_pattern);
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::findByNamePattern failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<DeviceEntity> DeviceRepository::findDevicesForWorkers() {
    auto devices = findAllEnabled();
    logger_->Info("DeviceRepository::findDevicesForWorkers - Prepared " + std::to_string(devices.size()) + " devices for workers");
    return devices;
}

// =============================================================================
// 관계 데이터 사전 로딩 (임시 구현)
// =============================================================================

void DeviceRepository::preloadDataPoints(std::vector<DeviceEntity>& /* devices */) {
    logger_->Info("DeviceRepository::preloadDataPoints - Not implemented yet");
}

void DeviceRepository::preloadAlarmConfigs(std::vector<DeviceEntity>& /* devices */) {
    logger_->Info("DeviceRepository::preloadAlarmConfigs - Not implemented yet");
}

void DeviceRepository::preloadAllRelations(std::vector<DeviceEntity>& devices) {
    preloadDataPoints(devices);
    preloadAlarmConfigs(devices);
}

// =============================================================================
// 통계 메서드들
// =============================================================================

std::map<std::string, int> DeviceRepository::getCountByProtocol() {
    std::map<std::string, int> counts;
    
    try {
        const std::string query = "SELECT protocol_type, COUNT(*) as count FROM devices GROUP BY protocol_type";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        for (const auto& row : results) {
            auto protocol_it = row.find("protocol_type");
            auto count_it = row.find("count");
            if (protocol_it != row.end() && count_it != row.end()) {
                counts[protocol_it->second] = std::stoi(count_it->second);
            }
        }
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::getCountByProtocol failed: " + std::string(e.what()));
    }
    
    return counts;
}

std::map<int, int> DeviceRepository::getCountByTenant() {
    std::map<int, int> counts;
    
    try {
        const std::string query = "SELECT tenant_id, COUNT(*) as count FROM devices GROUP BY tenant_id";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        for (const auto& row : results) {
            auto tenant_it = row.find("tenant_id");
            auto count_it = row.find("count");
            if (tenant_it != row.end() && count_it != row.end()) {
                counts[std::stoi(tenant_it->second)] = std::stoi(count_it->second);
            }
        }
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::getCountByTenant failed: " + std::string(e.what()));
    }
    
    return counts;
}

std::map<int, int> DeviceRepository::getCountBySite() {
    std::map<int, int> counts;
    
    try {
        const std::string query = "SELECT site_id, COUNT(*) as count FROM devices GROUP BY site_id";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        for (const auto& row : results) {
            auto site_it = row.find("site_id");
            auto count_it = row.find("count");
            if (site_it != row.end() && count_it != row.end()) {
                counts[std::stoi(site_it->second)] = std::stoi(count_it->second);
            }
        }
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::getCountBySite failed: " + std::string(e.what()));
    }
    
    return counts;
}

std::map<std::string, int> DeviceRepository::getCountByStatus() {
    std::map<std::string, int> counts;
    
    try {
        const std::string query = "SELECT is_enabled, COUNT(*) as count FROM devices GROUP BY is_enabled";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        for (const auto& row : results) {
            auto enabled_it = row.find("is_enabled");
            auto count_it = row.find("count");
            if (enabled_it != row.end() && count_it != row.end()) {
                std::string status = db_layer.parseBoolean(enabled_it->second) ? "enabled" : "disabled";
                counts[status] = std::stoi(count_it->second);
            }
        }
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::getCountByStatus failed: " + std::string(e.what()));
    }
    
    return counts;
}

int DeviceRepository::updateDeviceStatuses(const std::map<int, std::string>& status_updates) {
    int updated_count = 0;
    
    try {
        DatabaseAbstractionLayer db_layer;
        
        for (const auto& [device_id, status] : status_updates) {
            const std::string query = "UPDATE devices SET updated_at = " + db_layer.getCurrentTimestamp() + 
                                    " WHERE id = " + std::to_string(device_id);
            
            if (db_layer.executeNonQuery(query)) {
                updated_count++;
                clearCacheForId(device_id);
            }
        }
        
        logger_->Info("DeviceRepository::updateDeviceStatuses - Updated " + std::to_string(updated_count) + " device statuses");
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::updateDeviceStatuses failed: " + std::string(e.what()));
    }
    
    return updated_count;
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

std::vector<DeviceEntity> DeviceRepository::mapResultsToEntities(
    const std::vector<std::map<std::string, std::string>>& results) {
    
    std::vector<DeviceEntity> entities;
    entities.reserve(results.size());
    
    for (const auto& row : results) {
        try {
            entities.push_back(mapRowToEntity(row));
        } catch (const std::exception& e) {
            logger_->Warn("DeviceRepository::mapResultsToEntities - Failed to map row: " + std::string(e.what()));
        }
    }
    
    return entities;
}

DeviceEntity DeviceRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    DeviceEntity entity;
    
    try {
        DatabaseAbstractionLayer db_layer;
        
        // 기본 식별자
        auto it = row.find("id");
        if (it != row.end()) {
            entity.setId(std::stoi(it->second));
        }
        
        it = row.find("tenant_id");
        if (it != row.end()) {
            entity.setTenantId(std::stoi(it->second));
        }
        
        it = row.find("site_id");
        if (it != row.end()) {
            entity.setSiteId(std::stoi(it->second));
        }
        
        it = row.find("device_group_id");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setDeviceGroupId(std::stoi(it->second));
        }
        
        it = row.find("edge_server_id");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setEdgeServerId(std::stoi(it->second));
        }
        
        // 디바이스 기본 정보
        it = row.find("name");
        if (it != row.end()) {
            entity.setName(it->second);
        }
        
        it = row.find("description");
        if (it != row.end()) {
            entity.setDescription(it->second);
        }
        
        it = row.find("device_type");
        if (it != row.end()) {
            entity.setDeviceType(it->second);
        }
        
        it = row.find("manufacturer");
        if (it != row.end()) {
            entity.setManufacturer(it->second);
        }
        
        it = row.find("model");
        if (it != row.end()) {
            entity.setModel(it->second);
        }
        
        it = row.find("serial_number");
        if (it != row.end()) {
            entity.setSerialNumber(it->second);
        }
        
        // 통신 설정
        it = row.find("protocol_type");
        if (it != row.end()) {
            entity.setProtocolType(it->second);
        }
        
        it = row.find("endpoint");
        if (it != row.end()) {
            entity.setEndpoint(it->second);
        }
        
        it = row.find("config");
        if (it != row.end()) {
            entity.setConfig(it->second);
        }
        
        // 수집 설정
        it = row.find("polling_interval");
        if (it != row.end()) {
            entity.setPollingInterval(std::stoi(it->second));
        }
        
        it = row.find("timeout");
        if (it != row.end()) {
            entity.setTimeout(std::stoi(it->second));
        }
        
        it = row.find("retry_count");
        if (it != row.end()) {
            entity.setRetryCount(std::stoi(it->second));
        }
        
        // 상태 정보
        it = row.find("is_enabled");
        if (it != row.end()) {
            entity.setEnabled(db_layer.parseBoolean(it->second));
        }
        
        it = row.find("created_by");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setCreatedBy(std::stoi(it->second));
        }
        
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::mapRowToEntity failed: " + std::string(e.what()));
        throw;
    }
}

bool DeviceRepository::ensureTableExists() {
    try {
        const std::string base_create_query = R"(
            CREATE TABLE IF NOT EXISTS devices (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                tenant_id INTEGER NOT NULL,
                site_id INTEGER NOT NULL,
                device_group_id INTEGER,
                edge_server_id INTEGER,
                
                -- 디바이스 기본 정보
                name VARCHAR(100) NOT NULL,
                description TEXT,
                device_type VARCHAR(50) NOT NULL,
                manufacturer VARCHAR(100),
                model VARCHAR(100),
                serial_number VARCHAR(100),
                
                -- 통신 설정
                protocol_type VARCHAR(50) NOT NULL,
                endpoint VARCHAR(255) NOT NULL,
                config TEXT NOT NULL,
                
                -- 수집 설정
                polling_interval INTEGER DEFAULT 1000,
                timeout INTEGER DEFAULT 3000,
                retry_count INTEGER DEFAULT 3,
                
                -- 상태 정보
                is_enabled BOOLEAN DEFAULT true,
                installation_date DATE,
                last_maintenance DATE,
                
                created_by INTEGER,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                
                FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
                FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
                FOREIGN KEY (device_group_id) REFERENCES device_groups(id) ON DELETE SET NULL,
                FOREIGN KEY (edge_server_id) REFERENCES edge_servers(id) ON DELETE SET NULL,
                FOREIGN KEY (created_by) REFERENCES users(id)
            )
        )";
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeCreateTable(base_create_query);
        
        if (success) {
            logger_->Debug("DeviceRepository::ensureTableExists - Table creation/check completed");
        } else {
            logger_->Error("DeviceRepository::ensureTableExists - Table creation failed");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::ensureTableExists failed: " + std::string(e.what()));
        return false;
    }
}

bool DeviceRepository::validateDevice(const DeviceEntity& entity) const {
    if (!entity.isValid()) {
        logger_->Warn("DeviceRepository::validateDevice - Invalid device: " + entity.getName());
        return false;
    }
    
    if (entity.getName().empty()) {
        logger_->Warn("DeviceRepository::validateDevice - Device name is empty");
        return false;
    }
    
    if (entity.getProtocolType().empty()) {
        logger_->Warn("DeviceRepository::validateDevice - Protocol type is empty for: " + entity.getName());
        return false;
    }
    
    if (entity.getEndpoint().empty()) {
        logger_->Warn("DeviceRepository::validateDevice - Endpoint is empty for: " + entity.getName());
        return false;
    }
    
    return true;
}

std::string DeviceRepository::escapeString(const std::string& str) const {
    std::string escaped = str;
    size_t pos = 0;
    while ((pos = escaped.find("'", pos)) != std::string::npos) {
        escaped.replace(pos, 1, "''");
        pos += 2;
    }
    return escaped;
}

// 나머지 빌드 메서드들은 임시 구현
std::string DeviceRepository::buildSelectQuery(
    const std::vector<QueryCondition>& /* conditions */,
    const std::optional<OrderBy>& /* order_by */,
    const std::optional<Pagination>& /* pagination */) {
    
    return "SELECT * FROM devices ORDER BY name";
}

std::string DeviceRepository::buildWhereClause(const std::vector<QueryCondition>& /* conditions */) const {
    return "";
}

std::string DeviceRepository::buildOrderByClause(const std::optional<OrderBy>& /* order_by */) const {
    return " ORDER BY name";
}

std::string DeviceRepository::buildLimitClause(const std::optional<Pagination>& /* pagination */) const {
    return "";
}

std::string DeviceRepository::buildWhereClauseWithAlias(const std::vector<QueryCondition>& /* conditions */) {
    return "";
}

std::vector<std::map<std::string, std::string>> DeviceRepository::executePostgresQuery(const std::string& /* sql */) {
    return {};
}

std::vector<std::map<std::string, std::string>> DeviceRepository::executeSQLiteQuery(const std::string& /* sql */) {
    return {};
}

bool DeviceRepository::executeUnifiedNonQuery(const std::string& /* sql */) {
    return false;
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne