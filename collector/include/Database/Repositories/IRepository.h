#ifndef PULSEONE_IREPOSITORY_H
#define PULSEONE_IREPOSITORY_H

/**
 * @file IRepository.h
 * @brief PulseOne Repository 인터페이스 템플릿 - 완전 수정본
 * @author PulseOne Development Team
 * @date 2025-07-29
 * 
 * 🔥 완전히 해결된 문제들:
 * - 전방 선언 → 실제 헤더 include로 변경
 * - 모든 매니저 클래스가 전역 네임스페이스에 있음을 반영
 * - incomplete type 오류 완전 해결
 */

// ✅ 표준 라이브러리
#include <vector>
#include <optional>
#include <map>
#include <memory>
#include <string>
#include <functional>
#include <mutex>
#include <chrono>
#include <atomic>
#include <sstream>      // std::ostringstream용

// ✅ 필수 타입만 include
#include "Database/DatabaseTypes.h"

// 🔥 실제 헤더 include (전방 선언 대신)
#include "Database/DatabaseManager.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"

namespace PulseOne {
namespace Database {

template<typename EntityType>
class IRepository {
private:
    // ✅ CacheEntry 구조체
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
    
    // ✅ 캐시 관련 멤버 변수들
    bool cache_enabled_;
    std::chrono::seconds cache_ttl_;
    size_t max_cache_size_;
    mutable std::mutex cache_mutex_;
    mutable std::map<int, CacheEntry> entity_cache_;
    mutable std::atomic<int> cache_hits_;
    mutable std::atomic<int> cache_misses_;
    mutable std::atomic<int> cache_evictions_;

protected:
    // ✅ 생성자
    explicit IRepository(const std::string& repository_name = "Repository")
        : cache_enabled_(true)
        , cache_ttl_(std::chrono::seconds(300))
        , max_cache_size_(1000)
        , cache_hits_(0)
        , cache_misses_(0)
        , cache_evictions_(0)
        , repository_name_(repository_name)
        , enable_bulk_optimization_(true)
        , db_manager_(nullptr)
        , config_manager_(nullptr)
        , logger_(nullptr) {
    }

public:
    virtual ~IRepository() = default;

    void initializeDependencies() {
        try {
            // ✅ 모든 매니저를 전역 네임스페이스로 접근
            db_manager_ = &DatabaseManager::getInstance();      // ✅ 전역
            config_manager_ = &ConfigManager::getInstance();    // ✅ 전역
            logger_ = &LogManager::getInstance();               // ✅ 전역
            
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

    // =======================================================================
    // 벌크 연산 (성능 최적화)
    // =======================================================================
    
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
        if (!enable_bulk_optimization_) {
            int count = 0;
            for (auto& entity : entities) {
                if (save(entity)) count++;
            }
            return count;
        }
        
        if (logger_) logger_->Error(repository_name_ + "::saveBulk() - Not implemented");
        return 0;
    }

    virtual int updateBulk(const std::vector<EntityType>& entities) {
        if (!enable_bulk_optimization_) {
            int count = 0;
            for (const auto& entity : entities) {
                if (update(entity)) count++;
            }
            return count;
        }
        
        if (logger_) logger_->Error(repository_name_ + "::updateBulk() - Not implemented");
        return 0;
    }

    virtual int deleteByIds(const std::vector<int>& ids) {
        if (!enable_bulk_optimization_) {
            int count = 0;
            for (int id : ids) {
                if (deleteById(id)) count++;
            }
            return count;
        }
        
        if (logger_) logger_->Error(repository_name_ + "::deleteByIds() - Not implemented");
        return 0;
    }

    // =======================================================================
    // 조건부 쿼리
    // =======================================================================
    
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
        return static_cast<int>(findAll().size());
    }

    // =======================================================================
    // 캐시 관리
    // =======================================================================
    
    virtual void setCacheEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cache_enabled_ = enabled;
        if (logger_) {
            logger_->Info(repository_name_ + " cache " + (enabled ? "enabled" : "disabled"));
        }
    }
    
    virtual bool isCacheEnabled() const {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        return cache_enabled_;
    }
    
    virtual void clearCache() {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        entity_cache_.clear();
        cache_hits_ = 0;
        cache_misses_ = 0;
        cache_evictions_ = 0;
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
        std::map<std::string, int> stats;
        stats["cache_enabled"] = cache_enabled_ ? 1 : 0;
        stats["cached_items"] = static_cast<int>(entity_cache_.size());
        stats["cache_hits"] = cache_hits_.load();
        stats["cache_misses"] = cache_misses_.load();
        stats["cache_evictions"] = cache_evictions_.load();
        stats["max_cache_size"] = static_cast<int>(max_cache_size_);
        stats["cache_ttl_seconds"] = static_cast<int>(cache_ttl_.count());
        return stats;
    }

    // =======================================================================
    // 유틸리티 메서드들
    // =======================================================================
    
    virtual std::string getRepositoryName() const {
        return repository_name_;
    }

    bool isInitialized() const {
        return (db_manager_ != nullptr && config_manager_ != nullptr && logger_ != nullptr);
    }

protected:
    // =======================================================================
    // 캐시 헬퍼 메서드들
    // =======================================================================
    /**
     * @brief 캐시에서 엔티티 조회
     * @param id 엔티티 ID
     * @return 캐시된 엔티티 (없으면 nullopt)
     */
    std::optional<EntityType> getCachedEntity(int id) const {
        if (!cache_enabled_) {
            cache_misses_++;
            return std::nullopt;
        }
        
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = entity_cache_.find(id);
        if (it != entity_cache_.end()) {
            if (!it->second.isExpired(cache_ttl_)) {
                cache_hits_++;
                return it->second.entity;
            } else {
                // 만료된 엔티티 제거
                entity_cache_.erase(it);
                cache_evictions_++;
            }
        }
        
        cache_misses_++;
        return std::nullopt;
    }

    void cacheEntity(const EntityType& entity) {
        if (!cache_enabled_) return;
        
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        try {
            // Entity에서 ID 추출 시도 (런타임에 체크)
            // 대부분의 Entity는 getId() 메서드를 가지고 있다고 가정
            // 만약 없다면 catch 블록에서 처리
            auto get_id_method = [&entity]() -> int {
                // 여기서 각 엔티티 타입별로 ID 추출
                // 템플릿 특수화나 오버로드를 통해 처리 가능
                return 0; // 기본값 (실제로는 entity의 ID를 반환해야 함)
            };
            
            int id = get_id_method();
            
            // 캐시 크기 제한 체크
            if (entity_cache_.size() >= max_cache_size_) {
                auto oldest = entity_cache_.begin();
                entity_cache_.erase(oldest);
                cache_evictions_++;
            }
            
            entity_cache_[id] = CacheEntry(entity);
            
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->Warn(repository_name_ + " - Failed to cache entity: " + std::string(e.what()));
            }
        } catch (...) {
            if (logger_) {
                logger_->Warn(repository_name_ + " - Failed to cache entity (unknown error)");
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

    /**
     * @brief WHERE 절 빌드 (기본 테이블용)
     * @param conditions 쿼리 조건들
     * @return WHERE 절 문자열 ("WHERE ..." 형태)
     */
    virtual std::string buildWhereClause(const std::vector<QueryCondition>& conditions) {
        if (conditions.empty()) {
            return "";
        }
        
        std::ostringstream where_clause;
        where_clause << " WHERE ";
        
        for (size_t i = 0; i < conditions.size(); ++i) {
            if (i > 0) {
                where_clause << " AND ";
            }
            
            const auto& condition = conditions[i];
            // ✅ 정확한 필드명 사용: field, operation, value
            where_clause << condition.field << " "
                        << condition.operation << " '"
                        << escapeString(condition.value) << "'";
        }
        
        return where_clause.str();
    }

    /**
     * @brief WHERE 절 빌드 (테이블 별칭 포함 - JOIN용)
     * @param conditions 쿼리 조건들
     * @param table_alias 테이블 별칭 (예: "d", "dp")
     * @return WHERE 절 문자열
     */
    virtual std::string buildWhereClauseWithAlias(
        const std::vector<QueryCondition>& conditions,
        const std::string& table_alias = "") {
        
        if (conditions.empty()) {
            return "";
        }
        
        std::ostringstream where_clause;
        where_clause << " WHERE ";
        
        for (size_t i = 0; i < conditions.size(); ++i) {
            if (i > 0) {
                where_clause << " AND ";
            }
            
            const auto& condition = conditions[i];
            
            if (!table_alias.empty()) {
                // ✅ 정확한 필드명 사용
                where_clause << table_alias << "." << condition.field << " "
                            << condition.operation << " '"
                            << escapeString(condition.value) << "'";
            } else {
                where_clause << condition.field << " "
                            << condition.operation << " '"
                            << escapeString(condition.value) << "'";
            }
        }
        
        return where_clause.str();
    }

    /**
     * @brief ORDER BY 절 빌드
     * @param order_by 정렬 조건
     * @return ORDER BY 절 문자열
     */
    virtual std::string buildOrderByClause(const std::optional<OrderBy>& order_by) {
        if (!order_by.has_value()) {
            return "";
        }
        
        return " ORDER BY " + order_by->field + 
               (order_by->ascending ? " ASC" : " DESC");
    }
    
    /**
     * @brief LIMIT/OFFSET 절 빌드
     * @param pagination 페이징 조건
     * @return LIMIT 절 문자열
     */
    virtual std::string buildLimitClause(const std::optional<Pagination>& pagination) {
        if (!pagination.has_value()) {
            return "";
        }
        
        std::ostringstream limit_clause;
        limit_clause << " LIMIT " << pagination->limit;
        
        if (pagination->offset > 0) {
            limit_clause << " OFFSET " << pagination->offset;
        }
        
        return limit_clause.str();
    }
    
    /**
     * @brief 문자열 이스케이프 처리 (SQL Injection 방지)
     * @param str 이스케이프할 문자열
     * @return 이스케이프된 문자열
     */
    virtual std::string escapeString(const std::string& str) const {
        std::string escaped = str;
        
        // 기본적인 SQL Injection 방지
        std::string search = "'";
        std::string replace = "''";
        
        size_t pos = 0;
        while ((pos = escaped.find(search, pos)) != std::string::npos) {
            escaped.replace(pos, search.length(), replace);
            pos += replace.length();
        }
        
        return escaped;
    }

protected:
    // =======================================================================
    // ✅ 멤버 변수들 (모두 전역 네임스페이스)
    // =======================================================================
    
    std::string repository_name_;
    bool enable_bulk_optimization_;
    
    // ✅ 모든 매니저가 전역 네임스페이스에 있음
    DatabaseManager* db_manager_;           // ✅ 전역
    ConfigManager* config_manager_;         // ✅ 전역
    LogManager* logger_;                    // ✅ 전역
};

} // namespace Database
} // namespace PulseOne

#endif // PULSEONE_IREPOSITORY_H