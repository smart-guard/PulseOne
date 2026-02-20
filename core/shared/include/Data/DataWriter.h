// =============================================================================
// core/shared/include/Data/DataWriter.h
// Redis 데이터 쓰기 전용 클래스 (Export Gateway용)
// =============================================================================

#ifndef DATA_WRITER_H
#define DATA_WRITER_H

#include "Data/RedisDataTypes.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

namespace PulseOne {
class RedisClient;
}

namespace PulseOne {
namespace Shared {
namespace Data {

// =============================================================================
// DataWriter 옵션
// =============================================================================

struct DataWriterOptions {
  bool enable_batch_write = true;
  int batch_size = 100;
  int batch_flush_interval_ms = 1000;
  int default_ttl_seconds = 3600;
  int log_ttl_seconds = 86400; // 24시간
  bool enable_async_write = true;
  int async_queue_size = 10000;
  int retry_count = 3;
  int retry_delay_ms = 100;
};

// =============================================================================
// DataWriter 클래스
// =============================================================================

/**
 * @brief Redis 데이터 쓰기 전용 클래스 (Export Gateway용)
 *
 * 특징:
 * - Export 로그 기록
 * - 서비스 상태 관리
 * - 카운터 관리
 * - 비동기/배치 쓰기
 * - 멀티스레드 안전
 *
 * 사용 예:
 * @code
 * auto redis = std::make_shared<RedisClientImpl>();
 * DataWriter writer(redis);
 *
 * ExportLogEntry log;
 * log.service_id = 1;
 * log.status = "success";
 * writer.WriteExportLog(log);
 *
 * writer.IncrementCounter("export:success");
 * @endcode
 */
class DataWriter {
public:
  // ==========================================================================
  // 생성자 및 소멸자
  // ==========================================================================

  explicit DataWriter(std::shared_ptr<PulseOne::RedisClient> redis,
                      const DataWriterOptions &options = DataWriterOptions());
  ~DataWriter();

  // 복사/이동 금지
  DataWriter(const DataWriter &) = delete;
  DataWriter &operator=(const DataWriter &) = delete;
  DataWriter(DataWriter &&) = delete;
  DataWriter &operator=(DataWriter &&) = delete;

  // ==========================================================================
  // Export 로그 쓰기
  // ==========================================================================

  /**
   * @brief Export 로그 기록
   * @param log Export 로그 엔트리
   * @param async 비동기 쓰기 여부 (기본: true)
   * @return 성공 여부
   */
  bool WriteExportLog(const ExportLogEntry &log, bool async = true);

  // ==========================================================================
  // 서비스 상태 관리
  // ==========================================================================

  /**
   * @brief 서비스 상태 업데이트
   * @param status 서비스 상태
   * @return 성공 여부
   */
  bool UpdateServiceStatus(const ServiceStatus &status);

  /**
   * @brief 서비스 시작 기록
   */
  bool RecordServiceStart(int service_id, const std::string &service_type,
                          const std::string &service_name);

  /**
   * @brief 서비스 중지 기록
   */
  bool RecordServiceStop(int service_id);

  /**
   * @brief 서비스 요청 카운트 증가
   */
  bool IncrementServiceRequest(int service_id, bool success,
                               int response_time_ms);

  // ==========================================================================
  // 카운터 관리
  // ==========================================================================

  /**
   * @brief 카운터 증가
   * @param counter_name 카운터 이름
   * @param increment 증가량 (기본: 1)
   * @return 증가 후 값
   */
  int IncrementCounter(const std::string &counter_name, int increment = 1);

  /**
   * @brief 카운터 값 설정
   */
  bool SetCounter(const std::string &counter_name, int value);

  /**
   * @brief 카운터 초기화
   */
  bool ResetCounter(const std::string &counter_name);

  // ==========================================================================
  // 에러 로깅
  // ==========================================================================

  /**
   * @brief 에러 기록
   */
  bool WriteError(const std::string &service_name,
                  const std::string &error_message,
                  const std::string &error_code = "",
                  const std::string &details = "");

  // ==========================================================================
  // 일반 데이터 쓰기
  // ==========================================================================

  /**
   * @brief 키-값 쓰기 (TTL 포함)
   */
  bool Write(const std::string &key, const std::string &value,
             int ttl_seconds = 0);

  /**
   * @brief JSON 쓰기
   */
  bool WriteJson(const std::string &key, const nlohmann::json &json,
                 int ttl_seconds = 0);

  /**
   * @brief 키 삭제
   */
  bool Delete(const std::string &key);

  // ==========================================================================
  // Pub/Sub 발행
  // ==========================================================================

  /**
   * @brief 채널에 메시지 발행
   */
  bool Publish(const std::string &channel, const std::string &message);

  /**
   * @brief JSON 메시지 발행
   */
  bool PublishJson(const std::string &channel, const nlohmann::json &json);

  // ==========================================================================
  // 배치 처리
  // ==========================================================================

  /**
   * @brief 배치 쓰기 강제 플러시
   * @return 플러시된 항목 수
   */
  int FlushBatch();

  /**
   * @brief 대기 중인 비동기 쓰기 완료 대기
   * @param timeout_ms 타임아웃 (밀리초)
   * @return 성공 여부
   */
  bool WaitForPendingWrites(int timeout_ms = 5000);

  // ==========================================================================
  // 유틸리티
  // ==========================================================================

  struct Statistics {
    uint64_t total_writes;
    uint64_t successful_writes;
    uint64_t failed_writes;
    uint64_t async_writes;
    uint64_t log_writes;
    uint64_t status_updates;
    uint64_t counter_increments;
    double avg_write_time_ms;
  };

  Statistics GetStatistics() const;
  void ResetStatistics();
  bool IsConnected() const;
  size_t GetPendingQueueSize() const;

private:
  // ==========================================================================
  // 내부 구조체
  // ==========================================================================

  struct WriteTask {
    std::string key;
    std::string value;
    int ttl_seconds;
  };

  // ==========================================================================
  // 내부 메서드들
  // ==========================================================================

  bool writeWithRetry(const std::string &key, const std::string &value,
                      int ttl_seconds);
  bool enqueueWrite(const std::string &key, const std::string &value,
                    int ttl_seconds);
  void asyncWriteThreadFunc();
  void updateStats(bool success, double elapsed_ms,
                   const std::string &operation_type);
  std::string generateLogKey(const ExportLogEntry &log);

  // ==========================================================================
  // 멤버 변수들
  // ==========================================================================

  std::shared_ptr<PulseOne::RedisClient> redis_;
  DataWriterOptions options_;

  // 비동기 쓰기
  std::atomic<bool> running_{true};
  std::queue<WriteTask> async_queue_;
  mutable std::mutex async_mutex_;
  std::condition_variable async_cv_;
  std::unique_ptr<std::thread> async_thread_;

  // 통계
  mutable std::mutex stats_mutex_;
  Statistics stats_;
};

} // namespace Data
} // namespace Shared
} // namespace PulseOne

#endif // DATA_WRITER_H