/**
 * @file BACnetDriver.h
 * @brief BACnet 드라이버 클래스 - 중복 구조체 제거 버전
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 1.0.0
 */

#ifndef BACNET_DRIVER_H
#define BACNET_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Drivers/Bacnet/BACnetCommonTypes.h"  // ✅ 공통 타입들 포함
#include "Drivers/Bacnet/BACnetErrorMapper.h"
#include "Drivers/Bacnet/BACnetStatisticsManager.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <atomic>

#ifdef HAS_BACNET_STACK
extern "C" {
    #include <bacnet/config.h>
    #include <bacnet/bacdef.h>
    #include <bacnet/bacenum.h>
    #include <bacnet/bacerror.h>
    #include <bacnet/bactext.h>
    #include <bacnet/bacapp.h>
    #include <bacnet/bacdcode.h>
    #include <bacnet/bacaddr.h>
    #include <bacnet/npdu.h>
    #include <bacnet/apdu.h>
    
    // 데이터링크 (조건부 - 파일이 있는지 확인)
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
    
    #include <bacnet/basic/object/device.h>
} // ✅ extern "C" 블록 올바르게 닫기
    #define BACNET_HEADERS_AVAILABLE 1
#else
    #define BACNET_HEADERS_AVAILABLE 0
#endif

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 🔥 타입 별칭 정의 (중복 제거)
// =============================================================================
using DataPoint = PulseOne::Structs::DataPoint;
using DataValue = PulseOne::Structs::DataValue;
using TimestampedValue = PulseOne::Structs::TimestampedValue;
using DriverConfig = PulseOne::Structs::DriverConfig;
using DriverStatistics = PulseOne::Structs::DriverStatistics;

// BACnet 구조체들은 BACnetCommonTypes.h에서 정의됨 (중복 제거)
// BACnetDeviceInfo, BACnetObjectInfo는 BACnetCommonTypes.h 사용

/**
 * @brief BACnet 드라이버 클래스 (올바른 인터페이스 구현)
 */
class BACnetDriver : public IProtocolDriver {
public:
    // ==========================================================================
    // 생성자 및 소멸자
    // ==========================================================================
    BACnetDriver();
    virtual ~BACnetDriver();
    
    // 복사/이동 방지
    BACnetDriver(const BACnetDriver&) = delete;
    BACnetDriver& operator=(const BACnetDriver&) = delete;
    BACnetDriver(BACnetDriver&&) = delete;
    BACnetDriver& operator=(BACnetDriver&&) = delete;
    
    // ==========================================================================
    // 🔥 IProtocolDriver 인터페이스 구현 (올바른 시그니처)
    // ==========================================================================
    
    /**
     * @brief 드라이버 초기화 (IProtocolDriver 인터페이스)
     */
    bool Initialize(const DriverConfig& config) override;
    
    /**
     * @brief 드라이버 연결 (IProtocolDriver 인터페이스)
     */
    bool Connect() override;
    
    /**
     * @brief 드라이버 연결 해제 (IProtocolDriver 인터페이스)
     */
    bool Disconnect() override;
    
    /**
     * @brief 연결 상태 확인 (IProtocolDriver 인터페이스)
     */
    bool IsConnected() const override;
    
    /**
     * @brief 단일 데이터 읽기 (IProtocolDriver 인터페이스)
     */
    DataValue ReadData(const std::string& address) override;
    
    /**
     * @brief 단일 데이터 쓰기 (IProtocolDriver 인터페이스)
     */
    bool WriteData(const std::string& address, const DataValue& value) override;
    
    /**
     * @brief 다중 데이터 읽기 (IProtocolDriver 인터페이스)
     */
    bool ReadValues(const std::vector<DataPoint>& points, 
                   std::vector<TimestampedValue>& values) override;
    
    /**
     * @brief 다중 데이터 쓰기 (IProtocolDriver 인터페이스)
     */
    bool WriteValues(const std::vector<DataPoint>& points, 
                    const std::vector<DataValue>& values) override;
    
    /**
     * @brief 마지막 에러 조회 (IProtocolDriver 인터페이스)
     */
    Structs::DriverError GetLastError() const override;
    
    /**
     * @brief 드라이버 통계 조회 (IProtocolDriver 인터페이스)
     */
    const DriverStatistics& GetStatistics() const override;
    
    /**
     * @brief 디바이스 주소 목록 조회 (IProtocolDriver 인터페이스)
     */
    std::vector<std::string> DiscoverDevices() override;
    
    // ==========================================================================
    // 🔥 BACnet 특화 인터페이스 (BACnetCommonTypes.h 구조체 사용)
    // ==========================================================================
    
    /**
     * @brief BACnet 디바이스 발견 (새로운 API)
     * @param devices 발견된 디바이스 맵 (key: device_id)
     * @param timeout_ms 타임아웃 (밀리초)
     * @return 발견된 디바이스 수
     */
    int DiscoverDevices(std::map<uint32_t, BACnetDeviceInfo>& devices, 
                       uint32_t timeout_ms = 3000);
    
    /**
     * @brief 디바이스의 객체 목록 조회
     * @param device_id 디바이스 ID
     * @return 객체 정보 벡터
     */
    std::vector<BACnetObjectInfo> GetDeviceObjects(uint32_t device_id);
    
    /**
     * @brief BACnet 특화 통계
     */
    const BACnetStatistics& GetBACnetStatistics() const;
    
    /**
     * @brief 진단 정보
     */
    std::string GetDiagnosticInfo() const;
    
    /**
     * @brief 헬스 체크
     */
    bool HealthCheck() const;
    
    /**
     * @brief COV 구독
     */
    bool SubscribeToCOV(uint32_t device_id, uint32_t object_instance, 
                       BACNET_OBJECT_TYPE object_type, uint32_t lifetime_seconds = 300);
    
    /**
     * @brief COV 구독 해제
     */
    bool UnsubscribeFromCOV(uint32_t device_id, uint32_t object_instance, 
                           BACNET_OBJECT_TYPE object_type);

    // ==========================================================================
    // 🔥 공개 유틸리티 메서드들
    // ==========================================================================
    
    /**
     * @brief BACnet 주소 파싱
     * @param address "device123.AI.0.PV" 형식의 주소
     * @param device_id 파싱된 디바이스 ID
     * @param object_type 파싱된 객체 타입
     * @param object_instance 파싱된 객체 인스턴스
     * @param property_id 파싱된 프로퍼티 ID
     * @return 파싱 성공 시 true
     */
    static bool ParseBACnetAddress(const std::string& address,
                                  uint32_t& device_id,
                                  BACNET_OBJECT_TYPE& object_type,
                                  uint32_t& object_instance,
                                  BACNET_PROPERTY_ID& property_id);
    
    /**
     * @brief BACnet 주소 생성
     */
    static std::string CreateBACnetAddress(uint32_t device_id,
                                          BACNET_OBJECT_TYPE object_type,
                                          uint32_t object_instance,
                                          BACNET_PROPERTY_ID property_id = PROP_PRESENT_VALUE);

protected:
    // ==========================================================================
    // 보호된 메서드들
    // ==========================================================================
    bool DoStart();
    bool DoStop();

private:
    // ==========================================================================
    // 🔥 내부 BACnet 메서드들 (private로 변경)
    // ==========================================================================
    
    /**
     * @brief BACnet 프로퍼티 읽기 (내부 전용)
     */
    bool ReadProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                     uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                     DataValue& result);
    
    /**
     * @brief BACnet 프로퍼티 쓰기 (내부 전용)
     */
    bool WriteProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                      uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                      const DataValue& value);

    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // 컴포넌트들
    std::unique_ptr<BACnetErrorMapper> error_mapper_;
    std::unique_ptr<BACnetStatisticsManager> statistics_manager_;
    
    // 상태 관리
    std::atomic<Structs::DriverStatus> status_{Structs::DriverStatus::UNINITIALIZED};
    mutable std::mutex config_mutex_;
    BACnetDriverConfig config_;
    
    // BACnet 스택 관련
    std::atomic<bool> stack_initialized_{false};
    std::mutex stack_mutex_;
    
    // 디바이스 관리
    mutable std::mutex devices_mutex_;
    std::map<uint32_t, BACnetDeviceInfo> discovered_devices_;
    std::chrono::steady_clock::time_point last_discovery_;
    
    // 통신 관리
    std::atomic<bool> communication_enabled_{false};
    std::mutex communication_mutex_;
    
    // 통계 및 에러
    mutable std::mutex statistics_mutex_;
    mutable DriverStatistics driver_statistics_;
    mutable Structs::DriverError last_error_;
    
    // 스레드 관리
    std::atomic<bool> worker_running_{false};
    std::unique_ptr<std::thread> worker_thread_;
    std::condition_variable worker_cv_;
    
    // ==========================================================================
    // 내부 헬퍼 메서드들
    // ==========================================================================
    
    // 초기화 및 정리
    bool InitializeBACnetStack();
    void ShutdownBACnetStack();
    bool ConfigureLocalDevice();
    
    // 통신
    bool SendWhoIs(uint32_t timeout_ms = 3000);
    bool ProcessIncomingMessages();
    void WorkerThreadFunction();
    
    // 데이터 변환
    bool BACnetValueToDataValue(const BACNET_APPLICATION_DATA_VALUE& bacnet_value, 
                               DataValue& data_value);
    bool DataValueToBACnetValue(const DataValue& data_value, 
                               BACNET_APPLICATION_DATA_VALUE& bacnet_value);
    
    // 에러 및 통계
    void SetLastError(Structs::ErrorCode code, const std::string& message);
    void UpdateStatistics(bool success, const std::string& operation);
    
    // 디바이스 관리
    void UpdateDiscoveredDevice(uint32_t device_id, const BACnetDeviceInfo& device_info);
    bool GetDiscoveredDevice(uint32_t device_id, BACnetDeviceInfo& device_info) const;
    void CleanupStaleDevices();
    
#ifdef HAS_BACNET_STACK
    // BACnet 스택 특화 헬퍼들
    bool SendRPRequest(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                      uint32_t object_instance, BACNET_PROPERTY_ID property_id);
    bool SendWPRequest(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                      uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                      const BACNET_APPLICATION_DATA_VALUE& value);
    
    // 콜백 함수들 (C 함수로 구현)
    static void IAmCallback(uint8_t* service_request, uint16_t service_len, BACNET_ADDRESS* src);
    static void RPAckCallback(uint8_t* service_request, uint16_t service_len, BACNET_ADDRESS* src);
    static void ErrorCallback(BACNET_ADDRESS* src, uint8_t invoke_id, BACNET_ERROR_CLASS error_class, BACNET_ERROR_CODE error_code);
#endif
};

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_DRIVER_H