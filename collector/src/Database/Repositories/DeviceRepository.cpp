/**
 * @file DeviceRepository.cpp
 * @brief PulseOne DeviceRepository 구현 - SQLQueries.h 상수 100% 적용
 * @author PulseOne Development Team
 * @date 2025-08-07
 * 
 * 🎯 SQLQueries.h 상수 완전 적용:
 * - 모든 하드코딩된 쿼리를 SQL:: 상수로 교체
 * - 동적 파라미터 처리 개선
 * - 쿼리 중앙 관리로 유지보수성 향상
 * - DeviceSettingsRepository 패턴 유지
 */

#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/SQLQueries.h" 
#include "Database/DatabaseAbstractionLayer.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 기본 CRUD 구현 (SQLQueries.h 상수 사용)
// =============================================================================

std::vector<DeviceEntity> DeviceRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            logger_->Error("DeviceRepository::findAll - Table creation failed");
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::Device::FIND_ALL);  // ✅ 상수 사용
        
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
        // 캐시 확인 먼저
        if (isCacheEnabled()) {
            if (auto cached = getCachedEntity(id)) {
                logger_->Debug("DeviceRepository::findById - Cache hit for ID: " + std::to_string(id));
                return cached;
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용 + 동적 파라미터 처리
        std::string query = RepositoryHelpers::replaceParameter(SQL::Device::FIND_BY_ID, std::to_string(id));
        
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty()) {
            auto entity = mapRowToEntity(results[0]);
            
            // 캐시에 저장
            if (isCacheEnabled()) {
                cacheEntity(entity);
            }
            
            logger_->Debug("DeviceRepository::findById - Found device: " + entity.getName());
            return entity;
        }
        
        logger_->Debug("DeviceRepository::findById - Device not found with ID: " + std::to_string(id));
        return std::nullopt;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::findById failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool DeviceRepository::save(DeviceEntity& entity) {
    try {
        if (!validateDevice(entity)) {
            logger_->Error("DeviceRepository::save - Invalid device: " + entity.getName());
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🔧 entityToParams 메서드 사용하여 맵 생성
        std::map<std::string, std::string> data = entityToParams(entity);
        
        std::vector<std::string> primary_keys = {"id"};
        
        bool success = db_layer.executeUpsert("devices", data, primary_keys);
        
        if (success) {
            // 새로 생성된 경우 ID 조회 - SQLQueries.h 상수 사용
            if (entity.getId() <= 0) {
                auto id_result = db_layer.executeQuery(SQL::Device::GET_LAST_INSERT_ID);
                if (!id_result.empty()) {
                    entity.setId(std::stoi(id_result[0].at("id")));
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
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::Device::DELETE_BY_ID, std::to_string(id));
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            
            logger_->Info("DeviceRepository::deleteById - Deleted device ID: " + std::to_string(id));
        } else {
            logger_->Error("DeviceRepository::deleteById - Failed to delete device ID: " + std::to_string(id));
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
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::Device::EXISTS_BY_ID, std::to_string(id));
        
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
        
        // 🎯 기본 쿼리에 IN 절 추가 (SQLQueries.h 기반)
        std::string query = SQL::Device::FIND_ALL;
        // ORDER BY 앞에 WHERE 절 삽입
        size_t order_pos = query.find("ORDER BY");
        if (order_pos != std::string::npos) {
            query.insert(order_pos, "WHERE id IN (" + ids_ss.str() + ") ");
        }
        
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
        
        logger_->Info("DeviceRepository::findByIds - Found " + std::to_string(entities.size()) + " devices for " + std::to_string(ids.size()) + " IDs");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::findByIds failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<DeviceEntity> DeviceRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        // 🎯 기본 쿼리 사용 후 조건 추가
        std::string query = SQL::Device::FIND_ALL;
        
        // ORDER BY 제거 후 조건 추가
        size_t order_pos = query.find("ORDER BY");
        if (order_pos != std::string::npos) {
            query = query.substr(0, order_pos);
        }
        
        query += RepositoryHelpers::buildWhereClause(conditions);
        query += RepositoryHelpers::buildOrderByClause(order_by);
        query += RepositoryHelpers::buildLimitClause(pagination);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<DeviceEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("DeviceRepository::findByConditions - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Debug("DeviceRepository::findByConditions - Found " + std::to_string(entities.size()) + " devices");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::findByConditions failed: " + std::string(e.what()));
        return {};
    }
}

int DeviceRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = SQL::Device::COUNT_ALL;
        query += RepositoryHelpers::buildWhereClause(conditions);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::countByConditions failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// Device 전용 메서드들 (SQLQueries.h 상수 사용)
// =============================================================================

std::vector<DeviceEntity> DeviceRepository::findByProtocol(const std::string& protocol_type) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용 + 파라미터 치환
        std::string query = RepositoryHelpers::replaceParameterWithQuotes(SQL::Device::FIND_BY_PROTOCOL, protocol_type);
        
        auto results = db_layer.executeQuery(query);
        
        std::vector<DeviceEntity> entities = mapResultToEntities(results);
        
        logger_->Info("DeviceRepository::findByProtocol - Found " + std::to_string(entities.size()) + " devices for protocol: " + protocol_type);
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
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::Device::FIND_BY_TENANT, std::to_string(tenant_id));
        
        auto results = db_layer.executeQuery(query);
        
        std::vector<DeviceEntity> entities = mapResultToEntities(results);
        
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
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::Device::FIND_BY_SITE, std::to_string(site_id));
        
        auto results = db_layer.executeQuery(query);
        
        std::vector<DeviceEntity> entities = mapResultToEntities(results);
        
        logger_->Info("DeviceRepository::findBySite - Found " + std::to_string(entities.size()) + " devices for site " + std::to_string(site_id));
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::findBySite failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<DeviceEntity> DeviceRepository::findEnabledDevices() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        auto results = db_layer.executeQuery(SQL::Device::FIND_ENABLED);
        
        std::vector<DeviceEntity> entities = mapResultToEntities(results);
        
        logger_->Info("DeviceRepository::findEnabledDevices - Found " + std::to_string(entities.size()) + " enabled devices");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::findEnabledDevices failed: " + std::string(e.what()));
        return {};
    }
}

std::map<std::string, std::vector<DeviceEntity>> DeviceRepository::groupByProtocol() {
    std::map<std::string, std::vector<DeviceEntity>> grouped;
    
    try {
        auto devices = findAll();
        for (const auto& device : devices) {
            grouped[device.getProtocolType()].push_back(device);
        }
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::groupByProtocol failed: " + std::string(e.what()));
    }
    
    return grouped;
}

// =============================================================================
// 벌크 연산 (DeviceSettingsRepository 패턴)
// =============================================================================

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
// 실시간 디바이스 관리
// =============================================================================

bool DeviceRepository::enableDevice(int device_id) {
    return updateDeviceStatus(device_id, true);
}

bool DeviceRepository::disableDevice(int device_id) {
    return updateDeviceStatus(device_id, false);
}

bool DeviceRepository::updateDeviceStatus(int device_id, bool is_enabled) {
    try {
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용 + 파라미터 치환
        std::string query = SQL::Device::UPDATE_STATUS;
        
        // 첫 번째 ? → is_enabled 값
        size_t pos = query.find('?');
        if (pos != std::string::npos) {
            query.replace(pos, 1, is_enabled ? "1" : "0");
        }
        
        // 두 번째 ? → 현재 시간
        pos = query.find('?');
        if (pos != std::string::npos) {
            query.replace(pos, 1, "'" + formatTimestamp(std::chrono::system_clock::now()) + "'");
        }
        
        // 세 번째 ? → device_id
        pos = query.find('?');
        if (pos != std::string::npos) {
            query.replace(pos, 1, std::to_string(device_id));
        }
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                clearCacheForId(device_id);
            }
            logger_->Info("DeviceRepository::updateDeviceStatus - " + 
                         std::string(is_enabled ? "Enabled" : "Disabled") + 
                         " device ID: " + std::to_string(device_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::updateDeviceStatus failed: " + std::string(e.what()));
        return false;
    }
}

bool DeviceRepository::updateEndpoint(int device_id, const std::string& endpoint) {
    try {
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용 + 파라미터 치환
        std::string query = SQL::Device::UPDATE_ENDPOINT;
        
        // 첫 번째 ? → endpoint 값
        size_t pos = query.find('?');
        if (pos != std::string::npos) {
            query.replace(pos, 1, "'" + RepositoryHelpers::escapeString(endpoint) + "'");
        }
        
        // 두 번째 ? → 현재 시간
        pos = query.find('?');
        if (pos != std::string::npos) {
            query.replace(pos, 1, "'" + formatTimestamp(std::chrono::system_clock::now()) + "'");
        }
        
        // 세 번째 ? → device_id
        pos = query.find('?');
        if (pos != std::string::npos) {
            query.replace(pos, 1, std::to_string(device_id));
        }
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                clearCacheForId(device_id);
            }
            logger_->Info("DeviceRepository::updateEndpoint - Updated endpoint for device ID: " + std::to_string(device_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::updateEndpoint failed: " + std::string(e.what()));
        return false;
    }
}

bool DeviceRepository::updateConfig(int device_id, const std::string& config) {
    try {
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용 + 파라미터 치환
        std::string query = SQL::Device::UPDATE_CONFIG;
        
        // 첫 번째 ? → config 값
        size_t pos = query.find('?');
        if (pos != std::string::npos) {
            query.replace(pos, 1, "'" + RepositoryHelpers::escapeString(config) + "'");
        }
        
        // 두 번째 ? → 현재 시간
        pos = query.find('?');
        if (pos != std::string::npos) {
            query.replace(pos, 1, "'" + formatTimestamp(std::chrono::system_clock::now()) + "'");
        }
        
        // 세 번째 ? → device_id
        pos = query.find('?');
        if (pos != std::string::npos) {
            query.replace(pos, 1, std::to_string(device_id));
        }
        
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                clearCacheForId(device_id);
            }
            logger_->Info("DeviceRepository::updateConfig - Updated config for device ID: " + std::to_string(device_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::updateConfig failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 통계 및 분석 (SQLQueries.h 상수 사용)
// =============================================================================

std::string DeviceRepository::getDeviceStatistics() const {
    try {
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 전체 개수 - SQLQueries.h 상수 사용
        auto total_result = db_layer.executeQuery(SQL::Device::COUNT_ALL);
        int total_count = 0;
        if (!total_result.empty() && total_result[0].find("count") != total_result[0].end()) {
            total_count = std::stoi(total_result[0].at("count"));
        }
        
        // 🎯 활성화된 개수 - SQLQueries.h 상수 사용
        auto enabled_result = db_layer.executeQuery(SQL::Device::COUNT_ENABLED);
        int enabled_count = 0;
        if (!enabled_result.empty() && enabled_result[0].find("count") != enabled_result[0].end()) {
            enabled_count = std::stoi(enabled_result[0].at("count"));
        }
        
        // JSON 형태로 반환
        std::ostringstream oss;
        oss << "{ \"total\": " << total_count << ", \"enabled\": " << enabled_count << ", \"disabled\": " << (total_count - enabled_count) << " }";
        return oss.str();
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DeviceRepository::getDeviceStatistics failed: " + std::string(e.what()));
        }
        return "{ \"error\": \"Statistics calculation failed\" }";
    }
}

std::vector<DeviceEntity> DeviceRepository::findInactiveDevices() const {
    try {
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        auto results = db_layer.executeQuery(SQL::Device::FIND_DISABLED);
        
        std::vector<DeviceEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                DeviceEntity entity;
                // mapRowToEntity는 const가 아니므로 여기서는 간단 구현
                auto it = row.find("name");
                if (it != row.end()) {
                    entity.setName(it->second);
                }
                
                it = row.find("id");
                if (it != row.end()) {
                    entity.setId(std::stoi(it->second));
                }
                
                entities.push_back(entity);
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("DeviceRepository::findInactiveDevices - Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DeviceRepository::findInactiveDevices failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::map<std::string, int> DeviceRepository::getProtocolDistribution() const {
    std::map<std::string, int> distribution;
    
    try {
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        auto results = db_layer.executeQuery(SQL::Device::GET_PROTOCOL_DISTRIBUTION);
        
        for (const auto& row : results) {
            if (row.find("protocol_type") != row.end() && row.find("count") != row.end()) {
                distribution[row.at("protocol_type")] = std::stoi(row.at("count"));
            }
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DeviceRepository::getProtocolDistribution failed: " + std::string(e.what()));
        }
    }
    
    return distribution;
}

int DeviceRepository::getTotalCount() {
    return countByConditions({});
}

// =============================================================================
// 내부 헬퍼 메서드들 (DeviceSettingsRepository 패턴)
// =============================================================================

DeviceEntity DeviceRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    DeviceEntity entity;
    
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
        
        it = row.find("device_group_id");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setDeviceGroupId(std::stoi(it->second));
        }
        
        it = row.find("edge_server_id");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setEdgeServerId(std::stoi(it->second));
        }
        
        // 디바이스 기본 정보
        if ((it = row.find("name")) != row.end()) entity.setName(it->second);
        if ((it = row.find("description")) != row.end()) entity.setDescription(it->second);
        if ((it = row.find("device_type")) != row.end()) entity.setDeviceType(it->second);
        if ((it = row.find("manufacturer")) != row.end()) entity.setManufacturer(it->second);
        if ((it = row.find("model")) != row.end()) entity.setModel(it->second);
        if ((it = row.find("serial_number")) != row.end()) entity.setSerialNumber(it->second);
        
        // 통신 설정
        if ((it = row.find("protocol_type")) != row.end()) entity.setProtocolType(it->second);
        if ((it = row.find("endpoint")) != row.end()) entity.setEndpoint(it->second);
        if ((it = row.find("config")) != row.end()) entity.setConfig(it->second);
        
        // 상태 정보
        it = row.find("is_enabled");
        if (it != row.end()) {
            entity.setEnabled(db_layer.parseBoolean(it->second));
        }
        
        it = row.find("created_by");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setCreatedBy(std::stoi(it->second));
        }
        
        // 타임스탬프는 기본값 사용 (실제 구현에서는 파싱 필요)
        entity.setCreatedAt(std::chrono::system_clock::now());
        entity.setUpdatedAt(std::chrono::system_clock::now());
        
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("DeviceRepository::mapRowToEntity failed: " + std::string(e.what()));
        throw;
    }
}

std::vector<DeviceEntity> DeviceRepository::mapResultToEntities(
    const std::vector<std::map<std::string, std::string>>& result) {
    
    std::vector<DeviceEntity> entities;
    entities.reserve(result.size());
    
    for (const auto& row : result) {
        try {
            entities.push_back(mapRowToEntity(row));
        } catch (const std::exception& e) {
            logger_->Warn("DeviceRepository::mapResultToEntities - Failed to map row: " + std::string(e.what()));
        }
    }
    
    return entities;
}

std::map<std::string, std::string> DeviceRepository::entityToParams(const DeviceEntity& entity) {
    DatabaseAbstractionLayer db_layer;
    
    std::map<std::string, std::string> params;
    
    // 기본 정보 (ID는 AUTO_INCREMENT이므로 제외)
    params["tenant_id"] = std::to_string(entity.getTenantId());
    params["site_id"] = std::to_string(entity.getSiteId());
    
    if (entity.getDeviceGroupId().has_value()) {
        params["device_group_id"] = std::to_string(entity.getDeviceGroupId().value());
    } else {
        params["device_group_id"] = "NULL";
    }
    
    if (entity.getEdgeServerId().has_value()) {
        params["edge_server_id"] = std::to_string(entity.getEdgeServerId().value());
    } else {
        params["edge_server_id"] = "NULL";
    }
    
    // 디바이스 정보
    params["name"] = entity.getName();
    params["description"] = entity.getDescription();
    params["device_type"] = entity.getDeviceType();
    params["manufacturer"] = entity.getManufacturer();
    params["model"] = entity.getModel();
    params["serial_number"] = entity.getSerialNumber();
    
    // 통신 설정
    params["protocol_type"] = entity.getProtocolType();
    params["endpoint"] = entity.getEndpoint();
    params["config"] = entity.getConfig();
    
    // 상태 정보
    params["is_enabled"] = db_layer.formatBoolean(entity.isEnabled());
    
    if (entity.getInstallationDate().has_value()) {
        params["installation_date"] = formatTimestamp(entity.getInstallationDate().value());
    } else {
        params["installation_date"] = "NULL";
    }
    
    if (entity.getLastMaintenance().has_value()) {
        params["last_maintenance"] = formatTimestamp(entity.getLastMaintenance().value());
    } else {
        params["last_maintenance"] = "NULL";
    }
    
    if (entity.getCreatedBy().has_value()) {
        params["created_by"] = std::to_string(entity.getCreatedBy().value());
    } else {
        params["created_by"] = "NULL";
    }
    
    params["created_at"] = db_layer.getCurrentTimestamp();
    params["updated_at"] = db_layer.getCurrentTimestamp();
    
    return params;
}

bool DeviceRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        bool success = db_layer.executeCreateTable(SQL::Device::CREATE_TABLE);
        
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

// =============================================================================
// 유틸리티 함수들
// =============================================================================

std::string DeviceRepository::formatTimestamp(const std::chrono::system_clock::time_point& timestamp) const {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne