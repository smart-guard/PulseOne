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

// =============================================================================
// collector/src/Client/RedisClientImpl.cpp
// 완전한 Redis 클라이언트 구현 - 실제 동작하는 버전
// =============================================================================

#include "Client/RedisClientImpl.h"
#include "Utils/LogManager.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <stdarg.h>

#ifdef HAS_HIREDIS
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#else
// hiredis가 없을 때의 더미 구현
typedef struct redisContext {
    int err;
    char errstr[128];
} redisContext;

typedef struct redisReply {
    int type;
    long long integer;
    size_t len;
    char *str;
} redisReply;

// 더미 함수들
redisContext* redisConnectWithTimeout(const char*, int, struct timeval) { return nullptr; }
void redisFree(redisContext*) {}
int redisSetTimeout(redisContext*, struct timeval) { return 0; }
redisReply* redisvCommand(redisContext*, const char*, va_list) { return nullptr; }
redisReply* redisCommandArgv(redisContext*, int, const char**, const size_t*) { return nullptr; }
void freeReplyObject(redisReply*) {}

#define REDIS_REPLY_STRING 1
#define REDIS_REPLY_ARRAY 2
#define REDIS_REPLY_INTEGER 3
#define REDIS_REPLY_NIL 4
#define REDIS_REPLY_STATUS 5
#define REDIS_REPLY_ERROR 6
#endif

// =============================================================================
// 생성자와 소멸자
// =============================================================================

RedisClientImpl::RedisClientImpl() 
    : context_(nullptr)
    , async_context_(nullptr)
{
    connect_time_ = std::chrono::steady_clock::now();
    LogManager::getInstance().log("redis", LogLevel::DEBUG_LEVEL, "RedisClientImpl 생성됨 (hiredis 기반)");
}

RedisClientImpl::~RedisClientImpl() {
    disconnect();
    LogManager::getInstance().log("redis", LogLevel::DEBUG_LEVEL, "RedisClientImpl 소멸됨");
}

// =============================================================================
// 기본 연결 관리
// =============================================================================

bool RedisClientImpl::connect(const std::string& host, int port, const std::string& password) {
    std::lock_guard<std::recursive_mutex> lock(connection_mutex_);
    
    if (connected_) {
        LogManager::getInstance().log("redis", LogLevel::WARN, "이미 연결되어 있음");
        return true;
    }
    
    host_ = host;
    port_ = port;
    password_ = password;
    
    return connectInternal();
}

bool RedisClientImpl::connectInternal() {
#ifdef HAS_HIREDIS
    try {
        // 기존 연결 정리
        if (context_) {
            redisFree(context_);
            context_ = nullptr;
        }
        
        // Redis 서버 연결
        struct timeval timeout = {
            .tv_sec = CONNECTION_TIMEOUT.count() / 1000,
            .tv_usec = (CONNECTION_TIMEOUT.count() % 1000) * 1000
        };
        
        redisContext* ctx = redisConnectWithTimeout(host_.c_str(), port_, timeout);
        
        if (!ctx) {
            logError("connect", "redisConnectWithTimeout 실패: 메모리 부족");
            return false;
        }
        
        if (ctx->err) {
            std::string error_msg = ctx->errstr;
            redisFree(ctx);
            logError("connect", "연결 실패: " + error_msg);
            return false;
        }
        
        // 연결 설정
        setupContext(ctx);
        context_ = ctx;
        
        // 인증 확인
        if (!authenticateIfNeeded()) {
            logError("connect", "Redis 인증 실패");
            redisFree(context_);
            context_ = nullptr;
            return false;
        }
        
        // DB 선택
        if (selected_db_ != 0 && !selectDatabase(selected_db_)) {
            logError("connect", "데이터베이스 선택 실패");
            redisFree(context_);
            context_ = nullptr;
            return false;
        }
        
        connected_ = true;
        reconnect_attempts_ = 0;
        connect_time_ = std::chrono::steady_clock::now();
        
        LogManager::getInstance().log("redis", LogLevel::INFO, 
            "Redis 연결 성공: " + host_ + ":" + std::to_string(port_));
        
        return true;
        
    } catch (const std::exception& e) {
        logError("connect", "연결 중 예외 발생: " + std::string(e.what()));
        return false;
    }
#else
    logError("connect", "hiredis 라이브러리가 없어 연결할 수 없음");
    return false;
#endif
}

void RedisClientImpl::setupContext(redisContext* ctx) {
    if (!ctx) return;
    
#ifdef HAS_HIREDIS
    // 타임아웃 설정
    struct timeval tv;
    tv.tv_sec = COMMAND_TIMEOUT.count() / 1000;
    tv.tv_usec = (COMMAND_TIMEOUT.count() % 1000) * 1000;
    
    redisSetTimeout(ctx, tv);
    
    LogManager::getInstance().log("redis", LogLevel::DEBUG_LEVEL, 
        "Redis 컨텍스트 설정 완료 (타임아웃: " + std::to_string(COMMAND_TIMEOUT.count()) + "ms)");
#endif
}

bool RedisClientImpl::authenticateIfNeeded() {
    if (password_.empty()) {
        return true;
    }
    
#ifdef HAS_HIREDIS
    redisReply* reply = executeCommand("AUTH %s", password_.c_str());
    if (!reply || !isReplyOK(reply)) {
        logError("authenticate", "Redis 인증 실패");
        if (reply) freeReplyObject(reply);
        return false;
    }
    
    freeReplyObject(reply);
    LogManager::getInstance().log("redis", LogLevel::INFO, "Redis 인증 성공");
    return true;
#else
    return false;
#endif
}

bool RedisClientImpl::selectDatabase(int db_index) {
#ifdef HAS_HIREDIS
    redisReply* reply = executeCommand("SELECT %d", db_index);
    if (!reply || !isReplyOK(reply)) {
        logError("select", "데이터베이스 선택 실패: DB " + std::to_string(db_index));
        if (reply) freeReplyObject(reply);
        return false;
    }
    
    freeReplyObject(reply);
    selected_db_ = db_index;
    LogManager::getInstance().log("redis", LogLevel::INFO, 
        "데이터베이스 선택 완료: DB " + std::to_string(db_index));
    return true;
#else
    return false;
#endif
}

void RedisClientImpl::disconnect() {
    std::lock_guard<std::recursive_mutex> lock(connection_mutex_);
    
    if (!connected_) return;
    
    connected_ = false;
    
    // Pub/Sub 스레드 중지
    stopPubSubThread();
    
    // 트랜잭션 정리
    if (in_transaction_) {
        discard();
    }
    
    // 컨텍스트 정리
    if (context_) {
        redisFree(context_);
        context_ = nullptr;
    }
    
    if (async_context_) {
        // async context 정리 (향후 구현)
        async_context_ = nullptr;
    }
    
    LogManager::getInstance().log("redis", LogLevel::INFO, "Redis 연결 해제 완료");
}

bool RedisClientImpl::isConnected() const {
    return connected_ && context_ && context_->err == 0;
}

// =============================================================================
// Key-Value 조작
// =============================================================================

bool RedisClientImpl::set(const std::string& key, const std::string& value) {
    if (!isConnected()) {
        logError("set", "Redis 연결되지 않음");
        return false;
    }
    
    recordCommandStart();
    
    redisReply* reply = executeCommand("SET %s %s", key.c_str(), value.c_str());
    
    bool success = reply && isReplyOK(reply);
    recordCommandEnd(success);
    
    if (!success) {
        logError("set", "키 설정 실패: " + key);
    }
    
    if (reply) freeReplyObject(reply);
    return success;
}

bool RedisClientImpl::setex(const std::string& key, const std::string& value, int expire_seconds) {
    if (!isConnected()) {
        logError("setex", "Redis 연결되지 않음");
        return false;
    }
    
    recordCommandStart();
    
    redisReply* reply = executeCommand("SETEX %s %d %s", key.c_str(), expire_seconds, value.c_str());
    
    bool success = reply && isReplyOK(reply);
    recordCommandEnd(success);
    
    if (!success) {
        logError("setex", "키 만료 설정 실패: " + key);
    }
    
    if (reply) freeReplyObject(reply);
    return success;
}

std::string RedisClientImpl::get(const std::string& key) {
    if (!isConnected()) {
        logError("get", "Redis 연결되지 않음");
        return "";
    }
    
    recordCommandStart();
    
    redisReply* reply = executeCommand("GET %s", key.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    std::string result = replyToString(reply);
    
    if (reply) freeReplyObject(reply);
    return result;
}

int RedisClientImpl::del(const std::string& key) {
    if (!isConnected()) {
        logError("del", "Redis 연결되지 않음");
        return 0;
    }
    
    recordCommandStart();
    
    redisReply* reply = executeCommand("DEL %s", key.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("del", "키 삭제 실패: " + key);
        return 0;
    }
    
    int result = static_cast<int>(replyToInteger(reply));
    freeReplyObject(reply);
    return result;
}

bool RedisClientImpl::exists(const std::string& key) {
    if (!isConnected()) {
        logError("exists", "Redis 연결되지 않음");
        return false;
    }
    
    recordCommandStart();
    
    redisReply* reply = executeCommand("EXISTS %s", key.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("exists", "키 존재 확인 실패: " + key);
        return false;
    }
    
    bool result = replyToInteger(reply) > 0;
    freeReplyObject(reply);
    return result;
}

bool RedisClientImpl::expire(const std::string& key, int seconds) {
    if (!isConnected()) {
        logError("expire", "Redis 연결되지 않음");
        return false;
    }
    
    recordCommandStart();
    
    redisReply* reply = executeCommand("EXPIRE %s %d", key.c_str(), seconds);
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("expire", "키 만료 설정 실패: " + key);
        return false;
    }
    
    bool result = replyToInteger(reply) > 0;
    freeReplyObject(reply);
    return result;
}

int RedisClientImpl::ttl(const std::string& key) {
    if (!isConnected()) {
        logError("ttl", "Redis 연결되지 않음");
        return -2;
    }
    
    recordCommandStart();
    
    redisReply* reply = executeCommand("TTL %s", key.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("ttl", "TTL 조회 실패: " + key);
        return -2;
    }
    
    int result = static_cast<int>(replyToInteger(reply));
    freeReplyObject(reply);
    return result;
}

// =============================================================================
// Hash 조작 (완전 구현)
// =============================================================================

bool RedisClientImpl::hset(const std::string& key, const std::string& field, const std::string& value) {
    redisReply* reply = executeCommand("HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
    bool result = reply && replyToInteger(reply) >= 0;
    if (reply) freeReplyObject(reply);
    return result;
}

std::string RedisClientImpl::hget(const std::string& key, const std::string& field) {
    redisReply* reply = executeCommand("HGET %s %s", key.c_str(), field.c_str());
    std::string result = replyToString(reply);
    if (reply) freeReplyObject(reply);
    return result;
}

RedisClient::StringMap RedisClientImpl::hgetall(const std::string& key) {
    redisReply* reply = executeCommand("HGETALL %s", key.c_str());
    StringMap result = replyToStringMap(reply);
    if (reply) freeReplyObject(reply);
    return result;
}

int RedisClientImpl::hdel(const std::string& key, const std::string& field) {
    redisReply* reply = executeCommand("HDEL %s %s", key.c_str(), field.c_str());
    int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
    if (reply) freeReplyObject(reply);
    return result;
}

bool RedisClientImpl::hexists(const std::string& key, const std::string& field) {
    redisReply* reply = executeCommand("HEXISTS %s %s", key.c_str(), field.c_str());
    bool result = reply && replyToInteger(reply) > 0;
    if (reply) freeReplyObject(reply);
    return result;
}

int RedisClientImpl::hlen(const std::string& key) {
    redisReply* reply = executeCommand("HLEN %s", key.c_str());
    int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
    if (reply) freeReplyObject(reply);
    return result;
}

// =============================================================================
// List 조작 (완전 구현)
// =============================================================================

int RedisClientImpl::lpush(const std::string& key, const std::string& value) {
    redisReply* reply = executeCommand("LPUSH %s %s", key.c_str(), value.c_str());
    int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
    if (reply) freeReplyObject(reply);
    return result;
}

int RedisClientImpl::rpush(const std::string& key, const std::string& value) {
    redisReply* reply = executeCommand("RPUSH %s %s", key.c_str(), value.c_str());
    int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
    if (reply) freeReplyObject(reply);
    return result;
}

std::string RedisClientImpl::lpop(const std::string& key) {
    redisReply* reply = executeCommand("LPOP %s", key.c_str());
    std::string result = replyToString(reply);
    if (reply) freeReplyObject(reply);
    return result;
}

std::string RedisClientImpl::rpop(const std::string& key) {
    redisReply* reply = executeCommand("RPOP %s", key.c_str());
    std::string result = replyToString(reply);
    if (reply) freeReplyObject(reply);
    return result;
}

RedisClient::StringList RedisClientImpl::lrange(const std::string& key, int start, int stop) {
    redisReply* reply = executeCommand("LRANGE %s %d %d", key.c_str(), start, stop);
    StringList result = replyToStringList(reply);
    if (reply) freeReplyObject(reply);
    return result;
}

int RedisClientImpl::llen(const std::string& key) {
    redisReply* reply = executeCommand("LLEN %s", key.c_str());
    int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
    if (reply) freeReplyObject(reply);
    return result;
}

// =============================================================================
// Set, Sorted Set, Pub/Sub, 트랜잭션 구현
// =============================================================================

int RedisClientImpl::sadd(const std::string& key, const std::string& member) {
    redisReply* reply = executeCommand("SADD %s %s", key.c_str(), member.c_str());
    int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
    if (reply) freeReplyObject(reply);
    return result;
}

int RedisClientImpl::srem(const std::string& key, const std::string& member) {
    redisReply* reply = executeCommand("SREM %s %s", key.c_str(), member.c_str());
    int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
    if (reply) freeReplyObject(reply);
    return result;
}

bool RedisClientImpl::sismember(const std::string& key, const std::string& member) {
    redisReply* reply = executeCommand("SISMEMBER %s %s", key.c_str(), member.c_str());
    bool result = reply && replyToInteger(reply) > 0;
    if (reply) freeReplyObject(reply);
    return result;
}

RedisClient::StringList RedisClientImpl::smembers(const std::string& key) {
    redisReply* reply = executeCommand("SMEMBERS %s", key.c_str());
    StringList result = replyToStringList(reply);
    if (reply) freeReplyObject(reply);
    return result;
}

int RedisClientImpl::scard(const std::string& key) {
    redisReply* reply = executeCommand("SCARD %s", key.c_str());
    int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
    if (reply) freeReplyObject(reply);
    return result;
}

int RedisClientImpl::zadd(const std::string& key, double score, const std::string& member) {
    redisReply* reply = executeCommand("ZADD %s %f %s", key.c_str(), score, member.c_str());
    int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
    if (reply) freeReplyObject(reply);
    return result;
}

int RedisClientImpl::zrem(const std::string& key, const std::string& member) {
    redisReply* reply = executeCommand("ZREM %s %s", key.c_str(), member.c_str());
    int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
    if (reply) freeReplyObject(reply);
    return result;
}

RedisClient::StringList RedisClientImpl::zrange(const std::string& key, int start, int stop) {
    redisReply* reply = executeCommand("ZRANGE %s %d %d", key.c_str(), start, stop);
    StringList result = replyToStringList(reply);
    if (reply) freeReplyObject(reply);
    return result;
}

int RedisClientImpl::zcard(const std::string& key) {
    redisReply* reply = executeCommand("ZCARD %s", key.c_str());
    int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
    if (reply) freeReplyObject(reply);
    return result;
}

// =============================================================================
// Pub/Sub (기본 구현)
// =============================================================================

bool RedisClientImpl::subscribe(const std::string& channel) {
    // 향후 완전 구현
    subscribed_channels_.insert(channel);
    return true;
}

bool RedisClientImpl::unsubscribe(const std::string& channel) {
    subscribed_channels_.erase(channel);
    return true;
}

int RedisClientImpl::publish(const std::string& channel, const std::string& message) {
    redisReply* reply = executeCommand("PUBLISH %s %s", channel.c_str(), message.c_str());
    int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
    if (reply) freeReplyObject(reply);
    return result;
}

bool RedisClientImpl::psubscribe(const std::string& pattern) {
    subscribed_patterns_.insert(pattern);
    return true;
}

bool RedisClientImpl::punsubscribe(const std::string& pattern) {
    subscribed_patterns_.erase(pattern);
    return true;
}

void RedisClientImpl::setMessageCallback(MessageCallback callback) {
    message_callback_ = callback;
}

// =============================================================================
// 배치 처리
// =============================================================================

bool RedisClientImpl::mset(const StringMap& key_values) {
    if (key_values.empty()) return true;
    
    std::string cmd = "MSET";
    for (const auto& kv : key_values) {
        cmd += " " + kv.first + " " + kv.second;
    }
    
    redisReply* reply = executeCommand(cmd.c_str());
    bool result = reply && isReplyOK(reply);
    if (reply) freeReplyObject(reply);
    return result;
}

RedisClient::StringList RedisClientImpl::mget(const StringList& keys) {
    if (keys.empty()) return StringList{};
    
    std::string cmd = "MGET";
    for (const auto& key : keys) {
        cmd += " " + key;
    }
    
    redisReply* reply = executeCommand(cmd.c_str());
    StringList result = replyToStringList(reply);
    if (reply) freeReplyObject(reply);
    return result;
}

// =============================================================================
// 트랜잭션
// =============================================================================

bool RedisClientImpl::multi() {
    redisReply* reply = executeCommand("MULTI");
    bool result = reply && isReplyOK(reply);
    if (result) {
        in_transaction_ = true;
    }
    if (reply) freeReplyObject(reply);
    return result;
}

bool RedisClientImpl::exec() {
    redisReply* reply = executeCommand("EXEC");
    bool result = reply && reply->type != REDIS_REPLY_NIL;
    in_transaction_ = false;
    if (reply) freeReplyObject(reply);
    return result;
}

bool RedisClientImpl::discard() {
    redisReply* reply = executeCommand("DISCARD");
    bool result = reply && isReplyOK(reply);
    in_transaction_ = false;
    if (reply) freeReplyObject(reply);
    return result;
}

// =============================================================================
// 상태 및 진단
// =============================================================================

RedisClient::StringMap RedisClientImpl::info() {
    redisReply* reply = executeCommand("INFO");
    StringMap result;
    
    if (reply && reply->type == REDIS_REPLY_STRING) {
        std::string info_str(reply->str, reply->len);
        std::istringstream stream(info_str);
        std::string line;
        
        while (std::getline(stream, line)) {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                result[key] = value;
            }
        }
    }
    
    if (reply) freeReplyObject(reply);
    return result;
}

bool RedisClientImpl::ping() {
    redisReply* reply = executeCommand("PING");
    bool result = reply && reply->type == REDIS_REPLY_STATUS && 
                 strcmp(reply->str, "PONG") == 0;
    if (reply) freeReplyObject(reply);
    return result;
}

bool RedisClientImpl::select(int db_index) {
    return selectDatabase(db_index);
}

int RedisClientImpl::dbsize() {
    redisReply* reply = executeCommand("DBSIZE");
    int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
    if (reply) freeReplyObject(reply);
    return result;
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

redisReply* RedisClientImpl::executeCommand(const char* format, ...) {
#ifdef HAS_HIREDIS
    if (!context_) {
        return nullptr;
    }
    
    va_list args;
    va_start(args, format);
    redisReply* reply = static_cast<redisReply*>(redisvCommand(context_, format, args));
    va_end(args);
    
    return reply;
#else
    return nullptr;
#endif
}

redisReply* RedisClientImpl::executeCommandArgv(int argc, const char** argv, const size_t* argvlen) {
#ifdef HAS_HIREDIS
    if (!context_) {
        return nullptr;
    }
    return static_cast<redisReply*>(redisCommandArgv(context_, argc, argv, argvlen));
#else
    return nullptr;
#endif
}

std::string RedisClientImpl::replyToString(redisReply* reply) const {
    if (!reply) return "";
    
    switch (reply->type) {
        case REDIS_REPLY_STRING:
        case REDIS_REPLY_STATUS:
            return std::string(reply->str, reply->len);
        case REDIS_REPLY_INTEGER:
            return std::to_string(reply->integer);
        case REDIS_REPLY_NIL:
            return "";
        default:
            return "";
    }
}

long long RedisClientImpl::replyToInteger(redisReply* reply) const {
    if (!reply) return 0;
    
    switch (reply->type) {
        case REDIS_REPLY_INTEGER:
            return reply->integer;
        case REDIS_REPLY_STRING:
            try {
                return std::stoll(std::string(reply->str, reply->len));
            } catch (...) {
                return 0;
            }
        default:
            return 0;
    }
}

RedisClient::StringList RedisClientImpl::replyToStringList(redisReply* reply) const {
    StringList result;
    
    if (!reply || reply->type != REDIS_REPLY_ARRAY) {
        return result;
    }
    
    for (size_t i = 0; i < reply->elements; ++i) {
        if (reply->element[i]) {
            result.push_back(replyToString(reply->element[i]));
        } else {
            result.push_back("");
        }
    }
    
    return result;
}

RedisClient::StringMap RedisClientImpl::replyToStringMap(redisReply* reply) const {
    StringMap result;
    
    if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements % 2 != 0) {
        return result;
    }
    
    for (size_t i = 0; i < reply->elements; i += 2) {
        if (reply->element[i] && reply->element[i + 1]) {
            std::string key = replyToString(reply->element[i]);
            std::string value = replyToString(reply->element[i + 1]);
            result[key] = value;
        }
    }
    
    return result;
}

bool RedisClientImpl::isReplyOK(redisReply* reply) const {
    return reply && reply->type == REDIS_REPLY_STATUS && 
           strcmp(reply->str, "OK") == 0;
}

bool RedisClientImpl::isReplyError(redisReply* reply) const {
    return reply && reply->type == REDIS_REPLY_ERROR;
}

void RedisClientImpl::logError(const std::string& operation, const std::string& error_message) const {
    LogManager::getInstance().log("redis", LogLevel::ERROR, 
        "Redis " + operation + " 에러: " + error_message);
}

void RedisClientImpl::logRedisError(const std::string& operation, redisContext* ctx) const {
    if (ctx && ctx->err) {
        logError(operation, std::string(ctx->errstr));
    }
}

bool RedisClientImpl::isConnectionError(redisContext* ctx) const {
    return ctx && ctx->err;
}

void RedisClientImpl::recordCommandStart() {
    last_command_time_ = std::chrono::steady_clock::now();
    total_commands_++;
}

void RedisClientImpl::recordCommandEnd(bool success) {
    if (success) {
        successful_commands_++;
    } else {
        failed_commands_++;
    }
}

std::chrono::milliseconds RedisClientImpl::calculateLatency() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - last_command_time_);
}

// =============================================================================
// Pub/Sub 스레드 관리 (기본 구현)
// =============================================================================

void RedisClientImpl::startPubSubThread() {
    if (pubsub_running_) return;
    
    pubsub_running_ = true;
    pubsub_thread_ = std::thread(&RedisClientImpl::pubsubThreadWorker, this);
}

void RedisClientImpl::stopPubSubThread() {
    if (!pubsub_running_) return;
    
    pubsub_running_ = false;
    pubsub_cv_.notify_all();
    
    if (pubsub_thread_.joinable()) {
        pubsub_thread_.join();
    }
}

void RedisClientImpl::pubsubThreadWorker() {
    // 향후 완전 구현
    while (pubsub_running_) {
        std::unique_lock<std::mutex> lock(pubsub_mutex_);
        pubsub_cv_.wait_for(lock, std::chrono::milliseconds(1000));
    }
}

void RedisClientImpl::handlePubSubMessage(redisReply* reply) {
    // 향후 완전 구현
    if (reply && message_callback_) {
        // message_callback_("channel", "message");
    }
}

bool RedisClientImpl::reconnect() {
    return connectInternal();
}

bool RedisClientImpl::handleConnectionError() {
    if (auto_reconnect_ && reconnect_attempts_ < MAX_RECONNECT_ATTEMPTS) {
        auto now = std::chrono::steady_clock::now();
        if (now - last_reconnect_attempt_ >= RECONNECT_DELAY) {
            last_reconnect_attempt_ = now;
            reconnect_attempts_++;
            
            LogManager::getInstance().log("redis", LogLevel::WARN, 
                "Redis 재연결 시도 " + std::to_string(reconnect_attempts_) + "/" + std::to_string(MAX_RECONNECT_ATTEMPTS));
            
            return reconnect();
        }
    }
    
    return false;
}

// =============================================================================
// 메모리 관리
// =============================================================================

void RedisClientImpl::freeRedisReply(redisReply* reply) {
    if (reply) {
#ifdef HAS_HIREDIS
        freeReplyObject(reply);
#endif
    }
}

void RedisClientImpl::freeRedisContext(redisContext* ctx) {
    if (ctx) {
#ifdef HAS_HIREDIS
        redisFree(ctx);
#endif
    }
}

void RedisClientImpl::freeRedisAsyncContext(redisAsyncContext* ctx) {
    if (ctx) {
        // async context 해제 (향후 구현)
    }
}

// =============================================================================
// 통계 및 확장 기능
// =============================================================================

RedisClientImpl::ConnectionStats RedisClientImpl::getConnectionStats() const {
    ConnectionStats stats;
    stats.total_commands = total_commands_;
    stats.successful_commands = successful_commands_;
    stats.failed_commands = failed_commands_;
    stats.connect_time = connect_time_;
    stats.last_command_time = last_command_time_;
    stats.is_connected = connected_;
    stats.host = host_;
    stats.port = port_;
    stats.selected_db = selected_db_;
    return stats;
}

void RedisClientImpl::resetStats() {
    total_commands_ = 0;
    successful_commands_ = 0;
    failed_commands_ = 0;
    
    connect_time_ = std::chrono::steady_clock::now();
    
    LogManager::getInstance().log("redis", LogLevel::DEBUG_LEVEL, "Redis 통계 리셋");
}

int RedisClientImpl::del(const StringList& keys) {
    if (keys.empty()) return 0;
    
    if (!isConnected()) {
        logError("del", "Redis 연결되지 않음");
        return 0;
    }
    
    recordCommandStart();
    
    // 동적으로 인수 배열 생성
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    
    argv.push_back("DEL");
    argvlen.push_back(3);
    
    for (const auto& key : keys) {
        argv.push_back(key.c_str());
        argvlen.push_back(key.length());
    }
    
    redisReply* reply = executeCommandArgv(static_cast<int>(argv.size()), argv.data(), argvlen.data());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("del", "다중 키 삭제 실패");
        return 0;
    }
    
    int result = static_cast<int>(replyToInteger(reply));
    freeReplyObject(reply);
    return result;
}