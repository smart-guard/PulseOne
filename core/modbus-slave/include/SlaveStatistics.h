// =============================================================================
// core/modbus-slave/include/SlaveStatistics.h
// 서비스 전체 통신 통계 (슬라이딩 윈도우 + 누적)
// =============================================================================
#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>

namespace PulseOne {
namespace ModbusSlave {

// Function Code별 카운터
struct FcCounters {
  std::atomic<uint64_t> fc01{0}; // Read Coils
  std::atomic<uint64_t> fc02{0}; // Read Discrete Inputs
  std::atomic<uint64_t> fc03{0}; // Read Holding Registers (가장 많음)
  std::atomic<uint64_t> fc04{0}; // Read Input Registers
  std::atomic<uint64_t> fc05{0}; // Write Single Coil
  std::atomic<uint64_t> fc06{0}; // Write Single Register
  std::atomic<uint64_t> fc15{0}; // Write Multiple Coils
  std::atomic<uint64_t> fc16{0}; // Write Multiple Registers
  std::atomic<uint64_t> unknown{0};

  void Increment(uint8_t fc);
  uint64_t Total() const;
};

// 1분 단위 슬라이딩 윈도우 버킷 (60개 = 60분)
struct MinuteBucket {
  uint64_t requests = 0;
  uint64_t failures = 0;
  uint64_t sum_us = 0;   // 응답시간 합계 (마이크로초)
  uint32_t count_us = 0; // 응답시간 샘플 수
};

class SlaveStatistics {
public:
  SlaveStatistics();

  // 요청 1건 기록
  void Record(uint8_t func_code, bool success, uint64_t response_us);

  // === 조회 ===

  // 누적 통계
  uint64_t TotalRequests() const { return total_requests_.load(); }
  uint64_t TotalFailures() const { return total_failures_.load(); }
  uint64_t TotalSuccesses() const;
  double OverallSuccessRate() const;

  // 응답 시간
  double AvgResponseUs() const;
  uint64_t MaxResponseUs() const { return max_response_us_.load(); }
  uint64_t MinResponseUs() const { return min_response_us_.load(); }

  // FC별 카운터
  const FcCounters &GetFcCounters() const { return fc_counters_; }

  // 최근 N분 통계 (N <= 60)
  struct WindowStats {
    uint64_t requests;
    uint64_t failures;
    double avg_response_us;
    double success_rate;
    double req_per_minute; // RPS 역할
  };
  WindowStats GetWindowStats(int minutes = 5) const;

  // 서비스 가동 시간 (초)
  int64_t UptimeSeconds() const;

  // JSON 직렬화
  std::string ToJson() const;

  // 통계 초기화
  void Reset();

private:
  // 누적
  std::atomic<uint64_t> total_requests_{0};
  std::atomic<uint64_t> total_failures_{0};
  std::atomic<uint64_t> total_response_us_{0}; // 응답 시간 합계
  std::atomic<uint64_t> response_sample_count_{0};
  std::atomic<uint64_t> max_response_us_{0};
  std::atomic<uint64_t> min_response_us_{UINT64_MAX};

  FcCounters fc_counters_;

  // 슬라이딩 윈도우 (60분, 1분 버킷)
  mutable std::mutex window_mutex_;
  std::array<MinuteBucket, 60> window_buckets_{};
  int current_bucket_ = 0;
  std::chrono::steady_clock::time_point bucket_start_;

  void AdvanceBucketIfNeeded();

  // 서비스 시작 시각
  std::chrono::system_clock::time_point start_time_;
};

} // namespace ModbusSlave
} // namespace PulseOne
