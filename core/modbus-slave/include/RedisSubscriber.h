// =============================================================================
// core/modbus-slave/include/RedisSubscriber.h
// Redis 구독 → 레지스터 테이블 갱신
// =============================================================================
#pragma once

#include "MappingConfig.h"
#include "RegisterTable.h"
#include <atomic>
#include <string>
#include <thread>
#include <vector>

// hiredis 조건부 포함
#ifdef HAVE_REDIS
#include <hiredis/hiredis.h>
#else
struct redisContext;
struct redisReply;
#endif

namespace PulseOne {
namespace ModbusSlave {

class RedisSubscriber {
public:
  RedisSubscriber();
  ~RedisSubscriber();

  // Redis 연결 및 구독 시작
  bool Start(const std::string &host, int port, RegisterTable &table,
             const std::vector<RegisterMapping> &mappings);
  void Stop();

  bool IsConnected() const { return connected_.load(); }

private:
  void SubscribeLoop();

  // JSON 페이로드 파싱 → 레지스터 갱신
  // 예상 포맷: {"point_id":42,"value":23.5,"quality":"GOOD","timestamp":"..."}
  void OnMessage(const std::string &payload);

  void WriteValueToTable(const RegisterMapping &m, double value);

  redisContext *ctx_ = nullptr;
  RegisterTable *table_ = nullptr;
  std::vector<RegisterMapping> mappings_;

  // point_id → mapping 빠른 조회
  std::unordered_map<int, const RegisterMapping *> point_map_;

  std::atomic<bool> running_{false};
  std::atomic<bool> connected_{false};
  std::thread sub_thread_;

  std::string host_;
  int port_ = 6379;
};

} // namespace ModbusSlave
} // namespace PulseOne
