// =============================================================================
// collector/src/VirtualPoint/ScriptLibraryManager.cpp
// PulseOne 스크립트 라이브러리 매니저 구현 - VirtualPointTypes 사용
// =============================================================================

/**
 * @file ScriptLibraryManager.cpp
 * @brief PulseOne ScriptLibraryManager 구현 - 컴파일 에러 완전 해결
 * @author PulseOne Development Team
 * @date 2025-08-11
 * 
 * 🎯 컴파일 에러 완전 해결:
 * - VirtualPointTypes.h 사용
 * - shared_mutex 헤더 추가
 * - isSystem() 메서드 사용 (getIsSystem() 아님)
 * - 실제 정의된 enum 값들만 사용
 */

#include "VirtualPoint/ScriptLibraryManager.h"
#include "VirtualPoint/VirtualPointTypes.h"
#include "Database/RepositoryFactory.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Database/DatabaseManager.h"
#include <regex>
#include <sstream>
#include <algorithm>
#include <shared_mutex>  // 🔥 누락된 헤더 추가

namespace PulseOne {
namespace VirtualPoint {

// =============================================================================
// ScriptDefinition 변환 메서드들 - VirtualPointTypes.h 함수 사용
// =============================================================================

std::string categoryToString(Database::Entities::ScriptLibraryEntity::Category category) {
    // VirtualPointTypes.h의 ScriptCategory로 변환해서 사용
    ScriptCategory vp_category;
    switch (category) {
        case Database::Entities::ScriptLibraryEntity::Category::FUNCTION:
            vp_category = ScriptCategory::FUNCTION;
            break;
        case Database::Entities::ScriptLibraryEntity::Category::FORMULA:
            vp_category = ScriptCategory::FORMULA;
            break;
        case Database::Entities::ScriptLibraryEntity::Category::TEMPLATE:
            vp_category = ScriptCategory::TEMPLATE;
            break;
        case Database::Entities::ScriptLibraryEntity::Category::CUSTOM:
            vp_category = ScriptCategory::CUSTOM;
            break;
        default:
            vp_category = ScriptCategory::CUSTOM;
            break;
    }
    return scriptCategoryToString(vp_category);
}

Database::Entities::ScriptLibraryEntity::Category stringToCategory(const std::string& str) {
    // VirtualPointTypes.h 함수 사용해서 변환
    ScriptCategory vp_category = stringToScriptCategory(str);
    
    switch (vp_category) {
        case ScriptCategory::FUNCTION:
            return Database::Entities::ScriptLibraryEntity::Category::FUNCTION;
        case ScriptCategory::FORMULA:
            return Database::Entities::ScriptLibraryEntity::Category::FORMULA;
        case ScriptCategory::TEMPLATE:
            return Database::Entities::ScriptLibraryEntity::Category::TEMPLATE;
        case ScriptCategory::CUSTOM:
            return Database::Entities::ScriptLibraryEntity::Category::CUSTOM;
        default:
            return Database::Entities::ScriptLibraryEntity::Category::CUSTOM;
    }
}

std::string returnTypeToString(Database::Entities::ScriptLibraryEntity::ReturnType type) {
    // VirtualPointTypes.h의 ScriptReturnType으로 변환해서 사용
    ScriptReturnType vp_type;
    switch (type) {
        case Database::Entities::ScriptLibraryEntity::ReturnType::FLOAT:
            vp_type = ScriptReturnType::FLOAT;
            break;
        case Database::Entities::ScriptLibraryEntity::ReturnType::STRING:
            vp_type = ScriptReturnType::STRING;
            break;
        case Database::Entities::ScriptLibraryEntity::ReturnType::BOOLEAN:
            vp_type = ScriptReturnType::BOOLEAN;
            break;
        case Database::Entities::ScriptLibraryEntity::ReturnType::OBJECT:
            vp_type = ScriptReturnType::OBJECT;
            break;
        default:
            vp_type = ScriptReturnType::FLOAT;
            break;
    }
    return scriptReturnTypeToString(vp_type);
}

Database::Entities::ScriptLibraryEntity::ReturnType stringToReturnType(const std::string& str) {
    // VirtualPointTypes.h 함수 사용해서 변환
    ScriptReturnType vp_type = stringToScriptReturnType(str);
    
    switch (vp_type) {
        case ScriptReturnType::FLOAT:
            return Database::Entities::ScriptLibraryEntity::ReturnType::FLOAT;
        case ScriptReturnType::STRING:
            return Database::Entities::ScriptLibraryEntity::ReturnType::STRING;
        case ScriptReturnType::BOOLEAN:
            return Database::Entities::ScriptLibraryEntity::ReturnType::BOOLEAN;
        case ScriptReturnType::OBJECT:
            return Database::Entities::ScriptLibraryEntity::ReturnType::OBJECT;
        default:
            return Database::Entities::ScriptLibraryEntity::ReturnType::FLOAT;
    }
}

ScriptDefinition ScriptDefinition::fromEntity(const ScriptLibraryEntity& entity) {
    ScriptDefinition def;
    def.id = entity.getId();
    def.category = categoryToString(entity.getCategory());
    def.name = entity.getName();
    def.display_name = entity.getDisplayName();
    def.description = entity.getDescription();
    def.script_code = entity.getScriptCode();
    
    // Parameters JSON 파싱
    try {
        def.parameters = entity.getParameters();
    } catch (const std::exception& e) {
        def.parameters = nlohmann::json::object();
    }
    
    def.return_type = returnTypeToString(entity.getReturnType());
    def.tags = entity.getTags();
    def.example_usage = entity.getExampleUsage();
    def.is_system = entity.isSystem();  // 🔥 getIsSystem() → isSystem() 수정
    def.usage_count = entity.getUsageCount();
    def.rating = entity.getRating();
    
    return def;
}

ScriptLibraryEntity ScriptDefinition::toEntity() const {
    ScriptLibraryEntity entity(0, name, script_code);  // tenant_id는 나중에 설정
    
    entity.setCategory(stringToCategory(category));
    entity.setDisplayName(display_name);
    entity.setDescription(description);
    entity.setParameters(parameters);
    entity.setReturnType(stringToReturnType(return_type));
    entity.setTags(tags);
    entity.setExampleUsage(example_usage);
    entity.setIsSystem(is_system);
    entity.setUsageCount(usage_count);
    entity.setRating(rating);
    
    return entity;
}

// =============================================================================
// ScriptLibraryManager 싱글톤 구현
// =============================================================================

ScriptLibraryManager::ScriptLibraryManager() = default;

ScriptLibraryManager::~ScriptLibraryManager() {
    shutdown();
}

ScriptLibraryManager& ScriptLibraryManager::getInstance() {
    static ScriptLibraryManager instance;
    return instance;
}

// =============================================================================
// 초기화 및 종료
// =============================================================================

bool ScriptLibraryManager::initialize(std::shared_ptr<::DatabaseManager> db_manager) {
    if (initialized_.load()) {
        return true;
    }
    
    try {
        // Repository 초기화
        if (db_manager) {
            // 외부에서 제공된 DatabaseManager 사용
            auto& repo_factory = Database::RepositoryFactory::getInstance();
            repository_ = repo_factory.getScriptLibraryRepository();
        } else {
            // 기본 DatabaseManager 사용
            auto& repo_factory = Database::RepositoryFactory::getInstance();
            repository_ = repo_factory.getScriptLibraryRepository();
        }
        
        if (!repository_) {
            auto& logger = LogManager::getInstance(); {
                logger.Error("ScriptLibraryManager::initialize - Failed to get ScriptLibraryRepository");
            }
            return false;
        }
        
        // 시스템 스크립트 로드
        if (!loadSystemScripts()) {
            auto& logger = LogManager::getInstance(); {
                logger.Warn("ScriptLibraryManager::initialize - Failed to load system scripts");
            }
            // 경고만 하고 계속 진행
        }
        
        initialized_.store(true);
        
        auto& logger = LogManager::getInstance(); {
            logger.Info("ScriptLibraryManager initialized successfully");
        }
        
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance(); {
            logger.Error("ScriptLibraryManager::initialize failed: " + std::string(e.what()));
        }
        return false;
    }
}

void ScriptLibraryManager::shutdown() {
    if (!initialized_.load()) {
        return;
    }
    
    try {
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        
        // 캐시 정리
        script_cache_.clear();
        script_cache_by_id_.clear();
        template_cache_.clear();
        system_script_names_.clear();
        
        // Repository 해제
        repository_.reset();
        
        initialized_.store(false);
        
        auto& logger = LogManager::getInstance(); {
            logger.Info("ScriptLibraryManager shutdown completed");
        }
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance(); {
            logger.Error("ScriptLibraryManager::shutdown failed: " + std::string(e.what()));
        }
    }
}

// =============================================================================
// 스크립트 로드
// =============================================================================

bool ScriptLibraryManager::loadScripts(int tenant_id) {
    if (!initialized_.load() || !repository_) {
        return false;
    }
    
    try {
        current_tenant_id_ = tenant_id;
        
        // Repository에서 스크립트 로드
        auto entities = repository_->findByTenantId(tenant_id);
        
        {
            std::unique_lock<std::shared_mutex> lock(cache_mutex_);
            
            for (const auto& entity : entities) {
                try {
                    updateCacheFromEntity(entity);
                } catch (const std::exception& e) {
                    auto& logger = LogManager::getInstance(); {
                        logger.Warn("ScriptLibraryManager::loadScripts - Failed to cache script '" + 
                                   entity.getName() + "': " + std::string(e.what()));
                    }
                }
            }
        }
        
        auto& logger = LogManager::getInstance(); {
            logger.Info("ScriptLibraryManager::loadScripts - Loaded " + std::to_string(entities.size()) + 
                        " scripts for tenant " + std::to_string(tenant_id));
        }
        
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance(); {
            logger.Error("ScriptLibraryManager::loadScripts failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool ScriptLibraryManager::loadSystemScripts() {
    if (!initialized_.load() || !repository_) {
        return false;
    }
    
    try {
        // 시스템 스크립트 로드
        auto entities = repository_->findSystemScripts();
        
        {
            std::unique_lock<std::shared_mutex> lock(cache_mutex_);
            
            system_script_names_.clear();
            
            for (const auto& entity : entities) {
                try {
                    updateCacheFromEntity(entity);
                    system_script_names_.push_back(entity.getName());
                } catch (const std::exception& e) {
                    auto& logger = LogManager::getInstance(); {
                        logger.Warn("ScriptLibraryManager::loadSystemScripts - Failed to cache system script '" + 
                                   entity.getName() + "': " + std::string(e.what()));
                    }
                }
            }
        }
        
        auto& logger = LogManager::getInstance(); {
            logger.Info("ScriptLibraryManager::loadSystemScripts - Loaded " + std::to_string(entities.size()) + " system scripts");
        }
        
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance(); {
            logger.Error("ScriptLibraryManager::loadSystemScripts failed: " + std::string(e.what()));
        }
        return false;
    }
}

// =============================================================================
// 스크립트 조회
// =============================================================================

std::optional<ScriptDefinition> ScriptLibraryManager::getScript(const std::string& name, int tenant_id) {
    if (!initialized_.load()) {
        return std::nullopt;
    }
    
    try {
        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        
        // 캐시에서 조회
        auto it = script_cache_.find(name);
        if (it != script_cache_.end()) {
            return it->second;
        }
        
        lock.unlock();
        
        // 캐시에 없으면 DB에서 조회
        if (repository_) {
            auto entity = repository_->findByName(tenant_id, name);
            if (entity.has_value()) {
                auto def = ScriptDefinition::fromEntity(entity.value());
                
                // 캐시 업데이트
                std::unique_lock<std::shared_mutex> write_lock(cache_mutex_);
                script_cache_[name] = def;
                script_cache_by_id_[def.id] = def;
                
                return def;
            }
        }
        
        return std::nullopt;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance(); {
            logger.Error("ScriptLibraryManager::getScript failed: " + std::string(e.what()));
        }
        return std::nullopt;
    }
}

std::optional<ScriptDefinition> ScriptLibraryManager::getScriptById(int script_id) {
    if (!initialized_.load()) {
        return std::nullopt;
    }
    
    try {
        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        
        // 캐시에서 조회
        auto it = script_cache_by_id_.find(script_id);
        if (it != script_cache_by_id_.end()) {
            return it->second;
        }
        
        lock.unlock();
        
        // 캐시에 없으면 DB에서 조회
        if (repository_) {
            auto entity = repository_->findById(script_id);
            if (entity.has_value()) {
                auto def = ScriptDefinition::fromEntity(entity.value());
                
                // 캐시 업데이트
                std::unique_lock<std::shared_mutex> write_lock(cache_mutex_);
                script_cache_[def.name] = def;
                script_cache_by_id_[script_id] = def;
                
                return def;
            }
        }
        
        return std::nullopt;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance(); {
            logger.Error("ScriptLibraryManager::getScriptById failed: " + std::string(e.what()));
        }
        return std::nullopt;
    }
}

// =============================================================================
// 스크립트 등록/수정/삭제 (기본 구현)
// =============================================================================

int ScriptLibraryManager::registerScript(const ScriptDefinition& script) {
    if (!initialized_.load() || !repository_) {
        return -1;
    }
    
    try {
        auto entity = script.toEntity();
        entity.setTenantId(current_tenant_id_);
        
        if (repository_->save(entity)) {
            updateCache(script);
            return entity.getId();
        }
        
        return -1;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance(); {
            logger.Error("ScriptLibraryManager::registerScript failed: " + std::string(e.what()));
        }
        return -1;
    }
}

bool ScriptLibraryManager::updateScript(int script_id, const ScriptDefinition& script) {
    // 기본 구현 - 실제로는 더 복잡한 로직 필요
    return false;
}

bool ScriptLibraryManager::deleteScript(int script_id) {
    // 기본 구현 - 실제로는 더 복잡한 로직 필요
    return false;
}

// =============================================================================
// 내부 헬퍼 메서드
// =============================================================================

std::shared_ptr<Database::Repositories::ScriptLibraryRepository> ScriptLibraryManager::getRepository() {
    return repository_;
}

void ScriptLibraryManager::updateCache(const ScriptDefinition& script) {
    if (cache_enabled_) {
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        script_cache_[script.name] = script;
        script_cache_by_id_[script.id] = script;
    }
}

void ScriptLibraryManager::updateCacheFromEntity(const ScriptLibraryEntity& entity) {
    if (cache_enabled_) {
        auto def = ScriptDefinition::fromEntity(entity);
        script_cache_[def.name] = def;
        script_cache_by_id_[def.id] = def;
    }
}

// =============================================================================
// 나머지 메서드들은 기본 구현으로 제공 (추후 확장 가능)
// =============================================================================

std::vector<ScriptDefinition> ScriptLibraryManager::getScriptsByCategory(const std::string& category) {
    return {};  // 기본 구현
}

std::vector<ScriptDefinition> ScriptLibraryManager::getScriptsByTags(const std::vector<std::string>& tags) {
    return {};  // 기본 구현
}

std::vector<ScriptDefinition> ScriptLibraryManager::getAllScripts(int tenant_id) {
    return {};  // 기본 구현
}

bool ScriptLibraryManager::validateScript(const std::string& script_code) {
    return !script_code.empty();  // 기본 구현
}

std::string ScriptLibraryManager::testScript(const std::string& script_code, const json& test_inputs) {
    return "";  // 기본 구현
}

void ScriptLibraryManager::clearCache() {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    script_cache_.clear();
    script_cache_by_id_.clear();
    template_cache_.clear();
}

std::vector<std::string> ScriptLibraryManager::extractFunctionNames(const std::string& script_code) {
    return {};  // 기본 구현
}

std::string ScriptLibraryManager::replaceVariables(const std::string& template_code, const json& variables) {
    return template_code;  // 기본 구현
}

std::vector<std::string> ScriptLibraryManager::collectDependencies(const std::string& formula) {
    std::vector<std::string> dependencies;
    
    try {
        // 기본적인 함수 이름 추출 (정규식 사용)
        std::regex function_pattern(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\()");
        std::sregex_iterator iter(formula.begin(), formula.end(), function_pattern);
        std::sregex_iterator end;
        
        std::set<std::string> unique_functions;
        
        while (iter != end) {
            std::string func_name = (*iter)[1].str();
            
            // JavaScript 내장 함수들은 제외
            if (func_name != "Math" && func_name != "console" && 
                func_name != "parseInt" && func_name != "parseFloat" &&
                func_name != "isNaN" && func_name != "isFinite" &&
                func_name != "Number" && func_name != "String" &&
                func_name != "Boolean" && func_name != "Date") {
                unique_functions.insert(func_name);
            }
            
            ++iter;
        }
        
        // set을 vector로 변환
        dependencies.assign(unique_functions.begin(), unique_functions.end());
        
        auto& logger = LogManager::getInstance();
        logger.log("ScriptLibraryManager", LogLevel::DEBUG, 
                  "collectDependencies - Found " + std::to_string(dependencies.size()) + " dependencies");
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("ScriptLibraryManager", LogLevel::ERROR,
                  "collectDependencies failed: " + std::string(e.what()));
    }
    
    return dependencies;
}

void ScriptLibraryManager::recordUsage(int script_id, int virtual_point_id, const std::string& context) {
    try {
        if (script_id <= 0 || virtual_point_id <= 0) {
            return;
        }
        
        if (!repository_) {
            LogManager::getInstance().log("ScriptLibraryManager", LogLevel::ERROR,
                                        "recordUsage - Repository not available");
            return;
        }
        
        // 사용 횟수 증가
        repository_->incrementUsageCount(script_id);
        
        // 추가적인 사용 이력 기록 (필요시)
        repository_->recordUsage(script_id, virtual_point_id, 0, context);
        
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::DEBUG,
                                    "recordUsage - Recorded usage for script " + 
                                    std::to_string(script_id) + " in VP " + std::to_string(virtual_point_id));
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::ERROR,
                                    "recordUsage failed: " + std::string(e.what()));
    }
}

} // namespace VirtualPoint
} // namespace PulseOne