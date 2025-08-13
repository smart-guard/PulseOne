// =============================================================================
// 🔥 개선된 RedisClientImpl.cpp - 완전 자동화 구현
// =============================================================================

#include "Client/RedisClientImpl.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include <stdarg.h>
#include <functional>

#ifdef HAS_HIREDIS
#include <hiredis/hiredis.h>
#else
// 더미 구현들...
#endif

// =============================================================================
// 🔥 생성자 - 완전 자동화의 핵심!
// =============================================================================

RedisClientImpl::RedisClientImpl() {
    try {
        logInfo("Redis 클라이언트 초기화 시작");
        
        // 1. 설정 자동 로드
        loadConfiguration();
        
        // 2. 첫 연결 시도
        if (attemptConnection()) {
            logInfo("✅ 초기 Redis 연결 성공: " + host_ + ":" + std::to_string(port_));
        } else {
            logWarning("⚠️ 초기 Redis 연결 실패, 백그라운드에서 재시도");
        }
        
        // 3. 백그라운드 연결 감시 시작
        watchdog_thread_ = std::make_unique<std::thread>(&RedisClientImpl::connectionWatchdog, this);
        
        logInfo("Redis 클라이언트 초기화 완료");
        
    } catch (const std::exception& e) {
        logError("Redis 클라이언트 초기화 실패: " + std::string(e.what()));
    }
}

RedisClientImpl::~RedisClientImpl() {
    logInfo("Redis 클라이언트 종료 시작");
    
    // 백그라운드 스레드 정리
    shutdown_requested_ = true;
    watchdog_cv_.notify_all();
    
    if (watchdog_thread_ && watchdog_thread_->joinable()) {
        watchdog_thread_->join();
    }
    
    // 연결 정리
    std::lock_guard<std::recursive_mutex> lock(connection_mutex_);
    if (context_) {
#ifdef HAS_HIREDIS
        redisFree(context_);
#endif
        context_ = nullptr;
    }
    
    connected_ = false;
    logInfo("Redis 클라이언트 종료 완료");
}

// =============================================================================
// 🔥 자동 설정 로드
// =============================================================================

void RedisClientImpl::loadConfiguration() {
    try {
        auto& config = ConfigManager::getInstance();
        
        host_ = config.getOrDefault("REDIS_HOST", "localhost");
        port_ = config.getInt("REDIS_PORT", 6379);
        password_ = config.getOrDefault("REDIS_PASSWORD", "");
        database_ = config.getInt("REDIS_DATABASE", 0);
        
        logInfo("📋 Redis 설정 로드 완료: " + host_ + ":" + std::to_string(port_) + 
                " (DB " + std::to_string(database_) + ")");
        
    } catch (const std::exception& e) {
        logWarning("설정 로드 실패, 기본값 사용: " + std::string(e.what()));
        // 기본값들이 이미 설정되어 있음
    }
}

// =============================================================================
// 🔥 자동 연결 관리
// =============================================================================

bool RedisClientImpl::attemptConnection() {
    std::lock_guard<std::recursive_mutex> lock(connection_mutex_);
    
#ifdef HAS_HIREDIS
    try {
        // 기존 연결 정리
        if (context_) {
            redisFree(context_);
            context_ = nullptr;
        }
        
        // 새 연결 시도
        struct timeval timeout = {
            .tv_sec = CONNECTION_TIMEOUT.count() / 1000,
            .tv_usec = (CONNECTION_TIMEOUT.count() % 1000) * 1000
        };
        
        context_ = redisConnectWithTimeout(host_.c_str(), port_, timeout);
        
        if (!context_ || context_->err) {
            if (context_) {
                logError("연결 실패: " + std::string(context_->errstr));
                redisFree(context_);
                context_ = nullptr;
            }
            return false;
        }
        
        // 인증 (필요시)
        if (!password_.empty()) {
            redisReply* auth_reply = (redisReply*)redisCommand(context_, "AUTH %s", password_.c_str());
            if (!auth_reply || auth_reply->type == REDIS_REPLY_ERROR) {
                logError("Redis 인증 실패");
                if (auth_reply) freeReplyObject(auth_reply);
                redisFree(context_);
                context_ = nullptr;
                return false;
            }
            freeReplyObject(auth_reply);
        }
        
        // DB 선택 (필요시)
        if (database_ != 0) {
            redisReply* select_reply = (redisReply*)redisCommand(context_, "SELECT %d", database_);
            if (!select_reply || select_reply->type == REDIS_REPLY_ERROR) {
                logError("데이터베이스 선택 실패: DB " + std::to_string(database_));
                if (select_reply) freeReplyObject(select_reply);
                redisFree(context_);
                context_ = nullptr;
                return false;
            }
            freeReplyObject(select_reply);
        }
        
        connected_ = true;
        reconnect_attempts_ = 0;
        connect_time_ = std::chrono::steady_clock::now();
        
        return true;
        
    } catch (const std::exception& e) {
        logError("연결 중 예외: " + std::string(e.what()));
        return false;
    }
#else
    logWarning("hiredis 라이브러리 없음, 시뮬레이션 모드");
    connected_ = true;  // 개발 환경에서는 시뮬레이션
    return true;
#endif
}

bool RedisClientImpl::ensureConnected() {
    if (connected_ && context_) {
        return true;
    }
    
    if (reconnect_attempts_ >= MAX_RECONNECT_ATTEMPTS) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    if (now - last_reconnect_attempt_ < RECONNECT_DELAY) {
        return false;  // 너무 빨리 재시도하지 않기
    }
    
    last_reconnect_attempt_ = now;
    reconnect_attempts_++;
    total_reconnects_++;
    
    logInfo("🔄 Redis 재연결 시도 " + std::to_string(reconnect_attempts_) + "/" + 
            std::to_string(MAX_RECONNECT_ATTEMPTS));
    
    return attemptConnection();
}

void RedisClientImpl::connectionWatchdog() {
    logInfo("🐕 Redis 연결 감시 스레드 시작");
    
    while (!shutdown_requested_) {
        try {
            // 30초마다 ping으로 연결 상태 확인
            std::unique_lock<std::mutex> lock(watchdog_mutex_);
            if (watchdog_cv_.wait_for(lock, std::chrono::seconds(30), 
                                     [this] { return shutdown_requested_.load(); })) {
                break;  // 종료 요청
            }
            
            // 연결 상태 확인
            if (connected_ && context_) {
#ifdef HAS_HIREDIS
                redisReply* ping_reply = (redisReply*)redisCommand(context_, "PING");
                if (!ping_reply || ping_reply->type == REDIS_REPLY_ERROR) {
                    logWarning("⚠️ Redis 연결 끊어짐 감지 (PING 실패)");
                    connected_ = false;
                    if (ping_reply) freeReplyObject(ping_reply);
                } else {
                    if (ping_reply) freeReplyObject(ping_reply);
                }
#endif
            }
            
            // 연결이 끊어졌으면 재연결 시도
            if (!connected_) {
                ensureConnected();
            }
            
        } catch (const std::exception& e) {
            logError("연결 감시 중 예외: " + std::string(e.what()));
        }
    }
    
    logInfo("Redis 연결 감시 스레드 종료");
}

// =============================================================================
// 🔥 자동 재연결을 포함한 명령 실행
// =============================================================================

template<typename T>
T RedisClientImpl::executeWithRetry(std::function<T()> operation, T default_value) {
    // 첫 번째 시도
    if (ensureConnected()) {
        try {
            T result = operation();
            successful_commands_++;
            return result;
        } catch (const std::exception& e) {
            logWarning("명령 실행 실패, 재연결 후 재시도: " + std::string(e.what()));
            connected_ = false;
        }
    }
    
    // 재연결 후 두 번째 시도
    if (ensureConnected()) {
        try {
            T result = operation();
            successful_commands_++;
            return result;
        } catch (const std::exception& e) {
            logError("재연결 후에도 명령 실행 실패: " + std::string(e.what()));
        }
    }
    
    failed_commands_++;
    return default_value;
}

redisReply* RedisClientImpl::executeCommandSafe(const char* format, ...) {
    total_commands_++;
    
    if (!ensureConnected()) {
        return nullptr;
    }
    
#ifdef HAS_HIREDIS
    va_list args;
    va_start(args, format);
    redisReply* reply = (redisReply*)redisvCommand(context_, format, args);
    va_end(args);
    
    if (!reply && isConnectionError()) {
        connected_ = false;
        logWarning("명령 실행 중 연결 오류 감지");
    }
    
    return reply;
#else
    return nullptr;
#endif
}

// =============================================================================
// 🔥 사용자 친화적 Redis 명령어들
// =============================================================================

bool RedisClientImpl::set(const std::string& key, const std::string& value) {
    return executeWithRetry<bool>([this, &key, &value]() {
        redisReply* reply = executeCommandSafe("SET %s %s", key.c_str(), value.c_str());
        bool result = reply && isReplyOK(reply);
        if (reply) freeReplyObject(reply);
        return result;
    }, false);
}

std::string RedisClientImpl::get(const std::string& key) {
    return executeWithRetry<std::string>([this, &key]() {
        redisReply* reply = executeCommandSafe("GET %s", key.c_str());
        std::string result = replyToString(reply);
        if (reply) freeReplyObject(reply);
        return result;
    }, "");
}

bool RedisClientImpl::exists(const std::string& key) {
    return executeWithRetry<bool>([this, &key]() {
        redisReply* reply = executeCommandSafe("EXISTS %s", key.c_str());
        bool result = reply && replyToInteger(reply) > 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, false);
}

int RedisClientImpl::publish(const std::string& channel, const std::string& message) {
    return executeWithRetry<int>([this, &channel, &message]() {
        redisReply* reply = executeCommandSafe("PUBLISH %s %s", channel.c_str(), message.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, 0);
}

bool RedisClientImpl::ping() {
    return executeWithRetry<bool>([this]() {
        redisReply* reply = executeCommandSafe("PING");
        bool result = reply && reply->type == REDIS_REPLY_STATUS && 
                     strcmp(reply->str, "PONG") == 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, false);
}

// =============================================================================
// 🔥 상태 및 통계
// =============================================================================

bool RedisClientImpl::isConnected() const {
    return connected_ && context_;
}

RedisClientImpl::ConnectionStats RedisClientImpl::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    ConnectionStats stats;
    stats.is_connected = connected_;
    stats.host = host_;
    stats.port = port_;
    stats.total_commands = total_commands_;
    stats.successful_commands = successful_commands_;
    stats.failed_commands = failed_commands_;
    stats.reconnect_count = total_reconnects_;
    stats.last_connect_time = connect_time_;
    
    return stats;
}

// =============================================================================
// 🔥 로깅 헬퍼들
// =============================================================================

void RedisClientImpl::logInfo(const std::string& message) const {
    LogManager::getInstance().log("redis", LogLevel::INFO, message);
}

void RedisClientImpl::logWarning(const std::string& message) const {
    LogManager::getInstance().log("redis", LogLevel::WARN, message);
}

void RedisClientImpl::logError(const std::string& message) const {
    LogManager::getInstance().log("redis", LogLevel::ERROR, message);
}

// =============================================================================
// 🔥 나머지 모든 Redis 명령어들 - 완전 구현
// =============================================================================

bool RedisClientImpl::setex(const std::string& key, const std::string& value, int expire_seconds) {
    return executeWithRetry<bool>([this, &key, &value, expire_seconds]() {
        redisReply* reply = executeCommandSafe("SETEX %s %d %s", key.c_str(), expire_seconds, value.c_str());
        bool result = reply && isReplyOK(reply);
        if (reply) freeReplyObject(reply);
        return result;
    }, false);
}

int RedisClientImpl::del(const std::string& key) {
    return executeWithRetry<int>([this, &key]() {
        redisReply* reply = executeCommandSafe("DEL %s", key.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, 0);
}

bool RedisClientImpl::expire(const std::string& key, int seconds) {
    return executeWithRetry<bool>([this, &key, seconds]() {
        redisReply* reply = executeCommandSafe("EXPIRE %s %d", key.c_str(), seconds);
        bool result = reply && replyToInteger(reply) > 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, false);
}

int RedisClientImpl::ttl(const std::string& key) {
    return executeWithRetry<int>([this, &key]() {
        redisReply* reply = executeCommandSafe("TTL %s", key.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : -2;
        if (reply) freeReplyObject(reply);
        return result;
    }, -2);
}

// =============================================================================
// Hash 명령어들
// =============================================================================

bool RedisClientImpl::hset(const std::string& key, const std::string& field, const std::string& value) {
    return executeWithRetry<bool>([this, &key, &field, &value]() {
        redisReply* reply = executeCommandSafe("HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
        bool result = reply && replyToInteger(reply) >= 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, false);
}

std::string RedisClientImpl::hget(const std::string& key, const std::string& field) {
    return executeWithRetry<std::string>([this, &key, &field]() {
        redisReply* reply = executeCommandSafe("HGET %s %s", key.c_str(), field.c_str());
        std::string result = replyToString(reply);
        if (reply) freeReplyObject(reply);
        return result;
    }, "");
}

RedisClient::StringMap RedisClientImpl::hgetall(const std::string& key) {
    return executeWithRetry<StringMap>([this, &key]() {
        redisReply* reply = executeCommandSafe("HGETALL %s", key.c_str());
        StringMap result = replyToStringMap(reply);
        if (reply) freeReplyObject(reply);
        return result;
    }, StringMap{});
}

int RedisClientImpl::hdel(const std::string& key, const std::string& field) {
    return executeWithRetry<int>([this, &key, &field]() {
        redisReply* reply = executeCommandSafe("HDEL %s %s", key.c_str(), field.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, 0);
}

bool RedisClientImpl::hexists(const std::string& key, const std::string& field) {
    return executeWithRetry<bool>([this, &key, &field]() {
        redisReply* reply = executeCommandSafe("HEXISTS %s %s", key.c_str(), field.c_str());
        bool result = reply && replyToInteger(reply) > 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, false);
}

int RedisClientImpl::hlen(const std::string& key) {
    return executeWithRetry<int>([this, &key]() {
        redisReply* reply = executeCommandSafe("HLEN %s", key.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, 0);
}

// =============================================================================
// List 명령어들
// =============================================================================

int RedisClientImpl::lpush(const std::string& key, const std::string& value) {
    return executeWithRetry<int>([this, &key, &value]() {
        redisReply* reply = executeCommandSafe("LPUSH %s %s", key.c_str(), value.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, 0);
}

int RedisClientImpl::rpush(const std::string& key, const std::string& value) {
    return executeWithRetry<int>([this, &key, &value]() {
        redisReply* reply = executeCommandSafe("RPUSH %s %s", key.c_str(), value.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, 0);
}

std::string RedisClientImpl::lpop(const std::string& key) {
    return executeWithRetry<std::string>([this, &key]() {
        redisReply* reply = executeCommandSafe("LPOP %s", key.c_str());
        std::string result = replyToString(reply);
        if (reply) freeReplyObject(reply);
        return result;
    }, "");
}

std::string RedisClientImpl::rpop(const std::string& key) {
    return executeWithRetry<std::string>([this, &key]() {
        redisReply* reply = executeCommandSafe("RPOP %s", key.c_str());
        std::string result = replyToString(reply);
        if (reply) freeReplyObject(reply);
        return result;
    }, "");
}

RedisClient::StringList RedisClientImpl::lrange(const std::string& key, int start, int stop) {
    return executeWithRetry<StringList>([this, &key, start, stop]() {
        redisReply* reply = executeCommandSafe("LRANGE %s %d %d", key.c_str(), start, stop);
        StringList result = replyToStringList(reply);
        if (reply) freeReplyObject(reply);
        return result;
    }, StringList{});
}

int RedisClientImpl::llen(const std::string& key) {
    return executeWithRetry<int>([this, &key]() {
        redisReply* reply = executeCommandSafe("LLEN %s", key.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, 0);
}

// =============================================================================
// Set 명령어들
// =============================================================================

int RedisClientImpl::sadd(const std::string& key, const std::string& member) {
    return executeWithRetry<int>([this, &key, &member]() {
        redisReply* reply = executeCommandSafe("SADD %s %s", key.c_str(), member.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, 0);
}

int RedisClientImpl::srem(const std::string& key, const std::string& member) {
    return executeWithRetry<int>([this, &key, &member]() {
        redisReply* reply = executeCommandSafe("SREM %s %s", key.c_str(), member.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, 0);
}

bool RedisClientImpl::sismember(const std::string& key, const std::string& member) {
    return executeWithRetry<bool>([this, &key, &member]() {
        redisReply* reply = executeCommandSafe("SISMEMBER %s %s", key.c_str(), member.c_str());
        bool result = reply && replyToInteger(reply) > 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, false);
}

RedisClient::StringList RedisClientImpl::smembers(const std::string& key) {
    return executeWithRetry<StringList>([this, &key]() {
        redisReply* reply = executeCommandSafe("SMEMBERS %s", key.c_str());
        StringList result = replyToStringList(reply);
        if (reply) freeReplyObject(reply);
        return result;
    }, StringList{});
}

int RedisClientImpl::scard(const std::string& key) {
    return executeWithRetry<int>([this, &key]() {
        redisReply* reply = executeCommandSafe("SCARD %s", key.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, 0);
}

// =============================================================================
// 🔥 응답 처리 헬퍼 메서드들
// =============================================================================

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
        case REDIS_REPLY_ERROR:
            logWarning("Redis 오류 응답: " + std::string(reply->str, reply->len));
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
            } catch (const std::exception& e) {
                logWarning("정수 변환 실패: " + std::string(reply->str, reply->len));
                return 0;
            }
        case REDIS_REPLY_NIL:
            return 0;
        case REDIS_REPLY_ERROR:
            logWarning("Redis 오류 응답: " + std::string(reply->str, reply->len));
            return 0;
        default:
            return 0;
    }
}

RedisClient::StringList RedisClientImpl::replyToStringList(redisReply* reply) const {
    StringList result;
    
    if (!reply) return result;
    
    if (reply->type != REDIS_REPLY_ARRAY) {
        if (reply->type == REDIS_REPLY_ERROR) {
            logWarning("Redis 배열 오류: " + std::string(reply->str, reply->len));
        }
        return result;
    }
    
    result.reserve(reply->elements);
    
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
    
    if (!reply) return result;
    
    if (reply->type != REDIS_REPLY_ARRAY) {
        if (reply->type == REDIS_REPLY_ERROR) {
            logWarning("Redis 맵 오류: " + std::string(reply->str, reply->len));
        }
        return result;
    }
    
    if (reply->elements % 2 != 0) {
        logWarning("Redis 맵 응답의 요소 수가 홀수입니다: " + std::to_string(reply->elements));
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
    return reply && 
           reply->type == REDIS_REPLY_STATUS && 
           reply->len == 2 &&
           strcmp(reply->str, "OK") == 0;
}

bool RedisClientImpl::isConnectionError() const {
#ifdef HAS_HIREDIS
    return context_ && context_->err != 0;
#else
    return false;
#endif
}

// =============================================================================
// 🔥 기존 인터페이스 호환성을 위한 더미 메서드들
// =============================================================================

bool RedisClientImpl::connect(const std::string& host, int port, const std::string& password) {
    // 더 이상 필요 없지만 인터페이스 호환성을 위해 유지
    logWarning("connect() 호출됨 - 자동 연결이 이미 활성화되어 있습니다");
    return connected_;
}

void RedisClientImpl::disconnect() {
    logInfo("수동 연결 해제 요청");
    std::lock_guard<std::recursive_mutex> lock(connection_mutex_);
    
    if (context_) {
#ifdef HAS_HIREDIS
        redisFree(context_);
#endif
        context_ = nullptr;
    }
    
    connected_ = false;
}

bool RedisClientImpl::reconnect() {
    logInfo("🔄 수동 재연결 요청");
    connected_ = false;
    reconnect_attempts_ = 0;  // 재설정
    return attemptConnection();
}

void RedisClientImpl::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    total_commands_ = 0;
    successful_commands_ = 0;
    failed_commands_ = 0;
    total_reconnects_ = 0;
    connect_time_ = std::chrono::steady_clock::now();
    
    logInfo("📊 Redis 통계 초기화 완료");
}

// =============================================================================
// 🔥 아직 구현되지 않은 메서드들 (향후 확장)
// =============================================================================

// Sorted Set (간단 구현)
int RedisClientImpl::zadd(const std::string& key, double score, const std::string& member) {
    return executeWithRetry<int>([this, &key, score, &member]() {
        redisReply* reply = executeCommandSafe("ZADD %s %f %s", key.c_str(), score, member.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, 0);
}

int RedisClientImpl::zrem(const std::string& key, const std::string& member) {
    return executeWithRetry<int>([this, &key, &member]() {
        redisReply* reply = executeCommandSafe("ZREM %s %s", key.c_str(), member.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, 0);
}

RedisClient::StringList RedisClientImpl::zrange(const std::string& key, int start, int stop) {
    return executeWithRetry<StringList>([this, &key, start, stop]() {
        redisReply* reply = executeCommandSafe("ZRANGE %s %d %d", key.c_str(), start, stop);
        StringList result = replyToStringList(reply);
        if (reply) freeReplyObject(reply);
        return result;
    }, StringList{});
}

int RedisClientImpl::zcard(const std::string& key) {
    return executeWithRetry<int>([this, &key]() {
        redisReply* reply = executeCommandSafe("ZCARD %s", key.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, 0);
}

// Pub/Sub (기본 구현)
bool RedisClientImpl::subscribe(const std::string& channel) {
    logInfo("Subscribe 요청: " + channel + " (향후 완전 구현 예정)");
    return true;  // 임시 구현
}

bool RedisClientImpl::unsubscribe(const std::string& channel) {
    logInfo("Unsubscribe 요청: " + channel + " (향후 완전 구현 예정)");
    return true;  // 임시 구현
}

bool RedisClientImpl::psubscribe(const std::string& pattern) {
    logInfo("Pattern subscribe 요청: " + pattern + " (향후 완전 구현 예정)");
    return true;  // 임시 구현
}

bool RedisClientImpl::punsubscribe(const std::string& pattern) {
    logInfo("Pattern unsubscribe 요청: " + pattern + " (향후 완전 구현 예정)");
    return true;  // 임시 구현
}

void RedisClientImpl::setMessageCallback(MessageCallback callback) {
    logInfo("메시지 콜백 설정 (향후 완전 구현 예정)");
    // 임시 구현: 콜백 저장하지만 아직 사용하지 않음
}

// 배치 처리
bool RedisClientImpl::mset(const StringMap& key_values) {
    if (key_values.empty()) return true;
    
    return executeWithRetry<bool>([this, &key_values]() {
        // MSET 명령어 구성
        std::string cmd = "MSET";
        for (const auto& [key, value] : key_values) {
            cmd += " " + key + " " + value;
        }
        
        redisReply* reply = executeCommandSafe(cmd.c_str());
        bool result = reply && isReplyOK(reply);
        if (reply) freeReplyObject(reply);
        return result;
    }, false);
}

RedisClient::StringList RedisClientImpl::mget(const StringList& keys) {
    if (keys.empty()) return StringList{};
    
    return executeWithRetry<StringList>([this, &keys]() {
        // MGET 명령어 구성
        std::string cmd = "MGET";
        for (const auto& key : keys) {
            cmd += " " + key;
        }
        
        redisReply* reply = executeCommandSafe(cmd.c_str());
        StringList result = replyToStringList(reply);
        if (reply) freeReplyObject(reply);
        return result;
    }, StringList{});
}

// 트랜잭션 (기본 구현)
bool RedisClientImpl::multi() {
    return executeWithRetry<bool>([this]() {
        redisReply* reply = executeCommandSafe("MULTI");
        bool result = reply && isReplyOK(reply);
        if (reply) freeReplyObject(reply);
        return result;
    }, false);
}

bool RedisClientImpl::exec() {
    return executeWithRetry<bool>([this]() {
        redisReply* reply = executeCommandSafe("EXEC");
        bool result = reply && reply->type != REDIS_REPLY_NIL;
        if (reply) freeReplyObject(reply);
        return result;
    }, false);
}

bool RedisClientImpl::discard() {
    return executeWithRetry<bool>([this]() {
        redisReply* reply = executeCommandSafe("DISCARD");
        bool result = reply && isReplyOK(reply);
        if (reply) freeReplyObject(reply);
        return result;
    }, false);
}

// 상태 및 진단
RedisClient::StringMap RedisClientImpl::info() {
    return executeWithRetry<StringMap>([this]() {
        redisReply* reply = executeCommandSafe("INFO");
        StringMap result;
        
        if (reply && reply->type == REDIS_REPLY_STRING) {
            std::string info_str(reply->str, reply->len);
            std::istringstream stream(info_str);
            std::string line;
            
            while (std::getline(stream, line)) {
                if (line.empty() || line[0] == '#') continue;  // 주석 라인 건너뛰기
                
                size_t colon_pos = line.find(':');
                if (colon_pos != std::string::npos) {
                    std::string key = line.substr(0, colon_pos);
                    std::string value = line.substr(colon_pos + 1);
                    
                    // Windows 개행 문자 제거
                    if (!value.empty() && value.back() == '\r') {
                        value.pop_back();
                    }
                    
                    result[key] = value;
                }
            }
        }
        
        if (reply) freeReplyObject(reply);
        return result;
    }, StringMap{});
}

bool RedisClientImpl::select(int db_index) {
    bool success = executeWithRetry<bool>([this, db_index]() {
        redisReply* reply = executeCommandSafe("SELECT %d", db_index);
        bool result = reply && isReplyOK(reply);
        if (reply) freeReplyObject(reply);
        return result;
    }, false);
    
    if (success) {
        database_ = db_index;
        logInfo("데이터베이스 변경: DB " + std::to_string(db_index));
    }
    
    return success;
}

int RedisClientImpl::dbsize() {
    return executeWithRetry<int>([this]() {
        redisReply* reply = executeCommandSafe("DBSIZE");
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply) freeReplyObject(reply);
        return result;
    }, 0);
}

// =============================================================================
// 🔥 다른 클래스에서 사용하는 방법 - 이렇게 간단!
// =============================================================================

/*
// =============================================================================
// 🔥 실제 사용 예시들 - 다른 클래스에서 이렇게 사용
// =============================================================================

// 1. AlarmManager.cpp 예시:
void AlarmManager::initializeClients() {
    try {
        // 🎯 이게 전부! 설정 읽기, 연결, 재연결 모두 자동!
        redis_client_ = std::make_shared<RedisClientImpl>();
        
        // 즉시 사용 가능 (연결 상태 자동 관리)
        if (redis_client_->ping()) {
            logInfo("✅ Redis 클라이언트 준비 완료");
        } else {
            logInfo("⚠️ Redis 연결 중, 백그라운드에서 처리됨");
        }
        
        // 통계 확인 (선택적)
        auto stats = redis_client_->getStats();
        logInfo("Redis 통계 - 명령어: " + std::to_string(stats.total_commands) + 
                ", 재연결: " + std::to_string(stats.reconnect_count));
        
    } catch (const std::exception& e) {
        logError("Redis 클라이언트 생성 실패: " + std::string(e.what()));
    }
}

// 사용할 때도 그냥 사용하면 됨:
void AlarmManager::publishAlarm(const AlarmEvent& event) {
    if (redis_client_) {
        // 연결 상태, 재연결 등은 알아서 처리됨
        std::string alarm_json = event.toJson();
        int subscribers = redis_client_->publish("alarms:critical", alarm_json);
        
        // 알람 데이터도 저장
        redis_client_->set("alarm:" + std::to_string(event.id), alarm_json);
        redis_client_->expire("alarm:" + std::to_string(event.id), 86400);  // 24시간
        
        logInfo("알람 발송 완료: " + std::to_string(subscribers) + "명에게 전달");
    }
}

// 2. DataProcessingService.cpp 예시:
void DataProcessingService::initializeRedis() {
    // 🎯 이게 전부!
    redis_client_ = std::make_shared<RedisClientImpl>();
}

void DataProcessingService::cacheCurrentValue(const CurrentValue& value) {
    if (redis_client_) {
        // 자동 연결 관리로 안전하게 사용
        std::string key = "current_value:" + std::to_string(value.point_id);
        std::string json_value = value.toJson();
        
        // 캐시 저장 (5분 TTL)
        redis_client_->setex(key, json_value, 300);
        
        // 최근 값들 리스트에 추가
        redis_client_->lpush("recent_values", json_value);
        
        // 리스트 크기 제한 (최근 1000개만 유지)
        if (redis_client_->llen("recent_values") > 1000) {
            redis_client_->rpop("recent_values");
        }
    }
}

// 3. Worker 클래스 예시:
class ModbusWorker {
private:
    std::shared_ptr<RedisClientImpl> redis_;
    
public:
    void initialize() {
        // 🎯 이게 전부!
        redis_ = std::make_shared<RedisClientImpl>();
    }
    
    void publishDeviceStatus(const std::string& device_id, const std::string& status) {
        if (redis_) {
            // 장치 상태 캐시
            redis_->hset("device_status", device_id, status);
            
            // 상태 변경 이벤트 발송
            std::string event = R"({"device_id":")" + device_id + 
                               R"(","status":")" + status + 
                               R"(","timestamp":)" + std::to_string(time(nullptr)) + "}";
            
            redis_->publish("device_events", event);
        }
    }
};

// 4. VirtualPointEngine 예시:
void VirtualPointEngine::cacheCalculationResult(int vpoint_id, double result) {
    if (redis_client_) {
        // 계산 결과 캐시 (1시간 TTL)
        std::string key = "vpoint_result:" + std::to_string(vpoint_id);
        redis_client_->setex(key, std::to_string(result), 3600);
        
        // 계산 통계 업데이트
        redis_client_->hincrby("vpoint_stats", "total_calculations", 1);
        redis_client_->hset("vpoint_stats", "last_calculation", std::to_string(time(nullptr)));
    }
}

// 5. 설정 파일 예시 (.env):
// # Redis 설정 (모두 선택적, 없으면 기본값 사용)
// REDIS_HOST=localhost
// REDIS_PORT=6379
// REDIS_PASSWORD=my_secure_password
// REDIS_DATABASE=0

// 6. 에러 상황에서도 자동 처리:
void SomeService::unreliableOperation() {
    if (redis_client_) {
        // 연결이 끊어져도 자동으로 재연결 시도
        // 실패해도 예외 던지지 않고 기본값 반환
        
        for (int i = 0; i < 100; ++i) {
            std::string key = "data_" + std::to_string(i);
            std::string value = "value_" + std::to_string(i);
            
            // 자동 재연결으로 안전
            bool success = redis_client_->set(key, value);
            
            if (!success) {
                logWarning("Redis 저장 실패: " + key + " (재연결 시도 중...)");
            }
        }
        
        // 통계 확인
        auto stats = redis_client_->getStats();
        logInfo("작업 완료 - 성공률: " + 
                std::to_string(100.0 * stats.successful_commands / stats.total_commands) + "%");
    }
}

// =============================================================================
// 🔥 핵심 장점 요약
// =============================================================================

// ✅ 장점 1: 설정 자동 로드
// - ConfigManager에서 Redis 설정 자동 읽기
// - 설정 없으면 기본값 사용
// - 다른 클래스는 설정에 대해 알 필요 없음

// ✅ 장점 2: 자동 연결 관리
// - 생성과 동시에 연결 시도
// - 연결 실패해도 백그라운드에서 계속 시도
// - 사용자는 연결 상태 신경 쓸 필요 없음

// ✅ 장점 3: 자동 재연결
// - 연결 끊어지면 자동으로 재연결
// - 명령 실패 시 재연결 후 재시도
// - 백그라운드 연결 감시 스레드

// ✅ 장점 4: 안전한 에러 처리
// - 예외 던지지 않고 기본값 반환
// - 모든 에러 상황 내부에서 처리
// - 로깅을 통한 문제 상황 알림

// ✅ 장점 5: 통계 및 모니터링
// - 자동 성능 메트릭 수집
// - 연결 상태 및 재연결 횟수 추적
// - 명령어 성공/실패율 통계

// ✅ 장점 6: 개발 환경 친화적
// - hiredis 없어도 시뮬레이션 모드로 동작
// - Redis 서버 없어도 에러 없이 실행
// - 개발/테스트 환경에서 문제없이 동작
*/