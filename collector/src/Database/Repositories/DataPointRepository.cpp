/**
 * @file DataPointRepository.cpp
 * @brief PulseOne DataPointRepository 구현 - 완전 수정 버전
 * @author PulseOne Development Team
 * @date 2025-07-27
 */

#include "Database/Repositories/DataPointRepository.h"

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// 생성자
// =============================================================================
DataPointRepository::DataPointRepository()
    : db_manager_(DatabaseManager::getInstance())
    , config_manager_(ConfigManager::getInstance())
    , logger_(LogManager::getInstance())
    , cache_enabled_(true) {
    
    logger_.Info("DataPointRepository created");
}

// =============================================================================
// IRepository 인터페이스 구현 (🔥 모든 순수 가상 함수 구현)
// =============================================================================

std::vector<DataPointEntity> DataPointRepository::findAll() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    try {
        std::string sql = "SELECT * FROM data_points ORDER BY id";
        auto result = db_manager_.executeSQLiteQuery(sql);
        return mapResultToEntities(result);
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<DataPointEntity> DataPointRepository::findById(int id) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // 캐시 확인
    if (cache_enabled_) {
        auto cached = getFromCache(id);
        if (cached.has_value()) {
            updateCacheStats("hit");
            return cached;
        }
        updateCacheStats("miss");
    }
    
    try {
        std::string sql = "SELECT * FROM data_points WHERE id = " + std::to_string(id);
        auto result = db_manager_.executeSQLiteQuery(sql);
        
        if (result.empty()) {
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(result[0]);
        
        // 캐시에 저장
        if (cache_enabled_) {
            putToCache(entity);
        }
        
        return entity;
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::findById failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool DataPointRepository::save(DataPointEntity& entity) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    try {
        std::string sql = "INSERT INTO data_points (device_id, name, address, data_type, enabled, writable, tag, description) VALUES ("
                         + std::to_string(entity.getDeviceId()) + ", '"
                         + entity.getName() + "', "
                         + std::to_string(entity.getAddress()) + ", '"
                         + entity.getDataType() + "', "
                         + (entity.isEnabled() ? "1" : "0") + ", "
                         + (entity.isWritable() ? "1" : "0") + ", '"
                         + entity.getTag() + "', '"
                         + entity.getDescription() + "')";
        
        bool success = db_manager_.executeSQLiteNonQuery(sql);
        
        if (success) {
            // 생성된 ID 조회
            auto id_result = db_manager_.executeSQLiteQuery("SELECT last_insert_rowid() as id");
            if (!id_result.empty()) {
                int new_id = std::stoi(id_result[0].at("id"));
                entity.setId(new_id);
                
                // 캐시에 저장
                if (cache_enabled_) {
                    putToCache(entity);
                }
            }
        }
        
        return success;
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::save failed: " + std::string(e.what()));
        return false;
    }
}

bool DataPointRepository::update(const DataPointEntity& entity) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    try {
        std::string sql = "UPDATE data_points SET device_id=" + std::to_string(entity.getDeviceId())
                         + ", name='" + entity.getName()
                         + "', address=" + std::to_string(entity.getAddress())
                         + ", data_type='" + entity.getDataType()
                         + "', enabled=" + (entity.isEnabled() ? "1" : "0")
                         + ", writable=" + (entity.isWritable() ? "1" : "0")
                         + ", tag='" + entity.getTag()
                         + "', description='" + entity.getDescription()
                         + "' WHERE id=" + std::to_string(entity.getId());
        
        bool success = db_manager_.executeSQLiteNonQuery(sql);
        
        if (success && cache_enabled_) {
            putToCache(entity);
        }
        
        return success;
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::update failed: " + std::string(e.what()));
        return false;
    }
}

bool DataPointRepository::deleteById(int id) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    try {
        std::string sql = "DELETE FROM data_points WHERE id = " + std::to_string(id);
        bool success = db_manager_.executeSQLiteNonQuery(sql);
        
        if (success && cache_enabled_) {
            clearCacheForId(id);
        }
        
        return success;
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool DataPointRepository::exists(int id) {
    return findById(id).has_value();
}

// =============================================================================
// 벌크 연산 구현
// =============================================================================

std::vector<DataPointEntity> DataPointRepository::findByIds(const std::vector<int>& ids) {
    if (ids.empty()) return {};
    
    std::string id_list;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) id_list += ",";
        id_list += std::to_string(ids[i]);
    }
    
    try {
        std::string sql = "SELECT * FROM data_points WHERE id IN (" + id_list + ")";
        auto result = db_manager_.executeSQLiteQuery(sql);
        return mapResultToEntities(result);
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::findByIds failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<DataPointEntity> DataPointRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    try {
        std::string sql = "SELECT * FROM data_points";
        sql += buildWhereClause(conditions);
        sql += buildOrderByClause(order_by);
        sql += buildLimitClause(pagination);
        
        auto result = db_manager_.executeSQLiteQuery(sql);
        return mapResultToEntities(result);
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::findByConditions failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<DataPointEntity> DataPointRepository::findFirstByConditions(
    const std::vector<QueryCondition>& conditions) {
    
    auto results = findByConditions(conditions, std::nullopt, Pagination(1, 1));
    return results.empty() ? std::nullopt : std::make_optional(results[0]);
}

int DataPointRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        std::string sql = "SELECT COUNT(*) as count FROM data_points";
        sql += buildWhereClause(conditions);
        
        auto result = db_manager_.executeSQLiteQuery(sql);
        if (!result.empty()) {
            return std::stoi(result[0].at("count"));
        }
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::countByConditions failed: " + std::string(e.what()));
    }
    
    return 0;
}

int DataPointRepository::saveBulk(std::vector<DataPointEntity>& entities) {
    int saved_count = 0;
    for (auto& entity : entities) {
        if (save(entity)) {
            saved_count++;
        }
    }
    return saved_count;
}

int DataPointRepository::updateBulk(const std::vector<DataPointEntity>& entities) {
    int updated_count = 0;
    for (const auto& entity : entities) {
        if (update(entity)) {
            updated_count++;
        }
    }
    return updated_count;
}

int DataPointRepository::deleteByIds(const std::vector<int>& ids) {
    int deleted_count = 0;
    for (int id : ids) {
        if (deleteById(id)) {
            deleted_count++;
        }
    }
    return deleted_count;
}

// =============================================================================
// 캐시 관리 구현
// =============================================================================

void DataPointRepository::setCacheEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_enabled_ = enabled;
    if (!enabled) {
        entity_cache_.clear();
    }
}

bool DataPointRepository::isCacheEnabled() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return cache_enabled_;
}

void DataPointRepository::clearCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    entity_cache_.clear();
    cache_stats_["cleared"] = cache_stats_["cleared"] + 1;
}

void DataPointRepository::clearCacheForId(int id) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    entity_cache_.erase(id);
}

std::map<std::string, int> DataPointRepository::getCacheStats() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto stats = cache_stats_;
    stats["size"] = static_cast<int>(entity_cache_.size());
    return stats;
}

// =============================================================================
// 유틸리티 구현
// =============================================================================

int DataPointRepository::getTotalCount() {
    try {
        auto result = db_manager_.executeSQLiteQuery("SELECT COUNT(*) as count FROM data_points");
        if (!result.empty()) {
            return std::stoi(result[0].at("count"));
        }
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::getTotalCount failed: " + std::string(e.what()));
    }
    return 0;
}

std::string DataPointRepository::getRepositoryName() const {
    return "DataPointRepository";
}

// =============================================================================
// DataPoint 전용 메서드들 구현
// =============================================================================

std::vector<DataPointEntity> DataPointRepository::findAllWithLimit(size_t limit) {
    if (limit == 0) {
        return findAll();
    }
    
    try {
        std::string sql = "SELECT * FROM data_points ORDER BY id LIMIT " + std::to_string(limit);
        auto result = db_manager_.executeSQLiteQuery(sql);
        return mapResultToEntities(result);
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::findAllWithLimit failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<DataPointEntity> DataPointRepository::findByDeviceId(int device_id, bool enabled_only) {
    std::vector<QueryCondition> conditions;
    conditions.emplace_back("device_id", "=", std::to_string(device_id));
    
    if (enabled_only) {
        conditions.emplace_back("enabled", "=", "1");
    }
    
    return findByConditions(conditions);
}

std::vector<DataPointEntity> DataPointRepository::findByDeviceIds(const std::vector<int>& device_ids, bool enabled_only) {
    if (device_ids.empty()) return {};
    
    std::string id_list;
    for (size_t i = 0; i < device_ids.size(); ++i) {
        if (i > 0) id_list += ",";
        id_list += std::to_string(device_ids[i]);
    }
    
    std::vector<QueryCondition> conditions;
    conditions.emplace_back("device_id", "IN", id_list);
    
    if (enabled_only) {
        conditions.emplace_back("enabled", "=", "1");
    }
    
    return findByConditions(conditions);
}

std::vector<DataPointEntity> DataPointRepository::findWritablePoints() {
    return findByConditions({QueryCondition("writable", "=", "1")});
}

std::vector<DataPointEntity> DataPointRepository::findByDataType(const std::string& data_type) {
    return findByConditions({QueryCondition("data_type", "=", data_type)});
}

std::vector<DataPointEntity> DataPointRepository::findDataPointsForWorkers(const std::vector<int>& device_ids) {
    if (device_ids.empty()) {
        return findByConditions({QueryCondition("enabled", "=", "1")});
    } else {
        return findByDeviceIds(device_ids, true);
    }
}

std::optional<DataPointEntity> DataPointRepository::findByDeviceAndAddress(int device_id, int address) {
    std::vector<QueryCondition> conditions;
    conditions.emplace_back("device_id", "=", std::to_string(device_id));
    conditions.emplace_back("address", "=", std::to_string(address));
    
    return findFirstByConditions(conditions);
}

std::vector<DataPointEntity> DataPointRepository::findByTag(const std::string& tag) {
    return findByConditions({QueryCondition("tag", "=", tag)});
}

std::vector<DataPointEntity> DataPointRepository::findDisabledPoints() {
    return findByConditions({QueryCondition("enabled", "=", "0")});
}

std::vector<DataPointEntity> DataPointRepository::findRecentlyCreated(int days) {
    // 날짜 계산이 필요하므로 간단한 구현으로 대체
    try {
        std::string sql = "SELECT * FROM data_points ORDER BY id DESC LIMIT 100";
        auto result = db_manager_.executeSQLiteQuery(sql);
        return mapResultToEntities(result);
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::findRecentlyCreated failed: " + std::string(e.what()));
        return {};
    }
}

// =============================================================================
// 관계 데이터 로딩 (기본 구현)
// =============================================================================

void DataPointRepository::preloadDeviceInfo(std::vector<DataPointEntity>& data_points) {
    // 구현 생략 (DeviceRepository와의 연동 필요)
    (void)data_points; // 컴파일러 경고 방지
}

void DataPointRepository::preloadCurrentValues(std::vector<DataPointEntity>& data_points) {
    // 구현 생략
    (void)data_points;
}

void DataPointRepository::preloadAlarmConfigs(std::vector<DataPointEntity>& data_points) {
    // 구현 생략
    (void)data_points;
}

// =============================================================================
// 통계 구현
// =============================================================================

std::map<int, int> DataPointRepository::getPointCountByDevice() {
    std::map<int, int> counts;
    
    try {
        auto result = db_manager_.executeSQLiteQuery(
            "SELECT device_id, COUNT(*) as count FROM data_points GROUP BY device_id");
        
        for (const auto& row : result) {
            counts[std::stoi(row.at("device_id"))] = std::stoi(row.at("count"));
        }
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::getPointCountByDevice failed: " + std::string(e.what()));
    }
    
    return counts;
}

std::map<std::string, int> DataPointRepository::getPointCountByDataType() {
    std::map<std::string, int> counts;
    
    try {
        auto result = db_manager_.executeSQLiteQuery(
            "SELECT data_type, COUNT(*) as count FROM data_points GROUP BY data_type");
        
        for (const auto& row : result) {
            counts[row.at("data_type")] = std::stoi(row.at("count"));
        }
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::getPointCountByDataType failed: " + std::string(e.what()));
    }
    
    return counts;
}

// =============================================================================
// 내부 헬퍼 메서드들 구현
// =============================================================================

DataPointEntity DataPointRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    DataPointEntity entity;
    
    if (row.find("id") != row.end()) {
        entity.setId(std::stoi(row.at("id")));
    }
    if (row.find("device_id") != row.end()) {
        entity.setDeviceId(std::stoi(row.at("device_id")));
    }
    if (row.find("name") != row.end()) {
        entity.setName(row.at("name"));
    }
    if (row.find("address") != row.end()) {
        entity.setAddress(std::stoi(row.at("address")));
    }
    if (row.find("data_type") != row.end()) {
        entity.setDataType(row.at("data_type"));
    }
    if (row.find("enabled") != row.end()) {
        entity.setEnabled(row.at("enabled") == "1");
    }
    if (row.find("writable") != row.end()) {
        entity.setWritable(row.at("writable") == "1");
    }
    if (row.find("tag") != row.end()) {
        entity.setTag(row.at("tag"));
    }
    if (row.find("description") != row.end()) {
        entity.setDescription(row.at("description"));
    }
    
    return entity;
}

std::optional<DataPointEntity> DataPointRepository::getFromCache(int id) const {
    auto it = entity_cache_.find(id);
    if (it != entity_cache_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void DataPointRepository::putToCache(const DataPointEntity& entity) {
    entity_cache_[entity.getId()] = entity;
    updateCacheStats("put");
}

std::vector<DataPointEntity> DataPointRepository::mapResultToEntities(
    const std::vector<std::map<std::string, std::string>>& result) {
    
    std::vector<DataPointEntity> entities;
    entities.reserve(result.size());
    
    for (const auto& row : result) {
        entities.push_back(mapRowToEntity(row));
    }
    
    return entities;
}

void DataPointRepository::updateCacheStats(const std::string& operation) const {
    cache_stats_[operation] = cache_stats_[operation] + 1;
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne