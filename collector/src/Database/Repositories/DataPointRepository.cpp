// DataPointRepository.cpp - 올바른 DatabaseManager 메서드 사용

/**
 * @file DataPointRepository.cpp  
 * @brief PulseOne DataPointRepository 구현 - DatabaseManager 실제 메서드 사용
 * @author PulseOne Development Team
 * @date 2025-07-27
 */

#include "Database/Repositories/DataPointRepository.h"
#include <sstream>
#include <iomanip>

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
// DatabaseManager 래퍼 메서드들
// =============================================================================

std::vector<std::map<std::string, std::string>> DataPointRepository::executeDatabaseQuery(const std::string& sql) {
    try {
        std::string db_type = config_manager_.getOrDefault("DATABASE_TYPE", "SQLITE");
        
        if (db_type == "POSTGRESQL") {
            auto pq_result = db_manager_.executeQueryPostgres(sql);
            std::vector<std::map<std::string, std::string>> result;
            
            // 🔥 수정: pqxx::row 접근 방법 수정
            for (const auto& row : pq_result) {
                std::map<std::string, std::string> row_map;
                for (size_t i = 0; i < row.size(); ++i) {
                    std::string column_name = pq_result.column_name(static_cast<int>(i));
                    std::string value = row[static_cast<int>(i)].c_str();  // 🔥 수정: int로 캐스팅
                    row_map[column_name] = value;
                }
                result.push_back(row_map);
            }
            return result;
            
        } else {
            // SQLite 처리
            SQLiteQueryResult result;
            bool success = db_manager_.executeQuerySQLite(sql, sqlite_callback, &result);
            return success ? result.rows : std::vector<std::map<std::string, std::string>>{};
        }
        
    } catch (const std::exception& e) {
        logger_.Error("executeDatabaseQuery failed: " + std::string(e.what()));
        return {};
    }
}

bool DataPointRepository::executeDatabaseNonQuery(const std::string& sql) {
    try {
        // 🔥 수정: 실제 DatabaseManager 메서드 사용
        std::string db_type = config_manager_.getOrDefault("DATABASE_TYPE", "SQLITE");
        
        if (db_type == "POSTGRESQL") {
            return db_manager_.executeNonQueryPostgres(sql);
        } else {
            return db_manager_.executeNonQuerySQLite(sql);
        }
        
    } catch (const std::exception& e) {
        logger_.Error("executeDatabaseNonQuery failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// IRepository 인터페이스 구현
// =============================================================================

std::vector<DataPointEntity> DataPointRepository::findAll() {
    try {
        std::string sql = "SELECT * FROM data_points ORDER BY device_id, address";
        auto result = executeDatabaseQuery(sql);
        return mapResultToEntities(result);
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<DataPointEntity> DataPointRepository::findById(int id) {
    try {
        // 캐시에서 먼저 확인
        if (cache_enabled_) {
            auto cached = getFromCache(id);
            if (cached.has_value()) {
                updateCacheStats("hit");
                return cached;
            }
            updateCacheStats("miss");
        }
        
        std::string sql = "SELECT * FROM data_points WHERE id = " + std::to_string(id);
        auto result = executeDatabaseQuery(sql);
        
        if (result.empty()) {
            return std::nullopt;
        }
        
        DataPointEntity entity = mapRowToEntity(result[0]);
        return entity;
        
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::findById failed: " + std::string(e.what()));
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
                         + getCurrentTimestamp() + "')";
        
        bool success = executeDatabaseNonQuery(sql);
        
        if (success) {
            // ID 조회 (DB 타입별 처리)
            std::string db_type = config_manager_.getOrDefault("DATABASE_TYPE", "SQLITE");
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
        }
        
        return success;
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::save failed: " + std::string(e.what()));
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
                         + ", updated_at='" + getCurrentTimestamp() + "'"
                         + " WHERE id=" + std::to_string(entity.getId());
        
        bool success = executeDatabaseNonQuery(sql);
        
        if (success) {
            logger_.Info("DataPointRepository::update - Updated data point: " + entity.getName());
        }
        
        return success;
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::update failed: " + std::string(e.what()));
        return false;
    }
}

bool DataPointRepository::deleteById(int id) {
    try {
        std::string sql = "DELETE FROM data_points WHERE id = " + std::to_string(id);
        bool success = executeDatabaseNonQuery(sql);
        
        if (success) {
            clearCacheForId(id);
            logger_.Info("DataPointRepository::deleteById - Deleted data point ID: " + std::to_string(id));
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
// 벌크 연산 구현 (기본적인 것들만)
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
        
        auto result = executeDatabaseQuery(sql);
        return mapResultToEntities(result);
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::findByConditions failed: " + std::string(e.what()));
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
        logger_.Error("DataPointRepository::countByConditions failed: " + std::string(e.what()));
    }
    
    return 0;
}

// =============================================================================
// 캐시 관리 구현 (간단하게)
// =============================================================================

void DataPointRepository::setCacheEnabled(bool enabled) {
    cache_enabled_ = enabled;
}

bool DataPointRepository::isCacheEnabled() const {
    return cache_enabled_;
}

void DataPointRepository::clearCache() {
    // 캐시 기능 일시 비활성화 (BaseEntity 문제 해결 후 재구현)
}

void DataPointRepository::clearCacheForId(int id) {
    // 캐시 기능 일시 비활성화
    (void)id;
}

std::map<std::string, int> DataPointRepository::getCacheStats() const {
    return std::map<std::string, int>();
}

// =============================================================================
// 유틸리티 구현
// =============================================================================

int DataPointRepository::getTotalCount() {
    try {
        auto result = executeDatabaseQuery("SELECT COUNT(*) as count FROM data_points");
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
// DataPoint 전용 메서드들 구현 (기본적인 것들만)
// =============================================================================

std::vector<DataPointEntity> DataPointRepository::findByDeviceId(int device_id, bool enabled_only) {
    std::vector<QueryCondition> conditions;
    conditions.emplace_back("device_id", "=", std::to_string(device_id));
    
    if (enabled_only) {
        conditions.emplace_back("is_enabled", "=", "1");
    }
    
    return findByConditions(conditions);
}

// =============================================================================
// 내부 헬퍼 메서드들 구현
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
        logger_.Error("DataPointRepository::mapRowToEntity failed: " + std::string(e.what()));
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

std::optional<DataPointEntity> DataPointRepository::getFromCache(int id) const {
    // 캐시 기능 일시 비활성화
    (void)id;
    return std::nullopt;
}

void DataPointRepository::updateCacheStats(const std::string& operation) const {
    // 캐시 기능 일시 비활성화
    (void)operation;
}

// =============================================================================
// 유틸리티 함수들
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

std::string DataPointRepository::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
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

// =============================================================================
// SQL 빌더 헬퍼 메서드들
// =============================================================================

std::string DataPointRepository::buildWhereClause(const std::vector<QueryCondition>& conditions) const {
    if (conditions.empty()) return "";
    
    std::string clause = " WHERE ";
    for (size_t i = 0; i < conditions.size(); ++i) {
        if (i > 0) clause += " AND ";
        // 🔥 수정: operation 멤버 사용 (op가 아님)
        clause += conditions[i].field + " " + conditions[i].operation + " " + conditions[i].value;
    }
    return clause;
}

std::string DataPointRepository::buildOrderByClause(const std::optional<OrderBy>& order_by) const {
    if (!order_by.has_value()) return "";
    // 🔥 수정: ascending 멤버 사용 (direction이 아님)
    return " ORDER BY " + order_by->field + (order_by->ascending ? " ASC" : " DESC");
}

std::string DataPointRepository::buildLimitClause(const std::optional<Pagination>& pagination) const {
    if (!pagination.has_value()) return "";
    // 🔥 수정: getLimit(), getOffset() 메서드 사용
    return " LIMIT " + std::to_string(pagination->getLimit()) + 
           " OFFSET " + std::to_string(pagination->getOffset());
}

// 기타 누락된 메서드들 (기본 구현)
int DataPointRepository::saveBulk(std::vector<DataPointEntity>& entities) {
    int saved_count = 0;
    for (auto& entity : entities) {
        if (save(entity)) saved_count++;
    }
    return saved_count;
}

int DataPointRepository::updateBulk(const std::vector<DataPointEntity>& entities) {
    int updated_count = 0;
    for (const auto& entity : entities) {
        if (update(entity)) updated_count++;
    }
    return updated_count;
}

int DataPointRepository::deleteByIds(const std::vector<int>& ids) {
    int deleted_count = 0;
    for (int id : ids) {
        if (deleteById(id)) deleted_count++;
    }
    return deleted_count;
}

// 추가 DataPoint 전용 메서드들 (기본 구현)
std::vector<DataPointEntity> DataPointRepository::findAllWithLimit(size_t limit) {
    if (limit == 0) return findAll();
    
    try {
        std::string sql = "SELECT * FROM data_points ORDER BY id LIMIT " + std::to_string(limit);
        auto result = executeDatabaseQuery(sql);
        return mapResultToEntities(result);
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::findAllWithLimit failed: " + std::string(e.what()));
        return {};
    }
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
        logger_.Error("DataPointRepository::findRecentlyCreated failed: " + std::string(e.what()));
        return {};
    }
}

// 관계 데이터 로딩 (빈 구현)
void DataPointRepository::preloadDeviceInfo(std::vector<DataPointEntity>& data_points) {
    (void)data_points;
}

void DataPointRepository::preloadCurrentValues(std::vector<DataPointEntity>& data_points) {
    (void)data_points;
}

void DataPointRepository::preloadAlarmConfigs(std::vector<DataPointEntity>& data_points) {
    (void)data_points;
}

// 통계 메서드들
std::map<int, int> DataPointRepository::getPointCountByDevice() {
    std::map<int, int> counts;
    
    try {
        auto result = executeDatabaseQuery(
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
        auto result = executeDatabaseQuery(
            "SELECT data_type, COUNT(*) as count FROM data_points GROUP BY data_type");
        
        for (const auto& row : result) {
            counts[row.at("data_type")] = std::stoi(row.at("count"));
        }
    } catch (const std::exception& e) {
        logger_.Error("DataPointRepository::getPointCountByDataType failed: " + std::string(e.what()));
    }
    
    return counts;
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne