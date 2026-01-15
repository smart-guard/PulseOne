// =============================================================================
// collector/include/Drivers/Modbus/ModbusDriver.h
// Modbus 드라이버 - Facade 패턴 (심플하고 확장 가능한 구조)
// =============================================================================

#ifndef PULSEONE_DRIVERS_MODBUS_DRIVER_H
#define PULSEONE_DRIVERS_MODBUS_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Common/DriverStatistics.h"
#include "Common/Structs.h"
#include "Logging/LogManager.h"

// Modbus 라이브러리 조건부 포함
#ifdef HAVE_MODBUS
    #include <modbus/modbus.h>
#else
    // 라이브러리 부재 시 전방 선언
    struct _modbus;
    typedef struct _modbus modbus_t;
#endif

#include <memory>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <map>

// 전방 선언 (헤더 의존성 최소화)
namespace PulseOne::Drivers {
    class ModbusDiagnostics;
    class ModbusConnectionPool;
    class ModbusFailover;
    class ModbusPerformance;
    
    struct ConnectionPoolStats;
    struct SlaveHealthInfo;
    struct RegisterAccessPattern;
    struct ModbusPacketLog;
    
    // 타입 별칭 (IProtocolDriver 호환성)
    using ProtocolType = PulseOne::Enums::ProtocolType;
    using ErrorInfo = PulseOne::Structs::ErrorInfo;
}

namespace PulseOne {
namespace Drivers {

/**
 * @brief 표준화된 Modbus 드라이버 (Facade 패턴)
 * @details 핵심 통신 기능 + 선택적 고급 기능 활성화
 * 
 * 🎯 설계 목표:
 * - BACnetDriver와 동일한 심플함 (300줄 목표)
 * - Worker와 100% 호환성 유지
 * - 선택적 고급 기능 활성화
 * - 메모리 효율성 (사용하지 않는 기능은 로드하지 않음)
 * 
 * 사용 예시:
 * auto driver = std::make_shared<ModbusDriver>();
 * driver->Initialize(config);
 * driver->Connect();
 * 
 * // 선택적 고급 기능 활성화
 * driver->EnableDiagnostics();        // 진단 기능
 * driver->EnableConnectionPooling();  // 연결 풀링 & 스케일링
 * driver->EnableFailover();          // 페일오버 & 복구
 * driver->EnablePerformanceMode();   // 성능 최적화
 */
class ModbusDriver : public IProtocolDriver {
public:
    ModbusDriver();
    virtual ~ModbusDriver();
    
    // 복사/이동 방지 (리소스 관리를 위해)
    ModbusDriver(const ModbusDriver&) = delete;
    ModbusDriver& operator=(const ModbusDriver&) = delete;
    // =======================================================================
    // IProtocolDriver 인터페이스 구현 (Core 기능 - 항상 사용 가능)
    // =======================================================================
    
    bool Initialize(const DriverConfig& config) override;
    bool Start() override;                    
    bool Stop() override;                     
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    bool ReadValues(const std::vector<Structs::DataPoint>& points, 
                    std::vector<Structs::TimestampedValue>& values) override;
    bool WriteValue(const Structs::DataPoint& point, 
                    const Structs::DataValue& value) override;
    
    // 표준 통계 인터페이스 (DriverStatistics 사용)
    const DriverStatistics& GetStatistics() const override;
    void ResetStatistics() override;
    
    // IProtocolDriver 필수 구현 메서드들
    ProtocolType GetProtocolType() const override;
    Enums::DriverStatus GetStatus() const override;
    ErrorInfo GetLastError() const override;
    
    // =======================================================================
    /**
     * @brief MQTT 메시지 발행 (여기서는 Modbus용으로 사용되지 않지만 인터페이스 충족)
     */
    
    Structs::DataValue ExtractValueFromBuffer(const std::vector<uint16_t>& buffer, size_t offset, const Structs::DataPoint& point);
    
    // =======================================================================
    
    bool ReadHoldingRegisters(int slave_id, uint16_t start_addr, uint16_t count, 
                              std::vector<uint16_t>& values);
    bool ReadInputRegisters(int slave_id, uint16_t start_addr, uint16_t count, 
                            std::vector<uint16_t>& values);
    bool ReadCoils(int slave_id, uint16_t start_addr, uint16_t count, 
                   std::vector<uint8_t>& values);
    bool ReadDiscreteInputs(int slave_id, uint16_t start_addr, uint16_t count, 
                            std::vector<uint8_t>& values);
    
    bool WriteHoldingRegister(int slave_id, uint16_t address, uint16_t value);
    bool WriteHoldingRegisters(int slave_id, uint16_t start_addr, 
                               const std::vector<uint16_t>& values);
    bool WriteCoil(int slave_id, uint16_t address, bool value);
    bool WriteCoils(int slave_id, uint16_t start_addr, 
                    const std::vector<uint8_t>& values);
    
    // Mask Write (for bit-level manipulation in registers)
    bool MaskWriteRegister(int slave_id, uint16_t address, uint16_t and_mask, uint16_t or_mask);
    
    // Slave ID 관리
    void SetSlaveId(int slave_id);
    int GetSlaveId() const;
    
    // =======================================================================
    // 🔧 고급 기능 - 선택적 활성화 API (Facade 패턴)
    // =======================================================================
    
    // 진단 기능 (옵션) - EnableDiagnostics() 호출 시 활성화
    bool EnableDiagnostics(bool packet_logging = true, bool console_output = false);
    void DisableDiagnostics();
    bool IsDiagnosticsEnabled() const;
    
    // 진단 API (활성화된 경우에만 동작)
    std::string GetDiagnosticsJSON() const;
    std::map<std::string, std::string> GetDiagnostics() const;
    std::vector<uint64_t> GetResponseTimeHistogram() const;
    double GetCrcErrorRate() const;
    
    // 연결 풀링 기능 (옵션) - EnableConnectionPooling() 호출 시 활성화
    bool EnableConnectionPooling(size_t pool_size = 5, int timeout_seconds = 30);
    void DisableConnectionPooling();
    bool IsConnectionPoolingEnabled() const;
    
    // 자동 스케일링 (연결 풀링의 하위 기능)
    bool EnableAutoScaling(double load_threshold = 0.8, size_t max_connections = 20);
    void DisableAutoScaling();
    ConnectionPoolStats GetConnectionPoolStats() const;
    
    // 페일오버 기능 (옵션) - EnableFailover() 호출 시 활성화
    bool EnableFailover(int failure_threshold = 3, int recovery_check_interval = 60);
    void DisableFailover();
    bool IsFailoverEnabled() const;
    
    // 백업 엔드포인트 관리
    bool AddBackupEndpoint(const std::string& endpoint);
    void RemoveBackupEndpoint(const std::string& endpoint);
    std::vector<std::string> GetActiveEndpoints() const;
    
    // 성능 최적화 기능 (옵션) - EnablePerformanceMode() 호출 시 활성화
    bool EnablePerformanceMode();
    void DisablePerformanceMode();
    bool IsPerformanceModeEnabled() const;
    
    // 성능 튜닝
    void SetReadBatchSize(size_t batch_size);
    void SetWriteBatchSize(size_t batch_size);
    int TestConnectionQuality();
    bool StartRealtimeMonitoring(int interval_seconds = 5);
    void StopRealtimeMonitoring();

    const DriverConfig& GetConfiguration() const override {
        return config_;  // 실제 config 반환
    }

    // 1:N 시리얼 포트 공유를 위한 뮤텍스 관리 (static registry)
    static std::mutex& GetSerialMutex(const std::string& endpoint);

private:
    // =======================================================================
    // Core 멤버 변수 (항상 존재)
    // =======================================================================
    
    modbus_t* modbus_ctx_;
    DriverStatistics driver_statistics_{"MODBUS"};  // 먼저 선언
    Structs::ErrorInfo last_error_;
    std::atomic<bool> is_connected_;                 // 나중 선언 (순서 맞춤)
    std::mutex connection_mutex_;
    int current_slave_id_;
 
    std::atomic<bool> is_started_{false};
    std::recursive_mutex driver_mutex_;
    LogManager* logger_;
    PulseOne::Enums::ConnectionStatus status_ = PulseOne::Enums::ConnectionStatus::DISCONNECTED;    
    
    // =======================================================================
    // 고급 기능 멤버 (선택적 생성 - std::unique_ptr 사용)
    // =======================================================================
    
    std::unique_ptr<ModbusDiagnostics> diagnostics_;        // nullptr이면 비활성화
    std::unique_ptr<ModbusConnectionPool> connection_pool_; // nullptr이면 비활성화  
    std::unique_ptr<ModbusFailover> failover_;             // nullptr이면 비활성화
    std::unique_ptr<ModbusPerformance> performance_;       // nullptr이면 비활성화
    
    // =======================================================================
    // Core 내부 메서드 (항상 사용 가능)
    // =======================================================================
    
    // 통계 업데이트 (표준화된 방식)
    void UpdateStats(bool success, double response_time_ms, const std::string& operation = "read");
    
    // 에러 처리 (표준화된 방식)
    void SetError(Structs::ErrorCode code, const std::string& message);
    
    // 연결 관리
    bool EnsureConnection();
    bool ReconnectWithRetry(int max_retries = 3);
    bool SetupModbusConnection();
    void CleanupConnection();
    
    // 데이터 변환
    Structs::DataValue ConvertModbusValue(const Structs::DataPoint& point, uint16_t raw_value) const;
    bool ConvertToModbusValue(const Structs::DataValue& value, const Structs::DataPoint& point, uint16_t& modbus_value) const;
    
    // =======================================================================
    // 고급 기능 델리게이트 메서드 (해당 모듈이 활성화된 경우에만 호출)
    // =======================================================================
    
    // 진단 기능 내부 호출
    void RecordExceptionCode(uint8_t exception_code);
    void RecordCrcCheck(bool crc_valid);
    void RecordResponseTime(int slave_id, uint32_t response_time_ms);
    void RecordRegisterAccess(uint16_t address, bool is_read, bool is_write);
    void RecordSlaveRequest(int slave_id, bool success, uint32_t response_time_ms);
    
    // 연결 풀 내부 호출 (연결 풀이 활성화된 경우)
    bool PerformReadWithConnectionPool(const std::vector<Structs::DataPoint>& points,
                                      std::vector<Structs::TimestampedValue>& values);
    bool PerformWriteWithConnectionPool(const Structs::DataPoint& point,
                                       const Structs::DataValue& value);
    
    // 페일오버 내부 호출 (페일오버가 활성화된 경우)
    bool SwitchToBackupEndpoint();
    bool CheckPrimaryEndpointRecovery();
    
    // 친구 클래스들 (고급 기능 모듈들이 Core 기능에 접근할 수 있도록)
    friend class ModbusDiagnostics;
    friend class ModbusConnectionPool;
    friend class ModbusFailover;
    friend class ModbusPerformance;
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_MODBUS_DRIVER_H