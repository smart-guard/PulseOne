/**
 * @file RepositoryFactory.h
 * @brief PulseOne Repository 팩토리 - 간단 수정본
 * @author PulseOne Development Team
 * @date 2025-07-30
 */

#ifndef PULSEONE_REPOSITORY_FACTORY_H
#define PULSEONE_REPOSITORY_FACTORY_H

#include <memory>
#include <string>
#include <mutex>
#include <atomic>

// Forward declarations
class ConfigManager;
class LogManager;
class DatabaseManager;

namespace PulseOne {
namespace Database {

namespace Repositories {
    class DeviceRepository;
    class DataPointRepository;
    class CurrentValueRepository;
    class VirtualPointRepository;
    class SiteRepository;
    class TenantRepository;
    class UserRepository;
    class AlarmConfigRepository;
}

/**
 * @brief Repository 팩토리 클래스 (싱글톤)
 */
class RepositoryFactory {
public:
    // =============================================================================
    // 싱글톤 패턴
    // =============================================================================
    
    static RepositoryFactory& getInstance();
    
    // 복사 및 이동 방지
    RepositoryFactory(const RepositoryFactory&) = delete;
    RepositoryFactory& operator=(const RepositoryFactory&) = delete;
    RepositoryFactory(RepositoryFactory&&) = delete;
    RepositoryFactory& operator=(RepositoryFactory&&) = delete;

    // =============================================================================
    // 생명주기 관리
    // =============================================================================
    
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized_.load(); }

    // =============================================================================
    // Repository 접근자들 - shared_ptr 반환
    // =============================================================================
    
    std::shared_ptr<Repositories::DeviceRepository> getDeviceRepository() {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        if (!initialized_.load()) {
            throw std::runtime_error("RepositoryFactory not initialized");
        }
        creation_count_.fetch_add(1);
        return device_repository_;
    }
    
    std::shared_ptr<Repositories::DataPointRepository> getDataPointRepository() {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        if (!initialized_.load()) {
            throw std::runtime_error("RepositoryFactory not initialized");
        }
        creation_count_.fetch_add(1);
        return data_point_repository_;
    }
    
    std::shared_ptr<Repositories::CurrentValueRepository> getCurrentValueRepository() {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        if (!initialized_.load()) {
            throw std::runtime_error("RepositoryFactory not initialized");
        }
        creation_count_.fetch_add(1);
        return current_value_repository_;
    }
    
    std::shared_ptr<Repositories::VirtualPointRepository> getVirtualPointRepository() {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        if (!initialized_.load()) {
            throw std::runtime_error("RepositoryFactory not initialized");
        }
        creation_count_.fetch_add(1);
        return virtual_point_repository_;
    }
    
    std::shared_ptr<Repositories::SiteRepository> getSiteRepository() {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        if (!initialized_.load()) {
            throw std::runtime_error("RepositoryFactory not initialized");
        }
        creation_count_.fetch_add(1);  
        return site_repository_;
    }
    
    std::shared_ptr<Repositories::TenantRepository> getTenantRepository() {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        if (!initialized_.load()) {
            throw std::runtime_error("RepositoryFactory not initialized");
        }
        creation_count_.fetch_add(1);
        return tenant_repository_;
    }
    
    std::shared_ptr<Repositories::UserRepository> getUserRepository() {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        if (!initialized_.load()) {
            throw std::runtime_error("RepositoryFactory not initialized");
        }
        creation_count_.fetch_add(1);
        return user_repository_;
    }
    
    std::shared_ptr<Repositories::AlarmConfigRepository> getAlarmConfigRepository() {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        if (!initialized_.load()) {
            throw std::runtime_error("RepositoryFactory not initialized");
        }
        creation_count_.fetch_add(1);
        return alarm_config_repository_;
    }

    // =============================================================================
    // 캐시 관리
    // =============================================================================
    
    void setCacheEnabled(bool enabled);
    void clearAllCaches();
    void setCacheTTL(int ttl_seconds);
    void setMaxCacheSize(int max_size);

    // =============================================================================
    // 통계 및 상태
    // =============================================================================
    
    int getCreationCount() const { return creation_count_.load(); }
    int getErrorCount() const { return error_count_.load(); }

private:
    // =============================================================================
    // 생성자 및 소멸자 (private - 싱글톤)
    // =============================================================================
    
    RepositoryFactory();
    ~RepositoryFactory();

    // =============================================================================
    // 내부 메서드들
    // =============================================================================
    
    bool createRepositoryInstances();
    void applyRepositoryConfigurations();
    bool injectDependencies();
    void connectRepositoryDependencies();
    // =============================================================================
    // 데이터 멤버들
    // =============================================================================
    
    // 외부 의존성들 (포인터)
    DatabaseManager* db_manager_;
    ConfigManager* config_manager_;
    LogManager* logger_;
    
    // Repository 인스턴스들 (shared_ptr로 직접 관리)
    std::shared_ptr<Repositories::DeviceRepository> device_repository_;
    std::shared_ptr<Repositories::DataPointRepository> data_point_repository_;
    std::shared_ptr<Repositories::UserRepository> user_repository_;
    std::shared_ptr<Repositories::TenantRepository> tenant_repository_;
    std::shared_ptr<Repositories::AlarmConfigRepository> alarm_config_repository_;
    std::shared_ptr<Repositories::SiteRepository> site_repository_;
    std::shared_ptr<Repositories::VirtualPointRepository> virtual_point_repository_;
    std::shared_ptr<Repositories::CurrentValueRepository> current_value_repository_;
    
    // 상태 관리
    std::atomic<bool> initialized_{false};
    mutable std::mutex factory_mutex_;
    
    // 캐시 설정
    bool global_cache_enabled_{true};
    int cache_ttl_seconds_{300};
    int max_cache_size_{1000};

    mutable std::atomic<int> creation_count_{0};
    mutable std::atomic<int> error_count_{0};
    mutable std::atomic<int> transaction_count_{0};
    std::atomic<bool> transaction_active_{false};
};
    
} // namespace Database
} // namespace PulseOne

#endif // PULSEONE_REPOSITORY_FACTORY_H