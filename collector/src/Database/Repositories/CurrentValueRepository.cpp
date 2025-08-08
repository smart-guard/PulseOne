// =============================================================================
// collector/src/Database/Repositories/CurrentValueRepository.cpp
// 🔧 컴파일 에러 수정: 함수명 매칭 + Utils 함수 수정
// =============================================================================

/**
 * @file CurrentValueRepository.cpp - 컴파일 에러 수정
 * @brief PulseOne CurrentValueRepository 구현 - SQLQueries.h 사용
 * @author PulseOne Development Team
 * @date 2025-08-08
 * 
 * 🔧 수정사항:
 * - DBStringToTimestamp → TimestampToDBString 함수명 수정
 * - setQualityCode → 적절한 setter 사용
 * - 누락된 메서드들 헤더 선언 추가됨
 */

#include "Database/Repositories/CurrentValueRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Database/SQLQueries.h"  // 🔥 추가
#include "Common/Utils.h"
#include <sstream>
#include <iomanip>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// 🎯 SQLQueries.h 사용하는 깔끔한 구현
// =============================================================================

std::vector<CurrentValueEntity> CurrentValueRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            logger_->Error("CurrentValueRepository::findAll - Table creation failed");
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
        
        // 🔥 SQLQueries.h 사용 + 파라미터 바인딩
        std::string query = SQL::CurrentValue::FIND_BY_ID;
        RepositoryHelpers::replaceParameterPlaceholders(query, {std::to_string(id)});
        
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
        
        // 🔥 UPSERT 방식으로 insert/update 자동 처리
        std::map<std::string, std::string> data;
        data["point_id"] = std::to_string(entity.getPointId());
        data["current_value"] = entity.getCurrentValue();
        data["raw_value"] = entity.getRawValue();
        data["value_type"] = entity.getValueType();
        data["quality_code"] = std::to_string(static_cast<int>(entity.getQualityCode()));
        data["quality"] = PulseOne::Utils::DataQualityToString(entity.getQualityCode(), true);
        data["value_timestamp"] = PulseOne::Utils::TimestampToDBString(entity.getValueTimestamp());
        data["quality_timestamp"] = PulseOne::Utils::TimestampToDBString(entity.getQualityTimestamp());
        data["last_log_time"] = PulseOne::Utils::TimestampToDBString(entity.getLastLogTime());
        data["last_read_time"] = PulseOne::Utils::TimestampToDBString(entity.getLastReadTime());
        data["last_write_time"] = PulseOne::Utils::TimestampToDBString(entity.getLastWriteTime());
        data["read_count"] = std::to_string(entity.getReadCount());
        data["write_count"] = std::to_string(entity.getWriteCount());
        data["error_count"] = std::to_string(entity.getErrorCount());
        data["updated_at"] = PulseOne::Utils::TimestampToDBString(std::chrono::system_clock::now());
        
        std::vector<std::string> primary_keys = {"point_id"};
        bool success = db_layer.executeUpsert("current_values", data, primary_keys);
        
        if (success) {
            logger_->Debug("CurrentValueRepository::save - Saved current value for point_id: " + std::to_string(entity.getPointId()));
            
            // 캐시 업데이트
            if (isCacheEnabled()) {
                cacheEntity(entity);
            }
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
    try {
        if (!validateCurrentValue(entity)) {
            logger_->Error("CurrentValueRepository::update - Invalid current value for point_id: " + std::to_string(entity.getPointId()));
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        // 🔥 SQLQueries.h 사용 + 파라미터 바인딩
        std::vector<std::string> params = {
            entity.getCurrentValue(),
            entity.getRawValue(), 
            entity.getValueType(),
            std::to_string(static_cast<int>(entity.getQualityCode())),
            PulseOne::Utils::DataQualityToString(entity.getQualityCode(), true),
            PulseOne::Utils::TimestampToDBString(entity.getValueTimestamp()),
            PulseOne::Utils::TimestampToDBString(entity.getQualityTimestamp()),
            PulseOne::Utils::TimestampToDBString(entity.getLastLogTime()),
            PulseOne::Utils::TimestampToDBString(entity.getLastReadTime()),
            PulseOne::Utils::TimestampToDBString(entity.getLastWriteTime()),
            std::to_string(entity.getReadCount()),
            std::to_string(entity.getWriteCount()),
            std::to_string(entity.getErrorCount()),
            PulseOne::Utils::TimestampToDBString(std::chrono::system_clock::now()),
            std::to_string(entity.getPointId())  // WHERE 절
        };
        
        std::string query = SQL::CurrentValue::UPDATE;
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            logger_->Debug("CurrentValueRepository::update - Updated current value for point_id: " + std::to_string(entity.getPointId()));
            
            // 캐시 업데이트
            if (isCacheEnabled()) {
                cacheEntity(entity);
            }
        } else {
            logger_->Error("CurrentValueRepository::update - Failed to update current value for point_id: " + std::to_string(entity.getPointId()));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::update failed: " + std::string(e.what()));
        return false;
    }
}

bool CurrentValueRepository::deleteById(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        // 🔥 SQLQueries.h 사용
        std::string query = SQL::CurrentValue::DELETE_BY_ID;
        RepositoryHelpers::replaceParameterPlaceholders(query, {std::to_string(id)});
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            logger_->Debug("CurrentValueRepository::deleteById - Deleted current value for point_id: " + std::to_string(id));
            
            // 캐시에서 제거
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
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
        
        // 🔥 SQLQueries.h 사용
        std::string query = SQL::CurrentValue::EXISTS_BY_ID;
        RepositoryHelpers::replaceParameterPlaceholders(query, {std::to_string(id)});
        
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
        
        // 🔥 IN 절 구성
        std::string ids_str = RepositoryHelpers::buildInClause(ids);
        
        // 🔥 SQLQueries.h의 템플릿 사용
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

// =============================================================================
// 🔥 CurrentValue 전용 메서드들 - SQLQueries.h 사용
// =============================================================================

std::optional<CurrentValueEntity> CurrentValueRepository::findByDataPointId(int data_point_id) {
    // point_id가 primary key이므로 findById와 동일
    return findById(data_point_id);
}

std::vector<CurrentValueEntity> CurrentValueRepository::findByQuality(PulseOne::Enums::DataQuality quality) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string quality_str = PulseOne::Utils::DataQualityToString(quality, true);
        
        // 🔥 SQLQueries.h 사용
        std::string query = SQL::CurrentValue::FIND_BY_QUALITY;
        RepositoryHelpers::replaceParameterPlaceholders(query, {quality_str});
        
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

std::vector<CurrentValueEntity> CurrentValueRepository::findStaleValues(int hours) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(hours);
        std::string cutoff_str = PulseOne::Utils::TimestampToDBString(cutoff_time);
        
        // 🔥 SQLQueries.h 사용
        std::string query = SQL::CurrentValue::FIND_STALE_VALUES;
        RepositoryHelpers::replaceParameterPlaceholders(query, {cutoff_str});
        
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
        
        // 🔥 SQLQueries.h 사용
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::CurrentValue::FIND_BAD_QUALITY_VALUES);
        
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

bool CurrentValueRepository::updateValueOnly(int point_id, const std::string& current_value, const std::string& raw_value, const std::string& value_type) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        auto now_str = PulseOne::Utils::TimestampToDBString(std::chrono::system_clock::now());
        
        // 🔥 SQLQueries.h 사용
        std::vector<std::string> params = {
            current_value, raw_value, value_type, now_str, now_str, std::to_string(point_id)
        };
        
        std::string query = SQL::CurrentValue::UPDATE_VALUE;
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            logger_->Debug("CurrentValueRepository::updateValueOnly - Updated value for point_id: " + std::to_string(point_id));
            
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(point_id);
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::updateValueOnly failed: " + std::string(e.what()));
        return false;
    }
}

bool CurrentValueRepository::incrementReadCount(int point_id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        auto now_str = PulseOne::Utils::TimestampToDBString(std::chrono::system_clock::now());
        
        // 🔥 SQLQueries.h 사용
        std::vector<std::string> params = {now_str, now_str, std::to_string(point_id)};
        
        std::string query = SQL::CurrentValue::INCREMENT_READ_COUNT;
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            logger_->Debug("CurrentValueRepository::incrementReadCount - Read count incremented for point_id: " + std::to_string(point_id));
            
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(point_id);
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::incrementReadCount failed: " + std::string(e.what()));
        return false;
    }
}

bool CurrentValueRepository::incrementWriteCount(int point_id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        auto now_str = PulseOne::Utils::TimestampToDBString(std::chrono::system_clock::now());
        
        // 🔥 SQLQueries.h 사용
        std::vector<std::string> params = {now_str, now_str, std::to_string(point_id)};
        
        std::string query = SQL::CurrentValue::INCREMENT_WRITE_COUNT;
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            logger_->Debug("CurrentValueRepository::incrementWriteCount - Write count incremented for point_id: " + std::to_string(point_id));
            
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(point_id);
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::incrementWriteCount failed: " + std::string(e.what()));
        return false;
    }
}

bool CurrentValueRepository::incrementErrorCount(int point_id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        auto now_str = PulseOne::Utils::TimestampToDBString(std::chrono::system_clock::now());
        
        // 🔥 SQLQueries.h 사용
        std::vector<std::string> params = {now_str, std::to_string(point_id)};
        
        std::string query = SQL::CurrentValue::INCREMENT_ERROR_COUNT;
        RepositoryHelpers::replaceParameterPlaceholders(query, params);
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            logger_->Debug("CurrentValueRepository::incrementErrorCount - Error count incremented for point_id: " + std::to_string(point_id));
            
            // 캐시 무효화
            if (isCacheEnabled()) {
                clearCacheForId(point_id);
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::incrementErrorCount failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 🔥 헬퍼 메서드들
// =============================================================================

bool CurrentValueRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        return db_layer.executeCreateTable(SQL::CurrentValue::CREATE_TABLE);
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::ensureTableExists failed: " + std::string(e.what()));
        return false;
    }
}

CurrentValueEntity CurrentValueRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    try {
        // 🔥 업데이트된 스키마에 맞게 매핑
        CurrentValueEntity entity;
        
        // 기본 정보
        entity.setPointId(std::stoi(RepositoryHelpers::getRowValue(row, "point_id")));
        entity.setCurrentValue(RepositoryHelpers::getRowValue(row, "current_value"));
        entity.setRawValue(RepositoryHelpers::getRowValue(row, "raw_value"));
        entity.setValueType(RepositoryHelpers::getRowValue(row, "value_type"));
        
        // 🔧 품질 정보 - CurrentValueEntity의 실제 setter 사용
        int quality_code = std::stoi(RepositoryHelpers::getRowValue(row, "quality_code", "0"));
        entity.setQuality(static_cast<PulseOne::Enums::DataQuality>(quality_code));  // 🔧 수정
        
        // 🔧 타임스탬프들 - 올바른 함수명 사용
        if (!RepositoryHelpers::getRowValue(row, "value_timestamp").empty()) {
            entity.setValueTimestamp(PulseOne::Utils::StringToTimestamp(RepositoryHelpers::getRowValue(row, "value_timestamp")));
        }
        if (!RepositoryHelpers::getRowValue(row, "quality_timestamp").empty()) {
            entity.setQualityTimestamp(PulseOne::Utils::StringToTimestamp(RepositoryHelpers::getRowValue(row, "quality_timestamp")));
        }
        if (!RepositoryHelpers::getRowValue(row, "last_log_time").empty()) {
            entity.setLastLogTime(PulseOne::Utils::StringToTimestamp(RepositoryHelpers::getRowValue(row, "last_log_time")));
        }
        if (!RepositoryHelpers::getRowValue(row, "last_read_time").empty()) {
            entity.setLastReadTime(PulseOne::Utils::StringToTimestamp(RepositoryHelpers::getRowValue(row, "last_read_time")));
        }
        if (!RepositoryHelpers::getRowValue(row, "last_write_time").empty()) {
            entity.setLastWriteTime(PulseOne::Utils::StringToTimestamp(RepositoryHelpers::getRowValue(row, "last_write_time")));
        }
        
        // 통계 카운터들
        entity.setReadCount(std::stoi(RepositoryHelpers::getRowValue(row, "read_count", "0")));
        entity.setWriteCount(std::stoi(RepositoryHelpers::getRowValue(row, "write_count", "0")));
        entity.setErrorCount(std::stoi(RepositoryHelpers::getRowValue(row, "error_count", "0")));
        
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("CurrentValueRepository::mapRowToEntity failed: " + std::string(e.what()));
        throw;
    }
}

bool CurrentValueRepository::validateCurrentValue(const CurrentValueEntity& entity) const {
    // 기본 검증
    if (entity.getPointId() <= 0) {
        logger_->Warn("CurrentValueRepository::validateCurrentValue - Invalid point_id: " + std::to_string(entity.getPointId()));
        return false;
    }
    
    if (entity.getCurrentValue().empty()) {
        logger_->Warn("CurrentValueRepository::validateCurrentValue - Empty current_value for point_id: " + std::to_string(entity.getPointId()));
        return false;
    }
    
    return true;
}

// =============================================================================
// 🔥 캐시 관리 (기존 IRepository 상속 메서드들)
// =============================================================================

void CurrentValueRepository::enableCache() {
    setCacheEnabled(true);  // 🔧 수정: IRepository의 올바른 메서드 사용
    std::string status = isCacheEnabled() ? "enabled" : "disabled";
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
        logger_->Error("CurrentValueRepository::getTotalCount failed: " + std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// 🔥 호환성 메서드들 (기존 테스트와 호환)
// =============================================================================

std::vector<CurrentValueEntity> CurrentValueRepository::findByDataPointIds(const std::vector<int>& point_ids) {
    return findByIds(point_ids);  // point_id가 primary key이므로 동일
}

} // namespace Repositories
} // namespace Database  
} // namespace PulseOne