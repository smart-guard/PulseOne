/**
 * @file ExportLogRepository.cpp
 * @brief Export Log Repository 구현 - 에러 수정 완료
 * @author PulseOne Development Team
 * @date 2025-10-15
 * @version 1.0.1
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
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::ExportLog::FIND_BY_ID, std::to_string(id));
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            logger_->Debug("ExportLogRepository::findById - No log found for ID: " + 
                          std::to_string(id));
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시에 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        logger_->Debug("ExportLogRepository::findById - Found log ID: " + 
                      std::to_string(id));
        return entity;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::findById failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool ExportLogRepository::save(ExportLogEntity& entity) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        auto params = entityToParams(entity);
        std::string query = SQL::ExportLog::INSERT;
        
        // ? 플레이스홀더를 params 값으로 치환
        for (const auto& param : params) {
            size_t pos = query.find('?');
            if (pos != std::string::npos) {
                query.replace(pos, 1, "'" + param.second + "'");
            }
        }
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // ID 조회
            auto results = db_layer.executeQuery("SELECT last_insert_rowid() as id");
            if (!results.empty() && results[0].find("id") != results[0].end()) {
                entity.setId(std::stoi(results[0].at("id")));
            }
            
            logger_->Info("ExportLogRepository::save - Saved log ID: " + 
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
        if (!ensureTableExists()) {
            return false;
        }
        
        if (entity.getId() <= 0) {
            logger_->Error("ExportLogRepository::update - Invalid entity ID");
            return false;
        }
        
        auto params = entityToParams(entity);
        std::string query = SQL::ExportLog::UPDATE;
        
        // ? 플레이스홀더를 params 값으로 치환
        for (const auto& param : params) {
            size_t pos = query.find('?');
            if (pos != std::string::npos) {
                query.replace(pos, 1, "'" + param.second + "'");
            }
        }
        
        // 마지막 ? (WHERE id = ?)
        size_t pos = query.find('?');
        if (pos != std::string::npos) {
            query.replace(pos, 1, std::to_string(entity.getId()));
        }
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                clearCacheForId(entity.getId());
            }
            logger_->Info("ExportLogRepository::update - Updated log ID: " + 
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
        
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::ExportLog::DELETE_BY_ID, std::to_string(id));
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                clearCacheForId(id);
            }
            logger_->Info("ExportLogRepository::deleteById - Deleted log ID: " + 
                         std::to_string(id));
        } else {
            logger_->Error("ExportLogRepository::deleteById - Failed to delete log ID: " + 
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
        
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::ExportLog::EXISTS_BY_ID, std::to_string(id));
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            int count = std::stoi(results[0].at("count"));
            return count > 0;
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
    try {
        if (ids.empty() || !ensureTableExists()) {
            return {};
        }
        
        std::ostringstream ids_ss;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) ids_ss << ", ";
            ids_ss << ids[i];
        }
        
        std::string query = R"(
            SELECT 
                id, log_type, service_id, target_id, mapping_id, point_id,
                source_value, converted_value, status, error_message, error_code,
                response_data, http_status_code, processing_time_ms, timestamp, client_info
            FROM export_logs
            WHERE id IN ()" + ids_ss.str() + R"()
            ORDER BY timestamp DESC
        )";
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportLogEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("ExportLogRepository::findByIds - Failed to map row: " + 
                             std::string(e.what()));
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::findByIds failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<ExportLogEntity> ExportLogRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = R"(
            SELECT 
                id, log_type, service_id, target_id, mapping_id, point_id,
                source_value, converted_value, status, error_message, error_code,
                response_data, http_status_code, processing_time_ms, timestamp, client_info
            FROM export_logs
        )";
        
        query += RepositoryHelpers::buildWhereClause(conditions);
        query += RepositoryHelpers::buildOrderByClause(order_by);
        query += RepositoryHelpers::buildLimitClause(pagination);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportLogEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                logger_->Warn("ExportLogRepository::findByConditions - Failed to map row: " + 
                             std::string(e.what()));
            }
        }
        
        logger_->Debug("ExportLogRepository::findByConditions - Found " + 
                      std::to_string(entities.size()) + " logs");
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::findByConditions failed: " + 
                      std::string(e.what()));
        return {};
    }
}

int ExportLogRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        std::string query = "SELECT COUNT(*) as count FROM export_logs";
        query += RepositoryHelpers::buildWhereClause(conditions);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::countByConditions failed: " + 
                      std::string(e.what()));
        return 0;
    }
}

// =============================================================================
// Log 전용 조회 메서드
// =============================================================================

std::vector<ExportLogEntity> ExportLogRepository::findByTargetId(int target_id, int limit) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::ExportLog::FIND_BY_TARGET_ID;
        query = RepositoryHelpers::replaceTwoParameters(query, 
                                                        std::to_string(target_id),
                                                        std::to_string(limit));
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportLogEntity> entities;
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception&) {}
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
        
        std::string query = SQL::ExportLog::FIND_BY_STATUS;
        query = RepositoryHelpers::replaceParameterWithQuotes(query, status);
        query = RepositoryHelpers::replaceParameter(query, std::to_string(limit));
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportLogEntity> entities;
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception&) {}
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
        
        auto start_t = std::chrono::system_clock::to_time_t(start_time);
        auto end_t = std::chrono::system_clock::to_time_t(end_time);
        
        char start_str[64], end_str[64];
        std::strftime(start_str, sizeof(start_str), "%Y-%m-%d %H:%M:%S", 
                     std::gmtime(&start_t));
        std::strftime(end_str, sizeof(end_str), "%Y-%m-%d %H:%M:%S", 
                     std::gmtime(&end_t));
        
        std::string query = SQL::ExportLog::FIND_BY_TIME_RANGE;
        query = RepositoryHelpers::replaceParameterWithQuotes(query, start_str);
        query = RepositoryHelpers::replaceParameterWithQuotes(query, end_str);
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportLogEntity> entities;
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception&) {}
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
        
        std::string query = SQL::ExportLog::FIND_RECENT;
        query = RepositoryHelpers::replaceTwoParameters(query, 
                                                        std::to_string(hours),
                                                        std::to_string(limit));
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportLogEntity> entities;
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception&) {}
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::findRecent failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<ExportLogEntity> ExportLogRepository::findRecentFailures(int hours, int limit) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = SQL::ExportLog::FIND_RECENT_FAILURES;
        query = RepositoryHelpers::replaceTwoParameters(query, 
                                                        std::to_string(hours),
                                                        std::to_string(limit));
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportLogEntity> entities;
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception&) {}
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
        
        std::string query = SQL::ExportLog::FIND_BY_POINT_ID;
        query = RepositoryHelpers::replaceTwoParameters(query, 
                                                        std::to_string(point_id),
                                                        std::to_string(limit));
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportLogEntity> entities;
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception&) {}
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::findByPointId failed: " + 
                      std::string(e.what()));
        return {};
    }
}

// =============================================================================
// 통계 메서드
// =============================================================================

std::map<std::string, int> ExportLogRepository::getTargetStatistics(
    int target_id, int hours) {
    
    std::map<std::string, int> stats;
    
    try {
        if (!ensureTableExists()) {
            return stats;
        }
        
        std::string query = SQL::ExportLog::GET_TARGET_STATISTICS;
        query = RepositoryHelpers::replaceTwoParameters(query, 
                                                        std::to_string(target_id),
                                                        std::to_string(hours));
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
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
        
        std::string query = SQL::ExportLog::GET_OVERALL_STATISTICS;
        query = RepositoryHelpers::replaceParameter(query, std::to_string(hours));
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
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
        
        std::string query = SQL::ExportLog::GET_AVERAGE_PROCESSING_TIME;
        query = RepositoryHelpers::replaceTwoParameters(query, 
                                                        std::to_string(target_id),
                                                        std::to_string(hours));
        
        DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("avg_time") != results[0].end()) {
            return std::stod(results[0].at("avg_time"));
        }
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::getAverageProcessingTime failed: " + 
                      std::string(e.what()));
    }
    
    return 0.0;
}

// =============================================================================
// 정리 메서드
// =============================================================================

int ExportLogRepository::deleteOlderThan(int days) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        std::string query = SQL::ExportLog::DELETE_OLDER_THAN;
        query = RepositoryHelpers::replaceParameter(query, std::to_string(days));
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                clearCache();
            }
            logger_->Info("ExportLogRepository::deleteOlderThan - Deleted logs older than " + 
                         std::to_string(days) + " days");
            return 1;
        }
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::deleteOlderThan failed: " + 
                      std::string(e.what()));
    }
    
    return 0;
}

int ExportLogRepository::deleteSuccessLogsOlderThan(int days) {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        std::string query = SQL::ExportLog::DELETE_SUCCESS_LOGS_OLDER_THAN;
        query = RepositoryHelpers::replaceParameter(query, std::to_string(days));
        
        DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            if (isCacheEnabled()) {
                clearCache();
            }
            logger_->Info("ExportLogRepository::deleteSuccessLogsOlderThan - " +
                         std::string("Deleted success logs older than ") + 
                         std::to_string(days) + " days");
            return 1;
        }
        
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::deleteSuccessLogsOlderThan failed: " + 
                      std::string(e.what()));
    }
    
    return 0;
}

// =============================================================================
// 캐시 관리
// =============================================================================

std::map<std::string, int> ExportLogRepository::getCacheStats() const {
    std::map<std::string, int> stats;
    
    if (isCacheEnabled()) {
        stats["cache_enabled"] = 1;
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
            // timestamp 문자열을 time_point로 변환
            std::tm tm = {};
            std::istringstream ss(it->second);
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            entity.setTimestamp(tp);
        }
        
        it = row.find("client_info");
        if (it != row.end()) {
            entity.setClientInfo(it->second);
        }
        
        return entity;
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to map row to ExportLogEntity: " + 
                               std::string(e.what()));
    }
}

std::map<std::string, std::string> ExportLogRepository::entityToParams(
    const ExportLogEntity& entity) {
    
    std::map<std::string, std::string> params;
    
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
    params["client_info"] = entity.getClientInfo();
    
    return params;
}

bool ExportLogRepository::ensureTableExists() {
    try {
        DatabaseAbstractionLayer db_layer;
        return db_layer.executeNonQuery(SQL::ExportLog::CREATE_TABLE) &&
               db_layer.executeNonQuery(SQL::ExportLog::CREATE_INDEXES);
    } catch (const std::exception& e) {
        logger_->Error("ExportLogRepository::ensureTableExists failed: " + 
                      std::string(e.what()));
        return false;
    }
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne