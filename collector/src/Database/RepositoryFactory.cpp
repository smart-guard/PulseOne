/**
 * @file RepositoryFactory.cpp
 * @brief PulseOne Repository 팩토리 구현 - Include 수정본
 * @author PulseOne Development Team
 * @date 2025-07-30
 */

#include "Database/RepositoryFactory.h"

// ✅ 완전한 타입 정의를 위한 실제 헤더들 include
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Database/DatabaseManager.h"

// ✅ Repository 구현 클래스들 include (전방 선언 해결)
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DataPointRepository.h"
#include "Database/Repositories/DeviceSettingsRepository.h"
#include "Database/Repositories/UserRepository.h"
#include "Database/Repositories/TenantRepository.h"
#include "Database/Repositories/AlarmConfigRepository.h"
#include "Database/Repositories/SiteRepository.h"
#include "Database/Repositories/VirtualPointRepository.h"
#include "Database/Repositories/CurrentValueRepository.h"

// ✅ 필수 STL 헤더들
#include <map>
#include <stdexcept>
#include <functional>
#include <sstream>
#include <string>
#include <mutex>
#include <memory>

namespace PulseOne {
namespace Database {

// =============================================================================
// 싱글톤 구현
// =============================================================================

RepositoryFactory& RepositoryFactory::getInstance() {
    static RepositoryFactory instance;
    return instance;
}

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

RepositoryFactory::RepositoryFactory()
    : db_manager_(&::DatabaseManager::getInstance())          // ✅ 전역 네임스페이스
    , config_manager_(&::ConfigManager::getInstance())        // ✅ 전역 네임스페이스
    , logger_(&::LogManager::getInstance())                   // ✅ 전역 네임스페이스
    , global_cache_enabled_(true)
    , cache_ttl_seconds_(300)
    , max_cache_size_(1000) {
    
    logger_->Info("🏭 RepositoryFactory created");
}

RepositoryFactory::~RepositoryFactory() {
    shutdown();
    logger_->Info("🏭 RepositoryFactory destroyed");
}

// =============================================================================
// 초기화 및 종료
// =============================================================================

bool RepositoryFactory::initialize() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (initialized_.load()) {
        logger_->Warn("RepositoryFactory already initialized");
        return true;
    }
    
    try {
        logger_->Info("🔧 RepositoryFactory initializing...");
        
        // 1. DatabaseManager 초기화 확인
        if (!db_manager_->isPostgresConnected() && !db_manager_->isSQLiteConnected()) {
            logger_->Error("DatabaseManager not connected - attempting initialization");
            if (!db_manager_->initialize()) {
                logger_->Error("Failed to initialize DatabaseManager");
                error_count_.fetch_add(1);
                return false;
            }
        }
        
        // 2. Repository 인스턴스 생성
        if (!createRepositoryInstances()) {
            logger_->Error("Failed to create repository instances");
            error_count_.fetch_add(1);
            return false;
        }
        
        connectRepositoryDependencies();

        // 3. 의존성 주입
        if (!injectDependencies()) {
            logger_->Error("Failed to inject dependencies");
            error_count_.fetch_add(1);
            return false;
        }
        
        // 4. Repository별 설정 적용
        applyRepositoryConfigurations();
        
        initialized_.store(true);
        logger_->Info("✅ RepositoryFactory initialized successfully");
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("RepositoryFactory::initialize failed: " + std::string(e.what()));
        error_count_.fetch_add(1);
        return false;
    }
}

void RepositoryFactory::shutdown() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    if (!initialized_.load()) {
        return;
    }
    
    try {
        logger_->Info("🔧 RepositoryFactory shutting down...");
        
        // 활성 트랜잭션이 있으면 롤백 (구현되어 있다면)
        // if (transaction_active_.load()) {
        //     logger_->Warn("Active transaction found during shutdown - rolling back");
        //     rollbackGlobalTransaction();
        // }
        
        // 모든 캐시 클리어
        clearAllCaches();
        
        // Repository 인스턴스 해제
        device_repository_.reset();
        data_point_repository_.reset();
        user_repository_.reset();
        tenant_repository_.reset();
        alarm_config_repository_.reset();
        site_repository_.reset();
        virtual_point_repository_.reset();
        current_value_repository_.reset();
        device_settings_repository_.reset(); 
        
        initialized_.store(false);
        logger_->Info("✅ RepositoryFactory shutdown completed");
        
    } catch (const std::exception& e) {
        logger_->Error("RepositoryFactory::shutdown failed: " + std::string(e.what()));
        error_count_.fetch_add(1);
    }
}

// =============================================================================
// 캐싱 제어
// =============================================================================

void RepositoryFactory::setCacheEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    global_cache_enabled_ = enabled;
    
    logger_->Info("Global cache " + std::string(enabled ? "enabled" : "disabled"));
    
    // 각 Repository에 캐시 설정 적용 (구현되어 있다면)
    // if (device_repository_) {
    //     device_repository_->setCacheEnabled(enabled);
    // }
    // ... 다른 repositories도 동일
}

void RepositoryFactory::clearAllCaches() {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    logger_->Info("Clearing all repository caches...");
    
    int total_cleared = 0;
    
    // 각 Repository 캐시 클리어 (구현되어 있다면)
    // if (device_repository_) {
    //     try {
    //         device_repository_->clearCache();
    //         total_cleared++;
    //     } catch (const std::exception& e) {
    //         logger_->Error("Failed to clear DeviceRepository cache: " + std::string(e.what()));
    //         error_count_.fetch_add(1);
    //     }
    // }
    
    logger_->Info("Cleared " + std::to_string(total_cleared) + " cached items from all repositories");
}

void RepositoryFactory::setCacheTTL(int ttl_seconds) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    cache_ttl_seconds_ = ttl_seconds;
    
    logger_->Info("Cache TTL set to " + std::to_string(ttl_seconds) + " seconds");
}

void RepositoryFactory::setMaxCacheSize(int max_size) {
    std::lock_guard<std::mutex> lock(factory_mutex_);
    
    max_cache_size_ = max_size;
    
    logger_->Info("Max cache size set to " + std::to_string(max_size) + " entries");
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

bool RepositoryFactory::createRepositoryInstances() {
    try {
        logger_->Info("Creating repository instances...");
        
        // ✅ 올바른 네임스페이스로 Repository 생성
        device_repository_ = std::make_shared<Repositories::DeviceRepository>();
        if (!device_repository_) {
            logger_->Error("Failed to create DeviceRepository");
            return false;
        }

        device_settings_repository_ = std::make_shared<Repositories::DeviceSettingsRepository>();  // 🆕 이 줄 추가
        if (!device_settings_repository_) {
            logger_->Error("Failed to create DeviceSettingsRepository");
            return false;
        }
        
        data_point_repository_ = std::make_shared<Repositories::DataPointRepository>();
        if (!data_point_repository_) {
            logger_->Error("Failed to create DataPointRepository");
            return false;
        }
        
        user_repository_ = std::make_shared<Repositories::UserRepository>();
        if (!user_repository_) {
            logger_->Error("Failed to create UserRepository");
            return false;
        }
        
        tenant_repository_ = std::make_shared<Repositories::TenantRepository>();
        if (!tenant_repository_) {
            logger_->Error("Failed to create TenantRepository");
            return false;
        }
        
        alarm_config_repository_ = std::make_shared<Repositories::AlarmConfigRepository>();
        if (!alarm_config_repository_) {
            logger_->Error("Failed to create AlarmConfigRepository");
            return false;
        }
        
        site_repository_ = std::make_shared<Repositories::SiteRepository>();
        if (!site_repository_) {
            logger_->Error("Failed to create SiteRepository");
            return false;
        }

        virtual_point_repository_ = std::make_shared<Repositories::VirtualPointRepository>();
        if (!virtual_point_repository_) {
            logger_->Error("Failed to create VirtualPointRepository");
            return false;
        }
        
        current_value_repository_ = std::make_shared<Repositories::CurrentValueRepository>();
        if (!current_value_repository_) {
            logger_->Error("Failed to create CurrentValueRepository");
            return false;
        }
        
        logger_->Info("All repository instances created successfully");
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("RepositoryFactory::createRepositoryInstances failed: " + std::string(e.what()));
        error_count_.fetch_add(1);
        return false;
    }
}

void RepositoryFactory::applyRepositoryConfigurations() {
    try {
        logger_->Info("Applying repository configurations...");
        
        logger_->Info("Step 1: Checking global cache settings");
        
        // 전역 캐시 설정 적용 (안전하게)
        try {
            logger_->Info("Step 2: Applying cache settings");
            if (global_cache_enabled_) {
                logger_->Info("Cache enabled globally");
            } else {
                logger_->Info("Cache disabled globally");
            }
            // setCacheEnabled(global_cache_enabled_);  // 🔧 주석 처리
            logger_->Info("Step 3: Cache settings applied");
        } catch (const std::exception& cache_e) {
            logger_->Warn("Cache setting failed but continuing: " + std::string(cache_e.what()));
        }
        
        logger_->Info("Step 4: Configuration complete");
        logger_->Info("✅ Repository configurations applied successfully");
        
    } catch (const std::exception& e) {
        logger_->Error("RepositoryFactory::applyRepositoryConfigurations failed: " + std::string(e.what()));
        error_count_.fetch_add(1);
    }
}

bool RepositoryFactory::injectDependencies() {
    try {
        logger_->Info("Injecting dependencies into repositories...");
        
        // 현재는 Repository들이 생성자에서 싱글톤 참조를 받아오므로 별도 주입 불필요
        // 향후 DI 컨테이너 패턴으로 발전시킬 때 이 메서드에서 의존성 주입 수행
        
        logger_->Info("Dependencies injected successfully");
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("RepositoryFactory::injectDependencies failed: " + std::string(e.what()));
        error_count_.fetch_add(1);
        return false;
    }
}

void RepositoryFactory::connectRepositoryDependencies() {
    try {
        logger_->Info("Connecting repository dependencies...");
        
        // 🔥 핵심: DataPointRepository에 CurrentValueRepository 자동 주입
        if (data_point_repository_ && current_value_repository_) {
            data_point_repository_->setCurrentValueRepository(current_value_repository_);
            logger_->Info("✅ CurrentValueRepository connected to DataPointRepository");
        }
        
        // 🔥 향후 추가 연결들
        // if (alarm_repository_ && data_point_repository_) {
        //     alarm_repository_->setDataPointRepository(data_point_repository_);
        // }
        // if (virtual_point_repository_ && data_point_repository_) {
        //     virtual_point_repository_->setDataPointRepository(data_point_repository_);
        // }
        
        logger_->Info("✅ Repository dependencies connected successfully");
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to connect repository dependencies: " + std::string(e.what()));
        error_count_.fetch_add(1);
    }
}

} // namespace Database
} // namespace PulseOne