/**
 * @file DataPointRepository.cpp - DeviceSettingsRepository 패턴 100% 적용
 * @brief PulseOne DataPointRepository 구현 - DatabaseAbstractionLayer 사용
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 DeviceSettingsRepository 패턴 완전 적용:
 * - DatabaseAbstractionLayer 사용으로 멀티 DB 지원
 * - executeQuery/executeNonQuery/executeUpsert 패턴
 * - 기존 직접 DB 호출 제거
 * - 깔끔하고 유지보수 가능한 코드
 */

#include "Database/Repositories/DataPointRepository.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Database/Repositories/CurrentValueRepository.h"
#include "Common/Structs.h"
#include "Common/Utils.h"
#include <sstream>
#include <iomanip>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// 🎯 간단하고 깔끔한 구현 - DB 차이점은 추상화 레이어가 처리
// =============================================================================

std::vector<DataPointEntity> DataPointRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            logger_->Error("DataPointRepository::findAll - Table creation failed");
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, device_id, name, description, address, data_type, access_mode,
                is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
                log_enabled, log_interval_ms, log_deadband, tags, metadata,
                created_at, updated_at
            FROM data_points 
            ORDER BY device_id, address
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<DataPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("DataPointRepository::findAll - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("DataPointRepository::findAll - Found " + std::to_string(entities.size()) + " data points");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<DataPointEntity> DataPointRepository::findById(int id) {
    try {
        // 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                logger_->Debug("DataPointRepository::findById - Cache hit for ID: " + std::to_string(id));
                return cached.value();
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        const std::string query = R"(
            SELECT 
                id, device_id, name, description, address, data_type, access_mode,
                is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
                log_enabled, log_interval_ms, log_deadband, tags, metadata,
                created_at, updated_at
            FROM data_points 
            WHERE id = )" + std::to_string(id);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            logger_->Debug("DataPointRepository::findById - Data point not found: " + std::to_string(id));
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        logger_->Debug("DataPointRepository::findById - Found data point: " + entity.getName());
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::findById failed for ID " + std::to_string(id) + ": " + std::string(e.what()));
        return std::nullopt;
    }
}

bool DataPointRepository::save(DataPointEntity& entity) {
    try {
        if (!validateDataPoint(entity)) {
            logger_->Error("DataPointRepository::save - Invalid data point: " + entity.getName());
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::map<std::string, std::string> data = {
            {"device_id", std::to_string(entity.getDeviceId())},
            {"name", entity.getName()},
            {"description", entity.getDescription()},
            {"address", std::to_string(entity.getAddress())},
            {"data_type", entity.getDataType()},
            {"access_mode", entity.getAccessMode()},
            {"is_enabled", db_layer.formatBoolean(entity.isEnabled())},
            {"unit", entity.getUnit()},
            {"scaling_factor", std::to_string(entity.getScalingFactor())},
            {"scaling_offset", std::to_string(entity.getScalingOffset())},
            {"min_value", std::to_string(entity.getMinValue())},
            {"max_value", std::to_string(entity.getMaxValue())},
            {"log_enabled", db_layer.formatBoolean(entity.isLogEnabled())},
            {"log_interval_ms", std::to_string(entity.getLogInterval())},
            {"log_deadband", std::to_string(entity.getLogDeadband())},
            {"tags", tagsToString(entity.getTags())},
            {"metadata", entity.toJson()["metadata"].dump()},
            {"created_at", db_layer.getCurrentTimestamp()},
            {"updated_at", db_layer.getCurrentTimestamp()}
        };
        
        // ID가 있으면 UPDATE용으로 준비
        if (entity.getId() > 0) {
            data["id"] = std::to_string(entity.getId());
        }
        
        std::vector<std::string> primary_keys = {"id"};
        
        bool success = db_layer.executeUpsert("data_points", data, primary_keys);
        
        if (success) {
            // 새로 생성된 경우 ID 조회
            if (entity.getId() <= 0) {
                std::string id_query = "SELECT id FROM data_points WHERE device_id = " + 
                                     std::to_string(entity.getDeviceId()) + 
                                     " AND address = " + std::to_string(entity.getAddress()) + 
                                     " ORDER BY id DESC LIMIT 1";
                auto id_result = db_layer.executeQuery(id_query);
                if (!id_result.empty()) {
                    entity.setId(std::stoi(id_result[0].at("id")));
                }
            }
            
            // 캐시 업데이트
            if (isCacheEnabled()) {
                cacheEntity(entity);
            }
            
            logger_->Info("DataPointRepository::save - Saved data point: " + entity.getName());
        } else {
            logger_->Error("DataPointRepository::save - Failed to save data point: " + entity.getName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::save failed: " + std::string(e.what()));
        return false;
    }
}

bool DataPointRepository::update(const DataPointEntity& entity) {
    DataPointEntity mutable_entity = entity;
    return save(mutable_entity);
}

bool DataPointRepository::deleteById(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        const std::string query = "DELETE FROM data_points WHERE id = " + std::to_string(id);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            logger_->Info("DataPointRepository::deleteById - Deleted data point ID: " + std::to_string(id));
        } else {
            logger_->Error("DataPointRepository::deleteById - Failed to delete data point ID: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool DataPointRepository::exists(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        const std::string query = "SELECT COUNT(*) as count FROM data_points WHERE id = " + std::to_string(id);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            int count = std::stoi(results[0].at("count"));
            return count > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::exists failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<DataPointEntity> DataPointRepository::findByIds(const std::vector<int>& ids) {
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
                id, device_id, name, description, address, data_type, access_mode,
                is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
                log_enabled, log_interval_ms, log_deadband, tags, metadata,
                created_at, updated_at
            FROM data_points 
            WHERE id IN ()" + ids_ss.str() + ")";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<DataPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("DataPointRepository::findByIds - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("DataPointRepository::findByIds - Found " + std::to_string(entities.size()) + " data points for " + std::to_string(ids.size()) + " IDs");
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
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = R"(
            SELECT 
                id, device_id, name, description, address, data_type, access_mode,
                is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
                log_enabled, log_interval_ms, log_deadband, tags, metadata,
                created_at, updated_at
            FROM data_points
        )";
        
        // WHERE 절 추가
        if (!conditions.empty()) {
            query += buildWhereClause(conditions);
        }
        
        // ORDER BY 절 추가
        if (order_by.has_value()) {
            query += buildOrderByClause(order_by);
        } else {
            query += " ORDER BY device_id, address";
        }
        
        // LIMIT 절 추가
        if (pagination.has_value()) {
            query += buildLimitClause(pagination);
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<DataPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("DataPointRepository::findByConditions - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Debug("DataPointRepository::findByConditions - Found " + std::to_string(entities.size()) + " data points");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::findByConditions failed: " + std::string(e.what()));
        return {};
    }
}

// =============================================================================
// DataPoint 전용 조회 메서드들 (DeviceSettingsRepository 패턴)
// =============================================================================

std::vector<DataPointEntity> DataPointRepository::findByDeviceId(int device_id, bool enabled_only) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = R"(
            SELECT 
                id, device_id, name, description, address, data_type, access_mode,
                is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
                log_enabled, log_interval_ms, log_deadband, tags, metadata,
                created_at, updated_at
            FROM data_points 
            WHERE device_id = )" + std::to_string(device_id);
        
        if (enabled_only) {
            query += " AND is_enabled = 1";
        }
        
        query += " ORDER BY address";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<DataPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("DataPointRepository::findByDeviceId - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("DataPointRepository::findByDeviceId - Found " + std::to_string(entities.size()) + " data points for device: " + std::to_string(device_id));
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::findByDeviceId failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<DataPointEntity> DataPointRepository::findByDeviceIds(const std::vector<int>& device_ids, bool enabled_only) {
    try {
        if (device_ids.empty() || !ensureTableExists()) {
            return {};
        }
        
        // IN 절 구성
        std::ostringstream ids_ss;
        for (size_t i = 0; i < device_ids.size(); ++i) {
            if (i > 0) ids_ss << ", ";
            ids_ss << device_ids[i];
        }
        
        std::string query = R"(
            SELECT 
                id, device_id, name, description, address, data_type, access_mode,
                is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
                log_enabled, log_interval_ms, log_deadband, tags, metadata,
                created_at, updated_at
            FROM data_points 
            WHERE device_id IN ()" + ids_ss.str() + ")";
        
        if (enabled_only) {
            query += " AND is_enabled = 1";
        }
        
        query += " ORDER BY device_id, address";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<DataPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("DataPointRepository::findByDeviceIds - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("DataPointRepository::findByDeviceIds - Found " + std::to_string(entities.size()) + " data points for " + std::to_string(device_ids.size()) + " devices");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::findByDeviceIds failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<DataPointEntity> DataPointRepository::findByDeviceAndAddress(int device_id, int address) {
    try {
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        const std::string query = R"(
            SELECT 
                id, device_id, name, description, address, data_type, access_mode,
                is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
                log_enabled, log_interval_ms, log_deadband, tags, metadata,
                created_at, updated_at
            FROM data_points 
            WHERE device_id = )" + std::to_string(device_id) + 
            " AND address = " + std::to_string(address);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            logger_->Debug("DataPointRepository::findByDeviceAndAddress - No data point found for device " + 
                          std::to_string(device_id) + ", address " + std::to_string(address));
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        logger_->Debug("DataPointRepository::findByDeviceAndAddress - Found data point: " + entity.getName());
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::findByDeviceAndAddress failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<DataPointEntity> DataPointRepository::findWritablePoints() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, device_id, name, description, address, data_type, access_mode,
                is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
                log_enabled, log_interval_ms, log_deadband, tags, metadata,
                created_at, updated_at
            FROM data_points 
            WHERE access_mode IN ('write', 'read_write') AND is_enabled = 1
            ORDER BY device_id, address
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<DataPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("DataPointRepository::findWritablePoints - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("DataPointRepository::findWritablePoints - Found " + std::to_string(entities.size()) + " writable data points");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::findWritablePoints failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<DataPointEntity> DataPointRepository::findByDataType(const std::string& data_type) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, device_id, name, description, address, data_type, access_mode,
                is_enabled, unit, scaling_factor, scaling_offset, min_value, max_value,
                log_enabled, log_interval_ms, log_deadband, tags, metadata,
                created_at, updated_at
            FROM data_points 
            WHERE data_type = ')" + escapeString(data_type) + R"('
            ORDER BY device_id, address
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<DataPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("DataPointRepository::findByDataType - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("DataPointRepository::findByDataType - Found " + std::to_string(entities.size()) + " data points with type: " + data_type);
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::findByDataType failed: " + std::string(e.what()));
        return {};
    }
}

// =============================================================================
// 내부 헬퍼 메서드들 - 🎯 DeviceSettingsRepository 패턴 완전 준수
// =============================================================================

DataPointEntity DataPointRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    DataPointEntity entity;
    
    try {
        DatabaseAbstractionLayer db_layer;
        
        auto it = row.find("id");
        if (it != row.end()) {
            entity.setId(std::stoi(it->second));
        }
        
        it = row.find("device_id");
        if (it != row.end()) {
            entity.setDeviceId(std::stoi(it->second));
        }
        
        it = row.find("name");
        if (it != row.end()) {
            entity.setName(it->second);
        }
        
        it = row.find("description");
        if (it != row.end()) {
            entity.setDescription(it->second);
        }
        
        it = row.find("address");
        if (it != row.end()) {
            entity.setAddress(std::stoi(it->second));
        }
        
        it = row.find("data_type");
        if (it != row.end()) {
            entity.setDataType(it->second);
        }
        
        it = row.find("access_mode");
        if (it != row.end()) {
            entity.setAccessMode(it->second);
        }
        
        it = row.find("is_enabled");
        if (it != row.end()) {
            entity.setEnabled(db_layer.parseBoolean(it->second));
        }
        
        it = row.find("unit");
        if (it != row.end()) {
            entity.setUnit(it->second);
        }
        
        it = row.find("scaling_factor");
        if (it != row.end()) {
            entity.setScalingFactor(std::stod(it->second));
        }
        
        it = row.find("scaling_offset");
        if (it != row.end()) {
            entity.setScalingOffset(std::stod(it->second));
        }
        
        it = row.find("min_value");
        if (it != row.end()) {
            entity.setMinValue(std::stod(it->second));
        }
        
        it = row.find("max_value");
        if (it != row.end()) {
            entity.setMaxValue(std::stod(it->second));
        }
        
        it = row.find("log_enabled");
        if (it != row.end()) {
            entity.setLogEnabled(db_layer.parseBoolean(it->second));
        }
        
        it = row.find("log_interval_ms");
        if (it != row.end()) {
            entity.setLogInterval(std::stoi(it->second));
        }
        
        it = row.find("log_deadband");
        if (it != row.end()) {
            entity.setLogDeadband(std::stod(it->second));
        }
        
        it = row.find("tags");
        if (it != row.end()) {
            entity.setTags(parseTagsFromString(it->second));
        }
        
        it = row.find("metadata");
        if (it != row.end() && !it->second.empty()) {
            try {
                json metadata = json::parse(it->second);
                entity.setMetadata(metadata.get<std::map<std::string, std::string>>());
            } catch (const std::exception&) {
                entity.setMetadata(std::map<std::string, std::string>());
            }
        }
        
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::mapRowToEntity failed: " + std::string(e.what()));
        throw;
    }
}

bool DataPointRepository::ensureTableExists() {
    try {
        const std::string base_create_query = R"(
            CREATE TABLE IF NOT EXISTS data_points (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id INTEGER NOT NULL,
                name VARCHAR(100) NOT NULL,
                description TEXT,
                address INTEGER NOT NULL,
                data_type VARCHAR(20) NOT NULL,
                access_mode VARCHAR(10) DEFAULT 'read',
                is_enabled BOOLEAN DEFAULT true,
                unit VARCHAR(20),
                scaling_factor DECIMAL(10,6) DEFAULT 1.0,
                scaling_offset DECIMAL(10,6) DEFAULT 0.0,
                min_value DECIMAL(15,6),
                max_value DECIMAL(15,6),
                log_enabled BOOLEAN DEFAULT true,
                log_interval_ms INTEGER DEFAULT 0,
                log_deadband DECIMAL(10,6) DEFAULT 0.0,
                tags TEXT,
                metadata TEXT,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                
                FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
                UNIQUE(device_id, address)
            )
        )";
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeCreateTable(base_create_query);
        
        if (success) {
            logger_->Debug("DataPointRepository::ensureTableExists - Table creation/check completed");
        } else {
            logger_->Error("DataPointRepository::ensureTableExists - Table creation failed");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::ensureTableExists failed: " + std::string(e.what()));
        return false;
    }
}

bool DataPointRepository::validateDataPoint(const DataPointEntity& entity) const {
    if (!entity.isValid()) {
        logger_->Warn("DataPointRepository::validateDataPoint - Invalid data point: " + entity.getName());
        return false;
    }
    
    if (entity.getDeviceId() <= 0) {
        logger_->Warn("DataPointRepository::validateDataPoint - Invalid device_id for data point: " + entity.getName());
        return false;
    }
    
    if (entity.getName().empty()) {
        logger_->Warn("DataPointRepository::validateDataPoint - Empty name for data point");
        return false;
    }
    
    if (entity.getDataType().empty()) {
        logger_->Warn("DataPointRepository::validateDataPoint - Empty data_type for data point: " + entity.getName());
        return false;
    }
    
    return true;
}

std::string DataPointRepository::escapeString(const std::string& str) const {
    std::string escaped = str;
    size_t pos = 0;
    while ((pos = escaped.find("'", pos)) != std::string::npos) {
        escaped.replace(pos, 1, "''");
        pos += 2;
    }
    return escaped;
}

// =============================================================================
// SQL 빌더 헬퍼 메서드들 (기존 로직 유지)
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
// 유틸리티 함수들 (기존 로직 유지)
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

// CurrentValueRepository 의존성 주입 구현
void DataPointRepository::setCurrentValueRepository(std::shared_ptr<CurrentValueRepository> current_value_repo) {
    current_value_repo_ = current_value_repo;
    logger_->Info("✅ CurrentValueRepository injected into DataPointRepository");
}

// =============================================================================
// 🔥 핵심 메서드: 현재값이 포함된 완성된 DataPoint 조회 (기존 로직 유지)
// =============================================================================

std::vector<PulseOne::Structs::DataPoint> DataPointRepository::getDataPointsWithCurrentValues(int device_id, bool enabled_only) {
    std::vector<PulseOne::Structs::DataPoint> result;
    
    try {
        logger_->Debug("🔍 Loading DataPoints with current values for device: " + std::to_string(device_id));
        
        // 1. DataPointEntity들 조회 (이미 DatabaseAbstractionLayer 사용)
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
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::getDataPointsWithCurrentValues failed: " + std::string(e.what()));
    }
    
    return result;
}

// =============================================================================
// 추가 메서드들 (간단한 구현으로 대체)
// =============================================================================

int DataPointRepository::getTotalCount() {
    try {
        const std::string query = "SELECT COUNT(*) as count FROM data_points";
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty()) {
            return std::stoi(results[0].at("count"));
        }
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::getTotalCount failed: " + std::string(e.what()));
    }
    
    return 0;
}

std::map<int, int> DataPointRepository::getPointCountByDevice() {
    std::map<int, int> counts;
    
    try {
        const std::string query = "SELECT device_id, COUNT(*) as count FROM data_points GROUP BY device_id";
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        for (const auto& row : results) {
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
        const std::string query = "SELECT data_type, COUNT(*) as count FROM data_points GROUP BY data_type";
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        for (const auto& row : results) {
            counts[row.at("data_type")] = std::stoi(row.at("count"));
        }
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::getPointCountByDataType failed: " + std::string(e.what()));
    }
    
    return counts;
}

// 나머지 메서드들은 기본 구현으로 대체 (필요시 추후 확장)
std::vector<DataPointEntity> DataPointRepository::findAllWithLimit(size_t limit) {
    // Pagination 사용으로 간소화
    return findByConditions({}, OrderBy("id", true), Pagination(limit, 0));
}

std::vector<DataPointEntity> DataPointRepository::findDataPointsForWorkers(const std::vector<int>& device_ids) {
    if (device_ids.empty()) {
        return findByConditions({QueryCondition("is_enabled", "=", "1")});
    } else {
        return findByDeviceIds(device_ids, true);
    }
}

std::vector<DataPointEntity> DataPointRepository::findByTag(const std::string& tag) {
    return findByConditions({QueryCondition("tags", "LIKE", "'%" + tag + "%'")});
}

std::vector<DataPointEntity> DataPointRepository::findDisabledPoints() {
    return findByConditions({QueryCondition("is_enabled", "=", "0")});
}

std::vector<DataPointEntity> DataPointRepository::findRecentlyCreated(int days) {
    (void)days;  // 사용하지 않는 파라미터
    return findByConditions({}, OrderBy("created_at", false), Pagination(100, 0));
}

// 관계 데이터 사전 로딩 (기본 구현)
void DataPointRepository::preloadDeviceInfo(std::vector<DataPointEntity>&) {
    logger_->Debug("DataPointRepository::preloadDeviceInfo - Not implemented yet");
}

void DataPointRepository::preloadCurrentValues(std::vector<DataPointEntity>&) {
    logger_->Debug("DataPointRepository::preloadCurrentValues - Not implemented yet");
}

void DataPointRepository::preloadAlarmConfigs(std::vector<DataPointEntity>&) {
    logger_->Debug("DataPointRepository::preloadAlarmConfigs - Not implemented yet");
}


} // namespace Repositories
} // namespace Database
} // namespace PulseOne