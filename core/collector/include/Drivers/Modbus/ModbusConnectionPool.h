// =============================================================================
// collector/include/Drivers/Modbus/ModbusConnectionPool.h
// 실제 병렬 Modbus TCP 연결 풀
//
// 설정:
//   parallel_connections = 1  → 순차 읽기 (기본, 단일세션 장비)
//   parallel_connections = N  → N개 TCP 세션 병렬 읽기 (멀티세션 지원 장비)
// =============================================================================

#ifndef PULSEONE_MODBUS_CONNECTION_POOL_H
#define PULSEONE_MODBUS_CONNECTION_POOL_H

#include "Common/Structs.h"
#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// modbus.h를 헤더에서 직접 include하면 ERROR 매크로가 LogTypes.hpp와 충돌
// → forward declaration만 선언하고 .cpp에서만 modbus.h include
typedef struct _modbus modbus_t;

namespace PulseOne {
namespace Drivers {

class ModbusDriver;

// ---------------------------------------------------------------------------
// 연결 풀 통계
// ---------------------------------------------------------------------------
struct ConnectionPoolStats {
  size_t total_connections = 0;
  size_t active_connections = 0;
  size_t idle_connections = 0;
  double average_load = 0.0;
  uint64_t total_requests = 0;
  uint64_t pool_hits = 0;
  uint64_t pool_misses = 0;
};

// ---------------------------------------------------------------------------
// ModbusConnectionPool
// ---------------------------------------------------------------------------
class ModbusConnectionPool {
public:
  explicit ModbusConnectionPool(ModbusDriver *parent_driver);
  ~ModbusConnectionPool();

  ModbusConnectionPool(const ModbusConnectionPool &) = delete;
  ModbusConnectionPool &operator=(const ModbusConnectionPool &) = delete;

  // -----------------------------------------------------------------------
  // 풀 활성화 / 비활성화
  //   pool_size       : 병렬 TCP 세션 수 (1 = 순차)
  //   timeout_seconds : 연결 타임아웃
  // -----------------------------------------------------------------------
  bool EnableConnectionPooling(size_t pool_size = 1, int timeout_seconds = 5);
  void DisableConnectionPooling();
  bool IsEnabled() const { return enabled_.load(); }

  // AutoScaling — 현재 미구현 (인터페이스 유지)
  bool EnableAutoScaling(double load_threshold = 0.8,
                         size_t max_connections = 20);
  void DisableAutoScaling();
  bool IsAutoScalingEnabled() const { return auto_scaling_enabled_.load(); }

  ConnectionPoolStats GetConnectionPoolStats() const;

  // -----------------------------------------------------------------------
  // 병렬 배치 읽기
  //   pool_size == 1 이면 parent_driver_->ReadValuesImpl() 위임 (순차)
  //   pool_size  > 1 이면 포인트를 N청크로 분할해 N개 스레드 병렬 처리
  // -----------------------------------------------------------------------
  bool PerformBatchRead(const std::vector<Structs::DataPoint> &points,
                        std::vector<Structs::TimestampedValue> &values);

  bool PerformWrite(const Structs::DataPoint &point,
                    const Structs::DataValue &value);

private:
  // -----------------------------------------------------------------------
  // 내부: 단일 modbus_t* 컨텍스트로 청크 하나를 읽는 워커
  // -----------------------------------------------------------------------
  struct PooledContext {
    modbus_t *ctx = nullptr;
    std::mutex mtx; // 이 세션 전용 락
    bool connected = false;
  };

  bool ConnectContext(PooledContext &pc) const;
  void DisconnectContext(PooledContext &pc) const;

  // 청크 단위 읽기 (한 세션에서 실행)
  bool ReadChunk(PooledContext &pc,
                 const std::vector<Structs::DataPoint> &chunk,
                 std::vector<Structs::TimestampedValue> &out) const;

  // -----------------------------------------------------------------------
  // 멤버
  // -----------------------------------------------------------------------
  ModbusDriver *parent_driver_;
  std::atomic<bool> enabled_{false};
  std::atomic<bool> auto_scaling_enabled_{false};

  std::vector<std::unique_ptr<PooledContext>> pool_;
  mutable std::mutex pool_mutex_;

  size_t pool_size_ = 1;
  int timeout_sec_ = 5;

  // 통계
  mutable std::atomic<uint64_t> stat_total_requests_{0};
  mutable std::atomic<uint64_t> stat_pool_hits_{0};
  mutable std::atomic<uint64_t> stat_pool_misses_{0};
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_MODBUS_CONNECTION_POOL_H