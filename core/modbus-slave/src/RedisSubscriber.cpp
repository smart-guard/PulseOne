// =============================================================================
// core/modbus-slave/src/RedisSubscriber.cpp
// Redis SUBSCRIBE → 레지스터 테이블 갱신
// =============================================================================
#include "RedisSubscriber.h"
#include <iostream>
#include <stdexcept>

#ifdef HAVE_REDIS
#include <hiredis/hiredis.h>
#endif

// nlohmann/json이 없으면 간단한 파서로 대체
#ifdef HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

namespace PulseOne {
namespace ModbusSlave {

RedisSubscriber::RedisSubscriber() = default;

RedisSubscriber::~RedisSubscriber() { Stop(); }

bool RedisSubscriber::Start(const std::string &host, int port,
                            RegisterTable &table,
                            const std::vector<RegisterMapping> &mappings) {
#ifndef HAVE_REDIS
  std::cout << "[RedisSubscriber] hiredis 없음 — 더미 모드로 동작\n";
  return true; // 뼈대: hiredis 없어도 기동은 성공
#else
  host_ = host;
  port_ = port;
  table_ = &table;
  mappings_ = mappings;

  // point_id → mapping 빠른 조회 맵 구성
  for (const auto &m : mappings_) {
    point_map_[m.point_id] = &m;
  }

  // Redis 연결 테스트
  redisContext *test_ctx = redisConnect(host.c_str(), port);
  if (!test_ctx || test_ctx->err) {
    std::cerr << "[RedisSubscriber] 연결 실패: "
              << (test_ctx ? test_ctx->errstr : "null") << "\n";
    if (test_ctx)
      redisFree(test_ctx);
    return false;
  }
  redisFree(test_ctx);

  running_ = true;
  sub_thread_ = std::thread(&RedisSubscriber::SubscribeLoop, this);
  std::cout << "[RedisSubscriber] 시작: " << host << ":" << port << " ("
            << mappings_.size() << "개 포인트 매핑)\n";
  return true;
#endif
}

void RedisSubscriber::Stop() {
  running_ = false;
  if (sub_thread_.joinable())
    sub_thread_.join();
  connected_ = false;
#ifdef HAVE_REDIS
  if (ctx_) {
    redisFree(ctx_);
    ctx_ = nullptr;
  }
#endif
}

void RedisSubscriber::SubscribeLoop() {
#ifndef HAVE_REDIS
  return;
#else
  while (running_) {
    // Redis 연결 (재시도 루프)
    ctx_ = redisConnect(host_.c_str(), port_);
    if (!ctx_ || ctx_->err) {
      std::cerr << "[RedisSubscriber] 재연결 실패, 5초 후 재시도\n";
      if (ctx_) {
        redisFree(ctx_);
        ctx_ = nullptr;
      }
      for (int i = 0; i < 50 && running_; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    // 수신 타임아웃 설정 (1초마다 running_ 체크 가능하도록)
    struct timeval tv = {1, 0};
    redisSetTimeout(ctx_, tv);

    // 구독 채널: "point:update" (Collector가 발행)
    redisReply *reply =
        static_cast<redisReply *>(redisCommand(ctx_, "SUBSCRIBE point:update"));
    if (!reply) {
      redisFree(ctx_);
      ctx_ = nullptr;
      continue;
    }
    freeReplyObject(reply);
    connected_ = true;
    std::cout << "[RedisSubscriber] SUBSCRIBE point:update 완료\n";

    // 메시지 수신 루프
    while (running_) {
      redisReply *msg = nullptr;
      int rc = redisGetReply(ctx_, reinterpret_cast<void **>(&msg));

      if (rc == REDIS_ERR) {
        // 타임아웃 또는 연결 끊김
        if (ctx_->err == REDIS_ERR_IO)
          continue; // 타임아웃은 정상이므로 계속
        break;      // 진짜 에러면 재연결
      }

      if (msg && msg->type == REDIS_REPLY_ARRAY && msg->elements == 3) {
        // elements[0] = "message", [1] = channel, [2] = payload
        if (msg->element[2] && msg->element[2]->str) {
          OnMessage(std::string(msg->element[2]->str, msg->element[2]->len));
        }
      }
      if (msg)
        freeReplyObject(msg);
    }

    connected_ = false;
    if (ctx_) {
      redisFree(ctx_);
      ctx_ = nullptr;
    }
  }
#endif
}

void RedisSubscriber::OnMessage(const std::string &payload) {
#ifdef HAS_NLOHMANN_JSON
  try {
    auto j = json::parse(payload);
    int point_id = j.value("point_id", 0);
    double value = j.value("value", 0.0);
    // std::string quality = j.value("quality", "GOOD");

    auto it = point_map_.find(point_id);
    if (it != point_map_.end() && it->second->enabled) {
      WriteValueToTable(*it->second, value);
    }
  } catch (...) {
    // 파싱 실패는 무시
  }
#endif
}

void RedisSubscriber::WriteValueToTable(const RegisterMapping &m,
                                        double value) {
  // 스케일링: register_raw = (value * scale_factor) + scale_offset
  // SCADA Master가 역산: value = (raw - offset) / factor
  double raw = value;
  if (m.scale_factor != 0.0)
    raw = value * m.scale_factor + m.scale_offset;

  switch (m.register_type) {
  case RegisterType::HOLDING_REGISTER: {
    switch (m.data_type) {
    case DataType::FLOAT32: {
      float fv = static_cast<float>(raw);
      uint32_t bits;
      std::memcpy(&bits, &fv, 4);
      uint16_t w0, w1;
      if (m.big_endian) {
        w0 = static_cast<uint16_t>((bits >> 16) & 0xFFFF); // MSW
        w1 = static_cast<uint16_t>(bits & 0xFFFF);         // LSW
      } else {
        w0 = static_cast<uint16_t>(bits & 0xFFFF);         // LSW
        w1 = static_cast<uint16_t>((bits >> 16) & 0xFFFF); // MSW
      }
      if (m.word_swap)
        std::swap(w0, w1);
      table_->SetHoldingRegister(m.address, w0);
      table_->SetHoldingRegister(m.address + 1, w1);
      break;
    }
    case DataType::FLOAT64: {
      double v = raw;
      uint64_t bits;
      std::memcpy(&bits, &v, 8);
      uint16_t w[4];
      if (m.big_endian) {
        w[0] = static_cast<uint16_t>((bits >> 48) & 0xFFFF);
        w[1] = static_cast<uint16_t>((bits >> 32) & 0xFFFF);
        w[2] = static_cast<uint16_t>((bits >> 16) & 0xFFFF);
        w[3] = static_cast<uint16_t>(bits & 0xFFFF);
      } else {
        w[0] = static_cast<uint16_t>(bits & 0xFFFF);
        w[1] = static_cast<uint16_t>((bits >> 16) & 0xFFFF);
        w[2] = static_cast<uint16_t>((bits >> 32) & 0xFFFF);
        w[3] = static_cast<uint16_t>((bits >> 48) & 0xFFFF);
      }
      if (m.word_swap) {
        std::swap(w[0], w[1]);
        std::swap(w[2], w[3]);
      }
      for (int i = 0; i < 4; i++)
        table_->SetHoldingRegister(m.address + i, w[i]);
      break;
    }
    case DataType::INT32: {
      int32_t iv = static_cast<int32_t>(raw);
      uint32_t bits;
      std::memcpy(&bits, &iv, 4);
      uint16_t w0 = m.big_endian ? static_cast<uint16_t>((bits >> 16) & 0xFFFF)
                                 : static_cast<uint16_t>(bits & 0xFFFF);
      uint16_t w1 = m.big_endian ? static_cast<uint16_t>(bits & 0xFFFF)
                                 : static_cast<uint16_t>((bits >> 16) & 0xFFFF);
      if (m.word_swap)
        std::swap(w0, w1);
      table_->SetHoldingRegister(m.address, w0);
      table_->SetHoldingRegister(m.address + 1, w1);
      break;
    }
    case DataType::UINT32: {
      uint32_t uv = static_cast<uint32_t>(raw);
      uint16_t w0 = m.big_endian ? static_cast<uint16_t>((uv >> 16) & 0xFFFF)
                                 : static_cast<uint16_t>(uv & 0xFFFF);
      uint16_t w1 = m.big_endian ? static_cast<uint16_t>(uv & 0xFFFF)
                                 : static_cast<uint16_t>((uv >> 16) & 0xFFFF);
      if (m.word_swap)
        std::swap(w0, w1);
      table_->SetHoldingRegister(m.address, w0);
      table_->SetHoldingRegister(m.address + 1, w1);
      break;
    }
    case DataType::INT16:
      table_->SetHoldingRegister(
          m.address, static_cast<uint16_t>(static_cast<int16_t>(raw)));
      break;
    case DataType::UINT16:
    default:
      table_->SetHoldingRegister(m.address, static_cast<uint16_t>(raw));
      break;
    }
    break;
  }
  case RegisterType::INPUT_REGISTER: {
    switch (m.data_type) {
    case DataType::FLOAT32: {
      float fv = static_cast<float>(raw);
      uint32_t bits;
      std::memcpy(&bits, &fv, 4);
      uint16_t w0 = m.big_endian ? static_cast<uint16_t>((bits >> 16) & 0xFFFF)
                                 : static_cast<uint16_t>(bits & 0xFFFF);
      uint16_t w1 = m.big_endian ? static_cast<uint16_t>(bits & 0xFFFF)
                                 : static_cast<uint16_t>((bits >> 16) & 0xFFFF);
      if (m.word_swap)
        std::swap(w0, w1);
      table_->SetInputRegister(m.address, w0);
      table_->SetInputRegister(m.address + 1, w1);
      break;
    }
    case DataType::INT16:
    case DataType::UINT16:
    default:
      table_->SetInputRegister(m.address, static_cast<uint16_t>(raw));
      break;
    }
    break;
  }
  case RegisterType::COIL:
    table_->SetCoil(m.address, raw != 0.0);
    break;
  case RegisterType::DISCRETE_INPUT:
    table_->SetDiscreteInput(m.address, raw != 0.0);
    break;
  }
}

} // namespace ModbusSlave
} // namespace PulseOne
