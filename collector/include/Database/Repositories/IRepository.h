#ifndef PULSEONE_IREPOSITORY_H
#define PULSEONE_IREPOSITORY_H

/**
 * @file IRepository.h
 * @brief PulseOne Repository 인터페이스 템플릿 (캐시 기능 내장)
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🔥 타입 정의 문제 해결:
 * - DatabaseTypes.h로 Database 전용 타입들 분리
 * - UnifiedCommonTypes.h 대신 적절한 타입 import
 * - 네임스페이스 일관성 확보
 */

#include "Database/DatabaseTypes.h"    // 🔥 Database 전용 타입들
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

// 🔥 Database 네임스페이스 내에서 직접 사용 (별칭 불필요)
// using QueryCondition = PulseOne::Database::QueryCondition;  ❌ 제거
// using OrderBy = PulseOne::Database::OrderBy;                ❌ 제거  
// using Pagination = PulseOne::Database::Pagination;          ❌ 제거

/**
 * @brief Repository 통합 인터페이스 템플릿 (캐시 기능 내장)
 * @tparam EntityType 엔티티 타입 (DeviceEntity, DataPointEntity 등)
 */
template<typename EntityType>
class IRepository {
private:
    // =======================================================================
    // 🔥 캐시 전용 구조체 (Repository에서만 사용)
    // =======================================================================
    
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
    // =======================================================================
    // 🔥 보호된 생성자 (기존 DeviceRepository 호환)
    // =======================================================================
    
    /**
     * @brief Repository 초기화 (캐시 포함)
     * @param repository_name Repository 이름 (로깅용)
     */
    explicit IRepository(const std::string& repository_name = "Repository")
        : repository_name_(repository_name)
        , db_manager_(&DatabaseManager::getInstance())
        , config_manager_(&ConfigManager::getInstance())
        , logger_(&PulseOne::LogManager::getInstance())
        , cache_enabled_(true)
        , cache_ttl_(std::chrono::seconds(300))  // 기본 5분 TTL
        , cache_hits_(0)
        , cache_misses_(0)
        , cache_evictions_(0)
        , max_cache_size_(1000)  // 기본 최대 1000개
        , enable_bulk_optimization_(true) {
        
        loadCacheConfiguration();
        logger_->Info("🗄️ " + repository_name_ + " initialized with caching enabled");
    }

public:
    /**
     * @brief 가상 소멸자
     */
    virtual ~IRepository() = default;

    // =======================================================================
    // 🔥 순수 가상 함수들 (기존 DeviceRepository 호환)
    // =======================================================================
    
    virtual std::vector<EntityType> findAll() = 0;
    virtual std::optional<EntityType> findById(int id) = 0;
    virtual bool save(EntityType& entity) = 0;
    virtual bool update(const EntityType& entity) = 0;
    virtual bool deleteById(int id) = 0;
    virtual bool exists(int id) = 0;  // 🔥 누락된 메서드 추가
    virtual std::vector<EntityType> findByIds(const std::vector<int>& ids) = 0;
    virtual int saveBulk(std::vector<EntityType>& entities) = 0;
    virtual int updateBulk(const std::vector<EntityType>& entities) = 0;
    virtual int deleteByIds(const std::vector<int>& ids) = 0;
    virtual std::vector<EntityType> findByConditions(
        const std::vector<QueryCondition>& conditions,
        const std::optional<OrderBy>& order_by = std::nullopt,
        const std::optional<Pagination>& pagination = std::nullopt) = 0;
    virtual int countByConditions(const std::vector<QueryCondition>& conditions) = 0;
    virtual int getTotalCount() = 0;
    virtual std::string getRepositoryName() const = 0;
    
    // =======================================================================
    // 🔥 캐시 관리 가상 함수들 (기존 DeviceRepository 호환)
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
    // 🔥 편의 메서드들 (기존 DeviceRepository 호환)
    // =======================================================================
    
    /**
     * @brief 단일 필드 조건으로 조회
     */
    std::vector<EntityType> findBy(const std::string& field, const std::string& operation, const std::string& value) {
        return findByConditions({QueryCondition(field, operation, value)});
    }
    
    /**
     * @brief 조건으로 첫 번째 엔티티 조회
     */
    std::optional<EntityType> findFirstByConditions(const std::vector<QueryCondition>& conditions) {
        auto results = findByConditions(conditions, std::nullopt, Pagination(1, 0));
        return results.empty() ? std::nullopt : std::make_optional(results[0]);
    }

protected:
    // =======================================================================
    // 🔥 파생 클래스에서 사용할 캐시 헬퍼 메서드들 (기존 DeviceRepository 호환)
    // =======================================================================
    
    /**
     * @brief 캐시에서 엔티티 조회 (기존 DeviceRepository::getCachedEntity와 동일)
     */
    std::optional<EntityType> getCachedEntity(int id) {
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
        
        // TTL 확인 (기존 DeviceRepository와 동일 로직)
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
    
    /**
     * @brief 캐시에 엔티티 저장 (기존 DeviceRepository::cacheEntity와 동일)
     */
    void cacheEntity(const EntityType& entity) {
        if (!cache_enabled_) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        // 캐시 크기 제한 확인 (기존 DeviceRepository와 동일 로직)
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
    
    // =======================================================================
    // 🔥 파생 클래스에서 접근할 수 있는 공통 멤버들 (기존 DeviceRepository 호환)
    // =======================================================================
    
    DatabaseManager* db_manager_;
    ConfigManager* config_manager_;
    PulseOne::LogManager* logger_;
    bool enable_bulk_optimization_;

    // =======================================================================
    // 🔥 SQL 빌더 헬퍼 메서드들 (기존 DeviceRepository 헬퍼들과 호환)
    // =======================================================================
    
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
    
    // 캐시 관련 (Repository 내부 전용)
    mutable std::mutex cache_mutex_;
    bool cache_enabled_;
    std::map<int, CacheEntry> entity_cache_;  // 🔥 내부 CacheEntry 사용
    std::chrono::seconds cache_ttl_;
    std::atomic<int> cache_hits_;
    std::atomic<int> cache_misses_;
    std::atomic<int> cache_evictions_;
    int max_cache_size_;
    
    // =======================================================================
    // 내부 헬퍼 메서드들 (기존 DeviceRepository와 동일)
    // =======================================================================
    
    void cleanupExpiredCache() {
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
    
    void loadCacheConfiguration() {
        try {
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
        }
    }
    
    std::string escapeString(const std::string& str) const {
        std::string escaped = str;
        size_t pos = 0;
        while ((pos = escaped.find("'", pos)) != std::string::npos) {
            escaped.replace(pos, 1, "''");
            pos += 2;
        }
        return escaped;
    }
};

} // namespace Database
} // namespace PulseOne

#endif // PULSEONE_IREPOSITORY_H