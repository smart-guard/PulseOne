// collector/include/Client/RedisClientImpl.h
#ifndef REDIS_CLIENT_IMPL_H
#define REDIS_CLIENT_IMPL_H

#include "Client/RedisClient.h"
#include <hiredis/hiredis.h>        // ✅ hiredis 라이브러리 사용
#include <hiredis/async.h>          // 비동기 지원
#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <unordered_set>

/**
 * @brief hiredis 라이브러리를 사용한 완전한 RedisClient 구현체
 * @details 실제 프로덕션 환경에서 사용 가능한 고성능 Redis 클라이언트
 * @author PulseOne Development Team
 * @date 2025-07-31
 */
class RedisClientImpl : public RedisClient {
private:
    // =============================================================================
    // hiredis 연결 관리
    // =============================================================================
    
    std::unique_ptr<redisContext, void(*)(redisContext*)> context_;
    std::unique_ptr<redisAsyncContext, void(*)(redisAsyncContext*)> async_context_;
    
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
    
    // 연결 풀 지원 (향후 확장용)
    static constexpr size_t MAX_POOL_SIZE = 10;
    bool pool_enabled_{false};

public:
    // =============================================================================
    // 생성자/소멸자
    // =============================================================================
    
    RedisClientImpl();
    virtual ~RedisClientImpl();
    
    // 복사/이동 생성자 삭제
    RedisClientImpl(const RedisClientImpl&) = delete;
    RedisClientImpl& operator=(const RedisClientImpl&) = delete;
    RedisClientImpl(RedisClientImpl&&) = delete;
    RedisClientImpl& operator=(RedisClientImpl&&) = delete;
    
    // =============================================================================
    // RedisClient 인터페이스 구현
    // =============================================================================
    
    // 기본 연결 관리
    bool connect(const std::string& host, int port, const std::string& password = "") override;
    void disconnect() override;
    bool isConnected() const override;
    
    // 기본 Key-Value 조작
    bool set(const std::string& key, const std::string& value) override;
    std::string get(const std::string& key) override;
    bool setex(const std::string& key, const std::string& value, int ttl_seconds) override;
    bool exists(const std::string& key) override;
    int del(const std::string& key) override;
    int del(const StringList& keys) override;
    int ttl(const std::string& key) override;
    
    // Hash 조작
    bool hset(const std::string& key, const std::string& field, const std::string& value) override;
    std::string hget(const std::string& key, const std::string& field) override;
    StringMap hgetall(const std::string& key) override;
    bool hmset(const std::string& key, const StringMap& field_values) override;
    int hdel(const std::string& key, const std::string& field) override;
    
    // List 조작
    int lpush(const std::string& key, const std::string& value) override;
    int rpush(const std::string& key, const std::string& value) override;
    std::string lpop(const std::string& key) override;
    std::string rpop(const std::string& key) override;
    StringList lrange(const std::string& key, int start, int stop) override;
    int llen(const std::string& key) override;
    
    // Sorted Set 조작
    int zadd(const std::string& key, double score, const std::string& member) override;
    StringList zrangebyscore(const std::string& key, double min_score, double max_score) override;
    int zcard(const std::string& key) override;
    int zremrangebyscore(const std::string& key, double min_score, double max_score) override;
    
    // Pub/Sub 메시징
    bool publish(const std::string& channel, const std::string& message) override;
    bool subscribe(const std::string& channel) override;
    bool unsubscribe(const std::string& channel) override;
    bool psubscribe(const std::string& pattern) override;
    bool punsubscribe(const std::string& pattern) override;
    void setMessageCallback(MessageCallback callback) override;
    
    // 배치 처리
    bool mset(const StringMap& key_values) override;
    StringList mget(const StringList& keys) override;
    
    // 트랜잭션 지원
    bool multi() override;
    bool exec() override;
    bool discard() override;
    
    // 상태 및 진단
    StringMap info() override;
    bool ping() override;
    bool select(int db_index) override;
    int dbsize() override;
    
    // =============================================================================
    // hiredis 특화 고급 기능들
    // =============================================================================
    
    /**
     * @brief 파이프라인 모드 시작
     * @return 성공 시 true
     * @details 여러 명령어를 배치로 전송하여 네트워크 라운트트립 최소화
     */
    bool startPipeline();
    
    /**
     * @brief 파이프라인에 명령어 추가
     * @param command Redis 명령어
     * @return 성공 시 true
     */
    bool addToPipeline(const std::string& command);
    
    /**
     * @brief 파이프라인 실행
     * @return 모든 응답의 벡터
     */
    std::vector<std::unique_ptr<redisReply, void(*)(redisReply*)>> executePipeline();
    
    /**
     * @brief 스크립트 실행 (Lua)
     * @param script Lua 스크립트
     * @param keys 키 목록
     * @param args 인수 목록
     * @return 스크립트 실행 결과
     */
    std::string evalScript(const std::string& script, 
                          const StringList& keys = StringList{},
                          const StringList& args = StringList{});
    
    /**
     * @brief 스크립트 SHA1 해시로 실행
     * @param sha1 스크립트 SHA1 해시
     * @param keys 키 목록
     * @param args 인수 목록
     * @return 스크립트 실행 결과
     */
    std::string evalSha(const std::string& sha1,
                       const StringList& keys = StringList{},
                       const StringList& args = StringList{});
    
    /**
     * @brief 스크립트 로드
     * @param script Lua 스크립트
     * @return SHA1 해시
     */
    std::string scriptLoad(const std::string& script);
    
    /**
     * @brief 키 스캔 (커서 기반)
     * @param cursor 스캔 커서 (0부터 시작)
     * @param pattern 매칭 패턴 (선택사항)
     * @param count 한 번에 가져올 키 수 (선택사항)
     * @return {새로운 커서, 키 목록} 쌍
     */
    std::pair<std::string, StringList> scan(const std::string& cursor = "0",
                                           const std::string& pattern = "",
                                           int count = 10);
    
    /**
     * @brief Hash 필드 스캔
     * @param key Hash 키
     * @param cursor 스캔 커서
     * @param pattern 매칭 패턴 (선택사항)
     * @param count 한 번에 가져올 필드 수 (선택사항)
     * @return {새로운 커서, 필드-값 맵} 쌍
     */
    std::pair<std::string, StringMap> hscan(const std::string& key,
                                           const std::string& cursor = "0",
                                           const std::string& pattern = "",
                                           int count = 10);
    
    /**
     * @brief Sorted Set 스캔
     * @param key Sorted Set 키
     * @param cursor 스캔 커서
     * @param pattern 매칭 패턴 (선택사항)
     * @param count 한 번에 가져올 멤버 수 (선택사항)
     * @return {새로운 커서, 멤버-점수 맵} 쌍
     */
    std::pair<std::string, std::map<std::string, double>> zscan(const std::string& key,
                                                               const std::string& cursor = "0",
                                                               const std::string& pattern = "",
                                                               int count = 10);
    
    // =============================================================================
    // 성능 및 모니터링
    // =============================================================================
    
    /**
     * @brief 연결 통계 조회
     */
    struct ConnectionStats {
        uint64_t total_commands;
        uint64_t successful_commands;
        uint64_t failed_commands;
        double success_rate;
        std::chrono::milliseconds uptime;
        std::chrono::milliseconds last_command_latency;
        int reconnect_attempts;
        bool is_connected;
        bool pubsub_active;
        size_t subscribed_channels_count;
        size_t subscribed_patterns_count;
    };
    
    ConnectionStats getConnectionStats() const;
    
    /**
     * @brief 통계 리셋
     */
    void resetStats();
    
    /**
     * @brief 연결 풀 활성화/비활성화
     * @param enable 풀 사용 여부
     */
    void enableConnectionPool(bool enable) { pool_enabled_ = enable; }
    
    /**
     * @brief 자동 재연결 설정
     * @param enable 자동 재연결 사용 여부
     */
    void setAutoReconnect(bool enable) { auto_reconnect_ = enable; }
    
    /**
     * @brief 타임아웃 설정
     * @param connect_timeout_ms 연결 타임아웃 (밀리초)
     * @param command_timeout_ms 명령어 타임아웃 (밀리초)
     */
    void setTimeouts(int connect_timeout_ms, int command_timeout_ms);

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
    
    // 명령어 실행
    std::unique_ptr<redisReply, void(*)(redisReply*)> executeCommand(const char* format, ...);
    std::unique_ptr<redisReply, void(*)(redisReply*)> executeCommandArgv(int argc, const char** argv, const size_t* argvlen);
    bool executeCommandNoReply(const char* format, ...);
    
    // Pub/Sub 관리
    void startPubSubThread();
    void stopPubSubThread();
    void pubsubThreadWorker();
    void handlePubSubMessage(redisReply* reply);
    
    // 비동기 작업 관리
    void startAsyncThread();
    void stopAsyncThread();
    void asyncThreadWorker();
    
    // 에러 처리
    void logError(const std::string& operation, const std::string& error_message);
    void logRedisError(const std::string& operation, redisContext* ctx);
    bool handleConnectionError();
    bool isConnectionError(redisContext* ctx) const;
    
    // 유틸리티
    std::string replyToString(redisReply* reply);
    long long replyToInteger(redisReply* reply);
    StringList replyToStringList(redisReply* reply);
    StringMap replyToStringMap(redisReply* reply);
    bool isReplyOK(redisReply* reply);
    bool isReplyError(redisReply* reply);
    
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
};

#endif // REDIS_CLIENT_IMPL_H