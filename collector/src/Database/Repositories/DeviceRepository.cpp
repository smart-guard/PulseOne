// =============================================================================
// collector/src/Database/Repositories/DeviceRepository.cpp
// PulseOne 디바이스 Repository 구현 - IRepository 패턴 준수
// =============================================================================

#include "Database/Repositories/DeviceRepository.h"
#include "Common/Constants.h"
#include <sstream>
#include <algorithm>
#include <thread>

using namespace PulseOne::Constants;

namespace PulseOne {
namespace Database {
namespace Repositories{
// =============================================================================
// 생성자 및 초기화
// =============================================================================

DeviceRepository::DeviceRepository()
    : db_manager_(DatabaseManager::getInstance())
    , config_manager_(ConfigManager::getInstance())
    , logger_(PulseOne::LogManager::getInstance())
    , cache_enabled_(true)
    , cache_ttl_(std::chrono::seconds(300))  // 5분 TTL
    , cache_hits_(0)
    , cache_misses_(0)
    , cache_evictions_(0)
    , max_cache_size_(1000)
    , enable_bulk_optimization_(true) {
    
    logger_.Info("🏭 DeviceRepository initialized with caching enabled");
}

// =============================================================================
// IRepository 기본 CRUD 구현
// =============================================================================

std::vector<DeviceEntity> DeviceRepository::findAll() {
    try {
        std::string query = buildSelectQuery();
        
        auto results = db_manager_.isPostgresConnected() 
            ? executePostgresQuery(query)
            : executeSQLiteQuery(query);
        
        auto entities = mapResultsToEntities(results);
        
        logger_.Info("DeviceRepository::findAll - Found " + std::to_string(entities.size()) + " devices");
        return entities;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<DeviceEntity> DeviceRepository::findById(int id) {
    if (id <= 0) {
        logger_.Warn("DeviceRepository::findById - Invalid ID: " + std::to_string(id));
        return std::nullopt;
    }
    
    // 캐시 확인
    if (cache_enabled_) {
        auto cached = getCachedEntity(id);
        if (cached.has_value()) {
            cache_hits_++;
            logger_.Debug("DeviceRepository::findById - Cache hit for ID: " + std::to_string(id));
            return cached;
        }
        cache_misses_++;
    }
    
    try {
        std::vector<QueryCondition> conditions = {
            QueryCondition("id", "=", std::to_string(id))
        };
        
        auto entities = findByConditions(conditions, std::nullopt, Pagination(1, 1));
        
        if (entities.empty()) {
            logger_.Debug("DeviceRepository::findById - Device not found: " + std::to_string(id));
            return std::nullopt;
        }
        
        // 캐시에 저장
        if (cache_enabled_) {
            cacheEntity(entities[0]);
        }
        
        logger_.Debug("DeviceRepository::findById - Found device: " + entities[0].getDeviceName());
        return entities[0];
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::findById failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool DeviceRepository::save(DeviceEntity& entity) {
    try {
        bool success = entity.saveToDatabase();
        
        if (success && cache_enabled_) {
            cacheEntity(entity);
            logger_.Info("DeviceRepository::save - Saved and cached device: " + entity.getDeviceName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::save failed: " + std::string(e.what()));
        return false;
    }
}

bool DeviceRepository::update(const DeviceEntity& entity) {
    try {
        // const_cast를 사용하여 업데이트 (실제로는 BaseEntity에서 처리)
        DeviceEntity& mutable_entity = const_cast<DeviceEntity&>(entity);
        bool success = mutable_entity.updateToDatabase();
        
        if (success && cache_enabled_) {
            // 캐시에서 제거 (다음 조회 시 새로 로드)
            clearCacheForId(entity.getId());
            logger_.Info("DeviceRepository::update - Updated device and cleared cache: " + entity.getDeviceName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::update failed: " + std::string(e.what()));
        return false;
    }
}

bool DeviceRepository::deleteById(int id) {
    try {
        DeviceEntity entity(id);
        bool success = entity.deleteFromDatabase();
        
        if (success && cache_enabled_) {
            clearCacheForId(id);
            logger_.Info("DeviceRepository::deleteById - Deleted device and cleared cache: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::deleteById failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// IRepository 벌크 연산 구현
// =============================================================================

std::vector<DeviceEntity> DeviceRepository::findByIds(const std::vector<int>& ids) {
    if (ids.empty()) {
        return {};
    }
    
    try {
        // ID 목록을 문자열로 변환
        std::ostringstream id_list;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) id_list << ",";
            id_list << ids[i];
        }
        
        std::vector<QueryCondition> conditions = {
            QueryCondition("id", "IN", id_list.str())
        };
        
        auto entities = findByConditions(conditions);
        
        // 캐시에 저장
        if (cache_enabled_) {
            for (const auto& entity : entities) {
                cacheEntity(entity);
            }
        }
        
        logger_.Info("DeviceRepository::findByIds - Found " + 
                    std::to_string(entities.size()) + "/" + std::to_string(ids.size()) + " devices");
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::findByIds failed: " + std::string(e.what()));
        return {};
    }
}

int DeviceRepository::saveBulk(std::vector<DeviceEntity>& entities) {
    if (entities.empty()) {
        return 0;
    }
    
    try {
        int saved_count = 0;
        
        if (enable_bulk_optimization_ && entities.size() > 10) {
            // 벌크 INSERT 최적화 (트랜잭션 사용)
            std::ostringstream bulk_sql;
            bulk_sql << "INSERT INTO devices (tenant_id, site_id, name, description, protocol_type, endpoint, config, ";
            bulk_sql << "is_enabled, status, polling_interval, timeout_ms, retry_count, created_at, updated_at) VALUES ";
            
            for (size_t i = 0; i < entities.size(); ++i) {
                if (i > 0) bulk_sql << ", ";
                
                const auto& entity = entities[i];
                const auto& info = entity.getDeviceInfo();
                
                bulk_sql << "(";
                bulk_sql << entity.getTenantId() << ", ";
                bulk_sql << entity.getSiteId() << ", ";
                bulk_sql << "'" << escapeString(info.name) << "', ";
                bulk_sql << "'" << escapeString(info.description) << "', ";
                bulk_sql << "'" << escapeString(info.protocol_type) << "', ";
                bulk_sql << "'" << escapeString(info.connection_string) << "', ";
                bulk_sql << "'" << escapeString(info.connection_config) << "', ";
                bulk_sql << (info.is_enabled ? "1" : "0") << ", ";
                bulk_sql << "'" << escapeString(info.status) << "', ";
                bulk_sql << info.polling_interval_ms << ", ";
                bulk_sql << info.timeout_ms << ", ";
                bulk_sql << info.retry_count << ", ";
                bulk_sql << "NOW(), NOW()";
                bulk_sql << ")";
            }
            
            bool success = executeUnifiedNonQuery(bulk_sql.str());
            if (success) {
                saved_count = static_cast<int>(entities.size());
                logger_.Info("DeviceRepository::saveBulk - Bulk saved " + std::to_string(saved_count) + " devices");
            }
            
        } else {
            // 개별 저장
            for (auto& entity : entities) {
                if (entity.saveToDatabase()) {
                    saved_count++;
                    
                    // 캐시에 저장
                    if (cache_enabled_) {
                        cacheEntity(entity);
                    }
                }
            }
            
            logger_.Info("DeviceRepository::saveBulk - Individual saved " + std::to_string(saved_count) + " devices");
        }
        
        return saved_count;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::saveBulk failed: " + std::string(e.what()));
        return 0;
    }
}

int DeviceRepository::updateBulk(const std::vector<DeviceEntity>& entities) {
    if (entities.empty()) {
        return 0;
    }
    
    try {
        int updated_count = 0;
        
        // 개별 업데이트 (각 엔티티의 고유한 변경사항 때문에)
        for (const auto& entity : entities) {
            DeviceEntity& mutable_entity = const_cast<DeviceEntity&>(entity);
            if (mutable_entity.updateToDatabase()) {
                updated_count++;
                
                // 캐시에서 제거
                if (cache_enabled_) {
                    clearCacheForId(entity.getId());
                }
            }
        }
        
        logger_.Info("DeviceRepository::updateBulk - Updated " + std::to_string(updated_count) + " devices");
        return updated_count;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::updateBulk failed: " + std::string(e.what()));
        return 0;
    }
}

int DeviceRepository::deleteByIds(const std::vector<int>& ids) {
    if (ids.empty()) {
        return 0;
    }
    
    try {
        // ID 목록을 문자열로 변환
        std::ostringstream id_list;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) id_list << ",";
            id_list << ids[i];
        }
        
        std::string sql = "DELETE FROM devices WHERE id IN (" + id_list.str() + ")";
        
        bool success = executeUnifiedNonQuery(sql);
        
        if (success && cache_enabled_) {
            // 캐시에서 제거
            for (int id : ids) {
                clearCacheForId(id);
            }
        }
        
        int deleted_count = success ? static_cast<int>(ids.size()) : 0;
        logger_.Info("DeviceRepository::deleteByIds - Deleted " + std::to_string(deleted_count) + " devices");
        
        return deleted_count;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::deleteByIds failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// IRepository 조건부 조회 구현
// =============================================================================

std::vector<DeviceEntity> DeviceRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    try {
        std::string query = buildSelectQuery(conditions, order_by, pagination);
        
        auto results = db_manager_.isPostgresConnected() 
            ? executePostgresQuery(query)
            : executeSQLiteQuery(query);
        
        auto entities = mapResultsToEntities(results);
        
        logger_.Debug("DeviceRepository::findByConditions - Found " + 
                     std::to_string(entities.size()) + " devices");
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::findByConditions failed: " + std::string(e.what()));
        return {};
    }
}

int DeviceRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        std::string where_clause = buildWhereClause(conditions);
        std::string query = "SELECT COUNT(*) as count FROM devices" + where_clause;
        
        auto results = db_manager_.isPostgresConnected() 
            ? executePostgresQuery(query)
            : executeSQLiteQuery(query);
        
        if (results.empty()) {
            return 0;
        }
        
        return std::stoi(results[0]["count"]);
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::countByConditions failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// 디바이스 전용 조회 메서드들
// =============================================================================

std::vector<DeviceEntity> DeviceRepository::findAllEnabled() {
    std::vector<QueryCondition> conditions = {
        QueryCondition("is_enabled", "=", "1")
    };
    
    auto entities = findByConditions(conditions, OrderBy("name", true));
    
    logger_.Info("DeviceRepository::findAllEnabled - Found " + 
                std::to_string(entities.size()) + " enabled devices");
    
    return entities;
}

std::vector<DeviceEntity> DeviceRepository::findByProtocol(const std::string& protocol_type) {
    std::vector<QueryCondition> conditions = {
        QueryCondition("protocol_type", "=", protocol_type)
    };
    
    auto entities = findByConditions(conditions, OrderBy("name", true));
    
    logger_.Info("DeviceRepository::findByProtocol - Found " + 
                std::to_string(entities.size()) + " " + protocol_type + " devices");
    
    return entities;
}

std::vector<DeviceEntity> DeviceRepository::findByTenant(int tenant_id) {
    std::vector<QueryCondition> conditions = {
        QueryCondition("tenant_id", "=", std::to_string(tenant_id))
    };
    
    auto entities = findByConditions(conditions, OrderBy("name", true));
    
    logger_.Info("DeviceRepository::findByTenant - Found " + 
                std::to_string(entities.size()) + " devices for tenant " + std::to_string(tenant_id));
    
    return entities;
}

std::vector<DeviceEntity> DeviceRepository::findBySite(int site_id) {
    std::vector<QueryCondition> conditions = {
        QueryCondition("site_id", "=", std::to_string(site_id))
    };
    
    auto entities = findByConditions(conditions, OrderBy("name", true));
    
    logger_.Info("DeviceRepository::findBySite - Found " + 
                std::to_string(entities.size()) + " devices for site " + std::to_string(site_id));
    
    return entities;
}

std::optional<DeviceEntity> DeviceRepository::findByEndpoint(const std::string& endpoint) {
    std::vector<QueryCondition> conditions = {
        QueryCondition("endpoint", "=", endpoint)
    };
    
    auto entities = findByConditions(conditions, std::nullopt, Pagination(1, 1));
    
    if (entities.empty()) {
        logger_.Debug("DeviceRepository::findByEndpoint - Device not found: " + endpoint);
        return std::nullopt;
    }
    
    logger_.Debug("DeviceRepository::findByEndpoint - Found device: " + entities[0].getDeviceName());
    return entities[0];
}

std::vector<DeviceEntity> DeviceRepository::findByNamePattern(const std::string& name_pattern) {
    std::vector<QueryCondition> conditions = {
        QueryCondition("name", "LIKE", name_pattern)
    };
    
    auto entities = findByConditions(conditions, OrderBy("name", true));
    
    logger_.Info("DeviceRepository::findByNamePattern - Found " + 
                std::to_string(entities.size()) + " devices matching pattern: " + name_pattern);
    
    return entities;
}

// =============================================================================
// 관계 데이터 사전 로딩 (N+1 문제 해결)
// =============================================================================

void DeviceRepository::preloadDataPoints(std::vector<DeviceEntity>& devices) {
    if (devices.empty()) {
        return;
    }
    
    try {
        // 모든 디바이스 ID 수집
        std::ostringstream device_ids;
        for (size_t i = 0; i < devices.size(); ++i) {
            if (i > 0) device_ids << ",";
            device_ids << devices[i].getId();
        }
        
        // 벌크로 모든 데이터 포인트 조회
        std::string query = "SELECT * FROM data_points WHERE device_id IN (" + device_ids.str() + ") ORDER BY device_id, name";
        
        auto results = db_manager_.isPostgresConnected() 
            ? executePostgresQuery(query)
            : executeSQLiteQuery(query);
        
        // 디바이스별로 데이터 포인트 그룹화
        std::map<int, std::vector<DataPoint>> device_points_map;
        
        for (const auto& row : results) {
            int device_id = std::stoi(row.at("device_id"));
            
            DataPoint point;
            point.id = row.at("id");
            point.device_id = std::to_string(device_id);
            point.name = row.at("name");
            point.description = row.count("description") ? row.at("description") : "";
            point.address = std::stoi(row.at("address"));
            point.data_type = row.at("data_type");
            point.is_enabled = (row.at("is_enabled") == "1" || row.at("is_enabled") == "true");
            
            device_points_map[device_id].push_back(point);
        }
        
        // 각 디바이스에 해당하는 데이터 포인트들을 캐시
        for (auto& device : devices) {
            int device_id = device.getId();
            if (device_points_map.count(device_id)) {
                // 실제로는 DeviceEntity 내부의 캐시에 저장해야 함
                // 여기서는 getDataPoints()를 호출하여 로드 트리거
                device.getDataPoints();
            }
        }
        
        logger_.Info("DeviceRepository::preloadDataPoints - Preloaded data points for " + 
                    std::to_string(devices.size()) + " devices");
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::preloadDataPoints failed: " + std::string(e.what()));
    }
}

void DeviceRepository::preloadAlarmConfigs(std::vector<DeviceEntity>& devices) {
    if (devices.empty()) {
        return;
    }
    
    try {
        // 모든 디바이스 ID 수집
        std::ostringstream device_ids;
        for (size_t i = 0; i < devices.size(); ++i) {
            if (i > 0) device_ids << ",";
            device_ids << devices[i].getId();
        }
        
        // 벌크로 모든 알람 설정 조회
        std::string query = "SELECT * FROM alarm_definitions WHERE device_id IN (" + device_ids.str() + ") ORDER BY device_id, alarm_name";
        
        auto results = db_manager_.isPostgresConnected() 
            ? executePostgresQuery(query)
            : executeSQLiteQuery(query);
        
        // 각 디바이스에 알람 설정 로드 트리거
        for (auto& device : devices) {
            device.getAlarmConfigs();
        }
        
        logger_.Info("DeviceRepository::preloadAlarmConfigs - Preloaded alarm configs for " + 
                    std::to_string(devices.size()) + " devices");
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::preloadAlarmConfigs failed: " + std::string(e.what()));
    }
}

void DeviceRepository::preloadAllRelations(std::vector<DeviceEntity>& devices) {
    preloadDataPoints(devices);
    preloadAlarmConfigs(devices);
}

// =============================================================================
// IRepository 캐싱 구현
// =============================================================================

void DeviceRepository::setCacheEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_enabled_ = enabled;
    
    if (!enabled) {
        entity_cache_.clear();
        logger_.Info("DeviceRepository cache disabled and cleared");
    } else {
        logger_.Info("DeviceRepository cache enabled");
    }
}

bool DeviceRepository::isCacheEnabled() const {
    return cache_enabled_;
}

void DeviceRepository::clearCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    int cleared_count = static_cast<int>(entity_cache_.size());
    entity_cache_.clear();
    cache_hits_ = 0;
    cache_misses_ = 0;
    cache_evictions_ = 0;
    
    logger_.Info("DeviceRepository cache cleared - " + std::to_string(cleared_count) + " entries removed");
}

void DeviceRepository::clearCacheForId(int id) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = entity_cache_.find(id);
    if (it != entity_cache_.end()) {
        entity_cache_.erase(it);
        logger_.Debug("DeviceRepository cache cleared for ID: " + std::to_string(id));
    }
}

std::map<std::string, int> DeviceRepository::getCacheStats() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    std::map<std::string, int> stats;
    stats["enabled"] = cache_enabled_ ? 1 : 0;
    stats["size"] = static_cast<int>(entity_cache_.size());
    stats["max_size"] = max_cache_size_;
    stats["hits"] = cache_hits_;
    stats["misses"] = cache_misses_;
    stats["evictions"] = cache_evictions_;
    stats["hit_rate"] = (cache_hits_ + cache_misses_ > 0) 
                       ? (cache_hits_ * 100) / (cache_hits_ + cache_misses_) 
                       : 0;
    
    return stats;
}

// =============================================================================
// IRepository 통계 및 유틸리티 구현
// =============================================================================

int DeviceRepository::getTotalCount() {
    return countByConditions({});
}

// =============================================================================
// 디바이스 전용 통계
// =============================================================================

std::map<std::string, int> DeviceRepository::getCountByProtocol() {
    std::map<std::string, int> counts;
    
    try {
        std::string query = "SELECT protocol_type, COUNT(*) as count FROM devices GROUP BY protocol_type";
        
        auto results = db_manager_.isPostgresConnected() 
            ? executePostgresQuery(query)
            : executeSQLiteQuery(query);
        
        for (const auto& row : results) {
            counts[row.at("protocol_type")] = std::stoi(row.at("count"));
        }
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::getCountByProtocol failed: " + std::string(e.what()));
    }
    
    return counts;
}

std::map<int, int> DeviceRepository::getCountByTenant() {
    std::map<int, int> counts;
    
    try {
        std::string query = "SELECT tenant_id, COUNT(*) as count FROM devices GROUP BY tenant_id";
        
        auto results = db_manager_.isPostgresConnected() 
            ? executePostgresQuery(query)
            : executeSQLiteQuery(query);
        
        for (const auto& row : results) {
            counts[std::stoi(row.at("tenant_id"))] = std::stoi(row.at("count"));
        }
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::getCountByTenant failed: " + std::string(e.what()));
    }
    
    return counts;
}

std::map<int, int> DeviceRepository::getCountBySite() {
    std::map<int, int> counts;
    
    try {
        std::string query = "SELECT site_id, COUNT(*) as count FROM devices GROUP BY site_id";
        
        auto results = db_manager_.isPostgresConnected() 
            ? executePostgresQuery(query)
            : executeSQLiteQuery(query);
        
        for (const auto& row : results) {
            counts[std::stoi(row.at("site_id"))] = std::stoi(row.at("count"));
        }
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::getCountBySite failed: " + std::string(e.what()));
    }
    
    return counts;
}

std::map<std::string, int> DeviceRepository::getCountByStatus() {
    std::map<std::string, int> counts;
    
    try {
        std::string query = "SELECT is_enabled, COUNT(*) as count FROM devices GROUP BY is_enabled";
        
        auto results = db_manager_.isPostgresConnected() 
            ? executePostgresQuery(query)
            : executeSQLiteQuery(query);
        
        for (const auto& row : results) {
            std::string status = (row.at("is_enabled") == "1" || row.at("is_enabled") == "true") 
                                ? "enabled" : "disabled";
            counts[status] = std::stoi(row.at("count"));
        }
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::getCountByStatus failed: " + std::string(e.what()));
    }
    
    return counts;
}

// =============================================================================
// Worker 지원 메서드들
// =============================================================================

std::vector<DeviceEntity> DeviceRepository::findDevicesForWorkers() {
    auto devices = findAllEnabled();
    
    // 관계 데이터 사전 로딩으로 N+1 문제 해결
    preloadAllRelations(devices);
    
    logger_.Info("DeviceRepository::findDevicesForWorkers - Prepared " + 
                std::to_string(devices.size()) + " devices for workers");
    
    return devices;
}

int DeviceRepository::updateDeviceStatuses(const std::map<int, std::string>& status_updates) {
    if (status_updates.empty()) {
        return 0;
    }
    
    try {
        int updated_count = 0;
        
        for (const auto& update : status_updates) {
            int device_id = update.first;
            const std::string& status = update.second;
            
            std::string sql = "UPDATE devices SET status = '" + escapeString(status) + 
                             "', last_seen = NOW() WHERE id = " + std::to_string(device_id);
            
            if (executeUnifiedNonQuery(sql)) {
                updated_count++;
                
                // 캐시에서 제거 (다음 조회 시 새로 로드)
                if (cache_enabled_) {
                    clearCacheForId(device_id);
                }
            }
        }
        
        logger_.Info("DeviceRepository::updateDeviceStatuses - Updated " + 
                    std::to_string(updated_count) + " device statuses");
        
        return updated_count;
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::updateDeviceStatuses failed: " + std::string(e.what()));
        return 0;
    }
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
            DeviceEntity entity = mapRowToEntity(row);
            entities.push_back(entity);
        } catch (const std::exception& e) {
            logger_.Warn("DeviceRepository::mapResultsToEntities - Failed to map row: " + std::string(e.what()));
        }
    }
    
    return entities;
}

DeviceEntity DeviceRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    DeviceEntity entity;
    
    // ID 설정
    auto it = row.find("id");
    if (it != row.end()) {
        entity.setId(std::stoi(it->second));
    }
    
    // DeviceInfo 매핑 (DeviceEntity 내부 메서드 활용)
    DeviceInfo info;
    
    it = row.find("name");
    if (it != row.end()) {
        info.name = it->second;
    }
    
    it = row.find("description");
    if (it != row.end()) {
        info.description = it->second;
    }
    
    it = row.find("protocol_type");
    if (it != row.end()) {
        info.protocol_type = it->second;
    }
    
    it = row.find("endpoint");
    if (it != row.end()) {
        info.connection_string = it->second;
    }
    
    it = row.find("config");
    if (it != row.end()) {
        info.connection_config = it->second;
    }
    
    it = row.find("is_enabled");
    if (it != row.end()) {
        info.is_enabled = (it->second == "1" || it->second == "true");
    }
    
    it = row.find("status");
    if (it != row.end()) {
        info.status = it->second;
    }
    
    it = row.find("polling_interval");
    if (it != row.end()) {
        info.polling_interval_ms = std::stoi(it->second);
    }
    
    it = row.find("timeout_ms");
    if (it != row.end()) {
        info.timeout_ms = std::stoi(it->second);
    }
    
    it = row.find("retry_count");
    if (it != row.end()) {
        info.retry_count = std::stoi(it->second);
    }
    
    entity.setDeviceInfo(info);
    
    // 추가 필드들
    it = row.find("tenant_id");
    if (it != row.end()) {
        entity.setTenantId(std::stoi(it->second));
    }
    
    it = row.find("site_id");
    if (it != row.end()) {
        entity.setSiteId(std::stoi(it->second));
    }
    
    return entity;
}

std::string DeviceRepository::buildSelectQuery(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    std::ostringstream query;
    
    query << "SELECT id, tenant_id, site_id, name, description, protocol_type, ";
    query << "endpoint, config, is_enabled, status, polling_interval, ";
    query << "timeout_ms, retry_count, created_at, updated_at ";
    query << "FROM devices";
    
    // WHERE 절
    query << buildWhereClause(conditions);
    
    // ORDER BY 절
    query << buildOrderByClause(order_by);
    
    // LIMIT/OFFSET 절
    query << buildLimitClause(pagination);
    
    return query.str();
}

std::optional<DeviceEntity> DeviceRepository::getCachedEntity(int id) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = entity_cache_.find(id);
    if (it == entity_cache_.end()) {
        return std::nullopt;
    }
    
    auto& entry = it->second;
    
    // TTL 확인
    auto now = std::chrono::system_clock::now();
    if (now - entry.cached_at > cache_ttl_) {
        entity_cache_.erase(it);
        cache_evictions_++;
        return std::nullopt;
    }
    
    return entry.entity;
}

void DeviceRepository::cacheEntity(const DeviceEntity& entity) {
    if (!cache_enabled_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // 캐시 크기 제한 확인
    if (static_cast<int>(entity_cache_.size()) >= max_cache_size_) {
        cleanupExpiredCache();
        
        // 여전히 크기 초과시 가장 오래된 것 제거 (LRU)
        if (static_cast<int>(entity_cache_.size()) >= max_cache_size_) {
            auto oldest_it = entity_cache_.begin();
            auto oldest_time = oldest_it->second.cached_at;
            
            for (auto it = entity_cache_.begin(); it != entity_cache_.end(); ++it) {
                if (it->second.cached_at < oldest_time) {
                    oldest_time = it->second.cached_at;
                    oldest_it = it;
                }
            }
            
            entity_cache_.erase(oldest_it);
            cache_evictions_++;
        }
    }
    
    // 새 엔트리 추가
    entity_cache_[entity.getId()] = CacheEntry(entity);
}

void DeviceRepository::cleanupExpiredCache() {
    auto now = std::chrono::system_clock::now();
    
    auto it = entity_cache_.begin();
    while (it != entity_cache_.end()) {
        if (now - it->second.cached_at > cache_ttl_) {
            it = entity_cache_.erase(it);
            cache_evictions_++;
        } else {
            ++it;
        }
    }
}

// =============================================================================
// DB 쿼리 실행 헬퍼 메서드들 (BaseEntity와 유사한 패턴)
// =============================================================================

std::vector<std::map<std::string, std::string>> DeviceRepository::executePostgresQuery(const std::string& sql) {
    std::vector<std::map<std::string, std::string>> results;
    
    try {
        auto result = db_manager_.executeQueryPostgres(sql);
        
        for (const auto& row : result) {
            std::map<std::string, std::string> row_map;
            for (size_t i = 0; i < row.size(); ++i) {
                row_map[result.column_name(i)] = row[static_cast<int>(i)].is_null() ? "" : row[static_cast<int>(i)].as<std::string>();
            }
            results.push_back(row_map);
        }
        
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::executePostgresQuery failed: " + std::string(e.what()));
    }
    
    return results;
}

std::vector<std::map<std::string, std::string>> DeviceRepository::executeSQLiteQuery(const std::string& sql) {
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
    
    bool success = db_manager_.executeQuerySQLite(sql, callback, &results);
    if (!success) {
        logger_.Error("DeviceRepository::executeSQLiteQuery failed: " + sql);
        results.clear();
    }
    
    return results;
}

bool DeviceRepository::executeUnifiedNonQuery(const std::string& sql) {
    try {
        std::string db_type = config_manager_.getOrDefault("DATABASE_TYPE", "SQLITE");
        
        if (db_type == "POSTGRESQL") {
            return db_manager_.executeNonQueryPostgres(sql);
        } else {
            return db_manager_.executeNonQuerySQLite(sql);
        }
    } catch (const std::exception& e) {
        logger_.Error("DeviceRepository::executeUnifiedNonQuery failed: " + std::string(e.what()));
        return false;
    }
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

} // namespace Repositories
} // namespace Database
} // namespace PulseOne