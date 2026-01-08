/**
 * @file ExportTargetRepository.cpp
 * @brief Export Target Repository 구현부 (통계 필드 제거 버전)
 * @version 3.0.0 - 통계 필드 완전 제거
 * @date 2025-10-21
 * 
 * 저장 위치: core/shared/src/Database/Repositories/ExportTargetRepository.cpp
 * 
 * 주요 변경사항:
 *   - mapRowToEntity: 통계 필드 파싱 코드 제거
 *   - entityToParams: 통계 필드 변환 코드 제거
 *   - updateStatistics 메서드 삭제
 */

#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "DatabaseAbstractionLayer.hpp"
#include "Database/ExportSQLQueries.h"
#include "Logging/LogManager.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =============================================================================
// CRUD 기본 연산
// =============================================================================

std::vector<ExportTargetEntity> ExportTargetRepository::findAll() {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTarget::FIND_ALL);
        
        std::vector<ExportTargetEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("Failed to map row: " + std::string(e.what()));
                }
            }
        }
        
        if (logger_) {
            logger_->Debug("findAll: mapped " + std::to_string(entities.size()) + 
                          " of " + std::to_string(results.size()) + " rows");
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findAll failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::optional<ExportTargetEntity> ExportTargetRepository::findById(int id) {
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
        
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::ExportTarget::FIND_BY_ID, std::to_string(id));
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        auto entity = mapRowToEntity(results[0]);
        
        // 캐시 저장
        if (isCacheEnabled()) {
            cacheEntity(entity);
        }
        
        return entity;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findById failed: " + std::string(e.what()));
        }
        return std::nullopt;
    }
}

// ExportTargetRepository.cpp의 save() 메서드 - 완전 수정 버전
bool ExportTargetRepository::save(ExportTargetEntity& entity) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        auto params = entityToParams(entity);
        std::string query = SQL::ExportTarget::INSERT;
        
        // ✅ FIX: template_id 포함하여 10개 파라미터 모두 처리
        std::vector<std::string> insert_order = {
            "profile_id",      // 1
            "name",            // 2
            "target_type",     // 3
            "description",     // 4
            "is_enabled",      // 5
            "config",          // 6
            "template_id",     // 7 ← 이것이 누락되어 있었음!
            "export_mode",     // 8
            "export_interval", // 9
            "batch_size"       // 10
        };
        
        for (const auto& key : insert_order) {
            size_t pos = query.find('?');
            if (pos != std::string::npos) {
                std::string value = params[key];
                // SQL Injection 방지: 작은따옴표 이스케이프
                size_t escape_pos = 0;
                while ((escape_pos = value.find('\'', escape_pos)) != std::string::npos) {
                    value.replace(escape_pos, 1, "''");
                    escape_pos += 2;
                }
                query.replace(pos, 1, "'" + value + "'");
            }
        }
        
        DbLib::DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success) {
            // 생성된 ID 가져오기
            auto results = db_layer.executeQuery("SELECT last_insert_rowid() as id");
            if (!results.empty() && results[0].find("id") != results[0].end()) {
                entity.setId(std::stoi(results[0].at("id")));
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("save failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool ExportTargetRepository::update(const ExportTargetEntity& entity) {
    try {
        if (!ensureTableExists() || entity.getId() <= 0) {
            return false;
        }
        
        auto params = entityToParams(entity);
        std::string query = SQL::ExportTarget::UPDATE;
        
        // UPDATE 쿼리의 SET 절 순서대로 파라미터 치환
        std::vector<std::string> update_order = {
            "profile_id",
            "name",
            "target_type",
            "description",
            "is_enabled",
            "config",
            "export_mode",
            "export_interval",
            "batch_size"
        };
        
        for (const auto& key : update_order) {
            size_t pos = query.find('?');
            if (pos != std::string::npos) {
                std::string value = params[key];
                // SQL Injection 방지
                size_t escape_pos = 0;
                while ((escape_pos = value.find('\'', escape_pos)) != std::string::npos) {
                    value.replace(escape_pos, 1, "''");
                    escape_pos += 2;
                }
                query.replace(pos, 1, "'" + value + "'");
            }
        }
        
        // WHERE id = ? 치환
        size_t pos = query.find('?');
        if (pos != std::string::npos) {
            query.replace(pos, 1, std::to_string(entity.getId()));
        }
        
        DbLib::DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success && isCacheEnabled()) {
            clearCacheForId(entity.getId());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("update failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool ExportTargetRepository::deleteById(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::ExportTarget::DELETE_BY_ID, std::to_string(id));
        
        DbLib::DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeNonQuery(query);
        
        if (success && isCacheEnabled()) {
            clearCacheForId(id);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("deleteById failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool ExportTargetRepository::exists(int id) {
    try {
        if (!ensureTableExists()) {
            return false;
        }
        
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::ExportTarget::EXISTS_BY_ID, std::to_string(id));
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count")) > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("exists failed: " + std::string(e.what()));
        }
        return false;
    }
}

// =============================================================================
// 벌크 연산
// =============================================================================

std::vector<ExportTargetEntity> ExportTargetRepository::findByIds(
    const std::vector<int>& ids) {
    
    try {
        if (ids.empty() || !ensureTableExists()) {
            return {};
        }
        
        std::ostringstream ids_ss;
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) ids_ss << ", ";
            ids_ss << ids[i];
        }
        
        std::string query = "SELECT * FROM export_targets WHERE id IN (" + 
                           ids_ss.str() + ")";
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportTargetEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("Failed to map row in findByIds: " + 
                                 std::string(e.what()));
                }
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findByIds failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::vector<ExportTargetEntity> ExportTargetRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = "SELECT * FROM export_targets";
        query += RepositoryHelpers::buildWhereClause(conditions);
        query += RepositoryHelpers::buildOrderByClause(order_by);
        query += RepositoryHelpers::buildLimitClause(pagination);
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportTargetEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("Failed to map row in findByConditions: " + 
                                 std::string(e.what()));
                }
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findByConditions failed: " + std::string(e.what()));
        }
        return {};
    }
}

int ExportTargetRepository::countByConditions(
    const std::vector<QueryCondition>& conditions) {
    
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        std::string query = "SELECT COUNT(*) as count FROM export_targets";
        query += RepositoryHelpers::buildWhereClause(conditions);
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("countByConditions failed: " + std::string(e.what()));
        }
        return 0;
    }
}

// =============================================================================
// 전용 조회 메서드
// =============================================================================

std::vector<ExportTargetEntity> ExportTargetRepository::findByEnabled(bool enabled) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::ExportTarget::FIND_BY_ENABLED,
            enabled ? "1" : "0"
        );
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportTargetEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("Failed to map row in findByEnabled: " + 
                                 std::string(e.what()));
                }
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findByEnabled failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::vector<ExportTargetEntity> ExportTargetRepository::findByTargetType(
    const std::string& target_type) {
    
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = RepositoryHelpers::replaceParameterWithQuotes(
            SQL::ExportTarget::FIND_BY_TARGET_TYPE, target_type);
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportTargetEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("Failed to map row in findByTargetType: " + 
                                 std::string(e.what()));
                }
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findByTargetType failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::vector<ExportTargetEntity> ExportTargetRepository::findByProfileId(int profile_id) {
    try {
        if (!ensureTableExists()) {
            return {};
        }
        
        std::string query = RepositoryHelpers::replaceParameter(
            SQL::ExportTarget::FIND_BY_PROFILE_ID, std::to_string(profile_id));
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        std::vector<ExportTargetEntity> entities;
        entities.reserve(results.size());
        
        for (const auto& row : results) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("Failed to map row in findByProfileId: " + 
                                 std::string(e.what()));
                }
            }
        }
        
        return entities;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findByProfileId failed: " + std::string(e.what()));
        }
        return {};
    }
}

std::optional<ExportTargetEntity> ExportTargetRepository::findByName(
    const std::string& name) {
    
    try {
        if (!ensureTableExists()) {
            return std::nullopt;
        }
        
        std::string query = RepositoryHelpers::replaceParameterWithQuotes(
            SQL::ExportTarget::FIND_BY_NAME, name);
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(query);
        
        if (results.empty()) {
            return std::nullopt;
        }
        
        return mapRowToEntity(results[0]);
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("findByName failed: " + std::string(e.what()));
        }
        return std::nullopt;
    }
}

// =============================================================================
// 카운트 및 통계 (설정 기반만)
// =============================================================================

int ExportTargetRepository::getTotalCount() {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTarget::COUNT_ALL);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("getTotalCount failed: " + std::string(e.what()));
        }
        return 0;
    }
}

int ExportTargetRepository::getActiveCount() {
    try {
        if (!ensureTableExists()) {
            return 0;
        }
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTarget::COUNT_ACTIVE);
        
        if (!results.empty() && results[0].find("count") != results[0].end()) {
            return std::stoi(results[0].at("count"));
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("getActiveCount failed: " + std::string(e.what()));
        }
        return 0;
    }
}

std::map<std::string, int> ExportTargetRepository::getCountByType() {
    std::map<std::string, int> counts;
    
    try {
        if (!ensureTableExists()) {
            return counts;
        }
        
        DbLib::DatabaseAbstractionLayer db_layer;
        auto results = db_layer.executeQuery(SQL::ExportTarget::COUNT_BY_TYPE);
        
        for (const auto& row : results) {
            try {
                auto type_it = row.find("target_type");
                auto count_it = row.find("count");
                
                if (type_it != row.end() && count_it != row.end()) {
                    counts[type_it->second] = std::stoi(count_it->second);
                }
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->Warn("Failed to parse count by type row: " + 
                                 std::string(e.what()));
                }
            }
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("getCountByType failed: " + std::string(e.what()));
        }
    }
    
    return counts;
}

// =============================================================================
// 캐시 관리
// =============================================================================

std::map<std::string, int> ExportTargetRepository::getCacheStats() const {
    std::map<std::string, int> stats;
    
    if (isCacheEnabled()) {
        stats["cache_enabled"] = 1;
        auto base_stats = IRepository<ExportTargetEntity>::getCacheStats();
        stats.insert(base_stats.begin(), base_stats.end());
    } else {
        stats["cache_enabled"] = 0;
    }
    
    return stats;
}

// =============================================================================
// 내부 헬퍼 메서드
// =============================================================================

/**
 * @brief DB Row를 ExportTargetEntity로 변환 (수정 버전 2)
 * @note created_at/updated_at은 BaseEntity가 자동 관리
 */
ExportTargetEntity ExportTargetRepository::mapRowToEntity(
    const std::map<std::string, std::string>& row) {
    
    ExportTargetEntity entity;
    const size_t MAX_CONFIG_SIZE = 10 * 1024 * 1024; // 10MB
    
    // ==========================================================================
    // ID (필수 필드)
    // ==========================================================================
    try {
        auto it = row.find("id");
        if (it != row.end() && !it->second.empty()) {
            entity.setId(std::stoi(it->second));
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("mapRowToEntity: Failed to parse 'id': " + 
                          std::string(e.what()));
        }
        throw; // ID는 필수이므로 재throw
    }
    
    // ==========================================================================
    // profile_id
    // ==========================================================================
    try {
        auto it = row.find("profile_id");
        if (it != row.end() && !it->second.empty()) {
            entity.setProfileId(std::stoi(it->second));
        } else {
            entity.setProfileId(0);
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Debug("mapRowToEntity: Failed to parse 'profile_id': " + 
                          std::string(e.what()));
        }
        entity.setProfileId(0);
    }
    
    // ==========================================================================
    // name (필수 필드)
    // ==========================================================================
    try {
        auto it = row.find("name");
        if (it != row.end() && !it->second.empty()) {
            entity.setName(it->second);
        } else {
            entity.setName("Unnamed Target");
            if (logger_) {
                logger_->Warn("mapRowToEntity: 'name' field is empty");
            }
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Warn("mapRowToEntity: Failed to parse 'name': " + 
                         std::string(e.what()));
        }
        entity.setName("Unnamed Target");
    }
    
    // ==========================================================================
    // target_type (필수 필드)
    // ==========================================================================
    try {
        auto it = row.find("target_type");
        if (it != row.end() && !it->second.empty()) {
            entity.setTargetType(it->second);
        } else {
            entity.setTargetType("HTTP");
            if (logger_) {
                logger_->Warn("mapRowToEntity: 'target_type' empty, using HTTP");
            }
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Warn("mapRowToEntity: Failed to parse 'target_type': " + 
                         std::string(e.what()));
        }
        entity.setTargetType("HTTP");
    }
    
    // ==========================================================================
    // description (선택 필드)
    // ==========================================================================
    try {
        auto it = row.find("description");
        if (it != row.end()) {
            entity.setDescription(it->second);
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Debug("mapRowToEntity: Failed to parse 'description': " + 
                          std::string(e.what()));
        }
        entity.setDescription("");
    }
    
    // ==========================================================================
    // is_enabled
    // ==========================================================================
    try {
        auto it = row.find("is_enabled");
        if (it != row.end() && !it->second.empty()) {
            entity.setEnabled(std::stoi(it->second) != 0);
        } else {
            entity.setEnabled(false);
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Debug("mapRowToEntity: Failed to parse 'is_enabled': " + 
                          std::string(e.what()));
        }
        entity.setEnabled(false);
    }
    
    // ==========================================================================
    // config (JSON 필드 - 가장 중요!)
    // ==========================================================================
    try {
        auto it = row.find("config");
        if (it != row.end()) {
            const std::string& config_value = it->second;
            
            // 크기 체크
            if (config_value.size() > MAX_CONFIG_SIZE) {
                if (logger_) {
                    logger_->Warn("mapRowToEntity: config too large (" + 
                                 std::to_string(config_value.size()) + " bytes)");
                }
                entity.setConfig("{}");
            } 
            else if (config_value.empty()) {
                entity.setConfig("{}");
            }
            else {
                // 안전한 문자열 복사
                std::string config_copy;
                config_copy.reserve(config_value.size() + 1);
                config_copy = config_value;
                
                // JSON 유효성 검사
                try {
                    auto parsed = nlohmann::json::parse(config_copy);
                    (void)parsed; // unused 경고 방지
                    entity.setConfig(config_copy);
                    
                    if (logger_) {
                        logger_->Debug("mapRowToEntity: config OK (" + 
                                      std::to_string(config_copy.size()) + " bytes)");
                    }
                } catch (const nlohmann::json::exception& json_err) {
                    if (logger_) {
                        logger_->Warn("mapRowToEntity: Invalid JSON: " + 
                                     std::string(json_err.what()));
                    }
                    entity.setConfig("{}");
                }
            }
        } else {
            entity.setConfig("{}");
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("mapRowToEntity: config parse failed: " + 
                          std::string(e.what()));
        }
        entity.setConfig("{}");
    }
    
    // ==========================================================================
    // template_id (NULL 가능)
    // ==========================================================================
    try {
        auto it = row.find("template_id");
        if (it != row.end() && !it->second.empty() && it->second != "NULL") {
            entity.setTemplateId(std::stoi(it->second));
        } else {
            entity.setTemplateId(std::nullopt);
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Debug("mapRowToEntity: template_id parse failed: " + 
                          std::string(e.what()));
        }
        entity.setTemplateId(std::nullopt);
    }
    
    // ==========================================================================
    // export_mode
    // ==========================================================================
    try {
        auto it = row.find("export_mode");
        if (it != row.end() && !it->second.empty()) {
            entity.setExportMode(it->second);
        } else {
            entity.setExportMode("realtime");
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Debug("mapRowToEntity: export_mode parse failed: " + 
                          std::string(e.what()));
        }
        entity.setExportMode("realtime");
    }
    
    // ==========================================================================
    // export_interval
    // ==========================================================================
    try {
        auto it = row.find("export_interval");
        if (it != row.end() && !it->second.empty()) {
            entity.setExportInterval(std::stoi(it->second));
        } else {
            entity.setExportInterval(60);
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Debug("mapRowToEntity: export_interval parse failed: " + 
                          std::string(e.what()));
        }
        entity.setExportInterval(60);
    }
    
    // ==========================================================================
    // batch_size
    // ==========================================================================
    try {
        auto it = row.find("batch_size");
        if (it != row.end() && !it->second.empty()) {
            entity.setBatchSize(std::stoi(it->second));
        } else {
            entity.setBatchSize(100);
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Debug("mapRowToEntity: batch_size parse failed: " + 
                          std::string(e.what()));
        }
        entity.setBatchSize(100);
    }
    
    // ==========================================================================
    // ✅ created_at/updated_at은 BaseEntity가 자동 관리 - 파싱 불필요!
    // ==========================================================================
    
    // 매핑 성공 로그
    if (logger_) {
        logger_->Debug("mapRowToEntity: Mapped [" + entity.getName() + 
                      "] (ID: " + std::to_string(entity.getId()) + 
                      ", Type: " + entity.getTargetType() + ")");
    }
    
    return entity;
}


std::map<std::string, std::string> ExportTargetRepository::entityToParams(
    const ExportTargetEntity& entity) {
    
    std::map<std::string, std::string> params;
    
    // 설정 정보만 변환 (통계 필드 제거됨)
    params["profile_id"] = std::to_string(entity.getProfileId());
    params["name"] = entity.getName();
    params["target_type"] = entity.getTargetType();
    params["description"] = entity.getDescription();
    params["is_enabled"] = entity.isEnabled() ? "1" : "0";
    params["config"] = entity.getConfig();
    params["export_mode"] = entity.getExportMode();
    params["export_interval"] = std::to_string(entity.getExportInterval());
    params["batch_size"] = std::to_string(entity.getBatchSize());
    
    // ❌ 통계 필드 변환 코드 완전 제거
    
    return params;
}

bool ExportTargetRepository::ensureTableExists() {
    try {
        DbLib::DatabaseAbstractionLayer db_layer;
        bool success = db_layer.executeCreateTable(SQL::ExportTarget::CREATE_TABLE);
        
        if (success) {
            if (logger_) {
                logger_->Debug("Table export_targets is ready");
            }
        } else {
            if (logger_) {
                logger_->Error("Failed to ensure table exists");
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("ensureTableExists failed: " + std::string(e.what()));
        }
        return false;
    }
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne