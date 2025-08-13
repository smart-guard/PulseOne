// =============================================================================
// 🔥 개선된 RedisClientImpl.h - 완전 자동화 버전
// 다른 클래스는 그냥 생성만 하면 끝!
// =============================================================================

#ifndef REDIS_CLIENT_IMPL_H
#define REDIS_CLIENT_IMPL_H

#include "Client/RedisClient.h"
#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <unordered_set>
#include <chrono>
#include <map>

// hiredis 헤더 포함
#ifdef HAS_HIREDIS
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#else
struct redisContext;
struct redisAsyncContext;  
struct redisReply;
#endif

/**
 * @brief 완전 자동화된 Redis 클라이언트 구현체
 * @details 생성만 하면 자동으로 설정 읽고 연결, 재연결, 에러 처리 모두 자동
 */
class RedisClientImpl : public RedisClient {
public:
    // =============================================================================
    // 🔥 사용자 친화적 인터페이스 - 이것만 알면 됨!
    // =============================================================================
    
    /**
     * @brief 기본 생성자 - 자동으로 설정 읽고 연결 시도
     */
    RedisClientImpl();
    
    /**
     * @brief 소멸자 - 자동으로 연결 해제
     */
    ~RedisClientImpl() override;
    
    /**
     * @brief 현재 연결 상태 확인
     * @return 연결되어 있으면 true
     */
    bool isConnected() const override;
    
    /**
     * @brief 수동 재연결 (보통 필요 없음, 자동으로 처리됨)
     * @return 재연결 성공 시 true
     */
    bool reconnect();
    
    // =============================================================================
    // 🔥 Redis 명령어들 - 자동으로 연결 상태 확인하고 재연결까지!
    // =============================================================================
    
    // Key-Value 조작
    bool set(const std::string& key, const std::string& value) override;
    bool setex(const std::string& key, const std::string& value, int expire_seconds) override;
    std::string get(const std::string& key) override;
    int del(const std::string& key) override;
    bool exists(const std::string& key) override;
    bool expire(const std::string& key, int seconds) override;
    int ttl(const std::string& key) override;
    
    // Hash 조작
    bool hset(const std::string& key, const std::string& field, const std::string& value) override;
    std::string hget(const std::string& key, const std::string& field) override;
    StringMap hgetall(const std::string& key) override;
    int hdel(const std::string& key, const std::string& field) override;
    bool hexists(const std::string& key, const std::string& field) override;
    int hlen(const std::string& key) override;
    
    // List, Set, Sorted Set, Pub/Sub, 트랜잭션 등 모든 명령어
    // (기존과 동일하지만 내부에서 자동 연결 관리)
    int lpush(const std::string& key, const std::string& value) override;
    int rpush(const std::string& key, const std::string& value) override;
    std::string lpop(const std::string& key) override;
    std::string rpop(const std::string& key) override;
    StringList lrange(const std::string& key, int start, int stop) override;
    int llen(const std::string& key) override;
    
    int sadd(const std::string& key, const std::string& member) override;
    int srem(const std::string& key, const std::string& member) override;
    bool sismember(const std::string& key, const std::string& member) override;
    StringList smembers(const std::string& key) override;
    int scard(const std::string& key) override;
    
    int publish(const std::string& channel, const std::string& message) override;
    bool ping() override;
    
    // =============================================================================
    // 🔥 진단 및 통계 (선택적 사용)
    // =============================================================================
    
    struct ConnectionStats {
        bool is_connected;
        std::string host;
        int port;
        uint64_t total_commands;
        uint64_t successful_commands;
        uint64_t failed_commands;
        uint64_t reconnect_count;
        std::chrono::steady_clock::time_point last_connect_time;
    };
    
    ConnectionStats getStats() const;
    void resetStats();

private:
    // =============================================================================
    // 🔥 내부 구현 - 사용자가 신경 쓸 필요 없음
    // =============================================================================
    
    // 복사/이동 방지
    RedisClientImpl(const RedisClientImpl&) = delete;
    RedisClientImpl& operator=(const RedisClientImpl&) = delete;
    
    // 연결 관리 (완전 자동화)
    void loadConfiguration();           // 설정 파일에서 자동 로드
    bool attemptConnection();           // 연결 시도
    bool ensureConnected();            // 연결 보장 (명령 실행 전 호출)
    void scheduleReconnect();          // 백그라운드 재연결 스케줄링
    void connectionWatchdog();         // 연결 감시 스레드
    
    // 명령 실행 (자동 재연결 포함)
    template<typename T>
    T executeWithRetry(std::function<T()> operation, T default_value);
    
    redisReply* executeCommand(const char* format, ...);
    redisReply* executeCommandSafe(const char* format, ...);  // 재연결 포함
    
    // 응답 처리
    std::string replyToString(redisReply* reply) const;
    long long replyToInteger(redisReply* reply) const;
    StringList replyToStringList(redisReply* reply) const;
    StringMap replyToStringMap(redisReply* reply) const;
    bool isReplyOK(redisReply* reply) const;
    
    // 에러 처리 및 로깅
    void logInfo(const std::string& message) const;
    void logWarning(const std::string& message) const;
    void logError(const std::string& message) const;
    bool isConnectionError() const;
    
    // 멤버 변수
    redisContext* context_{nullptr};
    
    // 연결 설정 (자동 로드)
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
};

#endif // REDIS_CLIENT_IMPL_H