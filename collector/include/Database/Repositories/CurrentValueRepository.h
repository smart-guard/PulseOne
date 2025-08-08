// =============================================================================
// collector/include/Database/Repositories/CurrentValueRepository.h
// 🔧 누락된 메서드 선언들 추가 - 컴파일 에러 수정
// =============================================================================

#ifndef CURRENT_VALUE_REPOSITORY_H
#define CURRENT_VALUE_REPOSITORY_H

/**
 * @file CurrentValueRepository.h
 * @brief PulseOne Current Value Repository - DataPointRepository 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-08-08
 * 
 * 🎯 DataPointRepository 패턴 완전 적용:
 * - IRepository<CurrentValueEntity> 상속으로 캐시 자동 획득
 * - INTEGER ID 기반 CRUD 연산 (point_id)
 * - SQLQueries.h 사용
 * - 업데이트된 스키마 적용
 */

#include "Database/Repositories/IRepository.h"
#include "Database/Entities/CurrentValueEntity.h"
#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include "Common/Enums.h"
#include <memory>
#include <map>
#include <string>
#include <mutex>
#include <vector>
#include <optional>
#include <chrono>

namespace PulseOne {
namespace Database {
namespace Repositories {

// 🔥 타입 별칭 정의 (DataPointRepository 패턴)
using CurrentValueEntity = PulseOne::Database::Entities::CurrentValueEntity;

/**
 * @brief Current Value Repository 클래스 (IRepository 상속으로 캐시 자동 획득)
 * 
 * 기능:
 * - INTEGER ID 기반 CRUD 연산 (point_id)
 * - Redis ↔ RDB 양방향 저장
 * - 실시간 데이터 버퍼링
 * - 주기적/변경감지 저장
 * - 캐싱 및 벌크 연산 지원 (IRepository에서 자동 제공)
 */
class CurrentValueRepository : public IRepository<CurrentValueEntity> {
public:
    // =======================================================================
    // 생성자 및 소멸자
    // =======================================================================
    
    CurrentValueRepository() : IRepository<CurrentValueEntity>("CurrentValueRepository") {
        // 🔥 의존성 초기화를 여기서 호출
        initializeDependencies();
        
        if (logger_) {
            logger_->Info("🏭 CurrentValueRepository initialized with IRepository caching system");
            logger_->Info("✅ Cache enabled: " + std::string(isCacheEnabled() ? "true" : "false"));
            logger_->Info("📊 Cache stats: " + std::to_string(getCacheStats().size()) + " entries tracked");
        }
    }
    
    virtual ~CurrentValueRepository() = default;

    // =======================================================================
    // 🎯 IRepository 필수 구현 메서드들 (CRUD)
    // =======================================================================
    
    /**
     * @brief 모든 현재값 조회
     * @return 모든 CurrentValueEntity 목록
     */
    std::vector<CurrentValueEntity> findAll() override;
    
    /**
     * @brief ID로 현재값 조회
     * @param id Point ID
     * @return CurrentValueEntity (optional)
     */
    std::optional<CurrentValueEntity> findById(int id) override;
    
    /**
     * @brief 현재값 저장/업데이트 (UPSERT)
     * @param entity CurrentValueEntity
     * @return 성공 시 true
     */
    bool save(CurrentValueEntity& entity) override;
    
    /**
     * @brief 현재값 업데이트
     * @param entity CurrentValueEntity
     * @return 성공 시 true
     */
    bool update(const CurrentValueEntity& entity) override;
    
    /**
     * @brief ID로 현재값 삭제
     * @param id Point ID
     * @return 성공 시 true
     */
    bool deleteById(int id) override;
    
    /**
     * @brief 현재값 존재 여부 확인
     * @param id Point ID
     * @return 존재하면 true
     */
    bool exists(int id) override;
    
    /**
     * @brief 여러 ID로 현재값들 조회
     * @param ids Point ID 목록
     * @return CurrentValueEntity 목록
     */
    std::vector<CurrentValueEntity> findByIds(const std::vector<int>& ids) override;

    // =======================================================================
    // 🔥 CurrentValue 전용 조회 메서드들
    // =======================================================================
    
    /**
     * @brief DataPoint ID로 현재값 조회 (호환성)
     * @param data_point_id DataPoint ID
     * @return CurrentValueEntity (optional)
     */
    std::optional<CurrentValueEntity> findByDataPointId(int data_point_id);
    
    /**
     * @brief 여러 DataPoint ID로 현재값들 조회 (호환성)
     * @param point_ids DataPoint ID 목록
     * @return CurrentValueEntity 목록
     */
    std::vector<CurrentValueEntity> findByDataPointIds(const std::vector<int>& point_ids);
    
    /**
     * @brief 품질별 현재값 조회
     * @param quality 데이터 품질 (GOOD, BAD, UNCERTAIN 등)
     * @return CurrentValueEntity 목록
     */
    std::vector<CurrentValueEntity> findByQuality(PulseOne::Enums::DataQuality quality);
    
    /**
     * @brief 오래된 현재값 조회
     * @param hours 시간 (시간 단위)
     * @return CurrentValueEntity 목록
     */
    std::vector<CurrentValueEntity> findStaleValues(int hours);
    
    /**
     * @brief 품질이 나쁜 현재값들 조회
     * @return CurrentValueEntity 목록
     */
    std::vector<CurrentValueEntity> findBadQualityValues();

    // =======================================================================
    // 🔥 새로 추가: 누락된 메서드 선언들
    // =======================================================================
    
    /**
     * @brief 값만 업데이트 (품질/통계는 그대로)
     * @param point_id Point ID
     * @param current_value 현재값 (JSON)
     * @param raw_value 원시값 (JSON)
     * @param value_type 값 타입
     * @return 성공 시 true
     */
    bool updateValueOnly(int point_id, const std::string& current_value, 
                        const std::string& raw_value, const std::string& value_type);
    
    /**
     * @brief 읽기 카운터 증가
     * @param point_id Point ID
     * @return 성공 시 true
     */
    bool incrementReadCount(int point_id);
    
    /**
     * @brief 쓰기 카운터 증가
     * @param point_id Point ID
     * @return 성공 시 true
     */
    bool incrementWriteCount(int point_id);
    
    /**
     * @brief 에러 카운터 증가
     * @param point_id Point ID
     * @return 성공 시 true
     */
    bool incrementErrorCount(int point_id);

    // =======================================================================
    // 🔥 캐시 관리 메서드들 (IRepository 오버라이드)
    // =======================================================================
    
    /**
     * @brief 캐시 활성화
     */
    void enableCache();
    
    /**
     * @brief 캐시 활성화 상태 확인
     * @return 활성화되어 있으면 true
     */
    bool isCacheEnabled() const override;
    
    /**
     * @brief 캐시 클리어
     */
    void clearCache() override;
    
    /**
     * @brief 특정 ID 캐시 클리어
     * @param id Point ID
     */
    void clearCacheForId(int id) override;
    
    /**
     * @brief 캐시 통계 조회
     * @return 캐시 통계 맵
     */
    std::map<std::string, int> getCacheStats() const override;
    
    /**
     * @brief 총 레코드 수 조회
     * @return 총 레코드 수
     */
    int getTotalCount() override;

private:
    // =======================================================================
    // 🔥 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief 테이블 존재 확인 및 생성
     * @return 성공 시 true
     */
    bool ensureTableExists();
    
    /**
     * @brief 행 데이터를 Entity로 변환
     * @param row 데이터베이스 행
     * @return CurrentValueEntity
     */
    CurrentValueEntity mapRowToEntity(const std::map<std::string, std::string>& row);
    
    /**
     * @brief CurrentValue 유효성 검사
     * @param entity CurrentValueEntity
     * @return 유효하면 true
     */
    bool validateCurrentValue(const CurrentValueEntity& entity) const;
    
    /**
     * @brief 의존성 초기화
     */
    void initializeDependencies() {
        logger_ = &LogManager::getInstance();
        config_manager_ = &ConfigManager::getInstance();
        db_manager_ = &DatabaseManager::getInstance();
    }
    
    // 멤버 변수들
    LogManager* logger_ = nullptr;
    ConfigManager* config_manager_ = nullptr;
    DatabaseManager* db_manager_ = nullptr;
};

} // namespace Repositories
} // namespace Database  
} // namespace PulseOne

#endif // CURRENT_VALUE_REPOSITORY_H