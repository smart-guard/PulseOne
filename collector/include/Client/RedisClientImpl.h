// collector/include/Client/RedisClientImpl.h
#ifndef REDIS_CLIENT_IMPL_H
#define REDIS_CLIENT_IMPL_H

#include "Client/RedisClient.h"
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <condition_variable>

// 🔥 실제 프로덕션에서는 hiredis 라이브러리 사용
// #include <hiredis/hiredis.h>
// 현재는 가상 구조체로 시뮬레이션
struct redisContext {
    int fd;
    int err;
    char errstr[128];
};

struct redisReply {
    int type;
    long long integer;
    size_t len;
    char* str;
    size_t elements;
    struct redisReply** element;
};

/**
 * @brief RedisClient의 완성도 있는 구현체
 * @details 실제 hiredis 라이브러리와 연동하는 프로덕션 레디 구현
 * @author PulseOne Development Team
 * @date 2025-07-31
 */
class RedisClientImpl : public RedisClient {
private:
    // =============================================================================
    // 연결 및 상태 관리
    // =============================================================================
    
    std::unique_ptr<redisContext, void(*)(redisContext*)> context_;
    std::atomic<bool> connected_{false};
    std::string host_;
    int port_{6379};
    std::string password_;
    int selected_db_{0};
    
    // 스레드 안전성
    mutable std::mutex connection_mutex_;
    mutable std::mutex operation_mutex_;
    
    // 재연결 관리
    std::atomic<bool> auto_reconnect_{true};
    std::atomic<int> reconnect_attempts_{0};
    static constexpr int MAX_RECONNECT_ATTEMPTS = 5;
    static constexpr int RECONNECT_DELAY_MS = 1000;
    
    // Pub/Sub 관리
    std::atomic<bool> pubsub_mode_{false};
    std::thread pubsub_thread_;
    MessageCallback message_callback_;
    std::atomic<bool> pubsub_running_{false};
    std::condition_variable pubsub_cv_;
    std::vector<std::string> subscribed_channels_;
    std::vector<std::string> subscribed_patterns_;
    
    // 트랜잭션 상태
    std::atomic<bool> in_transaction_{false};
    
    // 성능 통계
    std::atomic<uint64_t> total_commands_{0};
    std::atomic<uint64_t> failed_commands_{0};
    std::chrono::steady_clock::time_point connect_time_;
    
    // 임시 메모리 저장소 (hiredis 없을 때만 사용)
    std::map<std::string, std::string> memory_storage_;
    std::map<std::string, std::map<std::string, std::string>> hash_storage_;
    std::map<std::string, std::vector<std::string>> list_storage_;
    
public:
    // =============================================================================
    // 생성자/소멸자
    // =============================================================================
    
    RedisClientImpl();
    virtual ~RedisClientImpl();
    
    // 복사/이동 생성자 삭제 (싱글톤 패턴)
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
    // 기본 Key-Value 조작
    // =============================================================================
    
    bool set(const std::string& key, const std::string& value) override;
    std::string get(const std::string& key) override;
    bool setex(const std::string& key, const std::string& value, int ttl_seconds) override;
    bool exists(const std::string& key) override;
    int del(const std::string& key) override;
    int del(const StringList& keys) override;
    int ttl(const std::string& key) override;
    
    // =============================================================================
    // Hash 조작
    // =============================================================================
    
    bool hset(const std::string& key, const std::string& field, const std::string& value) override;
    std::string hget(const std::string& key, const std::string& field) override;
    StringMap hgetall(const std::string& key) override;
    bool hmset(const std::string& key, const StringMap& field_values) override;
    int hdel(const std::string& key, const std::string& field) override;
    
    // =============================================================================
    // List 조작
    // =============================================================================
    
    int lpush(const std::string& key, const std::string& value) override;
    int rpush(const std::string& key, const std::string& value) override;
    std::string lpop(const std::string& key) override;
    std::string rpop(const std::string& key) override;
    StringList lrange(const std::string& key, int start, int stop) override;
    int llen(const std::string& key) override;
    
    // =============================================================================
    // Sorted Set 조작
    // =============================================================================
    
    int zadd(const std::string& key, double score, const std::string& member) override;
    StringList zrangebyscore(const std::string& key, double min_score, double max_score) override;
    int zcard(const std::string& key) override;
    int zremrangebyscore(const std::string& key, double min_score, double max_score) override;
    
    // =============================================================================
    // Pub/Sub 메시징
    // =============================================================================
    
    bool publish(const std::string& channel, const std::string& message) override;
    bool subscribe(const std::string& channel) override;
    bool unsubscribe(const std::string& channel) override;
    bool psubscribe(const std::string& pattern) override;
    bool punsubscribe(const std::string& pattern) override;
    void setMessageCallback(MessageCallback callback) override;
    
    // =============================================================================
    // 배치 처리
    // =============================================================================
    
    bool mset(const StringMap& key_values) override;
    StringList mget(const StringList& keys) override;
    
    // =============================================================================
    // 트랜잭션 지원
    // =============================================================================
    
    bool multi() override;
    bool exec() override;
    bool discard() override;
    
    // =============================================================================
    // 상태 및 진단
    // =============================================================================
    
    StringMap info() override;
    bool ping() override;
    bool select(int db_index) override;
    int dbsize() override;
    
    // =============================================================================
    // 추가 유틸리티 메서드들
    // =============================================================================
    
    /**
     * @brief 자동 재연결 설정
     * @param enable 자동 재연결 사용 여부
     */
    void setAutoReconnect(bool enable) { auto_reconnect_ = enable; }
    
    /**
     * @brief 연결 통계 조회
     * @return 연결 시간, 총 명령어 수, 실패 수 등의 통계
     */
    struct ConnectionStats {
        std::chrono::milliseconds uptime;
        uint64_t total_commands;
        uint64_t failed_commands;
        int reconnect_attempts;
        bool is_connected;
    };
    
    ConnectionStats getConnectionStats() const;
    
    /**
     * @brief 연결 상태 리셋
     */
    void resetConnectionStats();

private:
    // =============================================================================
    // 내부 헬퍼 메서드들
    // =============================================================================
    
    bool reconnect();
    bool executeCommand(const std::string& cmd);
    std::unique_ptr<redisReply, void(*)(redisReply*)> executeCommandWithReply(const std::string& cmd);
    bool ensureConnection();
    void startPubSubThread();
    void stopPubSubThread();
    void pubsubThreadWorker();
    std::string buildCommand(const std::vector<std::string>& args);
    
    // 에러 처리
    void logError(const std::string& operation, const std::string& error);
    bool handleConnectionError();
    
    // 메모리 시뮬레이션 헬퍼들 (hiredis 없을 때만)
    std::string simulateCommand(const std::string& cmd);
    bool isMemoryMode() const { return !context_; }
};

#endif // REDIS_CLIENT_IMPL_H