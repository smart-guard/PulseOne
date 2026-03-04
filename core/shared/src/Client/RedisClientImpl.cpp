// =============================================================================
// RedisClientImpl.cpp - 완전한 구현 (헤더 파일 기준)
// =============================================================================

#include "Client/RedisClientImpl.h"
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"
#include <functional>
#include <sstream>
#include <stdarg.h>

#ifdef HAVE_REDIS
#include <hiredis/hiredis.h>
#else
// 시뮬레이션 모드용 더미 정의
struct redisReply {
  int type;
  long long integer;
  char *str;
  size_t len;
  size_t elements;
  struct redisReply **element;
};
#define REDIS_REPLY_STRING 1
#define REDIS_REPLY_ARRAY 2
#define REDIS_REPLY_INTEGER 3
#define REDIS_REPLY_NIL 4
#define REDIS_REPLY_STATUS 5
#define REDIS_REPLY_ERROR 6
#endif

namespace PulseOne {

// =============================================================================
// 생성자/소멸자
// =============================================================================

RedisClientImpl::RedisClientImpl() {
  try {
    logInfo("Redis 클라이언트 초기화 시작");

    // 1. 설정 자동 로드
    loadConfiguration();

    // 2. 첫 연결 시도
    if (attemptConnection()) {
      logInfo("✅ 초기 Redis 연결 성공: " + host_ + ":" +
              std::to_string(port_));
    } else {
      logWarning("⚠️ 초기 Redis 연결 실패, 백그라운드에서 재시도");
    }

    // 3. 백그라운드 연결 감시 시작
    watchdog_thread_ = std::make_unique<std::thread>(
        &RedisClientImpl::connectionWatchdog, this);

    logInfo("Redis 클라이언트 초기화 완료");

  } catch (const std::exception &e) {
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
#ifdef HAVE_REDIS
    redisFree(context_);
#endif
    context_ = nullptr;
  }

  connected_ = false;
  logInfo("Redis 클라이언트 종료 완료");
}

// =============================================================================
// 기본 연결 관리
// =============================================================================

bool RedisClientImpl::connect(const std::string &host, int port,
                              const std::string &password) {
  logInfo("수동 연결 요청: " + host + ":" + std::to_string(port));

  std::lock_guard<std::recursive_mutex> lock(connection_mutex_);

  // 설정 업데이트
  host_ = host;
  port_ = port;
  password_ = password;

  // 기존 연결이 있다면 해제
  if (context_) {
#ifdef HAVE_REDIS
    redisFree(context_);
#endif
    context_ = nullptr;
  }

  connected_ = false;

  // 즉시 연결 시도
  return attemptConnection();
}

void RedisClientImpl::disconnect() {
  logInfo("수동 연결 해제 요청");
  std::lock_guard<std::recursive_mutex> lock(connection_mutex_);

  if (context_) {
#ifdef HAVE_REDIS
    redisFree(context_);
#endif
    context_ = nullptr;
  }

  connected_ = false;
  reconnect_attempts_ = 0; // 재연결 시도 횟수 초기화

  // 백그라운드 재연결 즉시 트리거
  watchdog_cv_.notify_one();
}

bool RedisClientImpl::forceReconnect() {
  std::lock_guard<std::recursive_mutex> lock(connection_mutex_);
  reconnect_attempts_ = 0;
  return attemptConnection();
}

bool RedisClientImpl::isConnected() const { return connected_.load(); }

void RedisClientImpl::setSubscriberMode(bool enabled) {
  is_subscriber_mode_ = enabled;
  if (enabled) {
    logInfo("구독 모드 활성화 - Watchdog PING 비활성화");
  } else {
    logInfo("구독 모드 비활성화 - Watchdog PING 활성화");
  }
}

bool RedisClientImpl::isSubscriberMode() const {
  return is_subscriber_mode_.load();
}

// =============================================================================
// Key-Value 조작
// =============================================================================

bool RedisClientImpl::set(const std::string &key, const std::string &value) {
  return executeWithRetry<bool>(
      [this, &key, &value]() {
#ifdef HAVE_REDIS
        redisReply *reply =
            executeCommandSafe("SET %s %s", key.c_str(), value.c_str());
        bool result = isReplyOK(reply);
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("SET " + key + " = " + value + " (시뮬레이션)");
        return true;
#endif
      },
      false);
}

bool RedisClientImpl::setex(const std::string &key, const std::string &value,
                            int expire_seconds) {
  return executeWithRetry<bool>(
      [this, key, value, expire_seconds]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("SETEX %s %d %s", key.c_str(),
                                               expire_seconds, value.c_str());
        bool result = isReplyOK(reply);
        if (!result) {
          if (reply) {
            std::string reply_str = "N/A";
            if (reply->type == REDIS_REPLY_STATUS ||
                reply->type == REDIS_REPLY_STRING ||
                reply->type == REDIS_REPLY_ERROR) {
              if (reply->str) {
                reply_str = std::string(reply->str, reply->len);
              } else {
                reply_str = "(null)";
              }
            }
            logWarning("Redis SETEX 실패: key=" + key +
                       ", reply_type=" + std::to_string(reply->type) +
                       ", len=" + std::to_string(reply->len) + ", str='" +
                       reply_str + "'");
          } else {
            logWarning("Redis SETEX 실패: key=" + key + ", reply is null");
          }
        }
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("SETEX " + key + " = " + value + " (시뮬레이션)");
        return true;
#endif
      },
      false);
}

std::string RedisClientImpl::get(const std::string &key) {
  return executeWithRetry<std::string>(
      [this, &key]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("GET %s", key.c_str());
        std::string result = replyToString(reply);
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("GET " + key + " (시뮬레이션)");
        return "simulation_value_" + key;
#endif
      },
      "");
}

int RedisClientImpl::del(const std::string &key) {
  return executeWithRetry<int>(
      [this, &key]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("DEL %s", key.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("DEL " + key + " (시뮬레이션)");
        return 1;
#endif
      },
      0);
}

bool RedisClientImpl::exists(const std::string &key) {
  return executeWithRetry<bool>(
      [this, &key]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("EXISTS %s", key.c_str());
        bool result = reply && replyToInteger(reply) > 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("EXISTS " + key + " (시뮬레이션)");
        return true;
#endif
      },
      false);
}

RedisClient::StringList RedisClientImpl::keys(const std::string &pattern) {
  return executeWithRetry<StringList>(
      [this, &pattern]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("KEYS %s", pattern.c_str());
        StringList result = replyToStringList(reply);
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("KEYS " + pattern + " (시뮬레이션)");
        StringList result;
        result.push_back("key1");
        result.push_back("key2");
        return result;
#endif
      },
      StringList{});
}

// SCAN: 논블로킹 커서 기반 키 순회 (프로덕션 안전, 30K+ 키 지원)
RedisClient::StringList RedisClientImpl::scan(const std::string &pattern,
                                              int count) {
  return executeWithRetry<StringList>(
      [this, &pattern, count]() {
        StringList all_keys;
#ifdef HAVE_REDIS
        long long cursor = 0;
        do {
          redisReply *reply = executeCommandSafe(
              "SCAN %lld MATCH %s COUNT %d", cursor, pattern.c_str(), count);
          if (!reply)
            break;

          if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 2) {
            // 첫 번째 요소: 다음 커서 (문자열)
            if (reply->element[0] && reply->element[0]->str)
              cursor = std::stoll(reply->element[0]->str);
            else
              cursor = 0;

            // 두 번째 요소: 이번 배치 키 목록
            if (reply->element[1]) {
              StringList batch = replyToStringList(reply->element[1]);
              all_keys.insert(all_keys.end(), batch.begin(), batch.end());
            }
          } else {
            freeReplyObject(reply);
            break;
          }
          freeReplyObject(reply);
        } while (cursor != 0);
#else
        logInfo("SCAN " + pattern + " (시뮬레이션)");
        all_keys.push_back("point:1:current");
        all_keys.push_back("point:2:current");
#endif
        return all_keys;
      },
      StringList{});
}

bool RedisClientImpl::expire(const std::string &key, int seconds) {
  return executeWithRetry<bool>(
      [this, &key, seconds]() {
#ifdef HAVE_REDIS
        redisReply *reply =
            executeCommandSafe("EXPIRE %s %d", key.c_str(), seconds);
        bool result = reply && replyToInteger(reply) == 1;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("EXPIRE " + key + " " + std::to_string(seconds) +
                " (시뮬레이션)");
        return true;
#endif
      },
      false);
}

int RedisClientImpl::ttl(const std::string &key) {
  return executeWithRetry<int>(
      [this, &key]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("TTL %s", key.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : -1;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("TTL " + key + " (시뮬레이션)");
        return 300; // 5분
#endif
      },
      -1);
}

int RedisClientImpl::incr(const std::string &key, int increment) {
  return executeWithRetry<int>(
      [this, &key, increment]() {
#ifdef HAVE_REDIS
        redisReply *reply;
        if (increment == 1) {
          reply = executeCommandSafe("INCR %s", key.c_str());
        } else {
          reply = executeCommandSafe("INCRBY %s %d", key.c_str(), increment);
        }
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("INCR " + key + " " + std::to_string(increment) +
                " (시뮬레이션)");
        return increment;
#endif
      },
      0);
}
// =============================================================================
// Hash 조작
// =============================================================================

bool RedisClientImpl::hset(const std::string &key, const std::string &field,
                           const std::string &value) {
  return executeWithRetry<bool>(
      [this, &key, &field, &value]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("HSET %s %s %s", key.c_str(),
                                               field.c_str(), value.c_str());
        bool result = reply && replyToInteger(reply) >= 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("HSET " + key + " " + field + " = " + value + " (시뮬레이션)");
        return true;
#endif
      },
      false);
}

std::string RedisClientImpl::hget(const std::string &key,
                                  const std::string &field) {
  return executeWithRetry<std::string>(
      [this, &key, &field]() {
#ifdef HAVE_REDIS
        redisReply *reply =
            executeCommandSafe("HGET %s %s", key.c_str(), field.c_str());
        std::string result = replyToString(reply);
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("HGET " + key + " " + field + " (시뮬레이션)");
        return "simulation_hash_value";
#endif
      },
      "");
}

RedisClient::StringMap RedisClientImpl::hgetall(const std::string &key) {
  return executeWithRetry<StringMap>(
      [this, &key]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("HGETALL %s", key.c_str());
        StringMap result = replyToStringMap(reply);
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("HGETALL " + key + " (시뮬레이션)");
        StringMap result;
        result["field1"] = "value1";
        result["field2"] = "value2";
        return result;
#endif
      },
      StringMap{});
}

int RedisClientImpl::hdel(const std::string &key, const std::string &field) {
  return executeWithRetry<int>(
      [this, &key, &field]() {
#ifdef HAVE_REDIS
        redisReply *reply =
            executeCommandSafe("HDEL %s %s", key.c_str(), field.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("HDEL " + key + " " + field + " (시뮬레이션)");
        return 1;
#endif
      },
      0);
}

bool RedisClientImpl::hexists(const std::string &key,
                              const std::string &field) {
  return executeWithRetry<bool>(
      [this, &key, &field]() {
#ifdef HAVE_REDIS
        redisReply *reply =
            executeCommandSafe("HEXISTS %s %s", key.c_str(), field.c_str());
        bool result = reply && replyToInteger(reply) > 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("HEXISTS " + key + " " + field + " (시뮬레이션)");
        return true;
#endif
      },
      false);
}

int RedisClientImpl::hlen(const std::string &key) {
  return executeWithRetry<int>(
      [this, &key]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("HLEN %s", key.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("HLEN " + key + " (시뮬레이션)");
        return 2;
#endif
      },
      0);
}

// =============================================================================
// List 조작
// =============================================================================

int RedisClientImpl::lpush(const std::string &key, const std::string &value) {
  return executeWithRetry<int>(
      [this, &key, &value]() {
#ifdef HAVE_REDIS
        redisReply *reply =
            executeCommandSafe("LPUSH %s %s", key.c_str(), value.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("LPUSH " + key + " " + value + " (시뮬레이션)");
        return 1;
#endif
      },
      0);
}

int RedisClientImpl::rpush(const std::string &key, const std::string &value) {
  return executeWithRetry<int>(
      [this, &key, &value]() {
#ifdef HAVE_REDIS
        redisReply *reply =
            executeCommandSafe("RPUSH %s %s", key.c_str(), value.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("RPUSH " + key + " " + value + " (시뮬레이션)");
        return 1;
#endif
      },
      0);
}

std::string RedisClientImpl::lpop(const std::string &key) {
  return executeWithRetry<std::string>(
      [this, &key]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("LPOP %s", key.c_str());
        std::string result = replyToString(reply);
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("LPOP " + key + " (시뮬레이션)");
        return "simulation_lpop_value";
#endif
      },
      "");
}

std::string RedisClientImpl::rpop(const std::string &key) {
  return executeWithRetry<std::string>(
      [this, &key]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("RPOP %s", key.c_str());
        std::string result = replyToString(reply);
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("RPOP " + key + " (시뮬레이션)");
        return "simulation_rpop_value";
#endif
      },
      "");
}

RedisClient::StringList RedisClientImpl::lrange(const std::string &key,
                                                int start, int stop) {
  return executeWithRetry<StringList>(
      [this, &key, start, stop]() {
#ifdef HAVE_REDIS
        redisReply *reply =
            executeCommandSafe("LRANGE %s %d %d", key.c_str(), start, stop);
        StringList result = replyToStringList(reply);
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("LRANGE " + key + " " + std::to_string(start) + " " +
                std::to_string(stop) + " (시뮬레이션)");
        StringList result;
        result.push_back("item1");
        result.push_back("item2");
        return result;
#endif
      },
      StringList{});
}

int RedisClientImpl::llen(const std::string &key) {
  return executeWithRetry<int>(
      [this, &key]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("LLEN %s", key.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("LLEN " + key + " (시뮬레이션)");
        return 5;
#endif
      },
      0);
}

// =============================================================================
// Set 조작
// =============================================================================

int RedisClientImpl::sadd(const std::string &key, const std::string &member) {
  return executeWithRetry<int>(
      [this, &key, &member]() {
#ifdef HAVE_REDIS
        redisReply *reply =
            executeCommandSafe("SADD %s %s", key.c_str(), member.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("SADD " + key + " " + member + " (시뮬레이션)");
        return 1;
#endif
      },
      0);
}

int RedisClientImpl::srem(const std::string &key, const std::string &member) {
  return executeWithRetry<int>(
      [this, &key, &member]() {
#ifdef HAVE_REDIS
        redisReply *reply =
            executeCommandSafe("SREM %s %s", key.c_str(), member.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("SREM " + key + " " + member + " (시뮬레이션)");
        return 1;
#endif
      },
      0);
}

bool RedisClientImpl::sismember(const std::string &key,
                                const std::string &member) {
  return executeWithRetry<bool>(
      [this, &key, &member]() {
#ifdef HAVE_REDIS
        redisReply *reply =
            executeCommandSafe("SISMEMBER %s %s", key.c_str(), member.c_str());
        bool result = reply && replyToInteger(reply) == 1;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("SISMEMBER " + key + " " + member + " (시뮬레이션)");
        return true;
#endif
      },
      false);
}

RedisClient::StringList RedisClientImpl::smembers(const std::string &key) {
  return executeWithRetry<StringList>(
      [this, &key]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("SMEMBERS %s", key.c_str());
        StringList result = replyToStringList(reply);
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("SMEMBERS " + key + " (시뮬레이션)");
        StringList result;
        result.push_back("member1");
        result.push_back("member2");
        return result;
#endif
      },
      StringList{});
}

int RedisClientImpl::scard(const std::string &key) {
  return executeWithRetry<int>(
      [this, &key]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("SCARD %s", key.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("SCARD " + key + " (시뮬레이션)");
        return 3;
#endif
      },
      0);
}

// =============================================================================
// Sorted Set 조작
// =============================================================================

int RedisClientImpl::zadd(const std::string &key, double score,
                          const std::string &member) {
  return executeWithRetry<int>(
      [this, &key, score, &member]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("ZADD %s %f %s", key.c_str(),
                                               score, member.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("ZADD " + key + " " + std::to_string(score) + " " + member +
                " (시뮬레이션)");
        return 1;
#endif
      },
      0);
}

int RedisClientImpl::zrem(const std::string &key, const std::string &member) {
  return executeWithRetry<int>(
      [this, &key, &member]() {
#ifdef HAVE_REDIS
        redisReply *reply =
            executeCommandSafe("ZREM %s %s", key.c_str(), member.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("ZREM " + key + " " + member + " (시뮬레이션)");
        return 1;
#endif
      },
      0);
}

RedisClient::StringList RedisClientImpl::zrange(const std::string &key,
                                                int start, int stop) {
  return executeWithRetry<StringList>(
      [this, &key, start, stop]() {
#ifdef HAVE_REDIS
        redisReply *reply =
            executeCommandSafe("ZRANGE %s %d %d", key.c_str(), start, stop);
        StringList result = replyToStringList(reply);
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("ZRANGE " + key + " " + std::to_string(start) + " " +
                std::to_string(stop) + " (시뮬레이션)");
        StringList result;
        result.push_back("member1");
        result.push_back("member2");
        return result;
#endif
      },
      StringList{});
}

int RedisClientImpl::zcard(const std::string &key) {
  return executeWithRetry<int>(
      [this, &key]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("ZCARD %s", key.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("ZCARD " + key + " (시뮬레이션)");
        return 2;
#endif
      },
      0);
}

// =============================================================================
// Pub/Sub
// =============================================================================

int RedisClientImpl::publish(const std::string &channel,
                             const std::string &message) {
  return executeWithRetry<int>(
      [this, &channel, &message]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("PUBLISH %s %s", channel.c_str(),
                                               message.c_str());
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("PUBLISH " + channel + " " + message + " (시뮬레이션)");
        return 1; // 구독자 1명 시뮬레이션
#endif
      },
      0);
}

bool RedisClientImpl::subscribe(const std::string &channel) {
  return executeWithRetry<bool>(
      [this, &channel]() {
#ifdef HAVE_REDIS
        if (!context_) {
          logError("Redis 연결 없음 - SUBSCRIBE 불가");
          return false;
        }

        redisReply *reply = executeCommandSafe("SUBSCRIBE %s", channel.c_str());
        bool result = reply && (reply->type == REDIS_REPLY_ARRAY);

        if (result) {
          logInfo("SUBSCRIBE " + channel + " (실제 구독 완료)");
        } else {
          logError("SUBSCRIBE " + channel + " 실패");
        }

        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("SUBSCRIBE " + channel + " (시뮬레이션)");
        return true;
#endif
      },
      false);
}

bool RedisClientImpl::unsubscribe(const std::string &channel) {
  return executeWithRetry<bool>(
      [this, &channel]() {
#ifdef HAVE_REDIS
        if (!context_) {
          logError("Redis 연결 없음 - UNSUBSCRIBE 불가");
          return false;
        }

        redisReply *reply =
            executeCommandSafe("UNSUBSCRIBE %s", channel.c_str());
        bool result = reply && (reply->type == REDIS_REPLY_ARRAY);

        if (result) {
          logInfo("UNSUBSCRIBE " + channel + " 성공");
        } else {
          logError("UNSUBSCRIBE " + channel + " 실패");
        }

        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("UNSUBSCRIBE " + channel + " (시뮬레이션)");
        return true;
#endif
      },
      false);
}

bool RedisClientImpl::psubscribe(const std::string &pattern) {
  return executeWithRetry<bool>(
      [this, &pattern]() {
#ifdef HAVE_REDIS
        if (!context_) {
          logError("Redis 연결 없음 - PSUBSCRIBE 불가");
          return false;
        }

        redisReply *reply =
            executeCommandSafe("PSUBSCRIBE %s", pattern.c_str());
        bool result = reply && (reply->type == REDIS_REPLY_ARRAY);

        if (result) {
          logInfo("PSUBSCRIBE " + pattern + " 성공");
        } else {
          logError("PSUBSCRIBE " + pattern + " 실패");
        }

        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("PSUBSCRIBE " + pattern + " (시뮬레이션)");
        return true;
#endif
      },
      false);
}

bool RedisClientImpl::punsubscribe(const std::string &pattern) {
  return executeWithRetry<bool>(
      [this, &pattern]() {
#ifdef HAVE_REDIS
        if (!context_) {
          logError("Redis 연결 없음 - PUNSUBSCRIBE 불가");
          return false;
        }

        redisReply *reply =
            executeCommandSafe("PUNSUBSCRIBE %s", pattern.c_str());
        bool result = reply && (reply->type == REDIS_REPLY_ARRAY);

        if (result) {
          logInfo("PUNSUBSCRIBE " + pattern + " 성공");
        } else {
          logError("PUNSUBSCRIBE " + pattern + " 실패");
        }

        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("PUNSUBSCRIBE " + pattern + " (시뮬레이션)");
        return true;
#endif
      },
      false);
}

void RedisClientImpl::setMessageCallback(MessageCallback callback) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  message_callback_ = callback;
  logInfo("메시지 콜백 설정 완료");
}

bool RedisClientImpl::waitForMessage(int timeout_ms) {
#ifdef HAVE_REDIS
  // [LOCK ORDER CONTRACT]
  // ① connection_mutex_ 획득 → fd 스냅샷 후 즉시 해제  (블로킹 I/O 전)
  // ② ::select() 호출 — 락 없음 (블로킹이므로 락 보유 금지)
  // ③ connection_mutex_ 재획득 → redisGetReply()
  // ④ connection_mutex_ 보유 중 callback_mutex_ 획득 → 콜백 호출
  //
  // ⚠️  콜백 내부에서 Redis API(set/get/publish 등)를 호출하면
  //     connection_mutex_를 recursive하게 재진입하므로 허용되지만,
  //     콜백이 다시 waitForMessage()를 호출하는 것은 금지.
  //     (락 순서: connection_mutex_ → callback_mutex_)
  int fd_snapshot;
  {
    std::lock_guard<std::recursive_mutex> lock(connection_mutex_);
    if (!context_) {
      return false;
    }
    fd_snapshot = context_->fd;
  }

  // 타임아웃 설정
  struct timeval tv = {.tv_sec = timeout_ms / 1000,
                       .tv_usec = (timeout_ms % 1000) * 1000};

  // 소켓에서 읽을 데이터가 있는지 확인
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(fd_snapshot, &readfds);

  // ✅ 전역 네임스페이스의 select 명시 (lock 없이 호출 — 블로킹 I/O)
  int result = ::select(fd_snapshot + 1, &readfds, nullptr, nullptr, &tv);

  if (result > 0 && FD_ISSET(fd_snapshot, &readfds)) {
    // 메시지 읽기: context_를 다시 락으로 보호
    std::lock_guard<std::recursive_mutex> lock(connection_mutex_);
    if (!context_) {
      return false; // 선택(select) 대기 중 재연결로 context_ 교체됨
    }

    redisReply *reply = nullptr;
    if (redisGetReply(context_, (void **)&reply) == REDIS_OK && reply) {
      // 메시지 타입 확인
      if (reply->type == REDIS_REPLY_ARRAY && reply->elements >= 3) {
        std::string msg_type = replyToString(reply->element[0]);

        if (msg_type == "message") {
          // 일반 채널 메시지: ["message", "channel", "payload"]
          std::string channel = replyToString(reply->element[1]);
          std::string message = replyToString(reply->element[2]);

          // 콜백 호출 (connection_mutex_와 callback_mutex_ 순서 고정)
          std::lock_guard<std::mutex> cb_lock(callback_mutex_);
          if (message_callback_) {
            message_callback_(channel, message);
          }

        } else if (msg_type == "pmessage" && reply->elements >= 4) {
          // 패턴 메시지: ["pmessage", "pattern", "channel", "payload"]
          std::string channel = replyToString(reply->element[2]);
          std::string message = replyToString(reply->element[3]);

          // 콜백 호출
          std::lock_guard<std::mutex> cb_lock(callback_mutex_);
          if (message_callback_) {
            message_callback_(channel, message);
          }
        }
      }

      freeReplyObject(reply);
      return true;
    }
  }

  return false;
#else
  std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
  return false;
#endif
}
// =============================================================================
// 배치 처리
// =============================================================================

bool RedisClientImpl::mset(const StringMap &key_values) {
  if (key_values.empty())
    return true;

  return executeWithRetry<bool>(
      [this, &key_values]() {
#ifdef HAVE_REDIS
        std::vector<std::string> args;
        args.push_back("MSET");

        for (const auto &[key, value] : key_values) {
          args.push_back(key);
          args.push_back(value);
        }

        std::vector<const char *> argv;
        std::vector<size_t> argvlen;

        for (const auto &arg : args) {
          argv.push_back(arg.c_str());
          argvlen.push_back(arg.length());
        }

        redisReply *reply = static_cast<redisReply *>(redisCommandArgv(
            context_, argv.size(), argv.data(), argvlen.data()));

        bool result = isReplyOK(reply);
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("MSET (시뮬레이션): " + std::to_string(key_values.size()) +
                " 키-값 쌍");
        return true;
#endif
      },
      false);
}

RedisClient::StringList RedisClientImpl::mget(const StringList &keys) {
  if (keys.empty())
    return StringList{};

  return executeWithRetry<StringList>(
      [this, &keys]() {
#ifdef HAVE_REDIS
        std::vector<std::string> args;
        args.push_back("MGET");
        args.insert(args.end(), keys.begin(), keys.end());

        std::vector<const char *> argv;
        std::vector<size_t> argvlen;

        for (const auto &arg : args) {
          argv.push_back(arg.c_str());
          argvlen.push_back(arg.length());
        }

        redisReply *reply = static_cast<redisReply *>(redisCommandArgv(
            context_, argv.size(), argv.data(), argvlen.data()));

        StringList result = replyToStringList(reply);
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("MGET (시뮬레이션): " + std::to_string(keys.size()) +
                " 키 조회");
        StringList result;
        for (size_t i = 0; i < keys.size(); ++i) {
          result.push_back("simulation_value_" + std::to_string(i));
        }
        return result;
#endif
      },
      StringList{});
}

// =============================================================================
// 트랜잭션 지원
// =============================================================================

bool RedisClientImpl::multi() {
  return executeWithRetry<bool>(
      [this]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("MULTI");
        bool result = isReplyOK(reply);
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("MULTI (시뮬레이션)");
        return true;
#endif
      },
      false);
}

bool RedisClientImpl::exec() {
  return executeWithRetry<bool>(
      [this]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("EXEC");
        bool result = reply && reply->type == REDIS_REPLY_ARRAY;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("EXEC (시뮬레이션)");
        return true;
#endif
      },
      false);
}

bool RedisClientImpl::discard() {
  return executeWithRetry<bool>(
      [this]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("DISCARD");
        bool result = isReplyOK(reply);
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        logInfo("DISCARD (시뮬레이션)");
        return true;
#endif
      },
      false);
}

// =============================================================================
// 상태 및 진단
// =============================================================================

RedisClient::StringMap RedisClientImpl::info() {
  return executeWithRetry<StringMap>(
      [this]() {
        StringMap result;

#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("INFO");
        if (reply && reply->type == REDIS_REPLY_STRING) {
          std::string info_str(reply->str, reply->len);

          std::istringstream iss(info_str);
          std::string line;

          while (std::getline(iss, line)) {
            if (line.empty() || line[0] == '#')
              continue;

            auto pos = line.find(':');
            if (pos != std::string::npos) {
              std::string key = line.substr(0, pos);
              std::string value = line.substr(pos + 1);

              if (!value.empty() && value.back() == '\r') {
                value.pop_back();
              }

              result[key] = value;
            }
          }
        }
        if (reply)
          freeReplyObject(reply);
#else
        result["redis_version"] = "7.0.0-simulation";
        result["connected_clients"] = "1";
        result["used_memory"] = "1024000";
        result["uptime_in_seconds"] = "3600";
#endif

        return result;
      },
      StringMap{});
}

bool RedisClientImpl::ping() {
  return executeWithRetry<bool>(
      [this]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("PING");
        bool result = reply && reply->type == REDIS_REPLY_STATUS &&
                      strcmp(reply->str, "PONG") == 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        return connected_.load();
#endif
      },
      false);
}

bool RedisClientImpl::select(int db_index) {
  return executeWithRetry<bool>(
      [this, db_index]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("SELECT %d", db_index);
        bool result = isReplyOK(reply);
        if (result) {
          database_ = db_index;
        }
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        database_ = db_index;
        logInfo("SELECT " + std::to_string(db_index) + " (시뮬레이션)");
        return true;
#endif
      },
      false);
}

int RedisClientImpl::dbsize() {
  return executeWithRetry<int>(
      [this]() {
#ifdef HAVE_REDIS
        redisReply *reply = executeCommandSafe("DBSIZE");
        int result = reply ? static_cast<int>(replyToInteger(reply)) : 0;
        if (reply)
          freeReplyObject(reply);
        return result;
#else
        return 42; // 시뮬레이션 값
#endif
      },
      0);
}

// =============================================================================
// 통계 및 상태 조회
// =============================================================================

RedisClientImpl::ConnectionStats RedisClientImpl::getStats() const {
  std::lock_guard<std::mutex> lock(stats_mutex_);

  ConnectionStats stats;
  stats.total_commands = total_commands_;
  stats.successful_commands = successful_commands_;
  stats.failed_commands = failed_commands_;
  stats.total_reconnects = total_reconnects_;
  stats.current_reconnect_attempts = reconnect_attempts_;
  stats.connect_time = connect_time_;
  stats.is_connected = connected_;

  return stats;
}

void RedisClientImpl::resetStats() {
  std::lock_guard<std::mutex> lock(stats_mutex_);

  total_commands_ = 0;
  successful_commands_ = 0;
  failed_commands_ = 0;
  total_reconnects_ = 0;
  connect_time_ = std::chrono::steady_clock::now();

  logInfo("Redis 통계 초기화 완료");
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

void RedisClientImpl::loadConfiguration() {
  try {
    auto &config = ConfigManager::getInstance();

    logInfo("🔧 Redis 설정 로드 시작...");

    // 🔥 올바른 설정 키 사용 (REDIS_PRIMARY_* 사용)
    host_ = config.getOrDefault("REDIS_PRIMARY_HOST", "localhost");
    logInfo("📍 읽은 호스트: " + host_);

    port_ = config.getInt("REDIS_PRIMARY_PORT", 6379);
    logInfo("🔌 읽은 포트: " + std::to_string(port_));

    password_ = config.getOrDefault("REDIS_PRIMARY_PASSWORD", "");
    if (password_.empty()) {
      logInfo("🔐 Redis 패스워드: 없음");
    } else {
      logInfo("🔐 Redis 패스워드: 설정됨 (****)");
    }

    database_ = config.getInt("REDIS_PRIMARY_DB", 0);
    logInfo("🗄️ Redis DB: " + std::to_string(database_));

    // 🔥 추가 설정들
    bool enabled = config.getBool("REDIS_PRIMARY_ENABLED", true);
    logInfo("✅ Redis 활성화: " + std::string(enabled ? "true" : "false"));

    int timeout_ms = config.getInt("REDIS_PRIMARY_TIMEOUT_MS", 5000);
    logInfo("⏰ Redis 타임아웃: " + std::to_string(timeout_ms) + "ms");

    int connect_timeout_ms =
        config.getInt("REDIS_PRIMARY_CONNECT_TIMEOUT_MS", 3000);
    logInfo("🔗 Redis 연결 타임아웃: " + std::to_string(connect_timeout_ms) +
            "ms");

    bool test_mode = config.getBool("REDIS_TEST_MODE", false);
    logInfo("🧪 Redis 테스트 모드: " +
            std::string(test_mode ? "true" : "false"));

    logInfo("✅ Redis 설정 로드 완료: " + host_ + ":" + std::to_string(port_) +
            " (DB " + std::to_string(database_) + ")");

    // 🔥 활성화 체크 - 비활성화되어 있으면 연결 건너뛰기
    if (!enabled) {
      logWarning(
          "⚠️ Redis가 비활성화되어 있습니다 (REDIS_PRIMARY_ENABLED=false)");
    }

  } catch (const std::exception &e) {
    logError("❌ Redis 설정 로드 실패: " + std::string(e.what()));

    // 기본값으로 폴백
    host_ = "localhost";
    port_ = 6379;
    password_ = "";
    database_ = 0;

    logWarning("⚠️ Redis 기본값으로 폴백: " + host_ + ":" +
               std::to_string(port_));
  }
}

bool RedisClientImpl::attemptConnection() {
  std::lock_guard<std::recursive_mutex> lock(connection_mutex_);

#ifdef HAVE_REDIS
  try {
    if (context_) {
      redisFree(context_);
      context_ = nullptr;
    }

    struct timeval timeout = {.tv_sec = CONNECTION_TIMEOUT.count() / 1000,
                              .tv_usec =
                                  (CONNECTION_TIMEOUT.count() % 1000) * 1000};

    context_ = redisConnectWithTimeout(host_.c_str(), port_, timeout);

    if (!context_ || context_->err) {
      if (context_) {
        logError("연결 실패: " + std::string(context_->errstr));
        redisFree(context_);
        context_ = nullptr;
      }
      return false;
    }

    // 인증
    if (!password_.empty()) {
      redisReply *auth_reply =
          (redisReply *)redisCommand(context_, "AUTH %s", password_.c_str());
      if (!auth_reply || auth_reply->type == REDIS_REPLY_ERROR) {
        logError("Redis 인증 실패");
        if (auth_reply)
          freeReplyObject(auth_reply);
        redisFree(context_);
        context_ = nullptr;
        return false;
      }
      freeReplyObject(auth_reply);
    }

    // DB 선택
    if (database_ != 0) {
      redisReply *select_reply =
          (redisReply *)redisCommand(context_, "SELECT %d", database_);
      if (!select_reply || select_reply->type == REDIS_REPLY_ERROR) {
        logError("데이터베이스 선택 실패: DB " + std::to_string(database_));
        if (select_reply)
          freeReplyObject(select_reply);
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

  } catch (const std::exception &e) {
    logError("연결 중 예외: " + std::string(e.what()));
    return false;
  }
#else
  logInfo("hiredis 라이브러리 없음, 시뮬레이션 모드");
  connected_ = true;
  return true;
#endif
}

bool RedisClientImpl::ensureConnected() {
  // [BUG #13 FIX] connected_/context_/reconnect_attempts_를 뮤텍스 없이
  // 읽으면 watchdog 스레드의 재연결과 TOCTOU 레이스가 발생한다.
  // connection_mutex_를 잡고 전체 상태를 일관되게 확인한다.
  std::lock_guard<std::recursive_mutex> lock(connection_mutex_);

  if (connected_ && context_) {
    return true;
  }

  if (reconnect_attempts_ >= MAX_RECONNECT_ATTEMPTS) {
    return false;
  }

  auto now = std::chrono::steady_clock::now();
  if (now - last_reconnect_attempt_ < RECONNECT_DELAY) {
    return false;
  }

  last_reconnect_attempt_ = now;
  reconnect_attempts_++;

  // attemptConnection()도 connection_mutex_를 recursive하게 잡으므로 안전
  if (attemptConnection()) {
    total_reconnects_++;
    logInfo("재연결 성공 (시도 " + std::to_string(reconnect_attempts_) + ")");
    return true;
  }

  logWarning("재연결 실패 (시도 " + std::to_string(reconnect_attempts_) + "/" +
             std::to_string(MAX_RECONNECT_ATTEMPTS) + ")");
  return false;
}

void RedisClientImpl::connectionWatchdog() {
  // [LOCK ORDER CONTRACT]
  // watchdog_mutex_ → connection_mutex_ 순서로만 획득.
  // connection_mutex_ 보유 중에 watchdog_mutex_를 잡으면 ABBA 데드락 발생.
  // watchdog_cv_.notify_*() 는 connection_mutex_ 없이 호출 가능 (wait만 mutex
  // 필요).
  while (!shutdown_requested_) {
    std::unique_lock<std::mutex> lock(watchdog_mutex_);
    watchdog_cv_.wait_for(lock, std::chrono::seconds(30),
                          [this] { return shutdown_requested_.load(); });

    if (shutdown_requested_)
      break;

    if (connected_) {
      // 구독 모드일 경우 PING 건너뛰기 (hiredis 스레드 안전성 문제 및 프로토콜
      // 제한)
      if (is_subscriber_mode_) {
        continue;
      }

      if (!ping()) {
        logWarning("연결 감시: PING 실패, 재연결 시도");
        connected_ = false;
      }
    }

    // 연결되어 있지 않다면 재연결 시도 (초기 연결 실패 시나리오 대응)
    if (!connected_) {
      ensureConnected();
    }
  }
}

void RedisClientImpl::logInfo(const std::string &message) const {
  LogManager::getInstance().log("redis", LogLevel::INFO, message);
}

void RedisClientImpl::logWarning(const std::string &message) const {
  LogManager::getInstance().log("redis", LogLevel::WARN, message);
}

void RedisClientImpl::logError(const std::string &message) const {
  LogManager::getInstance().log("redis", LogLevel::LOG_ERROR, message);
}

void RedisClientImpl::incrementCommand() { total_commands_++; }

void RedisClientImpl::incrementSuccess() { successful_commands_++; }

void RedisClientImpl::incrementFailure() { failed_commands_++; }

#ifdef HAVE_REDIS
// =============================================================================
// hiredis 전용 구현들
// =============================================================================

redisReply *RedisClientImpl::executeCommandSafe(const char *format, ...) {
  if (!context_)
    return nullptr;

  va_list args;
  va_start(args, format);
  redisReply *reply = (redisReply *)redisvCommand(context_, format, args);
  va_end(args);

  if (!reply && isConnectionError()) {
    connected_ = false;
    logWarning("Redis 명령 실행 중 연결 오류 감지");
  }

  return reply;
}

std::string RedisClientImpl::replyToString(redisReply *reply) const {
  if (!reply)
    return "";

  switch (reply->type) {
  case REDIS_REPLY_STRING:
  case REDIS_REPLY_STATUS:
    if (reply->str == nullptr)
      return "";
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

long long RedisClientImpl::replyToInteger(redisReply *reply) const {
  if (!reply)
    return 0;

  switch (reply->type) {
  case REDIS_REPLY_INTEGER:
    return reply->integer;
  case REDIS_REPLY_STRING:
    try {
      return std::stoll(std::string(reply->str, reply->len));
    } catch (const std::exception &) {
      return 0;
    }
  case REDIS_REPLY_NIL:
    return 0;
  default:
    return 0;
  }
}

RedisClient::StringList
RedisClientImpl::replyToStringList(redisReply *reply) const {
  StringList result;

  if (!reply || reply->type != REDIS_REPLY_ARRAY) {
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

RedisClient::StringMap
RedisClientImpl::replyToStringMap(redisReply *reply) const {
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

bool RedisClientImpl::isReplyOK(redisReply *reply) const {
  return reply && reply->type == REDIS_REPLY_STATUS && reply->len == 2 &&
         strcmp(reply->str, "OK") == 0;
}

bool RedisClientImpl::isConnectionError() const {
  return context_ && context_->err != 0;
}
#endif

} // namespace PulseOne