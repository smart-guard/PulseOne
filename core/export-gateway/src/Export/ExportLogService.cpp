/**
 * @file ExportLogService.cpp
 * @brief 비동기 Export Log 저장 서비스 구현
 * @author PulseOne Development Team
 * @date 2026-01-30
 */

#include "Export/ExportLogService.h"
#include "Database/Repositories/ExportLogRepository.h"
#include "Database/RepositoryFactory.h" // Correct path
#include "Logging/LogManager.h"

namespace PulseOne {
namespace Export {

using namespace PulseOne::Database::Repositories;

ExportLogService &ExportLogService::getInstance() {
  static ExportLogService instance;
  return instance;
}

ExportLogService::ExportLogService() : running_(false) {}

ExportLogService::~ExportLogService() { stop(); }

void ExportLogService::start() {
  if (running_) {
    return;
  }

  running_ = true;
  worker_thread_ = std::thread(&ExportLogService::workerLoop, this);

  if (running_) {
    LogManager::getInstance().Info(
        "🚀 ExportLogService started (Async Queue Size: " +
        std::to_string(MAX_QUEUE_SIZE) + ")");
  }
}

void ExportLogService::stop() {
  if (!running_) {
    return;
  }

  if (running_) { // Use running check logic or just log
    LogManager::getInstance().Info("🛑 Stopping ExportLogService...");
  }

  running_ = false;

  // 큐가 비어있지 않아도 1초 타임아웃으로 workerLoop가 깨어나서 종료 조건을
  // 확인함 ThreadSafeQueue가 condition variable wait 중이면 깨워야 함. 하지만
  // 현재 구현된 ThreadSafeQueue에는 explicit notify가 없음. pop_batch의 timeout
  // 메커니즘을 이용하거나, dummy 데이터를 넣어서 깨울 수 있음. 여기서는
  // ThreadSafeQueue::push로 dummy 데이터를 넣는 대신, workerLoop가 timeout으로
  // 깨어나서 running_ 플래그를 확인하도록 설계됨.

  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }

  LogManager::getInstance().Info("✅ ExportLogService stopped");
}

void ExportLogService::enqueueLog(const ExportLogEntity &log) {
  if (!running_) {
    return; // 서비스가 실행 중이 아니면 무시 (혹은 저장 실패 처리)
  }

  // 큐 용량 제한 확인 (try_push 사용)
  // 큐 용량 제한 확인 (try_push 사용)
  if (!queue_.try_push(log, MAX_QUEUE_SIZE)) {
    // 너무 자주 로그를 남기면 성능 저하되므로, 실제로는 카운터만 증가시키거나
    // 해야 함 여기서는 간단히 Warn 로그 (실제 운영 시에는 Rate Limit 필요)
    static int drop_count = 0;
    drop_count++;
    if (drop_count % 100 == 0) {
      LogManager::getInstance().Warn("⚠️ ExportLog Queue Full! Dropped " +
                                     std::to_string(drop_count) +
                                     " logs so far.");
    }
  }
}

size_t ExportLogService::getQueueSize() const { return queue_.size(); }

void ExportLogService::workerLoop() {
  // Repository 인스턴스는 워커 스레드 내에서 얻거나, 멤버로 유지해도 됨.
  // RepositoryFactory는 싱글톤이고 thread-safe 가정.
  auto repo = PulseOne::Database::RepositoryFactory::getInstance()
                  .getExportLogRepository();

  if (!repo) {
    LogManager::getInstance().Error(
        "❌ Failed to get ExportLogRepository! Async logging disabled.");
    return;
  }

  while (running_ || !queue_.empty()) {
    // 배치로 가져오기 (타임아웃 1초)
    // running_이 false가 되어도 큐에 남은 데이터는 처리하고 종료함
    std::vector<ExportLogEntity> batch =
        queue_.pop_batch(BATCH_SIZE, BATCH_TIMEOUT_MS);

    if (!batch.empty()) {
      saveBatch(batch);
    }
  }
}

void ExportLogService::saveBatch(std::vector<ExportLogEntity> &batch) {
  if (batch.empty())
    return;

  auto repo = PulseOne::Database::RepositoryFactory::getInstance()
                  .getExportLogRepository();
  // Repository 타입 캐스팅 (IRepository -> ExportLogRepository)
  // getExportLogRepository가 이미 구체적인 타입을 리턴하는지 확인 필요
  // RepositoryFactory.h를 보면 std::shared_ptr<IExportLogRepository> 같은
  // 인터페이스일 수 있음. 하지만 RepositoryFactory.cpp 구현을 모르니 일단
  // 스마트 포인터 사용.

  // 트랜잭션 처리는 Repository 내부 동작을 알 수 없어 개별 저장 시도.
  // 만약 Repository가 bulk insert를 지원하지 않으면 loop save.

  int success_count = 0;
  for (auto &log : batch) {
    if (repo->save(log)) {
      success_count++;
    }
  }

  // 간단한 통계 로깅 (옵션)
  // if (logger_) {
  //    logger_->Debug("Saved batch: " + std::to_string(success_count) + "/" +
  //    std::to_string(batch.size()));
  // }
}

} // namespace Export
} // namespace PulseOne
