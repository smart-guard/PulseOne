// =============================================================================
// collector/src/Database/DataAccessManager.cpp (단순화된 구현)
// =============================================================================

#include "Database/DataAccessManager.h"

namespace PulseOne {
namespace Database {

// =============================================================================
// 싱글턴 구현
// =============================================================================
DataAccessManager& DataAccessManager::GetInstance() {
    static DataAccessManager instance;
    return instance;
}

// =============================================================================
// 팩토리 레지스트리 (static)
// =============================================================================
std::unordered_map<std::string, std::unique_ptr<IDataAccessFactory>>& 
DataAccessManager::GetFactoryRegistry() {
    static std::unordered_map<std::string, std::unique_ptr<IDataAccessFactory>> registry;
    return registry;
}

bool DataAccessManager::RegisterFactory(const std::string& domain_name,
                                       std::unique_ptr<IDataAccessFactory> factory) {
    auto& registry = GetFactoryRegistry();
    
    if (registry.find(domain_name) != registry.end()) {
        // 이미 등록된 도메인
        return false;
    }
    
    registry[domain_name] = std::move(factory);
    return true;
}

// =============================================================================
// 초기화 및 정리
// =============================================================================
bool DataAccessManager::Initialize(std::shared_ptr<LogManager> logger, 
                                  std::shared_ptr<ConfigManager> config) {
    std::lock_guard<std::mutex> lock(init_mutex_);
    
    if (initialized_) {
        return true;
    }
    
    logger_ = logger;
    config_ = config;
    
    if (!logger_ || !config_) {
        return false;
    }
    
    logger_->Info("DataAccessManager initializing...");
    
    // 등록된 모든 팩토리 확인
    auto& registry = GetFactoryRegistry();
    logger_->Info("Found " + std::to_string(registry.size()) + " registered data access domains");
    
    for (const auto& [domain_name, factory] : registry) {
        logger_->Info("Registered domain: " + domain_name);
    }
    
    initialized_ = true;
    logger_->Info("DataAccessManager initialized successfully");
    
    return true;
}

void DataAccessManager::Cleanup() {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    
    if (logger_) {
        logger_->Info("DataAccessManager cleaning up...");
    }
    
    // 모든 인스턴스 정리
    for (auto& [domain_name, instance] : instances_) {
        if (instance) {
            instance->Cleanup();
        }
    }
    
    instances_.clear();
    
    if (logger_) {
        logger_->Info("DataAccessManager cleanup completed");
    }
    
    initialized_ = false;
}

// =============================================================================
// 도메인 인스턴스 관리
// =============================================================================
std::shared_ptr<IDataAccess> DataAccessManager::GetDomainByName(const std::string& domain_name) {
    if (!initialized_) {
        if (logger_) {
            logger_->Error("DataAccessManager not initialized");
        }
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(instances_mutex_);
    
    // 캐시에서 확인
    auto it = instances_.find(domain_name);
    if (it != instances_.end() && it->second) {
        return it->second;
    }
    
    // 새로 생성
    auto instance = CreateDomainInstance(domain_name);
    if (instance) {
        instances_[domain_name] = instance;
    }
    
    return instance;
}

std::shared_ptr<IDataAccess> DataAccessManager::CreateDomainInstance(const std::string& domain_name) {
    auto& registry = GetFactoryRegistry();
    auto factory_it = registry.find(domain_name);
    
    if (factory_it == registry.end()) {
        if (logger_) {
            logger_->Error("No factory registered for domain: " + domain_name);
        }
        return nullptr;
    }
    
    // 팩토리로 인스턴스 생성
    auto instance = factory_it->second->Create(logger_, config_);
    if (!instance) {
        if (logger_) {
            logger_->Error("Failed to create instance for domain: " + domain_name);
        }
        return nullptr;
    }
    
    // 초기화
    if (!instance->Initialize()) {
        if (logger_) {
            logger_->Error("Failed to initialize domain: " + domain_name);
        }
        return nullptr;
    }
    
    if (logger_) {
        logger_->Info("Successfully created and initialized domain: " + domain_name);
    }
    
    return instance;
}

// =============================================================================
// 헬스 체크
// =============================================================================
bool DataAccessManager::HealthCheckAll() {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    
    bool all_healthy = true;
    
    for (const auto& [domain_name, instance] : instances_) {
        if (instance && !instance->HealthCheck()) {
            if (logger_) {
                logger_->Warning("Health check failed for domain: " + domain_name);
            }
            all_healthy = false;
        }
    }
    
    return all_healthy;
}

} // namespace Database
} // namespace PulseOne