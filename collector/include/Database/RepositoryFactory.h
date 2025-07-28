#ifndef PULSEONE_REPOSITORY_FACTORY_H
#define PULSEONE_REPOSITORY_FACTORY_H

/**
 * @file RepositoryFactory.h
 * @brief PulseOne Repository 팩토리 (싱글톤) - 모든 Repository 통합 관리 (CurrentValueRepository 추가)
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🔥 완전한 Repository 생태계:
 * - DeviceRepository, DataPointRepository (기존 완료)
 * - UserRepository, TenantRepository, AlarmConfigRepository (신규 추가)
 * - CurrentValueRepository (실시간 데이터 저장) 🆕
 * - IRepository 기반 통합 캐시 시스템
 */

#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/UserRepository.h"
#include "Database/Repositories/TenantRepository.h"
#include "Database/Repositories/AlarmConfigRepository.h"
#include "Database/Repositories/SiteRepository.h"
#include "Database/Repositories/VirtualPointRepository.h"
#include "Database/Repositories/CurrentValueRepository.h"  // 🆕 추가

#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include <memory>
#include <mutex>
#include <functional>
#include <map>
#include <vector>
#include <string>
#include <chrono>

namespace PulseOne {
namespace Database {

// 🔥 타입 별칭 정의 (Repositories 네임스페이스 해결)
using DeviceRepository = PulseOne::Database::Repositories::DeviceRepository;
using DataPointRepository = PulseOne::Database::Repositories::DataPointRepository;
using UserRepository = PulseOne::Database::Repositories::UserRepository;
using TenantRepository = PulseOne::Database::Repositories::TenantRepository;
using AlarmConfigRepository = PulseOne::Database::Repositories::AlarmConfigRepository;
using SiteRepository = PulseOne::Database::Repositories::SiteRepository;
using VirtualPointRepository = PulseOne::Database::Repositories::VirtualPointRepository;
using CurrentValueRepository = PulseOne::Database::Repositories::CurrentValueRepository;  // 🆕 추가

/**
 * @brief Repository 팩토리 (싱글톤)
 * @details 모든 Repository 인스턴스를 중앙에서 관리하며 통합 캐시 시스템 제공
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
    // Repository 인스턴스 조회 (완전한 라인업)
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
    
    /**
     * @brief UserRepository 인스턴스 조회
     * @return UserRepository 참조
     */
    UserRepository& getUserRepository();
    
    /**
     * @brief TenantRepository 인스턴스 조회
     * @return TenantRepository 참조
     */
    TenantRepository& getTenantRepository();
    
    /**
     * @brief AlarmConfigRepository 인스턴스 조회
     * @return AlarmConfigRepository 참조
     */
    AlarmConfigRepository& getAlarmConfigRepository();
    
    /**
     * @brief SiteRepository 인스턴스 조회
     * @return SiteRepository 참조
     */
    SiteRepository& getSiteRepository();
    
    /**
     * @brief VirtualPointRepository 인스턴스 조회
     * @return VirtualPointRepository 참조
     */
    VirtualPointRepository& getVirtualPointRepository();
    
    /**
     * @brief CurrentValueRepository 인스턴스 조회 🆕
     * @return CurrentValueRepository 참조
     */
    CurrentValueRepository& getCurrentValueRepository();

    // =======================================================================
    // 글로벌 트랜잭션 관리
    // =======================================================================
    
    /**
     * @brief 전역 트랜잭션 시작
     * @return 성공 시 true
     */
    bool beginGlobalTransaction();
    
    /**
     * @brief 전역 트랜잭션 커밋
     * @return 성공 시 true
     */
    bool commitGlobalTransaction();
    
    /**
     * @brief 전역 트랜잭션 롤백
     * @return 성공 시 true
     */
    bool rollbackGlobalTransaction();
    
    /**
     * @brief 트랜잭션 내에서 작업 실행
     * @param work 실행할 작업 함수
     * @return 성공 시 true
     */
    bool executeInGlobalTransaction(std::function<bool()> work);

    // =======================================================================
    // 캐싱 제어 (IRepository 통합 관리)
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
    
    /**
     * @brief 캐시 TTL 설정
     * @param ttl_seconds TTL (초)
     */
    void setCacheTTL(int ttl_seconds);
    
    /**
     * @brief 캐시 최대 크기 설정
     * @param max_size 최대 크기
     */
    void setMaxCacheSize(int max_size);

    // =======================================================================
    // 성능 모니터링 및 분석
    // =======================================================================
    
    /**
     * @brief 팩토리 통계 조회
     * @return 통계 정보 맵
     */
    std::map<std::string, int> getFactoryStats() const;
    
    /**
     * @brief 활성 Repository 수 조회
     * @return 활성 Repository 수
     */
    int getActiveRepositoryCount() const;
    
    /**
     * @brief 설정 다시 로드
     * @return 성공 시 true
     */
    bool reloadConfigurations();
    
    /**
     * @brief 통계 초기화
     */
    void resetStats();

    // =======================================================================
    // 복사 및 이동 제한 (싱글톤)
    // =======================================================================
    
    RepositoryFactory(const RepositoryFactory&) = delete;
    RepositoryFactory& operator=(const RepositoryFactory&) = delete;
    RepositoryFactory(RepositoryFactory&&) = delete;
    RepositoryFactory& operator=(RepositoryFactory&&) = delete;

private:
    // =======================================================================
    // 생성자 및 소멸자 (private)
    // =======================================================================
    
    RepositoryFactory();
    ~RepositoryFactory();

    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // 기본 시스템 컴포넌트들 (참조 방식)
    DatabaseManager& db_manager_;
    ConfigManager& config_manager_;
    PulseOne::LogManager& logger_;
    
    // Repository 인스턴스들
    std::unique_ptr<DeviceRepository> device_repository_;
    std::unique_ptr<DataPointRepository> data_point_repository_;
    std::unique_ptr<UserRepository> user_repository_;
    std::unique_ptr<TenantRepository> tenant_repository_;
    std::unique_ptr<AlarmConfigRepository> alarm_config_repository_;
    std::unique_ptr<SiteRepository> site_repository_;
    std::unique_ptr<VirtualPointRepository> virtual_point_repository_;
    std::unique_ptr<CurrentValueRepository> current_value_repository_;  // 🆕 추가
    
    // 동기화 및 상태 관리
    mutable std::mutex factory_mutex_;
    bool initialized_;
    
    // 캐시 관리
    bool global_cache_enabled_;
    int cache_ttl_seconds_;
    int max_cache_size_;
    
    // 트랜잭션 관리
    bool transaction_active_;
    
    // 성능 모니터링
    int creation_count_;
    int error_count_;
    int transaction_count_;

    // =======================================================================
    // 내부 헬퍼 메서드들
    // =======================================================================
    
    /**
     * @brief Repository 인스턴스 생성
     * @return 성공 시 true
     */
    bool createRepositoryInstances();
    
    /**
     * @brief Repository 설정 적용
     */
    void applyRepositoryConfigurations();
    
    /**
     * @brief 의존성 주입
     * @return 성공 시 true
     */
    bool injectDependencies();
};

} // namespace Database
} // namespace PulseOne

#endif // PULSEONE_REPOSITORY_FACTORY_H