#ifndef PULSEONE_REPOSITORY_FACTORY_H
#define PULSEONE_REPOSITORY_FACTORY_H

/**
 * @file RepositoryFactory.h
 * @brief PulseOne Repository 팩토리 (싱글톤) - 멤버 변수 타입 수정
 * @author PulseOne Development Team
 * @date 2025-07-29
 * 
 * 🔥 주요 수정사항:
 * - 멤버 변수를 참조(&)에서 포인터(*)로 변경
 * - cpp 구현과 일치하도록 수정
 * - transaction_count_ 멤버 변수 추가 (누락되어 있었음)
 */

#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/UserRepository.h"
#include "Database/Repositories/TenantRepository.h"
#include "Database/Repositories/AlarmConfigRepository.h"
#include "Database/Repositories/SiteRepository.h"
#include "Database/Repositories/VirtualPointRepository.h"
#include "Database/Repositories/CurrentValueRepository.h"

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

// 타입 별칭 정의
using DeviceRepository = PulseOne::Database::Repositories::DeviceRepository;
using DataPointRepository = PulseOne::Database::Repositories::DataPointRepository;
using UserRepository = PulseOne::Database::Repositories::UserRepository;
using TenantRepository = PulseOne::Database::Repositories::TenantRepository;
using AlarmConfigRepository = PulseOne::Database::Repositories::AlarmConfigRepository;
using SiteRepository = PulseOne::Database::Repositories::SiteRepository;
using VirtualPointRepository = PulseOne::Database::Repositories::VirtualPointRepository;
using CurrentValueRepository = PulseOne::Database::Repositories::CurrentValueRepository;

class RepositoryFactory {
public:
    // 싱글톤 패턴
    static RepositoryFactory& getInstance();
    bool initialize();
    void shutdown();

    // Repository 인스턴스 조회
    DeviceRepository& getDeviceRepository();
    DataPointRepository& getDataPointRepository();   
    UserRepository& getUserRepository();
    TenantRepository& getTenantRepository();
    AlarmConfigRepository& getAlarmConfigRepository();
    SiteRepository& getSiteRepository();
    VirtualPointRepository& getVirtualPointRepository();
    CurrentValueRepository& getCurrentValueRepository();

    // 글로벌 트랜잭션 관리
    bool beginGlobalTransaction();
    bool commitGlobalTransaction();
    bool rollbackGlobalTransaction();
    bool executeInGlobalTransaction(std::function<bool()> work);

    // 캐싱 제어
    void setCacheEnabled(bool enabled);
    void clearAllCaches();
    std::map<std::string, std::map<std::string, int>> getAllCacheStats();
    void setCacheTTL(int ttl_seconds);
    void setMaxCacheSize(int max_size);

    // 성능 모니터링
    std::map<std::string, int> getFactoryStats() const;
    int getActiveRepositoryCount() const;
    bool reloadConfigurations();
    void resetStats();

    // 복사 및 이동 제한 (싱글톤)
    RepositoryFactory(const RepositoryFactory&) = delete;
    RepositoryFactory& operator=(const RepositoryFactory&) = delete;
    RepositoryFactory(RepositoryFactory&&) = delete;
    RepositoryFactory& operator=(RepositoryFactory&&) = delete;

private:
    RepositoryFactory();
    ~RepositoryFactory();

    // =======================================================================
    // 🔥 멤버 변수들 - 포인터 타입으로 수정!
    // =======================================================================
    
    // 기본 시스템 컴포넌트들 (포인터 방식으로 변경)
    DatabaseManager* db_manager_;        // & → * 변경
    ConfigManager* config_manager_;      // & → * 변경  
    LogManager* logger_;       // & → * 변경
    
    // Repository 인스턴스들
    std::unique_ptr<DeviceRepository> device_repository_;
    std::unique_ptr<DataPointRepository> data_point_repository_;
    std::unique_ptr<UserRepository> user_repository_;
    std::unique_ptr<TenantRepository> tenant_repository_;
    std::unique_ptr<AlarmConfigRepository> alarm_config_repository_;
    std::unique_ptr<SiteRepository> site_repository_;
    std::unique_ptr<VirtualPointRepository> virtual_point_repository_;
    std::unique_ptr<CurrentValueRepository> current_value_repository_;
    
    // 동기화 및 상태 관리
    mutable std::mutex factory_mutex_;
    bool initialized_;
    
    // 캐시 관리
    bool global_cache_enabled_;
    int cache_ttl_seconds_;
    int max_cache_size_;
    
    // 트랜잭션 관리
    bool transaction_active_;
    
    // 성능 모니터링 (🔥 transaction_count_ 추가!)
    int creation_count_;
    int error_count_;
    int transaction_count_;  // 이게 빠져있었음!

    // 내부 헬퍼 메서드들
    bool createRepositoryInstances();
    void applyRepositoryConfigurations();
    bool injectDependencies();
};

} // namespace Database
} // namespace PulseOne

#endif // PULSEONE_REPOSITORY_FACTORY_H