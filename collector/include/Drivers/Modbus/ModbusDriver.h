// =============================================================================
// collector/include/Drivers/Modbus/ModbusDriver.h (완전 표준화 - 모든 기능 유지)
// 🔥 ModbusStatistics만 DriverStatistics로 교체, 나머지 모든 기능 유지
// =============================================================================

#ifndef PULSEONE_DRIVERS_MODBUS_DRIVER_H
#define PULSEONE_DRIVERS_MODBUS_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Common/DriverStatistics.h"  
#include "Common/Structs.h"
#include "Common/DriverError.h"
#include "Utils/LogManager.h"
#include "Database/DatabaseManager.h"

#include <modbus/modbus.h>
#include <memory>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <deque>
#include <queue>
#include <thread>
#include <condition_variable>
#include <map>
#include <array>
#include <functional>

namespace PulseOne {
namespace Drivers {

// =============================================================================
// Modbus 특화 구조체들 (기존 모든 구조체 유지)
// =============================================================================

struct ModbusDataPointInfo {
    double scaling_factor;
    double scaling_offset;
    double min_value;
    double max_value;
    
    ModbusDataPointInfo();
};

struct ModbusPacketLog {
    std::chrono::system_clock::time_point timestamp;
    std::string direction;
    uint8_t slave_id;
    uint8_t function_code;
    uint16_t start_address;
    uint16_t data_count;
    std::vector<uint16_t> values;
    bool success;
    std::string error_message;
    double response_time_ms;
    
    ModbusPacketLog();
};

struct SlaveHealthInfo {
    uint8_t slave_id;
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_requests{0};
    std::atomic<uint64_t> failed_requests{0};
    std::atomic<double> avg_response_time_ms{0.0};
    std::chrono::system_clock::time_point last_successful_communication;
    std::atomic<bool> is_online{false};
    
    SlaveHealthInfo() = default;
    SlaveHealthInfo(uint8_t id) : slave_id(id) {}
};

struct RegisterAccessPattern {
    std::atomic<uint64_t> read_count{0};
    std::atomic<uint64_t> write_count{0};
    std::atomic<double> avg_response_time_ms{0.0};
    std::chrono::system_clock::time_point last_access;
    
    RegisterAccessPattern() = default;
};

// =============================================================================
// 🔥 스케일링 및 로드밸런싱 타입 정의들 (기존 유지)
// =============================================================================

/**
 * @brief Modbus 연결 객체
 */
struct ModbusConnection {
    // 1. 스마트 포인터 (첫 번째)
    std::unique_ptr<modbus_t, void(*)(modbus_t*)> ctx;
    
    // 2. atomic 멤버들
    std::atomic<bool> is_connected{false};
    std::atomic<bool> is_busy{false};
    std::atomic<uint64_t> total_operations{0};
    std::atomic<uint64_t> successful_operations{0};
    std::atomic<double> avg_response_time_ms{0.0};
    
    // 3. 시간 관련 멤버들
    std::chrono::system_clock::time_point last_used;
    std::chrono::system_clock::time_point created_at;
    
    // 4. 문자열 멤버
    std::string endpoint;
    
    // 5. 정수 멤버
    int connection_id;
    
    // 6. 생성자
    ModbusConnection() : ctx(nullptr, modbus_free), connection_id(-1) {
        created_at = std::chrono::system_clock::now();
        last_used = created_at;
    }
    
    ~ModbusConnection() = default;
    
    // 7. 이동 생성자/할당 연산자
    ModbusConnection(ModbusConnection&& other) noexcept 
        : ctx(std::move(other.ctx))
        , is_connected(other.is_connected.load())
        , is_busy(other.is_busy.load())
        , total_operations(other.total_operations.load())
        , successful_operations(other.successful_operations.load())
        , avg_response_time_ms(other.avg_response_time_ms.load())
        , last_used(other.last_used)
        , created_at(other.created_at)
        , endpoint(std::move(other.endpoint))
        , connection_id(other.connection_id) {}
        
    ModbusConnection& operator=(ModbusConnection&& other) noexcept {
        if (this != &other) {
            ctx = std::move(other.ctx);
            is_connected = other.is_connected.load();
            is_busy = other.is_busy.load();
            total_operations = other.total_operations.load();
            successful_operations = other.successful_operations.load();
            avg_response_time_ms = other.avg_response_time_ms.load();
            last_used = other.last_used;
            created_at = other.created_at;
            endpoint = std::move(other.endpoint);
            connection_id = other.connection_id;
        }
        return *this;
    }
    
    // 복사 방지
    ModbusConnection(const ModbusConnection&) = delete;
    ModbusConnection& operator=(const ModbusConnection&) = delete;
};

/**
 * @brief 연결 풀 통계
 */
struct ConnectionPoolStats {
    std::atomic<size_t> total_connections{0};
    std::atomic<size_t> active_connections{0};
    std::atomic<size_t> idle_connections{0};
    std::atomic<uint64_t> pool_hits{0};
    std::atomic<uint64_t> pool_misses{0};
    std::atomic<uint64_t> connection_timeouts{0};
    std::atomic<double> avg_wait_time_ms{0.0};
    
    void Reset() {
        total_connections = 0;
        active_connections = 0;
        idle_connections = 0;
        pool_hits = 0;
        pool_misses = 0;
        connection_timeouts = 0;
        avg_wait_time_ms = 0.0;
    }
};

// =============================================================================
// ModbusDriver 클래스 (모든 기능 유지 + 표준화)
// =============================================================================

/**
 * @brief PulseOne Modbus TCP/RTU 드라이버 - 완전한 기능
 * @details 포함된 모든 기능:
 * - 기본 Modbus 통신 (TCP/RTU)
 * - 에러 핸들링 및 재연결
 * - 진단 기능 (패킷 로깅, 통계)
 * - 멀티스레드 안전성
 * - 고급 스케일링 및 로드밸런싱
 * - 자동 페일오버
 * - 성능 모니터링 및 최적화
 */
class ModbusDriver : public IProtocolDriver {
public:
    ModbusDriver();
    virtual ~ModbusDriver();
    
    // 복사/이동 방지
    ModbusDriver(const ModbusDriver&) = delete;
    ModbusDriver& operator=(const ModbusDriver&) = delete;
    
    // ==========================================================================
    // ✅ IProtocolDriver 인터페이스 구현 (표준화된 통계 사용)
    // ==========================================================================
    
    bool Initialize(const DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    bool ReadValues(
        const std::vector<Structs::DataPoint>& points,
        std::vector<TimestampedValue>& values
    ) override;
    
    bool WriteValue(
        const Structs::DataPoint& point,
        const Structs::DataValue& value
    ) override;
    
    Enums::ProtocolType GetProtocolType() const override;
    Structs::DriverStatus GetStatus() const override;
    Structs::ErrorInfo GetLastError() const override;
    
    // ✅ 표준화된 통계 인터페이스 (ModbusStatistics → DriverStatistics)
    const DriverStatistics& GetStatistics() const override;
    void ResetStatistics() override;
    
    // ==========================================================================
    // 기본 Modbus 통신 메서드들 (기존 모든 기능 유지)
    // ==========================================================================
    
    // 연결 관리
    bool SetSlaveId(int slave_id);
    int GetSlaveId() const;
    bool TestConnection();
    
    // 레지스터 읽기/쓰기
    bool ReadHoldingRegisters(int slave_id, uint16_t start_addr, uint16_t count, std::vector<uint16_t>& values);
    bool ReadInputRegisters(int slave_id, uint16_t start_addr, uint16_t count, std::vector<uint16_t>& values);
    bool ReadCoils(int slave_id, uint16_t start_addr, uint16_t count, std::vector<bool>& values);
    bool ReadDiscreteInputs(int slave_id, uint16_t start_addr, uint16_t count, std::vector<bool>& values);
    
    bool WriteHoldingRegister(int slave_id, uint16_t address, uint16_t value);
    bool WriteHoldingRegisters(int slave_id, uint16_t start_addr, const std::vector<uint16_t>& values);
    bool WriteCoil(int slave_id, uint16_t address, bool value);
    bool WriteCoils(int slave_id, uint16_t start_addr, const std::vector<bool>& values);
    
    // 대량 읽기 (최적화된)
    bool ReadHoldingRegistersBulk(int slave_id, uint16_t start_addr, uint16_t count,
                                std::vector<uint16_t>& values, int max_retries = 3);
    
    // ==========================================================================
    // 진단 및 모니터링 (기존 모든 기능 유지)
    // ==========================================================================
    
    bool EnableDiagnostics(DatabaseManager& db_manager,
                          bool enable_packet_logging = true,
                          bool enable_console_output = false);
    void DisableDiagnostics();
    void ToggleConsoleMonitoring();
    void TogglePacketLogging();
    std::string GetDiagnosticsJSON() const;
    std::string GetRecentPacketsJSON(int count = 100) const;
    
    // 진단 데이터 조회
    std::map<uint8_t, uint64_t> GetExceptionCodeStats() const;
    double GetCrcErrorRate() const;
    std::map<int, SlaveHealthInfo> GetSlaveHealthStatus() const;
    std::vector<uint64_t> GetResponseTimeHistogram() const;
    std::string GetRegisterAccessReport() const;
    std::string GetModbusHealthReport() const;
    
    // ==========================================================================
    // 🔥 고급 기능들 (기존 모든 기능 유지)
    // ==========================================================================
    
    // 연결 풀 관리
    bool EnableConnectionPooling(size_t pool_size = 5, int timeout_seconds = 30);
    void DisableConnectionPooling();
    bool IsConnectionPoolingEnabled() const;
    ConnectionPoolStats GetConnectionPoolStats() const;
    
    // 스케일링 및 로드밸런싱
    bool EnableAutoScaling(double load_threshold = 0.8, size_t max_connections = 20);
    void DisableAutoScaling();
    bool IsAutoScalingEnabled() const;
    
    // 페일오버 및 복구
    bool AddBackupEndpoint(const std::string& endpoint);
    void RemoveBackupEndpoint(const std::string& endpoint);
    bool EnableAutoFailover(int failure_threshold = 3, int recovery_check_interval_seconds = 60);
    void DisableAutoFailover();
    std::vector<std::string> GetActiveEndpoints() const;
    std::string GetCurrentEndpoint() const;
    
    // 성능 최적화
    void SetReadBatchSize(size_t batch_size);
    void SetWriteBatchSize(size_t batch_size);
    size_t GetReadBatchSize() const;
    size_t GetWriteBatchSize() const;
    
    // 동적 설정 변경
    bool UpdateTimeout(int timeout_ms);
    bool UpdateRetryCount(int retry_count);
    bool UpdateSlaveResponseDelay(int delay_ms);
    
    // 연결 품질 테스트
    int TestConnectionQuality();
    bool PerformLatencyTest(std::vector<double>& latencies, int test_count = 10);
    
    // 실시간 모니터링
    void StartRealtimeMonitoring(int interval_seconds = 5);
    void StopRealtimeMonitoring();
    bool IsRealtimeMonitoringEnabled() const;
    
    // 콜백 설정
    using ErrorCallback = std::function<void(int error_code, const std::string& message)>;
    using ConnectionStatusCallback = std::function<void(bool connected, const std::string& endpoint)>;
    using DataReceivedCallback = std::function<void(const std::vector<TimestampedValue>& values)>;
    
    void SetErrorCallback(ErrorCallback callback);
    void SetConnectionStatusCallback(ConnectionStatusCallback callback);
    void SetDataReceivedCallback(DataReceivedCallback callback);
    
    // ==========================================================================
    // 내부 진단 및 도구들 (기존 유지)
    // ==========================================================================
    
    // 새로운 에러 API
    std::string GetDetailedErrorInfo() const;
    DriverErrorCode GetDriverErrorCode() const;
    
    // 연결 풀 작업 메서드들
    bool PerformReadWithConnectionPool(const std::vector<Structs::DataPoint>& points,
                                     std::vector<TimestampedValue>& values);
    bool PerformWriteWithConnectionPool(const Structs::DataPoint& point, 
                                      const Structs::DataValue& value);

private:
    // ==========================================================================
    // ✅ 표준화된 멤버 변수들 (ModbusStatistics → DriverStatistics)
    // ==========================================================================
    
    // ✅ 표준 통계 (유일한 변경사항)
    mutable DriverStatistics driver_statistics_{"MODBUS"};
    
    // ✅ 표준 에러 정보
    Structs::ErrorInfo last_error_;
    
    // ==========================================================================
    // 기존 모든 멤버 변수들 (그대로 유지)
    // ==========================================================================
    
    // Modbus 연결 관련 (기존)
    modbus_t* modbus_ctx_;
    std::atomic<bool> is_connected_;
    std::mutex connection_mutex_;
    std::mutex operation_mutex_;
    int current_slave_id_;
    
    // 드라이버 설정 및 상태 (기존)
    DriverConfig config_;
    std::chrono::steady_clock::time_point last_successful_operation_;
    
    // 진단 기능 관련 (기존)
    std::atomic<bool> diagnostics_enabled_{false};
    std::atomic<bool> packet_logging_enabled_{false};
    std::atomic<bool> console_monitoring_enabled_{false};
    DatabaseManager* db_manager_{nullptr};
    
    // 패킷 로깅 (기존)
    static constexpr size_t MAX_PACKET_HISTORY = 1000;
    std::deque<ModbusPacketLog> packet_history_;
    mutable std::mutex packet_history_mutex_;
    
    // CRC 에러 추적 (기존)
    std::atomic<uint64_t> total_crc_checks_{0};
    std::atomic<uint64_t> crc_errors_{0};
    
    // 응답 시간 히스토그램 (기존)
    static constexpr size_t HISTOGRAM_BUCKETS = 10;
    std::array<std::atomic<uint64_t>, HISTOGRAM_BUCKETS> response_time_buckets_;
    
    // 예외 코드 통계 (기존)
    std::unordered_map<uint8_t, std::atomic<uint64_t>> exception_code_stats_;
    mutable std::mutex exception_stats_mutex_;
    
    // 슬레이브 상태 추적 (기존)
    std::unordered_map<int, SlaveHealthInfo> slave_health_map_;
    mutable std::mutex diagnostics_mutex_;
    
    // 레지스터 접근 패턴 (기존)
    std::unordered_map<uint16_t, RegisterAccessPattern> register_access_patterns_;
    
    // 스케일링 히스토리 (기존)
    static constexpr size_t MAX_SCALING_HISTORY = 100;
    std::deque<std::pair<std::chrono::system_clock::time_point, double>> scaling_history_;
    mutable std::mutex scaling_mutex_;
    
    // ==========================================================================
    // 🔥 고급 기능 멤버 변수들 (기존 모든 기능 유지)
    // ==========================================================================
    
    // 연결 풀링 관련
    std::atomic<bool> connection_pooling_enabled_{false};
    std::vector<std::unique_ptr<ModbusConnection>> connection_pool_;
    std::queue<size_t> available_connections_;
    mutable std::mutex pool_mutex_;
    std::condition_variable pool_condition_;
    ConnectionPoolStats pool_stats_;
    
    // 스케일링 관련
    std::atomic<bool> scaling_enabled_{false};
    std::atomic<double> load_threshold_{0.8};
    std::atomic<size_t> max_connections_{20};
    std::thread scaling_monitor_thread_;
    std::atomic<bool> scaling_monitor_running_{false};
    
    // 페일오버 관련
    std::vector<std::string> backup_endpoints_;
    std::atomic<bool> failover_enabled_{false};
    std::atomic<int> failure_threshold_{3};
    std::atomic<int> current_failures_{0};
    std::atomic<size_t> current_endpoint_index_{0};
    std::thread failover_monitor_thread_;
    std::atomic<bool> failover_monitor_running_{false};
    
    // 성능 최적화 관련
    std::atomic<size_t> read_batch_size_{10};
    std::atomic<size_t> write_batch_size_{5};
    
    // 실시간 모니터링 관련
    std::atomic<bool> realtime_monitoring_enabled_{false};
    std::thread realtime_monitor_thread_;
    std::atomic<bool> realtime_monitor_running_{false};
    std::atomic<int> monitoring_interval_seconds_{5};
    
    // 콜백 함수들
    ErrorCallback error_callback_;
    ConnectionStatusCallback connection_status_callback_;
    DataReceivedCallback data_received_callback_;
    mutable std::mutex callback_mutex_;
    
    // 동적 설정 관련
    std::atomic<int> dynamic_timeout_ms_{5000};
    std::atomic<int> dynamic_retry_count_{3};
    std::atomic<int> slave_response_delay_ms_{0};
    
    // 연결 품질 테스트 관련
    mutable std::mutex quality_test_mutex_;
    std::vector<double> quality_test_results_;
    
    // ==========================================================================
    // ✅ 표준화된 내부 메서드들 (통계 관련만 변경)
    // ==========================================================================
    
    // 통계 초기화 (새로운 표준 방식)
    void InitializeModbusStatistics();
    
    // 통계 업데이트 (표준화 + 기존 기능 유지)
    void UpdateStats(bool success, double response_time_ms, const std::string& operation = "read");
    
    // 에러 설정 (표준화)
    void SetError(Structs::ErrorCode code, const std::string& message);
    
    // ==========================================================================
    // 기존 모든 내부 메서드들 (그대로 유지)
    // ==========================================================================
    
    // 데이터 변환 (기존)
    Structs::DataValue ConvertModbusValue(const Structs::DataPoint& point, uint16_t raw_value) const;
    bool ConvertToModbusValue(const Structs::DataValue& value, const Structs::DataPoint& point, uint16_t& modbus_value) const;
    
    // 연결 헬퍼 (기존)
    bool EnsureConnection();
    bool ReconnectWithRetry(int max_retries = 3);
    bool SetupModbusConnection();
    void CleanupConnection();
    
    // 진단 헬퍼 (기존)
    void UpdateSlaveHealth(int slave_id, bool success, double response_time_ms);
    void UpdateRegisterAccessPattern(uint16_t address, bool is_read, bool is_write);
    void UpdateResponseTimeHistogram(double response_time_ms);
    void LogModbusPacket(const std::string& direction, int slave_id, uint8_t function_code,
                        uint16_t start_addr, uint16_t count, const std::vector<uint16_t>& values,
                        bool success, const std::string& error_msg, double response_time_ms);
    
    // 유틸리티 (기존)
    std::string BytesToHex(const uint8_t* packet, size_t length) const;
    std::string GetFunctionName(uint8_t function_code) const;
    std::string FormatPacketForConsole(const ModbusPacketLog& log) const;
    void TrimPacketHistory();
    
    // 데이터베이스 관련 (기존)
    bool LoadDataPointsFromDB();
    std::string QueryDeviceName(const std::string& device_id);
    bool QueryDataPoints(const std::string& device_id);
    
    // ==========================================================================
    // 🔥 고급 기능 내부 메서드들 (기존 모든 기능 유지)
    // ==========================================================================
    
    // 연결 풀 관리
    ModbusConnection* AcquireConnection(int timeout_ms = 5000);
    void ReleaseConnection(ModbusConnection* connection);
    bool CreateConnection(size_t connection_id);
    void CleanupConnectionPool();
    void UpdatePoolStatistics();
    
    // 스케일링
    void ScalingMonitorLoop();
    void CheckAndScale();
    double CalculateCurrentLoad() const;
    bool ScaleUp();
    bool ScaleDown();
    
    // 페일오버
    void FailoverMonitorLoop();
    bool SwitchToBackupEndpoint();
    bool TestEndpointConnectivity(const std::string& endpoint) const;
    void UpdateFailureCount(bool success);
    
    // 실시간 모니터링
    void RealtimeMonitorLoop();
    void CollectRealtimeMetrics();
    void PublishMetrics() const;
    
    // 연결 작업 (새로운)
    bool PerformReadWithConnection(ModbusConnection* conn,
                                 const std::vector<Structs::DataPoint>& points,
                                 std::vector<TimestampedValue>& values);
    bool PerformWriteWithConnection(ModbusConnection* conn,
                                  const Structs::DataPoint& point,
                                  const Structs::DataValue& value);
    
    // 단일 연결 작업 (기존 로직 분리)
    bool PerformReadWithSingleConnection(const std::vector<Structs::DataPoint>& points,
                                       std::vector<TimestampedValue>& values);
    bool PerformWriteWithSingleConnection(const Structs::DataPoint& point,
                                        const Structs::DataValue& value);
    
    // 에러 처리 (하이브리드 방식)
    void HandleModbusError(int errno_code, const std::string& context);
    
    // 콜백 호출
    void TriggerErrorCallback(int error_code, const std::string& message);
    void TriggerConnectionStatusCallback(bool connected, const std::string& endpoint);
    void TriggerDataReceivedCallback(const std::vector<TimestampedValue>& values);
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_MODBUS_DRIVER_H