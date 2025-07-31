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
        
        // 연결 설정
        setupContext(ctx);
        context_.reset(ctx);
        
        // 인증 확인
        if (!authenticateIfNeeded()) {
            logError("connect", "Redis 인증 실패");
            context_.reset();
            return false;
        }
        
        // DB 선택
        if (selected_db_ != 0 && !selectDatabase(selected_db_)) {
            logError("connect", "데이터베이스 선택 실패");
            context_.reset();
            return false;
        }
        
        connected_ = true;
        reconnect_attempts_ = 0;
        connect_time_ = std::chrono::steady_clock::now();
        
        // 비동기 스레드 시작
        startAsyncThread();
        
        LogManager::getInstance().log("redis", LogLevel::INFO, 
            "Redis 연결 성공: " + host_ + ":" + std::to_string(port_));
        
        return true;
        
    } catch (const std::exception& e) {
        logError("connect", "연결 중 예외 발생: " + std::string(e.what()));
        return false;
    }
}

void RedisClientImpl::setupContext(redisContext* ctx) {
    if (!ctx) return;
    
    // 타임아웃 설정
    struct timeval tv;
    tv.tv_sec = COMMAND_TIMEOUT.count() / 1000;
    tv.tv_usec = (COMMAND_TIMEOUT.count() % 1000) * 1000;
    
    redisSetTimeout(ctx, tv);
    
    LogManager::getInstance().log("redis", LogLevel::INFO, 
        "Redis 컨텍스트 설정 완료 (타임아웃: " + std::to_string(COMMAND_TIMEOUT.count()) + "ms)");
}

bool RedisClientImpl::authenticateIfNeeded() {
    if (password_.empty()) {
        return true;
    }
    
    auto reply = executeCommand("AUTH %s", password_.c_str());
    if (!reply || !isReplyOK(reply.get())) {
        logError("authenticate", "Redis 인증 실패");
        return false;
    }
    
    LogManager::getInstance().log("redis", LogLevel::INFO, "Redis 인증 성공");
    return true;
}

bool RedisClientImpl::selectDatabase(int db_index) {
    auto reply = executeCommand("SELECT %d", db_index);
    if (!reply || !isReplyOK(reply.get())) {
        logError("select", "데이터베이스 선택 실패: DB " + std::to_string(db_index));
        return false;
    }
    
    selected_db_ = db_index;
    LogManager::getInstance().log("redis", LogLevel::INFO, 
        "데이터베이스 선택 완료: DB " + std::to_string(db_index));
    return true;
}

void RedisClientImpl::disconnect() {
    std::lock_guard<std::recursive_mutex> lock(connection_mutex_);
    
    if (!connected_) return;
    
    connected_ = false;
    
    // Pub/Sub 스레드 중지
    stopPubSubThread();
    
    // 비동기 스레드 중지
    stopAsyncThread();
    
    // 트랜잭션 정리
    if (in_transaction_) {
        discard();
    }
    
    pipeline_commands_.clear();
    pipeline_mode_ = false;
    
    // 컨텍스트 정리
    context_.reset();
    async_context_.reset();
    
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
    
    auto reply = executeCommand("SET %s %s", key.c_str(), value.c_str());
    
    bool success = reply && isReplyOK(reply.get());
    recordCommandEnd(success);
    
    if (!success) {
        logError("set", "키 설정 실패: " + key);
    }
    
    return success;
}

bool RedisClientImpl::setex(const std::string& key, const std::string& value, int expire_seconds) {
    if (!isConnected()) {
        logError("setex", "Redis 연결되지 않음");
        return false;
    }
    
    if (expire_seconds <= 0) {
        logError("setex", "잘못된 만료 시간: " + std::to_string(expire_seconds));
        return false;
    }
    
    recordCommandStart();
    
    auto reply = executeCommand("SETEX %s %d %s", key.c_str(), expire_seconds, value.c_str());
    
    bool success = reply && isReplyOK(reply.get());
    recordCommandEnd(success);
    
    if (!success) {
        logError("setex", "키 설정 실패 (TTL 포함): " + key);
    }
    
    return success;
}

std::string RedisClientImpl::get(const std::string& key) {
    if (!isConnected()) {
        logError("get", "Redis 연결되지 않음");
        return "";
    }
    
    recordCommandStart();
    
    auto reply = executeCommand("GET %s", key.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("get", "키 조회 실패: " + key);
        return "";
    }
    
    return replyToString(reply.get());
}

int RedisClientImpl::del(const std::string& key) {
    if (!isConnected()) {
        logError("del", "Redis 연결되지 않음");
        return 0;
    }
    
    recordCommandStart();
    
    auto reply = executeCommand("DEL %s", key.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("del", "키 삭제 실패: " + key);
        return 0;
    }
    
    return static_cast<int>(replyToInteger(reply.get()));
}

bool RedisClientImpl::exists(const std::string& key) {
    if (!isConnected()) {
        logError("exists", "Redis 연결되지 않음");
        return false;
    }
    
    recordCommandStart();
    
    auto reply = executeCommand("EXISTS %s", key.c_str());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("exists", "키 존재 확인 실패: " + key);
        return false;
    }
    
    return replyToInteger(reply.get()) > 0;
}

bool RedisClientImpl::expire(const std::string& key, int seconds) {
    if (!isConnected()) {
        logError("expire", "Redis 연결되지 않음");
        return false;
    }
    
    recordCommandStart();
    
    auto reply = executeCommand("EXPIRE %s %d", key.c_str(), seconds);
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("expire", "키 만료 설정 실패: " + key);
        return false;
    }
    
    return replyToInteger(reply.get()) > 0;
}

int RedisClientImpl::ttl(const std::string& key) {
    if (!isConnected()) {
        logError("ttl", "Redis 연결되지 않음");
        return -2;
    }
    
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
    auto reply = executeCommand("HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
    return reply && replyToInteger(reply.get()) >= 0;
}

std::string RedisClientImpl::hget(const std::string& key, const std::string& field) {
    auto reply = executeCommand("HGET %s %s", key.c_str(), field.c_str());
    return reply ? replyToString(reply.get()) : "";
}

RedisClient::StringMap RedisClientImpl::hgetall(const std::string& /* key */) {
    // 🔧 수정: 매개변수 주석 처리로 워닝 제거
    return StringMap{};
}

int RedisClientImpl::hdel(const std::string& key, const std::string& field) {
    auto reply = executeCommand("HDEL %s %s", key.c_str(), field.c_str());
    return reply ? static_cast<int>(replyToInteger(reply.get())) : 0;
}

bool RedisClientImpl::hexists(const std::string& key, const std::string& field) {
    auto reply = executeCommand("HEXISTS %s %s", key.c_str(), field.c_str());
    return reply && replyToInteger(reply.get()) > 0;
}

int RedisClientImpl::hlen(const std::string& key) {
    auto reply = executeCommand("HLEN %s", key.c_str());
    return reply ? static_cast<int>(replyToInteger(reply.get())) : 0;
}

// =============================================================================
// List 조작
// =============================================================================

int RedisClientImpl::lpush(const std::string& key, const std::string& value) {
    auto reply = executeCommand("LPUSH %s %s", key.c_str(), value.c_str());
    return reply ? static_cast<int>(replyToInteger(reply.get())) : 0;
}

int RedisClientImpl::rpush(const std::string& key, const std::string& value) {
    auto reply = executeCommand("RPUSH %s %s", key.c_str(), value.c_str());
    return reply ? static_cast<int>(replyToInteger(reply.get())) : 0;
}

std::string RedisClientImpl::lpop(const std::string& key) {
    auto reply = executeCommand("LPOP %s", key.c_str());
    return reply ? replyToString(reply.get()) : "";
}

std::string RedisClientImpl::rpop(const std::string& key) {
    auto reply = executeCommand("RPOP %s", key.c_str());
    return reply ? replyToString(reply.get()) : "";
}

RedisClient::StringList RedisClientImpl::lrange(const std::string& /* key */, int /* start */, int /* stop */) {
    // 🔧 수정: 매개변수 주석 처리로 워닝 제거
    return StringList{};
}

int RedisClientImpl::llen(const std::string& key) {
    auto reply = executeCommand("LLEN %s", key.c_str());
    return reply ? static_cast<int>(replyToInteger(reply.get())) : 0;
}

// =============================================================================
// Set 조작
// =============================================================================

int RedisClientImpl::sadd(const std::string& key, const std::string& member) {
    auto reply = executeCommand("SADD %s %s", key.c_str(), member.c_str());
    return reply ? static_cast<int>(replyToInteger(reply.get())) : 0;
}

int RedisClientImpl::srem(const std::string& key, const std::string& member) {
    auto reply = executeCommand("SREM %s %s", key.c_str(), member.c_str());
    return reply ? static_cast<int>(replyToInteger(reply.get())) : 0;
}

bool RedisClientImpl::sismember(const std::string& key, const std::string& member) {
    auto reply = executeCommand("SISMEMBER %s %s", key.c_str(), member.c_str());
    return reply && replyToInteger(reply.get()) > 0;
}

RedisClient::StringList RedisClientImpl::smembers(const std::string& /* key */) {
    // 🔧 수정: 매개변수 주석 처리로 워닝 제거
    return StringList{};
}

int RedisClientImpl::scard(const std::string& key) {
    auto reply = executeCommand("SCARD %s", key.c_str());
    return reply ? static_cast<int>(replyToInteger(reply.get())) : 0;
}

// =============================================================================
// Sorted Set 조작
// =============================================================================

int RedisClientImpl::zadd(const std::string& key, double score, const std::string& member) {
    auto reply = executeCommand("ZADD %s %f %s", key.c_str(), score, member.c_str());
    return reply ? static_cast<int>(replyToInteger(reply.get())) : 0;
}

int RedisClientImpl::zrem(const std::string& key, const std::string& member) {
    auto reply = executeCommand("ZREM %s %s", key.c_str(), member.c_str());
    return reply ? static_cast<int>(replyToInteger(reply.get())) : 0;
}

RedisClient::StringList RedisClientImpl::zrange(const std::string& /* key */, int /* start */, int /* stop */) {
    // 🔧 수정: 매개변수 주석 처리로 워닝 제거
    return StringList{};
}

int RedisClientImpl::zcard(const std::string& key) {
    auto reply = executeCommand("ZCARD %s", key.c_str());
    return reply ? static_cast<int>(replyToInteger(reply.get())) : 0;
}

// =============================================================================
// Pub/Sub (최소 구현)
// =============================================================================

bool RedisClientImpl::subscribe(const std::string& /* channel */) {
    return false; // 향후 구현
}

bool RedisClientImpl::unsubscribe(const std::string& /* channel */) {
    return false; // 향후 구현
}

int RedisClientImpl::publish(const std::string& channel, const std::string& message) {
    auto reply = executeCommand("PUBLISH %s %s", channel.c_str(), message.c_str());
    return reply ? static_cast<int>(replyToInteger(reply.get())) : 0;
}

bool RedisClientImpl::psubscribe(const std::string& /* pattern */) {
    return false; // 향후 구현
}

bool RedisClientImpl::punsubscribe(const std::string& /* pattern */) {
    return false; // 향후 구현
}

void RedisClientImpl::setMessageCallback(MessageCallback callback) {
    message_callback_ = callback;
}

// =============================================================================
// 배치 처리 (최소 구현)
// =============================================================================

bool RedisClientImpl::mset(const StringMap& /* key_values */) {
    return false; // 향후 구현
}

RedisClient::StringList RedisClientImpl::mget(const StringList& /* keys */) {
    return StringList{};
}

// =============================================================================
// 트랜잭션
// =============================================================================

bool RedisClientImpl::multi() {
    recordCommandStart();
    
    auto reply = executeCommand("MULTI");
    
    bool success = reply && isReplyOK(reply.get());
    recordCommandEnd(success);
    
    if (success) {
        in_transaction_ = true;
        transaction_commands_.clear();
        LogManager::getInstance().log("redis", LogLevel::DEBUG_LEVEL, "트랜잭션 시작");
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
        LogManager::getInstance().log("redis", LogLevel::DEBUG_LEVEL, "트랜잭션 실행 완료");
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
        LogManager::getInstance().log("redis", LogLevel::DEBUG_LEVEL, "트랜잭션 취소 완료");
    } else {
        logError("discard", "트랜잭션 취소 실패");
    }
    
    return success;
}

// =============================================================================
// 상태 및 진단 (최소 구현)
// =============================================================================

RedisClient::StringMap RedisClientImpl::info() {
    return StringMap{};
}

bool RedisClientImpl::ping() {
    auto reply = executeCommand("PING");
    return reply && replyToString(reply.get()) == "PONG";
}

bool RedisClientImpl::select(int db_index) {
    return selectDatabase(db_index);
}

int RedisClientImpl::dbsize() {
    auto reply = executeCommand("DBSIZE");
    return reply ? static_cast<int>(replyToInteger(reply.get())) : 0;
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

std::unique_ptr<redisReply, void(*)(redisReply*)> RedisClientImpl::executeCommand(const char* format, ...) {
    if (!context_) {
        return {nullptr, freeRedisReply};
    }
    
    va_list args;
    va_start(args, format);
    redisReply* reply = static_cast<redisReply*>(redisvCommand(context_.get(), format, args));
    va_end(args);
    
    return {reply, freeRedisReply};
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

bool RedisClientImpl::isReplyOK(redisReply* reply) {
    return reply && reply->type == REDIS_REPLY_STATUS && 
           strcmp(reply->str, "OK") == 0;
}

bool RedisClientImpl::isReplyError(redisReply* reply) {
    return reply && reply->type == REDIS_REPLY_ERROR;
}

void RedisClientImpl::logError(const std::string& operation, const std::string& error_message) const {
    LogManager::getInstance().log("redis", LogLevel::ERROR, 
        "Redis " + operation + " 에러: " + error_message);
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

void RedisClientImpl::resetStats() {
    total_commands_ = 0;
    successful_commands_ = 0;
    failed_commands_ = 0;
    
    connect_time_ = std::chrono::steady_clock::now();
    
    LogManager::getInstance().log("redis", LogLevel::DEBUG_LEVEL, "Redis 통계 리셋");
}

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

// =============================================================================
// 메모리 관리 및 정리 함수들
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
// 스레드 관련 빈 구현들 (향후 구현)
// =============================================================================

void RedisClientImpl::startPubSubThread() {
    // 향후 구현
}

void RedisClientImpl::stopPubSubThread() {
    // 향후 구현
}

void RedisClientImpl::pubsubThreadWorker() {
    // 향후 구현
}

void RedisClientImpl::handlePubSubMessage(redisReply* /* reply */) {
    // 향후 구현
}

void RedisClientImpl::startAsyncThread() {
    // 향후 구현
}

void RedisClientImpl::stopAsyncThread() {
    // 향후 구현
}

void RedisClientImpl::asyncThreadWorker() {
    // 향후 구현
}

// =============================================================================
// 추가 헬퍼 메서드들의 빈 구현
// =============================================================================

std::unique_ptr<redisReply, void(*)(redisReply*)> RedisClientImpl::executeCommandArgv(int argc, const char** argv, const size_t* argvlen) {
    if (!context_) {
        return {nullptr, freeRedisReply};
    }
    return {static_cast<redisReply*>(redisCommandArgv(context_.get(), argc, argv, argvlen)), freeRedisReply};
}

bool RedisClientImpl::reconnect() {
    return connectInternal();
}

bool RedisClientImpl::handleConnectionError() {
    return false;
}

bool RedisClientImpl::isConnectionError(redisContext* ctx) const {
    return ctx && ctx->err;
}

void RedisClientImpl::logRedisError(const std::string& /* operation */, redisContext* /* ctx */) {
    // 향후 구현
}

RedisClient::StringList RedisClientImpl::replyToStringList(redisReply* /* reply */) const {
    return StringList{};
}

RedisClient::StringMap RedisClientImpl::replyToStringMap(redisReply* /* reply */) const {
    return StringMap{};
}

std::string RedisClientImpl::escapeString(const std::string& str) {
    return str;
}

std::vector<std::string> RedisClientImpl::splitCommand(const std::string& command) {
    return {command};
}

std::chrono::milliseconds RedisClientImpl::calculateLatency() const {
    return std::chrono::milliseconds(0);
}

// =============================================================================
// 추가 메서드들의 빈 구현 (🔧 수정: 타입 수정)
// =============================================================================

bool RedisClientImpl::hmset(const std::string& /* key */, const StringMap& /* field_values */) {
    return false;
}

RedisClient::StringList RedisClientImpl::zrangebyscore(const std::string& /* key */, double /* min_score */, double /* max_score */) {
    return StringList{};
}

int RedisClientImpl::zremrangebyscore(const std::string& /* key */, double /* min_score */, double /* max_score */) {
    return 0;
}

std::string RedisClientImpl::evalScript(const std::string& /* script */, const StringList& /* keys */, const StringList& /* args */) {
    return "";
}

std::string RedisClientImpl::evalSha(const std::string& /* sha1 */, const StringList& /* keys */, const StringList& /* args */) {
    return "";
}

std::string RedisClientImpl::scriptLoad(const std::string& /* script */) {
    return "";
}

std::pair<std::string, RedisClient::StringList> RedisClientImpl::scan(const std::string& /* cursor */, const std::string& /* pattern */, int /* count */) {
    return {"0", StringList{}};
}

std::pair<std::string, RedisClient::StringMap> RedisClientImpl::hscan(const std::string& /* key */, const std::string& /* cursor */, const std::string& /* pattern */, int /* count */) {
    return {"0", StringMap{}};
}

std::pair<std::string, std::map<std::string, double>> RedisClientImpl::zscan(const std::string& /* key */, const std::string& /* cursor */, const std::string& /* pattern */, int /* count */) {
    return {"0", std::map<std::string, double>{}};
}

void RedisClientImpl::setTimeouts(int /* connect_timeout_ms */, int /* command_timeout_ms */) {
    // 향후 구현
}

bool RedisClientImpl::startPipeline() {
    return false;
}

bool RedisClientImpl::addToPipeline(const std::string& /* command */) {
    return false;
}

std::vector<std::unique_ptr<redisReply, void(*)(redisReply*)>> RedisClientImpl::executePipeline() {
    return {};
}

// 🔧 수정: del 다중 키 버전 추가
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
    
    auto reply = executeCommandArgv(static_cast<int>(argv.size()), argv.data(), argvlen.data());
    
    recordCommandEnd(reply != nullptr);
    
    if (!reply) {
        logError("del", "다중 키 삭제 실패");
        return 0;
    }
    
    return static_cast<int>(replyToInteger(reply.get()));
}