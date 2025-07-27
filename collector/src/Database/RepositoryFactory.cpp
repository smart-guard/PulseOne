// =============================================================================
// collector/src/Database/RepositoryFactory.cpp
// PulseOne Repository 팩토리 구현 - 싱글톤 패턴
// =============================================================================

#include "Database/RepositoryFactory.h"
#include "Common/Constants.h"
#include <stdexcept>

using namespace PulseOne::Constants;

namespace PulseOne {
namespace Database {

// =============================================================================
// 싱글톤 구현
// =============================================================================

RepositoryFactory::RepositoryFactory()
    : initialized_(false)
    , db_manager_(DatabaseManager::getInstance())
    , config_manager_(ConfigManager::getInstance())
    , logger_(PulseOne::LogManager::getInstance())
    , creation_count_(0)
    , error_count_(0)
    , transaction_count_(0)
    , global_cache_enabled_(true)
    , cache_ttl_seconds_(300)  // 5분 기본값
    , max_cache_size_(1000)
    , transaction_active_(false) {
    
    logger_.Info("🏭 RepositoryFactory created");
}

RepositoryFactory::~RepositoryFactory() {
    shutdown();
    logger_.Info("🏭 RepositoryFactory destroyed");
}

RepositoryFactory& RepositoryFactory::getInstance() {
    static RepositoryFactory instance;
    return instance;
}

// =============================================================================
// 초기화 및 종료
// =============================================================================

bool RepositoryFactory::initialize() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (initialized_) {
        logger_.Warn("RepositoryFactory already initialized");
        return true;
    }
    
    try {
        logger_.Info("🔧 RepositoryFactory initializing...");
        
        // 1. DatabaseManager 초기화 확인
        if (!db_manager_.isPostgresConnected() && !db_manager_.isSQLiteConnected()) {
            logger_.Error("DatabaseManager not connected - attempting initialization");
            if (!db_manager_.initialize()) {
                logger_.Error("Failed to initialize DatabaseManager");
                error_count_++;
                return false;
            }
        }
        
        // 2. Repository 인스턴스 생성
        if (!createRepositoryInstances()) {
            logger_.Error("Failed to create repository instances");
            error_count_++;
            return false;
        }
        
        // 3. 의존성 주입
        if (!injectDependencies()) {
            logger_.Error("Failed to inject dependencies");
            error_count_++;
            return false;
        }
        
        // 4. Repository별 설정 적용
        applyRepositoryConfigurations();
        
        initialized_ = true;
        logger_.Info("✅ RepositoryFactory initialized successfully");
        
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("RepositoryFactory::initialize failed: " + std::string(e.what()));
        error_count_++;
        return false;
    }
}

void RepositoryFactory::shutdown() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (!initialized_) {
        return;
    }
    
    try {
        logger_.Info("🔧 RepositoryFactory shutting down...");
        
        // 활성 트랜잭션이 있으면 롤백
        if (transaction_active_) {
            logger_.Warn("Active transaction found during shutdown - rolling back");
            rollbackGlobalTransaction();
        }
        
        // 모든 캐시 클리어
        clearAllCaches();
        
        // Repository 인스턴스 해제
        device_repository_.reset();
        // TODO: 향후 추가할 Repository들
        /*
        data_point_repository_.reset();
        alarm_config_repository_.reset();
        user_repository_.reset();
        tenant_repository_.reset();
        site_repository_.reset();
        */
        
        initialized_ = false;
        logger_.Info("✅ RepositoryFactory shutdown completed");
        
    } catch (const std::exception& e) {
        logger_.Error("RepositoryFactory::shutdown failed: " + std::string(e.what()));
        error_count_++;
    }
}

// =============================================================================
// Repository 인스턴스 조회
// =============================================================================

DeviceRepository& RepositoryFactory::getDeviceRepository() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (!initialized_) {
        throw std::runtime_error("RepositoryFactory not initialized");
    }
    
    if (!device_repository_) {
        throw std::runtime_error("DeviceRepository not created");
    }
    
    creation_count_++;
    return *device_repository_;
}

// TODO: 향후 추가할 Repository들
/*
DataPointRepository& RepositoryFactory::getDataPointRepository() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (!initialized_) {
        throw std::runtime_error("RepositoryFactory not initialized");
    }
    
    if (!data_point_repository_) {
        throw std::runtime_error("DataPointRepository not created");
    }
    
    creation_count_++;
    return *data_point_repository_;
}

AlarmConfigRepository& RepositoryFactory::getAlarmConfigRepository() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (!initialized_) {
        throw std::runtime_error("RepositoryFactory not initialized");
    }
    
    if (!alarm_config_repository_) {
        throw std::runtime_error("AlarmConfigRepository not created");
    }
    
    creation_count_++;
    return *alarm_config_repository_;
}

UserRepository& RepositoryFactory::getUserRepository() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (!initialized_) {
        throw std::runtime_error("RepositoryFactory not initialized");
    }
    
    if (!user_repository_) {
        throw std::runtime_error("UserRepository not created");
    }
    
    creation_count_++;
    return *user_repository_;
}

TenantRepository& RepositoryFactory::getTenantRepository() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (!initialized_) {
        throw std::runtime_error("RepositoryFactory not initialized");
    }
    
    if (!tenant_repository_) {
        throw std::runtime_error("TenantRepository not created");
    }
    
    creation_count_++;
    return *tenant_repository_;
}

SiteRepository& RepositoryFactory::getSiteRepository() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (!initialized_) {
        throw std::runtime_error("RepositoryFactory not initialized");
    }
    
    if (!site_repository_) {
        throw std::runtime_error("SiteRepository not created");
    }
    
    creation_count_++;
    return *site_repository_;
}
*/

// =============================================================================
// 전역 캐싱 제어
// =============================================================================

void RepositoryFactory::setCacheEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    global_cache_enabled_ = enabled;
    
    if (initialized_) {
        // 모든 Repository에 캐시 설정 적용
        if (device_repository_) {
            device_repository_->setCacheEnabled(enabled);
        }
        
        // TODO: 향후 추가할 Repository들
        /*
        if (data_point_repository_) {
            data_point_repository_->setCacheEnabled(enabled);
        }
        if (alarm_config_repository_) {
            alarm_config_repository_->setCacheEnabled(enabled);
        }
        if (user_repository_) {
            user_repository_->setCacheEnabled(enabled);
        }
        if (tenant_repository_) {
            tenant_repository_->setCacheEnabled(enabled);
        }
        if (site_repository_) {
            site_repository_->setCacheEnabled(enabled);
        }
        */
    }
    
    logger_.Info("Global cache " + std::string(enabled ? "enabled" : "disabled"));
}

void RepositoryFactory::clearAllCaches() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (!initialized_) {
        return;
    }
    
    int total_cleared = 0;
    
    try {
        // 모든 Repository 캐시 클리어
        if (device_repository_) {
            auto stats = device_repository_->getCacheStats();
            total_cleared += stats["size"];
            device_repository_->clearCache();
        }
        
        // TODO: 향후 추가할 Repository들
        /*
        if (data_point_repository_) {
            auto stats = data_point_repository_->getCacheStats();
            total_cleared += stats["size"];
            data_point_repository_->clearCache();
        }
        if (alarm_config_repository_) {
            auto stats = alarm_config_repository_->getCacheStats();
            total_cleared += stats["size"];
            alarm_config_repository_->clearCache();
        }
        if (user_repository_) {
            auto stats = user_repository_->getCacheStats();
            total_cleared += stats["size"];
            user_repository_->clearCache();
        }
        if (tenant_repository_) {
            auto stats = tenant_repository_->getCacheStats();
            total_cleared += stats["size"];
            tenant_repository_->clearCache();
        }
        if (site_repository_) {
            auto stats = site_repository_->getCacheStats();
            total_cleared += stats["size"];
            site_repository_->clearCache();
        }
        */
        
        logger_.Info("Cleared all caches - " + std::to_string(total_cleared) + " entries removed");
        
    } catch (const std::exception& e) {
        logger_.Error("RepositoryFactory::clearAllCaches failed: " + std::string(e.what()));
        error_count_++;
    }
}

std::map<std::string, std::map<std::string, int>> RepositoryFactory::getAllCacheStats() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    std::map<std::string, std::map<std::string, int>> all_stats;
    
    if (!initialized_) {
        return all_stats;
    }
    
    try {
        // 각 Repository의 캐시 통계 수집
        if (device_repository_) {
            all_stats["DeviceRepository"] = device_repository_->getCacheStats();
        }
        
        // TODO: 향후 추가할 Repository들
        /*
        if (data_point_repository_) {
            all_stats["DataPointRepository"] = data_point_repository_->getCacheStats();
        }
        if (alarm_config_repository_) {
            all_stats["AlarmConfigRepository"] = alarm_config_repository_->getCacheStats();
        }
        if (user_repository_) {
            all_stats["UserRepository"] = user_repository_->getCacheStats();
        }
        if (tenant_repository_) {
            all_stats["TenantRepository"] = tenant_repository_->getCacheStats();
        }
        if (site_repository_) {
            all_stats["SiteRepository"] = site_repository_->getCacheStats();
        }
        */
        
    } catch (const std::exception& e) {
        logger_.Error("RepositoryFactory::getAllCacheStats failed: " + std::string(e.what()));
        error_count_++;
    }
    
    return all_stats;
}

// =============================================================================
// 성능 모니터링
// =============================================================================

int RepositoryFactory::getActiveRepositoryCount() const {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    int count = 0;
    
    if (device_repository_) count++;
    // TODO: 향후 추가할 Repository들
    /*
    if (data_point_repository_) count++;
    if (alarm_config_repository_) count++;
    if (user_repository_) count++;
    if (tenant_repository_) count++;
    if (site_repository_) count++;
    */
    
    return count;
}

std::map<std::string, int> RepositoryFactory::getFactoryStats() const {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    std::map<std::string, int> stats;
    
    stats["initialized"] = initialized_ ? 1 : 0;
    stats["active_repositories"] = getActiveRepositoryCount();
    stats["creation_count"] = creation_count_;
    stats["error_count"] = error_count_;
    stats["transaction_count"] = transaction_count_;
    stats["cache_enabled"] = global_cache_enabled_ ? 1 : 0;
    stats["cache_ttl_seconds"] = cache_ttl_seconds_;
    stats["max_cache_size"] = max_cache_size_;
    stats["transaction_active"] = transaction_active_ ? 1 : 0;
    
    return stats;
}

// =============================================================================
// 트랜잭션 지원 (전역)
// =============================================================================

bool RepositoryFactory::beginGlobalTransaction() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (!initialized_) {
        logger_.Error("RepositoryFactory not initialized for transaction");
        return false;
    }
    
    if (transaction_active_) {
        logger_.Warn("Global transaction already active");
        return true;
    }
    
    try {
        // PostgreSQL 트랜잭션 시작 (현재 PostgreSQL만 지원)
        std::string db_type = config_manager_.getOrDefault("DATABASE_TYPE", "SQLITE");
        
        if (db_type == "POSTGRESQL") {
            bool success = db_manager_.executeNonQueryPostgres("BEGIN");
            if (success) {
                transaction_active_ = true;
                transaction_count_++;
                logger_.Info("Global transaction started");
                return true;
            }
        } else {
            logger_.Warn("Global transactions not supported for " + db_type);
            return false;
        }
        
    } catch (const std::exception& e) {
        logger_.Error("RepositoryFactory::beginGlobalTransaction failed: " + std::string(e.what()));
        error_count_++;
    }
    
    return false;
}

bool RepositoryFactory::commitGlobalTransaction() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (!transaction_active_) {
        logger_.Warn("No active global transaction to commit");
        return true;
    }
    
    try {
        std::string db_type = config_manager_.getOrDefault("DATABASE_TYPE", "SQLITE");
        
        if (db_type == "POSTGRESQL") {
            bool success = db_manager_.executeNonQueryPostgres("COMMIT");
            transaction_active_ = false;
            
            if (success) {
                logger_.Info("Global transaction committed");
                return true;
            } else {
                logger_.Error("Failed to commit global transaction");
                error_count_++;
                return false;
            }
        }
        
    } catch (const std::exception& e) {
        logger_.Error("RepositoryFactory::commitGlobalTransaction failed: " + std::string(e.what()));
        transaction_active_ = false;
        error_count_++;
    }
    
    return false;
}

bool RepositoryFactory::rollbackGlobalTransaction() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (!transaction_active_) {
        logger_.Warn("No active global transaction to rollback");
        return true;
    }
    
    try {
        std::string db_type = config_manager_.getOrDefault("DATABASE_TYPE", "SQLITE");
        
        if (db_type == "POSTGRESQL") {
            bool success = db_manager_.executeNonQueryPostgres("ROLLBACK");
            transaction_active_ = false;
            
            if (success) {
                logger_.Info("Global transaction rolled back");
                return true;
            } else {
                logger_.Error("Failed to rollback global transaction");
                error_count_++;
                return false;
            }
        }
        
    } catch (const std::exception& e) {
        logger_.Error("RepositoryFactory::rollbackGlobalTransaction failed: " + std::string(e.what()));
        transaction_active_ = false;
        error_count_++;
    }
    
    return false;
}

bool RepositoryFactory::executeInGlobalTransaction(std::function<bool()> work) {
    if (!beginGlobalTransaction()) {
        return false;
    }
    
    try {
        if (work()) {
            return commitGlobalTransaction();
        } else {
            rollbackGlobalTransaction();
            return false;
        }
    } catch (const std::exception& e) {
        logger_.Error("RepositoryFactory::executeInGlobalTransaction work failed: " + std::string(e.what()));
        rollbackGlobalTransaction();
        error_count_++;
        return false;
    }
}

// =============================================================================
// 설정 관리
// =============================================================================

bool RepositoryFactory::reloadConfigurations() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (!initialized_) {
        logger_.Warn("RepositoryFactory not initialized for configuration reload");
        return false;
    }
    
    try {
        logger_.Info("Reloading repository configurations...");
        
        // ConfigManager에서 최신 설정 로드
        cache_ttl_seconds_ = std::stoi(config_manager_.getOrDefault("REPOSITORY_CACHE_TTL_SECONDS", "300"));
        max_cache_size_ = std::stoi(config_manager_.getOrDefault("REPOSITORY_MAX_CACHE_SIZE", "1000"));
        global_cache_enabled_ = (config_manager_.getOrDefault("REPOSITORY_CACHE_ENABLED", "true") == "true");
        
        // Repository별 설정 적용
        applyRepositoryConfigurations();
        
        logger_.Info("Repository configurations reloaded successfully");
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("RepositoryFactory::reloadConfigurations failed: " + std::string(e.what()));
        error_count_++;
        return false;
    }
}

void RepositoryFactory::setCacheTTL(int ttl_seconds) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    cache_ttl_seconds_ = ttl_seconds;
    
    logger_.Info("Cache TTL set to " + std::to_string(ttl_seconds) + " seconds");
    
    // Repository별 설정 적용 (실제 구현에서는 각 Repository에 TTL 설정 메서드 필요)
    // applyRepositoryConfigurations();
}

void RepositoryFactory::setMaxCacheSize(int max_size) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    max_cache_size_ = max_size;
    
    logger_.Info("Max cache size set to " + std::to_string(max_size) + " entries");
    
    // Repository별 설정 적용 (실제 구현에서는 각 Repository에 크기 설정 메서드 필요)
    // applyRepositoryConfigurations();
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

bool RepositoryFactory::createRepositoryInstances() {
    try {
        logger_.Info("Creating repository instances...");
        
        // DeviceRepository 생성
        device_repository_ = std::make_unique<DeviceRepository>();
        if (!device_repository_) {
            logger_.Error("Failed to create DeviceRepository");
            return false;
        }
        
        // TODO: 향후 추가할 Repository들
        /*
        data_point_repository_ = std::make_unique<DataPointRepository>();
        if (!data_point_repository_) {
            logger_.Error("Failed to create DataPointRepository");
            return false;
        }
        
        alarm_config_repository_ = std::make_unique<AlarmConfigRepository>();
        if (!alarm_config_repository_) {
            logger_.Error("Failed to create AlarmConfigRepository");
            return false;
        }
        
        user_repository_ = std::make_unique<UserRepository>();
        if (!user_repository_) {
            logger_.Error("Failed to create UserRepository");
            return false;
        }
        
        tenant_repository_ = std::make_unique<TenantRepository>();
        if (!tenant_repository_) {
            logger_.Error("Failed to create TenantRepository");
            return false;
        }
        
        site_repository_ = std::make_unique<SiteRepository>();
        if (!site_repository_) {
            logger_.Error("Failed to create SiteRepository");
            return false;
        }
        */
        
        logger_.Info("All repository instances created successfully");
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("RepositoryFactory::createRepositoryInstances failed: " + std::string(e.what()));
        error_count_++;
        return false;
    }
}

void RepositoryFactory::applyRepositoryConfigurations() {
    try {
        logger_.Info("Applying repository configurations...");
        
        // 전역 캐시 설정 적용
        setCacheEnabled(global_cache_enabled_);
        
        // 개별 Repository별 설정 (필요시)
        // 현재는 전역 설정만 적용하지만, 향후 Repository별 개별 설정 가능
        
        logger_.Info("Repository configurations applied successfully");
        
    } catch (const std::exception& e) {
        logger_.Error("RepositoryFactory::applyRepositoryConfigurations failed: " + std::string(e.what()));
        error_count_++;
    }
}

bool RepositoryFactory::injectDependencies() {
    try {
        logger_.Info("Injecting dependencies into repositories...");
        
        // 현재는 Repository들이 생성자에서 싱글톤 참조를 받아오므로 별도 주입 불필요
        // 향후 DI 컨테이너 패턴으로 발전시킬 때 이 메서드에서 의존성 주입 수행
        
        logger_.Info("Dependencies injected successfully");
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("RepositoryFactory::injectDependencies failed: " + std::string(e.what()));
        error_count_++;
        return false;
    }
}

} // namespace Database
} // namespace PulseOne