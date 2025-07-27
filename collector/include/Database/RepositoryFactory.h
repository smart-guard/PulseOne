#ifndef PULSEONE_REPOSITORY_FACTORY_H
#define PULSEONE_REPOSITORY_FACTORY_H

/**
 * @file RepositoryFactory.h
 * @brief PulseOne Repository 팩토리 (싱글톤)
 * @author PulseOne Development Team
 * @date 2025-07-26
 * 
 * Repository 인스턴스 중앙 관리:
 * - 싱글톤 패턴으로 전역 접근
 * - 모든 Repository 인스턴스 생성 및 관리
 * - DatabaseManager 의존성 자동 주입
 * - 캐싱 정책 중앙 제어
 */

#include "Database/Repositories/DeviceRepository.h"
// TODO: 향후 추가할 Repository들
// #include "Database/Repositories/DataPointRepository.h"
// #include "Database/Repositories/AlarmConfigRepository.h"
// #include "Database/Repositories/UserRepository.h"
// #include "Database/Repositories/TenantRepository.h"
// #include "Database/Repositories/SiteRepository.h"

#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include <memory>
#include <mutex>
#include <functional>

namespace PulseOne {
namespace Database {

/**
 * @brief Repository 팩토리 (싱글톤)
 * @details 모든 Repository 인스턴스를 중앙에서 관리
 */
class RepositoryFactory {
public:
    // =======================================================================
    // 싱글톤 패턴
    // =======================================================================
    
    /**
     * @brief 싱글톤 인스턴스 조회
     * @return RepositoryFactory 인스턴스
     */
    static RepositoryFactory& getInstance();
    
    /**
     * @brief 팩토리 초기화
     * @return 성공 시 true
     */
    bool initialize();
    
    /**
     * @brief 팩토리 종료 및 리소스 정리
     */
    void shutdown();

    // =======================================================================
    // Repository 인스턴스 조회
    // =======================================================================
    
    /**
     * @brief DeviceRepository 인스턴스 조회
     * @return DeviceRepository 참조
     */
    DeviceRepository& getDeviceRepository();
    
    // TODO: 향후 추가할 Repository들
    /*
    DataPointRepository& getDataPointRepository();
    AlarmConfigRepository& getAlarmConfigRepository();
    UserRepository& getUserRepository();
    TenantRepository& getTenantRepository();
    SiteRepository& getSiteRepository();
    */

    // =======================================================================
    // 전역 캐싱 제어
    // =======================================================================
    
    /**
     * @brief 모든 Repository 캐시 활성화/비활성화
     * @param enabled 캐시 사용 여부
     */
    void setCacheEnabled(bool enabled);
    
    /**
     * @brief 모든 Repository 캐시 삭제
     */
    void clearAllCaches();
    
    /**
     * @brief 전체 캐시 통계 조회
     * @return Repository별 캐시 통계
     */
    std::map<std::string, std::map<std::string, int>> getAllCacheStats();

    // =======================================================================
    // 성능 모니터링
    // =======================================================================
    
    /**
     * @brief 팩토리 상태 조회
     * @return 초기화 여부 및 상태 정보
     */
    bool isInitialized() const { return initialized_; }
    
    /**
     * @brief 활성 Repository 개수 조회
     * @return 생성된 Repository 인스턴스 수
     */
    int getActiveRepositoryCount() const;
    
    /**
     * @brief 팩토리 통계 조회
     * @return 생성 횟수, 에러 횟수 등
     */
    std::map<std::string, int> getFactoryStats() const;

    // =======================================================================
    // 트랜잭션 지원 (전역)
    // =======================================================================
    
    /**
     * @brief 글로벌 트랜잭션 시작
     * @return 성공 시 true
     */
    bool beginGlobalTransaction();
    
    /**
     * @brief 글로벌 트랜잭션 커밋
     * @return 성공 시 true
     */
    bool commitGlobalTransaction();
    
    /**
     * @brief 글로벌 트랜잭션 롤백
     * @return 성공 시 true
     */
    bool rollbackGlobalTransaction();
    
    /**
     * @brief 글로벌 트랜잭션 내에서 작업 실행
     * @param work 실행할 작업 (람다 함수)
     * @return 성공 시 true
     */
    bool executeInGlobalTransaction(std::function<bool()> work);

    // =======================================================================
    // 설정 관리
    // =======================================================================
    
    /**
     * @brief Repository 설정 리로드
     * @return 성공 시 true
     */
    bool reloadConfigurations();
    
    /**
     * @brief 캐시 TTL 설정
     * @param ttl_seconds TTL (초)
     */
    void setCacheTTL(int ttl_seconds);
    
    /**
     * @brief 최대 캐시 크기 설정
     * @param max_size 최대 캐시 항목 수
     */
    void setMaxCacheSize(int max_size);

private:
    // =======================================================================
    // 싱글톤 구현
    // =======================================================================
    
    /**
     * @brief 생성자 (private)
     */
    RepositoryFactory();
    
    /**
     * @brief 소멸자 (private)
     */
    ~RepositoryFactory();
    
    /**
     * @brief 복사 생성자 삭제
     */
    RepositoryFactory(const RepositoryFactory&) = delete;
    
    /**
     * @brief 대입 연산자 삭제
     */
    RepositoryFactory& operator=(const RepositoryFactory&) = delete;

    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief Repository 인스턴스 생성 및 초기화
     * @return 성공 시 true
     */
    bool createRepositoryInstances();
    
    /**
     * @brief Repository별 설정 적용
     */
    void applyRepositoryConfigurations();
    
    /**
     * @brief 의존성 주입 수행
     * @return 성공 시 true
     */
    bool injectDependencies();

private:
    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // 초기화 상태
    bool initialized_;
    mutable std::mutex factory_mutex_;
    
    // Repository 인스턴스들
    std::unique_ptr<DeviceRepository> device_repository_;
    // TODO: 향후 추가할 Repository들
    /*
    std::unique_ptr<DataPointRepository> data_point_repository_;
    std::unique_ptr<AlarmConfigRepository> alarm_config_repository_;
    std::unique_ptr<UserRepository> user_repository_;
    std::unique_ptr<TenantRepository> tenant_repository_;
    std::unique_ptr<SiteRepository> site_repository_;
    */
    
    // 의존성 참조
    DatabaseManager& db_manager_;
    ConfigManager& config_manager_;
    PulseOne::LogManager& logger_;
    
    // 팩토리 통계
    mutable int creation_count_;
    mutable int error_count_;
    mutable int transaction_count_;
    
    // 글로벌 설정
    bool global_cache_enabled_;
    int cache_ttl_seconds_;
    int max_cache_size_;
    bool transaction_active_;
};

} // namespace Database
} // namespace PulseOne

#endif // PULSEONE_REPOSITORY_FACTORY_H