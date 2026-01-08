#include "VirtualPoint/VirtualPointRegistry.h"
#include "Database/Repositories/VirtualPointRepository.h"
#include "Database/RepositoryFactory.h"
#include "Logging/LogManager.h"
#include <algorithm>

namespace PulseOne {
namespace VirtualPoint {

using json = nlohmann::json;

bool VirtualPointRegistry::loadVirtualPoints(int tenant_id) {
    LogManager::getInstance().log("VirtualPointRegistry", LogLevel::INFO,
                                 "테넌트 " + std::to_string(tenant_id) + "의 가상포인트 로드 중...");
    
    try {
        auto& factory = Database::RepositoryFactory::getInstance();
        auto repo = factory.getVirtualPointRepository();
        auto entities = repo->findByTenant(tenant_id);
        
        std::unique_lock<std::shared_mutex> vp_lock(vp_mutex_);
        std::unique_lock<std::shared_mutex> dep_lock(dep_mutex_);
        
        // 기존 테넌트 데이터 정리
        auto it = virtual_points_.begin();
        while (it != virtual_points_.end()) {
            if (it->second.tenant_id == tenant_id) {
                it = virtual_points_.erase(it);
            } else {
                ++it;
            }
        }
        
        // 새 데이터 로드
        for (const auto& entity : entities) {
            VirtualPointDef vp_def;
            vp_def.id = entity.getId();
            vp_def.tenant_id = entity.getTenantId();
            vp_def.name = entity.getName();
            vp_def.description = entity.getDescription();
            vp_def.formula = entity.getFormula();
            
            vp_def.execution_type = convertEntityExecutionType(entity.getExecutionType());
            vp_def.error_handling = convertEntityErrorHandling(entity.getErrorHandling());
            vp_def.state = entity.getIsEnabled() ? VirtualPointState::ACTIVE : VirtualPointState::INACTIVE;
            
            vp_def.calculation_interval_ms = std::chrono::milliseconds(entity.getCalculationInterval());
            vp_def.is_enabled = entity.getIsEnabled();
            
            if (!entity.getDependencies().empty()) {
                try {
                    vp_def.input_points = json::parse(entity.getDependencies());
                } catch (const std::exception& e) {
                    LogManager::getInstance().log("VirtualPointRegistry", LogLevel::WARN,
                                                 "가상포인트 " + std::to_string(vp_def.id) + 
                                                 " dependencies 파싱 실패: " + std::string(e.what()));
                }
            }
            
            virtual_points_[vp_def.id] = vp_def;
            
            // 의존성 맵 구축
            if (vp_def.input_points.contains("inputs") &&
                vp_def.input_points["inputs"].is_array()) {
                
                for (const auto& input : vp_def.input_points["inputs"]) {
                    if (input.contains("point_id")) {
                        int point_id = 0;
                        if (input["point_id"].is_number()) {
                            point_id = input["point_id"].get<int>();
                        } else if (input["point_id"].is_string()) {
                            try {
                                point_id = std::stoi(input["point_id"].get<std::string>());
                            } catch (...) { continue; }
                        } else {
                            continue;
                        }
                        
                        point_to_vp_map_[point_id].insert(vp_def.id);
                        vp_dependencies_[vp_def.id].insert(point_id);
                    }
                }
            }
        }
        
        LogManager::getInstance().log("VirtualPointRegistry", LogLevel::INFO,
                                     "가상포인트 " + std::to_string(entities.size()) + " 개 로드 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointRegistry", LogLevel::LOG_ERROR,
                                     "가상포인트 로드 실패: " + std::string(e.what()));
        return false;
    }
}

std::optional<VirtualPointDef> VirtualPointRegistry::getVirtualPoint(int vp_id) const {
    std::shared_lock<std::shared_mutex> lock(vp_mutex_);
    auto it = virtual_points_.find(vp_id);
    if (it != virtual_points_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool VirtualPointRegistry::reloadVirtualPoint(int vp_id) {
    // Single point reload logic (similar to loadVirtualPoints but for one ID)
    try {
        auto& factory = Database::RepositoryFactory::getInstance();
        auto repo = factory.getVirtualPointRepository();
        auto entity_opt = repo->findById(vp_id);
        
        if (!entity_opt) return false;
        
        const auto& entity = *entity_opt;
        
        std::unique_lock<std::shared_mutex> vp_lock(vp_mutex_);
        std::unique_lock<std::shared_mutex> dep_lock(dep_mutex_);
        
        // Remove old dependencies
        auto dep_it = vp_dependencies_.find(vp_id);
        if (dep_it != vp_dependencies_.end()) {
            for (int point_id : dep_it->second) {
                point_to_vp_map_[point_id].erase(vp_id);
            }
            vp_dependencies_.erase(dep_it);
        }
        
        VirtualPointDef vp_def;
        vp_def.id = entity.getId();
        vp_def.tenant_id = entity.getTenantId();
        vp_def.name = entity.getName();
        vp_def.description = entity.getDescription();
        vp_def.formula = entity.getFormula();
        vp_def.execution_type = convertEntityExecutionType(entity.getExecutionType());
        vp_def.error_handling = convertEntityErrorHandling(entity.getErrorHandling());
        vp_def.state = entity.getIsEnabled() ? VirtualPointState::ACTIVE : VirtualPointState::INACTIVE;
        vp_def.calculation_interval_ms = std::chrono::milliseconds(entity.getCalculationInterval());
        vp_def.is_enabled = entity.getIsEnabled();
        
        if (!entity.getDependencies().empty()) {
            try {
                vp_def.input_points = json::parse(entity.getDependencies());
            } catch (...) {}
        }
        
        virtual_points_[vp_def.id] = vp_def;
        
        // Add new dependencies
        if (vp_def.input_points.contains("inputs") && vp_def.input_points["inputs"].is_array()) {
            for (const auto& input : vp_def.input_points["inputs"]) {
                if (input.contains("point_id")) {
                    int pid = input["point_id"].get<int>();
                    point_to_vp_map_[pid].insert(vp_id);
                    vp_dependencies_[vp_id].insert(pid);
                }
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

void VirtualPointRegistry::clear() {
    std::unique_lock<std::shared_mutex> vp_lock(vp_mutex_);
    std::unique_lock<std::shared_mutex> dep_lock(dep_mutex_);
    virtual_points_.clear();
    point_to_vp_map_.clear();
    vp_dependencies_.clear();
}

std::vector<int> VirtualPointRegistry::getAffectedVirtualPoints(const PulseOne::Structs::DeviceDataMessage& msg) const {
    std::vector<int> affected_vps;
    std::shared_lock<std::shared_mutex> lock(dep_mutex_);
    
    for (const auto& data_point : msg.points) {
        auto it = point_to_vp_map_.find(data_point.point_id); 
        if (it != point_to_vp_map_.end()) {
            for (int vp_id : it->second) {
                if (std::find(affected_vps.begin(), affected_vps.end(), vp_id) == affected_vps.end()) {
                    affected_vps.push_back(vp_id);
                }
            }
        }
    }
    return affected_vps;
}

std::vector<int> VirtualPointRegistry::getDependentVirtualPoints(int point_id) const {
    std::shared_lock<std::shared_mutex> lock(dep_mutex_);
    auto it = point_to_vp_map_.find(point_id);
    if (it != point_to_vp_map_.end()) {
        return std::vector<int>(it->second.begin(), it->second.end());
    }
    return {};
}

bool VirtualPointRegistry::hasDependency(int vp_id, int point_id) const {
    std::shared_lock<std::shared_mutex> lock(dep_mutex_);
    auto it = vp_dependencies_.find(vp_id);
    if (it != vp_dependencies_.end()) {
        return it->second.find(point_id) != it->second.end();
    }
    return false;
}

ExecutionType VirtualPointRegistry::convertEntityExecutionType(const std::string& entity_type_str) {
    return stringToExecutionType(entity_type_str);
}

ErrorHandling VirtualPointRegistry::convertEntityErrorHandling(const std::string& entity_handling_str) {
    return stringToErrorHandling(entity_handling_str);
}

} // namespace VirtualPoint
} // namespace PulseOne
