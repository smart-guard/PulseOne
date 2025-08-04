// =============================================================================
// collector/include/Drivers/Modbus/ModbusDriver.h
// Modbus 드라이버 - Facade 패턴 (심플하고 확장 가능한 구조)
// =============================================================================

#ifndef PULSEONE_DRIVERS_MODBUS_DRIVER_H
#define PULSEONE_DRIVERS_MODBUS_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Common/DriverStatistics.h"
#include "Common/Structs.h"
#include <modbus/modbus.h>
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
}

namespace PulseOne {
namespace Drivers {

/**
 * @brief 표준화된 Modbus 드라이버 (Facade 패턴)
 * @details 핵심 통신 기능 + 선택적 고급 기능 활성화
 * 
 * 🎯 설계 목표:
 * - BACnetDriver와 동일한 심플함
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
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    bool ReadValues(const std::vector<Structs::DataPoint>& points,
                   std::vector<Structs::TimestampedValue>& values) override;
    bool WriteValue(const Structs::DataPoint& point,
                   const Structs::DataValue& value) override;
    
    Enums::ProtocolType GetProtocolType() const override;
    Structs::DriverStatus GetStatus() const override;
    Structs::ErrorInfo GetLastError() const override;
    
    // 표준화된 통계 인터페이스
    const DriverStatistics& GetStatistics() const override;
    void ResetStatistics() override;
    
    // =======================================================================
    // 기본 Modbus 통신 메서드 (Core 기능 - 항상 사용 가능)
    // =======================================================================
    
    // 연결 관리
    bool SetSlaveId(int slave_id);
    int GetSlaveId() const;
    bool TestConnection();
    
    // 레지스터 읽기/쓰기 (기본 Modbus 기능)
    bool ReadHoldingRegisters(int slave_id, uint16_t start_addr, uint16_t count, std::vector<uint16_t>& values);
    bool ReadInputRegisters(int slave_id, uint16_t start_addr, uint16_t count, std::vector<uint16_t>& values);
    bool ReadCoils(int slave_id, uint16_t start_addr, uint16_t count, std::vector<bool>& values);
    bool ReadDiscreteInputs(int slave_id, uint16_t start_addr, uint16_t count, std::vector<bool>& values);
    
    bool WriteHoldingRegister(int slave_id, uint16_t address, uint16_t value);
    bool WriteHoldingRegisters(int slave_id, uint16_t start_addr, const std::vector<uint16_t>& values);
    bool WriteCoil(int slave_id, uint16_t address, bool value);
    bool WriteCoils(int slave_id, uint16_t start_addr, const std::vector<bool>& values);
    
    // 대량 읽기 (최적화된 배치 읽기)
    bool ReadHoldingRegistersBulk(int slave_id, uint16_t start_addr, uint16_t count,
                                 std::vector<uint16_t>& values, int max_retries = 3);
    
    // =======================================================================
    // 🔧 진단 기능 (선택적 활성화) - EnableDiagnostics() 호출 시 활성화
    // =======================================================================
    
    bool EnableDiagnostics(bool packet_logging = true, bool console_output = false);
    void DisableDiagnostics();
    bool IsDiagnosticsEnabled() const;
    
    // 진단 API (활성화된 경우에만 동작, 비활성화시 기본값 반환)
    std::string GetDiagnosticsJSON() const;
    std::map<std::string, std::string> GetDiagnostics() const;
    std::vector<uint64_t> GetResponseTimeHistogram() const;
    std::map<uint8_t, uint64_t> GetExceptionCodeStats() const;
    double GetCrcErrorRate() const;
    std::map<int, SlaveHealthInfo> GetSlaveHealthStatus() const;
    
    // 패킷 로깅 (진단 기능의 일부)
    void TogglePacketLogging();
    void ToggleConsoleMonitoring();
    std::string GetRecentPacketsJSON(int count = 100) const;
    
    // =======================================================================
    // 🏊 연결 풀링 기능 (선택적 활성화) - EnableConnectionPooling() 호출 시 활성화
    // =======================================================================
    
    bool EnableConnectionPooling(size_t pool_size = 5, int timeout_seconds = 30);
    void DisableConnectionPooling();
    bool IsConnectionPoolingEnabled() const;
    
    // 자동 스케일링 (연결 풀링의 고급 기능)
    bool EnableAutoScaling(double load_threshold = 0.8, size_t max_connections = 20);
    void DisableAutoScaling();
    bool IsAutoScalingEnabled() const;
    ConnectionPoolStats GetConnectionPoolStats() const;
    
    // =======================================================================
    // 🔄 페일오버 기능 (선택적 활성화) - EnableFailover() 호출 시 활성화
    // =======================================================================
    
    bool EnableFailover(int failure_threshold = 3, int recovery_check_interval_seconds = 60);
    void DisableFailover();
    bool IsFailoverEnabled() const;
    
    // 백업 엔드포인트 관리
    bool AddBackupEndpoint(const std::string& endpoint);
    void RemoveBackupEndpoint(const std::string& endpoint);
    std::vector<std::string> GetActiveEndpoints() const;
    std::string GetCurrentEndpoint() const;
    
    // =======================================================================
    // ⚡ 성능 최적화 기능 (선택적 활성화) - EnablePerformanceMode() 호출 시 활성화
    // =======================================================================
    
    bool EnablePerformanceMode();
    void DisablePerformanceMode();
    bool IsPerformanceModeEnabled() const;
    
    // 성능 튜닝
    void SetReadBatchSize(size_t batch_size);
    void SetWriteBatchSize(size_t batch_size);
    size_t GetReadBatchSize() const;
    size_t GetWriteBatchSize() const;
    
    // 연결 품질 및 실시간 모니터링
    int TestConnectionQuality();
    bool StartRealtimeMonitoring(int interval_seconds = 5);
    void StopRealtimeMonitoring();
    bool IsRealtimeMonitoringEnabled() const;
    
    // 동적 설정 변경
    bool UpdateTimeout(int timeout_ms);
    bool UpdateRetryCount(int retry_count);
    bool UpdateSlaveResponseDelay(int delay_ms);

private:
    // =======================================================================
    // Core 멤버 변수 (항상 존재 - 기본 메모리 사용량 최소화)
    // =======================================================================
    
    // 표준 통계 및 에러 정보
    mutable DriverStatistics driver_statistics_{"MODBUS"};
    Structs::ErrorInfo last_error_;
    
    // Modbus 연결 관련
    modbus_t* modbus_ctx_;
    std::atomic<bool> is_connected_;
    mutable std::mutex connection_mutex_;
    mutable std::mutex operation_mutex_;
    int current_slave_id_;
    
    // 드라이버 설정 및 상태
    DriverConfig config_;
    std::chrono::steady_clock::time_point last_successful_operation_;
    
    // =======================================================================
    // 고급 기능 모듈 (선택적 생성 - nullptr이면 비활성화, 메모리 절약)
    // =======================================================================
    
    std::unique_ptr<ModbusDiagnostics> diagnostics_;        // 진단 기능
    std::unique_ptr<ModbusConnectionPool> connection_pool_; // 연결 풀링 & 스케일링
    std::unique_ptr<ModbusFailover> failover_;             // 페일오버 & 복구
    std::unique_ptr<ModbusPerformance> performance_;       // 성능 최적화
    
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