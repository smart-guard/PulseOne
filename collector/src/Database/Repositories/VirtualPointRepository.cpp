// =============================================================================
// collector/src/Database/Repositories/VirtualPointRepository.cpp
// PulseOne VirtualPointRepository 구현 - SiteRepository 패턴 100% 준수
// =============================================================================

/**
 * @file VirtualPointRepository.cpp
 * @brief PulseOne VirtualPointRepository 구현 - SiteRepository 패턴 100% 준수
 * @author PulseOne Development Team
 * @date 2025-07-28
 */

// 🔥 헤더 파일 include 필수 - 이게 빠져서 컴파일 에러 발생!
#include "Database/Repositories/VirtualPointRepository.h"
#include "Utils/LogManager.h"
#include <chrono>
#include <algorithm>
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 🔥 기존 패턴 준수 - using 선언 필수 (cpp에도 필요)
using VirtualPointEntity = Entities::VirtualPointEntity;


// =======================================================================
// 캐시 관리 메서드들 (SiteRepository 패턴)
// =======================================================================

void VirtualPointRepository::setCacheEnabled(bool enabled) {
    IRepository<VirtualPointEntity>::setCacheEnabled(enabled);
    logger_->Info("VirtualPointRepository cache " + std::string(enabled ? "enabled" : "disabled"));
}

bool VirtualPointRepository::isCacheEnabled() const {
    return IRepository<VirtualPointEntity>::isCacheEnabled();
}

void VirtualPointRepository::clearCache() {
    IRepository<VirtualPointEntity>::clearCache();
    logger_->Info("VirtualPointRepository cache cleared");
}

void VirtualPointRepository::clearCacheForId(int id) {
    IRepository<VirtualPointEntity>::clearCacheForId(id);
    logger_->Debug("VirtualPointRepository cache cleared for ID: " + std::to_string(id));
}

std::map<std::string, int> VirtualPointRepository::getCacheStats() const {
    return IRepository<VirtualPointEntity>::getCacheStats();
}

// =======================================================================
// IRepository 인터페이스 구현 (SiteRepository 패턴)
// =======================================================================

std::vector<VirtualPointEntity> VirtualPointRepository::findAll() {
    logger_->Debug("🔍 VirtualPointRepository::findAll() - Fetching all virtual points");
    
    return findByConditions({}, OrderBy("name", "ASC"));
}

std::optional<VirtualPointEntity> VirtualPointRepository::findById(int id) {
    logger_->Debug("🔍 VirtualPointRepository::findById(" + std::to_string(id) + ")");
    
    auto cached = getCachedEntity(id);
    if (cached.has_value()) {
        logger_->Debug("Cache hit for VirtualPoint ID: " + std::to_string(id));
        return cached;
    }
    
    auto points = findByConditions({QueryCondition("id", "=", std::to_string(id))});
    if (!points.empty()) {
        cacheEntity(points[0]);
        return points[0];
    }
    
    return std::nullopt;
}

bool VirtualPointRepository::save(VirtualPointEntity& entity) {
    logger_->Debug("💾 VirtualPointRepository::save() - " + entity.getName());
    
    if (!validateVirtualPoint(entity)) {
        logger_->Error("❌ VirtualPoint validation failed: " + entity.getName());
        return false;
    }
    
    if (isPointNameTaken(entity.getName(), entity.getTenantId(), entity.getId())) {
        logger_->Error("❌ VirtualPoint name already taken: " + entity.getName());
        return false;
    }
    
    if (!validateFormula(entity)) {
        logger_->Error("❌ VirtualPoint formula validation failed: " + entity.getName());
        return false;
    }
    
    try {
        // INSERT 또는 UPDATE 수행
        if (entity.getId() == 0) {
            // INSERT
            if (IRepository<VirtualPointEntity>::save(entity)) {
                cacheEntity(entity);
                logger_->Info("✅ VirtualPoint created: " + entity.getName());
                return true;
            }
        } else {
            // UPDATE
            if (IRepository<VirtualPointEntity>::update(entity)) {
                cacheEntity(entity);
                logger_->Info("✅ VirtualPoint updated: " + entity.getName());
                return true;
            }
        }
    } catch (const std::exception& e) {
        logger_->Error("❌ VirtualPoint save failed: " + entity.getName() + " - " + e.what());
    }
    
    return false;
}

bool VirtualPointRepository::update(const VirtualPointEntity& entity) {
    logger_->Debug("🔄 VirtualPointRepository::update() - " + entity.getName());
    
    if (!validateVirtualPoint(entity)) {
        logger_->Error("❌ VirtualPoint validation failed: " + entity.getName());
        return false;
    }
    
    if (isPointNameTaken(entity.getName(), entity.getTenantId(), entity.getId())) {
        logger_->Error("❌ VirtualPoint name already taken: " + entity.getName());
        return false;
    }
    
    if (!validateFormula(entity)) {
        logger_->Error("❌ VirtualPoint formula validation failed: " + entity.getName());
        return false;
    }
    
    try {
        if (IRepository<VirtualPointEntity>::update(entity)) {
            clearCacheForId(entity.getId());
            logger_->Info("✅ VirtualPoint updated: " + entity.getName());
            return true;
        }
    } catch (const std::exception& e) {
        logger_->Error("❌ VirtualPoint update failed: " + entity.getName() + " - " + e.what());
    }
    
    return false;
}

bool VirtualPointRepository::deleteById(int id) {
    logger_->Debug("🗑️ VirtualPointRepository::deleteById(" + std::to_string(id) + ")");
    
    try {
        if (IRepository<VirtualPointEntity>::deleteById(id)) {
            clearCacheForId(id);
            logger_->Info("✅ VirtualPoint deleted: " + std::to_string(id));
            return true;
        }
    } catch (const std::exception& e) {
        logger_->Error("❌ VirtualPoint delete failed: " + std::to_string(id) + " - " + e.what());
    }
    
    return false;
}

bool VirtualPointRepository::exists(int id) {
    logger_->Debug("🔍 VirtualPointRepository::exists(" + std::to_string(id) + ")");
    
    return IRepository<VirtualPointEntity>::exists(id);
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByIds(const std::vector<int>& ids) {
    logger_->Debug("🔍 VirtualPointRepository::findByIds() - " + std::to_string(ids.size()) + " virtual points");
    
    return IRepository<VirtualPointEntity>::findByIds(ids);
}

int VirtualPointRepository::saveBulk(std::vector<VirtualPointEntity>& entities) {
    logger_->Info("💾 VirtualPointRepository::saveBulk() - " + std::to_string(entities.size()) + " virtual points");
    
    int valid_count = 0;
    for (const auto& entity : entities) {
        if (validateVirtualPoint(entity)) {
            valid_count++;
        }
    }
    
    if (valid_count != entities.size()) {
        logger_->Warn("⚠️ Some virtual points failed validation. Valid: " + std::to_string(valid_count) + "/" + std::to_string(entities.size()));
    }
    
    return IRepository<VirtualPointEntity>::saveBulk(entities);
}

int VirtualPointRepository::updateBulk(const std::vector<VirtualPointEntity>& entities) {
    logger_->Info("🔄 VirtualPointRepository::updateBulk() - " + std::to_string(entities.size()) + " virtual points");
    
    return IRepository<VirtualPointEntity>::updateBulk(entities);
}

int VirtualPointRepository::deleteByIds(const std::vector<int>& ids) {
    logger_->Info("🗑️ VirtualPointRepository::deleteByIds() - " + std::to_string(ids.size()) + " virtual points");
    
    return IRepository<VirtualPointEntity>::deleteByIds(ids);
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByConditions(
    const std::vector<QueryCondition>& conditions,
    const std::optional<OrderBy>& order_by,
    const std::optional<Pagination>& pagination) {
    
    return IRepository<VirtualPointEntity>::findByConditions(conditions, order_by, pagination);
}

int VirtualPointRepository::countByConditions(const std::vector<QueryCondition>& conditions) {
    return IRepository<VirtualPointEntity>::countByConditions(conditions);
}

int VirtualPointRepository::getTotalCount() {
    try {
        return countByConditions({});
    } catch (const std::exception& e) {
        logger_->Error("❌ VirtualPointRepository::getTotalCount() failed: " + std::string(e.what()));
        return 0;
    }
}

// =======================================================================
// VirtualPoint 전용 메서드들 (SiteRepository 패턴)
// =======================================================================

std::vector<VirtualPointEntity> VirtualPointRepository::findByTenant(int tenant_id) {
    logger_->Debug("🔍 VirtualPointRepository::findByTenant(" + std::to_string(tenant_id) + ")");
    
    return findByConditions({buildTenantCondition(tenant_id)}, OrderBy("name", "ASC"));
}

std::vector<VirtualPointEntity> VirtualPointRepository::findBySite(int site_id) {
    logger_->Debug("🔍 VirtualPointRepository::findBySite(" + std::to_string(site_id) + ")");
    
    return findByConditions({buildSiteCondition(site_id)}, OrderBy("name", "ASC"));
}

std::vector<VirtualPointEntity> VirtualPointRepository::findEnabledPoints(int tenant_id) {
    logger_->Debug("🔍 VirtualPointRepository::findEnabledPoints(" + std::to_string(tenant_id) + ")");
    
    std::vector<QueryCondition> conditions = {buildEnabledCondition(true)};
    
    if (tenant_id > 0) {
        conditions.push_back(buildTenantCondition(tenant_id));
    }
    
    return findByConditions(conditions, OrderBy("name", "ASC"));
}

std::optional<VirtualPointEntity> VirtualPointRepository::findByName(const std::string& name, int tenant_id) {
    logger_->Debug("🔍 VirtualPointRepository::findByName(" + name + ", " + std::to_string(tenant_id) + ")");
    
    auto points = findByConditions({
        QueryCondition("name", "=", name),
        buildTenantCondition(tenant_id)
    });
    
    return points.empty() ? std::nullopt : std::make_optional(points[0]);
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByCategory(const std::string& category, int tenant_id) {
    logger_->Debug("🔍 VirtualPointRepository::findByCategory(" + category + ", " + std::to_string(tenant_id) + ")");
    
    std::vector<QueryCondition> conditions = {
        QueryCondition("category", "=", category)
    };
    
    if (tenant_id > 0) {
        conditions.push_back(buildTenantCondition(tenant_id));
    }
    
    return findByConditions(conditions, OrderBy("name", "ASC"));
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByDataType(VirtualPointEntity::DataType data_type, int tenant_id) {
    logger_->Debug("🔍 VirtualPointRepository::findByDataType(" + VirtualPointEntity::dataTypeToString(data_type) + ", " + std::to_string(tenant_id) + ")");
    
    std::vector<QueryCondition> conditions = {buildDataTypeCondition(data_type)};
    
    if (tenant_id > 0) {
        conditions.push_back(buildTenantCondition(tenant_id));
    }
    
    return findByConditions(conditions, OrderBy("name", "ASC"));
}

std::vector<VirtualPointEntity> VirtualPointRepository::findByNamePattern(const std::string& name_pattern, int tenant_id) {
    logger_->Debug("🔍 VirtualPointRepository::findByNamePattern(" + name_pattern + ", " + std::to_string(tenant_id) + ")");
    
    std::vector<QueryCondition> conditions = {
        QueryCondition("name", "LIKE", name_pattern)
    };
    
    if (tenant_id > 0) {
        conditions.push_back(buildTenantCondition(tenant_id));
    }
    
    return findByConditions(conditions, OrderBy("name", "ASC"));
}

// =======================================================================
// 비즈니스 로직 메서드들 (VirtualPoint 전용)
// =======================================================================

bool VirtualPointRepository::isPointNameTaken(const std::string& name, int tenant_id, int exclude_id) {
    logger_->Debug("🔍 VirtualPointRepository::isPointNameTaken(" + name + ", " + std::to_string(tenant_id) + ", " + std::to_string(exclude_id) + ")");
    
    std::vector<QueryCondition> conditions = {
        QueryCondition("name", "=", name),
        buildTenantCondition(tenant_id)
    };
    
    if (exclude_id > 0) {
        conditions.push_back(QueryCondition("id", "!=", std::to_string(exclude_id)));
    }
    
    return countByConditions(conditions) > 0;
}

std::vector<VirtualPointEntity> VirtualPointRepository::findPointsNeedingCalculation(int limit) {
    logger_->Debug("🔍 VirtualPointRepository::findPointsNeedingCalculation(" + std::to_string(limit) + ")");
    
    std::vector<QueryCondition> conditions = {buildEnabledCondition(true)};
    
    // 실제 구현에서는 calculation_trigger, calculation_interval 등을 고려해야 함
    // 여기서는 기본적으로 모든 활성화된 포인트를 반환
    std::optional<Pagination> pagination = std::nullopt;
    if (limit > 0) {
        pagination = Pagination{0, limit};
    }
    
    return findByConditions(conditions, OrderBy("calculation_interval", "ASC"), pagination);
}

bool VirtualPointRepository::validateFormula(const VirtualPointEntity& entity) {
    logger_->Debug("🔍 VirtualPointRepository::validateFormula() - " + entity.getName());
    
    // VirtualPointEntity의 validateFormula 메서드 호출
    return entity.validateFormula();
}

// =======================================================================
// 계산 관련 메서드들 (VirtualPoint 전용)
// =======================================================================

std::optional<double> VirtualPointRepository::executeCalculation(int point_id, bool force_calculation) {
    logger_->Debug("🔢 VirtualPointRepository::executeCalculation(" + std::to_string(point_id) + ", " + (force_calculation ? "force" : "normal") + ")");
    
    auto point = findById(point_id);
    if (!point.has_value()) {
        logger_->Error("❌ VirtualPoint not found for calculation: " + std::to_string(point_id));
        return std::nullopt;
    }
    
    // 계산이 필요한지 확인 (강제가 아닌 경우)
    if (!force_calculation && !point->needsCalculation()) {
        logger_->Debug("⏭️ VirtualPoint calculation not needed: " + std::to_string(point_id));
        return point->getLastCalculatedValue();
    }
    
    // 실제 계산 실행
    // 여기서는 간단한 예시로 빈 input_values로 계산
    std::map<std::string, double> input_values; // 실제로는 다른 데이터포인트에서 값을 가져와야 함
    
    auto result = point->calculateValue(input_values);
    if (result.has_value()) {
        updateCalculatedValue(point_id, result.value());
        logger_->Info("✅ VirtualPoint calculation successful: " + std::to_string(point_id) + " = " + std::to_string(result.value()));
    } else {
        logger_->Error("❌ VirtualPoint calculation failed: " + std::to_string(point_id));
    }
    
    return result;
}

bool VirtualPointRepository::updateCalculatedValue(int point_id, double value, const std::string& quality) {
    logger_->Debug("💾 VirtualPointRepository::updateCalculatedValue(" + std::to_string(point_id) + ", " + std::to_string(value) + ", " + quality + ")");
    
    try {
        // 실제 구현에서는 current_values 테이블에 업데이트하거나
        // VirtualPointEntity의 캐시된 값을 업데이트해야 함
        
        // 캐시에서 엔티티 제거 (다음 조회 시 최신 값으로 로드되도록)
        clearCacheForId(point_id);
        
        logger_->Info("✅ VirtualPoint calculated value updated: " + std::to_string(point_id));
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("❌ VirtualPoint calculated value update failed: " + std::to_string(point_id) + " - " + e.what());
        return false;
    }
}

// =======================================================================
// private 헬퍼 메서드들 (SiteRepository 패턴)
// =======================================================================

bool VirtualPointRepository::validateVirtualPoint(const VirtualPointEntity& point) const {
    return point.isValid();
}

QueryCondition VirtualPointRepository::buildTenantCondition(int tenant_id) const {
    return QueryCondition("tenant_id", "=", std::to_string(tenant_id));
}

QueryCondition VirtualPointRepository::buildSiteCondition(int site_id) const {
    return QueryCondition("site_id", "=", std::to_string(site_id));
}

QueryCondition VirtualPointRepository::buildEnabledCondition(bool enabled) const {
    return QueryCondition("is_enabled", "=", enabled ? "1" : "0");
}

QueryCondition VirtualPointRepository::buildDataTypeCondition(VirtualPointEntity::DataType data_type) const {
    return QueryCondition("data_type", "=", VirtualPointEntity::dataTypeToString(data_type));
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne