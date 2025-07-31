// =============================================================================
// collector/src/Database/Repositories/DataPointRepository.cpp
// PulseOne 데이터포인트 Repository 구현 - IRepository 마이그레이션 완성본
// =============================================================================

#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/CurrentValueRepository.h" 
#include "Common/Structs.h"
#include "Common/Utils.h"
#include <sstream>
#include <iomanip>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// SQLite 쿼리 결과를 map으로 변환하는 콜백
// =============================================================================
struct SQLiteQueryResult {
    std::vector<std::map<std::string, std::string>> rows;
};

static int sqlite_callback(void* data, int argc, char** argv, char** column_names) {
    SQLiteQueryResult* result = static_cast<SQLiteQueryResult*>(data);
    std::map<std::string, std::string> row;
    
    for (int i = 0; i < argc; i++) {
        std::string value = argv[i] ? argv[i] : "";
        row[column_names[i]] = value;
    }
    
    result->rows.push_back(row);
    return 0;
}

// =============================================================================
// DatabaseManager 래퍼 메서드들 (기존 로직 그대로 유지)
// =============================================================================

std::vector<std::map<std::string, std::string>> DataPointRepository::executeDatabaseQuery(const std::string& sql) {
    try {
        std::string db_type = config_manager_->getOrDefault("DATABASE_TYPE", "SQLITE");
        
        if (db_type == "POSTGRESQL") {
            auto pq_result = db_manager_->executeQueryPostgres(sql);
            std::vector<std::map<std::string, std::string>> result;
            
            // PostgreSQL 결과 처리
            for (const auto& row : pq_result) {
                std::map<std::string, std::string> row_map;
                for (size_t i = 0; i < row.size(); ++i) {
                    std::string column_name = pq_result.column_name(static_cast<int>(i));
                    std::string value = row[static_cast<int>(i)].c_str();
                    row_map[column_name] = value;
                }
                result.push_back(row_map);
            }
            return result;
            
        } else {
            // SQLite 처리
            SQLiteQueryResult result;
            bool success = db_manager_->executeQuerySQLite(sql, sqlite_callback, &result);
            return success ? result.rows : std::vector<std::map<std::string, std::string>>{};
        }
        
    } catch (const std::exception& e) {
        logger_->Error("executeDatabaseQuery failed: " + std::string(e.what()));
        return {};
    }
}

bool DataPointRepository::executeDatabaseNonQuery(const std::string& sql) {
    try {
        std::string db_type = config_manager_->getOrDefault("DATABASE_TYPE", "SQLITE");
        
        if (db_type == "POSTGRESQL") {
            return db_manager_->executeNonQueryPostgres(sql);
        } else {
            return db_manager_->executeNonQuerySQLite(sql);
        }
        
    } catch (const std::exception& e) {
        logger_->Error("executeDatabaseNonQuery failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// IRepository 기본 CRUD 구현 (캐시 자동 적용)
// =============================================================================

std::vector<DataPointEntity> DataPointRepository::findAll() {
    try {
        std::string sql = "SELECT * FROM data_points ORDER BY device_id, address";
        auto result = executeDatabaseQuery(sql);
        auto entities = mapResultToEntities(result);
        
        logger_->Info("DataPointRepository::findAll - Found " + std::to_string(entities.size()) + " data points");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<DataPointEntity> DataPointRepository::findById(int id) {
    if (id <= 0) {
        logger_->Warn("DataPointRepository::findById - Invalid ID: " + std::to_string(id));
        return std::nullopt;
    }
    
    // 🔥 IRepository의 캐시 자동 확인
    auto cached = getCachedEntity(id);
    if (cached.has_value()) {
        logger_->Debug("DataPointRepository::findById - Cache hit for ID: " + std::to_string(id));
        return cached;
    }
    
    try {
        std::string sql = "SELECT * FROM data_points WHERE id = " + std::to_string(id);
        auto result = executeDatabaseQuery(sql);
        
        if (result.empty()) {
            logger_->Debug("DataPointRepository::findById - Data point not found: " + std::to_string(id));
            return std::nullopt;
        }
        
        DataPointEntity entity = mapRowToEntity(result[0]);
        
        // 🔥 IRepository의 캐시 자동 저장
        cacheEntity(entity);
        
        logger_->Debug("DataPointRepository::findById - Found data point: " + entity.getName());
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::findById failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool DataPointRepository::save(DataPointEntity& entity) {
    try {
        if (entity.getId() > 0) {
            return update(entity);
        }
        
        std::string sql = "INSERT INTO data_points (device_id, name, description, address, data_type, access_mode, "
                         "is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value, "
                         "log_enabled, log_interval_ms, log_deadband, tags, metadata, created_at) VALUES ("
                         + std::to_string(entity.getDeviceId()) + ", '"
                         + escapeString(entity.getName()) + "', '"
                         + escapeString(entity.getDescription()) + "', "
                         + std::to_string(entity.getAddress()) + ", '"
                         + escapeString(entity.getDataType()) + "', '"
                         + escapeString(entity.getAccessMode()) + "', "
                         + (entity.isEnabled() ? "1" : "0") + ", '"
                         + escapeString(entity.getUnit()) + "', "
                         + std::to_string(entity.getScalingFactor()) + ", "
                         + std::to_string(entity.getScalingOffset()) + ", "
                         + std::to_string(entity.getMinValue()) + ", "
                         + std::to_string(entity.getMaxValue()) + ", "
                         + (entity.isLogEnabled() ? "1" : "0") + ", "
                         + std::to_string(entity.getLogInterval()) + ", "
                         + std::to_string(entity.getLogDeadband()) + ", '"
                         + escapeString(tagsToString(entity.getTags())) + "', '"
                         + escapeString(entity.getMetadata().dump()) + "', '"
                         + PulseOne::Utils::TimestampToISOString(PulseOne::Utils::GetCurrentTimestamp()) + "')";
        
        bool success = executeDatabaseNonQuery(sql);
        
        if (success) {
            // ID 조회 (DB 타입별 처리)
            std::string db_type = config_manager_->getOrDefault("DATABASE_TYPE", "SQLITE");
            std::string id_query;
            
            if (db_type == "POSTGRESQL") {
                id_query = "SELECT lastval() as id";
            } else {
                id_query = "SELECT last_insert_rowid() as id";
            }
            
            auto id_result = executeDatabaseQuery(id_query);
            if (!id_result.empty()) {
                entity.setId(std::stoi(id_result[0].at("id")));
            }
            
            // 🔥 IRepository의 캐시 자동 저장
            cacheEntity(entity);
            
            logger_->Info("DataPointRepository::save - Saved and cached data point: " + entity.getName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::save failed: " + std::string(e.what()));
        return false;
    }
}

bool DataPointRepository::update(const DataPointEntity& entity) {
    try {
        std::string sql = "UPDATE data_points SET "
                         "device_id=" + std::to_string(entity.getDeviceId())
                         + ", name='" + escapeString(entity.getName()) + "'"
                         + ", description='" + escapeString(entity.getDescription()) + "'"
                         + ", address=" + std::to_string(entity.getAddress())
                         + ", data_type='" + escapeString(entity.getDataType()) + "'"
                         + ", access_mode='" + escapeString(entity.getAccessMode()) + "'"
                         + ", is_enabled=" + (entity.isEnabled() ? "1" : "0")
                         + ", unit='" + escapeString(entity.getUnit()) + "'"
                         + ", scaling_factor=" + std::to_string(entity.getScalingFactor())
                         + ", scaling_offset=" + std::to_string(entity.getScalingOffset())
                         + ", min_value=" + std::to_string(entity.getMinValue())
                         + ", max_value=" + std::to_string(entity.getMaxValue())
                         + ", log_enabled=" + (entity.isLogEnabled() ? "1" : "0")
                         + ", log_interval_ms=" + std::to_string(entity.getLogInterval())
                         + ", log_deadband=" + std::to_string(entity.getLogDeadband())
                         + ", tags='" + escapeString(tagsToString(entity.getTags())) + "'"
                         + ", metadata='" + escapeString(entity.getMetadata().dump()) + "'"
                         + ", updated_at='" + PulseOne::Utils::TimestampToISOString(PulseOne::Utils::GetCurrentTimestamp()) + "'"
                         + " WHERE id=" + std::to_string(entity.getId());
        
        bool success = executeDatabaseNonQuery(sql);
        
        // 🔥 IRepository의 캐시 자동 무효화
        if (success) {
            clearCacheForId(entity.getId());
            logger_->Info("DataPointRepository::update - Updated data point and cleared cache: " + entity.getName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::update failed: " + std::string(e.what()));
        return false;
    }
}

bool DataPointRepository::deleteById(int id) {
    try {
        std::string sql = "DELETE FROM data_points WHERE id = " + std::to_string(id);
        bool success = executeDatabaseNonQuery(sql);
        
        // 🔥 IRepository의 캐시 자동 제거
        if (success) {
            clearCacheForId(id);
            logger_->Info("DataPointRepository::deleteById - Deleted data point and cleared cache: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool DataPointRepository::exists(int id) {
    return findById(id).has_value();
}

// =============================================================================
// 벌크 연산 구현 (캐시 자동 적용)
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
        auto result = executeDatabaseQuery(sql);
        auto entities = mapResultToEntities(result);
        
        // 🔥 IRepository의 캐시 자동 저장
        for (const auto& entity : entities) {
            cacheEntity(entity);
        }
        
        logger_->Info("DataPointRepository::findByIds - Found " + 
                    std::to_string(entities.size()) + "/" + std::to_string(ids.size()) + " data points");
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::findByIds failed: " + std::string(e.what()));
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
        
        auto result = executeDatabaseQuery(sql);
        auto entities = mapResultToEntities(result);
        
        logger_->Debug("DataPointRepository::findByConditions - Found " + 
                     std::to_string(entities.size()) + " data points");
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::findByConditions failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<DataPointEntity> DataPointRepository::findFirstByConditions(
    const std::vector<QueryCondition>& conditions) {
    
    auto results = findByConditions(conditions, std::nullopt, Pagination(1, 0));
    return results.empty() ? std::nullopt : std::make_optional(results[0]);
}

int DataPointRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        std::string sql = "SELECT COUNT(*) as count FROM data_points";
        sql += buildWhereClause(conditions);
        
        auto result = executeDatabaseQuery(sql);
        if (!result.empty()) {
            return std::stoi(result[0].at("count"));
        }
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::countByConditions failed: " + std::string(e.what()));
    }
    
    return 0;
}

int DataPointRepository::saveBulk(std::vector<DataPointEntity>& entities) {
    int saved_count = 0;
    for (auto& entity : entities) {
        if (save(entity)) {
            saved_count++;
            // save() 메서드에서 이미 캐시 처리됨
        }
    }
    
    logger_->Info("DataPointRepository::saveBulk - Saved " + std::to_string(saved_count) + " data points");
    return saved_count;
}

int DataPointRepository::updateBulk(const std::vector<DataPointEntity>& entities) {
    int updated_count = 0;
    for (const auto& entity : entities) {
        if (update(entity)) {
            updated_count++;
            // update() 메서드에서 이미 캐시 무효화됨
        }
    }
    
    logger_->Info("DataPointRepository::updateBulk - Updated " + std::to_string(updated_count) + " data points");
    return updated_count;
}

int DataPointRepository::deleteByIds(const std::vector<int>& ids) {
    int deleted_count = 0;
    for (int id : ids) {
        if (deleteById(id)) {
            deleted_count++;
            // deleteById() 메서드에서 이미 캐시 제거됨
        }
    }
    
    logger_->Info("DataPointRepository::deleteByIds - Deleted " + std::to_string(deleted_count) + " data points");
    return deleted_count;
}

// =============================================================================
// IRepository 캐시 관리 (자동 위임)
// =============================================================================

void DataPointRepository::setCacheEnabled(bool enabled) {
    // 🔥 IRepository의 캐시 관리 위임
    IRepository<DataPointEntity>::setCacheEnabled(enabled);
    logger_->Info("DataPointRepository cache " + std::string(enabled ? "enabled" : "disabled"));
}

bool DataPointRepository::isCacheEnabled() const {
    // 🔥 IRepository의 캐시 상태 위임
    return IRepository<DataPointEntity>::isCacheEnabled();
}

void DataPointRepository::clearCache() {
    // 🔥 IRepository의 캐시 클리어 위임
    IRepository<DataPointEntity>::clearCache();
    logger_->Info("DataPointRepository cache cleared");
}

void DataPointRepository::clearCacheForId(int id) {
    // 🔥 IRepository의 개별 캐시 클리어 위임
    IRepository<DataPointEntity>::clearCacheForId(id);
    logger_->Debug("DataPointRepository cache cleared for ID: " + std::to_string(id));
}

std::map<std::string, int> DataPointRepository::getCacheStats() const {
    // 🔥 IRepository의 캐시 통계 위임
    return IRepository<DataPointEntity>::getCacheStats();
}

int DataPointRepository::getTotalCount() {
    return countByConditions({});
}

// =============================================================================
// DataPoint 전용 메서드들 (기존 로직 그대로 유지)
// =============================================================================

std::vector<DataPointEntity> DataPointRepository::findAllWithLimit(size_t limit) {
    if (limit == 0) return findAll();
    
    try {
        std::string sql = "SELECT * FROM data_points ORDER BY id LIMIT " + std::to_string(limit);
        auto result = executeDatabaseQuery(sql);
        return mapResultToEntities(result);
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::findAllWithLimit failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<DataPointEntity> DataPointRepository::findByDeviceId(int device_id, bool enabled_only) {
    std::vector<QueryCondition> conditions;
    conditions.emplace_back("device_id", "=", std::to_string(device_id));
    
    if (enabled_only) {
        conditions.emplace_back("is_enabled", "=", "1");
    }
    
    auto entities = findByConditions(conditions);
    
    logger_->Info("DataPointRepository::findByDeviceId - Found " + 
                std::to_string(entities.size()) + " data points for device " + std::to_string(device_id));
    
    return entities;
}

std::vector<DataPointEntity> DataPointRepository::findByDeviceIds(const std::vector<int>& device_ids, bool enabled_only) {
    if (device_ids.empty()) return {};
    
    std::string id_list;
    for (size_t i = 0; i < device_ids.size(); ++i) {
        if (i > 0) id_list += ",";
        id_list += std::to_string(device_ids[i]);
    }
    
    std::vector<QueryCondition> conditions;
    conditions.emplace_back("device_id", "IN", "(" + id_list + ")");
    
    if (enabled_only) {
        conditions.emplace_back("is_enabled", "=", "1");
    }
    
    return findByConditions(conditions);
}

std::vector<DataPointEntity> DataPointRepository::findWritablePoints() {
    std::vector<QueryCondition> conditions;
    conditions.emplace_back("access_mode", "IN", "('write','read_write')");
    return findByConditions(conditions);
}

std::vector<DataPointEntity> DataPointRepository::findByDataType(const std::string& data_type) {
    return findByConditions({QueryCondition("data_type", "=", "'" + data_type + "'")});
}

std::vector<DataPointEntity> DataPointRepository::findDataPointsForWorkers(const std::vector<int>& device_ids) {
    if (device_ids.empty()) {
        return findByConditions({QueryCondition("is_enabled", "=", "1")});
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
    return findByConditions({QueryCondition("tags", "LIKE", "'%" + tag + "%'")});
}

std::vector<DataPointEntity> DataPointRepository::findDisabledPoints() {
    return findByConditions({QueryCondition("is_enabled", "=", "0")});
}

std::vector<DataPointEntity> DataPointRepository::findRecentlyCreated(int days) {
    (void)days;
    try {
        std::string sql = "SELECT * FROM data_points ORDER BY id DESC LIMIT 100";
        auto result = executeDatabaseQuery(sql);
        return mapResultToEntities(result);
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::findRecentlyCreated failed: " + std::string(e.what()));
        return {};
    }
}

// =============================================================================
// 관계 데이터 사전 로딩 (기본 구현)
// =============================================================================

void DataPointRepository::preloadDeviceInfo(std::vector<DataPointEntity>& data_points) {
    // 기본 구현: 향후 DeviceRepository와 연계하여 구현
    (void)data_points;
    logger_->Debug("DataPointRepository::preloadDeviceInfo - Not implemented yet");
}

void DataPointRepository::preloadCurrentValues(std::vector<DataPointEntity>& data_points) {
    // 기본 구현: 향후 실시간 데이터 저장소와 연계하여 구현
    (void)data_points;
    logger_->Debug("DataPointRepository::preloadCurrentValues - Not implemented yet");
}

void DataPointRepository::preloadAlarmConfigs(std::vector<DataPointEntity>& data_points) {
    // 기본 구현: 향후 AlarmConfigRepository와 연계하여 구현
    (void)data_points;
    logger_->Debug("DataPointRepository::preloadAlarmConfigs - Not implemented yet");
}

// =============================================================================
// 통계 메서드들 (기존 로직 그대로 유지)
// =============================================================================

std::map<int, int> DataPointRepository::getPointCountByDevice() {
    std::map<int, int> counts;
    
    try {
        auto result = executeDatabaseQuery(
            "SELECT device_id, COUNT(*) as count FROM data_points GROUP BY device_id");
        
        for (const auto& row : result) {
            counts[std::stoi(row.at("device_id"))] = std::stoi(row.at("count"));
        }
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::getPointCountByDevice failed: " + std::string(e.what()));
    }
    
    return counts;
}

std::map<std::string, int> DataPointRepository::getPointCountByDataType() {
    std::map<std::string, int> counts;
    
    try {
        auto result = executeDatabaseQuery(
            "SELECT data_type, COUNT(*) as count FROM data_points GROUP BY data_type");
        
        for (const auto& row : result) {
            counts[row.at("data_type")] = std::stoi(row.at("count"));
        }
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::getPointCountByDataType failed: " + std::string(e.what()));
    }
    
    return counts;
}

// =============================================================================
// 내부 헬퍼 메서드들 (기존 로직 그대로 유지)
// =============================================================================

DataPointEntity DataPointRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    DataPointEntity entity;
    
    try {
        // 기본 필드들
        entity.setId(std::stoi(row.at("id")));
        entity.setDeviceId(std::stoi(row.at("device_id")));
        entity.setName(row.at("name"));
        entity.setDescription(row.at("description"));
        entity.setAddress(std::stoi(row.at("address")));
        entity.setDataType(row.at("data_type"));
        entity.setAccessMode(row.at("access_mode"));
        entity.setEnabled(row.at("is_enabled") == "1");
        
        // 엔지니어링 정보
        if (row.count("unit")) entity.setUnit(row.at("unit"));
        if (row.count("scaling_factor")) entity.setScalingFactor(std::stod(row.at("scaling_factor")));
        if (row.count("scaling_offset")) entity.setScalingOffset(std::stod(row.at("scaling_offset")));
        if (row.count("min_value")) entity.setMinValue(std::stod(row.at("min_value")));
        if (row.count("max_value")) entity.setMaxValue(std::stod(row.at("max_value")));
        
        // 로깅 설정
        if (row.count("log_enabled")) entity.setLogEnabled(row.at("log_enabled") == "1");
        if (row.count("log_interval_ms")) entity.setLogInterval(std::stoi(row.at("log_interval_ms")));
        if (row.count("log_deadband")) entity.setLogDeadband(std::stod(row.at("log_deadband")));
        
        // 태그 파싱
        if (row.count("tags")) {
            std::string tags_str = row.at("tags");
            std::vector<std::string> tags = parseTagsFromString(tags_str);
            entity.setTags(tags);
        }
        
        // 메타데이터 파싱
        if (row.count("metadata") && !row.at("metadata").empty()) {
            try {
                json metadata = json::parse(row.at("metadata"));
                entity.setMetadata(metadata);
            } catch (const std::exception&) {
                entity.setMetadata(json::object());
            }
        }
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::mapRowToEntity failed: " + std::string(e.what()));
    }
    
    return entity;
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

// =============================================================================
// SQL 빌더 헬퍼 메서드들 (기존 로직 그대로 유지)
// =============================================================================

std::string DataPointRepository::buildWhereClause(const std::vector<QueryCondition>& conditions) const {
    if (conditions.empty()) return "";
    
    std::string clause = " WHERE ";
    for (size_t i = 0; i < conditions.size(); ++i) {
        if (i > 0) clause += " AND ";
        clause += conditions[i].field + " " + conditions[i].operation + " " + conditions[i].value;
    }
    return clause;
}

std::string DataPointRepository::buildOrderByClause(const std::optional<OrderBy>& order_by) const {
    if (!order_by.has_value()) return "";
    return " ORDER BY " + order_by->field + (order_by->ascending ? " ASC" : " DESC");
}

std::string DataPointRepository::buildLimitClause(const std::optional<Pagination>& pagination) const {
    if (!pagination.has_value()) return "";
    return " LIMIT " + std::to_string(pagination->getLimit()) + 
           " OFFSET " + std::to_string(pagination->getOffset());
}

// =============================================================================
// 유틸리티 함수들 (기존 로직 그대로 유지)
// =============================================================================

std::string DataPointRepository::tagsToString(const std::vector<std::string>& tags) {
    if (tags.empty()) return "";
    
    std::ostringstream oss;
    for (size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) oss << ",";
        oss << tags[i];
    }
    return oss.str();
}

std::vector<std::string> DataPointRepository::parseTagsFromString(const std::string& tags_str) {
    std::vector<std::string> tags;
    if (tags_str.empty()) return tags;
    
    std::istringstream iss(tags_str);
    std::string tag;
    while (std::getline(iss, tag, ',')) {
        if (!tag.empty()) {
            tags.push_back(tag);
        }
    }
    return tags;
}

std::string DataPointRepository::escapeString(const std::string& str) {
    std::string escaped = str;
    size_t pos = 0;
    while ((pos = escaped.find("'", pos)) != std::string::npos) {
        escaped.replace(pos, 1, "''");
        pos += 2;
    }
    return escaped;
}

// CurrentValueRepository 의존성 주입 구현
void DataPointRepository::setCurrentValueRepository(std::shared_ptr<CurrentValueRepository> current_value_repo) {
    current_value_repo_ = current_value_repo;
    logger_->Info("✅ CurrentValueRepository injected into DataPointRepository");
}

// 🔥 핵심 메서드: 현재값이 포함된 완성된 DataPoint 조회
std::vector<PulseOne::Structs::DataPoint> DataPointRepository::getDataPointsWithCurrentValues(int device_id, bool enabled_only) {
    std::vector<PulseOne::Structs::DataPoint> result;
    
    try {
        logger_->Debug("🔍 Loading DataPoints with current values for device: " + std::to_string(device_id));
        
        // 1. DataPointEntity들 조회 (기존 메서드 재사용)
        auto entities = findByDeviceId(device_id, enabled_only);
        
        logger_->Debug("📊 Found " + std::to_string(entities.size()) + " DataPoint entities");
        
        // 2. 각 Entity를 Structs::DataPoint로 변환 + 현재값 추가
        for (const auto& entity : entities) {
            
            // 🎯 핵심: Entity의 toDataPointStruct() 메서드 활용
            PulseOne::Structs::DataPoint data_point = entity.toDataPointStruct();
            
            // =======================================================================
            // 🔥 현재값 조회 및 설정 (Repository 패턴 준수)
            // =======================================================================
            
            if (current_value_repo_) {
                try {
                    // CurrentValueRepository에서 현재값 조회
                    auto current_value = current_value_repo_->findByDataPointId(entity.getId());
                    
                    if (current_value.has_value()) {
                        // 현재값이 존재하는 경우
                        data_point.current_value = PulseOne::BasicTypes::DataVariant(current_value->getValue());
                        data_point.quality_code = current_value->getQuality();
                        data_point.quality_timestamp = current_value->getTimestamp();
                        
                        logger_->Debug("💡 Current value loaded: " + data_point.name + 
                                      " = " + data_point.GetCurrentValueAsString() + 
                                      " (Quality: " + data_point.GetQualityCodeAsString() + ")");
                    } else {
                        // 현재값이 없는 경우 기본값 설정
                        data_point.current_value = PulseOne::BasicTypes::DataVariant(0.0);
                        data_point.quality_code = PulseOne::Enums::DataQuality::NOT_CONNECTED;
                        data_point.quality_timestamp = std::chrono::system_clock::now();
                        
                        logger_->Debug("⚠️ No current value found for: " + data_point.name + " (using defaults)");
                    }
                    
                } catch (const std::exception& e) {
                    logger_->Debug("❌ Current value query failed for " + data_point.name + ": " + std::string(e.what()));
                    
                    // 에러 시 BAD 품질로 설정
                    data_point.current_value = PulseOne::BasicTypes::DataVariant(0.0);
                    data_point.quality_code = PulseOne::Enums::DataQuality::BAD;
                    data_point.quality_timestamp = std::chrono::system_clock::now();
                }
            } else {
                // CurrentValueRepository가 주입되지 않은 경우
                logger_->Warn("⚠️ CurrentValueRepository not injected, using default values");
                
                data_point.current_value = PulseOne::BasicTypes::DataVariant(0.0);
                data_point.quality_code = PulseOne::Enums::DataQuality::NOT_CONNECTED;
                data_point.quality_timestamp = std::chrono::system_clock::now();
            }
            
            // 3. 주소 필드 동기화 (기존 메서드 활용)
            data_point.SyncAddressFields();
            
            // 4. Worker 컨텍스트 정보 추가
            try {
                auto worker_context = entity.getWorkerContext();
                if (!worker_context.empty()) {
                    data_point.metadata = worker_context;  // JSON을 metadata에 직접 할당
                }
            } catch (const std::exception& e) {
                logger_->Debug("Worker context not available: " + std::string(e.what()));
            }
            
            result.push_back(data_point);
            
            // ✅ 풍부한 로깅 - 새로운 필드들 포함
            logger_->Debug("✅ Converted DataPoint: " + data_point.name + 
                          " (Address: " + std::to_string(data_point.address) + 
                          ", Type: " + data_point.data_type + 
                          ", Writable: " + (data_point.IsWritable() ? "true" : "false") + 
                          ", LogEnabled: " + (data_point.log_enabled ? "true" : "false") + 
                          ", LogInterval: " + std::to_string(data_point.log_interval_ms) + "ms" + 
                          ", CurrentValue: " + data_point.GetCurrentValueAsString() + 
                          ", Quality: " + data_point.GetQualityCodeAsString() + ")");
        }
        
        logger_->Info("✅ Successfully loaded " + std::to_string(result.size()) + 
                     " complete data points for device: " + std::to_string(device_id));
        
        // =======================================================================
        // 🔥 통계 출력 - 완성된 필드들 활용
        // =======================================================================
        int writable_count = 0;
        int log_enabled_count = 0;
        int good_quality_count = 0;
        int connected_count = 0;
        
        for (const auto& dp : result) {
            if (dp.IsWritable()) writable_count++;
            if (dp.log_enabled) log_enabled_count++;
            if (dp.IsGoodQuality()) good_quality_count++;
            if (dp.quality_code != PulseOne::Enums::DataQuality::NOT_CONNECTED) connected_count++;
        }
        
        logger_->Debug("📊 DataPoint Statistics: " + 
                      std::to_string(writable_count) + " writable, " + 
                      std::to_string(log_enabled_count) + " log-enabled, " + 
                      std::to_string(good_quality_count) + " good-quality, " + 
                      std::to_string(connected_count) + " connected");
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::getDataPointsWithCurrentValues failed: " + std::string(e.what()));
    }
    
    return result;
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne