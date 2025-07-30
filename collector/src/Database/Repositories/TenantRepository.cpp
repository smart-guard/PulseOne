/**
 * @file TenantRepository.cpp
 * @brief PulseOne TenantRepository 구현 - DeviceEntity/DataPointEntity 패턴 100% 준수
 * @author PulseOne Development Team
 * @date 2025-07-28
 */

#include "Database/Repositories/TenantRepository.h"
#include "Utils/LogManager.h"
#include <chrono>
#include <algorithm>
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

// =======================================================================
// IRepository 인터페이스 구현 (DeviceRepository 패턴 100% 동일)
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
        // ❌ setCachedEntity(id, tenants[0]);  // 제거 - IRepository가 자동 관리
        logger_->Debug("✅ Tenant found: " + tenants[0].getName());
        return tenants[0];
    }
    
    logger_->Debug("❌ Tenant not found: " + std::to_string(id));
    return std::nullopt;
}

bool TenantRepository::save(TenantEntity& entity) {
    logger_->Info("💾 TenantRepository::save() - " + entity.getName());
    
    if (!validateTenant(entity)) {
        logger_->Error("❌ Invalid tenant data");
        return false;
    }
    
    if (isNameTaken(entity.getName())) {
        logger_->Error("❌ Tenant name already exists: " + entity.getName());
        return false;
    }
    
    if (isDomainTaken(entity.getDomain())) {
        logger_->Error("❌ Domain already exists: " + entity.getDomain());
        return false;
    }
    
    return IRepository<TenantEntity>::save(entity);
}

bool TenantRepository::update(const TenantEntity& entity) {
    logger_->Info("📝 TenantRepository::update() - " + entity.getName());
    
    if (!validateTenant(entity)) {
        logger_->Error("❌ Invalid tenant data");
        return false;
    }
    
    if (isNameTaken(entity.getName(), entity.getId())) {
        logger_->Error("❌ Tenant name already exists: " + entity.getName());
        return false;
    }
    
    if (isDomainTaken(entity.getDomain(), entity.getId())) {
        logger_->Error("❌ Domain already exists: " + entity.getDomain());
        return false;
    }
    
    return IRepository<TenantEntity>::update(entity);
}

bool TenantRepository::deleteById(int id) {
    logger_->Info("🗑️ TenantRepository::deleteById(" + std::to_string(id) + ")");
    
    return IRepository<TenantEntity>::deleteById(id);
}

bool TenantRepository::exists(int id) {
    return IRepository<TenantEntity>::exists(id);
}

std::vector<TenantEntity> TenantRepository::findByIds(const std::vector<int>& ids) {
    logger_->Debug("🔍 TenantRepository::findByIds() - " + std::to_string(ids.size()) + " IDs");
    
    return IRepository<TenantEntity>::findByIds(ids);
}

int TenantRepository::saveBulk(std::vector<TenantEntity>& entities) {
    logger_->Info("💾 TenantRepository::saveBulk() - " + std::to_string(entities.size()) + " tenants");
    
    // 유효성 검사
    int valid_count = 0;
    for (const auto& tenant : entities) {
        if (validateTenant(tenant) &&
            !isNameTaken(tenant.getName()) &&
            !isDomainTaken(tenant.getDomain())) {
            valid_count++;
        }
    }
    
    if (valid_count != static_cast<int>(entities.size())) {  // ✅ static_cast 추가
        logger_->Warn("⚠️ Some tenants failed validation. Valid: " +  // ✅ Warning → Warn
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
// 테넌트 전용 조회 메서드들 (DeviceRepository 패턴)
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
    
    return findByConditions({QueryCondition("status", "=", "ACTIVE")},
                           OrderBy("name", "ASC"));
}

std::vector<TenantEntity> TenantRepository::findTenantsExpiringSoon(int days_before_expiry) {
    logger_->Debug("🔍 TenantRepository::findTenantsExpiringSoon(" + std::to_string(days_before_expiry) + ")");
    
    // 현재 날짜부터 지정된 일수 후까지의 범위에서 만료 예정인 테넌트들
    auto future_date = std::chrono::system_clock::now() + std::chrono::hours(24 * days_before_expiry);
    
    std::vector<QueryCondition> conditions = {
        QueryCondition("subscription_end", ">=", "NOW()"),
        QueryCondition("subscription_end", "<=", "DATE_ADD(NOW(), INTERVAL " + std::to_string(days_before_expiry) + " DAY)")
    };
    
    return findByConditions(conditions, OrderBy("subscription_end", "ASC"));
}

std::vector<TenantEntity> TenantRepository::findExpiredTenants() {
    logger_->Debug("🔍 TenantRepository::findExpiredTenants()");
    
    return findByConditions({QueryCondition("subscription_end", "<", "NOW()")},
                           OrderBy("subscription_end", "DESC"));
}

std::vector<TenantEntity> TenantRepository::findTrialTenants() {
    logger_->Debug("🔍 TenantRepository::findTrialTenants()");
    
    return findByConditions({QueryCondition("status", "=", "TRIAL")},
                           OrderBy("created_at", "DESC"));
}

std::vector<TenantEntity> TenantRepository::findByNamePattern(const std::string& name_pattern) {
    logger_->Debug("🔍 TenantRepository::findByNamePattern(" + name_pattern + ")");
    
    return findByConditions({QueryCondition("name", "LIKE", name_pattern)},
                           OrderBy("name", "ASC"));
}

// =======================================================================
// 테넌트 관리 특화 메서드들
// =======================================================================

bool TenantRepository::isDomainTaken(const std::string& domain, int exclude_id) {
    logger_->Debug("🔍 TenantRepository::isDomainTaken(" + domain + ", " + std::to_string(exclude_id) + ")");
    
    std::vector<QueryCondition> conditions = {
        QueryCondition("domain", "=", domain)
    };
    
    if (exclude_id > 0) {
        conditions.push_back(QueryCondition("id", "!=", std::to_string(exclude_id)));
    }
    
    auto tenants = findByConditions(conditions);
    bool is_taken = !tenants.empty();
    
    logger_->Debug(is_taken ? "❌ Domain is taken" : "✅ Domain is available");
    return is_taken;
}

bool TenantRepository::isNameTaken(const std::string& name, int exclude_id) {
    logger_->Debug("🔍 TenantRepository::isNameTaken(" + name + ", " + std::to_string(exclude_id) + ")");
    
    std::vector<QueryCondition> conditions = {
        QueryCondition("name", "=", name)
    };
    
    if (exclude_id > 0) {
        conditions.push_back(QueryCondition("id", "!=", std::to_string(exclude_id)));
    }
    
    auto tenants = findByConditions(conditions);
    bool is_taken = !tenants.empty();
    
    logger_->Debug(is_taken ? "❌ Name is taken" : "✅ Name is available");
    return is_taken;
}

std::map<std::string, int> TenantRepository::checkLimits(int tenant_id) {
    logger_->Debug("🔍 TenantRepository::checkLimits(" + std::to_string(tenant_id) + ")");
    
    std::map<std::string, int> limits;
    
    auto tenant = findById(tenant_id);
    if (tenant.has_value()) {
        limits["max_users"] = tenant->getMaxUsers();
        limits["max_devices"] = tenant->getMaxDevices();
        limits["max_data_points"] = tenant->getMaxDataPoints();
        
        // 실제 사용량은 별도 쿼리로 조회 (간단화)
        limits["current_users"] = 0;
        limits["current_devices"] = 0;
        limits["current_data_points"] = 0;
    }
    
    return limits;
}

bool TenantRepository::extendSubscription(int tenant_id, int additional_days) {
    logger_->Info("📅 TenantRepository::extendSubscription(" + std::to_string(tenant_id) + 
                 ", " + std::to_string(additional_days) + ")");
    
    auto tenant = findById(tenant_id);
    if (!tenant.has_value()) {
        logger_->Error("❌ Tenant not found: " + std::to_string(tenant_id));
        return false;
    }
    
    tenant->extendSubscription(additional_days);
    return update(*tenant);
}

bool TenantRepository::updateStatus(int tenant_id, TenantEntity::Status new_status) {
    logger_->Info("🔄 TenantRepository::updateStatus(" + std::to_string(tenant_id) + ")");
    
    auto tenant = findById(tenant_id);
    if (!tenant.has_value()) {
        logger_->Error("❌ Tenant not found: " + std::to_string(tenant_id));
        return false;
    }
    
    tenant->setStatus(new_status);
    bool success = update(*tenant);
    
    if (success) {
        logger_->Info("✅ Status updated: " + tenant->getName() + " -> " + 
                     TenantEntity::statusToString(new_status));  // ✅ static 메서드로 호출
    }
    
    return success;
}

bool TenantRepository::updateLimits(int tenant_id, const std::map<std::string, int>& limits) {
    logger_->Info("🔄 TenantRepository::updateLimits(" + std::to_string(tenant_id) + ")");
    
    auto tenant = findById(tenant_id);
    if (!tenant.has_value()) {
        logger_->Error("❌ Tenant not found: " + std::to_string(tenant_id));
        return false;
    }
    
    // 제한값 업데이트
    if (limits.count("max_users")) {
        tenant->setMaxUsers(limits.at("max_users"));
    }
    if (limits.count("max_devices")) {
        tenant->setMaxDevices(limits.at("max_devices"));
    }
    if (limits.count("max_data_points")) {
        tenant->setMaxDataPoints(limits.at("max_data_points"));
    }
    
    return update(*tenant);
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
    
    // ❌ return tenant->getUsageStats();  // 존재하지 않는 메서드
    // ✅ 간단한 통계 반환
    std::map<std::string, int> stats;
    stats["max_users"] = tenant->getMaxUsers();
    stats["max_devices"] = tenant->getMaxDevices();
    stats["max_data_points"] = tenant->getMaxDataPoints();
    stats["current_users"] = 0;      // 실제로는 별도 쿼리 필요
    stats["current_devices"] = 0;    // 실제로는 별도 쿼리 필요
    stats["current_data_points"] = 0; // 실제로는 별도 쿼리 필요
    
    return stats;
}

std::map<std::string, int> TenantRepository::getTenantStatusStats() {
    logger_->Debug("📊 TenantRepository::getTenantStatusStats()");
    
    std::map<std::string, int> status_stats;
    auto all_tenants = findAll();
    
    for (const auto& tenant : all_tenants) {
        status_stats[TenantEntity::statusToString(tenant.getStatus())]++;  // ✅ static 메서드로 호출
    }
    
    return status_stats;
}

std::vector<std::pair<int, int>> TenantRepository::getExpirationSchedule(int days_ahead) {
    logger_->Debug("📊 TenantRepository::getExpirationSchedule(" + std::to_string(days_ahead) + ")");
    
    auto tenants = findTenantsExpiringSoon(days_ahead);
    
    std::vector<std::pair<int, int>> schedule;
    for (const auto& tenant : tenants) {
        // ❌ int remaining_days = tenant.getRemainingDays();  // 존재하지 않는 메서드
        // ✅ 직접 계산
        auto now = std::chrono::system_clock::now();
        auto remaining = std::chrono::duration_cast<std::chrono::hours>(
            tenant.getSubscriptionEnd() - now);
        int remaining_days = std::max(0, static_cast<int>(remaining.count() / 24));
        
        schedule.emplace_back(tenant.getId(), remaining_days);
    }
    
    return schedule;
}

std::vector<TenantEntity> TenantRepository::findTopUsageTenants(int limit) {
    logger_->Debug("📊 TenantRepository::findTopUsageTenants(" + std::to_string(limit) + ")");
    
    // 전체 테넌트를 가져와서 사용률 기준으로 정렬
    auto all_tenants = findAll();
    
    // ❌ 존재하지 않는 getUsageStats() 메서드 사용하는 정렬 제거
    // ✅ 간단한 정렬로 대체 (이름순)
    std::sort(all_tenants.begin(), all_tenants.end(),
             [](const TenantEntity& a, const TenantEntity& b) {
                 // 제한값 합계로 정렬 (임시)
                 int a_total = a.getMaxUsers() + a.getMaxDevices() + a.getMaxDataPoints();
                 int b_total = b.getMaxUsers() + b.getMaxDevices() + b.getMaxDataPoints();
                 return a_total > b_total;
             });
    
    if (limit < static_cast<int>(all_tenants.size())) {  // ✅ static_cast 추가
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
// 테넌트 고급 기능
// =======================================================================

std::string TenantRepository::exportTenantData(int tenant_id) {
    logger_->Info("📤 TenantRepository::exportTenantData(" + std::to_string(tenant_id) + ")");
    
    auto tenant = findById(tenant_id);
    if (!tenant.has_value()) {
        logger_->Error("❌ Tenant not found: " + std::to_string(tenant_id));
        return "";
    }
    
    // 간단한 JSON 형태로 내보내기
    std::stringstream json;
    json << "{\n";
    json << "  \"tenant_id\": " << tenant->getId() << ",\n";
    json << "  \"name\": \"" << tenant->getName() << "\",\n";
    json << "  \"domain\": \"" << tenant->getDomain() << "\",\n";
    json << "  \"status\": \"" << TenantEntity::statusToString(tenant->getStatus()) << "\",\n";  // ✅ static 메서드로 호출
    json << "  \"max_users\": " << tenant->getMaxUsers() << ",\n";
    json << "  \"max_devices\": " << tenant->getMaxDevices() << "\n";
    json << "}";
    
    return json.str();
}

std::optional<int> TenantRepository::importTenantData(const std::string& backup_data) {
    logger_->Info("📥 TenantRepository::importTenantData()");
    
    // 간단한 구현 (실제로는 JSON 파싱 필요)
    logger_->Warn("⚠️ importTenantData() - JSON parsing not implemented yet");  // ✅ Warning → Warn
    
    // 실제로는 backup_data를 파싱해서 TenantEntity 생성 후 저장
    return std::nullopt;
}

bool TenantRepository::cloneTenantConfig(int source_tenant_id, int target_tenant_id, bool copy_users) {
    logger_->Info("🔄 TenantRepository::cloneTenantConfig(" + std::to_string(source_tenant_id) + 
                 " -> " + std::to_string(target_tenant_id) + ")");
    
    // 간단한 구현 (실제로는 설정 복사 로직 필요)
    auto source = findById(source_tenant_id);
    auto target = findById(target_tenant_id);
    
    if (!source.has_value() || !target.has_value()) {
        logger_->Error("❌ Source or target tenant not found");
        return false;
    }
    
    // 제한값 복사
    target->setMaxUsers(source->getMaxUsers());
    target->setMaxDevices(source->getMaxDevices());
    target->setMaxDataPoints(source->getMaxDataPoints());
    
    return update(*target);
}

// =============================================================================
// IRepository 캐시 관리 (자동 위임)
// =============================================================================

void TenantRepository::setCacheEnabled(bool enabled) {
    // 🔥 IRepository의 캐시 관리 위임
    IRepository<TenantEntity>::setCacheEnabled(enabled);
    logger_->Info("AlarmConfigRepository cache " + std::string(enabled ? "enabled" : "disabled"));
}

bool TenantRepository::isCacheEnabled() const {
    // 🔥 IRepository의 캐시 상태 위임
    return IRepository<TenantEntity>::isCacheEnabled();
}

void TenantRepository::clearCache() {
    // 🔥 IRepository의 캐시 클리어 위임
    IRepository<TenantEntity>::clearCache();
    logger_->Info("AlarmConfigRepository cache cleared");
}

void TenantRepository::clearCacheForId(int id) {
    // 🔥 IRepository의 개별 캐시 클리어 위임
    IRepository<TenantEntity>::clearCacheForId(id);
    logger_->Debug("AlarmConfigRepository cache cleared for ID: " + std::to_string(id));
}

std::map<std::string, int> TenantRepository::getCacheStats() const {
    // 🔥 IRepository의 캐시 통계 위임
    return IRepository<TenantEntity>::getCacheStats();
}

// =======================================================================
// 내부 헬퍼 메서드들 (DeviceRepository 패턴)
// =======================================================================

bool TenantRepository::validateTenant(const TenantEntity& tenant) const {
    // 이름 검사
    if (tenant.getName().empty() || tenant.getName().length() > 100) {
        return false;
    }
    
    // 도메인 검사
    // ❌ if (tenant.getDomain().empty() || !TenantEntity::isValidDomain(tenant.getDomain())) {
    // ✅ 간단한 도메인 검사
    if (tenant.getDomain().empty() || tenant.getDomain().length() < 3) {
        return false;
    }
    
    // 제한값 검사
    if (tenant.getMaxUsers() <= 0 || tenant.getMaxDevices() <= 0 || tenant.getMaxDataPoints() <= 0) {
        return false;
    }
    
    return true;
}

QueryCondition TenantRepository::buildStatusCondition(TenantEntity::Status status) const {
    std::string status_str;
    switch (status) {
        case TenantEntity::Status::ACTIVE: status_str = "ACTIVE"; break;
        case TenantEntity::Status::SUSPENDED: status_str = "SUSPENDED"; break;
        case TenantEntity::Status::TRIAL: status_str = "TRIAL"; break;
        case TenantEntity::Status::EXPIRED: status_str = "EXPIRED"; break;
        case TenantEntity::Status::DISABLED: status_str = "DISABLED"; break;  // ✅ INACTIVE → DISABLED
        default: status_str = "TRIAL"; break;
    }
    return QueryCondition("status", "=", status_str);
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne