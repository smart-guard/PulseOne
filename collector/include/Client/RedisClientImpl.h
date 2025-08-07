// =============================================================================
// collector/include/Client/RedisClientImpl.h
// 완전한 Redis 클라이언트 구현체 - 실제 동작하는 버전
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
// hiredis가 없을 때의 forward declaration
struct redisContext;
struct redisAsyncContext;  
struct redisReply;
#endif

/**
 * @brief 완전한 Redis 클라이언트 구현체
 * @details hiredis 라이브러리를 사용한 완전한 Redis 클라이언트
 */
class RedisClientImpl : public RedisClient {
private:
    // =============================================================================
    // 멤버 변수
    // =============================================================================
    
    // Redis 연결 컨텍스트 (raw pointer 사용)
    redisContext* context_;
    redisAsyncContext* async_context_;
    
    // 연결 정보
    std::string host_;
    int port_{6379};
    std::string password_;
    int selected_db_{0};
    std::atomic<bool> connected_{false};
    
    // 통계 정보
    std::atomic<uint64_t> total_commands_{0};
    std::atomic<uint64_t> successful_commands_{0};
    std::atomic<uint64_t> failed_commands_{0};
    std::chrono::steady_clock::time_point connect_time_;
    std::chrono::steady_clock::time_point last_command_time_;
    
    // 스레드 안전성
    mutable std::recursive_mutex connection_mutex_;
    mutable std::mutex operation_mutex_;
    mutable std::mutex pubsub_mutex_;
    
    // 재연결 관리
    std::atomic<bool> auto_reconnect_{true};
    std::atomic<int> reconnect_attempts_{0};
    std::chrono::steady_clock::time_point last_reconnect_attempt_;
    static constexpr int MAX_RECONNECT_ATTEMPTS = 5;
    static constexpr std::chrono::milliseconds RECONNECT_DELAY{2000};
    static constexpr std::chrono::milliseconds CONNECTION_TIMEOUT{5000};
    static constexpr std::chrono::milliseconds COMMAND_TIMEOUT{3000};
    
    // Pub/Sub 관리
    std::atomic<bool> pubsub_mode_{false};
    std::thread pubsub_thread_;
    std::atomic<bool> pubsub_running_{false};
    std::condition_variable pubsub_cv_;
    MessageCallback message_callback_;
    std::unordered_set<std::string> subscribed_channels_;
    std::unordered_set<std::string> subscribed_patterns_;
    
    // 트랜잭션 관리
    std::atomic<bool> in_transaction_{false};
    std::queue<std::string> transaction_commands_;
    
    // Pipeline 관리
    std::queue<std::string> pipeline_commands_;
    std::atomic<bool> pipeline_mode_{false};

public:
    // =============================================================================
    // 생성자/소멸자
    // =============================================================================
    
    RedisClientImpl();
    ~RedisClientImpl() override;
    
    // 복사/이동 생성자 및 대입 연산자 삭제
    RedisClientImpl(const RedisClientImpl&) = delete;
    RedisClientImpl& operator=(const RedisClientImpl&) = delete;
    RedisClientImpl(RedisClientImpl&&) = delete;
    RedisClientImpl& operator=(RedisClientImpl&&) = delete;
    
    // =============================================================================
    // RedisClient 인터페이스 구현
    // =============================================================================
    
    // 연결 관리
    bool connect(const std::string& host, int port, const std::string& password = "") override;
    void disconnect() override;
    bool isConnected() const override;
    
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
    
    // List 조작
    int lpush(const std::string& key, const std::string& value) override;
    int rpush(const std::string& key, const std::string& value) override;
    std::string lpop(const std::string& key) override;
    std::string rpop(const std::string& key) override;
    StringList lrange(const std::string& key, int start, int stop) override;
    int llen(const std::string& key) override;
    
    // Set 조작
    int sadd(const std::string& key, const std::string& member) override;
    int srem(const std::string& key, const std::string& member) override;
    bool sismember(const std::string& key, const std::string& member) override;
    StringList smembers(const std::string& key) override;
    int scard(const std::string& key) override;
    
    // Sorted Set 조작
    int zadd(const std::string& key, double score, const std::string& member) override;
    int zrem(const std::string& key, const std::string& member) override;
    StringList zrange(const std::string& key, int start, int stop) override;
    int zcard(const std::string& key) override;
    
    // Pub/Sub
    bool subscribe(const std::string& channel) override;
    bool unsubscribe(const std::string& channel) override;
    int publish(const std::string& channel, const std::string& message) override;
    bool psubscribe(const std::string& pattern) override;
    bool punsubscribe(const std::string& pattern) override;
    void setMessageCallback(MessageCallback callback) override;
    
    // 배치 처리
    bool mset(const StringMap& key_values) override;
    StringList mget(const StringList& keys) override;
    
    // 트랜잭션
    bool multi() override;
    bool exec() override;
    bool discard() override;
    
    // 상태 및 진단
    StringMap info() override;
    bool ping() override;
    bool select(int db_index) override;
    int dbsize() override;

private:
    // =============================================================================
    // 내부 헬퍼 메서드들
    // =============================================================================
    
    // 연결 관리
    bool connectInternal();
    bool reconnect();
    void setupContext(redisContext* ctx);
    bool authenticateIfNeeded();
    bool selectDatabase(int db_index);
    bool handleConnectionError();
    
    // 명령 실행
    redisReply* executeCommand(const char* format, ...);
    redisReply* executeCommandArgv(int argc, const char** argv, const size_t* argvlen);
    
    // 응답 처리
    std::string replyToString(redisReply* reply) const;
    long long replyToInteger(redisReply* reply) const;
    StringList replyToStringList(redisReply* reply) const;
    StringMap replyToStringMap(redisReply* reply) const;
    bool isReplyOK(redisReply* reply) const;
    bool isReplyError(redisReply* reply) const;
    
    // 에러 처리
    void logError(const std::string& operation, const std::string& error_message) const;
    void logRedisError(const std::string& operation, redisContext* ctx) const;
    bool isConnectionError(redisContext* ctx) const;
    
    // 통계 및 성능
    void recordCommandStart();
    void recordCommandEnd(bool success);
    std::chrono::milliseconds calculateLatency() const;
    
    // Pub/Sub 관리
    void startPubSubThread();
    void stopPubSubThread();
    void pubsubThreadWorker();
    void handlePubSubMessage(redisReply* reply);
    
    // 메모리 관리
    static void freeRedisReply(redisReply* reply);
    static void freeRedisContext(redisContext* ctx);
    static void freeRedisAsyncContext(redisAsyncContext* ctx);

public:
    // =============================================================================
    // 추가 기능 (구현체 전용)
    // =============================================================================
    
    /**
     * @brief 연결 통계 구조체
     */
    struct ConnectionStats {
        uint64_t total_commands;
        uint64_t successful_commands;
        uint64_t failed_commands;
        std::chrono::steady_clock::time_point connect_time;
        std::chrono::steady_clock::time_point last_command_time;
        bool is_connected;
        std::string host;
        int port;
        int selected_db;
    };
    
    /**
     * @brief 연결 통계 조회
     * @return 연결 통계
     */
    ConnectionStats getConnectionStats() const;
    
    /**
     * @brief 통계 리셋
     */
    void resetStats();
    
    /**
     * @brief 다중 키 삭제 (확장 기능)
     * @param keys 삭제할 키들
     * @return 삭제된 키의 수
     */
    int del(const StringList& keys);
};

#endif // REDIS_CLIENT_IMPL_H