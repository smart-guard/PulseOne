// =============================================================================
// collector/include/Database/Repositories/ScriptLibraryRepository.h
// PulseOne ScriptLibraryRepository - DeviceRepository 패턴 100% 적용
// =============================================================================

#ifndef SCRIPT_LIBRARY_REPOSITORY_H
#define SCRIPT_LIBRARY_REPOSITORY_H

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/ScriptLibraryEntity.h"
#include "Database/DatabaseManager.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include <memory>
#include <map>
#include <string>
#include <mutex>
#include <vector>
#include <optional>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 타입 별칭 정의
using ScriptLibraryEntity = PulseOne::Database::Entities::ScriptLibraryEntity;

/**
 * @brief 스크립트 라이브러리 Repository 클래스
 */
class ScriptLibraryRepository : public IRepository<ScriptLibraryEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    ScriptLibraryRepository() : IRepository<ScriptLibraryEntity>("ScriptLibraryRepository") {
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("📚 ScriptLibraryRepository initialized");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "true" : "false"));
        }
    }
    
    virtual ~ScriptLibraryRepository() = default;
    
    // =======================================================================
    // IRepository 인터페이스 구현
    // =======================================================================
    std::vector<ScriptLibraryEntity> findAll() override;
    std::optional<ScriptLibraryEntity> findById(int id) override;
    bool save(ScriptLibraryEntity& entity) override;
    bool update(const ScriptLibraryEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;
    
    // =======================================================================
    // 특화 메서드들
    // =======================================================================
    
    // 카테고리별 조회
    std::vector<ScriptLibraryEntity> findByCategory(const std::string& category);
    
    // 테넌트별 조회
    std::vector<ScriptLibraryEntity> findByTenantId(int tenant_id);
    
    // 이름으로 조회
    std::optional<ScriptLibraryEntity> findByName(int tenant_id, const std::string& name);
    
    // 시스템 스크립트 조회
    std::vector<ScriptLibraryEntity> findSystemScripts();
    
    // 태그로 검색
    std::vector<ScriptLibraryEntity> findByTags(const std::vector<std::string>& tags);
    
    // 키워드 검색
    std::vector<ScriptLibraryEntity> search(const std::string& keyword);
    
    // 인기 스크립트 조회
    std::vector<ScriptLibraryEntity> findTopUsed(int limit = 10);
    
    // 사용 횟수 증가
    bool incrementUsageCount(int script_id);
    
    // 사용 이력 기록
    bool recordUsage(int script_id, int virtual_point_id, int tenant_id, const std::string& context);
    
    // 버전 저장
    bool saveVersion(int script_id, const std::string& version, const std::string& code, const std::string& change_log);
    
    // 템플릿 관련
    std::vector<std::map<std::string, std::string>> getTemplates(const std::string& category = "");
    std::optional<std::map<std::string, std::string>> getTemplateById(int template_id);
    
    // =======================================================================
    // 캐시 관리 (IRepository 상속)
    // =======================================================================
    void setCacheEnabled(bool enabled) override {
        IRepository<ScriptLibraryEntity>::setCacheEnabled(enabled);
        if (logger_) {
            logger_->Info("ScriptLibraryRepository cache " + std::string(enabled ? "enabled" : "disabled"));
        }
    }
    
    bool isCacheEnabled() const override {
        return IRepository<ScriptLibraryEntity>::isCacheEnabled();
    }
    
    void clearCache() override {
        IRepository<ScriptLibraryEntity>::clearCache();
        if (logger_) {
            logger_->Info("ScriptLibraryRepository cache cleared");
        }
    }
    
    // =======================================================================
    // 통계
    // =======================================================================
    nlohmann::json getUsageStatistics(int tenant_id = 0);
    
private:
    // =======================================================================
    // 의존성 관리
    // =======================================================================
    DatabaseManager* db_manager_;
    LogManager* logger_;
    ConfigManager* config_manager_;
    
    void initializeDependencies() {
        db_manager_ = &::DatabaseManager::getInstance();
        logger_ = &::LogManager::getInstance();
        config_manager_ = &::ConfigManager::getInstance();
    }
    
    // =======================================================================
    // 내부 헬퍼 메서드
    // =======================================================================
    ScriptLibraryEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    std::vector<ScriptLibraryEntity> mapResultToEntities(const std::vector<std::map<std::string, std::string>>& result);
    std::map<std::string, std::string> entityToParams(const ScriptLibraryEntity& entity);
    bool ensureTableExists();
    bool validateEntity(const ScriptLibraryEntity& entity) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // SCRIPT_LIBRARY_REPOSITORY_H