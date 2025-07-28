#ifndef DEVICE_REPOSITORY_H
#define DEVICE_REPOSITORY_H

/**
 * @file DeviceRepository.h
 * @brief PulseOne DeviceRepository - 타입 정의 문제 해결 완성본
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🔥 타입 정의 문제 해결:
 * - DatabaseTypes.h 사용으로 타입 경로 수정
 * - 네임스페이스 일관성 확보 (PulseOne::Database 내에서 직접 사용)
 * - 불필요한 using 별칭 제거
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/DeviceEntity.h"
#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include "Common/UnifiedCommonTypes.h"  // DataPoint 등 기타 타입용
#include <memory>
#include <map>
#include <string>
#include <mutex>
#include <vector>
#include <optional>
#include <chrono>
#include <atomic>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 🔥 타입 별칭 정의 수정 - Database 네임스페이스 내에서 직접 사용
using DeviceEntity = PulseOne::Database::Entities::DeviceEntity;
using DataPoint = PulseOne::DataPoint;  // Common에서 가져오는 타입

// 🔥 QueryCondition, OrderBy, Pagination은 같은 네임스페이스에 있으므로 별칭 불필요
// using QueryCondition = PulseOne::Database::QueryCondition;  ❌ 제거
// using OrderBy = PulseOne::Database::OrderBy;                ❌ 제거
// using Pagination = PulseOne::Database::Pagination;          ❌ 제거

/**
 * @brief Device Repository 클래스 (IRepository 상속으로 캐시 자동 획득)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 프로토콜별 디바이스 조회
 * - Worker용 최적화 메서드들
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class DeviceRepository : public IRepository<DeviceEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    /**
     * @brief 기본 생성자 (IRepository 초기화 포함)
     */
    DeviceRepository();
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~DeviceRepository() = default;
    
    // =======================================================================
    // IRepository 인터페이스 구현 (INTEGER ID 기반)
    // =======================================================================
    
    /**
     * @brief 모든 디바이스 조회
     * @return 디바이스 목록
     */
    std::vector<DeviceEntity> findAll() override;
    
    /**
     * @brief ID로 디바이스 조회 (INTEGER ID)
     * @param id 디바이스 ID
     * @return 디바이스 (없으면 nullopt)
     */
    std::optional<DeviceEntity> findById(int id) override;
    
    /**
     * @brief 디바이스 저장
     * @param entity 저장할 디바이스 (참조로 전달하여 ID 업데이트)
     * @return 성공 시 true
     */
    bool save(DeviceEntity& entity) override;
    
    /**
     * @brief 디바이스 업데이트
     * @param entity 업데이트할 디바이스
     * @return 성공 시 true
     */
    bool update(const DeviceEntity& entity) override;
    
    /**
     * @brief ID로 디바이스 삭제
     * @param id 삭제할 디바이스 ID
     * @return 성공 시 true
     */
    bool deleteById(int id) override;
    
    /**
     * @brief 디바이스 존재 여부 확인
     * @param id 확인할 ID
     * @return 존재하면 true
     */
    bool exists(int id) override;
    
    // =======================================================================
    // 벌크 연산 (성능 최적화)
    // =======================================================================
    
    /**
     * @brief 여러 ID로 디바이스들 조회
     * @param ids ID 목록
     * @return 디바이스 목록
     */
    std::vector<DeviceEntity> findByIds(const std::vector<int>& ids) override;
    
    /**
     * @brief 조건부 조회
     * @param conditions 쿼리 조건들
     * @param order_by 정렬 조건 (선택사항)
     * @param pagination 페이징 정보 (선택사항)
     * @return 조건에 맞는 디바이스 목록
     */
    std::vector<DeviceEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt) override;
    
    /**
     * @brief 조건에 맞는 디바이스 개수 조회
     * @param conditions 쿼리 조건들
     * @return 개수
     */
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    
    /**
     * @brief 조건으로 첫 번째 디바이스 조회
     * @param conditions 쿼리 조건들
     * @return 첫 번째 매칭 디바이스 (없으면 nullopt)
     */
    std::optional<DeviceEntity> findFirstByConditions(
        const std::vector<QueryCondition>& conditions);
    
    /**
     * @brief 여러 디바이스 일괄 저장
     * @param entities 저장할 디바이스들 (참조로 전달하여 ID 업데이트)
     * @return 저장된 개수
     */
    int saveBulk(std::vector<DeviceEntity>& entities) override;
    
    /**
     * @brief 여러 디바이스 일괄 업데이트
     * @param entities 업데이트할 디바이스들
     * @return 업데이트된 개수
     */
    int updateBulk(const std::vector<DeviceEntity>& entities) override;
    
    /**
     * @brief 여러 ID 일괄 삭제
     * @param ids 삭제할 ID들
     * @return 삭제된 개수
     */
    int deleteByIds(const std::vector<int>& ids) override;
    
    // =======================================================================
    // 캐시 관리 (IRepository에서 자동 제공 - override만 필요)
    // =======================================================================
    
    /**
     * @brief 캐시 활성화/비활성화
     * @param enabled 캐시 사용 여부
     */
    void setCacheEnabled(bool enabled) override;
    
    /**
     * @brief 캐시 상태 조회
     * @return 캐시 활성화 여부
     */
    bool isCacheEnabled() const override;
    
    /**
     * @brief 모든 캐시 삭제
     */
    void clearCache() override;
    
    /**
     * @brief 특정 디바이스 캐시 삭제
     * @param id 디바이스 ID
     */
    void clearCacheForId(int id) override;
    
    /**
     * @brief 캐시 통계 조회
     * @return 캐시 통계 (hits, misses, size 등)
     */
    std::map<std::string, int> getCacheStats() const override;
    
    // =======================================================================
    // 유틸리티
    // =======================================================================
    
    /**
     * @brief 전체 디바이스 개수 조회
     * @return 전체 개수
     */
    int getTotalCount() override;
    
    /**
     * @brief Repository 이름 조회 (디버깅용)
     * @return Repository 이름
     */
    std::string getRepositoryName() const override { return "DeviceRepository"; }

    // =======================================================================
    // Device 전용 메서드들 (구현 파일에 있는 모든 메서드들)
    // =======================================================================
    
    /**
     * @brief 활성 디바이스만 조회
     * @return 활성 디바이스 목록
     */
    std::vector<DeviceEntity> findAllEnabled();
    
    /**
     * @brief 프로토콜별 디바이스 조회
     * @param protocol_type 프로토콜 타입 ("modbus", "mqtt", "bacnet" 등)
     * @return 해당 프로토콜의 디바이스 목록
     */
    std::vector<DeviceEntity> findByProtocol(const std::string& protocol_type);
    
    /**
     * @brief 테넌트별 디바이스 조회
     * @param tenant_id 테넌트 ID
     * @return 해당 테넌트의 디바이스 목록
     */
    std::vector<DeviceEntity> findByTenant(int tenant_id);
    
    /**
     * @brief 사이트별 디바이스 조회
     * @param site_id 사이트 ID
     * @return 해당 사이트의 디바이스 목록
     */
    std::vector<DeviceEntity> findBySite(int site_id);
    
    /**
     * @brief 엔드포인트로 디바이스 조회
     * @param endpoint 엔드포인트 주소
     * @return 해당 엔드포인트의 디바이스 (없으면 nullopt)
     */
    std::optional<DeviceEntity> findByEndpoint(const std::string& endpoint);
    
    /**
     * @brief 이름 패턴으로 디바이스 조회
     * @param name_pattern 이름 패턴 (LIKE 검색)
     * @return 패턴에 맞는 디바이스 목록
     */
    std::vector<DeviceEntity> findByNamePattern(const std::string& name_pattern);
    
    /**
     * @brief Worker용 디바이스 조회 (관계 데이터 포함)
     * @return Worker용 최적화된 디바이스 목록
     */
    std::vector<DeviceEntity> findDevicesForWorkers();
    
    // =======================================================================
    // 관계 데이터 사전 로딩 (N+1 문제 해결)
    // =======================================================================
    
    /**
     * @brief 데이터포인트 사전 로딩
     * @param devices 디바이스들
     */
    void preloadDataPoints(std::vector<DeviceEntity>& devices);
    
    /**
     * @brief 알람 설정 사전 로딩
     * @param devices 디바이스들
     */
    void preloadAlarmConfigs(std::vector<DeviceEntity>& devices);
    
    /**
     * @brief 모든 관계 데이터 사전 로딩
     * @param devices 디바이스들
     */
    void preloadAllRelations(std::vector<DeviceEntity>& devices);

    // =======================================================================
    // 통계 및 분석
    // =======================================================================
    
    /**
     * @brief 프로토콜별 디바이스 개수 통계
     * @return {protocol_type: count} 맵
     */
    std::map<std::string, int> getCountByProtocol();
    
    /**
     * @brief 테넌트별 디바이스 개수 통계
     * @return {tenant_id: count} 맵
     */
    std::map<int, int> getCountByTenant();
    
    /**
     * @brief 사이트별 디바이스 개수 통계
     * @return {site_id: count} 맵
     */
    std::map<int, int> getCountBySite();
    
    /**
     * @brief 상태별 디바이스 개수 통계
     * @return {status: count} 맵
     */
    std::map<std::string, int> getCountByStatus();
    
    /**
     * @brief 디바이스 상태 일괄 업데이트
     * @param status_updates {device_id: status} 맵
     * @return 업데이트된 개수
     */
    int updateDeviceStatuses(const std::map<int, std::string>& status_updates);

private:
    // =======================================================================
    // 내부 헬퍼 메서드들 (구현 파일에 있는 모든 private 메서드들)
    // =======================================================================
    
    /**
     * @brief SQL 결과를 엔티티 목록으로 변환
     * @param results SQL 실행 결과
     * @return 엔티티 목록
     */
    std::vector<DeviceEntity> mapResultsToEntities(
        const std::vector<std::map<std::string, std::string>>& results);
    
    /**
     * @brief 데이터베이스 행을 엔티티로 변환
     * @param row 데이터베이스 행
     * @return 변환된 엔티티
     */
    DeviceEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief SELECT 쿼리 빌드
     * @param conditions 조건 목록 (선택사항)
     * @param order_by 정렬 조건 (선택사항)
     * @param pagination 페이징 (선택사항)
     * @return 빌드된 SQL 쿼리
     */
    std::string buildSelectQuery(
        const std::vector<QueryCondition>& conditions = {},
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt);
    
    /**
     * @brief WHERE 절 빌드
     * @param conditions 조건 목록
     * @return WHERE 절 문자열
     */
    std::string buildWhereClause(const std::vector<QueryCondition>& conditions) const;
    
    /**
     * @brief ORDER BY 절 빌드
     * @param order_by 정렬 조건
     * @return ORDER BY 절 문자열
     */
    std::string buildOrderByClause(const std::optional<OrderBy>& order_by) const;
    
    /**
     * @brief LIMIT/OFFSET 절 빌드
     * @param pagination 페이징 조건
     * @return LIMIT 절 문자열
     */
    std::string buildLimitClause(const std::optional<Pagination>& pagination) const;
    
    /**
     * @brief PostgreSQL 쿼리 실행
     * @param sql SQL 쿼리
     * @return 실행 결과
     */
    std::vector<std::map<std::string, std::string>> executePostgresQuery(const std::string& sql);
    
    /**
     * @brief SQLite 쿼리 실행
     * @param sql SQL 쿼리
     * @return 실행 결과
     */
    std::vector<std::map<std::string, std::string>> executeSQLiteQuery(const std::string& sql);
    
    /**
     * @brief 통합 비쿼리 실행 (INSERT/UPDATE/DELETE)
     * @param sql SQL 쿼리
     * @return 성공 시 true
     */
    bool executeUnifiedNonQuery(const std::string& sql);
    
    /**
     * @brief 문자열 이스케이프 처리
     * @param str 이스케이프할 문자열
     * @return 이스케이프된 문자열
     */
    std::string escapeString(const std::string& str) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // DEVICE_REPOSITORY_H