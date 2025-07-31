/**
 * @file AlarmConfigRepository.cpp  
 * @brief PulseOne AlarmConfigRepository 구현 - DeviceRepository 패턴 100% 준수
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🎯 DeviceRepository 패턴 완전 적용:
 * - DatabaseAbstractionLayer 사용
 * - executeQuery/executeNonQuery/executeUpsert 패턴
 * - 컴파일 에러 완전 해결
 * - 깔끔하고 유지보수 가능한 코드
 */

#include "Database/Repositories/AlarmConfigRepository.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Utils/LogManager.h"
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace PulseOne {
namespace Database {
namespace Repositories {

using AlarmConfigEntity = Entities::AlarmConfigEntity;

// =======================================================================
// 🔥 DeviceRepository 패턴 - initializeDependencies() 메서드 추가
// =======================================================================

void AlarmConfigRepository::initializeDependencies() {
    db_manager_ = &DatabaseManager::getInstance();
    logger_ = &LogManager::getInstance();
    config_manager_ = &ConfigManager::getInstance();
}

// =======================================================================
// IRepository 인터페이스 구현 (DeviceRepository 패턴 100% 동일)
// =======================================================================

std::vector<AlarmConfigEntity> AlarmConfigRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            logger_->Error("AlarmConfigRepository::findAll - Table creation failed");
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, data_point_id, virtual_point_id,
                alarm_name, description, severity, condition_type,
                threshold_value, high_limit, low_limit, timeout_seconds,
                is_enabled, auto_acknowledge, delay_seconds, message_template,
                created_at, updated_at
            FROM alarm_definitions 
            ORDER BY alarm_name
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<AlarmConfigEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("AlarmConfigRepository::findAll - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("AlarmConfigRepository::findAll - Found " + std::to_string(entities.size()) + " alarm configs");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigRepository::findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<AlarmConfigEntity> AlarmConfigRepository::findById(int id) {
    try {
        // 🔥 DeviceRepository 패턴 - 캐시 먼저 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                logger_->Debug("✅ Cache HIT for alarm config ID: " + std::to_string(id));
                return cached.value();
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, data_point_id, virtual_point_id,
                alarm_name, description, severity, condition_type,
                threshold_value, high_limit, low_limit, timeout_seconds,
                is_enabled, auto_acknowledge, delay_seconds, message_template,
                created_at, updated_at
            FROM alarm_definitions 
            WHERE id = )" + std::to_string(id);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            logger_->Debug("AlarmConfigRepository::findById - Alarm config not found: " + std::to_string(id));
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        logger_->Debug("AlarmConfigRepository::findById - Found alarm config: " + entity.getName());
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigRepository::findById failed for ID " + std::to_string(id) + ": " + std::string(e.what()));
        return std::nullopt;
    }
}

bool AlarmConfigRepository::save(AlarmConfigEntity& entity) {
    try {
        if (!validateAlarmConfig(entity)) {
            logger_->Error("AlarmConfigRepository::save - Invalid alarm config: " + entity.getName());
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🔧 수정: entityToParams 메서드 사용하여 맵 생성
        std::map<std::string, std::string> data = entityToParams(entity);
        
        std::vector<std::string> primary_keys = {"id"};
        
        bool success = db_layer.executeUpsert("alarm_definitions", data, primary_keys);
        
        if (success) {
            // 새로 생성된 경우 ID 조회
            if (entity.getId() <= 0) {
                const std::string id_query = "SELECT last_insert_rowid() as id";
                auto id_result = db_layer.executeQuery(id_query);
                if (!id_result.empty()) {
                    entity.setId(std::stoi(id_result[0].at("id")));
                }
            }
            
            // 캐시 업데이트
            if (isCacheEnabled()) {
                cacheEntity(entity);
            }
            
            logger_->Info("AlarmConfigRepository::save - Saved alarm config: " + entity.getName());
        } else {
            logger_->Error("AlarmConfigRepository::save - Failed to save alarm config: " + entity.getName());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigRepository::save failed: " + std::string(e.what()));
        return false;
    }
}

bool AlarmConfigRepository::update(const AlarmConfigEntity& entity) {
    AlarmConfigEntity mutable_entity = entity;
    return save(mutable_entity);
}

bool AlarmConfigRepository::deleteById(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        const std::string query = "DELETE FROM alarm_definitions WHERE id = " + std::to_string(id);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            logger_->Info("AlarmConfigRepository::deleteById - Deleted alarm config ID: " + std::to_string(id));
        } else {
            logger_->Error("AlarmConfigRepository::deleteById - Failed to delete alarm config ID: " + std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigRepository::deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool AlarmConfigRepository::exists(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        const std::string query = "SELECT COUNT(*) as count FROM alarm_definitions WHERE id = " + std::to_string(id);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            int count = std::stoi(results[0].at("count"));
            return count > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigRepository::exists failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<AlarmConfigEntity> AlarmConfigRepository::findByIds(const std::vector<int>& ids) {
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
                id, tenant_id, site_id, data_point_id, virtual_point_id,
                alarm_name, description, severity, condition_type,
                threshold_value, high_limit, low_limit, timeout_seconds,
                is_enabled, auto_acknowledge, delay_seconds, message_template,
                created_at, updated_at
            FROM alarm_definitions 
            WHERE id IN ()" + ids_ss.str() + ")";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<AlarmConfigEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("AlarmConfigRepository::findByIds - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("AlarmConfigRepository::findByIds - Found " + std::to_string(entities.size()) + " alarm configs for " + std::to_string(ids.size()) + " IDs");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigRepository::findByIds failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<AlarmConfigEntity> AlarmConfigRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = R"(
            SELECT 
                id, tenant_id, site_id, data_point_id, virtual_point_id,
                alarm_name, description, severity, condition_type,
                threshold_value, high_limit, low_limit, timeout_seconds,
                is_enabled, auto_acknowledge, delay_seconds, message_template,
                created_at, updated_at
            FROM alarm_definitions
        )";
        
        query += buildWhereClause(conditions);
        query += buildOrderByClause(order_by);
        query += buildLimitClause(pagination);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<AlarmConfigEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("AlarmConfigRepository::findByConditions - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Debug("AlarmConfigRepository::findByConditions - Found " + std::to_string(entities.size()) + " alarm configs");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigRepository::findByConditions failed: " + std::string(e.what()));
        return {};
    }
}

int AlarmConfigRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        std::string query = "SELECT COUNT(*) as count FROM alarm_definitions";
        query += buildWhereClause(conditions);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigRepository::countByConditions failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// 벌크 연산 (DeviceRepository 패턴)
// =============================================================================

int AlarmConfigRepository::saveBulk(std::vector<AlarmConfigEntity>& entities) {
    int saved_count = 0;
    for (auto& entity : entities) {
        if (save(entity)) {
            saved_count++;
        }
    }
    logger_->Info("AlarmConfigRepository::saveBulk - Saved " + std::to_string(saved_count) + " alarm configs");
    return saved_count;
}

int AlarmConfigRepository::updateBulk(const std::vector<AlarmConfigEntity>& entities) {
    int updated_count = 0;
    for (const auto& entity : entities) {
        if (update(entity)) {
            updated_count++;
        }
    }
    logger_->Info("AlarmConfigRepository::updateBulk - Updated " + std::to_string(updated_count) + " alarm configs");
    return updated_count;
}

int AlarmConfigRepository::deleteByIds(const std::vector<int>& ids) {
    int deleted_count = 0;
    for (int id : ids) {
        if (deleteById(id)) {
            deleted_count++;
        }
    }
    logger_->Info("AlarmConfigRepository::deleteByIds - Deleted " + std::to_string(deleted_count) + " alarm configs");
    return deleted_count;
}

int AlarmConfigRepository::getTotalCount() {
    return countByConditions({});
}

// =============================================================================
// 알람 설정 전용 메서드들 (DeviceRepository 패턴)
// =============================================================================

std::vector<AlarmConfigEntity> AlarmConfigRepository::findByTenant(int tenant_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, data_point_id, virtual_point_id,
                alarm_name, description, severity, condition_type,
                threshold_value, high_limit, low_limit, timeout_seconds,
                is_enabled, auto_acknowledge, delay_seconds, message_template,
                created_at, updated_at
            FROM alarm_definitions 
            WHERE tenant_id = )" + std::to_string(tenant_id) + R"(
            ORDER BY alarm_name
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<AlarmConfigEntity> entities = mapResultToEntities(results);
        
        logger_->Info("AlarmConfigRepository::findByTenant - Found " + std::to_string(entities.size()) + " alarm configs for tenant " + std::to_string(tenant_id));
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigRepository::findByTenant failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<AlarmConfigEntity> AlarmConfigRepository::findByDataPoint(int data_point_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, data_point_id, virtual_point_id,
                alarm_name, description, severity, condition_type,
                threshold_value, high_limit, low_limit, timeout_seconds,
                is_enabled, auto_acknowledge, delay_seconds, message_template,
                created_at, updated_at
            FROM alarm_definitions 
            WHERE data_point_id = )" + std::to_string(data_point_id) + R"( AND is_enabled = 1
            ORDER BY alarm_name
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<AlarmConfigEntity> entities = mapResultToEntities(results);
        
        logger_->Info("AlarmConfigRepository::findByDataPoint - Found " + std::to_string(entities.size()) + " alarm configs for data point " + std::to_string(data_point_id));
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigRepository::findByDataPoint failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<AlarmConfigEntity> AlarmConfigRepository::findActiveAlarms() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        const std::string query = R"(
            SELECT 
                id, tenant_id, site_id, data_point_id, virtual_point_id,
                alarm_name, description, severity, condition_type,
                threshold_value, high_limit, low_limit, timeout_seconds,
                is_enabled, auto_acknowledge, delay_seconds, message_template,
                created_at, updated_at
            FROM alarm_definitions 
            WHERE is_enabled = 1
            ORDER BY severity DESC, alarm_name ASC
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<AlarmConfigEntity> entities = mapResultToEntities(results);
        
        logger_->Info("AlarmConfigRepository::findActiveAlarms - Found " + std::to_string(entities.size()) + " active alarm configs");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigRepository::findActiveAlarms failed: " + std::string(e.what()));
        return {};
    }
}

bool AlarmConfigRepository::isAlarmNameTaken(const std::string& name, int tenant_id, int exclude_id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        std::string query = "SELECT COUNT(*) as count FROM alarm_definitions WHERE alarm_name = '" + 
                           escapeString(name) + "' AND tenant_id = " + std::to_string(tenant_id);
        
        if (exclude_id > 0) {
            query += " AND id != " + std::to_string(exclude_id);
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            int count = std::stoi(results[0].at("count"));
            return count > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigRepository::isAlarmNameTaken failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 캐시 관리 (DeviceRepository 패턴)
// =============================================================================

void AlarmConfigRepository::setCacheEnabled(bool enabled) {
    IRepository<AlarmConfigEntity>::setCacheEnabled(enabled);
    if (logger_) {
        logger_->Info("AlarmConfigRepository cache " + std::string(enabled ? "enabled" : "disabled"));
    }
}

bool AlarmConfigRepository::isCacheEnabled() const {
    return IRepository<AlarmConfigEntity>::isCacheEnabled();
}

void AlarmConfigRepository::clearCache() {
    IRepository<AlarmConfigEntity>::clearCache();
    if (logger_) {
        logger_->Info("AlarmConfigRepository cache cleared");
    }
}

void AlarmConfigRepository::clearCacheForId(int id) {
    IRepository<AlarmConfigEntity>::clearCacheForId(id);
    if (logger_) {
        logger_->Debug("AlarmConfigRepository cache cleared for ID: " + std::to_string(id));
    }
}

std::map<std::string, int> AlarmConfigRepository::getCacheStats() const {
    return IRepository<AlarmConfigEntity>::getCacheStats();
}

// =============================================================================
// 내부 헬퍼 메서드들 (DeviceRepository 패턴)
// =============================================================================

AlarmConfigEntity AlarmConfigRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    AlarmConfigEntity entity;
    
    try {
        DatabaseAbstractionLayer db_layer;
        
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
        
        it = row.find("data_point_id");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setDataPointId(std::stoi(it->second));
        }
        
        it = row.find("virtual_point_id");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setVirtualPointId(std::stoi(it->second));
        }
        
        // 알람 기본 정보
        if ((it = row.find("alarm_name")) != row.end()) entity.setName(it->second);
        if ((it = row.find("description")) != row.end()) entity.setDescription(it->second);
        
        if ((it = row.find("severity")) != row.end()) {
            entity.setSeverity(AlarmConfigEntity::stringToSeverity(it->second));
        }
        
        if ((it = row.find("condition_type")) != row.end()) {
            entity.setConditionType(AlarmConfigEntity::stringToConditionType(it->second));
        }
        
        // 임계값들
        if ((it = row.find("threshold_value")) != row.end()) {
            entity.setThresholdValue(std::stod(it->second));
        }
        if ((it = row.find("high_limit")) != row.end()) {
            entity.setHighLimit(std::stod(it->second));
        }
        if ((it = row.find("low_limit")) != row.end()) {
            entity.setLowLimit(std::stod(it->second));
        }
        if ((it = row.find("timeout_seconds")) != row.end()) {
            entity.setTimeoutSeconds(std::stoi(it->second));
        }
        
        // 제어 설정
        it = row.find("is_enabled");
        if (it != row.end()) {
            entity.setEnabled(db_layer.parseBoolean(it->second));
        }
        
        it = row.find("auto_acknowledge");
        if (it != row.end()) {
            entity.setAutoAcknowledge(db_layer.parseBoolean(it->second));
        }
        
        if ((it = row.find("delay_seconds")) != row.end()) {
            entity.setDelaySeconds(std::stoi(it->second));
        }
        if ((it = row.find("message_template")) != row.end()) {
            entity.setMessageTemplate(it->second);
        }
        
        // 타임스탬프는 DB에서 자동 설정됨 (DATETIME DEFAULT CURRENT_TIMESTAMP)
        // DB에서 읽어온 값을 파싱해야 하지만, 현재는 기본값 사용
        // entity.setCreatedAt(std::chrono::system_clock::now());
        // entity.setUpdatedAt(std::chrono::system_clock::now());
        
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigRepository::mapRowToEntity failed: " + std::string(e.what()));
        throw;
    }
}

std::vector<AlarmConfigEntity> AlarmConfigRepository::mapResultToEntities(
    const std::vector<std::map<std::string, std::string>>& result) {
    
    std::vector<AlarmConfigEntity> entities;
    entities.reserve(result.size());
    
    for (const auto& row : result) {
        try {
            entities.push_back(mapRowToEntity(row));
        } catch (const std::exception& e) {
            logger_->Warn("AlarmConfigRepository::mapResultToEntities - Failed to map row: " + std::string(e.what()));
        }
    }
    
    return entities;
}

std::map<std::string, std::string> AlarmConfigRepository::entityToParams(const AlarmConfigEntity& entity) {
    DatabaseAbstractionLayer db_layer;
    
    std::map<std::string, std::string> params;
    
    // 기본 정보
    params["tenant_id"] = std::to_string(entity.getTenantId());
    params["site_id"] = std::to_string(entity.getSiteId());
    
    if (entity.getDataPointId() > 0) {
        params["data_point_id"] = std::to_string(entity.getDataPointId());
    } else {
        params["data_point_id"] = "NULL";
    }
    
    if (entity.getVirtualPointId() > 0) {
        params["virtual_point_id"] = std::to_string(entity.getVirtualPointId());
    } else {
        params["virtual_point_id"] = "NULL";
    }
    
    // 알람 정보
    params["alarm_name"] = entity.getName();
    params["description"] = entity.getDescription();
    params["severity"] = AlarmConfigEntity::severityToString(entity.getSeverity());
    params["condition_type"] = AlarmConfigEntity::conditionTypeToString(entity.getConditionType());
    
    // 임계값들
    params["threshold_value"] = std::to_string(entity.getThresholdValue());
    params["high_limit"] = std::to_string(entity.getHighLimit());
    params["low_limit"] = std::to_string(entity.getLowLimit());
    params["timeout_seconds"] = std::to_string(entity.getTimeoutSeconds());
    
    // 제어 설정
    params["is_enabled"] = db_layer.formatBoolean(entity.isEnabled());
    params["auto_acknowledge"] = db_layer.formatBoolean(entity.isAutoAcknowledge());
    params["delay_seconds"] = std::to_string(entity.getDelaySeconds());
    params["message_template"] = entity.getMessageTemplate();
    
    params["created_at"] = db_layer.getCurrentTimestamp();
    params["updated_at"] = db_layer.getCurrentTimestamp();
    
    return params;
}

bool AlarmConfigRepository::ensureTableExists() {
    try {
        const std::string base_create_query = R"(
            CREATE TABLE IF NOT EXISTS alarm_definitions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                tenant_id INTEGER NOT NULL,
                site_id INTEGER,
                data_point_id INTEGER,
                virtual_point_id INTEGER,
                
                -- 알람 기본 정보
                alarm_name VARCHAR(100) NOT NULL,
                description TEXT,
                severity VARCHAR(20) NOT NULL DEFAULT 'MEDIUM',
                condition_type VARCHAR(30) NOT NULL DEFAULT 'GREATER_THAN',
                
                -- 임계값 설정
                threshold_value REAL NOT NULL DEFAULT 0.0,
                high_limit REAL DEFAULT 100.0,
                low_limit REAL DEFAULT 0.0,
                timeout_seconds INTEGER DEFAULT 30,
                
                -- 제어 설정
                is_enabled BOOLEAN DEFAULT 1,
                auto_acknowledge BOOLEAN DEFAULT 0,
                delay_seconds INTEGER DEFAULT 0,
                message_template TEXT DEFAULT 'Alarm: {{ALARM_NAME}} - Value: {{CURRENT_VALUE}}',
                
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                
                FOREIGN KEY (tenant_id) REFERENCES tenants(id) ON DELETE CASCADE,
                FOREIGN KEY (site_id) REFERENCES sites(id) ON DELETE CASCADE,
                FOREIGN KEY (data_point_id) REFERENCES data_points(id) ON DELETE CASCADE,
                FOREIGN KEY (virtual_point_id) REFERENCES virtual_points(id) ON DELETE CASCADE
            )
        )";
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeCreateTable(base_create_query);
        
        if (success) {
            logger_->Debug("AlarmConfigRepository::ensureTableExists - Table creation/check completed");
        } else {
            logger_->Error("AlarmConfigRepository::ensureTableExists - Table creation failed");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigRepository::ensureTableExists failed: " + std::string(e.what()));
        return false;
    }
}

bool AlarmConfigRepository::validateAlarmConfig(const AlarmConfigEntity& entity) const {
    if (!entity.isValid()) {
        logger_->Warn("AlarmConfigRepository::validateAlarmConfig - Invalid alarm config: " + entity.getName());
        return false;
    }
    
    if (entity.getName().empty()) {
        logger_->Warn("AlarmConfigRepository::validateAlarmConfig - Alarm name is empty");
        return false;
    }
    
    if (entity.getTenantId() <= 0) {
        logger_->Warn("AlarmConfigRepository::validateAlarmConfig - Invalid tenant ID for: " + entity.getName());
        return false;
    }
    
    // 데이터포인트 또는 가상포인트 중 하나는 있어야 함
    if (entity.getDataPointId() <= 0 && entity.getVirtualPointId() <= 0) {
        logger_->Warn("AlarmConfigRepository::validateAlarmConfig - No data point or virtual point for: " + entity.getName());
        return false;
    }
    
    return true;
}

// =============================================================================
// SQL 빌더 헬퍼 메서드들
// =============================================================================

std::string AlarmConfigRepository::buildWhereClause(const std::vector<QueryCondition>& conditions) const {
    if (conditions.empty()) return "";
    
    std::string clause = " WHERE ";
    for (size_t i = 0; i < conditions.size(); ++i) {
        if (i > 0) clause += " AND ";
        clause += conditions[i].field + " " + conditions[i].operation + " " + conditions[i].value;
    }
    return clause;
}

std::string AlarmConfigRepository::buildOrderByClause(const std::optional<OrderBy>& order_by) const {
    if (!order_by.has_value()) return "";
    return " ORDER BY " + order_by->field + (order_by->ascending ? " ASC" : " DESC");
}

std::string AlarmConfigRepository::buildLimitClause(const std::optional<Pagination>& pagination) const {
    if (!pagination.has_value()) return "";
    return " LIMIT " + std::to_string(pagination->getLimit()) + 
           " OFFSET " + std::to_string(pagination->getOffset());
}

QueryCondition AlarmConfigRepository::buildSeverityCondition(AlarmConfigEntity::Severity severity) const {
    std::string severity_str = AlarmConfigEntity::severityToString(severity);
    return QueryCondition("severity", "=", severity_str);
}

// =============================================================================
// 유틸리티 함수들
// =============================================================================

std::string AlarmConfigRepository::escapeString(const std::string& str) const {
    std::string escaped = str;
    size_t pos = 0;
    while ((pos = escaped.find("'", pos)) != std::string::npos) {
        escaped.replace(pos, 1, "''");
        pos += 2;
    }
    return escaped;
}

std::string AlarmConfigRepository::formatTimestamp(const std::chrono::system_clock::time_point& timestamp) const {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne