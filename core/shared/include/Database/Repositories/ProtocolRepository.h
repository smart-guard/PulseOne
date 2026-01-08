#ifndef PROTOCOL_REPOSITORY_H
#define PROTOCOL_REPOSITORY_H

/**
 * @file ProtocolRepository.h
 * @brief PulseOne ProtocolRepository - DeviceRepository 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-08-26
 * 
 * DeviceRepository와 동일한 패턴:
 * - DatabaseAbstractionLayer 사용
 * - executeQuery/executeNonQuery 패턴
 * - BaseEntity 상속 패턴 지원
 * - 캐싱 및 벌크 연산 지원
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/ProtocolEntity.h"
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

// 타입 별칭 정의
using ProtocolEntity = PulseOne::Database::Entities::ProtocolEntity;

/**
 * @brief Protocol Repository 클래스 - DeviceRepository 패턴 적용
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산
 * - protocol_type 기반 조회 (UNIQUE 컬럼)
 * - 카테고리별, 상태별 조회
 * - DatabaseAbstractionLayer 사용
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class ProtocolRepository : public IRepository<ProtocolEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    ProtocolRepository() : IRepository<ProtocolEntity>("ProtocolRepository") {
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("ProtocolRepository initialized with BaseEntity pattern");
            logger_->Info("Cache enabled: " + std::string(isCacheEnabled() ? "YES" : "NO"));
        }
    }
    
    virtual ~ProtocolRepository() = default;

    // =======================================================================
    // IRepository 인터페이스 구현
    // =======================================================================
    
    std::vector<ProtocolEntity> findAll() override;
    std::optional<ProtocolEntity> findById(int id) override;
    bool save(ProtocolEntity& entity) override;
    bool update(const ProtocolEntity& entity) override;
    bool deleteById(int id) override;
    bool exists(int id) override;

    // =======================================================================
    // 벌크 연산
    // =======================================================================
    
    std::vector<ProtocolEntity> findByIds(const std::vector<int>& ids) override;
    
    std::vector<ProtocolEntity> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt
    ) override;
    
    int countByConditions(const std::vector<QueryCondition>& conditions) override;

    // =======================================================================
    // Protocol 전용 메서드들
    // =======================================================================
    
    /**
     * @brief protocol_type으로 프로토콜 조회 (UNIQUE 컬럼)
     * @param protocol_type 프로토콜 타입 ("MODBUS_TCP", "MQTT" 등)
     * @return 프로토콜 엔티티 (optional)
     */
    std::optional<ProtocolEntity> findByType(const std::string& protocol_type);
    
    /**
     * @brief 카테고리별 프로토콜 조회
     * @param category 카테고리 ("industrial", "iot" 등)
     * @return 해당 카테고리의 프로토콜 목록
     */
    std::vector<ProtocolEntity> findByCategory(const std::string& category);
    
    /**
     * @brief 활성화된 프로토콜들만 조회
     * @return 활성화된 프로토콜 목록
     */
    std::vector<ProtocolEntity> findEnabledProtocols();
    
    /**
     * @brief 사용 중단되지 않은 프로토콜들 조회
     * @return 사용 중단되지 않은 프로토콜 목록
     */
    std::vector<ProtocolEntity> findActiveProtocols();
    
    /**
     * @brief 특정 연산을 지원하는 프로토콜들 조회
     * @param operation 연산 이름 ("read", "write", "subscribe" 등)
     * @return 해당 연산을 지원하는 프로토콜 목록
     */
    std::vector<ProtocolEntity> findByOperation(const std::string& operation);
    
    /**
     * @brief 특정 데이터 타입을 지원하는 프로토콜들 조회
     * @param data_type 데이터 타입 ("boolean", "float32" 등)
     * @return 해당 데이터 타입을 지원하는 프로토콜 목록
     */
    std::vector<ProtocolEntity> findByDataType(const std::string& data_type);
    
    /**
     * @brief 시리얼 통신 프로토콜들 조회
     * @return 시리얼 통신 프로토콜 목록
     */
    std::vector<ProtocolEntity> findSerialProtocols();
    
    /**
     * @brief 브로커 필요 프로토콜들 조회 (MQTT 등)
     * @return 브로커가 필요한 프로토콜 목록
     */
    std::vector<ProtocolEntity> findBrokerProtocols();
    
    /**
     * @brief 기본 포트별 프로토콜 조회
     * @param port 포트 번호
     * @return 해당 포트를 사용하는 프로토콜 목록
     */
    std::vector<ProtocolEntity> findByPort(int port);
    
    /**
     * @brief 카테고리별 프로토콜 그룹핑
     * @return 카테고리를 키로 하는 프로토콜 그룹 맵
     */
    std::map<std::string, std::vector<ProtocolEntity>> groupByCategory();

    // =======================================================================
    // 벌크 연산
    // =======================================================================
    
    int saveBulk(std::vector<ProtocolEntity>& entities);
    int updateBulk(const std::vector<ProtocolEntity>& entities);
    int deleteByIds(const std::vector<int>& ids);

    // =======================================================================
    // 통계 및 분석
    // =======================================================================
    
    std::string getProtocolStatistics() const;
    std::map<std::string, int> getCategoryDistribution() const;
    std::vector<ProtocolEntity> findDeprecatedProtocols() const;
    
    /**
     * @brief API용 프로토콜 목록 조회 (활성화된 것만)
     * @return API 응답용 프로토콜 목록
     */
    std::vector<std::map<std::string, std::string>> getApiProtocolList() const;

    // =======================================================================
    // 캐시 관리
    // =======================================================================
    
    void setCacheEnabled(bool enabled) override {
        IRepository<ProtocolEntity>::setCacheEnabled(enabled);
        if (logger_) {
            logger_->Info("ProtocolRepository cache " + std::string(enabled ? "enabled" : "disabled"));
        }
    }
    
    bool isCacheEnabled() const override {
        return IRepository<ProtocolEntity>::isCacheEnabled();
    }
    
    void clearCache() override {
        IRepository<ProtocolEntity>::clearCache();
        if (logger_) {
            logger_->Info("ProtocolRepository cache cleared");
        }
    }

    // =======================================================================
    // Worker용 최적화 메서드들
    // =======================================================================
    
    int getTotalCount();

private:
    // =======================================================================
    // 의존성 관리
    // =======================================================================
    
    DbLib::DatabaseManager* db_manager_;
    LogManager* logger_;
    
    void initializeDependencies() {
        db_manager_ = &DbLib::DatabaseManager::getInstance();
        logger_ = &LogManager::getInstance();
    }

    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief SQL 결과를 ProtocolEntity로 변환
     * @param row SQL 결과 행
     * @return ProtocolEntity
     */
    ProtocolEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief 여러 SQL 결과를 ProtocolEntity 벡터로 변환
     * @param result SQL 결과
     * @return ProtocolEntity 벡터
     */
    std::vector<ProtocolEntity> mapResultToEntities(const std::vector<std::map<std::string, std::string>>& result);
    
    /**
     * @brief ProtocolEntity를 SQL 파라미터 맵으로 변환
     * @param entity 엔티티
     * @return SQL 파라미터 맵
     */
    std::map<std::string, std::string> entityToParams(const ProtocolEntity& entity);
    
    /**
     * @brief protocols 테이블이 존재하는지 확인하고 없으면 생성
     * @return 성공 시 true
     */
    bool ensureTableExists();
    
    /**
     * @brief 프로토콜 검증
     * @param entity 검증할 프로토콜 엔티티
     * @return 유효하면 true
     */
    bool validateProtocol(const ProtocolEntity& entity) const;
    
    /**
     * @brief JSON 필드에서 특정 값 검색
     * @param json_field JSON 문자열 필드
     * @param search_value 검색할 값
     * @return 포함되어 있으면 true
     */
    bool jsonContains(const std::string& json_field, const std::string& search_value) const;
    
};

} // namespace Repositories
} // namespace Database
} // namespace PulseOne

#endif // PROTOCOL_REPOSITORY_H