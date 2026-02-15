// =============================================================================
// collector/include/Drivers/Bacnet/BACnetServiceManager.h
// BACnet 고급 서비스 관리자 헤더 - Windows/Linux 완전 호환 완성본
// =============================================================================

#ifndef BACNET_SERVICE_MANAGER_H
#define BACNET_SERVICE_MANAGER_H

// =============================================================================
// 필수 헤더 포함 (기존 프로젝트 패턴 준수)
// =============================================================================
#include "Common/Structs.h"
#include "Drivers/Bacnet/BACnetTypes.h"
#include <atomic>
#include <chrono>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>

// BACnet 스택 조건부 포함
#if defined(HAS_BACNET) || defined(HAS_BACNET) || defined(HAS_BACNET_STACK)
extern "C" {
#include <bacnet/apdu.h>
#include <bacnet/bacaddr.h>
#include <bacnet/bacapp.h>
#include <bacnet/bacdef.h>
#include <bacnet/bacenum.h>
#include <bacnet/rp.h>
}
#endif

namespace PulseOne {
namespace Drivers {

class IProtocolDriver; // 전방 선언

// =============================================================================
// 타입 별칭 정의 (기존 프로젝트 패턴)
// =============================================================================
using TimestampedValue = PulseOne::Structs::TimestampedValue;
using DataPoint = PulseOne::Structs::DataPoint;
using DataValue = PulseOne::Structs::DataValue;
using DeviceInfo = PulseOne::Structs::DeviceInfo;
using DataQuality = PulseOne::Enums::DataQuality;

/**
 * @brief BACnet 고급 서비스 관리자
 *
 * 고급 BACnet 서비스들을 관리:
 * - Read Property Multiple (RPM)
 * - Write Property Multiple (WPM)
 * - Create/Delete Object
 * - Device Discovery 최적화
 * - 배치 처리 최적화
 *
 * 주요 특징:
 * - Windows/Linux 크로스 플랫폼 지원
 * - 요청/응답 관리 시스템
 * - 자동 그룹핑 최적화
 * - 캐싱 및 통계 기능
 */
class BACnetServiceManager {
public:
  // ==========================================================================
  explicit BACnetServiceManager(IProtocolDriver *driver);
  ~BACnetServiceManager();

  // 복사/이동 방지
  BACnetServiceManager(const BACnetServiceManager &) = delete;
  BACnetServiceManager &operator=(const BACnetServiceManager &) = delete;

  // ==========================================================================
  // 고급 읽기 서비스
  // ==========================================================================

  /**
   * @brief Read Property Multiple 서비스
   * @param device_id 대상 디바이스 ID
   * @param objects 읽을 객체 목록
   * @param results 결과 저장소
   * @param timeout_ms 타임아웃 (기본: 5000ms)
   * @return 성공 여부
   */
  bool ReadPropertyMultiple(uint32_t device_id,
                            const std::vector<DataPoint> &objects,
                            std::vector<TimestampedValue> &results,
                            uint32_t timeout_ms = 5000);

  /**
   * @brief 개별 속성 읽기
   * @param device_id 대상 디바이스 ID
   * @param object_type BACnet 객체 타입
   * @param object_instance 객체 인스턴스 번호
   * @param property_id 프로퍼티 ID
   * @param result 결과 저장소
   * @param array_index 배열 인덱스 (기본: 전체)
   * @return 성공 여부
   */
  bool ReadProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                    uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                    TimestampedValue &result,
                    uint32_t array_index = BACNET_ARRAY_ALL);

  /**
   * @brief 최적화된 배치 읽기 (자동 RPM 그룹핑)
   * @param device_id 대상 디바이스 ID
   * @param points DataPoint 목록
   * @param results 결과 저장소
   * @return 성공 여부
   */
  bool BatchRead(uint32_t device_id, const std::vector<DataPoint> &points,
                 std::vector<TimestampedValue> &results);

  // ==========================================================================
  // 고급 쓰기 서비스
  // ==========================================================================

  /**
   * @brief Write Property Multiple 서비스
   * @param device_id 대상 디바이스 ID
   * @param values 쓸 값들의 맵
   * @param timeout_ms 타임아웃 (기본: 5000ms)
   * @return 성공 여부
   */
  bool WritePropertyMultiple(uint32_t device_id,
                             const std::map<DataPoint, DataValue> &values,
                             uint32_t timeout_ms = 5000);

  IProtocolDriver *GetDriver() const { return driver_; }

  /**
   * @brief 개별 속성 쓰기
   * @param device_id 대상 디바이스 ID
   * @param object_type BACnet 객체 타입
   * @param object_instance 객체 인스턴스 번호
   * @param property_id 프로퍼티 ID
   * @param value 쓸 값
   * @param priority 우선순위 (기본: 0 = 없음)
   * @param array_index 배열 인덱스 (기본: 전체)
   * @return 성공 여부
   */
  bool WriteProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                     uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                     const DataValue &value, uint8_t priority = 0,
                     uint32_t array_index = BACNET_ARRAY_ALL);

  /**
   * @brief 최적화된 배치 쓰기 (자동 WPM 그룹핑)
   * @param device_id 대상 디바이스 ID
   * @param point_values 포인트-값 쌍 목록
   * @return 성공 여부
   */
  bool
  BatchWrite(uint32_t device_id,
             const std::vector<std::pair<DataPoint, DataValue>> &point_values);

  // ==========================================================================
  // 객체 관리 서비스
  // ==========================================================================

  /**
   * @brief BACnet 객체 생성
   * @param device_id 대상 디바이스 ID
   * @param object_type 생성할 객체 타입
   * @param object_instance 객체 인스턴스 번호
   * @param initial_properties 초기 프로퍼티 값들
   * @return 성공 여부
   */
  bool CreateObject(
      uint32_t device_id, BACNET_OBJECT_TYPE object_type,
      uint32_t object_instance,
      const std::map<BACNET_PROPERTY_ID, DataValue> &initial_properties = {});

  /**
   * @brief BACnet 객체 삭제
   * @param device_id 대상 디바이스 ID
   * @param object_type 삭제할 객체 타입
   * @param object_instance 객체 인스턴스 번호
   * @return 성공 여부
   */
  bool DeleteObject(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                    uint32_t object_instance);

  // ==========================================================================
  // 디바이스 발견 및 관리
  // ==========================================================================

  /**
   * @brief 네트워크에서 BACnet 디바이스 발견
   * @param devices 발견된 디바이스 정보 맵
   * @param low_limit 검색 범위 하한 (기본: 0)
   * @param high_limit 검색 범위 상한 (기본: 최대값)
   * @param timeout_ms 타임아웃 (기본: 5000ms)
   * @return 발견된 디바이스 개수
   */
  int DiscoverDevices(std::map<uint32_t, DeviceInfo> &devices,
                      uint32_t low_limit = 0,
                      uint32_t high_limit = BACNET_MAX_DEVICE_ID,
                      uint32_t timeout_ms = 5000);

  /**
   * @brief 디바이스 정보 조회
   * @param device_id 디바이스 ID
   * @param device_info 디바이스 정보 저장소
   * @return 성공 여부
   */
  bool GetDeviceInfo(uint32_t device_id, DeviceInfo &device_info);

  /**
   * @brief 디바이스 연결 확인 (ping)
   * @param device_id 디바이스 ID
   * @param timeout_ms 타임아웃 (기본: 3000ms)
   * @return 응답 여부
   */
  bool PingDevice(uint32_t device_id, uint32_t timeout_ms = 3000);

  /**
   * @brief 디바이스의 객체 목록 조회
   * @param device_id 디바이스 ID
   * @param filter_type 필터할 객체 타입 (기본: 모든 타입)
   * @return 객체 목록
   */
  std::vector<DataPoint>
  GetDeviceObjects(uint32_t device_id,
                   BACNET_OBJECT_TYPE filter_type = MAX_BACNET_OBJECT_TYPE);

  // ==========================================================================
  // 통계 및 관리
  // ==========================================================================

  /**
   * @brief 서비스 통계 초기화
   */
  void ResetServiceStatistics();

  // ==========================================================================
  // Datalink Bridge (BIP Port 지원)
  // ==========================================================================

  /**
   * @brief 로우 패킷 전송 (bip_send_pdu 지원)
   */
  int SendRawPacket(uint8_t *dest_addr, uint32_t addr_len, uint8_t *payload,
                    uint32_t payload_len);

  /**
   * @brief 소켓 FD 조회 (bip_receive 지원)
   */
  int GetSocketFd() const;

private:
  // ==========================================================================
  // 내부 구조체들
  // ==========================================================================

  /**
   * @brief 대기 중인 요청 정보
   */
  struct PendingRequest {
    uint8_t invoke_id;
    std::string service_type;
    std::chrono::steady_clock::time_point timeout_time;
    std::promise<bool> promise;
    TimestampedValue *result_ptr;

    PendingRequest(uint8_t id, const std::string &type, uint32_t timeout_ms)
        : invoke_id(id), service_type(type),
          timeout_time(std::chrono::steady_clock::now() +
                       std::chrono::milliseconds(timeout_ms)),
          result_ptr(nullptr) {}

    // Context for special decoding
    uint32_t property_id = 0;
  };

  /**
   * @brief RPM 최적화를 위한 그룹
   */
  struct RPMGroup {
    std::vector<DataPoint> objects;
    std::vector<size_t> original_indices;
    size_t estimated_size_bytes = 0;
  };

  /**
   * @brief 간단한 서비스 통계
   */
  struct ServiceStatistics {
    std::atomic<uint64_t> rpm_count{0};
    std::atomic<uint64_t> wpm_count{0};
    std::atomic<uint64_t> rp_count{0};
    std::atomic<uint64_t> wp_count{0};
    std::atomic<uint64_t> success_count{0};
    std::atomic<uint64_t> error_count{0};

    void Reset() {
      rpm_count = 0;
      wpm_count = 0;
      rp_count = 0;
      wp_count = 0;
      success_count = 0;
      error_count = 0;
    }
  };

  // ==========================================================================
  // 멤버 변수들
  // ==========================================================================

  /// BACnet 드라이버 참조
  IProtocolDriver *driver_;

  /// Invoke ID 관리
  std::atomic<uint8_t> next_invoke_id_;

  /// 요청 관리
  std::mutex requests_mutex_;
  std::unordered_map<uint8_t, std::unique_ptr<PendingRequest>>
      pending_requests_;

  /// 디바이스 캐시
  std::mutex optimization_mutex_;
  std::map<uint32_t, DeviceInfo> device_cache_;

  /// 서비스 통계
  ServiceStatistics service_stats_;

  // ==========================================================================
  // 최적화 상수들
  // ==========================================================================
  static constexpr size_t MAX_RPM_OBJECTS = 20;
  static constexpr size_t MAX_APDU_SIZE = 1476;
  static constexpr size_t ESTIMATED_OVERHEAD = 20;

  // ==========================================================================
  // 내부 메서드들
  // ==========================================================================

  /// 유틸리티 함수들
  uint8_t GetNextInvokeId();
  void LogServiceError(const std::string &service_name,
                       const std::string &error_msg);
  void UpdateServiceStatistics(const std::string &service_type, bool success,
                               double time_ms);

  /// 디바이스 캐시 관리
  void UpdateDeviceCache(uint32_t device_id, const DeviceInfo &info);
  bool GetCachedDeviceInfo(uint32_t device_id, DeviceInfo &device_info);
  void CleanupDeviceCache();

  // ==========================================================================
  // C-to-C++ Bridge (Static Handlers)
  // ==========================================================================
#if defined(HAS_BACNET) || defined(HAS_BACNET) || defined(HAS_BACNET_STACK)
  static void
  HandlerReadPropertyAck(uint8_t *service_request, uint16_t service_len,
                         BACNET_ADDRESS *src,
                         BACNET_CONFIRMED_SERVICE_ACK_DATA *service_data);

  void ProcessReadPropertyAck(BACNET_READ_PROPERTY_DATA *data,
                              uint8_t invoke_id);
#endif

  /// 요청 관리 시스템
  void RegisterRequest(std::unique_ptr<PendingRequest> request);
  bool CompleteRequest(uint8_t invoke_id, bool success);
  std::shared_future<bool> GetRequestFuture(uint8_t invoke_id);
  void TimeoutRequests();

  /// RPM 최적화
  std::vector<RPMGroup>
  OptimizeRPMGroups(const std::vector<DataPoint> &objects);
  size_t EstimateObjectSize(const DataPoint &object);
  bool CanGroupObjects(const DataPoint &obj1, const DataPoint &obj2);

  /// WPM 최적화
  std::vector<std::map<DataPoint, DataValue>>
  OptimizeWPMGroups(const std::map<DataPoint, DataValue> &values);

  /// 데이터 변환 함수들
  DataPoint DataPointToBACnetObject(const DataPoint &point);
  TimestampedValue BACnetValueToTimestampedValue(
      const BACNET_APPLICATION_DATA_VALUE &bacnet_value);
  bool DataValueToBACnetValue(const DataValue &data_value,
                              BACNET_APPLICATION_DATA_VALUE &bacnet_value);

#if defined(HAS_BACNET) || defined(HAS_BACNET) || defined(HAS_BACNET_STACK)
  /// BACnet 프로토콜 헬퍼 함수들 (Linux에서만 컴파일)
  bool SendRPMRequest(uint32_t device_id, const std::vector<DataPoint> &objects,
                      uint8_t invoke_id);

  bool SendWPMRequest(uint32_t device_id,
                      const std::map<DataPoint, DataValue> &values,
                      uint8_t invoke_id);

  bool ParseRPMResponse(const uint8_t *service_data, uint16_t service_len,
                        const std::vector<DataPoint> &expected_objects,
                        std::vector<TimestampedValue> &results);

  bool ParseWPMResponse(const uint8_t *service_data, uint16_t service_len);

  bool GetDeviceAddress(uint32_t device_id, BACNET_ADDRESS &address);
  void CacheDeviceAddress(uint32_t device_id, const BACNET_ADDRESS &address);
#endif // HAS_BACNET_STACK

  friend class BACnetDriver;
  friend class IProtocolDriver;
};

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_SERVICE_MANAGER_H