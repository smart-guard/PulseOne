
#ifndef BACNET_DRIVER_H
#define BACNET_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Drivers/Bacnet/BACnetCommonTypes.h"
#include "Drivers/Bacnet/BACnetErrorMapper.h"
#include "Drivers/Bacnet/BACnetStatisticsManager.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <atomic>

// 조건부 BACnet 스택 포함
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
    #include <bacnet/datalink/datalink.h>
    #include <bacnet/datalink/bip.h>
}
#endif

namespace PulseOne {
namespace Drivers {

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
     * @brief 디바이스 연결 (IProtocolDriver 인터페이스)
     */
    bool Connect() override;
    
    /**
     * @brief 디바이스 연결 해제 (IProtocolDriver 인터페이스)
     */
    bool Disconnect() override;
    
    /**
     * @brief 연결 상태 확인 (IProtocolDriver 인터페이스)
     */
    bool IsConnected() const override;
    
    /**
     * @brief 다중 데이터 포인트 읽기 (IProtocolDriver 인터페이스)
     */
    bool ReadValues(const std::vector<Structs::DataPoint>& points,
                   std::vector<TimestampedValue>& values) override;
    
    /**
     * @brief 단일 값 쓰기 (IProtocolDriver 인터페이스)
     */
    bool WriteValue(const Structs::DataPoint& point, 
                   const Structs::DataValue& value) override;
    
    /**
     * @brief 프로토콜 타입 반환 (IProtocolDriver 인터페이스)
     */
    Enums::ProtocolType GetProtocolType() const override;
    
    /**
     * @brief 드라이버 상태 반환 (IProtocolDriver 인터페이스)
     */
    Structs::DriverStatus GetStatus() const override;
    
    /**
     * @brief 마지막 에러 정보 반환 (IProtocolDriver 인터페이스)
     */
    Structs::ErrorInfo GetLastError() const override;
    
    /**
     * @brief 통계 정보 반환 (IProtocolDriver 인터페이스)
     */
    const DriverStatistics& GetStatistics() const override;
    
    /**
     * @brief 통계 리셋 (IProtocolDriver 인터페이스)
     */
    void ResetStatistics() override;
    
    // ==========================================================================
    // 🔥 BACnet 특화 메서드 (기존 BACnetWorker 호환)
    // ==========================================================================
    
    /**
     * @brief BACnet 디바이스 검색
     */
    std::vector<std::string> DiscoverDevices();
    
    /**
     * @brief 다중 주소 읽기 (BACnetWorker 호환)
     */
    std::map<std::string, Structs::DataValue> ReadMultiple(
        const std::vector<std::string>& addresses);
    
    /**
     * @brief 단일 데이터 읽기 (BACnetWorker 호환)
     */
    Structs::DataValue ReadData(const std::string& address);
    
    /**
     * @brief 단일 데이터 쓰기 (BACnetWorker 호환)
     */
    bool WriteData(const std::string& address, const Structs::DataValue& value);
    
    /**
     * @brief BACnet 객체 읽기
     */
    bool ReadBACnetObject(uint32_t device_id, uint32_t object_type,
                         uint32_t object_instance, uint32_t property_id,
                         Structs::DataValue& result);
    
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
     * @brief 실행 상태 확인 (BACnetWorker 호환)
     */
    bool IsRunning() const;
    
    /**
     * @brief 드라이버 시작 (BACnetWorker 호환)
     */
    bool Start();
    
    /**
     * @brief 드라이버 정지 (BACnetWorker 호환)
     */
    bool Stop();

protected:
    // ==========================================================================
    // 보호된 메서드들
    // ==========================================================================
    bool DoStart();
    bool DoStop();

private:
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // 컴포넌트들
    std::unique_ptr<BACnetErrorMapper> error_mapper_;
    std::unique_ptr<BACnetStatisticsManager> statistics_manager_;
    
    // 상태 관리
    std::atomic<Structs::DriverStatus> status_{Structs::DriverStatus::UNINITIALIZED};
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> is_running_{false};
    std::atomic<bool> should_stop_{false};
    
    // BACnet 설정
    uint32_t local_device_id_;
    std::string target_ip_;
    uint16_t target_port_;
    
    // 스레드 관리
    std::thread network_thread_;
    std::mutex network_mutex_;
    std::condition_variable network_condition_;
    
    // 디바이스 관리
    std::mutex devices_mutex_;
    std::map<uint32_t, BACnetDeviceInfo> discovered_devices_;
    
    // 에러 상태
    mutable std::mutex error_mutex_;
    mutable Structs::ErrorInfo last_error_;
    mutable DriverStatistics statistics_cache_;
    
    // ==========================================================================
    // 내부 메서드들
    // ==========================================================================
    void NetworkThreadFunction();
    bool InitializeBACnetStack();
    void CleanupBACnetStack();
    void UpdateStatistics() const;
    void SetError(const std::string& message, Enums::ErrorCode code = Enums::ErrorCode::INTERNAL_ERROR);
    
    // BACnet 콜백들 (static)
    static BACnetDriver* instance_;
    
#ifdef HAS_BACNET_STACK
    static void IAmHandler(uint8_t* service_request, uint16_t service_len,
                          BACNET_ADDRESS* src, BACNET_CONFIRMED_SERVICE_DATA* service_data);
    static void ReadPropertyAckHandler(uint8_t* service_request, uint16_t service_len,
                                      BACNET_ADDRESS* src, BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data);
    static void ErrorHandler(BACNET_ADDRESS* src, uint8_t invoke_id,
                            uint8_t error_class, uint8_t error_code);
#endif
};

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_DRIVER_H