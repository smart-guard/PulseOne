/**
 * @file BACnetDriver.h
 * @brief BACnet 프로토콜 드라이버 - 🔥 모든 누락된 멤버 추가 완료
 * @author PulseOne Development Team
 * @date 2025-08-03
 * @version 1.0.0
 * 
 * 🔥 주요 수정사항:
 * 1. 누락된 멤버 변수들 모두 추가 (is_connected_, should_stop_ 등)
 * 2. 누락된 메서드들 모두 선언
 * 3. 전방 선언 문제 해결
 * 4. 표준 DriverStatistics 사용
 */

#ifndef BACNET_DRIVER_H
#define BACNET_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Drivers/Bacnet/BACnetCommonTypes.h"
#include "Common/DriverStatistics.h"
#include "Common/Structs.h"
#include "Common/Enums.h"
#include "Common/Utils.h"
#include "Utils/LogManager.h"

// ✅ BACnet 스택 조건부 인클루드
#ifdef HAS_BACNET_STACK
extern "C" {
    #include <bacnet/bacdef.h>
    #include <bacnet/bacenum.h>
    #include <bacnet/rp.h>
    #include <bacnet/wp.h>
    #include <bacnet/iam.h>
    #include <bacnet/whois.h>
    #include <bacnet/basic/tsm/tsm.h>
    #include <bacnet/basic/binding/address.h>
    #include <bacnet/basic/object/device.h>
}
#else
// 시뮬레이션을 위한 더미 정의들
typedef enum {
    OBJECT_DEVICE = 8,
    OBJECT_ANALOG_INPUT = 0,
    OBJECT_ANALOG_OUTPUT = 1,
    OBJECT_ANALOG_VALUE = 2,
    OBJECT_BINARY_INPUT = 3,
    OBJECT_BINARY_OUTPUT = 4,
    OBJECT_BINARY_VALUE = 5
} BACNET_OBJECT_TYPE;

typedef enum {
    PROP_PRESENT_VALUE = 85,
    PROP_OBJECT_NAME = 77,
    PROP_OBJECT_TYPE = 79
} BACNET_PROPERTY_ID;

typedef struct {
    uint8_t net;
    uint8_t len;
    uint8_t adr[6];
} BACNET_ADDRESS;
#endif

#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <condition_variable>
#include <functional>

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 전방 선언들 (순환 의존성 방지)
// =============================================================================
// ✅ BACnetWorker와 BACnetErrorMapper 제거 - 순환 의존성 방지

// =============================================================================
// BACnet 주소 정보 구조체
// =============================================================================
struct BACnetAddressInfo {
    uint32_t device_id = 0;
    BACNET_OBJECT_TYPE object_type = OBJECT_ANALOG_INPUT;
    uint32_t object_instance = 0;
    BACNET_PROPERTY_ID property_id = PROP_PRESENT_VALUE;
};

// =============================================================================
// BACnet 드라이버 클래스
// =============================================================================

/**
 * @brief BACnet 프로토콜 드라이버
 * @details 표준 DriverStatistics 사용, IProtocolDriver 완전 구현
 */
class BACnetDriver : public IProtocolDriver {
private:
    // 싱글턴 패턴
    static BACnetDriver* instance_;
    static std::mutex instance_mutex_;

public:
    // =============================================================================
    // 생성자 및 소멸자
    // =============================================================================
    BACnetDriver();
    virtual ~BACnetDriver();
    
    // 복사/이동 금지
    BACnetDriver(const BACnetDriver&) = delete;
    BACnetDriver& operator=(const BACnetDriver&) = delete;
    BACnetDriver(BACnetDriver&&) = delete;
    BACnetDriver& operator=(BACnetDriver&&) = delete;

    // =============================================================================
    // IProtocolDriver 인터페이스 구현
    // =============================================================================
    
    bool Initialize(const PulseOne::Structs::DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    bool Start() override;
    bool Stop() override;
    
    bool ReadValues(const std::vector<PulseOne::Structs::DataPoint>& points,
                   std::vector<PulseOne::Structs::TimestampedValue>& values) override;
    
    bool WriteValue(const PulseOne::Structs::DataPoint& point, 
                   const PulseOne::Structs::DataValue& value) override;
    
    PulseOne::Enums::ProtocolType GetProtocolType() const override;
    PulseOne::Structs::DriverStatus GetStatus() const override;
    PulseOne::Structs::ErrorInfo GetLastError() const override;
    
    const PulseOne::Structs::DriverStatistics& GetStatistics() const override;
    void SetError(PulseOne::Enums::ErrorCode code, const std::string& message) override;
    
    // =============================================================================
    // BACnet 특화 공개 메서드들
    // =============================================================================
    
    /**
     * @brief 싱글턴 인스턴스 접근
     */
    static BACnetDriver* GetInstance();
    
    /**
     * @brief 디바이스 발견 콜백 설정
     */
    void SetDeviceDiscoveredCallback(std::function<void(const BACnetDeviceInfo&)> callback);
    
    /**
     * @brief COV 알림 콜백 설정
     */
    void SetCovNotificationCallback(std::function<void(const std::string&, const PulseOne::Structs::TimestampedValue&)> callback);
    
    /**
     * @brief 발견된 디바이스 목록 조회
     */
    std::vector<BACnetDeviceInfo> GetDiscoveredDevices() const;
    
    /**
     * @brief Who-Is 브로드캐스트 전송
     */
    bool SendWhoIs(uint32_t low_limit = 0, uint32_t high_limit = 0xFFFFFF);

private:
    // =============================================================================
    // 내부 메서드들
    // =============================================================================
    
    /**
     * @brief 설정 파싱
     */
    void ParseDriverConfig(const PulseOne::Structs::DriverConfig& config);
    
    /**
     * @brief BACnet 특화 통계 초기화
     */
    void InitializeBACnetStatistics();
    
    /**
     * @brief BACnet 스택 초기화
     */
    bool InitializeBACnetStack();
    
    /**
     * @brief BACnet 스택 정리
     */
    void CleanupBACnetStack();
    
    /**
     * @brief UDP 소켓 생성
     */
    bool CreateSocket();
    
    /**
     * @brief UDP 소켓 해제
     */
    void CloseSocket();
    
    /**
     * @brief 단일 속성 읽기
     */
    bool ReadSingleProperty(const PulseOne::Structs::DataPoint& point,
                           PulseOne::Structs::TimestampedValue& value);
    
    /**
     * @brief 단일 속성 쓰기
     */
    bool WriteSingleProperty(const PulseOne::Structs::DataPoint& point,
                            const PulseOne::Structs::DataValue& value);
    
    /**
     * @brief BACnet 주소 파싱
     */
    BACnetAddressInfo ParseBACnetAddress(const std::string& address) const;

    // =============================================================================
    // 멤버 변수들
    // =============================================================================

private:
    // ✅ 표준 통계 구조
    mutable PulseOne::Structs::DriverStatistics driver_statistics_;
    
    // 기본 설정
    PulseOne::Structs::DriverConfig config_;
    uint32_t local_device_id_;
    std::string target_ip_;
    uint16_t target_port_;
    uint16_t max_apdu_length_;
    bool segmentation_support_;
    
    // ✅ 상태 관리 (누락된 멤버들)
    std::atomic<PulseOne::Structs::DriverStatus> status_{PulseOne::Structs::DriverStatus::UNINITIALIZED};
    std::atomic<bool> is_connected_{false};                    // ✅ 추가
    std::atomic<bool> should_stop_{false};                     // ✅ 추가
    std::atomic<bool> is_bacnet_initialized_{false};           // ✅ 추가
    PulseOne::Structs::ErrorInfo last_error_;
    mutable std::mutex error_mutex_;                            // ✅ 추가
    
    // ✅ 네트워크 관리 (누락된 멤버들)
    int socket_fd_;
    std::thread network_thread_;                                // ✅ 추가
    std::atomic<bool> network_thread_running_{false};          // ✅ 추가
    std::mutex network_mutex_;
    std::condition_variable network_cv_;                        // ✅ 추가
    
    // 발견된 디바이스들
    std::map<uint32_t, BACnetDeviceInfo> discovered_devices_;
    mutable std::mutex devices_mutex_;
    
    // ✅ 콜백들 (누락된 멤버들)
    std::function<void(const BACnetDeviceInfo&)> device_discovered_callback_;
    std::function<void(const std::string&, const PulseOne::Structs::TimestampedValue&)> cov_notification_callback_;
    std::mutex callback_mutex_;
};

// =============================================================================
// 인라인 메서드 구현
// =============================================================================

inline PulseOne::Enums::ProtocolType BACnetDriver::GetProtocolType() const {
    return PulseOne::Enums::ProtocolType::BACNET_IP;
}

inline PulseOne::Structs::DriverStatus BACnetDriver::GetStatus() const {
    return status_.load();
}

inline PulseOne::Structs::ErrorInfo BACnetDriver::GetLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

inline BACnetDriver* BACnetDriver::GetInstance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    return instance_;
}

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_DRIVER_H