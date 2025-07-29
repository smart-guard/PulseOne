#ifndef PULSEONE_IREPOSITORY_H
#define PULSEONE_IREPOSITORY_H

/**
 * @file IRepository.h
 * @brief PulseOne Repository 인터페이스 템플릿 (완전 새 버전)
 * @author PulseOne Development Team
 * @date 2025-07-28
 */

#include "Database/DatabaseTypes.h"
#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include <vector>
#include <optional>
#include <map>
#include <memory>
#include <string>
#include <functional>
#include <mutex>
#include <chrono>
#include <atomic>

namespace PulseOne {
namespace Database {

template<typename EntityType>
class IRepository {
private:
    struct CacheEntry {
        EntityType entity;
        std::chrono::system_clock::time_point cached_at;
        
        CacheEntry() : cached_at(std::chrono::system_clock::now()) {}
        CacheEntry(const EntityType& e) : entity(e), cached_at(std::chrono::system_clock::now()) {}
        
        bool isExpired(const std::chrono::seconds& ttl) const {
            auto now = std::chrono::system_clock::now();
            return (now - cached_at) > ttl;
    private:
    // =======================================================================
    // 캐시 관련 (private)
    // =======================================================================
    
    bool cache_enabled_;
    std::chrono::seconds cache_ttl_;
    size_t max_cache_size_;
    mutable std::mutex cache_mutex_;
    mutable std::map<int, CacheEntry> entity_cache_;
    mutable std::atomic<int> cache_hits_;
    mutable std::atomic<int> cache_misses_;
    mutable std::atomic<int> cache_evictions_;
    };

protected:
    explicit IRepository(const std::string& repository_name = "Repository")
        : repository_name_(repository_name)
        , enable_bulk_optimization_(true)
        , cache_enabled_(true)
        , cache_ttl_(std::chrono::seconds(300))
        , max_cache_size_(1000)
        , cache_hits_(0)
        , cache_misses_(0)
        , cache_evictions_(0)
        , db_manager_(nullptr)
        , config_manager_(nullptr)
        , logger_(nullptr) {
    }

public:
    virtual ~IRepository() = default;

    void initializeDependencies() {
        try {
            db_manager_ = &PulseOne::Database::DatabaseManager::getInstance();
            config_manager_ = &PulseOne::ConfigManager::getInstance();
            logger_ = &PulseOne::LogManager::getInstance();
            
            loadCacheConfiguration();
            if (logger_) {
                logger_->Info("🗄️ " + repository_name_ + " initialized with caching enabled");
            }
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->Error("initializeDependencies failed: " + std::string(e.what()));
            }
        }
    }

    // =======================================================================
    // 기본 가상 함수들
    // =======================================================================
    
    virtual std::vector<EntityType> findAll() {
        if (logger_) logger_->Error(repository_name_ + "::findAll() - Not implemented");
        return {};
    }
    
    virtual std::optional<EntityType> findById(int id) {
        if (logger_) logger_->Error(repository_name_ + "::findById() - Not implemented");
        return std::nullopt;
    }
    
    virtual bool save(EntityType& entity) {
        if (logger_) logger_->Error(repository_name_ + "::save() - Not implemented");
        return false;
    }
    
    virtual bool update(const EntityType& entity) {
        if (logger_) logger_->Error(repository_name_ + "::update() - Not implemented");
        return false;
    }
    
    virtual bool deleteById(int id) {
        if (logger_) logger_->Error(repository_name_ + "::deleteById() - Not implemented");
        return false;
    }
    
    virtual bool exists(int id) {
        if (logger_) logger_->Error(repository_name_ + "::exists() - Not implemented");
        return false;
    }
    
    virtual std::vector<EntityType> findByIds(const std::vector<int>& ids) {
        std::vector<EntityType> results;
        results.reserve(ids.size());
        
        for (int id : ids) {
            auto entity = findById(id);
            if (entity.has_value()) {
                results.push_back(entity.value());
            }
        }
        return results;
    }
    
    virtual int saveBulk(std::vector<EntityType>& entities) {
        int success_count = 0;
        for (auto& entity : entities) {
            if (save(entity)) {
                success_count++;
            }
        }
        return success_count;
    }
    
    virtual int updateBulk(const std::vector<EntityType>& entities) {
        int success_count = 0;
        for (const auto& entity : entities) {
            if (update(entity)) {
                success_count++;
            }
        }
        return success_count;
    }
    
    virtual int deleteByIds(const std::vector<int>& ids) {
        int success_count = 0;
        for (int id : ids) {
            if (deleteById(id)) {
                success_count++;
            }
        }
        return success_count;
    }
    
    virtual std::vector<EntityType> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt) {
        if (logger_) logger_->Error(repository_name_ + "::findByConditions() - Not implemented");
        return {};
    }
    
    virtual int countByConditions(const std::vector<QueryCondition>& conditions) {
        if (logger_) logger_->Error(repository_name_ + "::countByConditions() - Not implemented");
        return 0;
    }
    
    virtual int getTotalCount() {
        return countByConditions({});
    }
    
    virtual std::string getRepositoryName() const {
        return repository_name_;
    }
    
    // =======================================================================
    // 캐시 관리 메서드들
    // =======================================================================
    
    virtual void setCacheEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cache_enabled_ = enabled;
        if (logger_) {
            logger_->Info(repository_name_ + " cache " + (enabled ? "enabled" : "disabled"));
        }
    }
    
    virtual bool isCacheEnabled() const {
        return cache_enabled_;
    }
    
    virtual void clearCache() {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        entity_cache_.clear();
        if (logger_) {
            logger_->Info(repository_name_ + " cache cleared");
        }
    }
    
    virtual void clearCacheForId(int id) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = entity_cache_.find(id);
        if (it != entity_cache_.end()) {
            entity_cache_.erase(it);
            if (logger_) {
                logger_->Debug(repository_name_ + " cache cleared for ID: " + std::to_string(id));
            }
        }
    }
    
    virtual std::map<std::string, int> getCacheStats() const {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        return {
            {"hits", cache_hits_.load()},
            {"misses", cache_misses_.load()},
            {"evictions", cache_evictions_.load()},
            {"size", static_cast<int>(entity_cache_.size())},
            {"max_size", static_cast<int>(max_cache_size_)}
        };
    }

protected:
    // =======================================================================
    // 캐시 헬퍼 메서드들
    // =======================================================================
    
    std::optional<EntityType> getCachedEntity(int id) const {
        if (!cache_enabled_) {
            return std::nullopt;
        }
        
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = entity_cache_.find(id);
        if (it != entity_cache_.end()) {
            if (!it->second.isExpired(cache_ttl_)) {
                cache_hits_.fetch_add(1);
                return it->second.entity;
            } else {
                entity_cache_.erase(it);
                cache_evictions_.fetch_add(1);
            }
        }
        
        cache_misses_.fetch_add(1);
        return std::nullopt;
    }
    
    void cacheEntity(const EntityType& entity) {
        if (!cache_enabled_) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        if (entity_cache_.size() >= max_cache_size_) {
            auto oldest = entity_cache_.begin();
            entity_cache_.erase(oldest);
            cache_evictions_.fetch_add(1);
        }
        
        try {
            int id = entity.getId();
            entity_cache_[id] = CacheEntry(entity);
        } catch (...) {
            if (logger_) {
                logger_->Warn(repository_name_ + " - Failed to cache entity (no getId() method?)");
            }
        }
    }
    
    void loadCacheConfiguration() {
        try {
            if (!config_manager_) return;
            
            std::string cache_enabled_str = config_manager_->getOrDefault("CACHE_ENABLED", "true");
            cache_enabled_ = (cache_enabled_str == "true" || cache_enabled_str == "1");
            
            int ttl_seconds = std::stoi(config_manager_->getOrDefault("CACHE_TTL_SECONDS", "300"));
            cache_ttl_ = std::chrono::seconds(ttl_seconds);
            
            max_cache_size_ = static_cast<size_t>(std::stoi(config_manager_->getOrDefault("CACHE_MAX_SIZE", "1000")));
            
            enable_bulk_optimization_ = config_manager_->getOrDefault("ENABLE_BULK_OPTIMIZATION", "true") == "true";
                      
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->Warn(repository_name_ + " failed to load cache config, using defaults: " + std::string(e.what()));
            }
            cache_enabled_ = true;
            cache_ttl_ = std::chrono::seconds(300);
            max_cache_size_ = 1000;
            enable_bulk_optimization_ = true;
        }
    }

protected:
    // =======================================================================
    // 파생 클래스에서 접근 가능한 멤버들
    // =======================================================================
    
    std::string repository_name_;
    bool enable_bulk_optimization_;
    
    // 의존성들 (파생 클래스에서 접근 가능)
    PulseOne::Database::DatabaseManager* db_manager_;
    PulseOne::ConfigManager* config_manager_;
    PulseOne::LogManager* logger_;
};

} // namespace Database
} // namespace PulseOne

#endif // PULSEONE_IREPOSITORY_H