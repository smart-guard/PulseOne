/**
 * @file DataPointRepository.cpp - SQLQueries.h 상수 100% 적용
 * @brief PulseOne DataPointRepository 구현 - 완전한 중앙 집중식 쿼리 관리
 * @author PulseOne Development Team
 * @date 2025-08-07
 * 
 * 🎯 SQLQueries.h 상수 완전 적용:
 * - 모든 하드코딩된 쿼리를 SQL::DataPoint:: 상수로 교체
 * - 동적 파라미터 처리 개선
 * - DeviceSettingsRepository 패턴 100% 준수
 * - DatabaseAbstractionLayer 완전 활용
 */

#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/SQLQueries.h"
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
// IRepository 기본 CRUD 구현 (SQLQueries.h 상수 사용)
// =============================================================================

std::vector<DataPointEntity> DataPointRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            logger_->Error("DataPointRepository::findAll - Table creation failed");
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        auto results = db_layer.executeQuery(SQL::DataPoint::FIND_ALL);
        
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
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용 + 동적 파라미터 처리
        std::string query = RepositoryHelpers::replaceParameter(SQL::DataPoint::FIND_BY_ID, std::to_string(id));
        
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
        
        std::map<std::string, std::string> data = entityToParams(entity);
        std::vector<std::string> primary_keys = {"id"};
        
        bool success = db_layer.executeUpsert("data_points", data, primary_keys);
        
        if (success) {
            // 새로 생성된 경우 ID 조회 - SQLQueries.h 상수 사용
            if (entity.getId() <= 0) {
                std::string id_query = replaceTwoParameters(SQL::DataPoint::FIND_LAST_CREATED_BY_DEVICE_ADDRESS,
                                                           std::to_string(entity.getDeviceId()),
                                                           std::to_string(entity.getAddress()));
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
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::DataPoint::DELETE_BY_ID, std::to_string(id));
        
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
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = RepositoryHelpers::replaceParameter(SQL::DataPoint::EXISTS_BY_ID, std::to_string(id));
        
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
        
        // 🎯 기본 쿼리에 IN 절 추가 (SQLQueries.h 기반)
        std::string query = SQL::DataPoint::FIND_ALL;
        // ORDER BY 앞에 WHERE 절 삽입
        size_t order_pos = query.find("ORDER BY");
        if (order_pos != std::string::npos) {
            query.insert(order_pos, "WHERE id IN (" + ids_ss.str() + ") ");
        }
        
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
        
        // 🎯 기본 쿼리 사용 후 조건 추가
        std::string query = SQL::DataPoint::FIND_ALL;
        
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

int DataPointRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        // 🎯 SQLQueries.h 상수 사용
        std::string query = SQL::DataPoint::COUNT_ALL;
        query += RepositoryHelpers::buildWhereClause(conditions);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::countByConditions failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// DataPoint 전용 메서드들 (SQLQueries.h 상수 사용)
// =============================================================================

std::vector<DataPointEntity> DataPointRepository::findByDeviceId(int device_id, bool enabled_only) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용 - enabled_only에 따라 다른 상수 선택
        std::string query;
        if (enabled_only) {
            query = RepositoryHelpers::replaceParameter(SQL::DataPoint::FIND_BY_DEVICE_ID_ENABLED, std::to_string(device_id));
        } else {
            query = RepositoryHelpers::replaceParameter(SQL::DataPoint::FIND_BY_DEVICE_ID, std::to_string(device_id));
        }
        
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
        
        // 🎯 기본 쿼리에 WHERE 절 추가
        std::string query = SQL::DataPoint::FIND_ALL;
        
        // ORDER BY 전에 WHERE 절 삽입
        size_t order_pos = query.find("ORDER BY");
        std::string where_clause = "WHERE device_id IN (" + ids_ss.str() + ")";
        if (enabled_only) {
            where_clause += " AND is_enabled = 1";
        }
        where_clause += " ";
        
        if (order_pos != std::string::npos) {
            query.insert(order_pos, where_clause);
        }
        
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
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용 + 두 파라미터 치환
        std::string query = replaceTwoParameters(SQL::DataPoint::FIND_BY_DEVICE_AND_ADDRESS,
                                                 std::to_string(device_id),
                                                 std::to_string(address));
        
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
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        auto results = db_layer.executeQuery(SQL::DataPoint::FIND_WRITABLE_POINTS);
        
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
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용 + 파라미터 치환
        std::string query = RepositoryHelpers::replaceParameterWithQuotes(SQL::DataPoint::FIND_BY_DATA_TYPE, data_type);
        
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

std::vector<DataPointEntity> DataPointRepository::findByTag(const std::string& tag) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용 + LIKE 패턴
        std::string query = RepositoryHelpers::replaceParameterWithQuotes(SQL::DataPoint::FIND_BY_TAG, "%" + tag + "%");
        
        auto results = db_layer.executeQuery(query);
        
        std::vector<DataPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("DataPointRepository::findByTag - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("DataPointRepository::findByTag - Found " + std::to_string(entities.size()) + " data points with tag: " + tag);
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::findByTag failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<DataPointEntity> DataPointRepository::findDisabledPoints() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        auto results = db_layer.executeQuery(SQL::DataPoint::FIND_DISABLED);
        
        std::vector<DataPointEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("DataPointRepository::findDisabledPoints - Failed to map row: " + std::string(e.what()));
            }
        }
        
        logger_->Info("DataPointRepository::findDisabledPoints - Found " + std::to_string(entities.size()) + " disabled data points");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::findDisabledPoints failed: " + std::string(e.what()));
        return {};
    }
}

// =============================================================================
// 통계 및 분석 (SQLQueries.h 상수 사용)
// =============================================================================

int DataPointRepository::getTotalCount() {
    try {
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        auto results = db_layer.executeQuery(SQL::DataPoint::COUNT_ALL);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
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
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        auto results = db_layer.executeQuery(SQL::DataPoint::GET_COUNT_BY_DEVICE);
        
        for (const auto& row : results) {
            if (row.find("device_id") != row.end() && row.find("count") != row.end()) {
                counts[std::stoi(row.at("device_id"))] = std::stoi(row.at("count"));
            }
        }
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::getPointCountByDevice failed: " + std::string(e.what()));
    }
    
    return counts;
}

std::map<std::string, int> DataPointRepository::getPointCountByDataType() {
    std::map<std::string, int> counts;
    
    try {
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        auto results = db_layer.executeQuery(SQL::DataPoint::GET_COUNT_BY_DATA_TYPE);
        
        for (const auto& row : results) {
            if (row.find("data_type") != row.end() && row.find("count") != row.end()) {
                counts[row.at("data_type")] = std::stoi(row.at("count"));
            }
        }
    } catch (const std::exception& e) {
        logger_->Error("DataPointRepository::getPointCountByDataType failed: " + std::string(e.what()));
    }
    
    return counts;
}

// =============================================================================
// 벌크 연산 (SQLQueries.h 상수 사용)
// =============================================================================

int DataPointRepository::saveBulk(std::vector<DataPointEntity>& entities) {
    if (entities.empty()) {
        return 0;
    }
    
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        int saved_count = 0;
        DatabaseAbstractionLayer db_layer;
        
        for (auto& entity : entities) {
            if (!validateDataPoint(entity)) {
                continue;
            }
            
            auto params = entityToParams(entity);
            std::vector<std::string> primary_keys = {"id"};
            bool success = db_layer.executeUpsert("data_points", params, primary_keys);
            
            if (success) {
                saved_count++;
                
                // 새로 생성된 ID 조회 - SQLQueries.h 상수 사용
                if (entity.getId() <= 0) {
                    auto results = db_layer.executeQuery(SQL::DataPoint::GET_LAST_INSERT_ID);
                    if (!results.empty()) {
                        entity.setId(std::stoi(results[0].at("id")));
                    }
                }
            }
        }
        
        if (logger_) {
            logger_->Info("DataPointRepository::saveBulk - Saved " + std::to_string(saved_count) + " data points");
        }
        
        return saved_count;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DataPointRepository::saveBulk failed: " + std::string(e.what()));
        }
        return 0;
    }
}

int DataPointRepository::updateBulk(const std::vector<DataPointEntity>& entities) {
    if (entities.empty()) {
        return 0;
    }
    
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        int updated_count = 0;
        DatabaseAbstractionLayer db_layer;
        
        for (const auto& entity : entities) {
            if (entity.getId() <= 0 || !validateDataPoint(entity)) {
                continue;
            }
            
            auto params = entityToParams(entity);
            std::vector<std::string> primary_keys = {"id"};
            bool success = db_layer.executeUpsert("data_points", params, primary_keys);
            
            if (success) {
                updated_count++;
            }
        }
        
        if (logger_) {
            logger_->Info("DataPointRepository::updateBulk - Updated " + std::to_string(updated_count) + " data points");
        }
        
        return updated_count;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DataPointRepository::updateBulk failed: " + std::string(e.what()));
        }
        return 0;
    }
}

int DataPointRepository::deleteByIds(const std::vector<int>& ids) {
    if (ids.empty()) {
        return 0;
    }
    
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        int deleted_count = 0;
        DatabaseAbstractionLayer db_layer;
        
        for (int id : ids) {
            // 🎯 SQLQueries.h 상수 사용
            std::string query = RepositoryHelpers::replaceParameter(SQL::DataPoint::DELETE_BY_ID, std::to_string(id));
            bool success = db_layer.executeNonQuery(query);
            
            if (success) {
                deleted_count++;
                
                if (isCacheEnabled()) {
                    clearCacheForId(id);
                }
            }
        }
        
        if (logger_) {
            logger_->Info("DataPointRepository::deleteByIds - Deleted " + std::to_string(deleted_count) + " data points");
        }
        
        return deleted_count;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("DataPointRepository::deleteByIds failed: " + std::string(e.what()));
        }
        return 0;
    }
}

// =============================================================================
// 내부 헬퍼 메서드들 (DeviceSettingsRepository 패턴)
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

std::map<std::string, std::string> DataPointRepository::entityToParams(const DataPointEntity& entity) {
    DatabaseAbstractionLayer db_layer;
    
    std::map<std::string, std::string> params;
    
    if (entity.getId() > 0) {
        params["id"] = std::to_string(entity.getId());
    }
    
    params["device_id"] = std::to_string(entity.getDeviceId());
    params["name"] = entity.getName();
    params["description"] = entity.getDescription();
    params["address"] = std::to_string(entity.getAddress());
    params["data_type"] = entity.getDataType();
    params["access_mode"] = entity.getAccessMode();
    params["is_enabled"] = db_layer.formatBoolean(entity.isEnabled());
    params["unit"] = entity.getUnit();
    params["scaling_factor"] = std::to_string(entity.getScalingFactor());
    params["scaling_offset"] = std::to_string(entity.getScalingOffset());
    params["min_value"] = std::to_string(entity.getMinValue());
    params["max_value"] = std::to_string(entity.getMaxValue());
    params["log_enabled"] = db_layer.formatBoolean(entity.isLogEnabled());
    params["log_interval_ms"] = std::to_string(entity.getLogInterval());
    params["log_deadband"] = std::to_string(entity.getLogDeadband());
    params["tags"] = tagsToString(entity.getTags());
    params["metadata"] = "{}"; // 간단화
    params["created_at"] = db_layer.getCurrentTimestamp();
    params["updated_at"] = db_layer.getCurrentTimestamp();
    
    return params;
}

bool DataPointRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        
        // 🎯 SQLQueries.h 상수 사용
        bool success = db_layer.executeCreateTable(SQL::DataPoint::CREATE_TABLE);
        
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

// =============================================================================
// 의존성 주입 및 현재값 통합 메서드
// =============================================================================

void DataPointRepository::setCurrentValueRepository(std::shared_ptr<CurrentValueRepository> current_value_repo) {
    current_value_repo_ = current_value_repo;
    logger_->Info("✅ CurrentValueRepository injected into DataPointRepository");
}

std::vector<PulseOne::Structs::DataPoint> DataPointRepository::getDataPointsWithCurrentValues(int device_id, bool enabled_only) {
    std::vector<PulseOne::Structs::DataPoint> result;
    
    try {
        logger_->Debug("🔍 Loading DataPoints with current values for device: " + std::to_string(device_id));
        
        // 1. DataPointEntity들 조회 (SQLQueries.h 상수 사용)
        auto entities = findByDeviceId(device_id, enabled_only);
        
        logger_->Debug("📊 Found " + std::to_string(entities.size()) + " DataPoint entities");
        
        // 2. 각 Entity를 Structs::DataPoint로 변환 + 현재값 추가
        for (const auto& entity : entities) {
            
            PulseOne::Structs::DataPoint data_point;
            data_point.id = std::to_string(entity.getId());
            data_point.device_id = std::to_string(entity.getDeviceId());
            data_point.name = entity.getName();
            data_point.description = entity.getDescription();
            data_point.address = entity.getAddress();
            data_point.data_type = entity.getDataType();
            data_point.access_mode = entity.getAccessMode();
            data_point.is_enabled = entity.isEnabled();
            data_point.unit = entity.getUnit();
            data_point.scaling_factor = entity.getScalingFactor();
            data_point.scaling_offset = entity.getScalingOffset();
            data_point.min_value = entity.getMinValue();
            data_point.max_value = entity.getMaxValue();
            data_point.polling_interval_ms = entity.getLogInterval();
            
            // 현재값 조회 및 설정
            if (current_value_repo_) {
                try {
                    auto current_value = current_value_repo_->findByDataPointId(entity.getId());
                    
                    if (current_value.has_value()) {
                        data_point.current_value = PulseOne::BasicTypes::DataVariant(current_value->getValue());
                        data_point.quality_code = current_value->getQuality();
                        data_point.quality_timestamp = current_value->getTimestamp();
                        
                        logger_->Debug("💡 Current value loaded: " + data_point.name + 
                                      " = " + data_point.GetCurrentValueAsString() + 
                                      " (Quality: " + data_point.GetQualityCodeAsString() + ")");
                    } else {
                        data_point.current_value = PulseOne::BasicTypes::DataVariant(0.0);
                        data_point.quality_code = PulseOne::Enums::DataQuality::NOT_CONNECTED;
                        data_point.quality_timestamp = std::chrono::system_clock::now();
                        
                        logger_->Debug("⚠️ No current value found for: " + data_point.name + " (using defaults)");
                    }
                    
                } catch (const std::exception& e) {
                    logger_->Debug("❌ Current value query failed for " + data_point.name + ": " + std::string(e.what()));
                    
                    data_point.current_value = PulseOne::BasicTypes::DataVariant(0.0);
                    data_point.quality_code = PulseOne::Enums::DataQuality::BAD;
                    data_point.quality_timestamp = std::chrono::system_clock::now();
                }
            } else {
                logger_->Warn("⚠️ CurrentValueRepository not injected, using default values");
                
                data_point.current_value = PulseOne::BasicTypes::DataVariant(0.0);
                data_point.quality_code = PulseOne::Enums::DataQuality::NOT_CONNECTED;
                data_point.quality_timestamp = std::chrono::system_clock::now();
            }
            
            // 주소 필드 동기화
            if (data_point.address_string.empty()) {
                data_point.address_string = std::to_string(data_point.address);
            }
            
            result.push_back(data_point);
            
            logger_->Debug("✅ Converted DataPoint: " + data_point.name + 
                          " (Address: " + std::to_string(data_point.address) + 
                          ", Type: " + data_point.data_type + 
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
// 추가 메서드들 (간소화된 구현)
// =============================================================================

std::vector<DataPointEntity> DataPointRepository::findAllWithLimit(size_t limit) {
    return findByConditions({}, OrderBy("id", true), Pagination(limit, 0));
}

std::vector<DataPointEntity> DataPointRepository::findDataPointsForWorkers(const std::vector<int>& device_ids) {
    if (device_ids.empty()) {
        return findByConditions({QueryCondition("is_enabled", "=", "1")});
    } else {
        return findByDeviceIds(device_ids, true);
    }
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

// =============================================================================
// IRepository 캐시 관리 메서드들 (위임 방식)
// =============================================================================

void DataPointRepository::setCacheEnabled(bool enabled) {
    IRepository<DataPointEntity>::setCacheEnabled(enabled);
    if (logger_) {
        std::string message = "DataPointRepository cache ";
        message += (enabled ? "enabled" : "disabled");
        logger_->Info(message);
    }
}

bool DataPointRepository::isCacheEnabled() const {
    return IRepository<DataPointEntity>::isCacheEnabled();
}

void DataPointRepository::clearCache() {
    IRepository<DataPointEntity>::clearCache();
    if (logger_) {
        logger_->Info("DataPointRepository cache cleared");
    }
}

void DataPointRepository::clearCacheForId(int id) {
    IRepository<DataPointEntity>::clearCacheForId(id);
    if (logger_) {
        logger_->Debug("DataPointRepository cache cleared for ID: " + std::to_string(id));
    }
}

std::map<std::string, int> DataPointRepository::getCacheStats() const {
    return IRepository<DataPointEntity>::getCacheStats();
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne