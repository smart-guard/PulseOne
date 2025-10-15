/**
 * @file ExportLogRepository.cpp
 * @brief Export Log Repository 구현 - SiteRepository 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-10-15
 * @version 1.0.0
 * 저장 위치: core/shared/src/Database/Repositories/ExportLogRepository.cpp
 * 
 * 기존 패턴 준수:
 * - SiteRepository/ExportTargetRepository 패턴 적용
 * - DatabaseAbstractionLayer 사용
 * - IRepository 인터페이스 완전 구현
 * - 캐싱 및 벌크 연산 지원
 */

#include "Database/Repositories/ExportLogRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/DatabaseAbstractionLayer.h"
#include "Database/ExportSQLQueries.h"
#include "Database/SQLQueries.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <algorithm>
#include <ctime>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// IRepository 기본 CRUD 구현
// =============================================================================

std::vector<ExportLogEntity> ExportLogRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            logger_->Error("ExportLogRepository::findAll - Table creation failed");
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportLog::FIND_ALL);
        
        std::vector<ExportLogEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("ExportLogRepository::findAll - Failed to map row: " + 
                             std::string(e.what()));
            }
        }
        
        logger_->Info("ExportLogRepository::findAll - Found " + 
                     std::to_string(entities.size()) + " logs");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::findAll failed: " + std::string(e.what()));
        return {};
    }
}

std::optional<ExportLogEntity> ExportLogRepository::findById(int id) {
    try {
        // 캐시 확인
        if (isCacheEnabled()) {
            auto cached = getCachedEntity(id);
            if (cached.has_value()) {
                logger_->Debug("ExportLogRepository::findById - Cache hit for ID: " + 
                              std::to_string(id));
                return cached.value();
            }
        }
        
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportLog::FIND_BY_ID, 
                                            {std::to_string(id)});
        
        if (results.empty()) {
            logger_->Debug("ExportLogRepository::findById - Log not found: " + 
                          std::to_string(id));
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::findById failed for ID " + 
                      std::to_string(id) + ": " + std::string(e.what()));
        return std::nullopt;
    }
}

bool ExportLogRepository::save(ExportLogEntity& entity) {
    try {
        if (!validateLog(entity)) {
            logger_->Error("ExportLogRepository::save - Invalid log");
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::vector<std::string> params = {
            entity.getLogType(),
            std::to_string(entity.getServiceId()),
            std::to_string(entity.getTargetId()),
            std::to_string(entity.getMappingId()),
            std::to_string(entity.getPointId()),
            entity.getSourceValue(),
            entity.getConvertedValue(),
            entity.getStatus(),
            entity.getErrorMessage(),
            entity.getErrorCode(),
            entity.getResponseData(),
            std::to_string(entity.getHttpStatusCode()),
            std::to_string(entity.getProcessingTimeMs()),
            entity.getClientInfo()
        };
        
        bool success = db_layer.executeNonQuery(SQL::ExportLog::INSERT, params);
        
        if (success && entity.getId() <= 0) {
            auto id_result = db_layer.executeQuery(SQL::Common::GET_LAST_INSERT_ID);
            if (!id_result.empty() && !id_result[0].empty()) {
                entity.setId(std::stoi(id_result[0].at("id")));
            }
        }
        
        if (success) {
            entity.markSaved();
            
            if (isCacheEnabled()) {
                cacheEntity(entity);
            }
            
            logger_->Debug("ExportLogRepository::save - Saved log ID: " + 
                          std::to_string(entity.getId()));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::save failed: " + std::string(e.what()));
        return false;
    }
}

bool ExportLogRepository::update(const ExportLogEntity& entity) {
    try {
        if (!validateLog(entity)) {
            logger_->Error("ExportLogRepository::update - Invalid log");
            return false;
        }
        
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        
        std::vector<std::string> params = {
            entity.getLogType(),
            std::to_string(entity.getServiceId()),
            std::to_string(entity.getTargetId()),
            std::to_string(entity.getMappingId()),
            std::to_string(entity.getPointId()),
            entity.getSourceValue(),
            entity.getConvertedValue(),
            entity.getStatus(),
            entity.getErrorMessage(),
            entity.getErrorCode(),
            entity.getResponseData(),
            std::to_string(entity.getHttpStatusCode()),
            std::to_string(entity.getProcessingTimeMs()),
            entity.getClientInfo(),
            std::to_string(entity.getId())
        };
        
        bool success = db_layer.executeNonQuery(SQL::ExportLog::UPDATE, params);
        
        if (success && isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        if (success) {
            logger_->Debug("ExportLogRepository::update - Updated log ID: " + 
                          std::to_string(entity.getId()));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::update failed: " + std::string(e.what()));
        return false;
    }
}

bool ExportLogRepository::deleteById(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(SQL::ExportLog::DELETE_BY_ID, 
                                               {std::to_string(id)});
        
        if (success && isCacheEnabled()) {
            invalidateCache(id);
        }
        
        if (success) {
            logger_->Debug("ExportLogRepository::deleteById - Deleted log ID: " + 
                          std::to_string(id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::deleteById failed: " + std::string(e.what()));
        return false;
    }
}

bool ExportLogRepository::exists(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportLog::EXISTS_BY_ID, 
                                            {std::to_string(id)});
        
        if (!results.empty() && !results[0].empty()) {
            return std::stoi(results[0].at("count")) > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::exists failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 벌크 연산
// =============================================================================

std::vector<ExportLogEntity> ExportLogRepository::findByIds(const std::vector<int>& ids) {
    if (ids.empty()) return {};
    
    std::vector<ExportLogEntity> entities;
    entities.reserve(ids.size());
    
    for (int id : ids) {
        auto entity = findById(id);
        if (entity.has_value()) {
            entities.push_back(entity.value());
        }
    }
    
    return entities;
}

std::vector<ExportLogEntity> ExportLogRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    // IRepository의 기본 구현 활용
    return IRepository<ExportLogEntity>::findByConditions(conditions, order_by, pagination);
}

int ExportLogRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    // IRepository의 기본 구현 활용
    return IRepository<ExportLogEntity>::countByConditions(conditions);
}

// =============================================================================
// Log 전용 조회 메서드
// =============================================================================

std::vector<ExportLogEntity> ExportLogRepository::findByTargetId(int target_id, int limit) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportLog::FIND_BY_TARGET_ID, 
                                            {std::to_string(target_id), 
                                             std::to_string(limit)});
        
        std::vector<ExportLogEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("ExportLogRepository::findByTargetId - Failed to map row");
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::findByTargetId failed: " + 
                      std::string(e.what()));
        return {};
    }
}

std::vector<ExportLogEntity> ExportLogRepository::findByStatus(
    const std::string& status, int limit) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportLog::FIND_BY_STATUS, 
                                            {status, std::to_string(limit)});
        
        std::vector<ExportLogEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("ExportLogRepository::findByStatus - Failed to map row");
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::findByStatus failed: " + 
                      std::string(e.what()));
        return {};
    }
}

std::vector<ExportLogEntity> ExportLogRepository::findByTimeRange(
    const std::chrono::system_clock::time_point& start_time,
    const std::chrono::system_clock::time_point& end_time) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string start_str = RepositoryHelpers::formatTimestamp(start_time);
        std::string end_str = RepositoryHelpers::formatTimestamp(end_time);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportLog::FIND_BY_TIME_RANGE, 
                                            {start_str, end_str});
        
        std::vector<ExportLogEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("ExportLogRepository::findByTimeRange - Failed to map row");
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::findByTimeRange failed: " + 
                      std::string(e.what()));
        return {};
    }
}

std::vector<ExportLogEntity> ExportLogRepository::findRecent(int hours, int limit) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportLog::FIND_RECENT, 
                                            {std::to_string(hours), 
                                             std::to_string(limit)});
        
        std::vector<ExportLogEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("ExportLogRepository::findRecent - Failed to map row");
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::findRecent failed: " + 
                      std::string(e.what()));
        return {};
    }
}

std::vector<ExportLogEntity> ExportLogRepository::findRecentFailures(int hours, int limit) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportLog::FIND_RECENT_FAILURES, 
                                            {std::to_string(hours), 
                                             std::to_string(limit)});
        
        std::vector<ExportLogEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("ExportLogRepository::findRecentFailures - Failed to map row");
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::findRecentFailures failed: " + 
                      std::string(e.what()));
        return {};
    }
}

std::vector<ExportLogEntity> ExportLogRepository::findByPointId(int point_id, int limit) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportLog::FIND_BY_POINT_ID, 
                                            {std::to_string(point_id), 
                                             std::to_string(limit)});
        
        std::vector<ExportLogEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("ExportLogRepository::findByPointId - Failed to map row");
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::findByPointId failed: " + 
                      std::string(e.what()));
        return {};
    }
}

// =============================================================================
// 통계 및 분석
// =============================================================================

std::map<std::string, int> ExportLogRepository::getTargetStatistics(
    int target_id, int hours) {
    std::map<std::string, int> stats;
    
    try {
        if (!ensureTableExists()) {
            return stats;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportLog::GET_TARGET_STATISTICS, 
                                            {std::to_string(target_id), 
                                             std::to_string(hours)});
        
        for (const auto& row : results) {
            if (row.find("status") != row.end() && row.find("count") != row.end()) {
                stats[row.at("status")] = std::stoi(row.at("count"));
            }
        }
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::getTargetStatistics failed: " + 
                      std::string(e.what()));
    }
    
    return stats;
}

std::map<std::string, int> ExportLogRepository::getOverallStatistics(int hours) {
    std::map<std::string, int> stats;
    
    try {
        if (!ensureTableExists()) {
            return stats;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportLog::GET_OVERALL_STATISTICS, 
                                            {std::to_string(hours)});
        
        for (const auto& row : results) {
            if (row.find("status") != row.end() && row.find("count") != row.end()) {
                stats[row.at("status")] = std::stoi(row.at("count"));
            }
        }
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::getOverallStatistics failed: " + 
                      std::string(e.what()));
    }
    
    return stats;
}

double ExportLogRepository::getAverageProcessingTime(int target_id, int hours) {
    try {
        if (!ensureTableExists()) {
            return 0.0;
        }
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportLog::GET_AVERAGE_PROCESSING_TIME, 
                                            {std::to_string(target_id), 
                                             std::to_string(hours)});
        
        if (!results.empty() && results[0].find("avg_time") != results[0].end()) {
            const std::string& avg_str = results[0].at("avg_time");
            if (!avg_str.empty() && avg_str != "NULL") {
                return std::stod(avg_str);
            }
        }
        
        return 0.0;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::getAverageProcessingTime failed: " + 
                      std::string(e.what()));
        return 0.0;
    }
}

// =============================================================================
// 로그 정리
// =============================================================================

int ExportLogRepository::deleteOlderThan(int days) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(SQL::ExportLog::DELETE_OLDER_THAN, 
                                               {std::to_string(days)});
        
        if (success) {
            logger_->Info("ExportLogRepository::deleteOlderThan - Deleted logs older than " + 
                         std::to_string(days) + " days");
            
            // 캐시 전체 무효화
            if (isCacheEnabled()) {
                clearAllCache();
            }
            
            return 1; // 성공 표시
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::deleteOlderThan failed: " + 
                      std::string(e.what()));
        return 0;
    }
}

int ExportLogRepository::deleteSuccessLogsOlderThan(int days) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(
            SQL::ExportLog::DELETE_SUCCESS_LOGS_OLDER_THAN, 
            {std::to_string(days)});
        
        if (success) {
            logger_->Info("ExportLogRepository::deleteSuccessLogsOlderThan - " +
                         "Deleted success logs older than " + 
                         std::to_string(days) + " days");
            
            // 캐시 전체 무효화
            if (isCacheEnabled()) {
                clearAllCache();
            }
            
            return 1; // 성공 표시
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::deleteSuccessLogsOlderThan failed: " + 
                      std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// 캐시 관리
// =============================================================================

std::map<std::string, int> ExportLogRepository::getCacheStats() const {
    std::map<std::string, int> stats;
    
    if (isCacheEnabled()) {
        stats["cache_enabled"] = 1;
        // 기본 통계는 IRepository에서 제공
        auto base_stats = IRepository<ExportLogEntity>::getCacheStats();
        stats.insert(base_stats.begin(), base_stats.end());
    } else {
        stats["cache_enabled"] = 0;
    }
    
    return stats;
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

ExportLogEntity ExportLogRepository::mapRowToEntity(
    const std::map<std::string, std::string>& row) {
    try {
        ExportLogEntity entity;
        
        auto it = row.find("id");
        if (it != row.end() && !it->second.empty()) {
            entity.setId(std::stoi(it->second));
        }
        
        it = row.find("log_type");
        if (it != row.end()) {
            entity.setLogType(it->second);
        }
        
        it = row.find("service_id");
        if (it != row.end() && !it->second.empty()) {
            entity.setServiceId(std::stoi(it->second));
        }
        
        it = row.find("target_id");
        if (it != row.end() && !it->second.empty()) {
            entity.setTargetId(std::stoi(it->second));
        }
        
        it = row.find("mapping_id");
        if (it != row.end() && !it->second.empty()) {
            entity.setMappingId(std::stoi(it->second));
        }
        
        it = row.find("point_id");
        if (it != row.end() && !it->second.empty()) {
            entity.setPointId(std::stoi(it->second));
        }
        
        it = row.find("source_value");
        if (it != row.end()) {
            entity.setSourceValue(it->second);
        }
        
        it = row.find("converted_value");
        if (it != row.end()) {
            entity.setConvertedValue(it->second);
        }
        
        it = row.find("status");
        if (it != row.end()) {
            entity.setStatus(it->second);
        }
        
        it = row.find("error_message");
        if (it != row.end()) {
            entity.setErrorMessage(it->second);
        }
        
        it = row.find("error_code");
        if (it != row.end()) {
            entity.setErrorCode(it->second);
        }
        
        it = row.find("response_data");
        if (it != row.end()) {
            entity.setResponseData(it->second);
        }
        
        it = row.find("http_status_code");
        if (it != row.end() && !it->second.empty()) {
            entity.setHttpStatusCode(std::stoi(it->second));
        }
        
        it = row.find("processing_time_ms");
        if (it != row.end() && !it->second.empty()) {
            entity.setProcessingTimeMs(std::stoi(it->second));
        }
        
        it = row.find("timestamp");
        if (it != row.end() && !it->second.empty()) {
            auto timestamp = RepositoryHelpers::parseTimestamp(it->second);
            entity.setTimestamp(timestamp);
        }
        
        it = row.find("client_info");
        if (it != row.end()) {
            entity.setClientInfo(it->second);
        }
        
        entity.markSaved();
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::mapRowToEntity failed: " + 
                      std::string(e.what()));
        throw;
    }
}

std::map<std::string, std::string> ExportLogRepository::entityToParams(
    const ExportLogEntity& entity) {
    std::map<std::string, std::string> params;
    
    params["id"] = std::to_string(entity.getId());
    params["log_type"] = entity.getLogType();
    params["service_id"] = std::to_string(entity.getServiceId());
    params["target_id"] = std::to_string(entity.getTargetId());
    params["mapping_id"] = std::to_string(entity.getMappingId());
    params["point_id"] = std::to_string(entity.getPointId());
    params["source_value"] = entity.getSourceValue();
    params["converted_value"] = entity.getConvertedValue();
    params["status"] = entity.getStatus();
    params["error_message"] = entity.getErrorMessage();
    params["error_code"] = entity.getErrorCode();
    params["response_data"] = entity.getResponseData();
    params["http_status_code"] = std::to_string(entity.getHttpStatusCode());
    params["processing_time_ms"] = std::to_string(entity.getProcessingTimeMs());
    params["timestamp"] = RepositoryHelpers::formatTimestamp(entity.getTimestamp());
    params["client_info"] = entity.getClientInfo();
    
    return params;
}

bool ExportLogRepository::validateLog(const ExportLogEntity& entity) {
    return entity.validate();
}

bool ExportLogRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        
        // 테이블 생성
        if (!db_layer.executeNonQuery(SQL::ExportLog::CREATE_TABLE)) {
            return false;
        }
        
        // 인덱스 생성
        if (!db_layer.executeNonQuery(SQL::ExportLog::CREATE_INDEXES)) {
            logger_->Warn("ExportLogRepository::ensureTableExists - Index creation failed");
            // 인덱스 실패는 치명적이지 않으므로 계속 진행
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::ensureTableExists failed: " + 
                      std::string(e.what()));
        return false;
    }
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne