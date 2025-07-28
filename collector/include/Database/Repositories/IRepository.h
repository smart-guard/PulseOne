#ifndef PULSEONE_IREPOSITORY_H
#define PULSEONE_IREPOSITORY_H

/**
 * @file IRepository.h
 * @brief PulseOne Repository 인터페이스 템플릿 (캐시 기능 내장)
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🔥 링크 에러 해결:
 * - 누락된 템플릿 메서드 구현부들 추가
 * - C++ 템플릿 특성상 모든 구현이 헤더에 필요
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

/**
 * @brief Repository 통합 인터페이스 템플릿 (캐시 기능 내장)
 * @tparam EntityType 엔티티 타입 (DeviceEntity, DataPointEntity 등)
 */
template<typename EntityType>
class IRepository {
private:
    /**
     * @brief 캐시 엔트리 구조체 (Repository 내부에서만 사용)
     */
    struct CacheEntry {
        EntityType entity;
        std::chrono::system_clock::time_point cached_at;
        
        CacheEntry() : cached_at(std::chrono::system_clock::now()) {}
        CacheEntry(const EntityType& e) : entity(e), cached_at(std::chrono::system_clock::now()) {}
        
        bool isExpired(const std::chrono::seconds& ttl) const {
            auto now = std::chrono::system_clock::now();
            return (now - cached_at) > ttl;
        }
    };

protected:
    /**
     * @brief Repository 초기화 (캐시 포함)
     * @param repository_name Repository 이름 (로깅용)
     */
    explicit IRepository(const std::string& repository_name = "Repository")
        : db_manager_(&DatabaseManager::getInstance())        // ✅ 첫 번째
        , config_manager_(&ConfigManager::getInstance())
        , logger_(&PulseOne::LogManager::getInstance())
        , enable_bulk_optimization_(true)                     // ✅ 순서 조정
        , repository_name_(repository_name)                   // ✅ 순서 조정
        , cache_enabled_(true)
        , cache_ttl_(std::chrono::seconds(300))
        , cache_hits_(0)
        , cache_misses_(0)
        , cache_evictions_(0)
        , max_cache_size_(1000) {                             // ✅ 마지막
        
        loadCacheConfiguration();
        logger_->Info("🗄️ " + repository_name_ + " initialized with caching enabled");
    }

public:
    virtual ~IRepository() = default;

    // =======================================================================
    // 기본 구현이 있는 가상 함수들 (파생 클래스에서 오버라이드 가능)
    // =======================================================================
    
    virtual std::vector<EntityType> findAll() {
        // 기본 구현 (파생 클래스에서 오버라이드 해야 함)
        logger_->Error(repository_name_ + "::findAll() - Not implemented in derived class");
        return {};
    }
    
    virtual std::optional<EntityType> findById(int id) {
        // 기본 구현 (파생 클래스에서 오버라이드 해야 함)
        logger_->Error(repository_name_ + "::findById() - Not implemented in derived class");
        return std::nullopt;
    }
    
    virtual bool save(EntityType& entity) {
        // 기본 구현 (파생 클래스에서 오버라이드 해야 함)
        logger_->Error(repository_name_ + "::save() - Not implemented in derived class");
        return false;
    }
    
    virtual bool update(const EntityType& entity) {
        // 기본 구현 (파생 클래스에서 오버라이드 해야 함)
        logger_->Error(repository_name_ + "::update() - Not implemented in derived class");
        return false;
    }
    
    virtual bool deleteById(int id) {
        // 기본 구현 (파생 클래스에서 오버라이드 해야 함)
        logger_->Error(repository_name_ + "::deleteById() - Not implemented in derived class");
        return false;
    }
    
    virtual bool exists(int id) {
        // 기본 구현 (파생 클래스에서 오버라이드 해야 함)
        logger_->Error(repository_name_ + "::exists() - Not implemented in derived class");
        return false;
    }
    
    virtual std::vector<EntityType> findByIds(const std::vector<int>& ids) {
        // 기본 구현 - 각 ID를 개별 조회하여 합침
        std::vector<EntityType> results;
        results.reserve(ids.size());
        
        for (int id : ids) {
            auto entity = findById(id);
            if (entity.has_value()) {
                results.push_back(entity.value());
            }
        }
        
        logger_->Debug(repository_name_ + "::findByIds() - Found " + 
                      std::to_string(results.size()) + "/" + std::to_string(ids.size()));
        return results;
    }
    
    virtual int saveBulk(std::vector<EntityType>& entities) {
        // 기본 구현 - 각 엔티티를 개별 저장
        int success_count = 0;
        
        for (auto& entity : entities) {
            if (save(entity)) {
                success_count++;
            }
        }
        
        logger_->Info(repository_name_ + "::saveBulk() - Saved " + 
                     std::to_string(success_count) + "/" + std::to_string(entities.size()));
        return success_count;
    }
    
    virtual int updateBulk(const std::vector<EntityType>& entities) {
        // 기본 구현 - 각 엔티티를 개별 업데이트
        int success_count = 0;
        
        for (const auto& entity : entities) {
            if (update(entity)) {
                success_count++;
            }
        }
        
        logger_->Info(repository_name_ + "::updateBulk() - Updated " + 
                     std::to_string(success_count) + "/" + std::to_string(entities.size()));
        return success_count;
    }
    
    virtual int deleteByIds(const std::vector<int>& ids) {
        // 기본 구현 - 각 ID를 개별 삭제
        int success_count = 0;
        
        for (int id : ids) {
            if (deleteById(id)) {
                success_count++;
            }
        }
        
        logger_->Info(repository_name_ + "::deleteByIds() - Deleted " + 
                     std::to_string(success_count) + "/" + std::to_string(ids.size()));
        return success_count;
    }
    
    virtual std::vector<EntityType> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt) {
        // 기본 구현 (파생 클래스에서 오버라이드 해야 함)
        logger_->Error(repository_name_ + "::findByConditions() - Not implemented in derived class");
        return {};
    }
    
    virtual int countByConditions(const std::vector<QueryCondition>& conditions) {
        // 기본 구현 (파생 클래스에서 오버라이드 해야 함)
        logger_->Error(repository_name_ + "::countByConditions() - Not implemented in derived class");
        return 0;
    }
    
    virtual int getTotalCount() {
        // 기본 구현 - 조건 없이 전체 개수 조회
        return countByConditions({});
    }
    
    virtual std::string getRepositoryName() const {
        return repository_name_;
    }
    
    // =======================================================================
    // 캐시 관리 가상 함수들 (IRepository에서 구현 제공)
    // =======================================================================
    
    virtual void setCacheEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cache_enabled_ = enabled;
        
        if (!enabled) {
            entity_cache_.clear();
            logger_->Info(repository_name_ + " cache disabled and cleared");
        } else {
            logger_->Info(repository_name_ + " cache enabled");
        }
    }
    
    virtual bool isCacheEnabled() const {
        return cache_enabled_;
    }
    
    virtual void clearCache() {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        int cleared_count = static_cast<int>(entity_cache_.size());
        entity_cache_.clear();
        cache_hits_ = 0;
        cache_misses_ = 0;
        cache_evictions_ = 0;
        
        logger_->Info(repository_name_ + " cache cleared - " + std::to_string(cleared_count) + " entries removed");
    }
    
    virtual void clearCacheForId(int id) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = entity_cache_.find(id);
        if (it != entity_cache_.end()) {
            entity_cache_.erase(it);
            logger_->Debug(repository_name_ + " cache cleared for ID: " + std::to_string(id));
        }
    }
    
    virtual std::map<std::string, int> getCacheStats() const {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        std::map<std::string, int> stats;
        stats["enabled"] = cache_enabled_ ? 1 : 0;
        stats["size"] = static_cast<int>(entity_cache_.size());
        stats["max_size"] = max_cache_size_;
        stats["hits"] = cache_hits_;
        stats["misses"] = cache_misses_;
        stats["evictions"] = cache_evictions_;
        stats["hit_rate"] = (cache_hits_ + cache_misses_ > 0) 
                           ? (cache_hits_ * 100) / (cache_hits_ + cache_misses_) 
                           : 0;
        
        return stats;
    }
    
    // =======================================================================
    // 편의 메서드들
    // =======================================================================
    
    std::vector<EntityType> findBy(const std::string& field, const std::string& operation, const std::string& value) {
        return findByConditions({QueryCondition(field, operation, value)});
    }
    
    std::optional<EntityType> findFirstByConditions(const std::vector<QueryCondition>& conditions) {
        auto results = findByConditions(conditions, std::nullopt, Pagination(1, 0));
        return results.empty() ? std::nullopt : std::make_optional(results[0]);
    }

protected:
    // =======================================================================
    // 파생 클래스에서 사용할 캐시 헬퍼 메서드들
    // =======================================================================
    
    std::optional<EntityType> getCachedEntity(int id);
    void cacheEntity(const EntityType& entity);
    
    // 공통 멤버들
    DatabaseManager* db_manager_;
    ConfigManager* config_manager_;
    PulseOne::LogManager* logger_;
    bool enable_bulk_optimization_;
    
    // SQL 빌더 헬퍼 메서드들
    std::string buildWhereClause(const std::vector<QueryCondition>& conditions) const {
        if (conditions.empty()) {
            return "";
        }
        
        std::string where_clause = " WHERE ";
        for (size_t i = 0; i < conditions.size(); ++i) {
            if (i > 0) {
                where_clause += " AND ";
            }
            
            const auto& condition = conditions[i];
            where_clause += condition.field + " " + condition.operation + " ";
            
            if (condition.operation == "IN") {
                where_clause += "(" + condition.value + ")";
            } else if (condition.operation == "LIKE") {
                where_clause += "'%" + escapeString(condition.value) + "%'";
            } else {
                where_clause += "'" + escapeString(condition.value) + "'";
            }
        }
        
        return where_clause;
    }
    
    std::string buildOrderByClause(const std::optional<OrderBy>& order_by) const {
        if (!order_by.has_value()) {
            return "";
        }
        
        return " ORDER BY " + order_by->field + 
               (order_by->ascending ? " ASC" : " DESC");
    }
    
    std::string buildLimitClause(const std::optional<Pagination>& pagination) const {
        if (!pagination.has_value()) {
            return "";
        }
        
        return " LIMIT " + std::to_string(pagination->getLimit()) + 
               " OFFSET " + std::to_string(pagination->getOffset());
    }

private:
    std::string repository_name_;
    
    // 캐시 관련
    mutable std::mutex cache_mutex_;
    bool cache_enabled_;
    std::map<int, CacheEntry> entity_cache_;
    std::chrono::seconds cache_ttl_;
    std::atomic<int> cache_hits_;
    std::atomic<int> cache_misses_;
    std::atomic<int> cache_evictions_;
    int max_cache_size_;
    
    // 내부 헬퍼 메서드들
    void loadCacheConfiguration();
    void cleanupExpiredCache();
    std::string escapeString(const std::string& str) const;
};

// =============================================================================
// 🔥 누락된 템플릿 메서드 구현부들 (링크 에러 해결)
// =============================================================================

template<typename EntityType>
std::optional<EntityType> IRepository<EntityType>::getCachedEntity(int id) {
    if (!cache_enabled_) {
        return std::nullopt;
    }
    
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = entity_cache_.find(id);
    if (it == entity_cache_.end()) {
        cache_misses_.fetch_add(1);
        return std::nullopt;
    }
    
    auto& entry = it->second;
    
    // TTL 확인
    auto now = std::chrono::system_clock::now();
    if (now - entry.cached_at > cache_ttl_) {
        entity_cache_.erase(it);
        cache_evictions_.fetch_add(1);
        cache_misses_.fetch_add(1);
        return std::nullopt;
    }
    
    cache_hits_.fetch_add(1);
    return entry.entity;
}

template<typename EntityType>
void IRepository<EntityType>::cacheEntity(const EntityType& entity) {
    if (!cache_enabled_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // 캐시 크기 제한 확인
    if (static_cast<int>(entity_cache_.size()) >= max_cache_size_) {
        cleanupExpiredCache();
        
        // 여전히 크기 초과시 가장 오래된 것 제거 (LRU)
        if (static_cast<int>(entity_cache_.size()) >= max_cache_size_) {
            auto oldest_it = entity_cache_.begin();
            auto oldest_time = oldest_it->second.cached_at;
            
            for (auto it = entity_cache_.begin(); it != entity_cache_.end(); ++it) {
                if (it->second.cached_at < oldest_time) {
                    oldest_time = it->second.cached_at;
                    oldest_it = it;
                }
            }
            
            entity_cache_.erase(oldest_it);
            cache_evictions_.fetch_add(1);
        }
    }
    
    entity_cache_[entity.getId()] = CacheEntry(entity);
}

template<typename EntityType>
void IRepository<EntityType>::loadCacheConfiguration() {
    try {
        // 설정에서 캐시 관련 값들 로드
        std::string cache_enabled_str = config_manager_->getOrDefault("CACHE_ENABLED", "true");
        cache_enabled_ = (cache_enabled_str == "true" || cache_enabled_str == "1");
        
        int ttl_seconds = std::stoi(config_manager_->getOrDefault("CACHE_TTL_SECONDS", "300"));
        cache_ttl_ = std::chrono::seconds(ttl_seconds);
        
        max_cache_size_ = std::stoi(config_manager_->getOrDefault("CACHE_MAX_SIZE", "1000"));
        
        enable_bulk_optimization_ = config_manager_->getOrDefault("ENABLE_BULK_OPTIMIZATION", "true") == "true";
                
        logger_->Debug(repository_name_ + " cache config loaded - TTL: " + 
                      std::to_string(ttl_seconds) + "s, Max size: " + 
                      std::to_string(max_cache_size_));
                      
    } catch (const std::exception& e) {
        logger_->Warn(repository_name_ + " failed to load cache config, using defaults: " + 
                     std::string(e.what()));
        cache_enabled_ = true;
        cache_ttl_ = std::chrono::seconds(300);
        max_cache_size_ = 1000;
        enable_bulk_optimization_ = true;
    }
}

template<typename EntityType>
void IRepository<EntityType>::cleanupExpiredCache() {
    auto now = std::chrono::system_clock::now();
    
    auto it = entity_cache_.begin();
    while (it != entity_cache_.end()) {
        if (now - it->second.cached_at > cache_ttl_) {
            it = entity_cache_.erase(it);
            cache_evictions_.fetch_add(1);
        } else {
            ++it;
        }
    }
}

template<typename EntityType>
std::string IRepository<EntityType>::escapeString(const std::string& str) const {
    std::string escaped = str;
    size_t pos = 0;
    while ((pos = escaped.find("'", pos)) != std::string::npos) {
        escaped.replace(pos, 1, "''");
        pos += 2;
    }
    return escaped;
}

} // namespace Database
} // namespace PulseOne

#endif // PULSEONE_IREPOSITORY_H