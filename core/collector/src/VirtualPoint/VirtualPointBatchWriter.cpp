// =============================================================================
// collector/src/VirtualPoint/VirtualPointBatchWriter.cpp
// 🔥 컴파일 에러 수정 완성본
// =============================================================================

#include "VirtualPoint/VirtualPointBatchWriter.h"
#include "Common/Utils.h"
#include "Database/RuntimeSQLQueries.h"
using namespace PulseOne::Database::SQL;
#include "DatabaseManager.hpp"
#include "Logging/LogManager.h"
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace PulseOne {
namespace VirtualPoint {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

VirtualPointBatchWriter::VirtualPointBatchWriter(size_t batch_size,
                                                 int flush_interval_sec)
    : batch_size_(
          std::clamp(batch_size, static_cast<size_t>(1), MAX_BATCH_SIZE)),
      flush_interval_(std::chrono::seconds(
          std::clamp(flush_interval_sec, 1,
                     static_cast<int>(MAX_FLUSH_INTERVAL.count() / 1000)))),
      db_manager_(&DbLib::DatabaseManager::getInstance()) {

  LogManager::getInstance().log(
      "VirtualPointBatchWriter", LogLevel::INFO,
      "🔥 VirtualPointBatchWriter 생성 - 배치크기: " +
          std::to_string(batch_size_) +
          ", 플러시간격: " + std::to_string(flush_interval_.count()) + "초");
}

VirtualPointBatchWriter::~VirtualPointBatchWriter() {
  if (IsRunning()) {
    LogManager::getInstance().log("VirtualPointBatchWriter", LogLevel::WARN,
                                  "⚠️ 소멸자에서 자동 중지 수행");
    Stop();
  }

  LogManager::getInstance().log("VirtualPointBatchWriter", LogLevel::INFO,
                                "✅ VirtualPointBatchWriter 소멸 완료");
}

// =============================================================================
// 라이프사이클 관리
// =============================================================================

bool VirtualPointBatchWriter::Start() {
  if (IsRunning()) {
    LogManager::getInstance().log("VirtualPointBatchWriter", LogLevel::WARN,
                                  "⚠️ 이미 실행 중임");
    return true;
  }

  // DB 연결 확인
  if (!db_manager_) {
    LogManager::getInstance().log("VirtualPointBatchWriter",
                                  LogLevel::LOG_ERROR,
                                  "❌ DbLib::DatabaseManager가 null임");
    return false;
  }

  try {
    should_stop_.store(false);

    // 배치 처리 스레드 시작
    batch_writer_thread_ =
        std::thread(&VirtualPointBatchWriter::BatchWriterLoop, this);

    LogManager::getInstance().log("VirtualPointBatchWriter", LogLevel::INFO,
                                  "🚀 VirtualPointBatchWriter 시작 성공");

    return true;

  } catch (const std::exception &e) {
    LogManager::getInstance().log(
        "VirtualPointBatchWriter", LogLevel::LOG_ERROR,
        "❌ VirtualPointBatchWriter 시작 실패: " + std::string(e.what()));
    return false;
  }
}

void VirtualPointBatchWriter::Stop() {
  if (!IsRunning()) {
    return;
  }

  LogManager::getInstance().log("VirtualPointBatchWriter", LogLevel::INFO,
                                "🛑 VirtualPointBatchWriter 중지 시작...");

  // 중지 신호
  should_stop_.store(true);
  cv_.notify_all();

  // 스레드 종료 대기
  if (batch_writer_thread_.joinable()) {
    batch_writer_thread_.join();
  }

  // 남은 데이터 처리
  std::lock_guard<std::mutex> lock(queue_mutex_);
  if (!pending_results_.empty()) {
    LogManager::getInstance().log("VirtualPointBatchWriter", LogLevel::INFO,
                                  "📦 남은 " +
                                      std::to_string(pending_results_.size()) +
                                      "개 항목 최종 처리 중...");

    std::vector<VPResult> final_batch;
    while (!pending_results_.empty()) {
      final_batch.push_back(pending_results_.front());
      pending_results_.pop();
    }

    if (!final_batch.empty()) {
      WriteBatchToDatabase(final_batch);
    }
  }

  LogManager::getInstance().log("VirtualPointBatchWriter", LogLevel::INFO,
                                "✅ VirtualPointBatchWriter 중지 완료");
}

bool VirtualPointBatchWriter::IsRunning() const {
  return !should_stop_.load() && batch_writer_thread_.joinable();
}

// =============================================================================
// 데이터 처리 인터페이스
// =============================================================================

bool VirtualPointBatchWriter::QueueVirtualPointResult(
    const Structs::TimestampedValue &vp_result) {
  if (should_stop_.load()) {
    return false;
  }

  VPResult result = ConvertTimestampedValue(vp_result);
  return QueueResult(result);
}

bool VirtualPointBatchWriter::QueueResult(const VPResult &vp_result) {
  if (should_stop_.load()) {
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    // 큐 크기 제한 확인
    if (pending_results_.size() >= MAX_QUEUE_SIZE) {
      LogManager::getInstance().log("VirtualPointBatchWriter", LogLevel::WARN,
                                    "⚠️ 큐가 가득참 - 오래된 항목 삭제");
      pending_results_.pop(); // 오래된 항목 삭제
    }

    pending_results_.push(vp_result);
    statistics_.total_queued.fetch_add(1);
    statistics_.current_queue_size.store(pending_results_.size());
  }

  // 배치 크기 도달 시 즉시 처리 신호
  if (GetQueueSize() >= batch_size_) {
    cv_.notify_one();
  }

  return true;
}

size_t
VirtualPointBatchWriter::QueueResults(const std::vector<VPResult> &results) {
  if (should_stop_.load() || results.empty()) {
    return 0;
  }

  size_t added_count = 0;

  {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    for (const auto &result : results) {
      // 큐 크기 제한 확인
      if (pending_results_.size() >= MAX_QUEUE_SIZE) {
        LogManager::getInstance().log("VirtualPointBatchWriter", LogLevel::WARN,
                                      "⚠️ 큐가 가득참 - 나머지 항목 무시");
        break;
      }

      pending_results_.push(result);
      added_count++;
    }

    statistics_.total_queued.fetch_add(added_count);
    statistics_.current_queue_size.store(pending_results_.size());
  }

  // 배치 크기 도달 시 즉시 처리 신호
  if (GetQueueSize() >= batch_size_) {
    cv_.notify_one();
  }

  return added_count;
}

bool VirtualPointBatchWriter::FlushNow(bool wait_for_completion) {
  if (!IsRunning()) {
    return false;
  }

  // 즉시 처리 신호
  cv_.notify_one();

  if (wait_for_completion) {
    // 큐가 비워질 때까지 대기 (최대 5초)
    auto start_time = std::chrono::steady_clock::now();
    while (GetQueueSize() > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));

      auto elapsed = std::chrono::steady_clock::now() - start_time;
      if (elapsed > std::chrono::seconds(5)) {
        LogManager::getInstance().log("VirtualPointBatchWriter", LogLevel::WARN,
                                      "⚠️ FlushNow 타임아웃 - 큐 크기: " +
                                          std::to_string(GetQueueSize()));
        return false;
      }
    }
  }

  return true;
}

// =============================================================================
// 상태 및 통계 조회
// =============================================================================

size_t VirtualPointBatchWriter::GetQueueSize() const {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  return pending_results_.size();
}

VirtualPointBatchWriter::BatchWriterStatistics
VirtualPointBatchWriter::GetStatistics() const {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  return statistics_; // 🔥 수정: 복사 생성자 사용
}

nlohmann::json VirtualPointBatchWriter::GetStatisticsJson() const {
  auto stats = GetStatistics();

  json result;
  result["total_queued"] = stats.total_queued.load();
  result["total_written"] = stats.total_written.load();
  result["total_batches"] = stats.total_batches.load();
  result["failed_writes"] = stats.failed_writes.load();
  result["current_queue_size"] = stats.current_queue_size.load();
  result["avg_batch_size"] = stats.avg_batch_size.load();
  result["avg_write_time_ms"] = stats.avg_write_time_ms.load();
  result["last_write"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                             stats.last_write.time_since_epoch())
                             .count();
  result["is_running"] = IsRunning();
  result["batch_size"] = batch_size_;
  result["flush_interval_sec"] = flush_interval_.count();

  // 성능 지표 계산
  auto total_queued = stats.total_queued.load();
  auto total_written = stats.total_written.load();
  result["success_rate"] =
      total_queued > 0
          ? static_cast<double>(total_written) / total_queued * 100.0
          : 100.0;
  result["pending_items"] = total_queued - total_written;

  return result;
}

void VirtualPointBatchWriter::ResetStatistics() {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  statistics_.reset(); // 🔥 수정: reset() 메서드 사용

  LogManager::getInstance().log("VirtualPointBatchWriter", LogLevel::INFO,
                                "🔄 통계 초기화 완료");
}

// =============================================================================
// 설정 관리
// =============================================================================

void VirtualPointBatchWriter::SetBatchSize(size_t new_batch_size) {
  auto clamped_size =
      std::clamp(new_batch_size, static_cast<size_t>(1), MAX_BATCH_SIZE);
  batch_size_ = clamped_size;

  LogManager::getInstance().log("VirtualPointBatchWriter", LogLevel::INFO,
                                "⚙️ 배치 크기 변경: " +
                                    std::to_string(clamped_size));
}

void VirtualPointBatchWriter::SetFlushInterval(int new_interval_sec) {
  auto clamped_interval = std::chrono::seconds(
      std::clamp(new_interval_sec, 1,
                 static_cast<int>(MAX_FLUSH_INTERVAL.count() / 1000)));
  flush_interval_ = clamped_interval;

  LogManager::getInstance().log(
      "VirtualPointBatchWriter", LogLevel::INFO,
      "⚙️ 플러시 간격 변경: " + std::to_string(clamped_interval.count()) + "초");
}

std::pair<size_t, std::chrono::seconds>
VirtualPointBatchWriter::GetCurrentSettings() const {
  return {batch_size_, flush_interval_};
}

// =============================================================================
// 내부 구현 메서드들
// =============================================================================

void VirtualPointBatchWriter::BatchWriterLoop() {
  LogManager::getInstance().log("VirtualPointBatchWriter", LogLevel::INFO,
                                "🔄 배치 처리 루프 시작");

  while (!should_stop_.load()) {
    try {
      std::unique_lock<std::mutex> lock(queue_mutex_);

      // 배치 크기 도달하거나 타임아웃까지 대기
      cv_.wait_for(lock, flush_interval_, [this] {
        return pending_results_.size() >= batch_size_ || should_stop_.load();
      });

      if (pending_results_.empty()) {
        continue;
      }

      // 배치 추출
      auto batch = ExtractBatch(MAX_BATCH_SIZE);
      lock.unlock();

      if (!batch.empty()) {
        // DB 저장 (큐 잠금 해제 후)
        auto start_time = std::chrono::high_resolution_clock::now();
        bool success = WriteBatchToDatabase(batch);
        auto end_time = std::chrono::high_resolution_clock::now();

        auto write_time_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                  start_time)
                .count();

        UpdateStatistics(batch.size(), static_cast<double>(write_time_ms),
                         success);
      }

    } catch (const std::exception &e) {
      LogManager::getInstance().log(
          "VirtualPointBatchWriter", LogLevel::LOG_ERROR,
          "❌ 배치 처리 루프 예외: " + std::string(e.what()));

      // 잠시 대기 후 재시도
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  LogManager::getInstance().log("VirtualPointBatchWriter", LogLevel::INFO,
                                "✅ 배치 처리 루프 종료");
}

bool VirtualPointBatchWriter::WriteBatchToDatabase(
    const std::vector<VPResult> &batch) {
  if (batch.empty()) {
    return true;
  }

  try {
    size_t success_count = 0;

    for (const auto &result : batch) {
      bool item_success = true;

      try {
        // ✅ 1. virtual_point_execution_history 삽입
        if (!db_manager_->executeNonQuery(
                Runtime::VirtualPointBatch::INSERT_HISTORY(
                    result.vp_id, result.value))) {
          LogManager::getInstance().log(
              "VirtualPointBatchWriter", LogLevel::WARN,
              "⚠️ virtual_point_execution_history 삽입 실패 - VP ID: " +
                  std::to_string(result.vp_id));
          item_success = false;
        }

        // ✅ 2. virtual_point_values 업데이트 (현재값)
        if (!db_manager_->executeNonQuery(
                Runtime::VirtualPointBatch::UPSERT_CURRENT_VALUE(
                    result.vp_id, result.value, result.quality))) {
          LogManager::getInstance().log(
              "VirtualPointBatchWriter", LogLevel::WARN,
              "⚠️ virtual_point_values 업데이트 실패 - VP ID: " +
                  std::to_string(result.vp_id));
          item_success = false;
        }

        // ✅ 3. current_values 테이블에도 저장 (통합 현재값 관리)
        if (!db_manager_->executeNonQuery(
                Runtime::VirtualPointBatch::UPSERT_CURRENT_VALUES_TABLE(
                    result.vp_id, result.value, result.quality))) {
          LogManager::getInstance().log(
              "VirtualPointBatchWriter", LogLevel::WARN,
              "⚠️ current_values 업데이트 실패 - VP ID: " +
                  std::to_string(result.vp_id));
          item_success = false;
        }

        if (item_success) {
          success_count++;
        }

      } catch (const std::exception &e) {
        LogManager::getInstance().log(
            "VirtualPointBatchWriter", LogLevel::LOG_ERROR,
            "❌ 가상포인트 " + std::to_string(result.vp_id) +
                " 저장 중 예외: " + std::string(e.what()));
        // 개별 항목 실패해도 배치 처리 계속
      }
    }

    LogManager::getInstance().log(
        "VirtualPointBatchWriter", LogLevel::DEBUG_LEVEL,
        "✅ 가상포인트 배치 저장 완료: " + std::to_string(success_count) + "/" +
            std::to_string(batch.size()) + "개");

    // 절반 이상 성공하면 성공으로 간주
    return success_count >= (batch.size() / 2);

  } catch (const std::exception &e) {
    LogManager::getInstance().log(
        "VirtualPointBatchWriter", LogLevel::LOG_ERROR,
        "❌ 가상포인트 배치 저장 전체 실패: " + std::string(e.what()));

    return false;
  }
}

VirtualPointBatchWriter::VPResult
VirtualPointBatchWriter::ConvertTimestampedValue(
    const Structs::TimestampedValue &ts_value) {

  VPResult result;
  result.vp_id = ts_value.point_id;
  result.timestamp = ts_value.timestamp;
  result.quality = "good"; // 기본값

  // variant 값을 double로 변환
  result.value = std::visit(
      [](const auto &v) -> double {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_arithmetic_v<T>) {
          return static_cast<double>(v);
        } else if constexpr (std::is_same_v<T, std::string>) {
          try {
            return std::stod(v);
          } catch (...) {
            return 0.0;
          }
        }
        return 0.0;
      },
      ts_value.value);

  return result;
}

std::vector<VirtualPointBatchWriter::VPResult>
VirtualPointBatchWriter::ExtractBatch(size_t max_items) {
  std::vector<VPResult> batch;
  batch.reserve(std::min(max_items, pending_results_.size()));

  while (!pending_results_.empty() && batch.size() < max_items) {
    batch.push_back(pending_results_.front());
    pending_results_.pop();
  }

  statistics_.current_queue_size.store(pending_results_.size());
  return batch;
}

void VirtualPointBatchWriter::UpdateStatistics(size_t batch_size,
                                               double write_time_ms,
                                               bool success) {
  std::lock_guard<std::mutex> lock(stats_mutex_);

  statistics_.total_batches.fetch_add(1);
  statistics_.last_write = std::chrono::system_clock::now();

  if (success) {
    statistics_.total_written.fetch_add(batch_size);

    // 평균 배치 크기 업데이트
    auto total_batches = statistics_.total_batches.load();
    auto current_avg_batch = statistics_.avg_batch_size.load();
    statistics_.avg_batch_size.store(
        (current_avg_batch * (total_batches - 1) + batch_size) / total_batches);

    // 평균 쓰기 시간 업데이트
    auto current_avg_time = statistics_.avg_write_time_ms.load();
    statistics_.avg_write_time_ms.store(
        (current_avg_time * (total_batches - 1) + write_time_ms) /
        total_batches);
  } else {
    statistics_.failed_writes.fetch_add(1);
  }
}

bool VirtualPointBatchWriter::ValidateSettings(
    size_t batch_size, const std::chrono::seconds &flush_interval) const {
  return batch_size >= 1 && batch_size <= MAX_BATCH_SIZE &&
         flush_interval >= std::chrono::seconds(1) &&
         flush_interval <= MAX_FLUSH_INTERVAL;
}

} // namespace VirtualPoint
} // namespace PulseOne