/**
 * @file RepositoryFactory.h
 * @brief PulseOne Repository 팩토리 - Export Repository 추가
 * @author PulseOne Development Team
 * @date 2025-10-15
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
    class DeviceSettingsRepository;
    class CurrentValueRepository;
    class VirtualPointRepository;
    class SiteRepository;
    class TenantRepository;
    class UserRepository;
    class AlarmRuleRepository;
    class AlarmOccurrenceRepository;
    class ScriptLibraryRepository;
    class ProtocolRepository;
    
    // 🆕 Export 시스템 Repository들
    class ExportTargetRepository;
    class ExportTargetMappingRepository;
    class ExportLogRepository;
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

    std::shared_ptr<Repositories::DeviceSettingsRepository> getDeviceSettingsRepository() {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        if (!initialized_.load()) {
            throw std::runtime_error("RepositoryFactory not initialized");
        }
        creation_count_.fetch_add(1);
        return device_settings_repository_;
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
    
    std::shared_ptr<Repositories::AlarmRuleRepository> getAlarmRuleRepository() {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        if (!initialized_.load()) {
            throw std::runtime_error("RepositoryFactory not initialized");
        }
        creation_count_.fetch_add(1);
        return alarm_rule_repository_;
    }

    std::shared_ptr<Repositories::AlarmOccurrenceRepository> getAlarmOccurrenceRepository() {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        if (!initialized_.load()) {
            throw std::runtime_error("RepositoryFactory not initialized");
        }
        creation_count_.fetch_add(1);
        return alarm_occurrence_repository_;
    }

    std::shared_ptr<Repositories::ScriptLibraryRepository> getScriptLibraryRepository() {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        if (!initialized_.load()) {
            throw std::runtime_error("RepositoryFactory not initialized");
        }
        creation_count_.fetch_add(1);
        return script_library_repository_;
    }

    std::shared_ptr<Repositories::ProtocolRepository> getProtocolRepository() {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        if (!initialized_.load()) {
            throw std::runtime_error("RepositoryFactory not initialized");
        }
        creation_count_.fetch_add(1);
        return protocol_repository_;
    }

    // =============================================================================
    // 🆕 Export 시스템 Repository 접근자들
    // =============================================================================
    
    std::shared_ptr<Repositories::ExportTargetRepository> getExportTargetRepository() {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        if (!initialized_.load()) {
            throw std::runtime_error("RepositoryFactory not initialized");
        }
        creation_count_.fetch_add(1);
        return export_target_repository_;
    }
    
    std::shared_ptr<Repositories::ExportTargetMappingRepository> getExportTargetMappingRepository() {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        if (!initialized_.load()) {
            throw std::runtime_error("RepositoryFactory not initialized");
        }
        creation_count_.fetch_add(1);
        return export_target_mapping_repository_;
    }
    
    std::shared_ptr<Repositories::ExportLogRepository> getExportLogRepository() {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        if (!initialized_.load()) {
            throw std::runtime_error("RepositoryFactory not initialized");
        }
        creation_count_.fetch_add(1);
        return export_log_repository_;
    }

    // =============================================================================
    // 통계 및 디버깅
    // =============================================================================
    
    int getCreationCount() const { return creation_count_.load(); }
    int getErrorCount() const { return error_count_.load(); }
    
    // =============================================================================
    // 캐싱 제어
    // =============================================================================
    
    void setCacheEnabled(bool enabled);
    void clearAllCaches();
    void setCacheTTL(int ttl_seconds);
    void setMaxCacheSize(int max_size);

private:
    // =============================================================================
    // 싱글톤 생성자/소멸자
    // =============================================================================
    
    RepositoryFactory();
    ~RepositoryFactory();

    // =============================================================================
    // 내부 헬퍼 메서드들
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
    std::shared_ptr<Repositories::DeviceSettingsRepository> device_settings_repository_;
    std::shared_ptr<Repositories::UserRepository> user_repository_;
    std::shared_ptr<Repositories::TenantRepository> tenant_repository_;
    std::shared_ptr<Repositories::AlarmRuleRepository> alarm_rule_repository_;
    std::shared_ptr<Repositories::SiteRepository> site_repository_;
    std::shared_ptr<Repositories::VirtualPointRepository> virtual_point_repository_;
    std::shared_ptr<Repositories::CurrentValueRepository> current_value_repository_;
    std::shared_ptr<Repositories::AlarmOccurrenceRepository> alarm_occurrence_repository_;
    std::shared_ptr<Repositories::ScriptLibraryRepository> script_library_repository_;
    std::shared_ptr<Repositories::ProtocolRepository> protocol_repository_;
    
    // 🆕 Export 시스템 Repository들
    std::shared_ptr<Repositories::ExportTargetRepository> export_target_repository_;
    std::shared_ptr<Repositories::ExportTargetMappingRepository> export_target_mapping_repository_;
    std::shared_ptr<Repositories::ExportLogRepository> export_log_repository_;
    
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