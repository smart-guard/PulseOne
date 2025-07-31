// collector/src/Client/RedisClientImpl.cpp
#include "Client/RedisClientImpl.h"
#include "Utils/LogManager.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <stdarg.h>

// =============================================================================
// 생성자와 소멸자
// =============================================================================

RedisClientImpl::RedisClientImpl() 
    : context_(nullptr, &RedisClientImpl::freeRedisContext)
    , async_context_(nullptr, &RedisClientImpl::freeRedisAsyncContext)
{
    connect_time_ = std::chrono::steady_clock::now();
    LogManager::getInstance().log("redis", LogLevel::DEBUG, "RedisClientImpl 생성됨 (hiredis 기반)");
}

RedisClientImpl::~RedisClientImpl() {
    disconnect();
    LogManager::getInstance().log("redis", LogLevel::DEBUG, "RedisClientImpl 소멸됨");
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
    try {
        // 기존 연결 정리
        context_.reset();
        
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
        
        // 컨텍스트 설정
        context_.reset(ctx);
        setupContext(ctx);
        
        // 인증
        if (!authenticateIfNeeded()) {
            context_.reset();
            return false;
        }
        
        // 데이터베이스 선택
        if (selected_db_ != 0 && !selectDatabase(selected_db_)) {
            context_.reset();
            return false;
        }
        
        // 연결 성공
        connected_ = true;
        reconnect_attempts_ = 0;
        connect_time_ = std::chrono::steady_clock::now();
        
        // 비동기 스레드 시작
        startAsyncThread();
        
        LogManager::getInstance().log("redis", LogLevel::INFO,
            "✅ Redis 연결 성공: " + host_ + ":" + std::to_string(port_));
        
        return true;
        
    } catch (const std::exception& e) {
        logError("connect", e.what());
        connected_ = false;
        return false;
    }
}

void RedisClientImpl::setupContext(redisContext* ctx) {
    // 명령어 타임아웃 설정
    struct timeval timeout = {
        .tv_sec = COMMAND_TIMEOUT.count() / 1000,
        .tv_usec = (COMMAND_TIMEOUT.count() % 1000) * 1000
    };
    
    redisSetTimeout(ctx, timeout);
    
    // TCP keepalive 설정 (가능한 경우)
    redisEnableKeepAlive(ctx);
}

bool RedisClientImpl::authenticateIfNeeded() {
    if (password_.empty()) {
        return true;
    }
    
    auto reply = executeCommand("AUTH %s", password_.c_str());
    if (!reply || !isReplyOK(reply.get())) {
        logError("auth", "인증 실패");
        return false;
    }
    
    LogManager::getInstance().log("redis", LogLevel::DEBUG, "Redis 인증 성공");
    return true;
}

bool RedisClientImpl::selectDatabase(int db_index) {
    auto reply = executeCommand("SELECT %d", db_index);
    if (!reply || !isReplyOK(reply.get())) {
        logError("select", "데이터베이스 선택 실패: " + std::to_string(db_index));
        return false;
    }
    
    selected_db_ = db_index;
    LogManager::getInstance().log("redis", LogLevel::DEBUG, 
        "데이터베이스 선택: " + std::to_string(db_index));
    return true;
}

void RedisClientImpl::disconnect() {
    std::lock_guard<std::recursive_mutex> lock(connection_mutex_);
    
    if (!connected_) return;
    
    // Pub/Sub 중지
    stopPubSubThread();
    
    // 비동기 작업 중지
    stopAsyncThread();
    
    // 트랜잭션/파이프라인 정리
    if (in_transaction_) {
        discard();
    }
    
    pipeline_commands_.clear();
    pipeline_mode_ = false;
    
    // 연결 해제
    context_.reset();
    async_context_.reset();
    connected_ = false;
    
    LogManager::getInstance().log("redis", LogLevel::INFO,
        "Redis 연결 해제: " + host_ + ":" + std::to_string(port_));
}

bool RedisClientImpl::isConnected() const {
    std::lock_guard<std::recursive_mutex> lock(connection_mutex_);
    return connected_ && context_ && !context_->err;
}

// =============================================================================
// 기본 Key-Value 조작
// =============================================================================

bool RedisClientImpl::set(const std::string& key, const std::string& value) {
    recordCommandStart();
    
    auto reply = executeCommand("SET %s %s", key.c_str(), value.c_str());
    bool success = reply && isReplyOK(reply.get());
    
    recordCommandEnd(success);
    
    if (!success) {
        logError("set", "키 설정 실패: " + key);
    }
    
    return success;
}

std::string RedisClientImpl::get(const std::string& key) {
    recordCommandStart();
    
    auto reply = executeCommand("GET %s", key.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("get", "키 조회 실패: " + key);
        return "";
    }
    
    return replyToString(reply.get());
}

bool RedisClientImpl::setex(const std::string& key, const std::string& value, int ttl_seconds) {
    recordCommandStart();
    
    auto reply = executeCommand("SETEX %s %d %s", key.c_str(), ttl_seconds, value.c_str());
    bool success = reply && isReplyOK(reply.get());
    
    recordCommandEnd(success);
    
    if (!success) {
        logError("setex", "TTL 키 설정 실패: " + key);
    }
    
    return success;
}

bool RedisClientImpl::exists(const std::string& key) {
    recordCommandStart();
    
    auto reply = executeCommand("EXISTS %s", key.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("exists", "키 존재 확인 실패: " + key);
        return false;
    }
    
    return replyToInteger(reply.get()) > 0;
}

int RedisClientImpl::del(const std::string& key) {
    recordCommandStart();
    
    auto reply = executeCommand("DEL %s", key.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("del", "키 삭제 실패: " + key);
        return 0;
    }
    
    return static_cast<int>(replyToInteger(reply.get()));
}

int RedisClientImpl::del(const StringList& keys) {
    if (keys.empty()) return 0;
    
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
    
    auto reply = executeCommandArgv(static_cast<int>(argv.size()), argv.data(), argvlen.data());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("del", "다중 키 삭제 실패");
        return 0;
    }
    
    return static_cast<int>(replyToInteger(reply.get()));
}

int RedisClientImpl::ttl(const std::string& key) {
    recordCommandStart();
    
    auto reply = executeCommand("TTL %s", key.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("ttl", "TTL 조회 실패: " + key);
        return -2;
    }
    
    return static_cast<int>(replyToInteger(reply.get()));
}

// =============================================================================
// Hash 조작
// =============================================================================

bool RedisClientImpl::hset(const std::string& key, const std::string& field, const std::string& value) {
    recordCommandStart();
    
    auto reply = executeCommand("HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
    bool success = reply != nullptr;
    
    recordCommandEnd(success);
    
    if (!success) {
        logError("hset", "Hash 필드 설정 실패: " + key + "." + field);
    }
    
    return success;
}

std::string RedisClientImpl::hget(const std::string& key, const std::string& field) {
    recordCommandStart();
    
    auto reply = executeCommand("HGET %s %s", key.c_str(), field.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("hget", "Hash 필드 조회 실패: " + key + "." + field);
        return "";
    }
    
    return replyToString(reply.get());
}

RedisClient::StringMap RedisClientImpl::hgetall(const std::string& key) {
    recordCommandStart();
    
    auto reply = executeCommand("HGETALL %s", key.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("hgetall", "Hash 전체 조회 실패: " + key);
        return StringMap{};
    }
    
    return replyToStringMap(reply.get());
}

bool RedisClientImpl::hmset(const std::string& key, const StringMap& field_values) {
    if (field_values.empty()) return true;
    
    recordCommandStart();
    
    // 동적으로 인수 배열 생성
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    
    argv.push_back("HMSET");
    argvlen.push_back(5);
    
    argv.push_back(key.c_str());
    argvlen.push_back(key.length());
    
    for (const auto& pair : field_values) {
        argv.push_back(pair.first.c_str());
        argvlen.push_back(pair.first.length());
        argv.push_back(pair.second.c_str());
        argvlen.push_back(pair.second.length());
    }
    
    auto reply = executeCommandArgv(static_cast<int>(argv.size()), argv.data(), argvlen.data());
    bool success = reply && isReplyOK(reply.get());
    
    recordCommandEnd(success);
    
    if (!success) {
        logError("hmset", "Hash 다중 필드 설정 실패: " + key);
    }
    
    return success;
}

int RedisClientImpl::hdel(const std::string& key, const std::string& field) {
    recordCommandStart();
    
    auto reply = executeCommand("HDEL %s %s", key.c_str(), field.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("hdel", "Hash 필드 삭제 실패: " + key + "." + field);
        return 0;
    }
    
    return static_cast<int>(replyToInteger(reply.get()));
}

// =============================================================================
// List 조작
// =============================================================================

int RedisClientImpl::lpush(const std::string& key, const std::string& value) {
    recordCommandStart();
    
    auto reply = executeCommand("LPUSH %s %s", key.c_str(), value.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("lpush", "List 왼쪽 추가 실패: " + key);
        return 0;
    }
    
    return static_cast<int>(replyToInteger(reply.get()));
}

int RedisClientImpl::rpush(const std::string& key, const std::string& value) {
    recordCommandStart();
    
    auto reply = executeCommand("RPUSH %s %s", key.c_str(), value.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("rpush", "List 오른쪽 추가 실패: " + key);
        return 0;
    }
    
    return static_cast<int>(replyToInteger(reply.get()));
}

std::string RedisClientImpl::lpop(const std::string& key) {
    recordCommandStart();
    
    auto reply = executeCommand("LPOP %s", key.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("lpop", "List 왼쪽 제거 실패: " + key);
        return "";
    }
    
    return replyToString(reply.get());
}

std::string RedisClientImpl::rpop(const std::string& key) {
    recordCommandStart();
    
    auto reply = executeCommand("RPOP %s", key.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("rpop", "List 오른쪽 제거 실패: " + key);
        return "";
    }
    
    return replyToString(reply.get());
}

RedisClient::StringList RedisClientImpl::lrange(const std::string& key, int start, int stop) {
    recordCommandStart();
    
    auto reply = executeCommand("LRANGE %s %d %d", key.c_str(), start, stop);
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("lrange", "List 범위 조회 실패: " + key);
        return StringList{};
    }
    
    return replyToStringList(reply.get());
}

int RedisClientImpl::llen(const std::string& key) {
    recordCommandStart();
    
    auto reply = executeCommand("LLEN %s", key.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("llen", "List 길이 조회 실패: " + key);
        return 0;
    }
    
    return static_cast<int>(replyToInteger(reply.get()));
}

// =============================================================================
// Sorted Set 조작
// =============================================================================

int RedisClientImpl::zadd(const std::string& key, double score, const std::string& member) {
    recordCommandStart();
    
    auto reply = executeCommand("ZADD %s %.17g %s", key.c_str(), score, member.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("zadd", "Sorted Set 추가 실패: " + key);
        return 0;
    }
    
    return static_cast<int>(replyToInteger(reply.get()));
}

RedisClient::StringList RedisClientImpl::zrangebyscore(const std::string& key, double min_score, double max_score) {
    recordCommandStart();
    
    auto reply = executeCommand("ZRANGEBYSCORE %s %.17g %.17g", key.c_str(), min_score, max_score);
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("zrangebyscore", "Sorted Set 점수 범위 조회 실패: " + key);
        return StringList{};
    }
    
    return replyToStringList(reply.get());
}

int RedisClientImpl::zcard(const std::string& key) {
    recordCommandStart();
    
    auto reply = executeCommand("ZCARD %s", key.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("zcard", "Sorted Set 크기 조회 실패: " + key);
        return 0;
    }
    
    return static_cast<int>(replyToInteger(reply.get()));
}

int RedisClientImpl::zremrangebyscore(const std::string& key, double min_score, double max_score) {
    recordCommandStart();
    
    auto reply = executeCommand("ZREMRANGEBYSCORE %s %.17g %.17g", key.c_str(), min_score, max_score);
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("zremrangebyscore", "Sorted Set 점수 범위 삭제 실패: " + key);
        return 0;
    }
    
    return static_cast<int>(replyToInteger(reply.get()));
}

// =============================================================================
// Pub/Sub 메시징
// =============================================================================

bool RedisClientImpl::publish(const std::string& channel, const std::string& message) {
    recordCommandStart();
    
    auto reply = executeCommand("PUBLISH %s %s", channel.c_str(), message.c_str());
    bool success = reply != nullptr;
    
    recordCommandEnd(success);
    
    if (!success) {
        logError("publish", "메시지 발행 실패: " + channel);
    } else {
        LogManager::getInstance().log("redis", LogLevel::DEBUG,
            "메시지 발행: " + channel + " (구독자: " + std::to_string(replyToInteger(reply.get())) + "명)");
    }
    
    return success;
}

bool RedisClientImpl::subscribe(const std::string& channel) {
    std::lock_guard<std::mutex> lock(pubsub_mutex_);
    
    subscribed_channels_.insert(channel);
    
    if (!pubsub_running_) {
        startPubSubThread();
    }
    
    LogManager::getInstance().log("redis", LogLevel::INFO, "채널 구독: " + channel);
    return true;
}

bool RedisClientImpl::unsubscribe(const std::string& channel) {
    std::lock_guard<std::mutex> lock(pubsub_mutex_);
    
    subscribed_channels_.erase(channel);
    
    if (subscribed_channels_.empty() && subscribed_patterns_.empty()) {
        stopPubSubThread();
    }
    
    LogManager::getInstance().log("redis", LogLevel::INFO, "채널 구독 해제: " + channel);
    return true;
}

bool RedisClientImpl::psubscribe(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(pubsub_mutex_);
    
    subscribed_patterns_.insert(pattern);
    
    if (!pubsub_running_) {
        startPubSubThread();
    }
    
    LogManager::getInstance().log("redis", LogLevel::INFO, "패턴 구독: " + pattern);
    return true;
}

bool RedisClientImpl::punsubscribe(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(pubsub_mutex_);
    
    subscribed_patterns_.erase(pattern);
    
    if (subscribed_channels_.empty() && subscribed_patterns_.empty()) {
        stopPubSubThread();
    }
    
    LogManager::getInstance().log("redis", LogLevel::INFO, "패턴 구독 해제: " + pattern);
    return true;
}

void RedisClientImpl::setMessageCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(pubsub_mutex_);
    message_callback_ = callback;
}

// =============================================================================
// 배치 처리
// =============================================================================

bool RedisClientImpl::mset(const StringMap& key_values) {
    if (key_values.empty()) return true;
    
    recordCommandStart();
    
    // 동적으로 인수 배열 생성
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    
    argv.push_back("MSET");
    argvlen.push_back(4);
    
    for (const auto& pair : key_values) {
        argv.push_back(pair.first.c_str());
        argvlen.push_back(pair.first.length());
        argv.push_back(pair.second.c_str());
        argvlen.push_back(pair.second.length());
    }
    
    auto reply = executeCommandArgv(static_cast<int>(argv.size()), argv.data(), argvlen.data());
    bool success = reply && isReplyOK(reply.get());
    
    recordCommandEnd(success);
    
    if (!success) {
        logError("mset", "다중 키 설정 실패");
    }
    
    return success;
}

RedisClient::StringList RedisClientImpl::mget(const StringList& keys) {
    if (keys.empty()) return StringList{};
    
    recordCommandStart();
    
    // 동적으로 인수 배열 생성
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    
    argv.push_back("MGET");
    argvlen.push_back(4);
    
    for (const auto& key : keys) {
        argv.push_back(key.c_str());
        argvlen.push_back(key.length());
    }
    
    auto reply = executeCommandArgv(static_cast<int>(argv.size()), argv.data(), argvlen.data());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("mget", "다중 키 조회 실패");
        return StringList{};
    }
    
    return replyToStringList(reply.get());
}

// =============================================================================
// 트랜잭션 지원
// =============================================================================

bool RedisClientImpl::multi() {
    recordCommandStart();
    
    auto reply = executeCommand("MULTI");
    bool success = reply && isReplyOK(reply.get());
    
    recordCommandEnd(success);
    
    if (success) {
        in_transaction_ = true;
        transaction_commands_.clear();
        LogManager::getInstance().log("redis", LogLevel::DEBUG, "트랜잭션 시작");
    } else {
        logError("multi", "트랜잭션 시작 실패");
    }
    
    return success;
}

bool RedisClientImpl::exec() {
    if (!in_transaction_) {
        logError("exec", "트랜잭션이 시작되지 않음");
        return false;
    }
    
    recordCommandStart();
    
    auto reply = executeCommand("EXEC");
    bool success = reply != nullptr && !isReplyError(reply.get());
    
    recordCommandEnd(success);
    
    in_transaction_ = false;
    transaction_commands_.clear();
    
    if (success) {
        LogManager::getInstance().log("redis", LogLevel::DEBUG, "트랜잭션 실행 완료");
    } else {
        logError("exec", "트랜잭션 실행 실패");
    }
    
    return success;
}

bool RedisClientImpl::discard() {
    if (!in_transaction_) {
        logError("discard", "트랜잭션이 시작되지 않음");
        return false;
    }
    
    recordCommandStart();
    
    auto reply = executeCommand("DISCARD");
    bool success = reply && isReplyOK(reply.get());
    
    recordCommandEnd(success);
    
    in_transaction_ = false;
    transaction_commands_.clear();
    
    if (success) {
        LogManager::getInstance().log("redis", LogLevel::DEBUG, "트랜잭션 취소 완료");
    } else {
        logError("discard", "트랜잭션 취소 실패");
    }
    
    return success;
}

// =============================================================================
// 상태 및 진단
// =============================================================================

bool RedisClientImpl::ping() {
    recordCommandStart();
    
    auto reply = executeCommand("PING");
    bool success = reply && reply->type == REDIS_REPLY_STATUS && 
                   strcmp(reply->str, "PONG") == 0;
    
    recordCommandEnd(success);
    
    return success;
}

bool RedisClientImpl::select(int db_index) {
    return selectDatabase(db_index);
}

RedisClient::StringMap RedisClientImpl::info() {
    recordCommandStart();
    
    auto reply = executeCommand("INFO");
    
    recordCommandEnd(reply != nullptr);
    
    StringMap info_map;
    
    if (!reply || reply->type != REDIS_REPLY_STRING) {
        logError("info", "서버 정보 조회 실패");
        return info_map;
    }
    
    // INFO 응답 파싱 (간단한 구현)
    std::string info_str(reply->str, reply->len);
    std::istringstream stream(info_str);
    std::string line;
    
    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            
            // 개행 문자 제거
            if (!value.empty() && value.back() == '\r') {
                value.pop_back();
            }
            
            info_map[key] = value;
        }
    }
    
    return info_map;
}

int RedisClientImpl::dbsize() {
    recordCommandStart();
    
    auto reply = executeCommand("DBSIZE");
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("dbsize", "데이터베이스 크기 조회 실패");
        return 0;
    }
    
    return static_cast<int>(replyToInteger(reply.get()));
}

// =============================================================================
// 고급 기능들
// =============================================================================

std::string RedisClientImpl::evalScript(const std::string& script, 
                                        const StringList& keys,
                                        const StringList& args) {
    recordCommandStart();
    
    // 동적으로 인수 배열 생성
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    
    argv.push_back("EVAL");
    argvlen.push_back(4);
    
    argv.push_back(script.c_str());
    argvlen.push_back(script.length());
    
    std::string keys_count = std::to_string(keys.size());
    argv.push_back(keys_count.c_str());
    argvlen.push_back(keys_count.length());
    
    for (const auto& key : keys) {
        argv.push_back(key.c_str());
        argvlen.push_back(key.length());
    }
    
    for (const auto& arg : args) {
        argv.push_back(arg.c_str());
        argvlen.push_back(arg.length());
    }
    
    auto reply = executeCommandArgv(static_cast<int>(argv.size()), argv.data(), argvlen.data());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("eval", "Lua 스크립트 실행 실패");
        return "";
    }
    
    return replyToString(reply.get());
}

std::pair<std::string, RedisClient::StringList> RedisClientImpl::scan(const std::string& cursor,
                                                                      const std::string& pattern,
                                                                      int count) {
    recordCommandStart();
    
    std::unique_ptr<redisReply, void(*)(redisReply*)> reply;
    
    if (pattern.empty()) {
        reply = executeCommand("SCAN %s COUNT %d", cursor.c_str(), count);
    } else {
        reply = executeCommand("SCAN %s MATCH %s COUNT %d", cursor.c_str(), pattern.c_str(), count);
    }
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements != 2) {
        logError("scan", "키 스캔 실패");
        return std::make_pair("0", StringList{});
    }
    
    std::string new_cursor = replyToString(reply->element[0]);
    StringList keys = replyToStringList(reply->element[1]);
    
    return std::make_pair(new_cursor, keys);
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

std::unique_ptr<redisReply, void(*)(redisReply*)> RedisClientImpl::executeCommand(const char* format, ...) {
    if (!isConnected() && !reconnect()) {
        return std::unique_ptr<redisReply, void(*)(redisReply*)>(nullptr, &RedisClientImpl::freeRedisReply);
    }
    
    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    va_list args;
    va_start(args, format);
    
    redisReply* reply = static_cast<redisReply*>(redisvCommand(context_.get(), format, args));
    
    va_end(args);
    
    if (!reply) {
        handleConnectionError();
        return std::unique_ptr<redisReply, void(*)(redisReply*)>(nullptr, &RedisClientImpl::freeRedisReply);
    }
    
    return std::unique_ptr<redisReply, void(*)(redisReply*)>(reply, &RedisClientImpl::freeRedisReply);
}

std::unique_ptr<redisReply, void(*)(redisReply*)> RedisClientImpl::executeCommandArgv(int argc, const char** argv, const size_t* argvlen) {
    if (!isConnected() && !reconnect()) {
        return std::unique_ptr<redisReply, void(*)(redisReply*)>(nullptr, &RedisClientImpl::freeRedisReply);
    }
    
    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    redisReply* reply = static_cast<redisReply*>(redisCommandArgv(context_.get(), argc, argv, argvlen));
    
    if (!reply) {
        handleConnectionError();
        return std::unique_ptr<redisReply, void(*)(redisReply*)>(nullptr, &RedisClientImpl::freeRedisReply);
    }
    
    return std::unique_ptr<redisReply, void(*)(redisReply*)>(reply, &RedisClientImpl::freeRedisReply);
}

bool RedisClientImpl::reconnect() {
    if (!auto_reconnect_ || reconnect_attempts_ >= MAX_RECONNECT_ATTEMPTS) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    if (now - last_reconnect_attempt_ < RECONNECT_DELAY) {
        return false;
    }
    
    last_reconnect_attempt_ = now;
    reconnect_attempts_++;
    
    LogManager::getInstance().log("redis", LogLevel::INFO,
        "Redis 재연결 시도 #" + std::to_string(reconnect_attempts_));
    
    return connectInternal();
}

bool RedisClientImpl::handleConnectionError() {
    if (context_ && isConnectionError(context_.get())) {
        connected_ = false;
        LogManager::getInstance().log("redis", LogLevel::WARN, "연결 오류 감지, 재연결 시도");
        return auto_reconnect_ && reconnect();
    }
    return false;
}

bool RedisClientImpl::isConnectionError(redisContext* ctx) const {
    return ctx && (ctx->err == REDIS_ERR_IO || ctx->err == REDIS_ERR_EOF);
}

// =============================================================================
// 응답 변환 헬퍼들
// =============================================================================

std::string RedisClientImpl::replyToString(redisReply* reply) {
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

long long RedisClientImpl::replyToInteger(redisReply* reply) {
    if (!reply) return 0;
    
    switch (reply->type) {
        case REDIS_REPLY_INTEGER:
            return reply->integer;
        case REDIS_REPLY_STRING:
        case REDIS_REPLY_STATUS:
            try {
                return std::stoll(std::string(reply->str, reply->len));
            } catch (...) {
                return 0;
            }
        default:
            return 0;
    }
}

RedisClient::StringList RedisClientImpl::replyToStringList(redisReply* reply) {
    StringList result;
    
    if (!reply || reply->type != REDIS_REPLY_ARRAY) {
        return result;
    }
    
    result.reserve(reply->elements);
    
    for (size_t i = 0; i < reply->elements; ++i) {
        result.push_back(replyToString(reply->element[i]));
    }
    
    return result;
}

RedisClient::StringMap RedisClientImpl::replyToStringMap(redisReply* reply) {
    StringMap result;
    
    if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements % 2 != 0) {
        return result;
    }
    
    for (size_t i = 0; i < reply->elements; i += 2) {
        std::string key = replyToString(reply->element[i]);
        std::string value = replyToString(reply->element[i + 1]);
        result[key] = value;
    }
    
    return result;
}

bool RedisClientImpl::isReplyOK(redisReply* reply) {
    return reply && reply->type == REDIS_REPLY_STATUS && 
           strcmp(reply->str, "OK") == 0;
}

bool RedisClientImpl::isReplyError(redisReply* reply) {
    return reply && reply->type == REDIS_REPLY_ERROR;
}

// =============================================================================
// Pub/Sub 스레드 관리
// =============================================================================

void RedisClientImpl::startPubSubThread() {
    if (pubsub_running_) return;
    
    pubsub_running_ = true;
    pubsub_thread_ = std::thread(&RedisClientImpl::pubsubThreadWorker, this);
    
    LogManager::getInstance().log("redis", LogLevel::INFO, "Pub/Sub 스레드 시작됨");
}

void RedisClientImpl::stopPubSubThread() {
    if (!pubsub_running_) return;
    
    pubsub_running_ = false;
    pubsub_cv_.notify_all();
    
    if (pubsub_thread_.joinable()) {
        pubsub_thread_.join();
    }
    
    LogManager::getInstance().log("redis", LogLevel::INFO, "Pub/Sub 스레드 종료됨");
}

void RedisClientImpl::pubsubThreadWorker() {
    // 별도의 연결 생성 (Pub/Sub 전용)
    redisContext* pubsub_ctx = redisConnect(host_.c_str(), port_);
    if (!pubsub_ctx || pubsub_ctx->err) {
        LogManager::getInstance().log("redis", LogLevel::ERROR, "Pub/Sub 연결 실패");
        if (pubsub_ctx) redisFree(pubsub_ctx);
        return;
    }
    
    // 인증
    if (!password_.empty()) {
        redisReply* auth_reply = static_cast<redisReply*>(redisCommand(pubsub_ctx, "AUTH %s", password_.c_str()));
        if (!auth_reply || !isReplyOK(auth_reply)) {
            LogManager::getInstance().log("redis", LogLevel::ERROR, "Pub/Sub 인증 실패");
            if (auth_reply) freeReplyObject(auth_reply);
            redisFree(pubsub_ctx);
            return;
        }
        freeReplyObject(auth_reply);
    }
    
    // 구독
    {
        std::lock_guard<std::mutex> lock(pubsub_mutex_);
        
        for (const auto& channel : subscribed_channels_) {
            redisReply* sub_reply = static_cast<redisReply*>(redisCommand(pubsub_ctx, "SUBSCRIBE %s", channel.c_str()));
            if (sub_reply) freeReplyObject(sub_reply);
        }
        
        for (const auto& pattern : subscribed_patterns_) {
            redisReply* psub_reply = static_cast<redisReply*>(redisCommand(pubsub_ctx, "PSUBSCRIBE %s", pattern.c_str()));
            if (psub_reply) freeReplyObject(psub_reply);
        }
    }
    
    // 메시지 수신 루프
    while (pubsub_running_) {
        redisReply* reply = nullptr;
        
        if (redisGetReply(pubsub_ctx, reinterpret_cast<void**>(&reply)) == REDIS_OK && reply) {
            handlePubSubMessage(reply);
            freeReplyObject(reply);
        } else {
            // 연결 오류 또는 타임아웃
            if (pubsub_ctx->err) {
                LogManager::getInstance().log("redis", LogLevel::WARN, 
                    "Pub/Sub 연결 오류: " + std::string(pubsub_ctx->errstr));
                break;
            }
        }
    }
    
    redisFree(pubsub_ctx);
}

void RedisClientImpl::handlePubSubMessage(redisReply* reply) {
    if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements < 3) {
        return;
    }
    
    std::string message_type = replyToString(reply->element[0]);
    
    if (message_type == "message" && reply->elements >= 3) {
        std::string channel = replyToString(reply->element[1]);
        std::string message = replyToString(reply->element[2]);
        
        if (message_callback_) {
            message_callback_(channel, message);
        }
        
        LogManager::getInstance().log("redis", LogLevel::DEBUG,
            "Pub/Sub 메시지 수신: " + channel + " -> " + message.substr(0, 50) + "...");
    }
    else if (message_type == "pmessage" && reply->elements >= 4) {
        std::string pattern = replyToString(reply->element[1]);
        std::string channel = replyToString(reply->element[2]);
        std::string message = replyToString(reply->element[3]);
        
        if (message_callback_) {
            message_callback_(channel, message);
        }
        
        LogManager::getInstance().log("redis", LogLevel::DEBUG,
            "Pub/Sub 패턴 메시지 수신: " + pattern + " (" + channel + ") -> " + message.substr(0, 50) + "...");
    }
}

// =============================================================================
// 비동기 스레드 관리
// =============================================================================

void RedisClientImpl::startAsyncThread() {
    if (async_running_) return;
    
    async_running_ = true;
    async_thread_ = std::thread(&RedisClientImpl::asyncThreadWorker, this);
    
    LogManager::getInstance().log("redis", LogLevel::DEBUG, "비동기 작업 스레드 시작됨");
}

void RedisClientImpl::stopAsyncThread() {
    if (!async_running_) return;
    
    async_running_ = false;
    async_cv_.notify_all();
    
    if (async_thread_.joinable()) {
        async_thread_.join();
    }
    
    LogManager::getInstance().log("redis", LogLevel::DEBUG, "비동기 작업 스레드 종료됨");
}

void RedisClientImpl::asyncThreadWorker() {
    while (async_running_) {
        std::unique_lock<std::mutex> lock(async_mutex_);
        async_cv_.wait(lock, [this] { return !async_task_queue_.empty() || !async_running_; });
        
        if (!async_running_) break;
        
        if (!async_task_queue_.empty()) {
            auto task = async_task_queue_.front();
            async_task_queue_.pop();
            lock.unlock();
            
            try {
                task();
            } catch (const std::exception& e) {
                logError("async_task", e.what());
            }
        }
    }
}

// =============================================================================
// 성능 및 통계
// =============================================================================

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

RedisClientImpl::ConnectionStats RedisClientImpl::getConnectionStats() const {
    ConnectionStats stats;
    
    stats.total_commands = total_commands_;
    stats.successful_commands = successful_commands_;
    stats.failed_commands = failed_commands_;
    stats.success_rate = total_commands_ > 0 ? 
        static_cast<double>(successful_commands_) / total_commands_ : 0.0;
    
    auto now = std::chrono::steady_clock::now();
    stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(now - connect_time_);
    stats.last_command_latency = calculateLatency();
    
    stats.reconnect_attempts = reconnect_attempts_;
    stats.is_connected = connected_;
    stats.pubsub_active = pubsub_running_;
    
    {
        std::lock_guard<std::mutex> lock(pubsub_mutex_);
        stats.subscribed_channels_count = subscribed_channels_.size();
        stats.subscribed_patterns_count = subscribed_patterns_.size();
    }
    
    return stats;
}

void RedisClientImpl::resetStats() {
    total_commands_ = 0;
    successful_commands_ = 0;
    failed_commands_ = 0;
    reconnect_attempts_ = 0;
    connect_time_ = std::chrono::steady_clock::now();
    
    LogManager::getInstance().log("redis", LogLevel::DEBUG, "Redis 통계 리셋");
}

std::chrono::milliseconds RedisClientImpl::calculateLatency() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - last_command_time_);
}

void RedisClientImpl::setTimeouts(int connect_timeout_ms, int command_timeout_ms) {
    if (context_) {
        struct timeval timeout = {
            .tv_sec = command_timeout_ms / 1000,
            .tv_usec = (command_timeout_ms % 1000) * 1000
        };
        
        redisSetTimeout(context_.get(), timeout);
        
        LogManager::getInstance().log("redis", LogLevel::DEBUG,
            "타임아웃 설정 - 연결: " + std::to_string(connect_timeout_ms) + "ms, 명령어: " + std::to_string(command_timeout_ms) + "ms");
    }
}

// =============================================================================
// 파이프라인 지원
// =============================================================================

bool RedisClientImpl::startPipeline() {
    if (pipeline_mode_) {
        LogManager::getInstance().log("redis", LogLevel::WARN, "파이프라인이 이미 시작됨");
        return true;
    }
    
    pipeline_mode_ = true;
    pipeline_commands_.clear();
    
    LogManager::getInstance().log("redis", LogLevel::DEBUG, "파이프라인 모드 시작");
    return true;
}

bool RedisClientImpl::addToPipeline(const std::string& command) {
    if (!pipeline_mode_) {
        logError("addToPipeline", "파이프라인 모드가 시작되지 않음");
        return false;
    }
    
    pipeline_commands_.push_back(command);
    return true;
}

std::vector<std::unique_ptr<redisReply, void(*)(redisReply*)>> RedisClientImpl::executePipeline() {
    std::vector<std::unique_ptr<redisReply, void(*)(redisReply*)>> results;
    
    if (!pipeline_mode_ || pipeline_commands_.empty()) {
        pipeline_mode_ = false;
        return results;
    }
    
    if (!isConnected() && !reconnect()) {
        pipeline_mode_ = false;
        return results;
    }
    
    std::lock_guard<std::mutex> lock(operation_mutex_);
    
    // 모든 명령어를 파이프라인으로 전송
    for (const auto& command : pipeline_commands_) {
        redisAppendCommand(context_.get(), command.c_str());
    }
    
    // 모든 응답 수신
    results.reserve(pipeline_commands_.size());
    
    for (size_t i = 0; i < pipeline_commands_.size(); ++i) {
        redisReply* reply = nullptr;
        
        if (redisGetReply(context_.get(), reinterpret_cast<void**>(&reply)) == REDIS_OK) {
            results.emplace_back(reply, &RedisClientImpl::freeRedisReply);
        } else {
            results.emplace_back(nullptr, &RedisClientImpl::freeRedisReply);
            handleConnectionError();
        }
    }
    
    pipeline_mode_ = false;
    pipeline_commands_.clear();
    
    LogManager::getInstance().log("redis", LogLevel::DEBUG,
        "파이프라인 실행 완료: " + std::to_string(results.size()) + "개 명령어");
    
    return results;
}

// =============================================================================
// 스크립트 지원 추가 메서드들
// =============================================================================

std::string RedisClientImpl::evalSha(const std::string& sha1,
                                     const StringList& keys,
                                     const StringList& args) {
    recordCommandStart();
    
    // 동적으로 인수 배열 생성
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    
    argv.push_back("EVALSHA");
    argvlen.push_back(7);
    
    argv.push_back(sha1.c_str());
    argvlen.push_back(sha1.length());
    
    std::string keys_count = std::to_string(keys.size());
    argv.push_back(keys_count.c_str());
    argvlen.push_back(keys_count.length());
    
    for (const auto& key : keys) {
        argv.push_back(key.c_str());
        argvlen.push_back(key.length());
    }
    
    for (const auto& arg : args) {
        argv.push_back(arg.c_str());
        argvlen.push_back(arg.length());
    }
    
    auto reply = executeCommandArgv(static_cast<int>(argv.size()), argv.data(), argvlen.data());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("evalsha", "스크립트 SHA1 실행 실패: " + sha1);
        return "";
    }
    
    return replyToString(reply.get());
}

std::string RedisClientImpl::scriptLoad(const std::string& script) {
    recordCommandStart();
    
    auto reply = executeCommand("SCRIPT LOAD %s", script.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("script_load", "스크립트 로드 실패");
        return "";
    }
    
    std::string sha1 = replyToString(reply.get());
    
    LogManager::getInstance().log("redis", LogLevel::DEBUG,
        "스크립트 로드 완료: " + sha1);
    
    return sha1;
}

// =============================================================================
// 스캔 메서드들 추가 구현
// =============================================================================

std::pair<std::string, RedisClient::StringMap> RedisClientImpl::hscan(const std::string& key,
                                                                      const std::string& cursor,
                                                                      const std::string& pattern,
                                                                      int count) {
    recordCommandStart();
    
    std::unique_ptr<redisReply, void(*)(redisReply*)> reply;
    
    if (pattern.empty()) {
        reply = executeCommand("HSCAN %s %s COUNT %d", key.c_str(), cursor.c_str(), count);
    } else {
        reply = executeCommand("HSCAN %s %s MATCH %s COUNT %d", 
                              key.c_str(), cursor.c_str(), pattern.c_str(), count);
    }
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements != 2) {
        logError("hscan", "Hash 스캔 실패: " + key);
        return std::make_pair("0", StringMap{});
    }
    
    std::string new_cursor = replyToString(reply->element[0]);
    StringMap fields = replyToStringMap(reply->element[1]);
    
    return std::make_pair(new_cursor, fields);
}

std::pair<std::string, std::map<std::string, double>> RedisClientImpl::zscan(const std::string& key,
                                                                             const std::string& cursor,
                                                                             const std::string& pattern,
                                                                             int count) {
    recordCommandStart();
    
    std::unique_ptr<redisReply, void(*)(redisReply*)> reply;
    
    if (pattern.empty()) {
        reply = executeCommand("ZSCAN %s %s COUNT %d", key.c_str(), cursor.c_str(), count);
    } else {
        reply = executeCommand("ZSCAN %s %s MATCH %s COUNT %d", 
                              key.c_str(), cursor.c_str(), pattern.c_str(), count);
    }
    
    recordCommandEnd(reply != nullptr);
    
    std::map<std::string, double> members;
    
    if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements != 2) {
        logError("zscan", "Sorted Set 스캔 실패: " + key);
        return std::make_pair("0", members);
    }
    
    std::string new_cursor = replyToString(reply->element[0]);
    
    // Sorted Set 결과는 [member, score, member, score, ...] 형태
    redisReply* members_array = reply->element[1];
    if (members_array && members_array->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < members_array->elements; i += 2) {
            if (i + 1 < members_array->elements) {
                std::string member = replyToString(members_array->element[i]);
                double score = 0.0;
                
                try {
                    std::string score_str = replyToString(members_array->element[i + 1]);
                    score = std::stod(score_str);
                } catch (...) {
                    score = 0.0;
                }
                
                members[member] = score;
            }
        }
    }
    
    return std::make_pair(new_cursor, members);
}

// =============================================================================
// 에러 처리 및 로깅
// =============================================================================

void RedisClientImpl::logError(const std::string& operation, const std::string& error_message) {
    LogManager::getInstance().log("redis", LogLevel::ERROR,
        "Redis " + operation + " 실패: " + error_message);
}

void RedisClientImpl::logRedisError(const std::string& operation, redisContext* ctx) {
    if (ctx && ctx->err) {
        std::string error_msg = "오류 코드 " + std::to_string(ctx->err) + ": " + std::string(ctx->errstr);
        logError(operation, error_msg);
    }
}

// =============================================================================
// 정적 메모리 관리 함수들
// =============================================================================

void RedisClientImpl::freeRedisReply(redisReply* reply) {
    if (reply) {
        freeReplyObject(reply);
    }
}

void RedisClientImpl::freeRedisContext(redisContext* ctx) {
    if (ctx) {
        redisFree(ctx);
    }
}

void RedisClientImpl::freeRedisAsyncContext(redisAsyncContext* ctx) {
    if (ctx) {
        redisAsyncFree(ctx);
    }
}

// =============================================================================
// 문자열 유틸리티
// =============================================================================

std::string RedisClientImpl::escapeString(const std::string& str) {
    std::string escaped;
    escaped.reserve(str.length() * 2);
    
    for (char c : str) {
        switch (c) {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += c;
        }
    }
    
    return escaped;
}

std::vector<std::string> RedisClientImpl::splitCommand(const std::string& command) {
    std::vector<std::string> tokens;
    std::istringstream stream(command);
    std::string token;
    
    while (stream >> token) {
        tokens.push_back(token);
    }
    
    return tokens;
}