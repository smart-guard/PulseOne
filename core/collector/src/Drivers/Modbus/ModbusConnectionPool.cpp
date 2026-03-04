// =============================================================================
// collector/src/Drivers/Modbus/ModbusConnectionPool.cpp
//
// 실제 병렬 Modbus TCP 연결 풀 구현
//
// parallel_connections 설정값에 따른 동작:
//   1 (기본) → 순차: parent_driver_->ReadValuesImpl() 그대로 위임
//   N > 1    → 포인트를 N청크로 분할, N개 스레드가 각자 TCP 세션으로 병렬 읽기
//
// 장치 설정 예시 (DriverConfig.properties):
//   "parallel_connections" = "4"
// =============================================================================

#include "Drivers/Modbus/ModbusConnectionPool.h"
#include "Drivers/Modbus/ModbusDriver.h"

#include <algorithm>
#include <future>
#include <modbus/modbus.h>

namespace PulseOne {
namespace Drivers {

using DataPoint = Structs::DataPoint;
using TimestampedValue = Structs::TimestampedValue;

// =============================================================================
// 생성자 / 소멸자
// =============================================================================

ModbusConnectionPool::ModbusConnectionPool(ModbusDriver *parent_driver)
    : parent_driver_(parent_driver) {}

ModbusConnectionPool::~ModbusConnectionPool() { DisableConnectionPooling(); }

// =============================================================================
// EnableConnectionPooling
//   pool_size == 1 → 순차 모드 (풀 안 만듦)
//   pool_size  > 1 → pool_size개 modbus_t* 컨텍스트 생성 및 연결
// =============================================================================
bool ModbusConnectionPool::EnableConnectionPooling(size_t pool_size,
                                                   int timeout_seconds) {
  std::lock_guard<std::mutex> lk(pool_mutex_);

  pool_size_ = (pool_size < 1) ? 1 : pool_size;
  timeout_sec_ = timeout_seconds;

  // 순차 모드: 풀 불필요
  if (pool_size_ == 1) {
    enabled_.store(true);
    if (parent_driver_ && parent_driver_->logger_)
      parent_driver_->logger_->Info(
          "[ConnectionPool] pool_size=1 → 순차 모드 (풀 비생성)");
    return true;
  }

  if (!parent_driver_)
    return false;

  const auto &cfg = parent_driver_->config_; // friend class 접근

  // 기존 풀 정리
  for (auto &pc : pool_) {
    DisconnectContext(*pc);
    if (pc->ctx) {
      modbus_free(pc->ctx);
      pc->ctx = nullptr;
    }
  }
  pool_.clear();

  // pool_size개 컨텍스트 생성
  bool any_ok = false;
  for (size_t i = 0; i < pool_size_; ++i) {
    auto pc = std::make_unique<PooledContext>();

    // TCP 컨텍스트 생성 (RTU는 멀티세션 의미 없으므로 TCP 전용)
    std::string host;
    int port = 502;
    auto colon = cfg.endpoint.rfind(':');
    if (colon != std::string::npos) {
      host = cfg.endpoint.substr(0, colon);
      try {
        port = std::stoi(cfg.endpoint.substr(colon + 1));
      } catch (...) {
        port = 502;
      }
    } else {
      host = cfg.endpoint;
    }

    pc->ctx = modbus_new_tcp(host.c_str(), port);
    if (!pc->ctx) {
      if (parent_driver_ && parent_driver_->logger_)
        parent_driver_->logger_->Warn(
            "[ConnectionPool] 세션 " + std::to_string(i) +
            " 컨텍스트 생성 실패: " + host + ":" + std::to_string(port));
      continue;
    }

    // 타임아웃 설정
    uint32_t to_sec = static_cast<uint32_t>(timeout_sec_);
    uint32_t to_usec = 0;
    modbus_set_response_timeout(pc->ctx, to_sec, to_usec);

    // slave_id
    int slave_id = 1;
    if (cfg.properties.count("slave_id")) {
      try {
        slave_id = std::stoi(cfg.properties.at("slave_id"));
      } catch (...) {
      }
    }
    modbus_set_slave(pc->ctx, slave_id);

    if (ConnectContext(*pc)) {
      any_ok = true;
      if (parent_driver_ && parent_driver_->logger_)
        parent_driver_->logger_->Info("[ConnectionPool] 세션 " +
                                      std::to_string(i) + " 연결 완료 → " +
                                      host + ":" + std::to_string(port));
    } else {
      if (parent_driver_ && parent_driver_->logger_)
        parent_driver_->logger_->Warn("[ConnectionPool] 세션 " +
                                      std::to_string(i) +
                                      " 연결 실패 (폴링 시 재시도)");
    }

    pool_.push_back(std::move(pc));
  }

  enabled_.store(true);
  if (parent_driver_ && parent_driver_->logger_)
    parent_driver_->logger_->Info(
        "[ConnectionPool] 활성화: pool_size=" + std::to_string(pool_size_) +
        ", 연결 성공=" + std::to_string(pool_.size()));
  return any_ok || pool_.empty(); // 컨텍스트 생성만 돼도 OK
}

void ModbusConnectionPool::DisableConnectionPooling() {
  std::lock_guard<std::mutex> lk(pool_mutex_);
  for (auto &pc : pool_) {
    DisconnectContext(*pc);
    if (pc->ctx) {
      modbus_free(pc->ctx);
      pc->ctx = nullptr;
    }
  }
  pool_.clear();
  enabled_.store(false);
  auto_scaling_enabled_.store(false);
}

bool ModbusConnectionPool::EnableAutoScaling(double /*load_threshold*/,
                                             size_t /*max_connections*/) {
  if (!enabled_.load())
    return false;
  auto_scaling_enabled_.store(true);
  return true;
}

void ModbusConnectionPool::DisableAutoScaling() {
  auto_scaling_enabled_.store(false);
}

ConnectionPoolStats ModbusConnectionPool::GetConnectionPoolStats() const {
  ConnectionPoolStats s;
  std::lock_guard<std::mutex> lk(pool_mutex_);
  s.total_connections = pool_.size();
  s.active_connections = pool_.size(); // 단순화
  s.total_requests = stat_total_requests_.load();
  s.pool_hits = stat_pool_hits_.load();
  s.pool_misses = stat_pool_misses_.load();
  s.average_load = pool_.empty() ? 0.0
                                 : static_cast<double>(s.active_connections) /
                                       s.total_connections;
  return s;
}

// =============================================================================
// PerformBatchRead  — 핵심 병렬 읽기
// =============================================================================
bool ModbusConnectionPool::PerformBatchRead(
    const std::vector<DataPoint> &points,
    std::vector<TimestampedValue> &values) {

  if (!enabled_.load() || !parent_driver_)
    return false;

  ++stat_total_requests_;

  // ── [1] 순차 모드 (pool_size == 1) ─────────────────────────────────────
  if (pool_size_ <= 1 || pool_.empty()) {
    ++stat_pool_misses_;
    return parent_driver_->ReadValuesImpl(points, values);
  }

  // ── [2] 병렬 모드: pool_mutex_ 보호 하에 포인터 스냅샷 복사 ────────────
  ++stat_pool_hits_;

  // pool_mutex_ 보호 하에 pool_ 메타데이터 읽기
  // (DisableConnectionPooling과의 동시 접근 방지)
  std::vector<PooledContext *> ctx_snapshot;
  {
    std::lock_guard<std::mutex> lk(pool_mutex_);
    if (pool_.empty())
      return false;
    ctx_snapshot.reserve(pool_.size());
    for (auto &pc : pool_)
      ctx_snapshot.push_back(pc.get());
  }

  const size_t N = ctx_snapshot.size();
  const size_t total = points.size();
  const size_t chunk = (total + N - 1) / N; // 올림 나누기

  // 각 청크에 대한 future 결과
  struct ChunkResult {
    std::vector<TimestampedValue> vals;
    bool success = false;
  };

  std::vector<std::future<ChunkResult>> futures;
  futures.reserve(N);

  for (size_t i = 0; i < N; ++i) {
    size_t begin = i * chunk;
    if (begin >= total)
      break;
    size_t end = std::min(begin + chunk, total);

    std::vector<DataPoint> sub(points.begin() + begin, points.begin() + end);

    PooledContext *pc = ctx_snapshot[i];

    futures.push_back(
        std::async(std::launch::async,
                   [this, pc, sub = std::move(sub)]() mutable -> ChunkResult {
                     ChunkResult r;
                     r.success = ReadChunk(*pc, sub, r.vals);
                     return r;
                   }));
  }

  // 결과 수집
  values.clear();
  bool any_ok = false;
  size_t fut_idx = 0;
  for (auto &fut : futures) {
    auto r = fut.get();
    if (r.success) {
      values.insert(values.end(), r.vals.begin(), r.vals.end());
      any_ok = true;
    } else {
      // Bug32 Fix: 실패 청크 포인트를 BAD 품질로 결과에 추가 (누락 방지)
      size_t begin = fut_idx * chunk;
      size_t end_idx = std::min(begin + chunk, total);
      for (size_t pi = begin; pi < end_idx; ++pi) {
        TimestampedValue tv;
        try { tv.point_id = std::stoi(points[pi].id); } catch (...) { tv.point_id = 0; }
        tv.source = points[pi].name;
        tv.timestamp = std::chrono::system_clock::now();
        tv.quality = DataQuality::BAD;
        values.push_back(tv);
      }
    }
    ++fut_idx;
  }

  return any_ok;
}

bool ModbusConnectionPool::PerformWrite(const DataPoint &point,
                                        const DataValue &value) {
  if (!enabled_.load() || !parent_driver_)
    return false;
  // Write는 항상 주 드라이버(순차)로 처리
  return parent_driver_->WriteValueImpl(point, value);
}

// =============================================================================
// 내부 헬퍼
// =============================================================================

bool ModbusConnectionPool::ConnectContext(PooledContext &pc) const {
  if (!pc.ctx)
    return false;
  if (pc.connected)
    return true;
  if (modbus_connect(pc.ctx) == 0) {
    pc.connected = true;
    return true;
  }
  return false;
}

void ModbusConnectionPool::DisconnectContext(PooledContext &pc) const {
  if (pc.ctx && pc.connected) {
    modbus_close(pc.ctx);
    pc.connected = false;
  }
}

// 단일 세션(pc.ctx)으로 포인트 청크를 직접 읽기
// ── ctx swap 없이 pc.ctx를 독자적으로 사용 ──────────────────────────────────
bool ModbusConnectionPool::ReadChunk(PooledContext &pc,
                                     const std::vector<DataPoint> &chunk,
                                     std::vector<TimestampedValue> &out) const {
  std::lock_guard<std::mutex> lk(pc.mtx);

  // 연결 보장
  if (!pc.connected) {
    if (!ConnectContext(pc)) {
      if (parent_driver_ && parent_driver_->logger_)
        parent_driver_->logger_->Warn(
            "[ConnectionPool] ReadChunk: 세션 재연결 실패");
      return false;
    }
  }

  // ── 포인트를 SlaveID+FuncType 기준으로 그룹화 ─────────────────────────────
  // (ReadValuesImpl 로직과 동일하되 pc.ctx를 직접 사용)
  struct Group {
    int slave_id;
    std::string func_type;
    std::vector<const DataPoint *> pts;
  };
  std::map<std::pair<int, std::string>, Group> groups;

  // Bug27 Fix: current_slave_id_ is non-atomic; read via GetSlaveId() which
  // is still non-atomic but at least documents intent. For pool (async
  // threads), each DataPoint carries its own slave_id in protocol_params — fall
  // back to 1.
  int default_slave = 1;
  if (parent_driver_ && parent_driver_->GetSlaveId() != -1)
    default_slave = parent_driver_->GetSlaveId();
  for (const auto &p : chunk) {
    int sid = default_slave;
    if (p.protocol_params.count("slave_id")) {
      try {
        sid = std::stoi(p.protocol_params.at("slave_id"));
      } catch (...) {
      }
    }
    std::string ft = "HOLDING_REGISTER";
    if (!p.address_string.empty() && p.address_string.size() > 2) {
      auto pfx = p.address_string.substr(0, 3);
      std::transform(pfx.begin(), pfx.end(), pfx.begin(), ::toupper);
      if (pfx == "CO:")
        ft = "COIL";
      else if (pfx == "DI:")
        ft = "DISCRETE_INPUT";
      else if (pfx == "HR:")
        ft = "HOLDING_REGISTER";
      else if (pfx == "IR:")
        ft = "INPUT_REGISTER";
    }
    if (p.protocol_params.count("function_code")) {
      try {
        int fc = std::stoi(p.protocol_params.at("function_code"));
        if (fc == 1 || fc == 5)
          ft = "COIL";
        else if (fc == 2)
          ft = "DISCRETE_INPUT";
        else if (fc == 3 || fc == 6 || fc == 16)
          ft = "HOLDING_REGISTER";
        else if (fc == 4)
          ft = "INPUT_REGISTER";
      } catch (...) {
      } // stoi 파싱 실패 시 address_string 결과 유지
    }
    auto &g = groups[{sid, ft}];
    g.slave_id = sid;
    g.func_type = ft;
    g.pts.push_back(&p);
  }

  bool any_ok = false;
  const uint16_t MAX_REGS = 120;

  for (auto &[key, g] : groups) {
    // 주소 순 정렬
    std::sort(g.pts.begin(), g.pts.end(),
              [](const DataPoint *a, const DataPoint *b) {
                return a->address < b->address;
              });

    // SlaveID 설정 (이 session ctx에만 적용)
    modbus_set_slave(pc.ctx, g.slave_id);

    // 청킹
    size_t idx = 0;
    while (idx < g.pts.size()) {
      // Bug28: address는 uint32_t; 65535 초과 시 절단 방지
      if (g.pts[idx]->address > 0xFFFF) {
        idx++;
        continue;
      }
      uint16_t start = static_cast<uint16_t>(g.pts[idx]->address);
      uint16_t end = start;
      size_t nxt = idx;

      std::vector<const DataPoint *> cpts;
      while (nxt < g.pts.size()) {
        const auto *p = g.pts[nxt];
        uint16_t rc = (p->data_type == "FLOAT32" || p->data_type == "INT32" ||
                       p->data_type == "UINT32")
                          ? 2
                          : 1;
        if (p->address > static_cast<uint32_t>(end + 10))
          break;
        if ((p->address + rc) - start > MAX_REGS)
          break;
        cpts.push_back(p);
        end = std::max<uint16_t>(end, p->address + rc);
        nxt++;
      }

      uint16_t cnt = (end > start) ? (end - start) : 1;
      bool ok = false;

      // ── byte_order 확인 (ConvertRegistersToValue와 동일 기준) ─────────
      std::string byte_order = "big_endian";
      if (parent_driver_ &&
          parent_driver_->config_.properties.count("byte_order"))
        byte_order = parent_driver_->config_.properties.at("byte_order");
      bool swap_words =
          (byte_order == "swapped" || byte_order == "big_endian_swapped" ||
           byte_order == "little_endian");

      // ── 실제 libmodbus 호출 (pc.ctx 직접 사용, ctx swap 없음) ──────────
      if (g.func_type == "HOLDING_REGISTER") {
        std::vector<uint16_t> buf(cnt);
        ok = (modbus_read_registers(pc.ctx, start, cnt, buf.data()) == cnt);
        if (ok) {
          for (const auto *p : cpts) {
            TimestampedValue tv;
            try {
              tv.point_id = std::stoi(p->id);
            } catch (...) {
              tv.point_id = 0;
            }
            tv.source = p->name;
            tv.timestamp = std::chrono::system_clock::now();
            tv.quality = DataQuality::GOOD;
            bool pt_swap = swap_words;
            if (p->protocol_params.count("byte_order")) {
              const auto &bo = p->protocol_params.at("byte_order");
              pt_swap = (bo == "swapped" || bo == "big_endian_swapped" ||
                         bo == "little_endian");
            }
            uint16_t off = p->address - start;
            if (static_cast<size_t>(off) < buf.size()) {
              uint16_t reg = buf[off];
              // ── bit_start/bit_end: 비트 묶음 추출 ──────────────────────
              if (p->protocol_params.count("bit_start") &&
                  p->protocol_params.count("bit_end")) {
                try {
                  int bs = std::stoi(p->protocol_params.at("bit_start"));
                  int be = std::stoi(p->protocol_params.at("bit_end"));
                  if (bs >= 0 && be < 16 && bs <= be) {
                    uint16_t mask = static_cast<uint16_t>(((1 << (be - bs + 1)) - 1) << bs);
                    uint16_t extracted = (reg & mask) >> bs;
                    tv.value = static_cast<double>(extracted) *
                               p->scaling_factor + p->scaling_offset;
                  }
                } catch (...) {}
              }
              // ── bit_index: 단일 비트 추출 ────────────────────────────────
              else if (p->protocol_params.count("bit_index")) {
                try {
                  int bi = std::stoi(p->protocol_params.at("bit_index"));
                  if (bi >= 0 && bi < 16) {
                    bool bv = (reg & (1 << bi)) != 0;
                    if (p->data_type == "BOOL" || p->data_type == "COIL" ||
                        p->data_type == "bool")
                      tv.value = bv;
                    else
                      tv.value = (bv ? 1.0 : 0.0) * p->scaling_factor +
                                 p->scaling_offset;
                  }
                } catch (...) {}
              }
              // ── 일반 레지스터 타입 ─────────────────────────────────────
              else if (p->data_type == "FLOAT32" || p->data_type == "INT32" ||
                  p->data_type == "UINT32") {
                if (static_cast<size_t>(off + 1) < buf.size()) {
                  uint32_t v32 =
                      pt_swap ? ((uint32_t)buf[off + 1] << 16) | buf[off]
                              : ((uint32_t)buf[off] << 16) | buf[off + 1];
                  if (p->data_type == "FLOAT32") {
                    float f;
                    memcpy(&f, &v32, 4);
                    tv.value =
                        (double)f * p->scaling_factor + p->scaling_offset;
                  } else if (p->data_type == "INT32")
                    tv.value = (double)(int32_t)v32 * p->scaling_factor +
                               p->scaling_offset;
                  else
                    tv.value =
                        (double)v32 * p->scaling_factor + p->scaling_offset;
                }
              } else if (p->data_type == "INT16")
                tv.value = (double)(int16_t)reg * p->scaling_factor +
                           p->scaling_offset;
              else
                tv.value = (double)reg * p->scaling_factor + p->scaling_offset;
            }
            out.push_back(tv);
          }
        } else {
          // Bug34 Fix: mini-chunk 읽기 실패 시 해당 포인트들을 BAD quality로 추가
          for (const auto *p : cpts) {
            TimestampedValue tv;
            try { tv.point_id = std::stoi(p->id); } catch (...) { tv.point_id = 0; }
            tv.source = p->name;
            tv.timestamp = std::chrono::system_clock::now();
            tv.quality = DataQuality::BAD;
            out.push_back(tv);
          }
        }
      } else if (g.func_type == "INPUT_REGISTER") {
        std::vector<uint16_t> buf(cnt);
        ok = (modbus_read_input_registers(pc.ctx, start, cnt, buf.data()) ==
              cnt);
        if (ok) {
          for (const auto *p : cpts) {
            TimestampedValue tv;
            try {
              tv.point_id = std::stoi(p->id);
            } catch (...) {
              tv.point_id = 0;
            }
            tv.source = p->name;
            tv.timestamp = std::chrono::system_clock::now();
            tv.quality = DataQuality::GOOD;
            bool pt_swap = swap_words;
            if (p->protocol_params.count("byte_order")) {
              const auto &bo = p->protocol_params.at("byte_order");
              pt_swap = (bo == "swapped" || bo == "big_endian_swapped" ||
                         bo == "little_endian");
            }
            uint16_t off = p->address - start;
            if (static_cast<size_t>(off) < buf.size()) {
              uint16_t reg = buf[off];
              if (p->protocol_params.count("bit_start") &&
                  p->protocol_params.count("bit_end")) {
                try {
                  int bs = std::stoi(p->protocol_params.at("bit_start"));
                  int be = std::stoi(p->protocol_params.at("bit_end"));
                  if (bs >= 0 && be < 16 && bs <= be) {
                    uint16_t mask = static_cast<uint16_t>(((1 << (be - bs + 1)) - 1) << bs);
                    uint16_t extracted = (reg & mask) >> bs;
                    tv.value = static_cast<double>(extracted) *
                               p->scaling_factor + p->scaling_offset;
                  }
                } catch (...) {}
              } else if (p->protocol_params.count("bit_index")) {
                try {
                  int bi = std::stoi(p->protocol_params.at("bit_index"));
                  if (bi >= 0 && bi < 16) {
                    bool bv = (reg & (1 << bi)) != 0;
                    if (p->data_type == "BOOL" || p->data_type == "COIL" ||
                        p->data_type == "bool")
                      tv.value = bv;
                    else
                      tv.value = (bv ? 1.0 : 0.0) * p->scaling_factor +
                                 p->scaling_offset;
                  }
                } catch (...) {}
              } else if (p->data_type == "FLOAT32" || p->data_type == "INT32" ||
                  p->data_type == "UINT32") {
                if (static_cast<size_t>(off + 1) < buf.size()) {
                  uint32_t v32 =
                      pt_swap ? ((uint32_t)buf[off + 1] << 16) | buf[off]
                              : ((uint32_t)buf[off] << 16) | buf[off + 1];
                  if (p->data_type == "FLOAT32") {
                    float f;
                    memcpy(&f, &v32, 4);
                    tv.value =
                        (double)f * p->scaling_factor + p->scaling_offset;
                  } else if (p->data_type == "INT32")
                    tv.value = (double)(int32_t)v32 * p->scaling_factor +
                               p->scaling_offset;
                  else
                    tv.value =
                        (double)v32 * p->scaling_factor + p->scaling_offset;
                }
              } else if (p->data_type == "INT16")
                tv.value = (double)(int16_t)reg * p->scaling_factor +
                           p->scaling_offset;
              else
                tv.value = (double)reg * p->scaling_factor + p->scaling_offset;
            }
            out.push_back(tv);
          }
        } else {
          for (const auto *p : cpts) {
            TimestampedValue tv;
            try { tv.point_id = std::stoi(p->id); } catch (...) { tv.point_id = 0; }
            tv.source = p->name;
            tv.timestamp = std::chrono::system_clock::now();
            tv.quality = DataQuality::BAD;
            out.push_back(tv);
          }
        }
      } else if (g.func_type == "COIL") {
        std::vector<uint8_t> buf(cnt);
        ok = (modbus_read_bits(pc.ctx, start, cnt, buf.data()) == cnt);
        if (ok) {
          for (const auto *p : cpts) {
            TimestampedValue tv;
            try {
              tv.point_id = std::stoi(p->id);
            } catch (...) {
              tv.point_id = 0;
            }
            tv.source = p->name;
            tv.timestamp = std::chrono::system_clock::now();
            tv.quality = DataQuality::GOOD;
            uint16_t off = p->address - start;
            if (static_cast<size_t>(off) < buf.size())
              tv.value = (buf[off] != 0);
            out.push_back(tv);
          }
        } else {
          for (const auto *p : cpts) {
            TimestampedValue tv;
            try { tv.point_id = std::stoi(p->id); } catch (...) { tv.point_id = 0; }
            tv.source = p->name; tv.timestamp = std::chrono::system_clock::now();
            tv.quality = DataQuality::BAD; out.push_back(tv);
          }
        }
      } else { // DISCRETE_INPUT
        std::vector<uint8_t> buf(cnt);
        ok = (modbus_read_input_bits(pc.ctx, start, cnt, buf.data()) == cnt);
        if (ok) {
          for (const auto *p : cpts) {
            TimestampedValue tv;
            try {
              tv.point_id = std::stoi(p->id);
            } catch (...) {
              tv.point_id = 0;
            }
            tv.source = p->name;
            tv.timestamp = std::chrono::system_clock::now();
            tv.quality = DataQuality::GOOD;
            uint16_t off = p->address - start;
            if (static_cast<size_t>(off) < buf.size())
              tv.value = (buf[off] != 0);
            out.push_back(tv);
          }
        } else {
          for (const auto *p : cpts) {
            TimestampedValue tv;
            try { tv.point_id = std::stoi(p->id); } catch (...) { tv.point_id = 0; }
            tv.source = p->name; tv.timestamp = std::chrono::system_clock::now();
            tv.quality = DataQuality::BAD; out.push_back(tv);
          }
        }
      }

      if (!ok)
        pc.connected = false; // 이 세션 오류 표시
      else
        any_ok = true;

      idx = nxt;
    }
  }

  return any_ok;
}

} // namespace Drivers
} // namespace PulseOne
