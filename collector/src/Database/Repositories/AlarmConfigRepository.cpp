/**
 * @file AlarmConfigRepository.cpp
 * @brief PulseOne AlarmConfigRepository 구현 - IRepository 기반 알람 설정 관리
 * @author PulseOne Development Team
 * @date 2025-07-28
 */

#include "Database/Repositories/AlarmConfigRepository.h"
#include "Utils/LogManager.h"
#include <chrono>
#include <algorithm>
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =======================================================================
// 생성자 및 초기화
// =======================================================================

AlarmConfigRepository::AlarmConfigRepository() 
    : IRepository<AlarmConfigEntity>("AlarmConfigRepository") {
    logger_->Info("🚨 AlarmConfigRepository initialized with IRepository caching system");
    logger_->Info("✅ Cache enabled: " + std::string(cache_enabled_ ? "YES" : "NO"));
}

// =======================================================================
// IRepository 인터페이스 구현
// =======================================================================

std::vector<AlarmConfigEntity> AlarmConfigRepository::findAll() {
    logger_->Debug("🔍 AlarmConfigRepository::findAll() - Fetching all alarm configs");
    
    return findByConditions({}, OrderBy("name", "ASC"));
}

std::optional<AlarmConfigEntity> AlarmConfigRepository::findById(int id) {
    logger_->Debug("🔍 AlarmConfigRepository::findById(" + std::to_string(id) + ")");
    
    // 캐시 먼저 확인 (IRepository 자동 처리)
    auto cached = getCachedEntity(id);
    if (cached.has_value()) {
        logger_->Debug("✅ Cache HIT for alarm config ID: " + std::to_string(id));
        return cached;
    }
    
    // DB에서 조회
    auto configs = findByConditions({QueryCondition("id", "=", std::to_string(id))});
    if (!configs.empty()) {
        // 캐시에 저장 (IRepository 자동 처리)
        setCachedEntity(id, configs[0]);
        logger_->Debug("✅ Alarm config found and cached: " + configs[0].getName());
        return configs[0];
    }
    
    logger_->Debug("❌ Alarm config not found: " + std::to_string(id));
    return std::nullopt;
}

bool AlarmConfigRepository::save(AlarmConfigEntity& entity) {
    logger_->Debug("💾 AlarmConfigRepository::save() - " + entity.getName());
    
    // 유효성 검사
    if (!validateAlarmConfig(entity)) {
        logger_->Error("❌ Alarm config validation failed: " + entity.getName());
        return false;
    }
    
    // 이름 중복 검사 (같은 테넌트 내에서)
    if (isNameTaken(entity.getName(), entity.getTenantId())) {
        logger_->Error("❌ Alarm config name already taken: " + entity.getName());
        return false;
    }
    
    // IRepository의 표준 save 구현 사용
    return IRepository<AlarmConfigEntity>::save(entity);
}

bool AlarmConfigRepository::update(const AlarmConfigEntity& entity) {
    logger_->Debug("🔄 AlarmConfigRepository::update() - " + entity.getName());
    
    // 유효성 검사
    if (!validateAlarmConfig(entity)) {
        logger_->Error("❌ Alarm config validation failed: " + entity.getName());
        return false;
    }
    
    // 이름 중복 검사 (자신 제외)
    if (isNameTaken(entity.getName(), entity.getTenantId(), entity.getId())) {
        logger_->Error("❌ Alarm config name conflict: " + entity.getName());
        return false;
    }
    
    // IRepository의 표준 update 구현 사용
    return IRepository<AlarmConfigEntity>::update(entity);
}

bool AlarmConfigRepository::deleteById(int id) {
    logger_->Debug("🗑️ AlarmConfigRepository::deleteById(" + std::to_string(id) + ")");
    
    // IRepository의 표준 delete 구현 사용 (캐시도 자동 삭제)
    return IRepository<AlarmConfigEntity>::deleteById(id);
}

bool AlarmConfigRepository::exists(int id) {
    return findById(id).has_value();
}

std::vector<AlarmConfigEntity> AlarmConfigRepository::findByIds(const std::vector<int>& ids) {
    // IRepository의 표준 구현 사용 (자동 캐시 활용)
    return IRepository<AlarmConfigEntity>::findByIds(ids);
}

int AlarmConfigRepository::saveBulk(std::vector<AlarmConfigEntity>& entities) {
    logger_->Info("💾 AlarmConfigRepository::saveBulk() - " + std::to_string(entities.size()) + " configs");
    
    // 각 설정 유효성 검사
    int valid_count = 0;
    for (auto& config : entities) {
        if (validateAlarmConfig(config) && 
            !isNameTaken(config.getName(), config.getTenantId())) {
            valid_count++;
        }
    }
    
    if (valid_count != entities.size()) {
        logger_->Warning("⚠️ Some alarm configs failed validation. Valid: " + 
                        std::to_string(valid_count) + "/" + std::to_string(entities.size()));
    }
    
    // IRepository의 표준 saveBulk 구현 사용
    return IRepository<AlarmConfigEntity>::saveBulk(entities);
}

int AlarmConfigRepository::updateBulk(const std::vector<AlarmConfigEntity>& entities) {
    logger_->Info("🔄 AlarmConfigRepository::updateBulk() - " + std::to_string(entities.size()) + " configs");
    
    // IRepository의 표준 updateBulk 구현 사용
    return IRepository<AlarmConfigEntity>::updateBulk(entities);
}

int AlarmConfigRepository::deleteByIds(const std::vector<int>& ids) {
    logger_->Info("🗑️ AlarmConfigRepository::deleteByIds() - " + std::to_string(ids.size()) + " configs");
    
    // IRepository의 표준 deleteByIds 구현 사용
    return IRepository<AlarmConfigEntity>::deleteByIds(ids);
}

std::vector<AlarmConfigEntity> AlarmConfigRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    // IRepository의 표준 findByConditions 구현 사용
    return IRepository<AlarmConfigEntity>::findByConditions(conditions, order_by, pagination);
}

int AlarmConfigRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    // IRepository의 표준 countByConditions 구현 사용
    return IRepository<AlarmConfigEntity>::countByConditions(conditions);
}

int AlarmConfigRepository::getTotalCount() {
    return countByConditions({});
}

// =======================================================================
// 알람 설정 전용 조회 메서드들
// =======================================================================

std::optional<AlarmConfigEntity> AlarmConfigRepository::findByName(const std::string& name, int tenant_id) {
    logger_->Debug("🔍 AlarmConfigRepository::findByName(" + name + ", " + std::to_string(tenant_id) + ")");
    
    std::vector<QueryCondition> conditions = {
        QueryCondition("name", "=", name),
        QueryCondition("tenant_id", "=", std::to_string(tenant_id))
    };
    
    auto configs = findByConditions(conditions);
    return configs.empty() ? std::nullopt : std::make_optional(configs[0]);
}

std::vector<AlarmConfigEntity> AlarmConfigRepository::findByTenant(int tenant_id) {
    logger_->Debug("🔍 AlarmConfigRepository::findByTenant(" + std::to_string(tenant_id) + ")");
    
    return findByConditions({QueryCondition("tenant_id", "=", std::to_string(tenant_id))},
                           OrderBy("name", "ASC"));
}

std::vector<AlarmConfigEntity> AlarmConfigRepository::findByDataPoint(int data_point_id) {
    logger_->Debug("🔍 AlarmConfigRepository::findByDataPoint(" + std::to_string(data_point_id) + ")");
    
    return findByConditions({QueryCondition("data_point_id", "=", std::to_string(data_point_id))},
                           OrderBy("priority", "DESC"));
}

std::vector<AlarmConfigEntity> AlarmConfigRepository::findByType(AlarmConfigEntity::AlarmType type) {
    logger_->Debug("🔍 AlarmConfigRepository::findByType()");
    
    return findByConditions({buildTypeCondition(type)},
                           OrderBy("name", "ASC"));
}

std::vector<AlarmConfigEntity> AlarmConfigRepository::findBySeverity(AlarmConfigEntity::Severity severity) {
    logger_->Debug("🔍 AlarmConfigRepository::findBySeverity()");
    
    return findByConditions({buildSeverityCondition(severity)},
                           OrderBy("name", "ASC"));
}

std::vector<AlarmConfigEntity> AlarmConfigRepository::findActiveConfigs() {
    logger_->Debug("🔍 AlarmConfigRepository::findActiveConfigs()");
    
    return findByConditions({QueryCondition("is_enabled", "=", "true")},
                           OrderBy("priority", "DESC"));
}

std::vector<AlarmConfigEntity> AlarmConfigRepository::findByDevice(int device_id) {
    logger_->Debug("🔍 AlarmConfigRepository::findByDevice(" + std::to_string(device_id) + ")");
    
    // 디바이스의 데이터포인트들을 통해 간접 조회
    // 실제로는 JOIN 쿼리나 서브쿼리 사용
    return findByConditions({QueryCondition("device_id", "=", std::to_string(device_id))},
                           OrderBy("priority", "DESC"));
}

std::vector<AlarmConfigEntity> AlarmConfigRepository::findByPriorityRange(int min_priority, int max_priority) {
    logger_->Debug("🔍 AlarmConfigRepository::findByPriorityRange(" + 
                  std::to_string(min_priority) + "-" + std::to_string(max_priority) + ")");
    
    std::vector<QueryCondition> conditions = {
        QueryCondition("priority", ">=", std::to_string(min_priority)),
        QueryCondition("priority", "<=", std::to_string(max_priority))
    };
    
    return findByConditions(conditions, OrderBy("priority", "DESC"));
}

// =======================================================================
// 알람 관리 특화 메서드들
// =======================================================================

bool AlarmConfigRepository::isNameTaken(const std::string& name, int tenant_id, int exclude_id) {
    std::vector<QueryCondition> conditions = {
        QueryCondition("name", "=", name),
        QueryCondition("tenant_id", "=", std::to_string(tenant_id))
    };
    
    auto configs = findByConditions(conditions);
    
    // exclude_id가 있으면 해당 설정은 제외
    if (exclude_id > 0) {
        configs.erase(std::remove_if(configs.begin(), configs.end(),
                     [exclude_id](const AlarmConfigEntity& config) {
                         return config.getId() == exclude_id;
                     }), configs.end());
    }
    
    return !configs.empty();
}

bool AlarmConfigRepository::enableConfig(int config_id, bool enabled) {
    logger_->Info("⚡ AlarmConfigRepository::enableConfig(" + std::to_string(config_id) + 
                 ", " + (enabled ? "enable" : "disable") + ")");
    
    auto config = findById(config_id);
    if (!config.has_value()) {
        logger_->Error("❌ Alarm config not found: " + std::to_string(config_id));
        return false;
    }
    
    config->setEnabled(enabled);
    config->markModified();
    
    bool success = update(*config);
    if (success) {
        logger_->Info("✅ Alarm config " + (enabled ? "enabled" : "disabled") + ": " + config->getName());
    } else {
        logger_->Error("❌ Failed to " + std::string(enabled ? "enable" : "disable") + " alarm config");
    }
    
    return success;
}

bool AlarmConfigRepository::updateThresholds(int config_id, double low_threshold, double high_threshold) {
    logger_->Info("📊 AlarmConfigRepository::updateThresholds(" + std::to_string(config_id) + 
                 ", " + std::to_string(low_threshold) + ", " + std::to_string(high_threshold) + ")");
    
    auto config = findById(config_id);
    if (!config.has_value()) {
        logger_->Error("❌ Alarm config not found: " + std::to_string(config_id));
        return false;
    }
    
    config->setLowThreshold(low_threshold);
    config->setHighThreshold(high_threshold);
    config->markModified();
    
    bool success = update(*config);
    if (success) {
        logger_->Info("✅ Thresholds updated: " + config->getName());
    } else {
        logger_->Error("❌ Threshold update failed: " + config->getName());
    }
    
    return success;
}

bool AlarmConfigRepository::updatePriority(int config_id, int new_priority) {
    logger_->Info("🔢 AlarmConfigRepository::updatePriority(" + std::to_string(config_id) + 
                 ", " + std::to_string(new_priority) + ")");
    
    auto config = findById(config_id);
    if (!config.has_value()) {
        logger_->Error("❌ Alarm config not found: " + std::to_string(config_id));
        return false;
    }
    
    config->setPriority(new_priority);
    config->markModified();
    
    bool success = update(*config);
    if (success) {
        logger_->Info("✅ Priority updated: " + config->getName() + " -> " + std::to_string(new_priority));
    } else {
        logger_->Error("❌ Priority update failed: " + config->getName());
    }
    
    return success;
}

bool AlarmConfigRepository::updateSeverity(int config_id, AlarmConfigEntity::Severity new_severity) {
    logger_->Info("⚠️ AlarmConfigRepository::updateSeverity(" + std::to_string(config_id) + ")");
    
    auto config = findById(config_id);
    if (!config.has_value()) {
        logger_->Error("❌ Alarm config not found: " + std::to_string(config_id));
        return false;
    }
    
    config->setSeverity(new_severity);
    config->markModified();
    
    bool success = update(*config);
    if (success) {
        logger_->Info("✅ Severity updated: " + config->getName());
    } else {
        logger_->Error("❌ Severity update failed: " + config->getName());
    }
    
    return success;
}

// =======================================================================
// 알람 통계 및 분석 메서드들
// =======================================================================

int AlarmConfigRepository::countByTenant(int tenant_id) {
    return countByConditions({QueryCondition("tenant_id", "=", std::to_string(tenant_id))});
}

int AlarmConfigRepository::countByDataPoint(int data_point_id) {
    return countByConditions({QueryCondition("data_point_id", "=", std::to_string(data_point_id))});
}

std::map<std::string, int> AlarmConfigRepository::getTypeStats() {
    logger_->Debug("📊 AlarmConfigRepository::getTypeStats()");
    
    std::map<std::string, int> type_stats;
    auto all_configs = findAll();
    
    for (const auto& config : all_configs) {
        type_stats[config.typeToString()]++;
    }
    
    return type_stats;
}

std::map<std::string, int> AlarmConfigRepository::getSeverityStats() {
    logger_->Debug("📊 AlarmConfigRepository::getSeverityStats()");
    
    std::map<std::string, int> severity_stats;
    auto all_configs = findAll();
    
    for (const auto& config : all_configs) {
        severity_stats[config.severityToString()]++;
    }
    
    return severity_stats;
}

std::map<std::string, int> AlarmConfigRepository::getStatusStats() {
    logger_->Debug("📊 AlarmConfigRepository::getStatusStats()");
    
    std::map<std::string, int> status_stats;
    status_stats["enabled"] = countByConditions({QueryCondition("is_enabled", "=", "true")});
    status_stats["disabled"] = countByConditions({QueryCondition("is_enabled", "=", "false")});
    
    return status_stats;
}

std::vector<AlarmConfigEntity> AlarmConfigRepository::findTopPriorityConfigs(int limit) {
    logger_->Debug("📊 AlarmConfigRepository::findTopPriorityConfigs(" + std::to_string(limit) + ")");
    
    return findByConditions({QueryCondition("is_enabled", "=", "true")}, 
                           OrderBy("priority", "DESC"), 
                           Pagination(0, limit));
}

std::vector<AlarmConfigEntity> AlarmConfigRepository::findRecentConfigs(int limit) {
    logger_->Debug("📊 AlarmConfigRepository::findRecentConfigs(" + std::to_string(limit) + ")");
    
    return findByConditions({}, 
                           OrderBy("created_at", "DESC"), 
                           Pagination(0, limit));
}

// =======================================================================
// 알람 설정 배포 및 동기화
// =======================================================================

bool AlarmConfigRepository::deployToDevice(int config_id, int device_id) {
    logger_->Info("🚀 AlarmConfigRepository::deployToDevice(" + std::to_string(config_id) + 
                 " -> " + std::to_string(device_id) + ")");
    
    auto config = findById(config_id);
    if (!config.has_value()) {
        logger_->Error("❌ Alarm config not found: " + std::to_string(config_id));
        return false;
    }
    
    if (!config->isEnabled()) {
        logger_->Error("❌ Cannot deploy disabled alarm config: " + config->getName());
        return false;
    }
    
    // 실제 디바이스 배포 로직은 별도 서비스에서 처리
    logger_->Info("✅ Alarm config deployment initiated: " + config->getName());
    return true;
}

bool AlarmConfigRepository::syncWithDevice(int config_id) {
    logger_->Info("🔄 AlarmConfigRepository::syncWithDevice(" + std::to_string(config_id) + ")");
    
    auto config = findById(config_id);
    if (!config.has_value()) {
        logger_->Error("❌ Alarm config not found: " + std::to_string(config_id));
        return false;
    }
    
    // 실제 동기화 로직은 별도 서비스에서 처리
    logger_->Info("✅ Alarm config sync initiated: " + config->getName());
    return true;
}

std::vector<AlarmConfigEntity> AlarmConfigRepository::findConfigsNeedingSync() {
    logger_->Debug("🔄 AlarmConfigRepository::findConfigsNeedingSync()");
    
    // 수정된 후 동기화되지 않은 설정들 조회
    return findByConditions({
        QueryCondition("is_enabled", "=", "true"),
        QueryCondition("needs_sync", "=", "true")
    }, OrderBy("priority", "DESC"));
}

bool AlarmConfigRepository::markSyncCompleted(int config_id) {
    logger_->Info("✅ AlarmConfigRepository::markSyncCompleted(" + std::to_string(config_id) + ")");
    
    auto config = findById(config_id);
    if (!config.has_value()) {
        logger_->Error("❌ Alarm config not found: " + std::to_string(config_id));
        return false;
    }
    
    config->markSyncCompleted();
    config->markModified();
    
    return update(*config);
}

// =======================================================================
// 내부 헬퍼 메서드들
// =======================================================================

bool AlarmConfigRepository::validateAlarmConfig(const AlarmConfigEntity& config) const {
    // 이름 검사
    if (config.getName().empty() || config.getName().length() < 2) {
        return false;
    }
    
    // 데이터포인트 ID 검사
    if (config.getDataPointId() <= 0) {
        return false;
    }
    
    // 테넌트 ID 검사
    if (config.getTenantId() <= 0) {
        return false;
    }
    
    // 임계값 검사 (하한 < 상한)
    if (config.getLowThreshold() >= config.getHighThreshold()) {
        return false;
    }
    
    // 우선순위 검사 (1-100 범위)
    if (config.getPriority() < 1 || config.getPriority() > 100) {
        return false;
    }
    
    return true;
}

QueryCondition AlarmConfigRepository::buildTypeCondition(AlarmConfigEntity::AlarmType type) const {
    std::string type_str;
    switch (type) {
        case AlarmConfigEntity::AlarmType::THRESHOLD: type_str = "threshold"; break;
        case AlarmConfigEntity::AlarmType::DEVIATION: type_str = "deviation"; break;
        case AlarmConfigEntity::AlarmType::RATE_OF_CHANGE: type_str = "rate_of_change"; break;
        case AlarmConfigEntity::AlarmType::COMMUNICATION: type_str = "communication"; break;
        default: type_str = "unknown"; break;
    }
    
    return QueryCondition("alarm_type", "=", type_str);
}

QueryCondition AlarmConfigRepository::buildSeverityCondition(AlarmConfigEntity::Severity severity) const {
    std::string severity_str;
    switch (severity) {
        case AlarmConfigEntity::Severity::LOW: severity_str = "low"; break;
        case AlarmConfigEntity::Severity::MEDIUM: severity_str = "medium"; break;
        case AlarmConfigEntity::Severity::HIGH: severity_str = "high"; break;
        case AlarmConfigEntity::Severity::CRITICAL: severity_str = "critical"; break;
        default: severity_str = "unknown"; break;
    }
    
    return QueryCondition("severity", "=", severity_str);
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne