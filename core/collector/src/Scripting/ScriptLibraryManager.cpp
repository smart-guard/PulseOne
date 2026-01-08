#include "Scripting/ScriptLibraryManager.h"
#include "Database/RepositoryFactory.h"
#include "Logging/LogManager.h"
#include <regex>
#include <sstream>
#include <algorithm>
#include <set>

namespace PulseOne {
namespace Scripting {

// =============================================================================
// ✅ ScriptDefinition 변환 메서드들
// =============================================================================

ScriptDefinition ScriptDefinition::fromEntity(const ScriptLibraryEntity& entity) {
    ScriptDefinition def;
    def.id = entity.getId();
    def.name = entity.getName();
    def.script_code = entity.getScriptCode();
    
    auto category_enum = entity.getCategory();
    switch(category_enum) {
        case ScriptLibraryEntity::Category::FUNCTION: def.category = "FUNCTION"; break;
        case ScriptLibraryEntity::Category::FORMULA: def.category = "FORMULA"; break;
        case ScriptLibraryEntity::Category::TEMPLATE: def.category = "TEMPLATE"; break;
        default: def.category = "CUSTOM";
    }
    
    def.display_name = entity.getDisplayName();
    def.description = entity.getDescription();
    def.parameters = entity.getParameters();
    
    auto return_type_enum = entity.getReturnType();
    switch(return_type_enum) {
        case ScriptLibraryEntity::ReturnType::FLOAT: def.return_type = "FLOAT"; break;
        case ScriptLibraryEntity::ReturnType::STRING: def.return_type = "STRING"; break;
        case ScriptLibraryEntity::ReturnType::BOOLEAN: def.return_type = "BOOLEAN"; break;
        default: def.return_type = "FLOAT";
    }
    
    def.tags = entity.getTags();
    def.example_usage = entity.getExampleUsage();
    def.is_system = entity.isSystem();
    def.usage_count = entity.getUsageCount();
    def.rating = entity.getRating();
    def.version = entity.getVersion();
    def.author = entity.getAuthor();
    
    return def;
}

ScriptLibraryEntity ScriptDefinition::toEntity() const {
    ScriptLibraryEntity entity(0, name, script_code);
    
    ScriptLibraryEntity::Category category_enum;
    if (category == "FUNCTION") category_enum = ScriptLibraryEntity::Category::FUNCTION;
    else if (category == "FORMULA") category_enum = ScriptLibraryEntity::Category::FORMULA;
    else category_enum = ScriptLibraryEntity::Category::CUSTOM;
    entity.setCategory(category_enum);
    
    entity.setDisplayName(display_name);
    entity.setDescription(description);
    entity.setParameters(parameters);
    
    ScriptLibraryEntity::ReturnType return_type_enum;
    if (return_type == "FLOAT") return_type_enum = ScriptLibraryEntity::ReturnType::FLOAT;
    else if (return_type == "STRING") return_type_enum = ScriptLibraryEntity::ReturnType::STRING;
    else if (return_type == "BOOLEAN") return_type_enum = ScriptLibraryEntity::ReturnType::BOOLEAN;
    else return_type_enum = ScriptLibraryEntity::ReturnType::FLOAT;
    entity.setReturnType(return_type_enum);
    
    entity.setTags(tags);
    entity.setExampleUsage(example_usage);
    entity.setIsSystem(is_system);
    entity.setUsageCount(usage_count);
    entity.setRating(rating);
    entity.setVersion(version);
    entity.setAuthor(author);
    
    return entity;
}

// =============================================================================
// ✅ ScriptLibraryManager 싱글톤 구현
// =============================================================================

ScriptLibraryManager::ScriptLibraryManager() = default;

ScriptLibraryManager::~ScriptLibraryManager() {
    shutdown();
}

ScriptLibraryManager& ScriptLibraryManager::getInstance() {
    static ScriptLibraryManager instance;
    return instance;
}

bool ScriptLibraryManager::initialize(std::shared_ptr<DbLib::DatabaseManager> db_manager) {
    if (initialized_.load()) return true;
    
    try {
        auto& repo_factory = Database::RepositoryFactory::getInstance();
        repository_ = repo_factory.getScriptLibraryRepository();
        
        if (!repository_) return false;
        
        loadSystemScripts();
        initialized_.store(true);
        return true;
    } catch (...) {
        return false;
    }
}

void ScriptLibraryManager::shutdown() {
    if (!initialized_.load()) return;
    clearCache();
    initialized_.store(false);
}

bool ScriptLibraryManager::loadSystemScripts() {
    if (!repository_) return false;
    try {
        auto entities = repository_->findSystemScripts();
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        for (const auto& entity : entities) {
            updateCacheFromEntity(entity);
        }
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<ScriptDefinition> ScriptLibraryManager::getScript(const std::string& name, int tenant_id) {
    if (!initialized_.load()) initialize();
    
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    auto it = script_cache_.find(name);
    if (it != script_cache_.end()) return it->second;
    lock.unlock();
    
    if (repository_) {
        auto entity = repository_->findByName(tenant_id, name);
        if (entity.has_value()) {
            auto def = ScriptDefinition::fromEntity(entity.value());
            std::unique_lock<std::shared_mutex> write_lock(cache_mutex_);
            script_cache_[name] = def;
            script_cache_by_id_[def.id] = def;
            return def;
        }
    }
    return std::nullopt;
}

std::optional<ScriptDefinition> ScriptLibraryManager::getScriptById(int script_id) {
    if (!initialized_.load()) initialize();
    
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    auto it = script_cache_by_id_.find(script_id);
    if (it != script_cache_by_id_.end()) return it->second;
    lock.unlock();
    
    if (repository_) {
        auto entity = repository_->findById(script_id);
        if (entity.has_value()) {
            auto def = ScriptDefinition::fromEntity(entity.value());
            std::unique_lock<std::shared_mutex> write_lock(cache_mutex_);
            script_cache_[def.name] = def;
            script_cache_by_id_[script_id] = def;
            return def;
        }
    }
    return std::nullopt;
}

std::vector<ScriptDefinition> ScriptLibraryManager::getAllScripts(int tenant_id) {
    std::vector<ScriptDefinition> scripts;
    if (!initialized_.load()) initialize();
    if (!repository_) return scripts;
    
    auto entities = repository_->findByTenantId(tenant_id);
    for (const auto& entity : entities) {
        scripts.push_back(ScriptDefinition::fromEntity(entity));
    }
    return scripts;
}

std::vector<std::string> ScriptLibraryManager::collectDependencies(const std::string& script) {
    std::vector<std::string> dependencies;
    try {
        std::regex function_pattern(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\()");
        std::sregex_iterator iter(script.begin(), script.end(), function_pattern);
        std::sregex_iterator end;
        
        std::set<std::string> unique_functions;
        std::set<std::string> builtin_functions = {
            "Math", "console", "parseInt", "parseFloat", "isNaN", "isFinite",
            "Number", "String", "Boolean", "Date", "Array", "Object", "JSON",
            "abs", "acos", "asin", "atan", "atan2", "ceil", "cos", "exp", "floor",
            "log", "max", "min", "pow", "random", "round", "sin", "sqrt", "tan",
            "if", "else", "for", "while", "do", "switch", "return", "function", "var", "let", "const"
        };
        
        while (iter != end) {
            std::string func_name = (*iter)[1].str();
            if (builtin_functions.find(func_name) == builtin_functions.end()) {
                unique_functions.insert(func_name);
            }
            ++iter;
        }
        dependencies.assign(unique_functions.begin(), unique_functions.end());
    } catch (...) {}
    return dependencies;
}

std::vector<std::string> ScriptLibraryManager::extractFunctionNames(const std::string& script_code) {
    return collectDependencies(script_code);
}

void ScriptLibraryManager::clearCache() {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    script_cache_.clear();
    script_cache_by_id_.clear();
}

void ScriptLibraryManager::updateCacheFromEntity(const ScriptLibraryEntity& entity) {
    auto def = ScriptDefinition::fromEntity(entity);
    script_cache_[def.name] = def;
    script_cache_by_id_[def.id] = def;
}

} // namespace Scripting
} // namespace PulseOne
