#ifndef PULSEONE_REPOSITORY_FACTORY_H
#define PULSEONE_REPOSITORY_FACTORY_H

/**
 * @file RepositoryFactory.h
 * @brief PulseOne Repository 팩토리 (싱글톤) - 깃허브 기존 버전 + 타입 별칭
 * @author PulseOne Development Team
 * @date 2025-07-27
 * 
 * 🔥 네임스페이스 수정:
 * - DeviceRepository, DataPointRepository는 PulseOne::Database::Repositories 네임스페이스
 * - 타입 별칭으로 해결
 */

#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DataPointRepository.h"
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

// 🔥 타입 별칭 정의 (Repositories 네임스페이스 해결)
using DeviceRepository = PulseOne::Database::Repositories::DeviceRepository;
using DataPointRepository = PulseOne::Database::Repositories::DataPointRepository;
// TODO: 향후 추가할 Repository들
// using AlarmConfigRepository = PulseOne::Database::Repositories::AlarmConfigRepository;
// using UserRepository = PulseOne::Database::Repositories::UserRepository;
// using TenantRepository = PulseOne::Database::Repositories::TenantRepository;
// using SiteRepository = PulseOne::Database::Repositories::SiteRepository;

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
    // Repository 인스턴스 조회 (깃허브 기존 버전)
    // =======================================================================
    
    /**
     * @brief DeviceRepository 인스턴스 조회
     * @return DeviceRepository 참조
     */
    DeviceRepository& getDeviceRepository();
    
    /**
     * @brief DataPointRepository 인스턴스 조회
     * @return DataPointRepository 참조
     */
    DataPointRepository& getDataPointRepository();   
    
    // TODO: 향후 추가할 Repository들
    /*
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
    /**
     * @brief 통계 초기화  🔥 이 선언이 누락되어 있었음!
     */
    void resetStats();

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
    // 멤버 변수들 (깃허브 기존 버전)
    // =======================================================================
    
    // 초기화 상태
    bool initialized_;
    mutable std::mutex factory_mutex_;
    
    // Repository 인스턴스들 (타입 별칭 사용)
    std::unique_ptr<DeviceRepository> device_repository_;
    std::unique_ptr<DataPointRepository> data_point_repository_;

    // TODO: 향후 추가할 Repository들
    /*
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