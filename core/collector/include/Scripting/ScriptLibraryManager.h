#ifndef SCRIPT_LIBRARY_MANAGER_H
#define SCRIPT_LIBRARY_MANAGER_H

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <nlohmann/json.hpp>
#include "DatabaseManager.hpp"
#include "Database/Entities/ScriptLibraryEntity.h"
#include "Database/Repositories/ScriptLibraryRepository.h"
#include "Logging/LogManager.h"

namespace PulseOne {
namespace Scripting {

using json = nlohmann::json;
using ScriptLibraryEntity = Database::Entities::ScriptLibraryEntity;

/**
 * @brief 스크립트 정의 구조체
 */
struct ScriptDefinition {
    int id = 0;
    std::string category;
    std::string name;
    std::string display_name;
    std::string description;
    std::string script_code;
    json parameters;
    std::string return_type;
    std::vector<std::string> tags;
    std::string example_usage;
    bool is_system = false;
    int usage_count = 0;
    double rating = 0.0;
    std::string version = "1.0.0";
    std::string author = "system";
    
    // Entity 변환
    static ScriptDefinition fromEntity(const ScriptLibraryEntity& entity);
    ScriptLibraryEntity toEntity() const;
};

/**
 * @brief 스크립트 템플릿 구조체
 */
struct ScriptTemplate {
    int id = 0;
    std::string category;
    std::string name;
    std::string description;
    std::string template_code;
    json variables;
    std::string industry;
    std::string equipment_type;
};

/**
 * @brief 스크립트 라이브러리 매니저 클래스 (싱글톤)
 */
class ScriptLibraryManager {
public:
    static ScriptLibraryManager& getInstance();
    
    bool initialize(std::shared_ptr<DbLib::DatabaseManager> db_manager = nullptr);
    void shutdown();
    bool isInitialized() const { return initialized_; }
    
    // 스크립트 로드
    bool loadScripts(int tenant_id = 0);
    bool loadSystemScripts();
    
    // 스크립트 조회
    std::optional<ScriptDefinition> getScript(const std::string& name, int tenant_id = 0);
    std::optional<ScriptDefinition> getScriptById(int script_id);
    std::vector<ScriptDefinition> getAllScripts(int tenant_id = 0);
    
    // 유틸리티
    std::vector<std::string> collectDependencies(const std::string& script);
    std::vector<std::string> extractFunctionNames(const std::string& script_code);
    
    void clearCache();

private:
    ScriptLibraryManager();
    ~ScriptLibraryManager();
    ScriptLibraryManager(const ScriptLibraryManager&) = delete;
    ScriptLibraryManager& operator=(const ScriptLibraryManager&) = delete;
    
    std::shared_ptr<Database::Repositories::ScriptLibraryRepository> getRepository();
    void updateCacheFromEntity(const ScriptLibraryEntity& entity);
    
    // 스크립트 캐시 (name -> ScriptDefinition)
    std::map<std::string, ScriptDefinition> script_cache_;
    std::map<int, ScriptDefinition> script_cache_by_id_;
    
    mutable std::shared_mutex cache_mutex_;
    std::shared_ptr<Database::Repositories::ScriptLibraryRepository> repository_;
    std::atomic<bool> initialized_{false};
};

} // namespace Scripting
} // namespace PulseOne

#endif // SCRIPT_LIBRARY_MANAGER_H
