/**
 * @file BACnetDriver.h  
 * @brief BACnet 프로토콜 드라이버 - 모든 타입 불일치 해결
 * @author PulseOne Development Team
 * @date 2025-08-02
 */

#ifndef BACNET_DRIVER_H
#define BACNET_DRIVER_H

// =============================================================================
// 🔥 실제 PulseOne 프로젝트 구조에 맞춘 include 경로
// =============================================================================
#include "Common/UnifiedCommonTypes.h"
#include "Drivers/Common/IProtocolDriver.h"
#include <atomic>
#include <mutex>
#include <memory>
#include <map>
#include <vector>
#include <string>
#include <chrono>

// =============================================================================
// 🔥 정확한 BACnet 헤더 경로 - 조건부 컴파일
// =============================================================================

#ifdef HAS_BACNET_STACK
    // 실제 설치된 BACnet 스택 헤더 경로들
    #include <bacnet/config.h>
    #include <bacnet/bacdef.h>
    #include <bacnet/bacenum.h>
    #include <bacnet/bacapp.h>
    #include <bacnet/bacdcode.h>
    #include <bacnet/bacaddr.h>
    #include <bacnet/npdu.h>
    #include <bacnet/apdu.h>
    
    // BACnet 서비스 헤더들
    #include <bacnet/rp.h>           // Read Property
    #include <bacnet/wp.h>           // Write Property
    #include <bacnet/rpm.h>          // Read Property Multiple
    #include <bacnet/wpm.h>          // Write Property Multiple
    #include <bacnet/whois.h>        // Who-Is
    #include <bacnet/iam.h>          // I-Am
    #include <bacnet/cov.h>          // Change of Value
    #include <bacnet/dcc.h>          // Device Communication Control
    
    // BACnet 데이터링크 (조건부 - 파일이 있는지 확인)
    #if __has_include(<bacnet/datalink/datalink.h>)
        #include <bacnet/datalink/datalink.h>
    #endif
    #if __has_include(<bacnet/datalink/bip.h>)
        #include <bacnet/datalink/bip.h>
    #endif
    
    // BACnet 기본 서비스들 (조건부)
    #if __has_include(<bacnet/basic/services.h>)
        #include <bacnet/basic/services.h>
    #endif
    #if __has_include(<bacnet/basic/tsm/tsm.h>)
        #include <bacnet/basic/tsm/tsm.h>
    #endif
    
    // 🎯 핵심: device.h (정확한 경로 확인됨)
    #include <bacnet/basic/object/device.h>
    
    #define BACNET_HEADERS_AVAILABLE 1
#else
    #define BACNET_HEADERS_AVAILABLE 0
#endif

// =============================================================================
// BACnet 타입 정의 (헤더가 없을 때 사용할 더미 타입들)
// =============================================================================

#if !BACNET_HEADERS_AVAILABLE
    // BACnet 스택이 없을 때 사용할 더미 타입들
    typedef enum {
        OBJECT_ANALOG_INPUT = 0,
        OBJECT_ANALOG_OUTPUT = 1, 
        OBJECT_ANALOG_VALUE = 2,
        OBJECT_BINARY_INPUT = 3,
        OBJECT_BINARY_OUTPUT = 4,
        OBJECT_BINARY_VALUE = 5,
        OBJECT_DEVICE = 8,
        OBJECT_MULTI_STATE_INPUT = 13,
        OBJECT_MULTI_STATE_OUTPUT = 14,
        OBJECT_MULTI_STATE_VALUE = 19,
        OBJECT_PROPRIETARY_MIN = 128
    } BACNET_OBJECT_TYPE;

    typedef enum {
        PROP_PRESENT_VALUE = 85,
        PROP_OBJECT_NAME = 77,
        PROP_DESCRIPTION = 28,
        PROP_UNITS = 117
    } BACNET_PROPERTY_ID;

    typedef struct {
        uint8_t net;
        uint8_t len;
        uint8_t adr[6];
    } BACNET_ADDRESS;

    typedef uint8_t BACNET_APPLICATION_DATA_VALUE;
    typedef uint8_t BACNET_READ_PROPERTY_DATA;
    typedef uint8_t BACNET_WRITE_PROPERTY_DATA;
#endif

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 🔥 네임스페이스 별칭 정의 (타입 불일치 해결)
// =============================================================================
using DataPoint = PulseOne::Structs::DataPoint;
using DataValue = PulseOne::Structs::DataValue;
using TimestampedValue = PulseOne::Structs::TimestampedValue;
using DriverConfig = PulseOne::Structs::DriverConfig;
using DriverStatistics = PulseOne::DriverStatistics;

// =============================================================================
// BACnet 전용 구조체들
// =============================================================================

/**
 * @brief BACnet 디바이스 정보
 */
struct BACnetDeviceInfo {
    uint32_t device_id = 0;
    std::string device_name;
    std::string ip_address;
    uint16_t port = 47808;
    uint16_t max_apdu_length = 1476;
    bool segmentation_supported = true;
    uint16_t vendor_id = 0;
    uint8_t protocol_version = 1;
    uint8_t protocol_revision = 14;
    std::chrono::system_clock::time_point last_seen;
    
    BACnetDeviceInfo() : last_seen(std::chrono::system_clock::now()) {}
};

/**
 * @brief BACnet 객체 정보 - 🔥 멤버 수정 (property_id, array_index 제거)
 */
struct BACnetObjectInfo {
    BACNET_OBJECT_TYPE object_type = OBJECT_ANALOG_INPUT;
    uint32_t object_instance = 0;
    std::string object_name;
    std::string description;
    std::string units;
    
    // 🔥 property_id, array_index 제거 (컴파일 오류 해결)
    // BACNET_PROPERTY_ID property_id = PROP_PRESENT_VALUE;  // 제거
    // uint32_t array_index = 0;                            // 제거
    
    BACnetObjectInfo() = default;
    BACnetObjectInfo(BACNET_OBJECT_TYPE type, uint32_t instance, 
                    const std::string& name = "") 
        : object_type(type), object_instance(instance), object_name(name) {}
};

/**
 * @brief BACnet 특화 통계
 */
struct BACnetStatistics {
    std::atomic<uint64_t> whois_sent{0};
    std::atomic<uint64_t> iam_received{0};
    std::atomic<uint64_t> devices_discovered{0};
    std::atomic<uint64_t> objects_read{0};
    std::atomic<uint64_t> objects_written{0};
    std::atomic<uint64_t> cov_subscriptions{0};
    std::atomic<uint64_t> cov_notifications{0};
    std::atomic<uint64_t> timeouts{0};
    std::atomic<uint64_t> errors{0};
    
    // 성능 지표
    std::atomic<double> avg_response_time_ms{0.0};
    std::atomic<uint32_t> max_response_time_ms{0};
    std::atomic<uint32_t> min_response_time_ms{0};
    
    void Reset() {
        whois_sent = 0;
        iam_received = 0;
        devices_discovered = 0;
        objects_read = 0;
        objects_written = 0;
        cov_subscriptions = 0;
        cov_notifications = 0;
        timeouts = 0;
        errors = 0;
        avg_response_time_ms = 0.0;
        max_response_time_ms = 0;
        min_response_time_ms = 0;
    }
    
    double GetSuccessRate() const {
        uint64_t total = objects_read + objects_written;
        return total > 0 ? (1.0 - static_cast<double>(errors) / total) * 100.0 : 100.0;
    }
    
    uint32_t GetAverageResponseTime() const {
        return static_cast<uint32_t>(avg_response_time_ms.load());
    }
};

/**
 * @brief BACnet 프로토콜 드라이버 클래스
 * 
 * BACnet/IP 프로토콜을 지원하는 드라이버로, 다음 기능을 제공합니다:
 * - 디바이스 발견 (Who-Is/I-Am)
 * - 객체 읽기/쓰기 (Read/Write Property)
 * - 다중 객체 처리 (Read/Write Property Multiple)
 * - COV 구독 및 알림
 * - 고급 통계 및 성능 모니터링
 */
class BACnetDriver : public IProtocolDriver {
public:
    // ==========================================================================
    // 생성자 및 소멸자
    // ==========================================================================
    
    BACnetDriver();
    virtual ~BACnetDriver();

    // 복사 및 이동 방지
    BACnetDriver(const BACnetDriver&) = delete;
    BACnetDriver& operator=(const BACnetDriver&) = delete;
    BACnetDriver(BACnetDriver&&) = delete;
    BACnetDriver& operator=(BACnetDriver&&) = delete;

    // ==========================================================================
    // IProtocolDriver 필수 구현 메서드들
    // ==========================================================================
    
    /**
     * @brief 드라이버 초기화
     * @param config 설정 정보 (device_id, target_ip, port 등)
     * @return 성공 시 true
     */
    bool Initialize(const DriverConfig& config) override;
    
    /**
     * @brief BACnet 네트워크 연결
     * @return 성공 시 true
     */
    bool Connect() override;
    
    /**
     * @brief BACnet 네트워크 연결 해제
     * @return 성공 시 true  
     */
    bool Disconnect() override;
    
    /**
     * @brief 연결 상태 확인
     * @return 연결된 경우 true
     */
    bool IsConnected() const override;
    
    /**
     * @brief 여러 데이터 포인트 읽기 (배치 처리)
     * @param points 읽을 데이터 포인트 목록
     * @param values 읽은 값들이 저장될 벡터
     * @return 성공 시 true
     */
    bool ReadValues(const std::vector<DataPoint>& points, 
                   std::vector<TimestampedValue>& values) override;
    
    /**
     * @brief 단일 데이터 포인트 쓰기 - 🔥 타입 수정
     * @param point 쓸 데이터 포인트
     * @param value 쓸 값
     * @return 성공 시 true
     */
    bool WriteValue(const DataPoint& point, const DataValue& value) override;
    
    /**
     * @brief 프로토콜 타입 반환
     * @return BACnet 프로토콜 타입
     */
    Enums::ProtocolType GetProtocolType() const override;
    
    /**
     * @brief 드라이버 상태 반환
     * @return 현재 상태
     */
    Structs::DriverStatus GetStatus() const override;
    
    /**
     * @brief 마지막 오류 정보 반환
     * @return 오류 정보 구조체
     */
    Structs::ErrorInfo GetLastError() const override;
    
    /**
     * @brief 드라이버 통계 반환 - 🔥 반환 타입 수정 (참조로 반환)
     * @return 표준 드라이버 통계
     */
    const DriverStatistics& GetStatistics() const override;
    
    /**
     * @brief 통계 초기화
     */
    void ResetStatistics() override;
    
    /**
     * @brief 진단 정보 반환 (디버깅용)
     * @return 상태, 설정, 통계 등의 상세 정보
     */
    std::string GetDiagnosticInfo() const override;
    
    /**
     * @brief 헬스체크 수행
     * @return 정상 시 true
     */
    bool HealthCheck() override;
    
    // ==========================================================================
    // 🔥 BACnet 특화 고급 기능들
    // ==========================================================================
    
    /**
     * @brief BACnet 디바이스 발견
     * @param devices 발견된 디바이스 정보가 저장될 맵 (device_id -> info)
     * @param timeout_ms 발견 타임아웃 (기본: 3000ms)
     * @return 발견된 디바이스 수 (실패 시 -1)
     */
    int DiscoverDevices(std::map<uint32_t, BACnetDeviceInfo>& devices, 
                       int timeout_ms = 3000);
    
    /**
     * @brief 특정 디바이스의 객체 목록 조회
     * @param device_id 대상 디바이스 ID
     * @return 객체 정보 목록
     */
    std::vector<BACnetObjectInfo> GetDeviceObjects(uint32_t device_id);
    
    /**
     * @brief COV 구독 (Change of Value)
     * @param device_id 대상 디바이스 ID
     * @param object_instance 객체 인스턴스
     * @param object_type 객체 타입
     * @param lifetime_seconds 구독 지속 시간 (기본: 300초)
     * @return 성공 시 true
     */
    bool SubscribeToCOV(uint32_t device_id, uint32_t object_instance, 
                       BACNET_OBJECT_TYPE object_type, uint32_t lifetime_seconds = 300);
    
    /**
     * @brief COV 구독 해제
     * @param device_id 대상 디바이스 ID
     * @param object_instance 객체 인스턴스
     * @param object_type 객체 타입
     * @return 성공 시 true
     */
    bool UnsubscribeFromCOV(uint32_t device_id, uint32_t object_instance, 
                           BACNET_OBJECT_TYPE object_type);
    
    /**
     * @brief Read Property Multiple 서비스
     * @param device_id 대상 디바이스 ID
     * @param objects 읽을 객체 목록
     * @param results 결과 저장소
     * @return 성공 시 true
     */
    bool ReadPropertyMultiple(uint32_t device_id, 
                             const std::vector<BACnetObjectInfo>& objects,
                             std::vector<TimestampedValue>& results);
    
    /**
     * @brief Write Property Multiple 서비스 - 🔥 타입 수정
     * @param device_id 대상 디바이스 ID
     * @param values 쓸 값들의 맵 (객체 정보 -> 값)
     * @return 성공 시 true
     */
    bool WritePropertyMultiple(uint32_t device_id,
                              const std::map<BACnetObjectInfo, DataValue>& values);
    
    /**
     * @brief BACnet 특화 통계 반환
     * @return BACnet 전용 통계 정보
     */
    const BACnetStatistics& GetBACnetStatistics() const;
    
    /**
     * @brief 복잡한 객체 매핑 (가상 포인트 지원)
     * @param virtual_point_id 가상 포인트 ID
     * @param device_id BACnet 디바이스 ID
     * @param object_type 객체 타입
     * @param object_instance 객체 인스턴스
     * @return 성공 시 true
     */
    bool MapComplexObject(const std::string& virtual_point_id,
                         uint32_t device_id,
                         BACNET_OBJECT_TYPE object_type, 
                         uint32_t object_instance);

    // ==========================================================================
    // 🔥 레거시 호환성 메서드들 (BACnetWorker와의 호환성) - 타입 수정
    // ==========================================================================
    
    /**
     * @brief 단일 데이터 읽기 (레거시 호환)
     * @param address BACnet 주소 (예: "device12345.AI.0.PV")
     * @return 읽은 데이터 값
     */
    DataValue ReadData(const std::string& address);
    
    /**
     * @brief 단일 데이터 쓰기 (레거시 호환)
     * @param address BACnet 주소
     * @param value 쓸 값
     * @return 성공 시 true
     */
    bool WriteData(const std::string& address, const DataValue& value);
    
    /**
     * @brief 디바이스 발견 (레거시 호환)
     * @return 발견된 디바이스 주소 목록
     */
    std::vector<std::string> DiscoverDevices();

protected:
    // ==========================================================================
    // 보호된 메서드들
    // ==========================================================================
    
    /**
     * @brief 드라이버 시작 시 추가 작업
     * @return 성공 시 true
     */
    virtual bool DoStart();
    
    /**
     * @brief 드라이버 정지 시 추가 작업  
     * @return 성공 시 true
     */
    virtual bool DoStop();

private:
    // ==========================================================================
    // 비공개 멤버 변수들
    // ==========================================================================
    
    // 기본 설정
    uint32_t local_device_id_;           ///< 로컬 BACnet 디바이스 ID
    std::string target_ip_;              ///< 대상 IP 주소
    uint16_t target_port_;               ///< 대상 포트 (기본: 47808)
    uint32_t max_apdu_length_;           ///< 최대 APDU 길이
    uint32_t timeout_ms_;                ///< 응답 타임아웃 (ms)
    uint32_t retry_count_;               ///< 재시도 횟수
    
    // 상태 관리
    std::atomic<bool> is_connected_;     ///< 연결 상태
    std::atomic<bool> is_initialized_;   ///< 초기화 상태
    std::atomic<bool> should_stop_;      ///< 정지 요청 플래그
    
    // 통계 및 오류
    mutable std::mutex error_mutex_;     ///< 오류 정보 뮤텍스
    Structs::ErrorInfo last_error_;      ///< 마지막 오류 정보
    mutable DriverStatistics statistics_;///< 드라이버 통계
    
    // BACnet 특화 통계
    std::unique_ptr<BACnetStatistics> bacnet_stats_;
    
    // ==========================================================================
    // 비공개 헬퍼 메서드들
    // ==========================================================================
    
    /**
     * @brief 설정 파싱
     * @param config 드라이버 설정
     */
    void ParseDriverConfig(const DriverConfig& config);
    
    /**
     * @brief BACnet 주소 파싱
     * @param address 주소 문자열 (예: "device12345.AI.0.PV")
     * @param device_id 파싱된 디바이스 ID [출력]
     * @param object_type 파싱된 객체 타입 [출력]
     * @param object_instance 파싱된 객체 인스턴스 [출력]
     * @return 파싱 성공 시 true
     */
    bool ParseBACnetAddress(const std::string& address, 
                           uint32_t& device_id,
                           BACNET_OBJECT_TYPE& object_type,
                           uint32_t& object_instance);
    
    /**
     * @brief BACnet 주소 생성
     * @param device_id 디바이스 ID
     * @param object_type 객체 타입
     * @param object_instance 객체 인스턴스
     * @return 생성된 주소 문자열
     */
    std::string CreateBACnetAddress(uint32_t device_id,
                                   BACNET_OBJECT_TYPE object_type,
                                   uint32_t object_instance);
    
    /**
     * @brief 오류 설정
     * @param code 오류 코드
     * @param message 오류 메시지
     * @param context 추가 컨텍스트 (선택사항)
     */
    void SetError(Enums::ErrorCode code, const std::string& message, 
                  const std::string& context = "");
    
    /**
     * @brief 통계 업데이트
     * @param operation 수행된 작업
     * @param success 성공 여부
     * @param response_time_ms 응답 시간 (ms)
     */
    void UpdateStatistics(const std::string& operation, bool success, 
                         uint32_t response_time_ms = 0);

#if BACNET_HEADERS_AVAILABLE
    // ==========================================================================
    // BACnet 스택 전용 메서드들 (실제 구현) - 🔥 타입 수정
    // ==========================================================================
    
    /**
     * @brief BACnet 스택 초기화
     * @return 성공 시 true
     */
    bool InitializeBACnetStack();
    
    /**
     * @brief BACnet 스택 정리
     */
    void CleanupBACnetStack();
    
    /**
     * @brief Who-Is 요청 전송
     * @param low_limit 하위 디바이스 ID 제한 (선택사항)
     * @param high_limit 상위 디바이스 ID 제한 (선택사항)
     * @return 성공 시 true
     */
    bool SendWhoIs(uint32_t low_limit = 0, uint32_t high_limit = 4194303);
    
    /**
     * @brief I-Am 응답 처리
     * @param device_id 디바이스 ID
     * @param max_apdu 최대 APDU 길이
     * @param segmentation 분할 지원 여부
     * @param vendor_id 벤더 ID
     */
    void HandleIAmResponse(uint32_t device_id, unsigned max_apdu, 
                          int segmentation, unsigned vendor_id);
    
    /**
     * @brief Read Property 요청 - 🔥 타입 수정
     * @param device_id 대상 디바이스 ID
     * @param object_type 객체 타입
     * @param object_instance 객체 인스턴스
     * @param property_id 속성 ID
     * @param value 읽은 값 [출력]
     * @return 성공 시 true
     */
    bool ReadProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                     uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                     DataValue& value);
    
    /**
     * @brief Write Property 요청 - 🔥 타입 수정
     * @param device_id 대상 디바이스 ID
     * @param object_type 객체 타입
     * @param object_instance 객체 인스턴스
     * @param property_id 속성 ID
     * @param value 쓸 값
     * @return 성공 시 true
     */
    bool WriteProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                      uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                      const DataValue& value);
#endif
};

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_DRIVER_H