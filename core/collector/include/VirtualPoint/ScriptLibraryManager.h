// =============================================================================
// collector/include/VirtualPoint/ScriptLibraryManager.h
// PulseOne 스크립트 라이브러리 매니저 - shared_mutex 헤더 추가
// =============================================================================

#ifndef SCRIPT_LIBRARY_MANAGER_H
#define SCRIPT_LIBRARY_MANAGER_H

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <shared_mutex>  // 🔥 누락된 헤더 추가
#include <optional>
#include <nlohmann/json.hpp>
#include "Database/DatabaseManager.h"
#include "Database/Entities/ScriptLibraryEntity.h"
#include "Database/Repositories/ScriptLibraryRepository.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"

namespace PulseOne {
namespace VirtualPoint {

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
    // =======================================================================
    // 싱글톤 패턴
    // =======================================================================
    static ScriptLibraryManager& getInstance();
    
    // =======================================================================
    // 초기화
    // =======================================================================
    bool initialize(std::shared_ptr<::DatabaseManager> db_manager = nullptr);
    void shutdown();
    bool isInitialized() const { return initialized_; }
    
    // =======================================================================
    // 스크립트 관리
    // =======================================================================
    
    // 스크립트 로드
    bool loadScripts(int tenant_id = 0);
    bool loadSystemScripts();
    
    // 스크립트 조회
    std::optional<ScriptDefinition> getScript(const std::string& name, int tenant_id = 0);
    std::optional<ScriptDefinition> getScriptById(int script_id);
    std::vector<ScriptDefinition> getScriptsByCategory(const std::string& category);
    std::vector<ScriptDefinition> getScriptsByTags(const std::vector<std::string>& tags);
    std::vector<ScriptDefinition> getAllScripts(int tenant_id = 0);
    
    // 스크립트 등록/수정/삭제
    int registerScript(const ScriptDefinition& script);
    bool updateScript(int script_id, const ScriptDefinition& script);
    bool deleteScript(int script_id);
    
    // 스크립트 검증
    bool validateScript(const std::string& script_code);
    std::string testScript(const std::string& script_code, const json& test_inputs);
    
    // =======================================================================
    // 템플릿 관리
    // =======================================================================
    
    // 템플릿 조회
    std::vector<ScriptTemplate> getTemplates(const std::string& category = "");
    std::optional<ScriptTemplate> getTemplateById(int template_id);
    std::vector<ScriptTemplate> getTemplatesByIndustry(const std::string& industry);
    std::vector<ScriptTemplate> getTemplatesByEquipment(const std::string& equipment_type);
    
    // 템플릿 기반 코드 생성
    std::string generateFromTemplate(int template_id, const json& variables);
    std::string generateFromTemplate(const ScriptTemplate& tmpl, const json& variables);
    
    // =======================================================================
    // JavaScript 코드 생성
    // =======================================================================
    
    // 완전한 스크립트 빌드 (라이브러리 함수 포함)
    std::string buildCompleteScript(const std::string& formula, int tenant_id = 0);
    
    // 의존 함수 수집
    std::vector<std::string> collectDependencies(const std::string& formula);
    
    // 함수 주입
    std::string injectFunctions(const std::string& formula, const std::vector<std::string>& function_names);
    
    // =======================================================================
    // 사용 통계
    // =======================================================================
    
    // 사용 기록
    void recordUsage(int script_id, int virtual_point_id, const std::string& context = "virtual_point");
    void recordUsage(const std::string& script_name, int virtual_point_id, const std::string& context = "virtual_point");
    
    // 통계 조회
    json getUsageStatistics(int tenant_id = 0);
    std::vector<ScriptDefinition> getTopUsedScripts(int limit = 10);
    std::vector<ScriptDefinition> getRecentlyUsedScripts(int limit = 10);
    
    // =======================================================================
    // 검색
    // =======================================================================
    
    // 키워드 검색
    std::vector<ScriptDefinition> searchScripts(const std::string& keyword);
    
    // 고급 검색
    std::vector<ScriptDefinition> advancedSearch(
        const std::string& keyword = "",
        const std::string& category = "",
        const std::vector<std::string>& tags = {},
        const std::string& return_type = "",
        bool system_only = false
    );
    
    // =======================================================================
    // 버전 관리
    // =======================================================================
    
    // 버전 저장
    bool saveVersion(int script_id, const std::string& version, const std::string& code, const std::string& change_log = "");
    
    // 버전 조회
    std::vector<json> getVersionHistory(int script_id);
    
    // 버전 복원
    bool restoreVersion(int script_id, const std::string& version);
    
    // =======================================================================
    // 캐시 관리
    // =======================================================================
    
    void clearCache();
    void enableCache(bool enable) { cache_enabled_ = enable; }
    bool isCacheEnabled() const { return cache_enabled_; }
    
    // =======================================================================
    // 헬퍼 메서드
    // =======================================================================
    
    // 함수 이름 추출
    std::vector<std::string> extractFunctionNames(const std::string& script_code);
    
    // 변수 치환
    std::string replaceVariables(const std::string& template_code, const json& variables);
    
    // 태그 파싱
    std::vector<std::string> parseTags(const std::string& tags_json);
    std::string tagsToJson(const std::vector<std::string>& tags);
    
private:
    // =======================================================================
    // 생성자 (싱글톤)
    // =======================================================================
    ScriptLibraryManager();
    ~ScriptLibraryManager();
    ScriptLibraryManager(const ScriptLibraryManager&) = delete;
    ScriptLibraryManager& operator=(const ScriptLibraryManager&) = delete;
    
    // =======================================================================
    // 내부 메서드
    // =======================================================================
    
    // Repository 접근
    std::shared_ptr<Database::Repositories::ScriptLibraryRepository> getRepository();
    
    // 스크립트 캐시 업데이트
    void updateCache(const ScriptDefinition& script);
    void updateCacheFromEntity(const ScriptLibraryEntity& entity);
    
    // 의존성 분석
    bool analyzeScript(const std::string& script_code, std::vector<std::string>& dependencies);
    
    // =======================================================================
    // 멤버 변수
    // =======================================================================
    
    // 스크립트 캐시 (name -> ScriptDefinition)
    std::map<std::string, ScriptDefinition> script_cache_;
    std::map<int, ScriptDefinition> script_cache_by_id_;
    
    // 템플릿 캐시
    std::map<int, ScriptTemplate> template_cache_;
    
    // 시스템 스크립트 목록
    std::vector<std::string> system_script_names_;
    
    // 동기화
    mutable std::shared_mutex cache_mutex_;  // 🔥 이제 컴파일 됨
    
    // 설정
    bool cache_enabled_ = true;
    int current_tenant_id_ = 0;
    
    // Repository
    std::shared_ptr<Database::Repositories::ScriptLibraryRepository> repository_;
    
    // 상태
    std::atomic<bool> initialized_{false};
};

} // namespace VirtualPoint
} // namespace PulseOne

#endif // SCRIPT_LIBRARY_MANAGER_H