#ifndef DEVICE_REPOSITORY_H
#define DEVICE_REPOSITORY_H

/**
 * @file DeviceRepository.h
 * @brief PulseOne DeviceRepository - 완전 수정 버전 (네임스페이스 통일)
 * @author PulseOne Development Team
 * @date 2025-07-27
 * 
 * 🔥 주요 변경사항:
 * - 네임스페이스를 PulseOne::Database::Repositories로 통일
 * - INTEGER ID 기반으로 변경 (UUID 제거)
 * - IRepository 상속 구조 수정
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/DeviceEntity.h"
#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include <memory>
#include <map>
#include <string>
#include <mutex>
#include <vector>
#include <optional>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 🔥 타입 별칭 정의 (Entities 네임스페이스 참조)
using DeviceEntity = PulseOne::Database::Entities::DeviceEntity;
using QueryCondition = PulseOne::Database::QueryCondition;
using OrderBy = PulseOne::Database::OrderBy;
using Pagination = PulseOne::Database::Pagination;    

/**
 * @brief Device Repository 클래스 (INTEGER ID 기반)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - 프로토콜별 디바이스 조회
 * - Worker용 최적화 메서드들
 * - 캐싱 및 벌크 연산 지원
 */
class DeviceRepository : public IRepository<DeviceEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    /**
     * @brief 기본 생성자
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
     * @brief 조건으로 첫 번째 디바이스 조회
     * @param conditions 쿼리 조건들
     * @return 첫 번째 디바이스 (없으면 nullopt)
     */
    std::optional<DeviceEntity> findFirstByConditions(
        const std::vector<QueryCondition>& conditions) override;
    
    /**
     * @brief 조건에 맞는 디바이스 개수 조회
     * @param conditions 쿼리 조건들
     * @return 개수
     */
    int countByConditions(const std::vector<QueryCondition>& conditions) override;
    
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
    // 캐시 관리
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
    std::string getRepositoryName() const override;

    // =======================================================================
    // Device 전용 메서드들
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
     * @brief Worker용 디바이스 조회 (관계 데이터 포함)
     * @return Worker용 최적화된 디바이스 목록
     */
    std::vector<DeviceEntity> findDevicesForWorkers();
    
    /**
     * @brief 이름으로 디바이스 조회
     * @param device_name 디바이스 이름
     * @return 디바이스 (없으면 nullopt)
     */
    std::optional<DeviceEntity> findByName(const std::string& device_name);
    
    /**
     * @brief IP와 포트로 디바이스 조회
     * @param ip_address IP 주소
     * @param port 포트 번호
     * @return 해당 엔드포인트의 디바이스 목록
     */
    std::vector<DeviceEntity> findByEndpoint(const std::string& ip_address, int port);
    
    /**
     * @brief 비활성 디바이스 조회
     * @return 비활성 디바이스 목록
     */
    std::vector<DeviceEntity> findDisabled();
    
    /**
     * @brief 최근 통신이 없는 디바이스 조회
     * @param minutes 분 단위 (기본값: 60분)
     * @return 통신이 끊긴 디바이스 목록
     */
    std::vector<DeviceEntity> findOfflineDevices(int minutes = 60);

    // =======================================================================
    // 관계 데이터 사전 로딩 (N+1 문제 해결)
    // =======================================================================
    
    /**
     * @brief 데이터포인트 사전 로딩
     * @param devices 디바이스들
     */
    void preloadDataPoints(std::vector<DeviceEntity>& devices);
    
    /**
     * @brief 통계 정보 사전 로딩
     * @param devices 디바이스들
     */
    void preloadStatistics(std::vector<DeviceEntity>& devices);

    // =======================================================================
    // 통계 및 분석
    // =======================================================================
    
    /**
     * @brief 프로토콜별 디바이스 개수 통계
     * @return {protocol_type: count} 맵
     */
    std::map<std::string, int> getDeviceCountByProtocol();
    
    /**
     * @brief 디바이스 상태별 개수 통계
     * @return {status: count} 맵
     */
    std::map<std::string, int> getDeviceCountByStatus();
    
    /**
     * @brief 최근 생성된 디바이스 조회
     * @param days 최근 N일 (기본값: 7일)
     * @return 최근 생성된 디바이스 목록
     */
    std::vector<DeviceEntity> findRecentlyCreated(int days = 7);

private:
    // =======================================================================
    // 내부 멤버 변수들
    // =======================================================================
    
    DatabaseManager& db_manager_;            // 데이터베이스 관리자
    ConfigManager& config_manager_;          // 설정 관리자
    LogManager& logger_;                     // 로그 관리자
    
    // 캐싱 관련
    mutable std::mutex cache_mutex_;         // 캐시 뮤텍스
    bool cache_enabled_;                     // 캐시 활성화 여부
    std::map<int, DeviceEntity> entity_cache_;  // 엔티티 캐시
    
    // 캐시 통계
    mutable std::map<std::string, int> cache_stats_;
    
    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 데이터베이스 행을 엔티티로 변환
     * @param row 데이터베이스 행
     * @return 변환된 엔티티
     */
    DeviceEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 캐시에서 엔티티 조회
     * @param id 엔티티 ID
     * @return 캐시된 엔티티 (없으면 nullopt)
     */
    std::optional<DeviceEntity> getFromCache(int id) const;
    
    /**
     * @brief 캐시에 엔티티 저장
     * @param entity 저장할 엔티티
     */
    void putToCache(const DeviceEntity& entity);
    
    /**
     * @brief SQL 결과를 엔티티 목록으로 변환
     * @param result SQL 실행 결과
     * @return 엔티티 목록
     */
    std::vector<DeviceEntity> mapResultToEntities(
        const std::vector<std::map<std::string, std::string>>& result);
    
    /**
     * @brief 캐시 통계 업데이트
     * @param operation 연산 타입 ("hit", "miss", "put", "evict")
     */
    void updateCacheStats(const std::string& operation) const;
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // DEVICE_REPOSITORY_H