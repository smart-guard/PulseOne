// =============================================================================
// collector/include/Drivers/Bacnet/BACnetDriver.h
// 🔥 완성된 BACnet 드라이버 헤더 - 기존 호환성 + 고급 기능
// =============================================================================

#ifndef BACNET_DRIVER_H
#define BACNET_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Drivers/Bacnet/BACnetErrorMapper.h"
#include "Drivers/Bacnet/BACnetStatisticsManager.h"
#include "Drivers/Bacnet/BACnetCommonTypes.h"
#include "Common/UnifiedCommonTypes.h"
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <map>
#include <condition_variable>

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
    #include <bacnet/basic/client/bac-rw.h>
    #include <bacnet/wp.h>
    #include <bacnet/rp.h>
    #include <bacnet/whois.h>
    #include <bacnet/iam.h>
    #include <bacnet/device.h>
    #include <bacnet/basic/tsm/tsm.h>
    #include <bacnet/basic/client/bac-discover.h>
}
#else
// BACnet 스택이 없는 경우를 위한 더미 정의들
typedef enum {
    OBJECT_ANALOG_INPUT = 0,
    OBJECT_ANALOG_OUTPUT = 1,
    OBJECT_BINARY_INPUT = 3,
    OBJECT_BINARY_OUTPUT = 4,
    OBJECT_ANALOG_VALUE = 2,
    OBJECT_BINARY_VALUE = 5,
    OBJECT_DEVICE = 8
} BACNET_OBJECT_TYPE;

typedef enum {
    PROP_PRESENT_VALUE = 85,
    PROP_OUT_OF_SERVICE = 81,
    PROP_OBJECT_NAME = 77,
    PROP_OBJECT_LIST = 76
} BACNET_PROPERTY_ID;

#define BACNET_ARRAY_ALL (-1)
#define MAX_MPDU 1476
#define BACNET_MAX_INSTANCE 0x3FFFFF
#endif

namespace PulseOne {
namespace Drivers {

// 전방 선언
class BACnetServiceManager;
class BACnetCOVManager;
class BACnetObjectMapper;

/**
 * @brief 완성된 BACnet 드라이버 클래스
 * 
 * 기능:
 * - 기존 BACnetWorker와 완전 호환
 * - IProtocolDriver 인터페이스 완전 구현
 * - 실제 BACnet 스택 연동
 * - 모듈화된 구조 (에러 매퍼, 통계 관리자 등)
 * - 고급 BACnet 서비스 지원 (RPM, WPM, COV 등)
 * - 멀티스레드 안전성
 * - 디바이스 발견 및 객체 매핑
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
    // IProtocolDriver 필수 인터페이스 구현
    // ==========================================================================
    
    /**
     * @brief 드라이버 초기화
     * @param config 드라이버 설정 (device_id, target_ip, port 등)
     * @return 성공 시 true
     */
    bool Initialize(const DriverConfig& config) override;
    
    /**
     * @brief BACnet 네트워크 연결
     * @return 성공 시 true
     */
    bool Connect() override;
    
    /**
     * @brief 연결 해제 및 리소스 정리
     * @return 성공 시 true
     */
    bool Disconnect() override;
    
    /**
     * @brief 연결 상태 확인
     * @return 연결된 경우 true
     */
    bool IsConnected() const override;
    
    /**
     * @brief 다중 BACnet 포인트 읽기
     * @param points 읽을 데이터 포인트들 (주소: "device123.AI.0.PV" 형식)
     * @param values 읽은 값들이 저장될 벡터
     * @return 성공 시 true (부분 실패도 포함)
     */
    bool ReadValues(const std::vector<Structs::DataPoint>& points,
                   std::vector<TimestampedValue>& values) override;
    
    /**
     * @brief 단일 BACnet 포인트 쓰기
     * @param point 쓸 데이터 포인트
     * @param value 쓸 값
     * @return 성공 시 true
     */
    bool WriteValue(const Structs::DataPoint& point,
                   const Structs::DataValue& value) override;
    
    /**
     * @brief 프로토콜 타입 반환
     * @return BACNET_IP
     */
    Enums::ProtocolType GetProtocolType() const override;
    
    /**
     * @brief 현재 드라이버 상태 반환
     * @return 드라이버 상태 (UNINITIALIZED, INITIALIZED, RUNNING, ERROR 등)
     */
    Structs::DriverStatus GetStatus() const override;
    
    /**
     * @brief 마지막 에러 정보 반환
     * @return 에러 정보 (코드, 메시지, 타임스탬프 등)
     */
    Structs::ErrorInfo GetLastError() const override;
    
    /**
     * @brief 표준 드라이버 통계 반환
     * @return 읽기/쓰기 통계, 에러 카운트 등
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
     * @brief Write Property Multiple 서비스
     * @param device_id 대상 디바이스 ID
     * @param values 쓸 값들의 맵 (객체 정보 -> 값)
     * @return 성공 시 true
     */
    bool WritePropertyMultiple(uint32_t device_id,
                              const std::map<BACnetObjectInfo, Structs::DataValue>& values);
    
    /**
     * @brief 복잡한 객체 매핑 (가상 포인트 생성용)
     * @param object_identifier 객체 식별자 (사용자 정의)
     * @param device_id 디바이스 ID
     * @param object_type 객체 타입
     * @param object_instance 객체 인스턴스
     * @param properties 읽을 속성 목록 (기본: Present Value만)
     * @return 성공 시 true
     */
    bool MapComplexObject(const std::string& object_identifier,
                         uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                         uint32_t object_instance,
                         const std::vector<BACNET_PROPERTY_ID>& properties = {});
    
    /**
     * @brief BACnet 특화 통계 정보 반환
     * @return BACnet 프로토콜 전용 통계
     */
    const BACnetStatistics& GetBACnetStatistics() const;
    
    // ==========================================================================
    // 🔥 기존 BACnetWorker 호환 메서드들 (레거시 지원)
    // ==========================================================================
    
    /**
     * @brief 단일 데이터 읽기 (기존 BACnetWorker 호환)
     * @param address BACnet 주소 문자열
     * @return 읽은 값
     */
    Structs::DataValue ReadData(const std::string& address);
    
    /**
     * @brief 단일 데이터 쓰기 (기존 BACnetWorker 호환)
     * @param address BACnet 주소 문자열
     * @param value 쓸 값
     * @return 성공 시 true
     */
    bool WriteData(const std::string& address, const Structs::DataValue& value);
    
    /**
     * @brief 다중 주소 읽기 (기존 BACnetWorker 호환)
     * @param addresses 주소 목록
     * @return 주소별 값 맵
     */
    std::map<std::string, Structs::DataValue> ReadMultiple(
        const std::vector<std::string>& addresses);
    
    /**
     * @brief 디바이스 발견 (기존 호환, 문자열 목록 반환)
     * @return 발견된 디바이스들의 주소 목록
     */
    std::vector<std::string> DiscoverDevices();
    
    // ==========================================================================
    // 정적 인스턴스 접근 (기존 BACnetWorker 호환)
    // ==========================================================================
    
    /**
     * @brief 싱글톤 인스턴스 접근
     * @return 현재 활성 인스턴스 (없으면 nullptr)
     */
    static BACnetDriver* GetInstance() {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        return instance_;
    }

protected:
    // ==========================================================================
    // IProtocolDriver 보호된 메서드들
    // ==========================================================================
    
    /**
     * @brief 드라이버 시작 (백그라운드 작업 시작)
     * @return 성공 시 true
     */
    bool DoStart() override;
    
    /**
     * @brief 드라이버 정지 (백그라운드 작업 정지)
     * @return 성공 시 true
     */
    bool DoStop() override;

private:
    // ==========================================================================
    // 🔥 내부 구조체 및 타입 정의
    // ==========================================================================
    
    /**
     * @brief BACnet 주소 파싱 결과
     */
    struct BACnetAddress {
        uint32_t device_id;
        BACNET_OBJECT_TYPE object_type;
        uint32_t object_instance;
        BACNET_PROPERTY_ID property_id;
        int32_t array_index = BACNET_ARRAY_ALL;
    };
    
    /**
     * @brief 펜딩 요청 정보
     */
    struct PendingRequest {
        uint8_t invoke_id;
        std::chrono::system_clock::time_point timeout;
        uint8_t service_choice;
        uint32_t target_device_id;
        TimestampedValue response_value;
        std::atomic<bool> completed{false};
        std::atomic<bool> success{false};
    };
    
    // ==========================================================================
    // 🔥 모듈화된 컴포넌트들
    // ==========================================================================
    
    // 핵심 매니저들
    std::unique_ptr<BACnetErrorMapper> error_mapper_;
    std::unique_ptr<BACnetStatisticsManager> statistics_manager_;
    
    // 고급 서비스 매니저들 (필요시 활성화)
    std::unique_ptr<BACnetServiceManager> service_manager_;
    std::unique_ptr<BACnetCOVManager> cov_manager_;
    std::unique_ptr<BACnetObjectMapper> object_mapper_;
    
    // ==========================================================================
    // 상태 관리
    // ==========================================================================
    
    std::atomic<Structs::DriverStatus> status_{Structs::DriverStatus::UNINITIALIZED};
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> should_stop_{false};
    
    // 설정
    DriverConfig config_;
    uint32_t local_device_id_;
    std::string target_ip_;
    uint16_t target_port_;
    
    // ==========================================================================
    // 네트워크 및 스레드 관리
    // ==========================================================================
    
    std::atomic<bool> network_thread_running_{false};
    std::atomic<bool> is_bacnet_initialized_{false};
    std::thread network_thread_;
    std::mutex network_mutex_;
    std::condition_variable network_condition_;
    int socket_fd_;
    
    // ==========================================================================
    // 에러 및 요청 관리
    // ==========================================================================
    
    mutable std::mutex error_mutex_;
    mutable Structs::ErrorInfo last_error_;
    
    std::mutex request_mutex_;
    std::map<uint8_t, std::unique_ptr<PendingRequest>> pending_requests_;
    
    // 발견된 디바이스 캐시
    std::mutex devices_mutex_;
    std::map<uint32_t, BACnetDeviceInfo> discovered_devices_;
    std::chrono::system_clock::time_point last_discovery_time_;
    
    // ==========================================================================
    // 정적 멤버들 (싱글톤 지원)
    // ==========================================================================
    
    static BACnetDriver* instance_;
    static std::mutex instance_mutex_;
    
    // ==========================================================================
    // 🔥 비공개 헬퍼 메서드들
    // ==========================================================================
    
    // 초기화 및 정리
    void ParseDriverConfig(const DriverConfig& config);
    bool InitializeBACnetStack();
    void CleanupBACnetStack();
    
    // 네트워크 관리
    bool StartNetworkThread();
    void StopNetworkThread();
    void NetworkThreadFunction();
    void ProcessIncomingMessages();
    void ManageTimeouts();
    
    // 읽기/쓰기 구현
    bool ReadSingleValue(const Structs::DataPoint& point, TimestampedValue& value);
    bool WriteSingleValue(const Structs::DataPoint& point, const Structs::DataValue& value);
    
    // BACnet 주소 파싱
    bool ParseBACnetAddress(const std::string& address, BACnetAddress& addr);
    
#ifdef HAS_BACNET_STACK
    // BACnet 스택 인터페이스
    uint8_t SendReadPropertyRequest(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                                   uint32_t object_instance, BACNET_PROPERTY_ID property_id);
    uint8_t SendWritePropertyRequest(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                                    uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                                    const Structs::DataValue& value, int32_t array_index = BACNET_ARRAY_ALL,
                                    uint8_t priority = 16);
    
    // 타입 변환
    void ConvertDataValueToBACnet(const Structs::DataValue& value, 
                                 BACNET_APPLICATION_DATA_VALUE& bacnet_value);
    bool ConvertBACnetToDataValue(const BACNET_APPLICATION_DATA_VALUE& bacnet_value,
                                 Structs::DataValue& value);
    
    // 응답 처리
    bool CheckResponseReceived(uint8_t invoke_id, TimestampedValue& value);
    bool CheckWriteResponseReceived(uint8_t invoke_id);
    
    // BACnet 콜백 함수들 (정적)
    static void ReadPropertyAckHandler(uint8_t* service_request, uint16_t service_len,
                                      BACNET_ADDRESS* src, BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data);
    static void ErrorHandler(BACNET_ADDRESS* src, uint8_t invoke_id, BACNET_ERROR_CLASS error_class,
                            BACNET_ERROR_CODE error_code);
    static void AbortHandler(BACNET_ADDRESS* src, uint8_t invoke_id, uint8_t abort_reason, bool server);
    static void RejectHandler(BACNET_ADDRESS* src, uint8_t invoke_id, uint8_t reject_reason);
    static void IAMHandler(uint8_t* service_request, uint16_t service_len, BACNET_ADDRESS* src);
#endif
    
    // 에러 관리
    void SetError(Enums::ErrorCode code, const std::string& message);
    void SetBACnetError(uint8_t error_class, uint8_t error_code, const std::string& context = "");
    
    // 유틸리티
    std::string AddressToString(const BACnetAddress& addr) const;
    std::string ObjectTypeToString(BACNET_OBJECT_TYPE type) const;
    std::string PropertyIdToString(BACNET_PROPERTY_ID prop) const;
    
    // 검증
    bool ValidateDriverConfig(const DriverConfig& config) const;
    bool ValidateBACnetAddress(const std::string& address) const;
    
    // 디바이스 캐시 관리
    void UpdateDeviceCache(const BACnetDeviceInfo& device);
    bool GetCachedDevice(uint32_t device_id, BACnetDeviceInfo& device) const;
    void ClearDeviceCache();
    
    // 통계 헬퍼
    void UpdateConnectionStatistics(bool success);
    void UpdateOperationStatistics(const std::string& operation, bool success, 
                                  std::chrono::milliseconds duration);
};
};

} // namespace Drivers
} // namespace PulseOne

#endif // BACNET_DRIVER_H