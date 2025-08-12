// =============================================================================
// collector/include/Database/Repositories/VirtualPointRepository.h
// PulseOne VirtualPointRepository - ScriptLibraryRepository 패턴 100% 적용
// =============================================================================

#ifndef VIRTUAL_POINT_REPOSITORY_H
#define VIRTUAL_POINT_REPOSITORY_H

/**
 * @file VirtualPointRepository.h
 * @brief PulseOne VirtualPointRepository - ScriptLibraryRepository 패턴 완전 적용
 * @author PulseOne Development Team
 * @date 2025-08-12
 * 
 * 🎯 ScriptLibraryRepository 패턴 100% 적용:
 * - ExtendedSQLQueries.h 사용
 * - 표준 LogManager 사용법
 * - IRepository 상속 관계 정확히 준수
 * - 불필요한 의존성 제거
 * - 모든 구현 메서드 헤더 선언
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/VirtualPointEntity.h"
#include "Database/DatabaseTypes.h"
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <optional>
#include <chrono>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 타입 별칭 정의 (ScriptLibraryRepository 패턴)
using VirtualPointEntity = PulseOne::Database::Entities::VirtualPointEntity;

/**
 * @brief Virtual Point Repository 클래스 (ScriptLibraryRepository 패턴 적용)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 가상포인트 관리
 * - DatabaseAbstractionLayer 사용
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class VirtualPointRepository : public IRepository<VirtualPointEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자 (ScriptLibraryRepository 패턴)
    // =======================================================================
    
    VirtualPointRepository() : IRepository<VirtualPointEntity>("VirtualPointRepository") {
        initializeDependencies();
        
        // ✅ 표준 LogManager 사용법
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::INFO,
                                    "🧮 VirtualPointRepository initialized with ScriptLibraryRepository pattern");
        LogManager::getInstance().log("VirtualPointRepository", LogLevel::INFO,
                                    "✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
    }
    
    virtual ~VirtualPointRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현 (필수!)
    // =======================================================================
    
    /**
     * @brief 모든 가상포인트 조회
     * @return 모든 VirtualPointEntity 목록
     */
    std::vector<VirtualPointEntity> findAll() override;
    
    /**
     * @brief ID로 가상포인트 조회
     * @param id 가상포인트 ID
     * @return VirtualPointEntity (optional)
     */
    std::optional<VirtualPointEntity> findById(int id) override;
    
    /**
     * @brief 가상포인트 저장
     * @param entity VirtualPointEntity (참조로 전달하여 ID 업데이트)
     * @return 성공 시 true
     */
    bool save(VirtualPointEntity& entity) override;
    
    /**
     * @brief 가상포인트 업데이트
     * @param entity VirtualPointEntity
     * @return 성공 시 true
     */
    bool update(const VirtualPointEntity& entity) override;
    
    /**
     * @brief ID로 가상포인트 삭제
     * @param id 가상포인트 ID
     * @return 성공 시 true
     */
    bool deleteById(int id) override;
    
    /**
     * @brief 가상포인트 존재 여부 확인
     * @param id 가상포인트 ID
     * @return 존재하면 true
     */
    bool exists(int id) override;

    // =======================================================================
    // 벌크 연산 (IRepository 상속) - 모든 메서드 구현파일에서 구현됨
    // =======================================================================
    
    /**
     * @brief 여러 ID로 가상포인트들 조회
     * @param ids 가상포인트 ID 목록
     * @return VirtualPointEntity 목록
     */
    std::vector<VirtualPointEntity> findByIds(const std::vector<int>& ids) override;
    
    /**
     * @brief 조건에 맞는 가상포인트들 조회
     * @param conditions 쿼리 조건들
     * @param order_by 정렬 조건 (optional)
     * @param pagination 페이징 조건 (optional)
     * @return VirtualPointEntity 목록
     */
    std::vector<VirtualPointEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    /**
     * @brief 조건에 맞는 가상포인트 개수 조회
     * @param conditions 쿼리 조건들
     * @return 개수
     */
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    
    /**
     * @brief 여러 가상포인트들 저장 (벌크)
     * @param entities VirtualPointEntity 목록 (참조로 전달하여 ID 업데이트)
     * @return 성공한 개수
     */
    int saveBulk(std::vector<VirtualPointEntity>& entities) override;
    
    /**
     * @brief 여러 가상포인트들 업데이트 (벌크)
     * @param entities VirtualPointEntity 목록
     * @return 성공한 개수
     */
    int updateBulk(const std::vector<VirtualPointEntity>& entities) override;
    
    /**
     * @brief 여러 ID로 가상포인트들 삭제 (벌크)
     * @param ids 가상포인트 ID 목록
     * @return 성공한 개수
     */
    int deleteByIds(const std::vector<int>& ids) override;

    // =======================================================================
    // 캐시 관리 (IRepository에서 상속)
    // =======================================================================
    
    /**
     * @brief 모든 캐시 클리어
     */
    void clearCache() override;
    
    /**
     * @brief 캐시 통계 조회
     * @return 캐시 통계 맵
     */
    std::map<std::string, int> getCacheStats() const override;

    // =======================================================================
    // 통계 및 유틸리티 메서드들 (IRepository 표준)
    // =======================================================================
    
    /**
     * @brief 전체 가상포인트 개수 조회
     * @return 전체 개수
     */
    int getTotalCount() override;

    // =======================================================================
    // VirtualPoint 전용 메서드들
    // =======================================================================
    
    /**
     * @brief 테넌트별 가상포인트 조회
     * @param tenant_id 테넌트 ID
     * @return VirtualPointEntity 목록
     */
    std::vector<VirtualPointEntity> findByTenant(int tenant_id);
    
    /**
     * @brief 사이트별 가상포인트 조회
     * @param site_id 사이트 ID
     * @return VirtualPointEntity 목록
     */
    std::vector<VirtualPointEntity> findBySite(int site_id);
    
    /**
     * @brief 디바이스별 가상포인트 조회
     * @param device_id 디바이스 ID
     * @return VirtualPointEntity 목록
     */
    std::vector<VirtualPointEntity> findByDevice(int device_id);
    
    /**
     * @brief 활성화된 가상포인트만 조회
     * @return 활성 VirtualPointEntity 목록
     */
    std::vector<VirtualPointEntity> findEnabled();
    
    /**
     * @brief 카테고리별 가상포인트 조회
     * @param category 카테고리명
     * @return VirtualPointEntity 목록
     */
    std::vector<VirtualPointEntity> findByCategory(const std::string& category);
    
    /**
     * @brief 실행 타입별 가상포인트 조회
     * @param execution_type 실행 타입 (timer, event, manual)
     * @return VirtualPointEntity 목록
     */
    std::vector<VirtualPointEntity> findByExecutionType(const std::string& execution_type);
    
    /**
     * @brief 태그로 가상포인트 검색
     * @param tag 검색할 태그
     * @return VirtualPointEntity 목록
     */
    std::vector<VirtualPointEntity> findByTag(const std::string& tag);
    
    /**
     * @brief 수식에 특정 포인트를 참조하는 가상포인트 찾기
     * @param point_id 참조되는 포인트 ID
     * @return 의존성을 가진 VirtualPointEntity 목록
     */
    std::vector<VirtualPointEntity> findDependents(int point_id);
    
    /**
     * @brief 가상포인트 실행 통계 업데이트
     * @param id 가상포인트 ID
     * @param last_value 계산된 값
     * @param execution_time_ms 실행 시간 (밀리초)
     * @return 성공 시 true
     */
    bool updateExecutionStats(int id, double last_value, double execution_time_ms);
    
    /**
     * @brief 가상포인트 에러 정보 업데이트
     * @param id 가상포인트 ID
     * @param error_message 에러 메시지
     * @return 성공 시 true
     */
    bool updateLastError(int id, const std::string& error_message);
    
    /**
     * @brief 가상포인트 마지막 값만 업데이트
     * @param id 가상포인트 ID
     * @param last_value 마지막 값
     * @return 성공 시 true
     */
    bool updateLastValue(int id, double last_value);
    
    /**
     * @brief 가상포인트 활성화/비활성화
     * @param id 가상포인트 ID
     * @param enabled 활성화 여부
     * @return 성공 시 true
     */
    bool setEnabled(int id, bool enabled);
    
    /**
     * @brief 가상포인트 실행 통계 초기화
     * @param id 가상포인트 ID
     * @return 성공 시 true
     */
    bool resetExecutionStats(int id);

protected:
    // =======================================================================
    // 내부 헬퍼 메서드들 (ScriptLibraryRepository 패턴)
    // =======================================================================
    
    /**
     * @brief DB 행을 엔티티로 변환
     * @param row DB 행 데이터
     * @return VirtualPointEntity
     */
    VirtualPointEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 엔티티를 파라미터 맵으로 변환
     * @param entity VirtualPointEntity
     * @return 파라미터 맵
     */
    std::map<std::string, std::string> entityToParams(const VirtualPointEntity& entity);
    
    /**
     * @brief 가상포인트 데이터 유효성 검사
     * @param entity VirtualPointEntity
     * @return 유효하면 true
     */
    bool validateVirtualPoint(const VirtualPointEntity& entity);
    
    /**
     * @brief 테이블 존재 여부 확인 및 생성
     * @return 성공 시 true
     */
    bool ensureTableExists();

private:
    // =======================================================================
    // 캐시 관련 메서드들 (ScriptLibraryRepository 패턴)
    // =======================================================================
    
    /**
     * @brief 캐시 활성화 여부 확인
     * @return 캐시 활성화 시 true
     */
    bool isCacheEnabled() const;
    
    /**
     * @brief 캐시에서 엔티티 조회
     * @param id 가상포인트 ID
     * @return 캐시된 VirtualPointEntity (optional)
     */
    std::optional<VirtualPointEntity> getCachedEntity(int id);
    
    /**
     * @brief 엔티티를 캐시에 저장
     * @param id 가상포인트 ID
     * @param entity VirtualPointEntity
     */
    void cacheEntity(int id, const VirtualPointEntity& entity);
    
    /**
     * @brief 특정 ID의 캐시 제거
     * @param id 가상포인트 ID
     */
    void clearCacheForId(int id);
    
    /**
     * @brief 모든 캐시 제거
     */
    void clearAllCache();
    
    /**
     * @brief 캐시 키 생성
     * @param id 가상포인트 ID
     * @return 캐시 키 문자열
     */
    std::string generateCacheKey(int id) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // VIRTUAL_POINT_REPOSITORY_H