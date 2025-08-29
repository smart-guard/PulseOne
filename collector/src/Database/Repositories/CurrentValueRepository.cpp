// =============================================================================
// collector/src/Database/Repositories/CurrentValueRepository.cpp
// vector 초기화 방식 수정 - brace initialization 대신 push_back 사용
// =============================================================================

#include "Database/Repositories/CurrentValueRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Database/SQLQueries.h"
#include "Utils/LogManager.h"
#include "Common/Utils.h"

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 필수 구현 (CRUD)
// =============================================================================

std::vector<CurrentValueEntity> CurrentValueRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::CurrentValue::FIND_ALL);
        
        std::vector<CurrentValueEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().Warn("Failed to map row: " + std::string(e.what()));
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<CurrentValueEntity> CurrentValueRepository::findById(int id) {
    try {
        // 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                return cached.value();
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        std::string query = SQL::CurrentValue::FIND_BY_ID;
        
        // ✅ vector 초기화를 push_back으로 변경
        std::vector<std::string> params;
        params.push_back(std::to_string(id));
        
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        return entity;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("findById failed for ID " + std::to_string(id) + ": " + std::string(e.what()));
        return std::nullopt;
    }
}

bool CurrentValueRepository::save(CurrentValueEntity& entity) {
    try {
        if (!validateEntity(entity)) {
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto data = entityToParams(entity);
        
        // ✅ vector 초기화를 push_back으로 변경
        std::vector<std::string> primary_keys;
        primary_keys.push_back("point_id");
        
        bool success = db_layer.executeUpsert("current_values", data, primary_keys);
        
        if (success && isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("save failed: " + std::string(e.what()));
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
        
        DatabaseAbstractionLayer db_layer;
        std::string query = SQL::CurrentValue::DELETE_BY_ID;
        
        // ✅ vector 초기화를 push_back으로 변경
        std::vector<std::string> params;
        params.push_back(std::to_string(id));
        
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        bool success = db_layer.executeNonQuery(query);
        
        if (success && isCacheEnabled()) {
            clearCacheForId(id);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool CurrentValueRepository::exists(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        std::string query = SQL::CurrentValue::EXISTS_BY_ID;
        
        // ✅ vector 초기화를 push_back으로 변경
        std::vector<std::string> params;
        params.push_back(std::to_string(id));
        
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            int count = std::stoi(results[0].at("count"));
            return count > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("exists failed: " + std::string(e.what()));
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
        
        std::string ids_str = RepositoryHelpers::buildInClause(ids);
        std::string query = SQL::CurrentValue::FIND_BY_IDS;
        RepositoryHelpers::replaceStringPlaceholder(query, "%s", ids_str);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<CurrentValueEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().Warn("Failed to map row: " + std::string(e.what()));
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("findByIds failed: " + std::string(e.what()));
        return {};
    }
}

// =============================================================================
// CurrentValue 전용 메서드들
// =============================================================================

std::vector<CurrentValueEntity> CurrentValueRepository::findByDeviceId(int device_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        
        // DataPoint와 JOIN하여 device_id로 현재값들 조회
        std::string query = R"(
            SELECT cv.point_id, cv.current_value, cv.raw_value, cv.value_type,
                   cv.quality_code, cv.quality, cv.value_timestamp, 
                   cv.read_count, cv.write_count, cv.error_count, cv.updated_at
            FROM current_values cv
            INNER JOIN data_points dp ON cv.point_id = dp.id
            WHERE dp.device_id = ?
        )";
        
        // ✅ vector 초기화를 push_back으로 변경
        std::vector<std::string> params;
        params.push_back(std::to_string(device_id));
        
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        auto results = db_layer.executeQuery(query);
        
        std::vector<CurrentValueEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().Warn("Failed to map row: " + std::string(e.what()));
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("findByDeviceId failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<CurrentValueEntity> CurrentValueRepository::findByDataPointId(int data_point_id) {
    return findById(data_point_id);
}

std::vector<CurrentValueEntity> CurrentValueRepository::findByQuality(PulseOne::Enums::DataQuality quality) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        std::string query = SQL::CurrentValue::FIND_BY_QUALITY_CODE;
        
        // ✅ vector 초기화를 push_back으로 변경
        std::vector<std::string> params;
        params.push_back(std::to_string(static_cast<int>(quality)));
        
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        auto results = db_layer.executeQuery(query);
        
        std::vector<CurrentValueEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().Warn("Failed to map row: " + std::string(e.what()));
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("findByQuality failed: " + std::string(e.what()));
        return {};
    }
}

bool CurrentValueRepository::incrementReadCount(int point_id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto now_str = Utils::TimestampToDBString(Utils::GetCurrentTimestamp());
        
        std::string query = SQL::CurrentValue::INCREMENT_READ_COUNT;
        
        // ✅ vector 초기화를 push_back으로 변경 - 컴파일 에러 수정!
        std::vector<std::string> params;
        params.push_back(now_str);
        params.push_back(now_str);
        params.push_back(std::to_string(point_id));
        
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        bool success = db_layer.executeNonQuery(query);
        
        if (success && isCacheEnabled()) {
            clearCacheForId(point_id);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("incrementReadCount failed: " + std::string(e.what()));
        return false;
    }
}

bool CurrentValueRepository::incrementWriteCount(int point_id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto now_str = Utils::TimestampToDBString(Utils::GetCurrentTimestamp());
        
        std::string query = SQL::CurrentValue::INCREMENT_WRITE_COUNT;
        
        // ✅ vector 초기화를 push_back으로 변경 - 컴파일 에러 수정!
        std::vector<std::string> params;
        params.push_back(now_str);
        params.push_back(now_str);
        params.push_back(std::to_string(point_id));
        
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        bool success = db_layer.executeNonQuery(query);
        
        if (success && isCacheEnabled()) {
            clearCacheForId(point_id);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("incrementWriteCount failed: " + std::string(e.what()));
        return false;
    }
}

bool CurrentValueRepository::incrementErrorCount(int point_id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto now_str = Utils::TimestampToDBString(Utils::GetCurrentTimestamp());
        
        std::string query = SQL::CurrentValue::INCREMENT_ERROR_COUNT;
        
        // ✅ vector 초기화를 push_back으로 변경 - 컴파일 에러 수정!
        std::vector<std::string> params;
        params.push_back(now_str);
        params.push_back(std::to_string(point_id));
        
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        bool success = db_layer.executeNonQuery(query);
        
        if (success && isCacheEnabled()) {
            clearCacheForId(point_id);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("incrementErrorCount failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 캐시 관리 (IRepository 상속)
// =============================================================================

void CurrentValueRepository::clearCache() {
    IRepository::clearCache();
}

void CurrentValueRepository::clearCacheForId(int id) {
    IRepository::clearCacheForId(id);
}

bool CurrentValueRepository::isCacheEnabled() const {
    return IRepository::isCacheEnabled();
}

std::map<std::string, int> CurrentValueRepository::getCacheStats() const {
    return IRepository::getCacheStats();
}

int CurrentValueRepository::getTotalCount() {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::CurrentValue::COUNT_ALL);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("getTotalCount failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// IRepository 추상 메서드 구현
// =============================================================================

bool CurrentValueRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        return db_layer.executeNonQuery(SQL::CurrentValue::CREATE_TABLE);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ensureTableExists failed: " + std::string(e.what()));
        return false;
    }
}

CurrentValueEntity CurrentValueRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    CurrentValueEntity entity;
    
    try {
        // 기본 정보
        entity.setPointId(std::stoi(RepositoryHelpers::getRowValue(row, "point_id")));
        entity.setCurrentValue(RepositoryHelpers::getRowValue(row, "current_value"));
        entity.setRawValue(RepositoryHelpers::getRowValue(row, "raw_value"));
        entity.setValueType(RepositoryHelpers::getRowValue(row, "value_type"));
        
        // 품질 정보
        int quality_code = std::stoi(RepositoryHelpers::getRowValue(row, "quality_code", "0"));
        entity.setQuality(static_cast<PulseOne::Enums::DataQuality>(quality_code));
        
        // 타임스탬프들 (있는 것만 설정)
        if (row.find("value_timestamp") != row.end()) {
            entity.setValueTimestamp(Utils::StringToTimestamp(row.at("value_timestamp")));
        }
        if (row.find("updated_at") != row.end()) {
            entity.setUpdatedAt(Utils::StringToTimestamp(row.at("updated_at")));
        }
        
        // 통계 카운터들
        entity.setReadCount(std::stoi(RepositoryHelpers::getRowValue(row, "read_count", "0")));
        entity.setWriteCount(std::stoi(RepositoryHelpers::getRowValue(row, "write_count", "0")));
        entity.setErrorCount(std::stoi(RepositoryHelpers::getRowValue(row, "error_count", "0")));
        
        return entity;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("mapRowToEntity failed: " + std::string(e.what()));
        throw;
    }
}

std::map<std::string, std::string> CurrentValueRepository::entityToParams(const CurrentValueEntity& entity) {
    std::map<std::string, std::string> params;
    
    try {
        params["point_id"] = std::to_string(entity.getPointId());
        params["current_value"] = entity.getCurrentValue();
        params["raw_value"] = entity.getRawValue();
        params["value_type"] = entity.getValueType();
        params["quality_code"] = std::to_string(static_cast<int>(entity.getQualityCode()));
        params["quality"] = entity.getQuality();
        params["value_timestamp"] = Utils::TimestampToString(entity.getValueTimestamp());
        params["read_count"] = std::to_string(entity.getReadCount());
        params["write_count"] = std::to_string(entity.getWriteCount());
        params["error_count"] = std::to_string(entity.getErrorCount());
        params["updated_at"] = Utils::TimestampToString(Utils::GetCurrentTimestamp());
        
        return params;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("entityToParams failed: " + std::string(e.what()));
        throw;
    }
}

bool CurrentValueRepository::validateEntity(const CurrentValueEntity& entity) const {
    return validateCurrentValue(entity);
}

bool CurrentValueRepository::validateCurrentValue(const CurrentValueEntity& entity) const {
    if (entity.getPointId() <= 0) {
        LogManager::getInstance().Warn("Invalid point_id: " + std::to_string(entity.getPointId()));
        return false;
    }
    
    if (entity.getCurrentValue().empty()) {
        LogManager::getInstance().Warn("Empty current_value for point_id: " + std::to_string(entity.getPointId()));
        return false;
    }
    
    return true;
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne