// collector/src/Client/RedisClientImpl.cpp
#include "Client/RedisClientImpl.h"
#include "Common/Enums.h"
#include "Utils/LogManager.h"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <ctime>

// =============================================================================
// 생성자와 소멸자
// =============================================================================

RedisClientImpl::RedisClientImpl() 
    : context_(nullptr, [](redisContext* ctx) {
          // 실제 hiredis에서는: redisFree(ctx);
          if (ctx) delete ctx;
      })
{
    LogManager::getInstance().log("redis", PulseOne::Enums::LogLevel::DEBUG, "RedisClientImpl 생성됨");
}

RedisClientImpl::~RedisClientImpl() {
    disconnect();
    LogManager::getInstance().log("redis", PulseOne::Enums::LogLevel::DEBUG, "RedisClientImpl 소멸됨");
}

// =============================================================================
// 기본 연결 관리
// =============================================================================

bool RedisClientImpl::connect(const std::string& host, int port, const std::string& password) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    if (connected_) {
        LogManager::getInstance().log("redis", LogLevel::WARN, "이미 연결되어 있음");
        return true;
    }
    
    host_ = host;
    port_ = port;
    password_ = password;
    
    try {
        // 🔥 실제 프로덕션에서는:
        // context_.reset(redisConnect(host.c_str(), port));
        // if (!context_ || context_->err) return false;
        
        // 시뮬레이션 모드
        context_.reset(new redisContext{0, 0, ""});
        
        connected_ = true;
        connect_time_ = std::chrono::steady_clock::now();
        reconnect_attempts_ = 0;
        
        // 비밀번호 인증
        if (!password_.empty()) {
            std::string auth_cmd = "AUTH " + password_;
            if (!executeCommand(auth_cmd)) {
                disconnect();
                return false;
            }
        }
        
        // 데이터베이스 선택
        if (selected_db_ != 0) {
            if (!select(selected_db_)) {
                disconnect();
                return false;
            }
        }
        
        LogManager::getInstance().log("redis", LogLevel::INFO, 
            "✅ Redis 연결 성공: " + host + ":" + std::to_string(port));
        
        return true;
        
    } catch (const std::exception& e) {
        logError("connect", e.what());
        return false;
    }
}

void RedisClientImpl::disconnect() {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    if (!connected_) return;
    
    // Pub/Sub 스레드 종료
    stopPubSubThread();
    
    // 트랜잭션 상태 리셋
    in_transaction_ = false;
    
    // 연결 해제
    context_.reset();
    connected_ = false;
    
    // 메모리 저장소 정리
    memory_storage_.clear();
    hash_storage_.clear();
    list_storage_.clear();
    
    LogManager::getInstance().log("redis", LogLevel::INFO, 
        "Redis 연결 해제: " + host_ + ":" + std::to_string(port_));
}

bool RedisClientImpl::isConnected() const {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    return connected_ && context_ != nullptr;
}

// =============================================================================
// 기본 Key-Value 조작
// =============================================================================

bool RedisClientImpl::set(const std::string& key, const std::string& value) {
    if (!ensureConnection()) return false;
    
    std::lock_guard<std::mutex> lock(operation_mutex_);
    total_commands_++;
    
    try {
        if (isMemoryMode()) {
            memory_storage_[key] = value;
            LogManager::getInstance().log("redis", PulseOne::Enums::LogLevel::DEBUG, 
                "[MEMORY] SET " + key + " = " + value);
            return true;
        }
        
        // 실제 Redis 명령어
        std::string cmd = "SET " + key + " " + value;
        bool result = executeCommand(cmd);
        
        if (!result) failed_commands_++;
        return result;
        
    } catch (const std::exception& e) {
        failed_commands_++;
        logError("set", e.what());
        return false;
    }
}

std::string RedisClientImpl::get(const std::string& key) {
    if (!ensureConnection()) return "";
    
    std::lock_guard<std::mutex> lock(operation_mutex_);
    total_commands_++;
    
    try {
        if (isMemoryMode()) {
            auto it = memory_storage_.find(key);
            if (it != memory_storage_.end()) {
                LogManager::getInstance().log("redis", PulseOne::Enums::LogLevel::DEBUG, 
                    "[MEMORY] GET " + key + " = " + it->second);
                return it->second;
            }
            return "";
        }
        
        // 실제 Redis 명령어  
        std::string cmd = "GET " + key;
        auto reply = executeCommandWithReply(cmd);
        
        if (reply && reply->type == 1 && reply->str) { // REDIS_REPLY_STRING
            return std::string(reply->str, reply->len);
        }
        
        return "";
        
    } catch (const std::exception& e) {
        failed_commands_++;
        logError("get", e.what());
        return "";
    }
}

bool RedisClientImpl::setex(const std::string& key, const std::string& value, int ttl_seconds) {
    if (!ensureConnection()) return false;
    
    std::lock_guard<std::mutex> lock(operation_mutex_);
    total_commands_++;
    
    try {
        if (isMemoryMode()) {
            // 메모리 모드에서는 TTL 시뮬레이션하지 않음
            memory_storage_[key] = value;
            LogManager::getInstance().log("redis", LogLevel::DEBUG, 
                "[MEMORY] SETEX " + key + " " + std::to_string(ttl_seconds) + " " + value);
            return true;
        }
        
        std::string cmd = "SETEX " + key + " " + std::to_string(ttl_seconds) + " " + value;
        bool result = executeCommand(cmd);
        
        if (!result) failed_commands_++;
        return result;
        
    } catch (const std::exception& e) {
        failed_commands_++;
        logError("setex", e.what());
        return false;
    }
}

bool RedisClientImpl::exists(const std::string& key) {
    if (!ensureConnection()) return false;
    
    std::lock_guard<std::mutex> lock(operation_mutex_);
    total_commands_++;
    
    try {
        if (isMemoryMode()) {
            return memory_storage_.find(key) != memory_storage_.end();
        }
        
        std::string cmd = "EXISTS " + key;
        auto reply = executeCommandWithReply(cmd);
        
        if (reply && reply->type == 3) { // REDIS_REPLY_INTEGER
            return reply->integer > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        failed_commands_++;
        logError("exists", e.what());
        return false;
    }
}

int RedisClientImpl::del(const std::string& key) {
    if (!ensureConnection()) return 0;
    
    std::lock_guard<std::mutex> lock(operation_mutex_);
    total_commands_++;
    
    try {
        if (isMemoryMode()) {
            int count = memory_storage_.erase(key) > 0 ? 1 : 0;
            hash_storage_.erase(key);
            list_storage_.erase(key);
            return count;
        }
        
        std::string cmd = "DEL " + key;
        auto reply = executeCommandWithReply(cmd);
        
        if (reply && reply->type == 3) { // REDIS_REPLY_INTEGER
            return static_cast<int>(reply->integer);
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        failed_commands_++;
        logError("del", e.what());
        return 0;
    }
}

int RedisClientImpl::del(const StringList& keys) {
    if (!ensureConnection() || keys.empty()) return 0;
    
    std::lock_guard<std::mutex> lock(operation_mutex_);
    total_commands_++;
    
    try {
        if (isMemoryMode()) {
            int count = 0;
            for (const auto& key : keys) {
                if (memory_storage_.erase(key) > 0) count++;
                hash_storage_.erase(key);
                list_storage_.erase(key);
            }
            return count;
        }
        
        std::string cmd = "DEL";
        for (const auto& key : keys) {
            cmd += " " + key;
        }
        
        auto reply = executeCommandWithReply(cmd);
        
        if (reply && reply->type == 3) { // REDIS_REPLY_INTEGER
            return static_cast<int>(reply->integer);
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        failed_commands_++;
        logError("del", e.what());
        return 0;
    }
}

int RedisClientImpl::ttl(const std::string& key) {
    if (!ensureConnection()) return -2;
    
    std::lock_guard<std::mutex> lock(operation_mutex_);
    total_commands_++;
    
    try {
        if (isMemoryMode()) {
            // 메모리 모드에서는 TTL 시뮬레이션 없음
            return exists(key) ? -1 : -2;
        }
        
        std::string cmd = "TTL " + key;
        auto reply = executeCommandWithReply(cmd);
        
        if (reply && reply->type == 3) { // REDIS_REPLY_INTEGER
            return static_cast<int>(reply->integer);
        }
        
        return -2;
        
    } catch (const std::exception& e) {
        failed_commands_++;
        logError("ttl", e.what());
        return -2;
    }
}

// =============================================================================
// Hash 조작
// =============================================================================

bool RedisClientImpl::hset(const std::string& key, const std::string& field, const std::string& value) {
    if (!ensureConnection()) return false;
    
    std::lock_guard<std::mutex> lock(operation_mutex_);
    total_commands_++;
    
    try {
        if (isMemoryMode()) {
            hash_storage_[key][field] = value;
            LogManager::getInstance().log("redis", PulseOne::Enums::LogLevel::DEBUG, 
                "[MEMORY] HSET " + key + " " + field + " " + value);
            return true;
        }
        
        std::string cmd = "HSET " + key + " " + field + " " + value;
        bool result = executeCommand(cmd);
        
        if (!result) failed_commands_++;
        return result;
        
    } catch (const std::exception& e) {
        failed_commands_++;
        logError("hset", e.what());
        return false;
    }
}

std::string RedisClientImpl::hget(const std::string& key, const std::string& field) {
    if (!ensureConnection()) return "";
    
    std::lock_guard<std::mutex> lock(operation_mutex_);
    total_commands_++;
    
    try {
        if (isMemoryMode()) {
            auto key_it = hash_storage_.find(key);
            if (key_it != hash_storage_.end()) {
                auto field_it = key_it->second.find(field);
                if (field_it != key_it->second.end()) {
                    return field_it->second;
                }
            }
            return "";
        }
        
        std::string cmd = "HGET " + key + " " + field;
        auto reply = executeCommandWithReply(cmd);
        
        if (reply && reply->type == 1 && reply->str) { // REDIS_REPLY_STRING
            return std::string(reply->str, reply->len);
        }
        
        return "";
        
    } catch (const std::exception& e) {
        failed_commands_++;
        logError("hget", e.what());
        return "";
    }
}

RedisClient::StringMap RedisClientImpl::hgetall(const std::string& key) {
    StringMap result;
    
    if (!ensureConnection()) return result;
    
    std::lock_guard<std::mutex> lock(operation_mutex_);
    total_commands_++;
    
    try {
        if (isMemoryMode()) {
            auto it = hash_storage_.find(key);
            if (it != hash_storage_.end()) {
                result = it->second;
            }
            return result;
        }
        
        std::string cmd = "HGETALL " + key;
        auto reply = executeCommandWithReply(cmd);
        
        if (reply && reply->type == 2 && reply->elements % 2 == 0) { // REDIS_REPLY_ARRAY
            for (size_t i = 0; i < reply->elements; i += 2) {
                if (reply->element[i]->str && reply->element[i+1]->str) {
                    std::string field(reply->element[i]->str, reply->element[i]->len);
                    std::string value(reply->element[i+1]->str, reply->element[i+1]->len);
                    result[field] = value;
                }
            }
        }
        
        return result;
        
    } catch (const std::exception& e) {
        failed_commands_++;
        logError("hgetall", e.what());
        return result;
    }
}

bool RedisClientImpl::hmset(const std::string& key, const StringMap& field_values) {
    if (!ensureConnection() || field_values.empty()) return false;
    
    std::lock_guard<std::mutex> lock(operation_mutex_);
    total_commands_++;
    
    try {
        if (isMemoryMode()) {
            for (const auto& pair : field_values) {
                hash_storage_[key][pair.first] = pair.second;
            }
            return true;
        }
        
        std::string cmd = "HMSET " + key;
        for (const auto& pair : field_values) {
            cmd += " " + pair.first + " " + pair.second;
        }
        
        bool result = executeCommand(cmd);
        
        if (!result) failed_commands_++;
        return result;
        
    } catch (const std::exception& e) {
        failed_commands_++;
        logError("hmset", e.what());
        return false;
    }
}

int RedisClientImpl::hdel(const std::string& key, const std::string& field) {
    if (!ensureConnection()) return 0;
    
    std::lock_guard<std::mutex> lock(operation_mutex_);
    total_commands_++;
    
    try {
        if (isMemoryMode()) {
            auto key_it = hash_storage_.find(key);
            if (key_it != hash_storage_.end()) {
                return key_it->second.erase(field) > 0 ? 1 : 0;
            }
            return 0;
        }
        
        std::string cmd = "HDEL " + key + " " + field;
        auto reply = executeCommandWithReply(cmd);
        
        if (reply && reply->type == 3) { // REDIS_REPLY_INTEGER
            return static_cast<int>(reply->integer);
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        failed_commands_++;
        logError("hdel", e.what());
        return 0;
    }
}

// =============================================================================
// Pub/Sub 메시징
// =============================================================================

bool RedisClientImpl::publish(const std::string& channel, const std::string& message) {
    if (!ensureConnection()) return false;
    
    std::lock_guard<std::mutex> lock(operation_mutex_);
    total_commands_++;
    
    try {
        if (isMemoryMode()) {
            LogManager::getInstance().log("redis", PulseOne::Enums::LogLevel::DEBUG, 
                "[MEMORY] PUBLISH " + channel + " -> " + message);
            return true;
        }
        
        std::string cmd = "PUBLISH " + channel + " " + message;
        bool result = executeCommand(cmd);
        
        if (!result) failed_commands_++;
        return result;
        
    } catch (const std::exception& e) {
        failed_commands_++;
        logError("publish", e.what());
        return false;
    }
}

bool RedisClientImpl::subscribe(const std::string& channel) {
    if (!ensureConnection()) return false;
    
    try {
        subscribed_channels_.push_back(channel);
        
        if (!pubsub_running_) {
            startPubSubThread();
        }
        
        LogManager::getInstance().log("redis", PulseOne::Enums::LogLevel::INFO, 
            "채널 구독: " + channel);
        
        return true;
        
    } catch (const std::exception& e) {
        logError("subscribe", e.what());
        return false;
    }
}

bool RedisClientImpl::unsubscribe(const std::string& channel) {
    if (!ensureConnection()) return false;
    
    try {
        auto it = std::find(subscribed_channels_.begin(), subscribed_channels_.end(), channel);
        if (it != subscribed_channels_.end()) {
            subscribed_channels_.erase(it);
        }
        
        if (subscribed_channels_.empty() && subscribed_patterns_.empty()) {
            stopPubSubThread();
        }
        
        LogManager::getInstance().log("redis", PulseOne::Enums::LogLevel::INFO, 
            "채널 구독 해제: " + channel);
        
        return true;
        
    } catch (const std::exception& e) {
        logError("unsubscribe", e.what());
        return false;
    }
}

bool RedisClientImpl::psubscribe(const std::string& pattern) {
    if (!ensureConnection()) return false;
    
    try {
        subscribed_patterns_.push_back(pattern);
        
        if (!pubsub_running_) {
            startPubSubThread();
        }
        
        LogManager::getInstance().log("redis", PulseOne::Enums::LogLevel::INFO, 
            "패턴 구독: " + pattern);
        
        return true;
        
    } catch (const std::exception& e) {
        logError("psubscribe", e.what());
        return false;
    }
}

bool RedisClientImpl::punsubscribe(const std::string& pattern) {
    if (!ensureConnection()) return false;
    
    try {
        auto it = std::find(subscribed_patterns_.begin(), subscribed_patterns_.end(), pattern);
        if (it != subscribed_patterns_.end()) {
            subscribed_patterns_.erase(it);
        }
        
        if (subscribed_channels_.empty() && subscribed_patterns_.empty()) {
            stopPubSubThread();
        }
        
        LogManager::getInstance().log("redis", PulseOne::Enums::LogLevel::INFO, 
            "패턴 구독 해제: " + pattern);
        
        return true;
        
    } catch (const std::exception& e) {
        logError("punsubscribe", e.what());
        return false;
    }
}

void RedisClientImpl::setMessageCallback(MessageCallback callback) {
    message_callback_ = callback;
}

// =============================================================================
// 상태 및 진단
// =============================================================================

bool RedisClientImpl::ping() {
    if (!ensureConnection()) return false;
    
    std::lock_guard<std::mutex> lock(operation_mutex_);
    total_commands_++;
    
    try {
        if (isMemoryMode()) {
            return true;
        }
        
        std::string cmd = "PING";
        auto reply = executeCommandWithReply(cmd);
        
        if (reply && reply->type == 5) { // REDIS_REPLY_STATUS
            return std::string(reply->str, reply->len) == "PONG";
        }
        
        return false;
        
    } catch (const std::exception& e) {
        failed_commands_++;
        logError("ping", e.what());
        return false;
    }
}

bool RedisClientImpl::select(int db_index) {
    if (!ensureConnection()) return false;
    
    std::lock_guard<std::mutex> lock(operation_mutex_);
    total_commands_++;
    
    try {
        selected_db_ = db_index;
        
        if (isMemoryMode()) {
            return true;
        }
        
        std::string cmd = "SELECT " + std::to_string(db_index);
        bool result = executeCommand(cmd);
        
        if (!result) failed_commands_++;
        return result;
        
    } catch (const std::exception& e) {
        failed_commands_++;
        logError("select", e.what());
        return false;
    }
}

RedisClient::StringMap RedisClientImpl::info() {
    StringMap result;
    
    if (isMemoryMode()) {
        result["redis_version"] = "6.2.0-memory-simulation";
        result["connected_clients"] = "1";
        result["used_memory"] = std::to_string(memory_storage_.size() * 100);
        result["uptime_in_seconds"] = "3600";
        return result;
    }
    
    // 실제 INFO 명령어 구현
    return result;
}

int RedisClientImpl::dbsize() {
    if (!ensureConnection()) return 0;
    
    if (isMemoryMode()) {
        return static_cast<int>(memory_storage_.size() + hash_storage_.size() + list_storage_.size());
    }
    
    // 실제 DBSIZE 명령어 구현
    return 0;
}

// =============================================================================
// 트랜잭션과 배치 처리 (간단한 구현)
// =============================================================================

bool RedisClientImpl::multi() {
    in_transaction_ = true;
    return true;
}

bool RedisClientImpl::exec() {
    in_transaction_ = false;
    return true;
}

bool RedisClientImpl::discard() {
    in_transaction_ = false;
    return true;
}

bool RedisClientImpl::mset(const StringMap& key_values) {
    if (key_values.empty()) return true;
    
    for (const auto& pair : key_values) {
        if (!set(pair.first, pair.second)) {
            return false;
        }
    }
    
    return true;
}

RedisClient::StringList RedisClientImpl::mget(const StringList& keys) {
    StringList result;
    result.reserve(keys.size());
    
    for (const auto& key : keys) {
        result.push_back(get(key));
    }
    
    return result;
}

// =============================================================================
// List 조작 (기본 구현)
// =============================================================================

int RedisClientImpl::lpush(const std::string& key, const std::string& value) {
    if (!ensureConnection()) return 0;
    
    if (isMemoryMode()) {
        list_storage_[key].insert(list_storage_[key].begin(), value);
        return static_cast<int>(list_storage_[key].size());
    }
    
    return 1; // 간단한 구현
}

int RedisClientImpl::rpush(const std::string& key, const std::string& value) {
    if (!ensureConnection()) return 0;
    
    if (isMemoryMode()) {
        list_storage_[key].push_back(value);
        return static_cast<int>(list_storage_[key].size());
    }
    
    return 1; // 간단한 구현
}

std::string RedisClientImpl::lpop(const std::string& key) {
    if (!ensureConnection()) return "";
    
    if (isMemoryMode()) {
        auto& list = list_storage_[key];
        if (!list.empty()) {
            std::string value = list.front();
            list.erase(list.begin());
            return value;
        }
    }
    
    return ""; // 간단한 구현
}

std::string RedisClientImpl::rpop(const std::string& key) {
    if (!ensureConnection()) return "";
    
    if (isMemoryMode()) {
        auto& list = list_storage_[key];
        if (!list.empty()) {
            std::string value = list.back();
            list.pop_back();
            return value;
        }
    }
    
    return ""; // 간단한 구현
}

RedisClient::StringList RedisClientImpl::lrange(const std::string& key, int start, int stop) {
    StringList result;
    
    if (!ensureConnection()) return result;
    
    if (isMemoryMode()) {
        auto it = list_storage_.find(key);
        if (it != list_storage_.end()) {
            const auto& list = it->second;
            int size = static_cast<int>(list.size());
            
            if (start < 0) start += size;
            if (stop < 0) stop += size;
            
            start = std::max(0, start);
            stop = std::min(size - 1, stop);
            
            for (int i = start; i <= stop && i < size; ++i) {
                result.push_back(list[i]);
            }
        }
    }
    
    return result;
}

int RedisClientImpl::llen(const std::string& key) {
    if (!ensureConnection()) return 0;
    
    if (isMemoryMode()) {
        auto it = list_storage_.find(key);
        if (it != list_storage_.end()) {
            return static_cast<int>(it->second.size());
        }
    }
    
    return 0;
}

// =============================================================================
// Sorted Set 조작 (기본 구현)
// =============================================================================

int RedisClientImpl::zadd(const std::string& key, double score, const std::string& member) {
    // 간단한 구현 - 실제로는 sorted set 구조 필요
    return 1;
}

RedisClient::StringList RedisClientImpl::zrangebyscore(const std::string& key, double min_score, double max_score) {
    return StringList{}; // 간단한 구현
}

int RedisClientImpl::zcard(const std::string& key) {
    return 0; // 간단한 구현
}

int RedisClientImpl::zremrangebyscore(const std::string& key, double min_score, double max_score) {
    return 0; // 간단한 구현
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

bool RedisClientImpl::ensureConnection() {
    if (!connected_ && auto_reconnect_) {
        return reconnect();
    }
    return connected_;
}

bool RedisClientImpl::reconnect() {
    if (reconnect_attempts_ >= MAX_RECONNECT_ATTEMPTS) {
        LogManager::getInstance().log("redis", PulseOne::Enums::LogLevel::ERROR, 
            "재연결 시도 횟수 초과: " + std::to_string(MAX_RECONNECT_ATTEMPTS));
        return false;
    }
    
    reconnect_attempts_++;
    
    LogManager::getInstance().log("redis", PulseOne::Enums::LogLevel::INFO, 
        "Redis 재연결 시도 #" + std::to_string(reconnect_attempts_));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_DELAY_MS));
    
    return connect(host_, port_, password_);
}

bool RedisClientImpl::executeCommand(const std::string& cmd) {
    if (isMemoryMode()) {
        return !simulateCommand(cmd).empty();
    }
    
    // 실제 hiredis 명령어 실행
    // redisReply* reply = (redisReply*)redisCommand(context_.get(), cmd.c_str());
    // 여기서는 시뮬레이션
    return true;
}

std::unique_ptr<redisReply, void(*)(redisReply*)> RedisClientImpl::executeCommandWithReply(const std::string& cmd) {
    // 실제 구현에서는 redisCommand 사용
    // return std::unique_ptr<redisReply, void(*)(redisReply*)>(
    //     (redisReply*)redisCommand(context_.get(), cmd.c_str()),
    //     freeReplyObject
    // );
    
    // 시뮬레이션 모드
    return std::unique_ptr<redisReply, void(*)(redisReply*)>(nullptr, [](redisReply*){});
}

std::string RedisClientImpl::simulateCommand(const std::string& cmd) {
    // 간단한 명령어 시뮬레이션
    LogManager::getInstance().log("redis", PulseOne::Enums::LogLevel::DEBUG, "[SIMULATE] " + cmd);
    return "OK";
}

void RedisClientImpl::startPubSubThread() {
    if (pubsub_running_) return;
    
    pubsub_running_ = true;
    pubsub_thread_ = std::thread(&RedisClientImpl::pubsubThreadWorker, this);
    
    LogManager::getInstance().log("redis", PulseOne::Enums::LogLevel::INFO, "Pub/Sub 스레드 시작됨");
}

void RedisClientImpl::stopPubSubThread() {
    if (!pubsub_running_) return;
    
    pubsub_running_ = false;
    pubsub_cv_.notify_all();
    
    if (pubsub_thread_.joinable()) {
        pubsub_thread_.join();
    }
    
    LogManager::getInstance().log("redis", PulseOne::Enums::LogLevel::INFO, "Pub/Sub 스레드 종료됨");
}

void RedisClientImpl::pubsubThreadWorker() {
    while (pubsub_running_) {
        // 실제 구현에서는 Redis Pub/Sub 메시지 수신
        // 여기서는 시뮬레이션
        std::unique_lock<std::mutex> lock(connection_mutex_);
        pubsub_cv_.wait_for(lock, std::chrono::seconds(1));
    }
}

RedisClientImpl::ConnectionStats RedisClientImpl::getConnectionStats() const {
    ConnectionStats stats;
    
    if (connected_) {
        auto now = std::chrono::steady_clock::now();
        stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(now - connect_time_);
    } else {
        stats.uptime = std::chrono::milliseconds(0);
    }
    
    stats.total_commands = total_commands_;
    stats.failed_commands = failed_commands_;
    stats.reconnect_attempts = reconnect_attempts_;
    stats.is_connected = connected_;
    
    return stats;
}

void RedisClientImpl::resetConnectionStats() {
    total_commands_ = 0;
    failed_commands_ = 0;
    reconnect_attempts_ = 0;
    connect_time_ = std::chrono::steady_clock::now();
}

void RedisClientImpl::logError(const std::string& operation, const std::string& error) {
    LogManager::getInstance().log("redis", PulseOne::Enums::LogLevel::ERROR, 
        "Redis " + operation + " 실패: " + error);
}