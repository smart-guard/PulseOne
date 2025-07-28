/**
 * @file TenantRepository.cpp
 * @brief PulseOne TenantRepository 구현 - IRepository 기반 테넌트 관리
 * @author PulseOne Development Team
 * @date 2025-07-28
 */

#include "Database/Repositories/TenantRepository.h"
#include "Utils/LogManager.h"
#include <chrono>
#include <algorithm>
#include <sstream>
#include <regex>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =======================================================================
// 생성자 및 초기화
// =======================================================================

TenantRepository::TenantRepository() 
    : IRepository<TenantEntity>("TenantRepository") {
    logger_->Info("🏢 TenantRepository initialized with IRepository caching system");
    logger_->Info("✅ Cache enabled: " + std::string(cache_enabled_ ? "YES" : "NO"));
}

// =======================================================================
// IRepository 인터페이스 구현
// =======================================================================

std::vector<TenantEntity> TenantRepository::findAll() {
    logger_->Debug("🔍 TenantRepository::findAll() - Fetching all tenants");
    
    return findByConditions({}, OrderBy("name", "ASC"));
}

std::optional<TenantEntity> TenantRepository::findById(int id) {
    logger_->Debug("🔍 TenantRepository::findById(" + std::to_string(id) + ")");
    
    // 캐시 먼저 확인 (IRepository 자동 처리)
    auto cached = getCachedEntity(id);
    if (cached.has_value()) {
        logger_->Debug("✅ Cache HIT for tenant ID: " + std::to_string(id));
        return cached;
    }
    
    // DB에서 조회
    auto tenants = findByConditions({QueryCondition("id", "=", std::to_string(id))});
    if (!tenants.empty()) {
        // 캐시에 저장 (IRepository 자동 처리)
        setCachedEntity(id, tenants[0]);
        logger_->Debug("✅ Tenant found and cached: " + tenants[0].getName());
        return tenants[0];
    }
    
    logger_->Debug("❌ Tenant not found: " + std::to_string(id));
    return std::nullopt;
}

bool TenantRepository::save(TenantEntity& entity) {
    logger_->Debug("💾 TenantRepository::save() - " + entity.getName());
    
    // 유효성 검사
    if (!validateTenant(entity)) {
        logger_->Error("❌ Tenant validation failed: " + entity.getName());
        return false;
    }
    
    // 중복 검사
    if (isDomainTaken(entity.getDomain()) || isNameTaken(entity.getName())) {
        logger_->Error("❌ Domain or name already taken: " + entity.getName());
        return false;
    }
    
    // IRepository의 표준 save 구현 사용
    return IRepository<TenantEntity>::save(entity);
}

bool TenantRepository::update(const TenantEntity& entity) {
    logger_->Debug("🔄 TenantRepository::update() - " + entity.getName());
    
    // 유효성 검사
    if (!validateTenant(entity)) {
        logger_->Error("❌ Tenant validation failed: " + entity.getName());
        return false;
    }
    
    // 중복 검사 (자신 제외)
    if (isDomainTaken(entity.getDomain(), entity.getId()) || 
        isNameTaken(entity.getName(), entity.getId())) {
        logger_->Error("❌ Domain or name conflict: " + entity.getName());
        return false;
    }
    
    // IRepository의 표준 update 구현 사용
    return IRepository<TenantEntity>::update(entity);
}

bool TenantRepository::deleteById(int id) {
    logger_->Debug("🗑️ TenantRepository::deleteById(" + std::to_string(id) + ")");
    
    // IRepository의 표준 delete 구현 사용 (캐시도 자동 삭제)
    return IRepository<TenantEntity>::deleteById(id);
}

bool TenantRepository::exists(int id) {
    return findById(id).has_value();
}

std::vector<TenantEntity> TenantRepository::findByIds(const std::vector<int>& ids) {
    // IRepository의 표준 구현 사용 (자동 캐시 활용)
    return IRepository<TenantEntity>::findByIds(ids);
}

int TenantRepository::saveBulk(std::vector<TenantEntity>& entities) {
    logger_->Info("💾 TenantRepository::saveBulk() - " + std::to_string(entities.size()) + " tenants");
    
    // 각 테넌트 유효성 검사
    int valid_count = 0;
    for (auto& tenant : entities) {
        if (validateTenant(tenant) && 
            !isDomainTaken(tenant.getDomain()) && 
            !isNameTaken(tenant.getName())) {
            valid_count++;
        }
    }
    
    if (valid_count != entities.size()) {
        logger_->Warning("⚠️ Some tenants failed validation. Valid: " + 
                        std::to_string(valid_count) + "/" + std::to_string(entities.size()));
    }
    
    // IRepository의 표준 saveBulk 구현 사용
    return IRepository<TenantEntity>::saveBulk(entities);
}

int TenantRepository::updateBulk(const std::vector<TenantEntity>& entities) {
    logger_->Info("🔄 TenantRepository::updateBulk() - " + std::to_string(entities.size()) + " tenants");
    
    // IRepository의 표준 updateBulk 구현 사용
    return IRepository<TenantEntity>::updateBulk(entities);
}

int TenantRepository::deleteByIds(const std::vector<int>& ids) {
    logger_->Info("🗑️ TenantRepository::deleteByIds() - " + std::to_string(ids.size()) + " tenants");
    
    // IRepository의 표준 deleteByIds 구현 사용
    return IRepository<TenantEntity>::deleteByIds(ids);
}

std::vector<TenantEntity> TenantRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    // IRepository의 표준 findByConditions 구현 사용
    return IRepository<TenantEntity>::findByConditions(conditions, order_by, pagination);
}

int TenantRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    // IRepository의 표준 countByConditions 구현 사용
    return IRepository<TenantEntity>::countByConditions(conditions);
}

int TenantRepository::getTotalCount() {
    return countByConditions({});
}

// =======================================================================
// 테넌트 전용 조회 메서드들
// =======================================================================

std::optional<TenantEntity> TenantRepository::findByDomain(const std::string& domain) {
    logger_->Debug("🔍 TenantRepository::findByDomain(" + domain + ")");
    
    auto tenants = findByConditions({QueryCondition("domain", "=", domain)});
    return tenants.empty() ? std::nullopt : std::make_optional(tenants[0]);
}

std::optional<TenantEntity> TenantRepository::findByName(const std::string& name) {
    logger_->Debug("🔍 TenantRepository::findByName(" + name + ")");
    
    auto tenants = findByConditions({QueryCondition("name", "=", name)});
    return tenants.empty() ? std::nullopt : std::make_optional(tenants[0]);
}

std::vector<TenantEntity> TenantRepository::findByStatus(TenantEntity::Status status) {
    logger_->Debug("🔍 TenantRepository::findByStatus()");
    
    return findByConditions({buildStatusCondition(status)},
                           OrderBy("name", "ASC"));
}

std::vector<TenantEntity> TenantRepository::findActiveTenants() {
    logger_->Debug("🔍 TenantRepository::findActiveTenants()");
    
    return findByStatus(TenantEntity::Status::ACTIVE);
}

std::vector<TenantEntity> TenantRepository::findTenantsExpiringSoon(int days_before_expiry) {
    logger_->Debug("🔍 TenantRepository::findTenantsExpiringSoon(" + std::to_string(days_before_expiry) + ")");
    
    auto now_condition = buildDateRangeCondition("subscription_end", 0, false);
    auto future_condition = buildDateRangeCondition("subscription_end", days_before_expiry, true);
    
    return findByConditions({now_condition, future_condition},
                           OrderBy("subscription_end", "ASC"));
}

std::vector<TenantEntity> TenantRepository::findExpiredTenants() {
    logger_->Debug("🔍 TenantRepository::findExpiredTenants()");
    
    return findByConditions({buildDateRangeCondition("subscription_end", 0, true)},
                           OrderBy("subscription_end", "DESC"));
}

std::vector<TenantEntity> TenantRepository::findTrialTenants() {
    logger_->Debug("🔍 TenantRepository::findTrialTenants()");
    
    return findByConditions({QueryCondition("is_trial", "=", "true")},
                           OrderBy("subscription_end", "ASC"));
}

std::vector<TenantEntity> TenantRepository::findByNamePattern(const std::string& name_pattern) {
    logger_->Debug("🔍 TenantRepository::findByNamePattern(" + name_pattern + ")");
    
    return findByConditions({QueryCondition("name", "LIKE", "%" + name_pattern + "%")},
                           OrderBy("name", "ASC"));
}

// =======================================================================
// 테넌트 관리 특화 메서드들
// =======================================================================

bool TenantRepository::isDomainTaken(const std::string& domain, int exclude_id) {
    auto tenants = findByConditions({QueryCondition("domain", "=", domain)});
    
    // exclude_id가 있으면 해당 테넌트는 제외
    if (exclude_id > 0) {
        tenants.erase(std::remove_if(tenants.begin(), tenants.end(),
                     [exclude_id](const TenantEntity& tenant) {
                         return tenant.getId() == exclude_id;
                     }), tenants.end());
    }
    
    return !tenants.empty();
}

bool TenantRepository::isNameTaken(const std::string& name, int exclude_id) {
    auto tenants = findByConditions({QueryCondition("name", "=", name)});
    
    // exclude_id가 있으면 해당 테넌트는 제외
    if (exclude_id > 0) {
        tenants.erase(std::remove_if(tenants.begin(), tenants.end(),
                     [exclude_id](const TenantEntity& tenant) {
                         return tenant.getId() == exclude_id;
                     }), tenants.end());
    }
    
    return !tenants.empty();
}

std::map<std::string, int> TenantRepository::checkLimits(int tenant_id) {
    logger_->Debug("📊 TenantRepository::checkLimits(" + std::to_string(tenant_id) + ")");
    
    auto tenant = findById(tenant_id);
    if (!tenant.has_value()) {
        return {};
    }
    
    std::map<std::string, int> limits;
    limits["max_users"] = tenant->getMaxUsers();
    limits["max_devices"] = tenant->getMaxDevices();
    limits["max_data_points"] = tenant->getMaxDataPoints();
    
    return limits;
}

bool TenantRepository::extendSubscription(int tenant_id, int additional_days) {
    logger_->Info("📅 TenantRepository::extendSubscription(" + std::to_string(tenant_id) + 
                 ", +" + std::to_string(additional_days) + " days)");
    
    auto tenant = findById(tenant_id);
    if (!tenant.has_value()) {
        logger_->Error("❌ Tenant not found: " + std::to_string(tenant_id));
        return false;
    }
    
    tenant->extendSubscription(additional_days);
    tenant->markModified();
    
    bool success = update(*tenant);
    if (success) {
        logger_->Info("✅ Subscription extended: " + tenant->getName());
    } else {
        logger_->Error("❌ Subscription extension failed: " + tenant->getName());
    }
    
    return success;
}

bool TenantRepository::updateStatus(int tenant_id, TenantEntity::Status new_status) {
    logger_->Info("🔄 TenantRepository::updateStatus(" + std::to_string(tenant_id) + ")");
    
    auto tenant = findById(tenant_id);
    if (!tenant.has_value()) {
        logger_->Error("❌ Tenant not found: " + std::to_string(tenant_id));
        return false;
    }
    
    tenant->setStatus(new_status);
    tenant->markModified();
    
    bool success = update(*tenant);
    if (success) {
        logger_->Info("✅ Status updated: " + tenant->getName() + " -> " + tenant->statusToString());
    } else {
        logger_->Error("❌ Status update failed: " + tenant->getName());
    }
    
    return success;
}

bool TenantRepository::updateLimits(int tenant_id, const std::map<std::string, int>& limits) {
    logger_->Info("📊 TenantRepository::updateLimits(" + std::to_string(tenant_id) + ")");
    
    auto tenant = findById(tenant_id);
    if (!tenant.has_value()) {
        logger_->Error("❌ Tenant not found: " + std::to_string(tenant_id));
        return false;
    }
    
    for (const auto& [key, value] : limits) {
        if (key == "max_users") {
            tenant->setMaxUsers(value);
        } else if (key == "max_devices") {
            tenant->setMaxDevices(value);
        } else if (key == "max_data_points") {
            tenant->setMaxDataPoints(value);
        }
    }
    
    tenant->markModified();
    
    bool success = update(*tenant);
    if (success) {
        logger_->Info("✅ Limits updated: " + tenant->getName());
    } else {
        logger_->Error("❌ Limits update failed: " + tenant->getName());
    }
    
    return success;
}

// =======================================================================
// 테넌트 통계 및 분석 메서드들
// =======================================================================

std::map<std::string, int> TenantRepository::getTenantUsageStats(int tenant_id) {
    logger_->Debug("📊 TenantRepository::getTenantUsageStats(" + std::to_string(tenant_id) + ")");
    
    auto tenant = findById(tenant_id);
    if (!tenant.has_value()) {
        return {};
    }
    
    return tenant->getUsageStats();
}

std::map<std::string, int> TenantRepository::getTenantStatusStats() {
    logger_->Debug("📊 TenantRepository::getTenantStatusStats()");
    
    std::map<std::string, int> status_stats;
    auto all_tenants = findAll();
    
    for (const auto& tenant : all_tenants) {
        status_stats[tenant.statusToString()]++;
    }
    
    return status_stats;
}

std::vector<std::pair<int, int>> TenantRepository::getExpirationSchedule(int days_ahead) {
    logger_->Debug("📊 TenantRepository::getExpirationSchedule(" + std::to_string(days_ahead) + ")");
    
    auto future_condition = buildDateRangeCondition("subscription_end", days_ahead, true);
    auto tenants = findByConditions({future_condition}, OrderBy("subscription_end", "ASC"));
    
    std::vector<std::pair<int, int>> schedule;
    for (const auto& tenant : tenants) {
        int remaining_days = tenant.getRemainingDays();
        schedule.emplace_back(tenant.getId(), remaining_days);
    }
    
    return schedule;
}

std::vector<TenantEntity> TenantRepository::findTopUsageTenants(int limit) {
    logger_->Debug("📊 TenantRepository::findTopUsageTenants(" + std::to_string(limit) + ")");
    
    // 전체 테넌트를 가져와서 사용률 기준으로 정렬
    auto all_tenants = findAll();
    
    std::sort(all_tenants.begin(), all_tenants.end(),
             [](const TenantEntity& a, const TenantEntity& b) {
                 auto a_stats = a.getUsageStats();
                 auto b_stats = b.getUsageStats();
                 int a_total = a_stats["users"] + a_stats["devices"] + a_stats["data_points"];
                 int b_total = b_stats["users"] + b_stats["devices"] + b_stats["data_points"];
                 return a_total > b_total;
             });
    
    if (limit < all_tenants.size()) {
        all_tenants.resize(limit);
    }
    
    return all_tenants;
}

std::vector<TenantEntity> TenantRepository::findRecentTenants(int limit) {
    logger_->Debug("📊 TenantRepository::findRecentTenants(" + std::to_string(limit) + ")");
    
    return findByConditions({}, 
                           OrderBy("created_at", "DESC"), 
                           Pagination(0, limit));
}

// =======================================================================
// 백업 및 복원 메서드들
// =======================================================================

std::string TenantRepository::exportTenantData(int tenant_id) {
    logger_->Info("💾 TenantRepository::exportTenantData(" + std::to_string(tenant_id) + ")");
    
    auto tenant = findById(tenant_id);
    if (!tenant.has_value()) {
        logger_->Error("❌ Tenant not found for export: " + std::to_string(tenant_id));
        return "";
    }
    
    // JSON 형태로 테넌트 데이터 변환 (간단한 구현)
    std::stringstream json;
    json << "{\n";
    json << "  \"name\": \"" << tenant->getName() << "\",\n";
    json << "  \"domain\": \"" << tenant->getDomain() << "\",\n";
    json << "  \"status\": \"" << tenant->statusToString() << "\",\n";
    json << "  \"max_users\": " << tenant->getMaxUsers() << ",\n";
    json << "  \"max_devices\": " << tenant->getMaxDevices() << ",\n";
    json << "  \"max_data_points\": " << tenant->getMaxDataPoints() << "\n";
    json << "}";
    
    logger_->Info("✅ Tenant data exported: " + tenant->getName());
    return json.str();
}

std::optional<int> TenantRepository::importTenantData(const std::string& backup_data) {
    logger_->Info("📥 TenantRepository::importTenantData()");
    
    // JSON 파싱 및 테넌트 생성 (간단한 구현)
    // 실제로는 JSON 라이브러리 사용 권장
    
    logger_->Warning("⚠️ importTenantData() - JSON parsing not implemented yet");
    return std::nullopt;
}

bool TenantRepository::cloneTenantConfig(int source_tenant_id, int target_tenant_id, bool copy_users) {
    logger_->Info("📋 TenantRepository::cloneTenantConfig(" + 
                 std::to_string(source_tenant_id) + " -> " + std::to_string(target_tenant_id) + ")");
    
    auto source = findById(source_tenant_id);
    auto target = findById(target_tenant_id);
    
    if (!source.has_value() || !target.has_value()) {
        logger_->Error("❌ Source or target tenant not found");
        return false;
    }
    
    // 설정 복사
    target->setMaxUsers(source->getMaxUsers());
    target->setMaxDevices(source->getMaxDevices());
    target->setMaxDataPoints(source->getMaxDataPoints());
    target->markModified();
    
    bool success = update(*target);
    
    if (success) {
        logger_->Info("✅ Tenant config cloned: " + source->getName() + " -> " + target->getName());
    } else {
        logger_->Error("❌ Tenant config clone failed");
    }
    
    return success;
}

// =======================================================================
// 내부 헬퍼 메서드들
// =======================================================================

bool TenantRepository::isSubscriptionExpiredInternal(const TenantEntity& tenant) const {
    return tenant.isSubscriptionExpired();
}

bool TenantRepository::validateTenant(const TenantEntity& tenant) const {
    // 테넌트명 검사
    if (tenant.getName().empty() || tenant.getName().length() < 2) {
        return false;
    }
    
    // 도메인 검사
    if (tenant.getDomain().empty() || !TenantEntity::isValidDomain(tenant.getDomain())) {
        return false;
    }
    
    // 제한 값 검사
    if (tenant.getMaxUsers() < 1 || tenant.getMaxDevices() < 1 || tenant.getMaxDataPoints() < 1) {
        return false;
    }
    
    return true;
}

QueryCondition TenantRepository::buildStatusCondition(TenantEntity::Status status) const {
    std::string status_str;
    switch (status) {
        case TenantEntity::Status::ACTIVE: status_str = "active"; break;
        case TenantEntity::Status::INACTIVE: status_str = "inactive"; break;
        case TenantEntity::Status::SUSPENDED: status_str = "suspended"; break;
        case TenantEntity::Status::EXPIRED: status_str = "expired"; break;
        default: status_str = "unknown"; break;
    }
    
    return QueryCondition("status", "=", status_str);
}

QueryCondition TenantRepository::buildDateRangeCondition(const std::string& field_name, 
                                                        int days_from_now, 
                                                        bool is_before) const {
    auto now = std::chrono::system_clock::now();
    auto target_time = now + std::chrono::hours(24 * days_from_now);
    auto time_t = std::chrono::system_clock::to_time_t(target_time);
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    
    return QueryCondition(field_name, is_before ? "<=" : ">=", ss.str());
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne