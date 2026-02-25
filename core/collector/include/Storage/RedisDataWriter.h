//=============================================================================
// collector/include/Storage/RedisDataWriter.h
//
// 수정: BackendFormat 구조체들을 별도 헤더로 분리
// 변경사항:
//   - namespace BackendFormat { ... } 블록 제거
//   - #include "Storage/BackendFormat.h" 추가
//   - 나머지 RedisDataWriter 클래스는 그대로 유지
//=============================================================================

#ifndef REDIS_DATA_WRITER_H
#define REDIS_DATA_WRITER_H

#include "Client/RedisClient.h"
#include "Common/Enums.h"
#include "Common/Structs.h"
#include "Logging/LogManager.h"
#include "Storage/BackendFormat.h" // ← 새로 추가!
#include "nlohmann/json.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace PulseOne {
namespace Storage {

// BackendFormat 네임스페이스는 별도 헤더에서 include됨

/**
 * @brief Backend 완전 호환 Redis 데이터 저장 클래스
 */
class RedisDataWriter {
public:
  enum class StorageMode { LIGHTWEIGHT, FULL_DATA, HYBRID };

  // ==========================================================================
  // 생성자 및 초기화
  // ==========================================================================

  explicit RedisDataWriter(std::shared_ptr<RedisClient> redis_client = nullptr);
  ~RedisDataWriter() = default;

  void SetRedisClient(std::shared_ptr<RedisClient> redis_client);
  bool IsConnected() const;

  // ==========================================================================
  // Backend 완전 호환 저장 메서드들 (메인 API)
  // ==========================================================================
  // 설정 메서드들
  void SetStorageMode(StorageMode mode);
  void EnableDevicePatternStorage(bool enable);
  void EnablePointLatestStorage(bool enable);
  void EnableFullDataStorage(bool enable);

  /**
   * @brief DeviceDataMessage를 Backend 호환 형식으로 저장
   * device:{device_id}:{point_name} + point:{point_id}:latest 동시 저장
   * @param message 디바이스 메시지
   * @return 성공한 포인트 수
   */
  size_t SaveDeviceMessage(const Structs::DeviceDataMessage &message);

  /**
   * @brief 개별 포인트를 Backend 호환 형식으로 저장
   * @param point 타임스탬프 값
   * @param device_id 디바이스 ID ("device_001" 형식)
   * @return 성공 여부
   */
  bool SaveSinglePoint(const Structs::TimestampedValue &point,
                       const std::string &device_id);

  /**
   * @brief 알람 이벤트를 Redis에 발행 및 저장
   * @param alarm_data 알람 데이터
   * @return 성공 여부
   */
  bool PublishAlarmEvent(const BackendFormat::AlarmEventData &alarm_data);
  bool
  StoreVirtualPointToRedis(const Structs::TimestampedValue &virtual_point_data);

  // ==========================================================================
  // Worker 초기화 전용 메서드들
  // ==========================================================================

  /**
   * @brief Worker 초기화 완료 시 현재 데이터 저장
   * @param device_id 디바이스 ID ("device_001")
   * @param current_values DB에서 읽은 현재 값들
   * @return 성공한 포인트 수
   */
  size_t SaveWorkerInitialData(
      const std::string &device_id,
      const std::vector<Structs::TimestampedValue> &current_values);

  /**
   * @brief Worker 데이터 포인트 동기화 (기존 값 유지)
   * @param device_id 디바이스 ID
   * @param points 최신 데이터 포인트 목록
   * @return 동기화된 포인트 수
   */
  size_t SyncWorkerDataPoints(const std::string &device_id,
                              const std::vector<Structs::DataPoint> &points);

  /**
   * @brief Worker 상태 정보 저장
   * @param device_id 디바이스 ID
   * @param status 상태 ("initialized", "running", "stopped", "error")
   * @param metadata 추가 메타데이터
   * @return 성공 여부
   */
  bool SaveWorkerStatus(const std::string &device_id, const std::string &status,
                        const nlohmann::json &metadata = {});

  /**
   * @brief Collector 프로세스 자체의 heartbeat를 Redis에 기록
   * collector:heartbeat:{collector_id} 키에 TTL과 함께 저장
   * DashboardService가 이 키를 읽어 Collector 생존 여부를 판단
   * @param collector_id Collector 인스턴스 ID (환경변수 COLLECTOR_ID)
   * @param heartbeat_data JSON 페이로드
   * @param ttl_seconds TTL 초 (기본 170: 약 3분 후 자동 만료)
   * @return 성공 여부
   */
  bool SaveCollectorHeartbeat(const std::string &collector_id,
                              const nlohmann::json &heartbeat_data,
                              int ttl_seconds = 170);

  // ==========================================================================
  // 통계 및 상태
  // ==========================================================================

  struct WriteStats {
    std::atomic<uint64_t> total_writes{0};
    std::atomic<uint64_t> successful_writes{0};
    std::atomic<uint64_t> failed_writes{0};
    std::atomic<uint64_t> device_point_writes{0};
    std::atomic<uint64_t> point_latest_writes{0};
    std::atomic<uint64_t> alarm_publishes{0};
    std::atomic<uint64_t> worker_init_writes{0};

    // 기본 생성자만 유지
    WriteStats() = default;

    // 복사/이동 생성자와 대입연산자는 삭제됨 (atomic 때문에)
    WriteStats(const WriteStats &) = delete;
    WriteStats &operator=(const WriteStats &) = delete;
    WriteStats(WriteStats &&) = delete;
    WriteStats &operator=(WriteStats &&) = delete;

    nlohmann::json toJson() const {
      nlohmann::json j;
      j["total_writes"] = total_writes.load();
      j["successful_writes"] = successful_writes.load();
      j["failed_writes"] = failed_writes.load();
      j["device_point_writes"] = device_point_writes.load();
      j["point_latest_writes"] = point_latest_writes.load();
      j["alarm_publishes"] = alarm_publishes.load();
      j["worker_init_writes"] = worker_init_writes.load();
      return j;
    }
  };

  nlohmann::json GetStatistics() const; // WriteStats 대신 json 반환
  void ResetStatistics();
  nlohmann::json GetStatus() const;

private:
  // ==========================================================================
  // 내부 데이터 변환 메서드들
  // ==========================================================================

  /**
   * @brief TimestampedValue를 Backend 호환 DevicePointData로 변환
   */
  BackendFormat::DevicePointData
  ConvertToDevicePointData(const Structs::TimestampedValue &point,
                           const std::string &device_id) const;

  /**
   * @brief TimestampedValue를 Backend 호환 PointLatestData로 변환
   */
  BackendFormat::PointLatestData
  ConvertToPointLatestData(const Structs::TimestampedValue &point,
                           const std::string &device_id) const;

  /**
   * @brief DataValue를 문자열로 변환
   */
  std::string ConvertValueToString(const Structs::DataValue &value) const;

  /**
   * @brief DataQuality를 Backend 호환 문자열로 변환
   */
  std::string ConvertQualityToString(const Structs::DataQuality &quality) const;

  /**
   * @brief DataValue 타입을 문자열로 변환
   */
  std::string GetDataTypeString(const Structs::DataValue &value) const;

  // ==========================================================================
  // 내부 헬퍼 메서드들
  // ==========================================================================

  /**
   * @brief "device_001" -> "1" 변환
   */
  std::string ExtractDeviceNumber(const std::string &device_id) const;

  /**
   * @brief 포인트 ID에서 포인트 이름 가져오기
   */
  std::string GetPointName(int point_id) const;

  /**
   * @brief 포인트 ID에서 단위 문자열 가져오기
   */
  std::string GetUnit(int point_id) const;

  /**
   * @brief 포인트 ID에서 디바이스 ID 추론
   */
  std::string GetDeviceIdForPoint(int point_id) const;

  /**
   * @brief 에러 처리 및 통계 업데이트
   */
  void HandleError(const std::string &context,
                   const std::string &error_message);

  // ==========================================================================
  // 내부 저장 메서드들
  // ==========================================================================

  size_t SaveLightweightFormat(const Structs::DeviceDataMessage &message);
  size_t SaveFullDataFormat(const Structs::DeviceDataMessage &message);
  size_t SaveDevicePatternFormat(const Structs::DeviceDataMessage &message);
  size_t SavePointLatestFormat(const Structs::DeviceDataMessage &message);

  // ==========================================================================
  // 멤버 변수들
  // ==========================================================================

  /// Redis 클라이언트
  std::shared_ptr<RedisClient> redis_client_;

  /// 통계
  WriteStats stats_;

  /// 스레드 안전성을 위한 뮤텍스
  mutable std::mutex redis_mutex_;

  /// 포인트 정보 캐시 (성능 최적화)
  mutable std::unordered_map<int, std::string> point_name_cache_;
  mutable std::unordered_map<int, std::string> unit_cache_;
  mutable std::mutex cache_mutex_;

  // 저장 모드 설정
  StorageMode storage_mode_ = StorageMode::HYBRID;
  bool store_device_pattern_ = true;
  bool store_point_latest_ = true;
  bool store_full_data_ = true;
};

} // namespace Storage
} // namespace PulseOne

#endif // REDIS_DATA_WRITER_H