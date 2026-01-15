#ifndef DEVICE_REPOSITORY_H
#define DEVICE_REPOSITORY_H

/**
 * @file DeviceRepository.h
 * @brief PulseOne DeviceRepository - protocol_id 기반으로 완전 수정
 * @author PulseOne Development Team
 * @date 2025-08-26
 * 
 * 🔥 protocol_id 기반 완전 변경:
 * - findByProtocol(int protocol_id) 파라미터 타입 변경
 * - groupByProtocolId() 메서드명 및 반환타입 변경
 * - deprecated 메서드 관련 경고 완전 해결
 * - 새로운 컬럼들 지원
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/DeviceEntity.h"
#include "DatabaseManager.hpp"
#include "Logging/LogManager.h"
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

// 타입 별칭 정의 (DeviceSettingsRepository 패턴)
using DeviceEntity = PulseOne::Database::Entities::DeviceEntity;

/**
 * @brief Device Repository 클래스 (protocol_id 기반 완전 수정)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - protocol_id 기반 디바이스 조회
 * - DatabaseAbstractionLayer 사용
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 * - 새로운 컬럼들 지원 (polling_interval, timeout, retry_count)
 */
class DeviceRepository : public IRepository<DeviceEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    DeviceRepository() : IRepository<DeviceEntity>("DeviceRepository") {
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("🏭 DeviceRepository initialized with protocol_id support");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
        }
    }
    
    virtual ~DeviceRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현
    // =======================================================================
    
    std::vector<DeviceEntity> findAll() override;
    std::optional<DeviceEntity> findById(int id) override;
    bool save(DeviceEntity& entity) override;
    bool update(const DeviceEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;

    // =======================================================================
    // 벌크 연산
    // =======================================================================
    
    std::vector<DeviceEntity> findByIds(const std::vector<int>& ids) override;
    
    std::vector<DeviceEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;

    // =======================================================================
    // Device 전용 메서드들 - protocol_id 기반으로 변경
    // =======================================================================
    
    /**
     * @brief protocol_id로 디바이스 조회 (파라미터 타입 변경)
     * @param protocol_id 프로토콜 ID (정수)
     * @return 해당 프로토콜의 디바이스 목록
     */
    std::vector<DeviceEntity> findByProtocol(int protocol_id);  // 🔥 파라미터 타입 변경
    
    std::vector<DeviceEntity> findByTenant(int tenant_id);
    std::vector<DeviceEntity> findBySite(int site_id);
    std::vector<DeviceEntity> findByEdgeServer(int edge_server_id); // 🔥 추가: 에지 서버 필터링
    std::vector<DeviceEntity> findEnabledDevices();
    
    /**
     * @brief protocol_id별로 디바이스 그룹핑 (메서드명 및 반환타입 변경)
     * @return protocol_id를 키로 하는 디바이스 그룹 맵
     */
    std::map<int, std::vector<DeviceEntity>> groupByProtocolId();  // 🔥 메서드명+반환타입 변경

    // =======================================================================
    // 벌크 연산 (DeviceSettingsRepository 패턴)
    // =======================================================================
    
    int saveBulk(std::vector<DeviceEntity>& entities);
    int updateBulk(const std::vector<DeviceEntity>& entities);
    int deleteByIds(const std::vector<int>& ids);

    // =======================================================================
    // 실시간 디바이스 관리
    // =======================================================================
    
    bool enableDevice(int device_id);
    bool disableDevice(int device_id);
    bool updateDeviceStatus(int device_id, bool is_enabled);
    bool updateEndpoint(int device_id, const std::string& endpoint);
    bool updateConfig(int device_id, const std::string& config);

    // =======================================================================
    // 통계 및 분석 - protocol_id 기반으로 변경
    // =======================================================================
    
    std::string getDeviceStatistics() const;
    std::vector<DeviceEntity> findInactiveDevices() const;
    
    /**
     * @brief 프로토콜별 디바이스 분포 (반환타입 변경)
     * @return protocol_id를 키로 하는 디바이스 개수 맵
     */
    std::map<int, int> getProtocolDistribution() const;  // 🔥 반환타입 변경

    // =======================================================================
    // 캐시 관리
    // =======================================================================
    
    void setCacheEnabled(bool enabled) override {
        IRepository<DeviceEntity>::setCacheEnabled(enabled);
    }
    
    bool isCacheEnabled() const override {
        return IRepository<DeviceEntity>::isCacheEnabled();
    }
    
    void clearCache() override {
        IRepository<DeviceEntity>::clearCache();
    }

    // =======================================================================
    // Worker용 최적화 메서드들 (DeviceSettingsRepository 패턴)
    // =======================================================================
    
    int getTotalCount();

private:
    // =======================================================================
    // 내부 헬퍼 메서드들 (DeviceSettingsRepository 패턴)
    // =======================================================================
    
    /**
     * @brief SQL 결과를 DeviceEntity로 변환 (새 컬럼 포함)
     * @param row SQL 결과 행
     * @return DeviceEntity
     */
    DeviceEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 여러 SQL 결과를 DeviceEntity 벡터로 변환
     * @param result SQL 결과
     * @return DeviceEntity 벡터
     */
    std::vector<DeviceEntity> mapResultToEntities(const std::vector<std::map<std::string, std::string>>& result);
    
    /**
     * @brief DeviceEntity를 SQL 파라미터 맵으로 변환 (새 컬럼 포함)
     * @param entity 엔티티
     * @return SQL 파라미터 맵
     */
    std::map<std::string, std::string> entityToParams(const DeviceEntity& entity);
    
    /**
     * @brief devices 테이블이 존재하는지 확인하고 없으면 생성
     * @return 성공 시 true
     */
    bool ensureTableExists();
    
    /**
     * @brief 디바이스 검증 (protocol_id 기반)
     * @param entity 검증할 디바이스 엔티티
     * @return 유효하면 true
     */
    bool validateDevice(const DeviceEntity& entity) const;
    
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // DEVICE_REPOSITORY_H