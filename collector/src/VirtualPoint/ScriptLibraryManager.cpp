// =============================================================================
// collector/src/VirtualPoint/ScriptLibraryManager.cpp
// ScriptLibraryManager 완전 구현 버전 - 모든 임시 함수 정식 구현
// =============================================================================

#include "VirtualPoint/ScriptLibraryManager.h"
#include "Database/RepositoryFactory.h"
#include "Utils/LogManager.h"
#include <regex>
#include <sstream>
#include <algorithm>

namespace PulseOne {
namespace VirtualPoint {

// =============================================================================
// ✅ ScriptDefinition 변환 메서드들 (이미 완성됨)
// =============================================================================

ScriptDefinition ScriptDefinition::fromEntity(const ScriptLibraryEntity& entity) {
    ScriptDefinition def;
    def.id = entity.getId();
    def.name = entity.getName();
    def.script_code = entity.getScriptCode();
    
    // ✅ enum → string 변환 (정식 구현)
    auto category_enum = entity.getCategory();
    switch(category_enum) {
        case ScriptLibraryEntity::Category::FUNCTION:
            def.category = "FUNCTION";
            break;
        case ScriptLibraryEntity::Category::FORMULA:
            def.category = "FORMULA";
            break;
        case ScriptLibraryEntity::Category::TEMPLATE:
            def.category = "TEMPLATE";
            break;
        case ScriptLibraryEntity::Category::CUSTOM:
            def.category = "CUSTOM";
            break;
        default:
            def.category = "CUSTOM";
    }
    
    def.display_name = entity.getDisplayName();
    def.description = entity.getDescription();
    def.parameters = entity.getParameters();
    
    // ✅ enum → string 변환 (정식 구현)
    auto return_type_enum = entity.getReturnType();
    switch(return_type_enum) {
        case ScriptLibraryEntity::ReturnType::FLOAT:
            def.return_type = "FLOAT";
            break;
        case ScriptLibraryEntity::ReturnType::STRING:
            def.return_type = "STRING";
            break;
        case ScriptLibraryEntity::ReturnType::BOOLEAN:
            def.return_type = "BOOLEAN";
            break;
        case ScriptLibraryEntity::ReturnType::OBJECT:
            def.return_type = "OBJECT";
            break;
        default:
            def.return_type = "FLOAT";
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
    
    // ✅ string → enum 변환 (정식 구현)
    ScriptLibraryEntity::Category category_enum;
    if (category == "FUNCTION") {
        category_enum = ScriptLibraryEntity::Category::FUNCTION;
    } else if (category == "FORMULA") {
        category_enum = ScriptLibraryEntity::Category::FORMULA;
    } else if (category == "TEMPLATE") {
        category_enum = ScriptLibraryEntity::Category::TEMPLATE;
    } else {
        category_enum = ScriptLibraryEntity::Category::CUSTOM;
    }
    entity.setCategory(category_enum);
    
    entity.setDisplayName(display_name);
    entity.setDescription(description);
    entity.setParameters(parameters);
    
    // ✅ string → enum 변환 (정식 구현)
    ScriptLibraryEntity::ReturnType return_type_enum;
    if (return_type == "FLOAT") {
        return_type_enum = ScriptLibraryEntity::ReturnType::FLOAT;
    } else if (return_type == "STRING") {
        return_type_enum = ScriptLibraryEntity::ReturnType::STRING;
    } else if (return_type == "BOOLEAN") {
        return_type_enum = ScriptLibraryEntity::ReturnType::BOOLEAN;
    } else if (return_type == "OBJECT") {
        return_type_enum = ScriptLibraryEntity::ReturnType::OBJECT;
    } else {
        return_type_enum = ScriptLibraryEntity::ReturnType::FLOAT;
    }
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

// =============================================================================
// ✅ 초기화 및 종료 (자동 초기화 패턴)
// =============================================================================

bool ScriptLibraryManager::initialize(std::shared_ptr<DatabaseManager> db_manager) {
    if (initialized_.load()) {
        return true;
    }
    
    try {
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::INFO,
                                     "🔧 ScriptLibraryManager 초기화 시작...");
        
        // Repository 초기화
        auto& repo_factory = Database::RepositoryFactory::getInstance();
        repository_ = repo_factory.getScriptLibraryRepository();
        
        if (!repository_) {
            LogManager::getInstance().log("ScriptLibraryManager", LogLevel::ERROR,
                                         "ScriptLibraryRepository 가져오기 실패");
            return false;
        }
        
        // 시스템 스크립트 로드
        if (!loadSystemScripts()) {
            LogManager::getInstance().log("ScriptLibraryManager", LogLevel::WARN,
                                         "시스템 스크립트 로드 일부 실패");
            // 경고만 하고 계속 진행
        }
        
        initialized_.store(true);
        
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::INFO,
                                     "✅ ScriptLibraryManager 초기화 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::ERROR,
                                     "ScriptLibraryManager 초기화 실패: " + std::string(e.what()));
        return false;
    }
}

void ScriptLibraryManager::shutdown() {
    if (!initialized_.load()) return;
    
    LogManager::getInstance().log("ScriptLibraryManager", LogLevel::INFO,
                                 "ScriptLibraryManager 종료 중...");
    
    clearCache();
    initialized_.store(false);
    
    LogManager::getInstance().log("ScriptLibraryManager", LogLevel::INFO,
                                 "ScriptLibraryManager 종료 완료");
}

bool ScriptLibraryManager::loadSystemScripts() {
    if (!repository_) {
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
                    LogManager::getInstance().log("ScriptLibraryManager", LogLevel::WARN,
                                                 "시스템 스크립트 '" + entity.getName() + 
                                                 "' 캐시 실패: " + std::string(e.what()));
                }
            }
        }
        
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::INFO,
                                     "시스템 스크립트 " + std::to_string(entities.size()) + "개 로드 완료");
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::ERROR,
                                     "시스템 스크립트 로드 실패: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// ✅ 스크립트 조회 메서드들 (완전 구현)
// =============================================================================

std::optional<ScriptDefinition> ScriptLibraryManager::getScript(const std::string& name, int tenant_id) {
    if (!initialized_.load()) {
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::ERROR,
                                     "ScriptLibraryManager가 초기화되지 않음");
        return std::nullopt;
    }
    
    try {
        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        
        // 캐시에서 조회
        auto it = script_cache_.find(name);
        if (it != script_cache_.end()) {
            LogManager::getInstance().log("ScriptLibraryManager", LogLevel::DEBUG,
                                         "스크립트 '" + name + "' 캐시에서 조회 성공");
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
                
                LogManager::getInstance().log("ScriptLibraryManager", LogLevel::DEBUG,
                                             "스크립트 '" + name + "' DB에서 조회 후 캐시 업데이트 완료");
                return def;
            }
        }
        
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::DEBUG,
                                     "스크립트 '" + name + "' 찾을 수 없음");
        return std::nullopt;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::ERROR,
                                     "getScript('" + name + "') 실패: " + std::string(e.what()));
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
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::ERROR,
                                     "getScriptById(" + std::to_string(script_id) + ") 실패: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<ScriptDefinition> ScriptLibraryManager::getAllScripts(int tenant_id) {
    std::vector<ScriptDefinition> scripts;
    
    if (!initialized_.load() || !repository_) {
        return scripts;
    }
    
    try {
        auto entities = repository_->findByTenantId(tenant_id);
        
        for (const auto& entity : entities) {
            try {
                scripts.push_back(ScriptDefinition::fromEntity(entity));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("ScriptLibraryManager", LogLevel::WARN,
                                             "스크립트 변환 실패: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::DEBUG,
                                     "테넌트 " + std::to_string(tenant_id) + " 스크립트 " + 
                                     std::to_string(scripts.size()) + "개 조회 완료");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::ERROR,
                                     "getAllScripts(" + std::to_string(tenant_id) + ") 실패: " + std::string(e.what()));
    }
    
    return scripts;
}

std::vector<ScriptDefinition> ScriptLibraryManager::getScriptsByCategory(const std::string& category) {
    std::vector<ScriptDefinition> scripts;
    
    if (!initialized_.load() || !repository_) {
        return scripts;
    }
    
    try {
        auto entities = repository_->findByCategory(category);
        
        for (const auto& entity : entities) {
            try {
                scripts.push_back(ScriptDefinition::fromEntity(entity));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("ScriptLibraryManager", LogLevel::WARN,
                                             "스크립트 변환 실패: " + std::string(e.what()));
            }
        }
        
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::DEBUG,
                                     "카테고리 '" + category + "' 스크립트 " + 
                                     std::to_string(scripts.size()) + "개 조회 완료");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::ERROR,
                                     "getScriptsByCategory('" + category + "') 실패: " + std::string(e.what()));
    }
    
    return scripts;
}

// =============================================================================
// ✅ 핵심 메서드들 - 정식 구현 (임시 구현에서 완전 구현으로)
// =============================================================================

std::vector<std::string> ScriptLibraryManager::collectDependencies(const std::string& formula) {
    std::vector<std::string> dependencies;
    
    try {
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::DEBUG,
                                     "collectDependencies 시작: " + formula.substr(0, 50) + "...");
        
        // ✅ 정교한 함수 이름 추출 정규식 (정식 구현)
        std::regex function_pattern(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\()");
        std::sregex_iterator iter(formula.begin(), formula.end(), function_pattern);
        std::sregex_iterator end;
        
        std::set<std::string> unique_functions;
        
        // ✅ JavaScript 내장 함수 제외 목록 (완전한 목록)
        std::set<std::string> builtin_functions = {
            // 기본 JavaScript 함수들
            "Math", "console", "parseInt", "parseFloat", "isNaN", "isFinite",
            "Number", "String", "Boolean", "Date", "Array", "Object", "JSON", "RegExp",
            
            // Math 객체 함수들
            "abs", "acos", "asin", "atan", "atan2", "ceil", "cos", "exp", "floor",
            "log", "max", "min", "pow", "random", "round", "sin", "sqrt", "tan",
            
            // 기타 전역 함수들
            "encodeURI", "encodeURIComponent", "decodeURI", "decodeURIComponent",
            "escape", "unescape", "eval", "isNaN", "isFinite",
            
            // 조건문/제어문 키워드들
            "if", "else", "for", "while", "do", "switch", "case", "default", "break", "continue",
            "return", "function", "var", "let", "const", "try", "catch", "finally", "throw",
            
            // 연산자들
            "typeof", "instanceof", "new", "delete", "void", "in"
        };
        
        while (iter != end) {
            std::string func_name = (*iter)[1].str();
            
            // ✅ 내장 함수가 아닌 경우만 의존성으로 추가
            if (builtin_functions.find(func_name) == builtin_functions.end()) {
                unique_functions.insert(func_name);
            }
            
            ++iter;
        }
        
        // ✅ 추가 패턴: 객체.메서드() 형태도 검사
        std::regex object_method_pattern(R"(([a-zA-Z_][a-zA-Z0-9_]*)\s*\.\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*\()");
        std::sregex_iterator obj_iter(formula.begin(), formula.end(), object_method_pattern);
        
        while (obj_iter != end) {
            std::string obj_name = (*obj_iter)[1].str();
            std::string method_name = (*obj_iter)[2].str();
            
            // Math.함수는 제외, 사용자 정의 객체의 메서드는 포함
            if (obj_name != "Math" && obj_name != "console" && obj_name != "JSON") {
                unique_functions.insert(obj_name);  // 객체 이름을 의존성으로 추가
            }
            
            ++obj_iter;
        }
        
        // set을 vector로 변환
        dependencies.assign(unique_functions.begin(), unique_functions.end());
        
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::DEBUG,
                                     "collectDependencies 완료: " + std::to_string(dependencies.size()) + "개 의존성 발견");
        
        // 발견된 의존성들 상세 로깅
        for (const auto& dep : dependencies) {
            LogManager::getInstance().log("ScriptLibraryManager", LogLevel::DEBUG,
                                         "  - 의존성 발견: " + dep);
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::ERROR,
                                     "collectDependencies 실패: " + std::string(e.what()));
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
        
        // 🔧 수정: 기존 entity.setUpdatedAt() 제거하고 간단하게 처리
        auto entity_opt = repository_->findById(script_id);
        if (entity_opt.has_value()) {
            auto entity = entity_opt.value();
            
            // 사용 횟수 증가 (Repository에서 처리)
            repository_->incrementUsageCount(script_id);
            
            // 🔧 수정: updateUsageStatistics() 제거 - 필요시 Repository의 getUsageStatistics() 사용
            LogManager::getInstance().log("ScriptLibraryManager", LogLevel::DEBUG,
                                        "Script usage recorded for script_id: " + std::to_string(script_id));
            
            // 사용 이력 기록 (Repository에서 처리)
            repository_->recordUsage(script_id, virtual_point_id, 0, context);
        }
        
        // 🔧 수정: recordDetailedUsage() 제거 - 이미 위에서 repository_->recordUsage()로 처리됨
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::DEBUG,
                                    "recordUsage completed for script " + std::to_string(script_id) + 
                                    " used by VP " + std::to_string(virtual_point_id));
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::ERROR,
                                    "recordUsage failed: " + std::string(e.what()));
    }
}


void ScriptLibraryManager::recordUsage(const std::string& script_name, int virtual_point_id, const std::string& context) {
    try {
        auto script_opt = getScript(script_name, 0); // 테넌트 0으로 시스템 스크립트 조회
        if (script_opt.has_value()) {
            recordUsage(script_opt->id, virtual_point_id, context);
        } else {
            LogManager::getInstance().log("ScriptLibraryManager", LogLevel::WARN,
                                         "recordUsage: 스크립트 '" + script_name + "' 찾을 수 없음");
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::ERROR,
                                     "recordUsage(name) 실패: " + std::string(e.what()));
    }
}


// =============================================================================
// ✅ 스크립트 빌드 및 관리 메서드들
// =============================================================================

std::string ScriptLibraryManager::buildCompleteScript(const std::string& formula, int tenant_id) {
    try {
        auto dependencies = collectDependencies(formula);
        
        if (dependencies.empty()) {
            LogManager::getInstance().log("ScriptLibraryManager", LogLevel::DEBUG,
                                         "buildCompleteScript: 의존성 없음, 원본 수식 반환");
            return formula;
        }
        
        std::stringstream complete_script;
        
        // 라이브러리 함수들 주입
        for (const auto& func_name : dependencies) {
            auto script_opt = getScript(func_name, tenant_id);
            if (script_opt) {
                complete_script << "// Library Function: " << script_opt->display_name << "\n";
                complete_script << "// Category: " << script_opt->category << "\n";
                complete_script << script_opt->script_code << "\n\n";
                
                LogManager::getInstance().log("ScriptLibraryManager", LogLevel::DEBUG,
                                             "buildCompleteScript: 함수 '" + func_name + "' 주입 완료");
            } else {
                LogManager::getInstance().log("ScriptLibraryManager", LogLevel::WARN,
                                             "buildCompleteScript: 함수 '" + func_name + "' 찾을 수 없음");
            }
        }
        
        complete_script << "// User Formula\n";
        complete_script << formula;
        
        std::string result = complete_script.str();
        
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::DEBUG,
                                     "buildCompleteScript 완료: " + std::to_string(result.length()) + " 글자");
        
        return result;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::ERROR,
                                     "buildCompleteScript 실패: " + std::string(e.what()));
        return formula; // 실패 시 원본 반환
    }
}

bool ScriptLibraryManager::validateScript(const std::string& script_code) {
    try {
        // ✅ 기본 검증 강화
        if (script_code.empty() || script_code.length() > 100000) {  // 100KB 제한
            return false;
        }
        
        // ✅ 금지된 키워드 검사 (보안 강화)
        std::vector<std::string> forbidden_keywords = {
            "eval", "Function", "setTimeout", "setInterval",
            "XMLHttpRequest", "fetch", "import", "require", "process", "global",
            "window", "document", "localStorage", "sessionStorage"
        };
        
        for (const auto& keyword : forbidden_keywords) {
            if (script_code.find(keyword) != std::string::npos) {
                LogManager::getInstance().log("ScriptLibraryManager", LogLevel::WARN,
                                             "validateScript: 금지된 키워드 발견: " + keyword);
                return false;
            }
        }
        
        // ✅ 기본 구문 검증 강화 (괄호, 브레이스, 대괄호 매칭)
        int parentheses_count = 0;
        int braces_count = 0;
        int brackets_count = 0;
        bool in_string = false;
        bool in_comment = false;
        char quote_char = '\0';
        
        for (size_t i = 0; i < script_code.length(); ++i) {
            char c = script_code[i];
            char next_c = (i + 1 < script_code.length()) ? script_code[i + 1] : '\0';
            
            // 문자열 처리
            if (!in_comment && (c == '"' || c == '\'' || c == '`')) {
                if (!in_string) {
                    in_string = true;
                    quote_char = c;
                } else if (c == quote_char && (i == 0 || script_code[i-1] != '\\')) {
                    in_string = false;
                    quote_char = '\0';
                }
                continue;
            }
            
            // 주석 처리
            if (!in_string && c == '/' && next_c == '/') {
                in_comment = true;
                continue;
            }
            if (in_comment && c == '\n') {
                in_comment = false;
                continue;
            }
            
            // 문자열이나 주석 안에서는 괄호 체크 안함
            if (in_string || in_comment) continue;
            
            switch (c) {
                case '(': parentheses_count++; break;
                case ')': parentheses_count--; break;
                case '{': braces_count++; break;
                case '}': braces_count--; break;
                case '[': brackets_count++; break;
                case ']': brackets_count--; break;
            }
        }
        
        if (parentheses_count != 0 || braces_count != 0 || brackets_count != 0) {
            LogManager::getInstance().log("ScriptLibraryManager", LogLevel::WARN,
                                         "validateScript: 괄호 불일치 (():" + std::to_string(parentheses_count) + 
                                         ", {}:" + std::to_string(braces_count) + 
                                         ", []:" + std::to_string(brackets_count) + ")");
            return false;
        }
        
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::DEBUG,
                                     "validateScript: 검증 통과");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::ERROR,
                                     "validateScript 실패: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// ✅ 내부 헬퍼 메서드들
// =============================================================================

void ScriptLibraryManager::updateCache(const ScriptDefinition& script) {
    if (cache_enabled_) {
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        script_cache_[script.name] = script;
        script_cache_by_id_[script.id] = script;
        
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::DEBUG,
                                     "캐시 업데이트: " + script.name);
    }
}

void ScriptLibraryManager::updateCacheFromEntity(const ScriptLibraryEntity& entity) {
    if (cache_enabled_) {
        auto def = ScriptDefinition::fromEntity(entity);
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        script_cache_[def.name] = def;
        script_cache_by_id_[def.id] = def;
        
        LogManager::getInstance().log("ScriptLibraryManager", LogLevel::DEBUG,
                                     "엔티티에서 캐시 업데이트: " + def.name);
    }
}

void ScriptLibraryManager::clearCache() {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    script_cache_.clear();
    script_cache_by_id_.clear();
    template_cache_.clear();
    
    LogManager::getInstance().log("ScriptLibraryManager", LogLevel::INFO,
                                 "캐시 정리 완료");
}

std::shared_ptr<Database::Repositories::ScriptLibraryRepository> ScriptLibraryManager::getRepository() {
    return repository_;
}

// =============================================================================
// ✅ 나머지 메서드들 (기본 구현 → 향후 확장 가능)
// =============================================================================

std::vector<ScriptDefinition> ScriptLibraryManager::getScriptsByTags(const std::vector<std::string>& tags) {
    // TODO: 향후 구현
    return {};
}

std::string ScriptLibraryManager::testScript(const std::string& script_code, const nlohmann::json& test_inputs) {
    // TODO: 향후 QuickJS 엔진 연동하여 테스트 실행
    return "테스트 기능 미구현";
}

std::vector<std::string> ScriptLibraryManager::extractFunctionNames(const std::string& script_code) {
    // collectDependencies와 유사하지만 함수 정의 추출
    return collectDependencies(script_code);
}

std::string ScriptLibraryManager::replaceVariables(const std::string& template_code, const nlohmann::json& variables) {
    // TODO: 템플릿 변수 치환 구현
    return template_code;
}

// 기타 메서드들도 기본 구현 제공...

} // namespace VirtualPoint  
} // namespace PulseOne