// collector/include/Client/RedisClientImpl.h
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

// 🔧 조건부 hiredis 포함
#ifdef HAS_HIREDIS
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#else
// hiredis가 없을 때의 fallback 구조체들
struct redisContext;
struct redisAsyncContext;
struct redisReply;
#endif

/**
 * @brief 간소화된 RedisClient 구현체 (에러 해결용)
 * @details 기존 구현 파일과 완전히 호환되는 헤더
 */
class RedisClientImpl : public RedisClient {
private:
    // =============================================================================
    // 멤버 변수들 (구현 파일에 사용된 모든 변수들)
    // =============================================================================
    
#ifdef HAS_HIREDIS
    std::unique_ptr<redisContext, void(*)(redisContext*)> context_;
    std::unique_ptr<redisAsyncContext, void(*)(redisAsyncContext*)> async_context_;
#else
    void* context_;
    void* async_context_;
#endif
    
    // 연결 정보
    std::string host_;
    int port_{6379};
    std::string password_;
    int selected_db_{0};
    std::atomic<bool> connected_{false};
    
    // 스레드 안전성
    mutable std::recursive_mutex connection_mutex_;
    mutable std::mutex operation_mutex_;
    mutable std::mutex pubsub_mutex_;
    mutable std::mutex async_mutex_;
    
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
    
    // 비동기 작업 관리
    std::thread async_thread_;
    std::atomic<bool> async_running_{false};
    std::queue<std::function<void()>> async_task_queue_;
    std::condition_variable async_cv_;
    
    // 트랜잭션 상태
    std::atomic<bool> in_transaction_{false};
    std::vector<std::string> transaction_commands_;
    
    // 파이프라인 지원
    std::atomic<bool> pipeline_mode_{false};
    std::vector<std::string> pipeline_commands_;
    
    // 성능 통계
    std::atomic<uint64_t> total_commands_{0};
    std::atomic<uint64_t> successful_commands_{0};
    std::atomic<uint64_t> failed_commands_{0};
    std::chrono::steady_clock::time_point connect_time_;
    std::chrono::steady_clock::time_point last_command_time_;
    
    // =============================================================================
    // 내부 헬퍼 메서드들 (구현 파일에 있는 모든 메서드들)
    // =============================================================================
    
    // 연결 관리
    bool connectInternal();
    bool reconnect();
    void setupContext(redisContext* ctx);
    bool authenticateIfNeeded();
    bool selectDatabase(int db_index);
    
    // 명령어 실행
#ifdef HAS_HIREDIS
    std::unique_ptr<redisReply, void(*)(redisReply*)> executeCommand(const char* format, ...);
    std::unique_ptr<redisReply, void(*)(redisReply*)> executeCommandArgv(int argc, const char** argv, const size_t* argvlen);
#else
    void* executeCommand(const char* format, ...);
    void* executeCommandArgv(int argc, const char** argv, const size_t* argvlen);
#endif
    
    // Pub/Sub 관리
    void startPubSubThread();
    void stopPubSubThread();  
    void pubsubThreadWorker();
#ifdef HAS_HIREDIS
    void handlePubSubMessage(redisReply* reply);
#else
    void handlePubSubMessage(void* reply);
#endif
    
    // 비동기 작업 관리
    void startAsyncThread();
    void stopAsyncThread();
    void asyncThreadWorker();
    
    // 에러 처리
    void logError(const std::string& operation, const std::string& error_message) const;
    void logRedisError(const std::string& operation, redisContext* ctx);
    bool handleConnectionError();
    bool isConnectionError(redisContext* ctx) const;
    
    // 유틸리티
#ifdef HAS_HIREDIS
    std::string replyToString(redisReply* reply) const;
    long long replyToInteger(redisReply* reply) const;
    StringList replyToStringList(redisReply* reply) const;
    StringMap replyToStringMap(redisReply* reply) const;
    bool isReplyOK(redisReply* reply);
    bool isReplyError(redisReply* reply);
#else
    std::string replyToString(void* reply) const;
    long long replyToInteger(void* reply) const;
    StringList replyToStringList(void* reply) const;
    StringMap replyToStringMap(void* reply) const;
    bool isReplyOK(void* reply);
    bool isReplyError(void* reply);
#endif
    
    // 문자열 처리
    std::string escapeString(const std::string& str);
    std::vector<std::string> splitCommand(const std::string& command);
    
    // 성능 측정
    void recordCommandStart();
    void recordCommandEnd(bool success);
    std::chrono::milliseconds calculateLatency() const;
    
    // 메모리 관리
    static void freeRedisReply(redisReply* reply);
    static void freeRedisContext(redisContext* ctx);
    static void freeRedisAsyncContext(redisAsyncContext* ctx);
    
    // 추가 메서드들 (구현 파일에 있는 것들)
    bool hmset(const std::string& key, const StringMap& field_values);
    int del(const StringList& keys);
    StringList zrangebyscore(const std::string& key, double min_score, double max_score);
    int zremrangebyscore(const std::string& key, double min_score, double max_score);
    std::string evalScript(const std::string& script, const StringList& keys = StringList{}, const StringList& args = StringList{});
    std::string evalSha(const std::string& sha1, const StringList& keys = StringList{}, const StringList& args = StringList{});
    std::string scriptLoad(const std::string& script);
    std::pair<std::string, StringList> scan(const std::string& cursor = "0", const std::string& pattern = "", int count = 10);
    std::pair<std::string, StringMap> hscan(const std::string& key, const std::string& cursor = "0", const std::string& pattern = "", int count = 10);
    std::pair<std::string, std::map<std::string, double>> zscan(const std::string& key, const std::string& cursor = "0", const std::string& pattern = "", int count = 10);
    void setTimeouts(int connect_timeout_ms, int command_timeout_ms);
    bool startPipeline();
    bool addToPipeline(const std::string& command);
#ifdef HAS_HIREDIS
    std::vector<std::unique_ptr<redisReply, void(*)(redisReply*)>> executePipeline();
#else
    std::vector<void*> executePipeline();
#endif

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
    
    // =============================================================================
    // 추가 기능 (구현체 전용)
    // =============================================================================
    
    /**
     * @brief 통계 구조체
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
     * @brief 통계 조회
     * @return 연결 통계
     */
    ConnectionStats getConnectionStats() const;
    
    /**
     * @brief 통계 리셋
     */
    void resetStats();
};

#endif // REDIS_CLIENT_IMPL_H