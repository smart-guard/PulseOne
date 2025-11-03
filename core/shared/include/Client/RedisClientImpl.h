// =============================================================================
// RedisClientImpl.h - 완전한 Redis 클라이언트 구현 (중복 제거 완료)
// =============================================================================
#ifndef REDIS_CLIENT_IMPL_H
#define REDIS_CLIENT_IMPL_H

#include "Client/RedisClient.h"
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <functional>

// hiredis 라이브러리 체크
#ifdef HAVE_REDIS
#include <hiredis/hiredis.h>
#endif

/**
 * @brief 완전 자동화된 Redis 클라이언트 구현체
 * @details 모든 Redis 명령어를 완전히 구현하고 자동 연결 관리 제공
 */
class RedisClientImpl : public RedisClient {
public:
    // =============================================================================
    // 생성자/소멸자 - 완전 자동화
    // =============================================================================
    
    RedisClientImpl();
    ~RedisClientImpl() override;
    
    // 복사/이동 방지 (싱글톤 패턴)
    RedisClientImpl(const RedisClientImpl&) = delete;
    RedisClientImpl& operator=(const RedisClientImpl&) = delete;
    RedisClientImpl(RedisClientImpl&&) = delete;
    RedisClientImpl& operator=(RedisClientImpl&&) = delete;
    
    // =============================================================================
    // 기본 연결 관리 (RedisClient 인터페이스 구현)
    // =============================================================================
    
    bool connect(const std::string& host, int port, const std::string& password = "") override;
    void disconnect() override;
    bool isConnected() const override;
    
    // =============================================================================
    // Key-Value 조작 (RedisClient 인터페이스 구현)
    // =============================================================================
    
    bool set(const std::string& key, const std::string& value) override;
    bool setex(const std::string& key, const std::string& value, int expire_seconds) override;
    std::string get(const std::string& key) override;
    int del(const std::string& key) override;
    bool exists(const std::string& key) override;
    bool expire(const std::string& key, int seconds) override;
    int ttl(const std::string& key) override;
    int incr(const std::string& key, int increment = 1) override;
    StringList keys(const std::string& pattern) override;
    // =============================================================================
    // Hash 조작 (RedisClient 인터페이스 구현)
    // =============================================================================
    
    bool hset(const std::string& key, const std::string& field, const std::string& value) override;
    std::string hget(const std::string& key, const std::string& field) override;
    StringMap hgetall(const std::string& key) override;
    int hdel(const std::string& key, const std::string& field) override;
    bool hexists(const std::string& key, const std::string& field) override;
    int hlen(const std::string& key) override;
    
    // =============================================================================
    // List 조작 (RedisClient 인터페이스 구현)
    // =============================================================================
    
    int lpush(const std::string& key, const std::string& value) override;
    int rpush(const std::string& key, const std::string& value) override;
    std::string lpop(const std::string& key) override;
    std::string rpop(const std::string& key) override;
    StringList lrange(const std::string& key, int start, int stop) override;
    int llen(const std::string& key) override;
    
    // =============================================================================
    // Set 조작 (RedisClient 인터페이스 구현)
    // =============================================================================
    
    int sadd(const std::string& key, const std::string& member) override;
    int srem(const std::string& key, const std::string& member) override;
    bool sismember(const std::string& key, const std::string& member) override;
    StringList smembers(const std::string& key) override;
    int scard(const std::string& key) override;
    
    // =============================================================================
    // Sorted Set 조작 (RedisClient 인터페이스 구현)
    // =============================================================================
    
    int zadd(const std::string& key, double score, const std::string& member) override;
    int zrem(const std::string& key, const std::string& member) override;
    StringList zrange(const std::string& key, int start, int stop) override;
    int zcard(const std::string& key) override;
    
    // =============================================================================
    // Pub/Sub (RedisClient 인터페이스 구현)
    // =============================================================================
    
    int publish(const std::string& channel, const std::string& message) override;
    bool subscribe(const std::string& channel) override;
    bool unsubscribe(const std::string& channel) override;
    bool psubscribe(const std::string& pattern) override;
    bool punsubscribe(const std::string& pattern) override;
    void setMessageCallback(MessageCallback callback) override;
    bool waitForMessage(int timeout_ms = 100) override;
    // =============================================================================
    // 배치 처리 (RedisClient 인터페이스 구현)
    // =============================================================================
    
    bool mset(const StringMap& key_values) override;
    StringList mget(const StringList& keys) override;
    
    // =============================================================================
    // 트랜잭션 지원 (RedisClient 인터페이스 구현)
    // =============================================================================
    
    bool multi() override;
    bool exec() override;
    bool discard() override;
    
    // =============================================================================
    // 상태 및 진단 (RedisClient 인터페이스 구현)
    // =============================================================================
    
    StringMap info() override;
    bool ping() override;
    bool select(int db_index) override;
    int dbsize() override;
    
    // =============================================================================
    // 추가 기능들 (자동화 및 편의성)
    // =============================================================================
    
    struct ConnectionStats {
        uint64_t total_commands{0};
        uint64_t successful_commands{0};
        uint64_t failed_commands{0};
        uint64_t total_reconnects{0};
        int current_reconnect_attempts{0};
        std::chrono::steady_clock::time_point connect_time;
        bool is_connected{false};
    };
    
    ConnectionStats getStats() const;
    void resetStats();
    bool forceReconnect();  // 테스트용 즉시 재연결

private:
    // =============================================================================
    // 내부 헬퍼 메서드들
    // =============================================================================
    
    // 설정 및 연결 관리
    void loadConfiguration();
    bool attemptConnection();
    bool ensureConnected();
    void connectionWatchdog();
    
    // 에러 처리 및 로깅
    void logInfo(const std::string& message) const;
    void logWarning(const std::string& message) const;
    void logError(const std::string& message) const;
    
    // 통계 업데이트
    void incrementCommand();
    void incrementSuccess();
    void incrementFailure();
    
    // =============================================================================
    // 템플릿 메서드 (헤더에 구현)
    // =============================================================================
    
    template<typename T>
    T executeWithRetry(std::function<T()> operation, T default_value) {
        total_commands_++;
        
        try {
            if (ensureConnected()) {
                T result = operation();
                successful_commands_++;
                return result;
            }
        } catch (const std::exception& e) {
            logError("Redis 작업 중 예외: " + std::string(e.what()));
        }
        
        failed_commands_++;
        return default_value;
    }
    
#ifdef HAVE_REDIS
    // hiredis 전용 메서드들
    redisReply* executeCommandSafe(const char* format, ...);
    std::string replyToString(redisReply* reply) const;
    long long replyToInteger(redisReply* reply) const;
    StringList replyToStringList(redisReply* reply) const;
    StringMap replyToStringMap(redisReply* reply) const;
    bool isReplyOK(redisReply* reply) const;
    bool isConnectionError() const;
#endif
    
    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    // hiredis 연결 컨텍스트
#ifdef HAVE_REDIS
    redisContext* context_{nullptr};
#else
    void* context_{nullptr};  // 호환성을 위한 더미
#endif
    
    // 연결 설정 (자동 로드됨)
    std::string host_{"localhost"};
    int port_{6379};
    std::string password_;
    int database_{0};
    
    // 상태 관리
    std::atomic<bool> connected_{false};
    std::atomic<bool> auto_reconnect_enabled_{true};
    std::atomic<bool> shutdown_requested_{false};
    
    // 재연결 관리
    std::atomic<int> reconnect_attempts_{0};
    std::atomic<uint64_t> total_reconnects_{0};
    std::chrono::steady_clock::time_point last_reconnect_attempt_;
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
    static constexpr std::chrono::milliseconds RECONNECT_DELAY{2000};
    static constexpr std::chrono::milliseconds CONNECTION_TIMEOUT{5000};
    static constexpr std::chrono::seconds WATCHDOG_INTERVAL{30};
    
    // 통계
    std::atomic<uint64_t> total_commands_{0};
    std::atomic<uint64_t> successful_commands_{0};
    std::atomic<uint64_t> failed_commands_{0};
    std::chrono::steady_clock::time_point connect_time_;
    
    // 스레드 안전성
    mutable std::recursive_mutex connection_mutex_;
    mutable std::mutex stats_mutex_;
    
    // 백그라운드 스레드
    std::unique_ptr<std::thread> watchdog_thread_;
    std::condition_variable watchdog_cv_;
    mutable std::mutex watchdog_mutex_;
    
    // Pub/Sub 콜백
    MessageCallback message_callback_;
    std::mutex callback_mutex_;
};

#endif // REDIS_CLIENT_IMPL_H