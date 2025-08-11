// =============================================================================
// collector/include/Database/Repositories/ScriptLibraryRepository.h
// PulseOne ScriptLibraryRepository - DeviceRepository 패턴 100% 적용 (수정완료)
// =============================================================================

#ifndef SCRIPT_LIBRARY_REPOSITORY_H
#define SCRIPT_LIBRARY_REPOSITORY_H

/**
 * @file ScriptLibraryRepository.h
 * @brief PulseOne ScriptLibraryRepository - DeviceRepository 패턴 완전 적용
 * @author PulseOne Development Team
 * @date 2025-08-12
 * 
 * 🔥 완전 수정된 DeviceRepository 패턴:
 * - ExtendedSQLQueries.h 사용
 * - DatabaseAbstractionLayer 패턴
 * - 올바른 LogManager 사용법
 * - IRepository 상속 관계 정확히 준수
 * - 모든 구현 메서드 헤더 선언
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/ScriptLibraryEntity.h"
#include "Database/DatabaseTypes.h"
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 타입 별칭 정의 (DeviceRepository 패턴)
using ScriptLibraryEntity = PulseOne::Database::Entities::ScriptLibraryEntity;

/**
 * @brief Script Library Repository 클래스 (DeviceRepository 패턴 적용)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 스크립트 라이브러리 관리
 * - DatabaseAbstractionLayer 사용
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class ScriptLibraryRepository : public IRepository<ScriptLibraryEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    ScriptLibraryRepository() : IRepository<ScriptLibraryEntity>("ScriptLibraryRepository") {
        initializeDependencies();
        
        // ✅ 올바른 LogManager 사용법
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::INFO,
                                    "📚 ScriptLibraryRepository initialized with BaseEntity pattern");
        LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::INFO,
                                    "✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
    }
    
    virtual ~ScriptLibraryRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현 (필수!)
    // =======================================================================
    
    /**
     * @brief 모든 스크립트 조회
     * @return 모든 ScriptLibraryEntity 목록
     */
    std::vector<ScriptLibraryEntity> findAll() override;
    
    /**
     * @brief ID로 스크립트 조회
     * @param id 스크립트 ID
     * @return ScriptLibraryEntity (optional)
     */
    std::optional<ScriptLibraryEntity> findById(int id) override;
    
    /**
     * @brief 스크립트 저장
     * @param entity ScriptLibraryEntity (참조로 전달하여 ID 업데이트)
     * @return 성공 시 true
     */
    bool save(ScriptLibraryEntity& entity) override;
    
    /**
     * @brief 스크립트 업데이트
     * @param entity ScriptLibraryEntity
     * @return 성공 시 true
     */
    bool update(const ScriptLibraryEntity& entity) override;
    
    /**
     * @brief ID로 스크립트 삭제
     * @param id 스크립트 ID
     * @return 성공 시 true
     */
    bool deleteById(int id) override;
    
    /**
     * @brief 스크립트 존재 여부 확인
     * @param id 스크립트 ID
     * @return 존재하면 true
     */
    bool exists(int id) override;

    // =======================================================================
    // 벌크 연산 (IRepository 상속) - 모든 메서드 구현파일에서 구현됨
    // =======================================================================
    
    /**
     * @brief 여러 ID로 스크립트들 조회
     * @param ids 스크립트 ID 목록
     * @return ScriptLibraryEntity 목록
     */
    std::vector<ScriptLibraryEntity> findByIds(const std::vector<int>& ids) override;
    
    /**
     * @brief 조건에 맞는 스크립트들 조회
     * @param conditions 쿼리 조건들
     * @param order_by 정렬 조건 (optional)
     * @param pagination 페이징 조건 (optional)
     * @return ScriptLibraryEntity 목록
     */
    std::vector<ScriptLibraryEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    /**
     * @brief 조건에 맞는 스크립트 개수 조회
     * @param conditions 쿼리 조건들
     * @return 개수
     */
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    
    /**
     * @brief 여러 스크립트 일괄 저장
     * @param entities 저장할 스크립트들 (참조로 전달하여 ID 업데이트)
     * @return 저장된 개수
     */
    int saveBulk(std::vector<ScriptLibraryEntity>& entities) override;
    
    /**
     * @brief 여러 스크립트 일괄 업데이트
     * @param entities 업데이트할 스크립트들
     * @return 업데이트된 개수
     */
    int updateBulk(const std::vector<ScriptLibraryEntity>& entities) override;
    
    /**
     * @brief 여러 ID 일괄 삭제
     * @param ids 삭제할 ID들
     * @return 삭제된 개수
     */
    int deleteByIds(const std::vector<int>& ids) override;

    // =======================================================================
    // ScriptLibrary 전용 메서드들 (구현파일에서 구현됨)
    // =======================================================================
    
    /**
     * @brief 카테고리별 스크립트 조회
     * @param category 카테고리 ("FUNCTION", "FORMULA", "TEMPLATE", "CUSTOM")
     * @return ScriptLibraryEntity 목록
     */
    std::vector<ScriptLibraryEntity> findByCategory(const std::string& category);
    
    /**
     * @brief 테넌트별 스크립트 조회 (시스템 스크립트 포함)
     * @param tenant_id 테넌트 ID
     * @return ScriptLibraryEntity 목록
     */
    std::vector<ScriptLibraryEntity> findByTenantId(int tenant_id);
    
    /**
     * @brief 이름으로 스크립트 조회
     * @param tenant_id 테넌트 ID
     * @param name 스크립트 이름
     * @return ScriptLibraryEntity (optional)
     */
    std::optional<ScriptLibraryEntity> findByName(int tenant_id, const std::string& name);
    
    /**
     * @brief 시스템 스크립트들만 조회
     * @return ScriptLibraryEntity 목록
     */
    std::vector<ScriptLibraryEntity> findSystemScripts();
    
    /**
     * @brief 태그로 스크립트 검색
     * @param tags 검색할 태그 목록
     * @return ScriptLibraryEntity 목록
     */
    std::vector<ScriptLibraryEntity> findByTags(const std::vector<std::string>& tags);
    
    /**
     * @brief 키워드로 스크립트 검색 (이름, 설명, 태그)
     * @param keyword 검색 키워드
     * @return ScriptLibraryEntity 목록
     */
    std::vector<ScriptLibraryEntity> search(const std::string& keyword);
    
    /**
     * @brief 인기 스크립트 조회 (사용 횟수 순)
     * @param limit 조회할 개수 (기본값: 10)
     * @return ScriptLibraryEntity 목록
     */
    std::vector<ScriptLibraryEntity> findTopUsed(int limit = 10);
    
    /**
     * @brief 스크립트 사용 횟수 증가
     * @param script_id 스크립트 ID
     * @return 성공 시 true
     */
    bool incrementUsageCount(int script_id);
    
    /**
     * @brief 스크립트 사용 이력 기록
     * @param script_id 스크립트 ID
     * @param virtual_point_id 가상포인트 ID
     * @param tenant_id 테넌트 ID
     * @param context 사용 컨텍스트
     * @return 성공 시 true
     */
    bool recordUsage(int script_id, int virtual_point_id, int tenant_id, const std::string& context);
    
    /**
     * @brief 스크립트 버전 저장
     * @param script_id 스크립트 ID
     * @param version 버전 문자열
     * @param code 스크립트 코드
     * @param change_log 변경 로그
     * @return 성공 시 true
     */
    bool saveVersion(int script_id, const std::string& version, 
                    const std::string& code, const std::string& change_log);

    // =======================================================================
    // 템플릿 관련 (구현파일에서 구현 예정)
    // =======================================================================
    
    /**
     * @brief 템플릿 목록 조회
     * @param category 카테고리 (빈 문자열이면 모든 카테고리)
     * @return 템플릿 정보 목록 (단순화된 맵 형태)
     */
    std::vector<std::map<std::string, std::string>> getTemplates(const std::string& category = "");
    
    /**
     * @brief ID로 템플릿 조회
     * @param template_id 템플릿 ID
     * @return 템플릿 정보 (optional)
     */
    std::optional<std::map<std::string, std::string>> getTemplateById(int template_id);

    // =======================================================================
    // 통계 (구현파일에서 구현 예정)
    // =======================================================================
    
    /**
     * @brief 사용 통계 조회 (JSON 형태)
     * @param tenant_id 테넌트 ID (0이면 전체)
     * @return JSON 형태의 통계 데이터
     */
    nlohmann::json getUsageStatistics(int tenant_id = 0);

private:
    // =======================================================================
    // 내부 헬퍼 메서드들 (구현파일에서 구현됨)
    // =======================================================================
    
    /**
     * @brief 테이블 존재 여부 확인 및 생성
     * @return 성공 시 true
     */
    bool ensureTableExists();
    
    /**
     * @brief 데이터베이스 로우를 Entity로 변환
     * @param row 데이터베이스 로우 (key-value 맵)
     * @return ScriptLibraryEntity
     */
    ScriptLibraryEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 여러 로우를 Entity 목록으로 변환
     * @param result 데이터베이스 결과셋
     * @return ScriptLibraryEntity 목록
     */
    std::vector<ScriptLibraryEntity> mapResultToEntities(
        const std::vector<std::map<std::string, std::string>>& result) {
        std::vector<ScriptLibraryEntity> entities;
        entities.reserve(result.size());
        
        for (const auto& row : result) {
            try {
                entities.push_back(mapRowToEntity(row));
            } catch (const std::exception& e) {
                LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::WARN,
                                            "mapResultToEntities - Failed to map row: " + std::string(e.what()));
            }
        }
        
        return entities;
    }
    
    /**
     * @brief Entity를 데이터베이스 파라미터로 변환
     * @param entity ScriptLibraryEntity
     * @return 파라미터 맵
     */
    std::map<std::string, std::string> entityToParams(const ScriptLibraryEntity& entity) {
        std::map<std::string, std::string> params;
        
        // 기본 필드들
        params["tenant_id"] = std::to_string(entity.getTenantId());
        params["name"] = entity.getName();
        params["display_name"] = entity.getDisplayName();
        params["description"] = entity.getDescription();
        params["category"] = entity.getCategoryString();
        params["script_code"] = entity.getScriptCode();
        params["parameters"] = entity.getParameters().dump();
        params["return_type"] = entity.getReturnTypeString();
        params["tags"] = nlohmann::json(entity.getTags()).dump();
        params["example_usage"] = entity.getExampleUsage();
        params["is_system"] = entity.isSystem() ? "1" : "0";
        params["is_template"] = entity.isTemplate() ? "1" : "0";
        params["usage_count"] = std::to_string(entity.getUsageCount());
        params["rating"] = std::to_string(entity.getRating());
        params["version"] = entity.getVersion();
        params["author"] = entity.getAuthor();
        params["license"] = entity.getLicense();
        params["dependencies"] = nlohmann::json(entity.getDependencies()).dump();
        
        return params;
    }
    
    /**
     * @brief Entity 유효성 검증
     * @param entity ScriptLibraryEntity
     * @return 유효하면 true
     */
    bool validateEntity(const ScriptLibraryEntity& entity) const {
        if (entity.getName().empty()) {
            LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                        "validateEntity - Empty script name");
            return false;
        }
        
        if (entity.getScriptCode().empty()) {
            LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                        "validateEntity - Empty script code");
            return false;
        }
        
        if (entity.getTenantId() < 0) {
            LogManager::getInstance().log("ScriptLibraryRepository", LogLevel::ERROR,
                                        "validateEntity - Invalid tenant_id: " + std::to_string(entity.getTenantId()));
            return false;
        }
        
        return true;
    }

    // =======================================================================
    // 의존성 관리 - IRepository에서 이미 제공되므로 중복 제거
    // =======================================================================
    // ❌ 제거: DatabaseManager* db_manager_; (IRepository에서 제공)
    // ❌ 제거: LogManager* logger_; (IRepository에서 제공)  
    // ❌ 제거: ConfigManager* config_manager_; (IRepository에서 제공)
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // SCRIPT_LIBRARY_REPOSITORY_H