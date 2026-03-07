// =============================================================================
// core/modbus-slave/src/SlaveStatistics.cpp
// =============================================================================
#include "SlaveStatistics.h"
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#ifdef HAVE_REDIS
#include <hiredis/hiredis.h>
#endif

namespace PulseOne {
namespace ModbusSlave {

// --- FcCounters
// ---------------------------------------------------------------

void FcCounters::Increment(uint8_t fc) {
  switch (fc) {
  case 0x01:
    fc01++;
    break;
  case 0x02:
    fc02++;
    break;
  case 0x03:
    fc03++;
    break;
  case 0x04:
    fc04++;
    break;
  case 0x05:
    fc05++;
    break;
  case 0x06:
    fc06++;
    break;
  case 0x0F:
    fc15++;
    break;
  case 0x10:
    fc16++;
    break;
  default:
    unknown++;
    break;
  }
}

uint64_t FcCounters::Total() const {
  return fc01 + fc02 + fc03 + fc04 + fc05 + fc06 + fc15 + fc16 + unknown;
}

// --- SlaveStatistics
// ----------------------------------------------------------

SlaveStatistics::SlaveStatistics()
    : start_time_(std::chrono::system_clock::now()),
      bucket_start_(std::chrono::steady_clock::now()) {
  window_buckets_.fill(MinuteBucket{});
}

void SlaveStatistics::AdvanceBucketIfNeeded() {
  auto now = std::chrono::steady_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::minutes>(now - bucket_start_)
          .count();
  if (elapsed >= 1) {
    int advance = static_cast<int>(elapsed);
    for (int i = 0; i < advance && i < 60; i++) {
      current_bucket_ = (current_bucket_ + 1) % 60;
      window_buckets_[current_bucket_] = MinuteBucket{};
    }
    bucket_start_ += std::chrono::minutes(advance);
  }
}

void SlaveStatistics::Record(uint8_t func_code, bool success,
                             uint64_t response_us) {
  // 누적
  total_requests_++;
  if (!success)
    total_failures_++;

  total_response_us_ += response_us;
  response_sample_count_++;

  // max/min (CAS 패턴)
  uint64_t cur_max = max_response_us_.load();
  while (response_us > cur_max &&
         !max_response_us_.compare_exchange_weak(cur_max, response_us)) {
  }

  uint64_t cur_min = min_response_us_.load();
  while (response_us < cur_min &&
         !min_response_us_.compare_exchange_weak(cur_min, response_us)) {
  }

  fc_counters_.Increment(func_code);

  // 슬라이딩 윈도우
  std::lock_guard<std::mutex> lock(window_mutex_);
  AdvanceBucketIfNeeded();
  auto &bucket = window_buckets_[current_bucket_];
  bucket.requests++;
  if (!success)
    bucket.failures++;
  bucket.sum_us += response_us;
  bucket.count_us++;
}

uint64_t SlaveStatistics::TotalSuccesses() const {
  return total_requests_.load() - total_failures_.load();
}

double SlaveStatistics::OverallSuccessRate() const {
  uint64_t total = total_requests_.load();
  if (total == 0)
    return 1.0;
  return static_cast<double>(TotalSuccesses()) / static_cast<double>(total);
}

double SlaveStatistics::AvgResponseUs() const {
  uint64_t count = response_sample_count_.load();
  if (count == 0)
    return 0.0;
  return static_cast<double>(total_response_us_.load()) /
         static_cast<double>(count);
}

SlaveStatistics::WindowStats
SlaveStatistics::GetWindowStats(int minutes) const {
  if (minutes < 1 || minutes > 60)
    minutes = 5;

  std::lock_guard<std::mutex> lock(window_mutex_);
  const_cast<SlaveStatistics *>(this)->AdvanceBucketIfNeeded();

  uint64_t req = 0, fail = 0, sum_us = 0;
  uint32_t cnt = 0;
  for (int i = 0; i < minutes; i++) {
    int idx = (current_bucket_ - i + 60) % 60;
    req += window_buckets_[idx].requests;
    fail += window_buckets_[idx].failures;
    sum_us += window_buckets_[idx].sum_us;
    cnt += window_buckets_[idx].count_us;
  }

  WindowStats ws;
  ws.requests = req;
  ws.failures = fail;
  ws.avg_response_us = cnt > 0 ? static_cast<double>(sum_us) / cnt : 0.0;
  ws.success_rate = req > 0 ? static_cast<double>(req - fail) / req : 1.0;
  ws.req_per_minute = static_cast<double>(req) / minutes;
  return ws;
}

int64_t SlaveStatistics::UptimeSeconds() const {
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::seconds>(now - start_time_)
      .count();
}

std::string SlaveStatistics::ToJson() const {
  auto ws5 = GetWindowStats(5);
  auto ws60 = GetWindowStats(60);

  std::ostringstream j;
  j << std::fixed << std::setprecision(4);
  j << "{"
    << "\"uptime_sec\":" << UptimeSeconds() << ","
    << "\"total_requests\":" << TotalRequests() << ","
    << "\"total_failures\":" << TotalFailures() << ","
    << "\"overall_success_rate\":" << OverallSuccessRate() << ","
    << "\"avg_response_us\":" << AvgResponseUs() << ","
    << "\"max_response_us\":" << MaxResponseUs() << ","
    << "\"min_response_us\":"
    << (MinResponseUs() == UINT64_MAX ? 0 : MinResponseUs()) << ","
    << "\"fc_counters\":{"
    << "\"fc01\":" << fc_counters_.fc01.load() << ","
    << "\"fc02\":" << fc_counters_.fc02.load() << ","
    << "\"fc03\":" << fc_counters_.fc03.load() << ","
    << "\"fc04\":" << fc_counters_.fc04.load() << ","
    << "\"fc05\":" << fc_counters_.fc05.load() << ","
    << "\"fc06\":" << fc_counters_.fc06.load() << ","
    << "\"fc15\":" << fc_counters_.fc15.load() << ","
    << "\"fc16\":" << fc_counters_.fc16.load() << "},"
    << "\"window_5min\":{"
    << "\"requests\":" << ws5.requests << ","
    << "\"failures\":" << ws5.failures << ","
    << "\"success_rate\":" << ws5.success_rate << ","
    << "\"avg_resp_us\":" << ws5.avg_response_us << ","
    << "\"req_per_min\":" << ws5.req_per_minute << "},"
    << "\"window_60min\":{"
    << "\"requests\":" << ws60.requests << ","
    << "\"failures\":" << ws60.failures << ","
    << "\"success_rate\":" << ws60.success_rate << ","
    << "\"req_per_min\":" << ws60.req_per_minute << "}"
    << "}";
  return j.str();
}

void SlaveStatistics::Reset() {
  total_requests_.store(0);
  total_failures_.store(0);
  total_response_us_.store(0);
  response_sample_count_.store(0);
  max_response_us_.store(0);
  min_response_us_.store(UINT64_MAX);
  std::lock_guard<std::mutex> lock(window_mutex_);
  window_buckets_.fill(MinuteBucket{});
}

void SlaveStatistics::PublishToRedis(const std::string &host, int port,
                                     int device_id) const {
#ifdef HAVE_REDIS
  // 별도 커넥션 생성(구독 컨텍스트와 분리)
  redisContext *rc = redisConnect(host.c_str(), port);
  if (!rc || rc->err) {
    if (rc)
      redisFree(rc);
    return;
  }
  std::string key = "modbus:stats:" + std::to_string(device_id);
  std::string json = ToJson();
  // SETEX key 120 value  (120초 TTL: Worker가 죽으면 자동 만료)
  redisReply *reply = static_cast<redisReply *>(
      redisCommand(rc, "SETEX %s 120 %s", key.c_str(), json.c_str()));
  if (reply)
    freeReplyObject(reply);
  redisFree(rc);
#else
  (void)host;
  (void)port;
  (void)device_id;
#endif
}

} // namespace ModbusSlave
} // namespace PulseOne
