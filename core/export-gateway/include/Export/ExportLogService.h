/**
 * @file ExportLogService.h
 * @brief 비동기 Export Log 저장 서비스
 * @author PulseOne Development Team
 * @date 2026-01-30
 */

#ifndef EXPORT_LOG_SERVICE_H
#define EXPORT_LOG_SERVICE_H

#include "Database/Entities/ExportLogEntity.h"
#include "Utils/ThreadSafeQueue.h"
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace PulseOne {
namespace Export {

using ExportLogEntity = PulseOne::Database::Entities::ExportLogEntity;

/**
 * @class ExportLogService
 * @brief Export 로그를 비동기로 DB에 저장하는 싱글톤 서비스
 */
class ExportLogService {
public:
  static ExportLogService &getInstance();

  // 서비스 시작/종료
  void start();
  void stop();

  // 로그 큐에 추가 (Non-blocking attempt)
  void enqueueLog(const ExportLogEntity &log);

  // 큐 상태 확인
  size_t getQueueSize() const;

  // 복사/이동 금지
  ExportLogService(const ExportLogService &) = delete;
  ExportLogService &operator=(const ExportLogService &) = delete;

private:
  ExportLogService();
  ~ExportLogService();

  void workerLoop();
  void saveBatch(std::vector<ExportLogEntity> &batch);

private:
  std::atomic<bool> running_;
  std::thread worker_thread_;

  // 최대 10,000개까지 대기 가능
  const size_t MAX_QUEUE_SIZE = 10000;

  // 배치 처리를 위한 설정
  const size_t BATCH_SIZE = 50;
  const uint32_t BATCH_TIMEOUT_MS = 1000;

  PulseOne::Utils::ThreadSafeQueue<ExportLogEntity> queue_;
};

} // namespace Export
} // namespace PulseOne

#endif // EXPORT_LOG_SERVICE_H
