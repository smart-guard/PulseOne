/**
 * @file ClientCacheManager.h
 * @brief HTTP/S3/File 클라이언트 캐싱 매니저 (Thread-safe)
 * @author PulseOne Development Team
 * @date 2025-11-04
 * @version 1.0.0
 * 저장 위치: core/export-gateway/include/Utils/ClientCacheManager.h
 * 
 * 목적:
 * - 동일 설정의 클라이언트 재사용으로 성능 최적화
 * - Thread-safe 클라이언트 캐싱
 * - 자동 리소스 정리
 * 
 * 패턴:
 * - Stateless Handler + Client Caching
 * - 각 핸들러는 이 매니저를 통해 클라이언트 획득
 */

#ifndef CLIENT_CACHE_MANAGER_H
#define CLIENT_CACHE_MANAGER_H

#include <memory>
#include <unordered_map>
#include <mutex>
#include <string>
#include <functional>
#include <chrono>

namespace PulseOne {
namespace Utils {

/**
 * @brief 클라이언트 캐싱 매니저 (템플릿)
 * @tparam ClientType 캐싱할 클라이언트 타입 (HttpClient, S3Client 등)
 * @tparam ConfigType 클라이언트 생성에 필요한 설정 타입
 * 
 * 사용 예:
 * ClientCacheManager<HttpClient, HttpRequestOptions> http_cache;
 * auto client = http_cache.getOrCreate("http://api.com", config);
 */
template<typename ClientType, typename ConfigType>
class ClientCacheManager {
public:
    using ClientPtr = std::shared_ptr<ClientType>;
    using ClientFactory = std::function<ClientPtr(const ConfigType&)>;
    
    /**
     * @brief 생성자
     * @param factory 클라이언트 생성 팩토리 함수
     * @param max_idle_seconds 최대 유휴 시간 (초) - 이 시간 경과 후 자동 제거
     */
    explicit ClientCacheManager(
        ClientFactory factory,
        int max_idle_seconds = 300  // 기본 5분
    ) : factory_(factory), max_idle_seconds_(max_idle_seconds) {}
    
    /**
     * @brief 클라이언트 가져오기 또는 생성
     * @param key 캐시 키 (URL, 버킷명 등 고유 식별자)
     * @param config 클라이언트 설정
     * @return 클라이언트 공유 포인터
     */
    ClientPtr getOrCreate(const std::string& key, const ConfigType& config) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        // 캐시 확인
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            // 유효성 체크
            if (auto client = it->second.client.lock()) {
                it->second.last_access = std::chrono::steady_clock::now();
                return client;
            } else {
                // 만료된 weak_ptr 제거
                cache_.erase(it);
            }
        }
        
        // 새로 생성
        auto client = factory_(config);
        if (client) {
            CacheEntry entry;
            entry.client = client;
            entry.last_access = std::chrono::steady_clock::now();
            cache_[key] = entry;
        }
        
        // 주기적 정리 (100개마다)
        if (++access_count_ % 100 == 0) {
            cleanupExpired();
        }
        
        return client;
    }
    
    /**
     * @brief 특정 키의 캐시 제거
     */
    void remove(const std::string& key) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cache_.erase(key);
    }
    
    /**
     * @brief 모든 캐시 제거
     */
    void clear() {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cache_.clear();
    }
    
    /**
     * @brief 캐시 통계
     */
    struct Stats {
        size_t total_entries;
        size_t active_clients;
        size_t expired_entries;
    };
    
    Stats getStats() const {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        Stats stats{};
        stats.total_entries = cache_.size();
        stats.active_clients = 0;
        stats.expired_entries = 0;
        
        for (const auto& [key, entry] : cache_) {
            if (entry.client.expired()) {
                stats.expired_entries++;
            } else {
                stats.active_clients++;
            }
        }
        
        return stats;
    }
    
private:
    struct CacheEntry {
        std::weak_ptr<ClientType> client;
        std::chrono::steady_clock::time_point last_access;
    };
    
    /**
     * @brief 만료된 캐시 항목 정리
     */
    void cleanupExpired() {
        auto now = std::chrono::steady_clock::now();
        auto max_idle = std::chrono::seconds(max_idle_seconds_);
        
        for (auto it = cache_.begin(); it != cache_.end();) {
            // weak_ptr이 만료되었거나 유휴 시간 초과
            if (it->second.client.expired() ||
                (now - it->second.last_access) > max_idle) {
                it = cache_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    ClientFactory factory_;
    int max_idle_seconds_;
    mutable std::mutex cache_mutex_;
    std::unordered_map<std::string, CacheEntry> cache_;
    size_t access_count_ = 0;
};

} // namespace Utils
} // namespace PulseOne

#endif // CLIENT_CACHE_MANAGER_H