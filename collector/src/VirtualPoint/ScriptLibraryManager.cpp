// =============================================================================
// collector/src/VirtualPoint/ScriptLibraryManager.cpp
// PulseOne 스크립트 라이브러리 매니저 구현 - 컴파일 에러 완전 수정
// =============================================================================

#include "VirtualPoint/ScriptLibraryManager.h"
#include "Database/Repositories/ScriptLibraryRepository.h"
#include "Database/RepositoryFactory.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Database/DatabaseManager.h"
#include <regex>
#include <sstream>
#include <algorithm>
#include <shared_mutex>

namespace PulseOne {
namespace VirtualPoint {

// =============================================================================
// ScriptDefinition 변환 메서드들 - 열거형 변환 함수 추가
// =============================================================================

std::string categoryToString(Database::Entities::ScriptLibraryEntity::Category category) {
    switch (category) {
        case Database::Entities::ScriptLibraryEntity::Category::SYSTEM: return "system";
        case Database::Entities::ScriptLibraryEntity::Category::MATH: return "math";
        case Database::Entities::ScriptLibraryEntity::Category::UTILITY: return "utility";
        case Database::Entities::ScriptLibraryEntity::Category::CUSTOM: return "custom";
        default: return "unknown";
    }
}

Database::Entities::ScriptLibraryEntity::Category stringToCategory(const std::string& str) {
    if (str == "system") return Database::Entities::ScriptLibraryEntity::Category::SYSTEM;
    if (str == "math") return Database::Entities::ScriptLibraryEntity::Category::MATH;
    if (str == "utility") return Database::Entities::ScriptLibraryEntity::Category::UTILITY;
    if (str == "custom") return Database::Entities::ScriptLibraryEntity::Category::CUSTOM;
    return Database::Entities::ScriptLibraryEntity::Category::CUSTOM;
}

std::string returnTypeToString(Database::Entities::ScriptLibraryEntity::ReturnType type) {
    switch (type) {
        case Database::Entities::ScriptLibraryEntity::ReturnType::NUMBER: return "number";
        case Database::Entities::ScriptLibraryEntity::ReturnType::STRING: return "string";
        case Database::Entities::ScriptLibraryEntity::ReturnType::BOOLEAN: return "boolean";
        case Database::Entities::ScriptLibraryEntity::ReturnType::OBJECT: return "object";
        case Database::Entities::ScriptLibraryEntity::ReturnType::VOID: return "void";
        default: return "unknown";
    }
}

Database::Entities::ScriptLibraryEntity::ReturnType stringToReturnType(const std::string& str) {
    if (str == "number") return Database::Entities::ScriptLibraryEntity::ReturnType::NUMBER;
    if (str == "string") return Database::Entities::ScriptLibraryEntity::ReturnType::STRING;
    if (str == "boolean") return Database::Entities::ScriptLibraryEntity::ReturnType::BOOLEAN;
    if (str == "object") return Database::Entities::ScriptLibraryEntity::ReturnType::OBJECT;
    if (str == "void") return Database::Entities::ScriptLibraryEntity::ReturnType::VOID;
    return Database::Entities::ScriptLibraryEntity::ReturnType::OBJECT;
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
    if (!entity.getParameters().empty()) {
        try {
            def.parameters = json::parse(entity.getParameters());
        } catch (const std::exception& e) {
            // 파싱 실패시 빈 JSON
            def.parameters = json::array();
        }
    }
    
    def.return_type = returnTypeToString(entity.getReturnType());
    
    // Tags 가져오기 - getTags() 메서드 사용
    def.tags = entity.getTags();
    
    def.example_usage = entity.getExampleUsage();
    def.is_system = entity.getIsSystem();
    def.usage_count = entity.getUsageCount();
    def.rating = entity.getRating();
    
    return def;
}

ScriptLibraryEntity ScriptDefinition::toEntity() const {
    ScriptLibraryEntity entity(0, name, script_code);  // tenant_id는 나중에 설정
    
    entity.setCategory(stringToCategory(category));
    entity.setDisplayName(display_name);
    entity.setDescription(description);
    entity.setParameters(parameters.dump());
    entity.setReturnType(stringToReturnType(return_type));
    
    // Tags 설정 - setTags() 메서드 사용
    entity.setTags(tags);
    
    entity.setExampleUsage(example_usage);
    entity.setIsSystem(is_system);
    
    return entity;
}

// =============================================================================
// 싱글톤 구현
// =============================================================================

ScriptLibraryManager& ScriptLibraryManager::getInstance() {
    static ScriptLibraryManager instance;
    return instance;
}

ScriptLibraryManager::ScriptLibraryManager() {
    auto& logger = LogManager::getInstance();
    logger.log("scriptlib", LogLevel::INFO, "📚 ScriptLibraryManager 생성");
}

ScriptLibraryManager::~ScriptLibraryManager() {
    shutdown();
}

// =============================================================================
// 초기화/종료 - 올바른 싱글톤 사용법
// =============================================================================

bool ScriptLibraryManager::initialize() {
    if (initialized_) {
        auto& logger = LogManager::getInstance();
        logger.log("scriptlib", LogLevel::WARN, "ScriptLibraryManager 이미 초기화됨");
        return true;
    }
    
    try {
        // ✅ 올바른 싱글톤 사용법 - 직접 가져오기
        auto& db_manager = Database::DatabaseManager::getInstance();
        auto& config_manager = ConfigManager::getInstance();
        
        // Repository 생성
        repository_ = Database::RepositoryFactory::getInstance().createScriptLibraryRepository();
        
        // 시스템 스크립트 로드
        if (!loadSystemScripts()) {
            auto& logger = LogManager::getInstance();
            logger.log("scriptlib", LogLevel::ERROR, "시스템 스크립트 로드 실패");
            return false;
        }
        
        initialized_ = true;
        auto& logger = LogManager::getInstance();
        logger.log("scriptlib", LogLevel::INFO, "✅ ScriptLibraryManager 초기화 완료");
        
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("scriptlib", LogLevel::ERROR, "ScriptLibraryManager 초기화 실패: " + std::string(e.what()));
        return false;
    }
}

void ScriptLibraryManager::shutdown() {
    if (!initialized_) return;
    
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    
    script_cache_.clear();
    script_cache_by_id_.clear();
    template_cache_.clear();
    system_script_names_.clear();
    
    initialized_ = false;
    auto& logger = LogManager::getInstance();
    logger.log("scriptlib", LogLevel::INFO, "ScriptLibraryManager 종료");
}

// =============================================================================
// 스크립트 로드
// =============================================================================

bool ScriptLibraryManager::loadScripts(int tenant_id) {
    if (!repository_) {
        auto& logger = LogManager::getInstance();
        logger.log("scriptlib", LogLevel::ERROR, "Repository not initialized");
        return false;
    }
    
    try {
        // 테넌트별 스크립트 로드
        auto entities = repository_->findByTenantId(tenant_id);
        
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        
        for (const auto& entity : entities) {
            auto script_def = ScriptDefinition::fromEntity(entity);
            script_cache_[script_def.name] = script_def;
            script_cache_by_id_[script_def.id] = script_def;
        }
        
        current_tenant_id_ = tenant_id;
        
        auto& logger = LogManager::getInstance();
        logger.log("scriptlib", LogLevel::INFO, 
                   "테넌트 " + std::to_string(tenant_id) + "의 스크립트 " + 
                   std::to_string(entities.size()) + "개 로드");
        
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("scriptlib", LogLevel::ERROR, 
                   "스크립트 로드 실패: " + std::string(e.what()));
        return false;
    }
}

bool ScriptLibraryManager::loadSystemScripts() {
    if (!repository_) {
        auto& logger = LogManager::getInstance();
        logger.log("scriptlib", LogLevel::ERROR, "Repository not initialized");
        return false;
    }
    
    try {
        // 시스템 스크립트 로드 (is_system = true)
        auto entities = repository_->findSystemScripts();
        
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        
        for (const auto& entity : entities) {
            auto script_def = ScriptDefinition::fromEntity(entity);
            script_cache_[script_def.name] = script_def;
            script_cache_by_id_[script_def.id] = script_def;
            system_script_names_.push_back(script_def.name);
        }
        
        auto& logger = LogManager::getInstance();
        logger.log("scriptlib", LogLevel::INFO, 
                   "시스템 스크립트 " + std::to_string(entities.size()) + "개 로드");
        
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("scriptlib", LogLevel::ERROR, 
                   "시스템 스크립트 로드 실패: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 스크립트 조회
// =============================================================================

std::optional<ScriptDefinition> ScriptLibraryManager::getScript(const std::string& name, int tenant_id) {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    
    // 캐시에서 먼저 확인
    auto it = script_cache_.find(name);
    if (it != script_cache_.end()) {
        return it->second;
    }
    
    // 캐시에 없으면 DB에서 조회
    lock.unlock();
    
    if (repository_) {
        auto entity_opt = repository_->findByName(tenant_id, name);
        if (entity_opt) {
            auto script_def = ScriptDefinition::fromEntity(*entity_opt);
            
            // 캐시 업데이트
            std::unique_lock<std::shared_mutex> write_lock(cache_mutex_);
            script_cache_[name] = script_def;
            script_cache_by_id_[script_def.id] = script_def;
            
            return script_def;
        }
    }
    
    return std::nullopt;
}

std::optional<ScriptDefinition> ScriptLibraryManager::getScriptById(int script_id) {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    
    auto it = script_cache_by_id_.find(script_id);
    if (it != script_cache_by_id_.end()) {
        return it->second;
    }
    
    // 캐시에 없으면 DB에서 조회
    lock.unlock();
    
    if (repository_) {
        auto entity_opt = repository_->findById(script_id);
        if (entity_opt) {
            auto script_def = ScriptDefinition::fromEntity(*entity_opt);
            
            // 캐시 업데이트
            std::unique_lock<std::shared_mutex> write_lock(cache_mutex_);
            script_cache_[script_def.name] = script_def;
            script_cache_by_id_[script_def.id] = script_def;
            
            return script_def;
        }
    }
    
    return std::nullopt;
}

std::vector<ScriptDefinition> ScriptLibraryManager::getScriptsByCategory(const std::string& category) {
    std::vector<ScriptDefinition> results;
    
    if (!repository_) return results;
    
    auto cat_enum = stringToCategory(category);
    auto entities = repository_->findByCategory(cat_enum);
    for (const auto& entity : entities) {
        results.push_back(ScriptDefinition::fromEntity(entity));
    }
    
    return results;
}

// =============================================================================
// JavaScript 코드 생성
// =============================================================================

std::string ScriptLibraryManager::buildCompleteScript(const std::string& formula, int tenant_id) {
    // 수식에서 사용된 함수 이름 추출
    auto dependencies = collectDependencies(formula);
    
    if (dependencies.empty()) {
        return formula;  // 의존성 없으면 그대로 반환
    }
    
    std::stringstream complete_script;
    
    // 의존 함수들을 먼저 추가
    for (const auto& func_name : dependencies) {
        auto script_opt = getScript(func_name, tenant_id);
        if (script_opt) {
            complete_script << "// === " << script_opt->display_name << " ===\n";
            complete_script << script_opt->script_code << "\n\n";
        }
    }
    
    // 원본 수식 추가
    complete_script << "// === Main Formula ===\n";
    complete_script << formula;
    
    return complete_script.str();
}

std::vector<std::string> ScriptLibraryManager::collectDependencies(const std::string& formula) {
    std::vector<std::string> dependencies;
    
    // 함수 호출 패턴 찾기: function_name(...)
    std::regex func_pattern(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\()");
    std::smatch matches;
    
    std::string temp = formula;
    while (std::regex_search(temp, matches, func_pattern)) {
        std::string func_name = matches[1];
        
        // 시스템 함수인지 확인
        if (std::find(system_script_names_.begin(), system_script_names_.end(), func_name) != system_script_names_.end()) {
            // 중복 제거
            if (std::find(dependencies.begin(), dependencies.end(), func_name) == dependencies.end()) {
                dependencies.push_back(func_name);
            }
        }
        
        temp = matches.suffix();
    }
    
    return dependencies;
}

// =============================================================================
// 템플릿 관리
// =============================================================================

std::string ScriptLibraryManager::generateFromTemplate(int template_id, const json& variables) {
    auto tmpl_opt = getTemplateById(template_id);
    if (!tmpl_opt) {
        auto& logger = LogManager::getInstance();
        logger.log("scriptlib", LogLevel::ERROR, "템플릿 ID " + std::to_string(template_id) + " 찾을 수 없음");
        return "";
    }
    
    return generateFromTemplate(*tmpl_opt, variables);
}

std::string ScriptLibraryManager::generateFromTemplate(const ScriptTemplate& tmpl, const json& variables) {
    std::string result = tmpl.template_code;
    
    // {{variable_name}} 패턴을 실제 값으로 치환
    for (auto& [key, value] : variables.items()) {
        std::string placeholder = "{{" + key + "}}";
        std::string replacement;
        
        if (value.is_string()) {
            replacement = "\"" + value.get<std::string>() + "\"";
        } else if (value.is_number()) {
            replacement = std::to_string(value.get<double>());
        } else if (value.is_boolean()) {
            replacement = value.get<bool>() ? "true" : "false";
        } else {
            replacement = value.dump();
        }
        
        // 모든 occurrence 치환
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), replacement);
            pos += replacement.length();
        }
    }
    
    return result;
}

// =============================================================================
// 사용 통계
// =============================================================================

void ScriptLibraryManager::recordUsage(int script_id, int virtual_point_id, const std::string& context) {
    if (!repository_) return;
    
    // 사용 횟수 증가
    repository_->incrementUsageCount(script_id);
    
    // 사용 이력 기록
    repository_->recordUsage(script_id, virtual_point_id, current_tenant_id_, context);
    
    // 캐시 업데이트
    auto it = script_cache_by_id_.find(script_id);
    if (it != script_cache_by_id_.end()) {
        it->second.usage_count++;
    }
}

void ScriptLibraryManager::recordUsage(const std::string& script_name, int virtual_point_id, const std::string& context) {
    auto script_opt = getScript(script_name, current_tenant_id_);
    if (script_opt) {
        recordUsage(script_opt->id, virtual_point_id, context);
    }
}

json ScriptLibraryManager::getUsageStatistics(int tenant_id) {
    if (!repository_) return json::object();
    
    return repository_->getUsageStatistics(tenant_id);
}

// =============================================================================
// 검색
// =============================================================================

std::vector<ScriptDefinition> ScriptLibraryManager::searchScripts(const std::string& keyword) {
    std::vector<ScriptDefinition> results;
    
    if (!repository_) return results;
    
    auto entities = repository_->search(keyword);
    for (const auto& entity : entities) {
        results.push_back(ScriptDefinition::fromEntity(entity));
    }
    
    return results;
}

// =============================================================================
// 헬퍼 메서드
// =============================================================================

std::vector<std::string> ScriptLibraryManager::extractFunctionNames(const std::string& script_code) {
    std::vector<std::string> function_names;
    
    // function name(...) 패턴 찾기
    std::regex func_pattern(R"(function\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\()");
    std::smatch matches;
    
    std::string temp = script_code;
    while (std::regex_search(temp, matches, func_pattern)) {
        function_names.push_back(matches[1]);
        temp = matches.suffix();
    }
    
    return function_names;
}

std::vector<std::string> ScriptLibraryManager::parseTags(const std::string& tags_json) {
    std::vector<std::string> tags;
    
    try {
        json j = json::parse(tags_json);
        if (j.is_array()) {
            for (const auto& tag : j) {
                if (tag.is_string()) {
                    tags.push_back(tag.get<std::string>());
                }
            }
        }
    } catch (const std::exception& e) {
        // 파싱 실패시 빈 벡터 반환
    }
    
    return tags;
}

std::string ScriptLibraryManager::tagsToJson(const std::vector<std::string>& tags) {
    json j = tags;
    return j.dump();
}

// =============================================================================
// 템플릿 관련 구현
// =============================================================================

std::vector<ScriptTemplate> ScriptLibraryManager::getTemplates(const std::string& category) {
    std::vector<ScriptTemplate> templates;
    
    if (!repository_) return templates;
    
    auto template_rows = repository_->getTemplates(category);
    
    for (const auto& row : template_rows) {
        ScriptTemplate tmpl;
        
        // ID 변환
        if (row.find("id") != row.end()) {
            tmpl.id = std::stoi(row.at("id"));
        }
        
        // 문자열 필드들
        if (row.find("category") != row.end()) tmpl.category = row.at("category");
        if (row.find("name") != row.end()) tmpl.name = row.at("name");
        if (row.find("description") != row.end()) tmpl.description = row.at("description");
        if (row.find("template_code") != row.end()) tmpl.template_code = row.at("template_code");
        if (row.find("industry") != row.end()) tmpl.industry = row.at("industry");
        if (row.find("equipment_type") != row.end()) tmpl.equipment_type = row.at("equipment_type");
        
        // JSON 필드 파싱
        if (row.find("variables") != row.end() && !row.at("variables").empty()) {
            try {
                tmpl.variables = json::parse(row.at("variables"));
            } catch (const std::exception& e) {
                tmpl.variables = json::array();
            }
        }
        
        templates.push_back(tmpl);
    }
    
    return templates;
}

std::optional<ScriptTemplate> ScriptLibraryManager::getTemplateById(int template_id) {
    if (!repository_) return std::nullopt;
    
    auto row_opt = repository_->getTemplateById(template_id);
    if (!row_opt) return std::nullopt;
    
    const auto& row = *row_opt;
    ScriptTemplate tmpl;
    
    // ID 변환
    if (row.find("id") != row.end()) {
        tmpl.id = std::stoi(row.at("id"));
    }
    
    // 문자열 필드들
    if (row.find("category") != row.end()) tmpl.category = row.at("category");
    if (row.find("name") != row.end()) tmpl.name = row.at("name");
    if (row.find("description") != row.end()) tmpl.description = row.at("description");
    if (row.find("template_code") != row.end()) tmpl.template_code = row.at("template_code");
    if (row.find("industry") != row.end()) tmpl.industry = row.at("industry");
    if (row.find("equipment_type") != row.end()) tmpl.equipment_type = row.at("equipment_type");
    
    // JSON 필드 파싱
    if (row.find("variables") != row.end() && !row.at("variables").empty()) {
        try {
            tmpl.variables = json::parse(row.at("variables"));
        } catch (const std::exception& e) {
            tmpl.variables = json::array();
        }
    }
    
    return tmpl;
}

} // namespace VirtualPoint
} // namespace PulseOne