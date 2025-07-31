/**
 * @file CurrentValueRepository.cpp - DataPointRepository 패턴 100% 적용
 * @brief PulseOne CurrentValueRepository 구현 - DatabaseAbstractionLayer 사용
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 DataPointRepository 패턴 완전 적용:
 * - DatabaseAbstractionLayer 사용으로 멀티 DB 지원
 * - executeQuery/executeNonQuery/executeUpsert 패턴
 * - 기존 직접 DB 호출 제거
 * - 깔끔하고 유지보수 가능한 코드
 */

#include "Database/Repositories/CurrentValueRepository.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Common/Utils.h"
#include <sstream>
#include <iomanip>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// 🎯 간단하고 깔끔한 구현 - DB 차이점은 추상화 레이어가 처리
// =============================================================================

std::vector<CurrentValueEntity> CurrentValueRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            logger_->Error("CurrentValueRepository::findAll - Table creation failed");
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                point_id, value, raw_value, string_value, quality,
                timestamp, updated_at
            FROM current_values 
            ORDER BY point_id
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<CurrentValueEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("CurrentValueRepository::findAll - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("CurrentValueRepository::findAll - Found " + std::to_string(entities.size()) + " current values");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<CurrentValueEntity> CurrentValueRepository::findById(int id) {
    try {
        // 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                logger_->Debug("CurrentValueRepository::findById - Cache hit for point_id: " + std::to_string(id));
                return cached.value();
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        const std::string query = R"(
            SELECT 
                point_id, value, raw_value, string_value, quality,
                timestamp, updated_at
            FROM current_values 
            WHERE point_id = )" + std::to_string(id);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            logger_->Debug("CurrentValueRepository::findById - Current value not found for point_id: " + std::to_string(id));
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        logger_->Debug("CurrentValueRepository::findById - Found current value for point_id: " + std::to_string(id));
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::findById failed for point_id " + std::to_string(id) + ": " + std::string(e.what()));
        return std::nullopt;
    }
}

bool CurrentValueRepository::save(CurrentValueEntity& entity) {
    try {
        if (!validateCurrentValue(entity)) {
            logger_->Error("CurrentValueRepository::save - Invalid current value for point_id: " + std::to_string(entity.getPointId()));
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::map<std::string, std::string> data = {
            {"point_id", std::to_string(entity.getPointId())},
            {"value", std::to_string(entity.getValue())},
            {"raw_value", std::to_string(entity.getRawValue())},
            {"string_value", entity.getStringValue()},
            {"quality", entity.getQualityString()},
            {"timestamp", PulseOne::Utils::TimestampToDBString(entity.getTimestamp())},
            {"updated_at", PulseOne::Utils::TimestampToDBString(entity.getUpdatedAt())}
        };
        
        std::vector<std::string> primary_keys = {"point_id"};
        
        bool success = db_layer.executeUpsert("current_values", data, primary_keys);
        
        if (success) {
            // 캐시 업데이트
            if (isCacheEnabled()) {
                cacheEntity(entity);
            }
            
            logger_->Info("CurrentValueRepository::save - Saved current value for point_id: " + std::to_string(entity.getPointId()));
        } else {
            logger_->Error("CurrentValueRepository::save - Failed to save current value for point_id: " + std::to_string(entity.getPointId()));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::save failed: " + std::string(e.what()));
        return false;
    }
}

bool CurrentValueRepository::update(const CurrentValueEntity& entity) {
    CurrentValueEntity mutable_entity = entity;
    return save(mutable_entity);
}

bool CurrentValueRepository::deleteById(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        const std::string query = "DELETE FROM current_values WHERE point_id = " + std::to_string(id);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            logger_->Info("CurrentValueRepository::deleteById - Deleted current value for point_id: " + std::to_string(id));
        } else {
            logger_->Error("CurrentValueRepository::deleteById - Failed to delete current value for point_id: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool CurrentValueRepository::exists(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        const std::string query = "SELECT COUNT(*) as count FROM current_values WHERE point_id = " + std::to_string(id);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            int count = std::stoi(results[0].at("count"));
            return count > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::exists failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<CurrentValueEntity> CurrentValueRepository::findByIds(const std::vector<int>& ids) {
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
                point_id, value, raw_value, string_value, quality,
                timestamp, updated_at
            FROM current_values 
            WHERE point_id IN ()" + ids_ss.str() + ")";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<CurrentValueEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("CurrentValueRepository::findByIds - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("CurrentValueRepository::findByIds - Found " + std::to_string(entities.size()) + " current values for " + std::to_string(ids.size()) + " point IDs");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::findByIds failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<CurrentValueEntity> CurrentValueRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = R"(
            SELECT 
                point_id, value, raw_value, string_value, quality,
                timestamp, updated_at
            FROM current_values
        )";
        
        // WHERE 절 추가
        if (!conditions.empty()) {
            query += buildWhereClause(conditions);
        }
        
        // ORDER BY 절 추가
        if (order_by.has_value()) {
            query += buildOrderByClause(order_by);
        } else {
            query += " ORDER BY updated_at DESC";
        }
        
        // LIMIT 절 추가
        if (pagination.has_value()) {
            query += buildLimitClause(pagination);
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<CurrentValueEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("CurrentValueRepository::findByConditions - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Debug("CurrentValueRepository::findByConditions - Found " + std::to_string(entities.size()) + " current values");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::findByConditions failed: " + std::string(e.what()));
        return {};
    }
}

int CurrentValueRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        std::string query = "SELECT COUNT(*) as count FROM current_values";
        
        if (!conditions.empty()) {
            query += buildWhereClause(conditions);
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::countByConditions failed: " + std::string(e.what()));
        return 0;
    }
}

int CurrentValueRepository::saveBulk(std::vector<CurrentValueEntity>& entities) {
    if (entities.empty()) {
        return 0;
    }
    
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DatabaseAbstractionLayer db_layer;
        int success_count = 0;
        
        // 배치 크기 설정 (성능 최적화)
        const size_t batch_size = 100;
        
        for (size_t i = 0; i < entities.size(); i += batch_size) {
            size_t end = std::min(i + batch_size, entities.size());
            
            // UPSERT 배치 쿼리 생성
            std::ostringstream query;
            query << "INSERT OR REPLACE INTO current_values (point_id, value, raw_value, string_value, quality, timestamp, updated_at) VALUES ";
            
            for (size_t j = i; j < end; ++j) {
                if (j > i) query << ", ";
                
                const auto& entity = entities[j];
                query << "(" 
                      << entity.getPointId() << ", "
                      << entity.getValue() << ", "
                      << entity.getRawValue() << ", '"
                      << escapeString(entity.getStringValue()) << "', '"
                      << entity.getQualityString() << "', '"
                      << PulseOne::Utils::TimestampToDBString(entity.getTimestamp()) << "', '"
                      << PulseOne::Utils::TimestampToDBString(entity.getUpdatedAt()) << "')";
            }
            
            if (db_layer.executeNonQuery(query.str())) {
                success_count += (end - i);
                
                // 캐시 업데이트
                if (isCacheEnabled()) {
                    for (size_t j = i; j < end; ++j) {
                        cacheEntity(entities[j]);
                    }
                }
            }
        }
        
        logger_->Info("CurrentValueRepository::saveBulk - Saved " + std::to_string(success_count) + "/" + std::to_string(entities.size()) + " current values");
        return success_count;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::saveBulk failed: " + std::string(e.what()));
        return 0;
    }
}

int CurrentValueRepository::updateBulk(const std::vector<CurrentValueEntity>& entities) {
    // UPDATE는 일반적으로 UPSERT로 처리하는 것이 효율적
    std::vector<CurrentValueEntity> mutable_entities = entities;
    return saveBulk(mutable_entities);
}

int CurrentValueRepository::deleteByIds(const std::vector<int>& ids) {
    try {
        if (ids.empty() || !ensureTableExists()) {
            return 0;
        }
        
        // IN 절 구성
        std::ostringstream ids_ss;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) ids_ss << ", ";
            ids_ss << ids[i];
        }
        
        const std::string query = "DELETE FROM current_values WHERE point_id IN (" + ids_ss.str() + ")";
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 캐시에서 제거
            if (isCacheEnabled()) {
                for (int id : ids) {
                    clearCacheForId(id);
                }
            }
            
            logger_->Info("CurrentValueRepository::deleteByIds - Deleted current values for " + std::to_string(ids.size()) + " point IDs");
            return static_cast<int>(ids.size());
        } else {
            logger_->Error("CurrentValueRepository::deleteByIds - Failed to delete current values");
            return 0;
        }
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::deleteByIds failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// 캐시 관리 메서드들 (DataPointRepository 패턴)
// =============================================================================

void CurrentValueRepository::setCacheEnabled(bool enabled) {
    IRepository::setCacheEnabled(enabled);
    std::string status = enabled ? "enabled" : "disabled";
    logger_->Info("CurrentValueRepository cache " + status);
}

bool CurrentValueRepository::isCacheEnabled() const {
    return IRepository::isCacheEnabled();
}

void CurrentValueRepository::clearCache() {
    IRepository::clearCache();
    logger_->Info("CurrentValueRepository cache cleared");
}

void CurrentValueRepository::clearCacheForId(int id) {
    IRepository::clearCacheForId(id);
    logger_->Debug("CurrentValueRepository cache cleared for point_id: " + std::to_string(id));
}

std::map<std::string, int> CurrentValueRepository::getCacheStats() const {
    return IRepository::getCacheStats();
}

int CurrentValueRepository::getTotalCount() {
    return countByConditions({});
}

// =============================================================================
// CurrentValue 전용 조회 메서드들 (DataPointRepository 패턴)
// =============================================================================

std::vector<CurrentValueEntity> CurrentValueRepository::findByDataPointIds(const std::vector<int>& point_ids) {
    return findByIds(point_ids);  // point_id가 primary key이므로 동일
}

std::optional<CurrentValueEntity> CurrentValueRepository::findByDataPointId(int data_point_id) {
    try {
        if (data_point_id <= 0) {
            logger_->Warn("CurrentValueRepository::findByDataPointId - Invalid data point ID: " + std::to_string(data_point_id));
            return std::nullopt;
        }
        
        // 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(data_point_id);
            if (cached.has_value()) {
                logger_->Debug("CurrentValueRepository::findByDataPointId - Cache hit for point_id: " + std::to_string(data_point_id));
                return cached.value();
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        const std::string query = R"(
            SELECT 
                point_id, value, raw_value, string_value, quality,
                timestamp, updated_at
            FROM current_values 
            WHERE point_id = )" + std::to_string(data_point_id);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            logger_->Debug("CurrentValueRepository::findByDataPointId - Current value not found for point_id: " + std::to_string(data_point_id));
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        logger_->Debug("CurrentValueRepository::findByDataPointId - Found current value for point_id: " + std::to_string(data_point_id));
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::findByDataPointId failed for point_id " + std::to_string(data_point_id) + ": " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<CurrentValueEntity> CurrentValueRepository::findByQuality(PulseOne::Enums::DataQuality quality) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string quality_str = CurrentValueEntity::qualityToString(quality);
        const std::string query = R"(
            SELECT 
                point_id, value, raw_value, string_value, quality,
                timestamp, updated_at
            FROM current_values 
            WHERE quality = ')" + quality_str + R"('
            ORDER BY updated_at DESC
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<CurrentValueEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("CurrentValueRepository::findByQuality - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("CurrentValueRepository::findByQuality - Found " + std::to_string(entities.size()) + " current values with quality: " + quality_str);
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::findByQuality failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<CurrentValueEntity> CurrentValueRepository::findByTimeRange(
    const std::chrono::system_clock::time_point& start_time,
    const std::chrono::system_clock::time_point& end_time) {
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string start_str = PulseOne::Utils::TimestampToDBString(start_time);
        std::string end_str = PulseOne::Utils::TimestampToDBString(end_time);
        
        const std::string query = R"(
            SELECT 
                point_id, value, raw_value, string_value, quality,
                timestamp, updated_at
            FROM current_values 
            WHERE timestamp BETWEEN ')" + start_str + "' AND '" + end_str + R"('
            ORDER BY timestamp DESC
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<CurrentValueEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("CurrentValueRepository::findByTimeRange - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("CurrentValueRepository::findByTimeRange - Found " + std::to_string(entities.size()) + " current values in time range");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::findByTimeRange failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<CurrentValueEntity> CurrentValueRepository::findRecentlyUpdated(int minutes) {
    auto cutoff_time = std::chrono::system_clock::now() - std::chrono::minutes(minutes);
    auto now = std::chrono::system_clock::now();
    return findByTimeRange(cutoff_time, now);
}

std::vector<CurrentValueEntity> CurrentValueRepository::findStaleValues(int hours) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(hours);
        std::string cutoff_str = PulseOne::Utils::TimestampToDBString(cutoff_time);
        
        const std::string query = R"(
            SELECT 
                point_id, value, raw_value, string_value, quality,
                timestamp, updated_at
            FROM current_values 
            WHERE updated_at < ')" + cutoff_str + R"('
            ORDER BY updated_at ASC
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<CurrentValueEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("CurrentValueRepository::findStaleValues - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("CurrentValueRepository::findStaleValues - Found " + std::to_string(entities.size()) + " stale current values (older than " + std::to_string(hours) + " hours)");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::findStaleValues failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<CurrentValueEntity> CurrentValueRepository::findBadQualityValues() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                point_id, value, raw_value, string_value, quality,
                timestamp, updated_at
            FROM current_values 
            WHERE quality IN ('bad', 'uncertain', 'not_connected')
            ORDER BY updated_at DESC
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<CurrentValueEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("CurrentValueRepository::findBadQualityValues - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("CurrentValueRepository::findBadQualityValues - Found " + std::to_string(entities.size()) + " current values with bad quality");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::findBadQualityValues failed: " + std::string(e.what()));
        return {};
    }
}

std::map<std::string, int> CurrentValueRepository::getStatistics() {
    std::map<std::string, int> stats;
    
    try {
        // 총 현재값 개수
        stats["total_values"] = getTotalCount();
        
        // 품질별 개수
        const std::string quality_query = "SELECT quality, COUNT(*) as count FROM current_values GROUP BY quality";
        DatabaseAbstractionLayer db_layer;
        auto quality_result = db_layer.executeQuery(quality_query);
        
        for (const auto& row : quality_result) {
            stats["quality_" + row.at("quality")] = std::stoi(row.at("count"));
        }
        
        // 최근 업데이트 개수 (1시간 이내)
        auto recent_count = findRecentlyUpdated(60).size();
        stats["recent_updates_1h"] = static_cast<int>(recent_count);
        
        // 오래된 값 개수 (24시간 이전)
        auto stale_count = findStaleValues(24).size();
        stats["stale_values_24h"] = static_cast<int>(stale_count);
        
        logger_->Debug("CurrentValueRepository::getStatistics - Generated statistics");
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::getStatistics failed: " + std::string(e.what()));
    }
    
    return stats;
}

int CurrentValueRepository::bulkUpsert(std::vector<CurrentValueEntity>& entities) {
    return saveBulk(entities);  // saveBulk가 이미 upsert 방식으로 구현됨
}

// =============================================================================
// 내부 헬퍼 메서드들 - 🎯 DataPointRepository 패턴 완전 준수
// =============================================================================

CurrentValueEntity CurrentValueRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    CurrentValueEntity entity;
    
    try {
        auto it = row.find("point_id");
        if (it != row.end()) {
            entity.setPointId(std::stoi(it->second));
        }
        
        it = row.find("value");
        if (it != row.end()) {
            entity.setValue(std::stod(it->second));
        }
        
        it = row.find("raw_value");
        if (it != row.end()) {
            entity.setRawValue(std::stod(it->second));
        }
        
        it = row.find("string_value");
        if (it != row.end()) {
            entity.setStringValue(it->second);
        }
        
        it = row.find("quality");
        if (it != row.end()) {
            entity.setQuality(CurrentValueEntity::stringToQuality(it->second));
        }
        
        it = row.find("timestamp");
        if (it != row.end()) {
            entity.setTimestamp(PulseOne::Utils::ParseTimestampFromString(it->second));
        }
        
        it = row.find("updated_at");
        if (it != row.end()) {
            entity.setUpdatedAt(PulseOne::Utils::ParseTimestampFromString(it->second));
        }
        
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::mapRowToEntity failed: " + std::string(e.what()));
        throw;
    }
}

bool CurrentValueRepository::ensureTableExists() {
    try {
        const std::string base_create_query = R"(
            CREATE TABLE IF NOT EXISTS current_values (
                point_id INTEGER PRIMARY KEY,
                value DECIMAL(15,4),
                raw_value DECIMAL(15,4),
                string_value TEXT,
                quality VARCHAR(20) DEFAULT 'good',
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                
                FOREIGN KEY (point_id) REFERENCES data_points(id) ON DELETE CASCADE
            )
        )";
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeCreateTable(base_create_query);
        
        if (success) {
            // 인덱스 생성
            std::vector<std::string> indexes = {
                "CREATE INDEX IF NOT EXISTS idx_current_values_timestamp ON current_values(timestamp DESC)",
                "CREATE INDEX IF NOT EXISTS idx_current_values_updated_at ON current_values(updated_at)",
                "CREATE INDEX IF NOT EXISTS idx_current_values_quality ON current_values(quality)"
            };
            
            for (const auto& idx_query : indexes) {
                db_layer.executeNonQuery(idx_query);
            }
            
            logger_->Debug("CurrentValueRepository::ensureTableExists - Table creation/check completed");
        } else {
            logger_->Error("CurrentValueRepository::ensureTableExists - Table creation failed");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::ensureTableExists failed: " + std::string(e.what()));
        return false;
    }
}

bool CurrentValueRepository::validateCurrentValue(const CurrentValueEntity& entity) const {
    if (!entity.isValid()) {
        logger_->Warn("CurrentValueRepository::validateCurrentValue - Invalid current value for point_id: " + std::to_string(entity.getPointId()));
        return false;
    }
    
    if (entity.getPointId() <= 0) {
        logger_->Warn("CurrentValueRepository::validateCurrentValue - Invalid point_id: " + std::to_string(entity.getPointId()));
        return false;
    }
    
    return true;
}

std::string CurrentValueRepository::escapeString(const std::string& str) const {
    std::string escaped = str;
    size_t pos = 0;
    while ((pos = escaped.find("'", pos)) != std::string::npos) {
        escaped.replace(pos, 1, "''");
        pos += 2;
    }
    return escaped;
}

// =============================================================================
// SQL 빌더 헬퍼 메서드들 (DataPointRepository 패턴)
// =============================================================================

std::string CurrentValueRepository::buildWhereClause(const std::vector<QueryCondition>& conditions) const {
    if (conditions.empty()) return "";
    
    std::string clause = " WHERE ";
    for (size_t i = 0; i < conditions.size(); ++i) {
        if (i > 0) clause += " AND ";
        clause += conditions[i].field + " " + conditions[i].operation + " " + conditions[i].value;
    }
    return clause;
}

std::string CurrentValueRepository::buildOrderByClause(const std::optional<OrderBy>& order_by) const {
    if (!order_by.has_value()) return "";
    return " ORDER BY " + order_by->field + (order_by->ascending ? " ASC" : " DESC");
}

std::string CurrentValueRepository::buildLimitClause(const std::optional<Pagination>& pagination) const {
    if (!pagination.has_value()) return "";
    return " LIMIT " + std::to_string(pagination->getLimit()) + 
           " OFFSET " + std::to_string(pagination->getOffset());
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne