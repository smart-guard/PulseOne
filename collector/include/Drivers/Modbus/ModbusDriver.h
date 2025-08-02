// =============================================================================
// collector/include/Drivers/Modbus/ModbusDriver.h
// Modbus 프로토콜 드라이버 헤더 - 완성본 (스케일링 + 로드밸런싱 포함)
// =============================================================================

#ifndef PULSEONE_DRIVERS_MODBUS_DRIVER_H
#define PULSEONE_DRIVERS_MODBUS_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Drivers/Common/DriverLogger.h"
#include "Drivers/Modbus/ModbusConfig.h"
#include "Common/DriverStatistics.h"
#include "Common/DriverError.h"
#include "Utils/LogManager.h"
#include "Database/DatabaseManager.h"

// ✅ 조건부 modbus include
#include <modbus/modbus.h>

#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>
#include <chrono>
#include <vector>
#include <map>
#include <string>
#include <condition_variable>
#include <array>

namespace PulseOne {
namespace Drivers {
    using ErrorCode = PulseOne::Structs::ErrorCode;
    using ErrorInfo = PulseOne::Structs::ErrorInfo;
    using DriverErrorCode = PulseOne::Structs::DriverErrorCode;
// =============================================================================
// Modbus 특화 타입 정의들
// =============================================================================

/**
 * @brief Modbus 데이터 포인트 정보
 */
struct ModbusDataPointInfo {
    std::string name;                    // 포인트 이름
    std::string description;             // 포인트 설명
    std::string unit;                    // 단위 (°C, bar, L/min 등)
    double scaling_factor;               // 스케일링 계수
    double scaling_offset;               // 스케일링 오프셋
    std::string data_type;               // 데이터 타입 (bool, int16, float 등)
    double min_value;                    // 최소값
    double max_value;                    // 최대값
    
    ModbusDataPointInfo();
};

/**
 * @brief Modbus 패킷 로그
 */
struct ModbusPacketLog {
    std::chrono::system_clock::time_point timestamp;
    std::string direction;               // "TX" or "RX"
    int slave_id;
    uint8_t function_code;
    uint16_t start_address;
    uint16_t data_count;
    std::vector<uint16_t> values;
    bool success;
    std::string error_message;
    double response_time_ms;
    std::vector<uint8_t> raw_packet;
    std::string decoded_values;
    
    ModbusPacketLog();
};

/**
 * @brief Modbus 프로토콜 드라이버 - 완전체
 *
 * libmodbus 라이브러리를 사용하여 Modbus TCP/RTU 통신을 구현합니다.
 * 🚀 포함된 모든 기능:
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
    // ==========================================================================
    // 🔥 스케일링 및 로드밸런싱 타입 정의들
    // ==========================================================================
    
    /**
     * @brief Modbus 연결 객체
     */
    struct ModbusConnection {
    // 1. 스마트 포인터 (첫 번째)
    std::unique_ptr<modbus_t, void(*)(modbus_t*)> ctx;
    
    // 2. atomic 멤버들 (소스 파일 순서와 일치)
    std::atomic<bool> is_connected{false};
    std::atomic<bool> is_busy{false};
    std::atomic<uint64_t> total_operations{0};
    std::atomic<uint64_t> successful_operations{0};
    std::atomic<double> avg_response_time_ms{0.0};
    
    // 3. 시간 관련 멤버들 (소스 파일 순서와 일치)
    std::chrono::system_clock::time_point last_used;
    std::chrono::system_clock::time_point created_at;
    
    // 4. 문자열 멤버
    std::string endpoint;
    
    // 5. 정수 멤버
    int connection_id;
    
    // 6. 누락된 멤버 추가 (로드밸런싱용)
    double weight{1.0};
    
    // 생성자
    ModbusConnection(int id);
    
    // 메서드들
    double GetSuccessRate() const;
    bool IsHealthy() const;
    std::chrono::milliseconds GetIdleTime() const;
    void UpdateStats(bool success, double response_time_ms);
};
    
    /**
     * @brief 로드 밸런싱 전략
     */
    enum class LoadBalancingStrategy {
        SINGLE_CONNECTION,    // 기존 방식 (기본값)
        ROUND_ROBIN,         // 순환 배치
        LEAST_CONNECTIONS,   // 최소 연결
        HEALTH_BASED,        // 성능 기반
        ADAPTIVE,            // 적응형 (자동 선택)
        WEIGHTED_ROUND_ROBIN // 가중 순환 배치
    };
    
    /**
     * @brief 스케일링 설정
     */
    struct ScalingConfig {
        bool enabled = false;                               // 스케일링 활성화 여부
        LoadBalancingStrategy strategy = LoadBalancingStrategy::SINGLE_CONNECTION;
        
        // 연결 풀 설정
        size_t min_connections = 1;                         // 최소 연결 수
        size_t max_connections = 10;                        // 최대 연결 수
        size_t initial_connections = 2;                     // 초기 연결 수
        
        // 스케일링 임계값
        size_t target_operations_per_connection = 100;      // 연결당 목표 작업 수
        double max_response_time_ms = 500.0;                // 최대 허용 응답시간
        double min_success_rate = 95.0;                     // 최소 성공률
        double scale_up_threshold = 80.0;                   // 스케일 업 임계값 (%)
        double scale_down_threshold = 30.0;                 // 스케일 다운 임계값 (%)
        
        // 타이밍 설정
        std::chrono::seconds scale_check_interval{5};       // 스케일 체크 주기
        std::chrono::seconds connection_timeout{10};        // 연결 타임아웃
        std::chrono::seconds idle_timeout{300};             // 유휴 연결 타임아웃
        std::chrono::seconds connection_lifetime{3600};     // 연결 최대 수명
        
        // 재시도 설정
        int max_retries = 3;                                // 최대 재시도 횟수
        std::chrono::milliseconds base_retry_delay{100};    // 기본 재시도 지연
        double retry_backoff_multiplier = 2.0;              // 백오프 배수
        
        // 건강성 체크
        bool enable_health_check = true;                    // 건강성 체크 활성화
        std::chrono::seconds health_check_interval{30};     // 건강성 체크 주기
        int max_consecutive_failures = 3;                   // 최대 연속 실패 횟수
    };
    
    /**
     * @brief 연결 풀 상태 정보
     */
    struct PoolStatus {
        size_t total_connections;
        size_t active_connections;
        size_t available_connections;
        size_t healthy_connections;
        double avg_response_time_ms;
        double success_rate;
        uint64_t total_operations;
        LoadBalancingStrategy current_strategy;
        
        // 연결별 상세 정보
        struct ConnectionInfo {
            int id;
            bool connected;
            bool busy;
            bool healthy;
            uint64_t operations;
            double avg_response_ms;
            std::chrono::milliseconds idle_time;
            std::chrono::milliseconds lifetime;
        };
        std::vector<ConnectionInfo> connections;
    };
    
    /**
     * @brief 스케일링 이벤트 정보
     */
    struct ScalingEvent {
        enum Type { SCALE_UP, SCALE_DOWN, REPLACE_UNHEALTHY, HEALTH_CHECK };
        
        Type type;
        std::chrono::system_clock::time_point timestamp;
        std::string reason;
        int connections_before;
        int connections_after;
        double trigger_metric;
    };

    // ==========================================================================
    // 기존 진단 타입들
    // ==========================================================================
    
    /**
     * @brief 슬레이브 건강상태 정보 (복사 가능한 버전)
     */
    struct SlaveHealthInfo {
        uint64_t successful_requests = 0;
        uint64_t failed_requests = 0;
        uint32_t avg_response_time_ms = 0;
        std::chrono::system_clock::time_point last_response_time;
        bool is_online = false;
        
        SlaveHealthInfo();
        SlaveHealthInfo(const SlaveHealthInfo& other);
        SlaveHealthInfo& operator=(const SlaveHealthInfo& other);
        double GetSuccessRate() const;
    };

    // ==========================================================================
    // 생성자 및 소멸자
    // ==========================================================================
    
    ModbusDriver();
    virtual ~ModbusDriver();

    // ==========================================================================
    // IProtocolDriver 인터페이스 구현
    // ==========================================================================
    
    bool Initialize(const DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    bool ReadValues(const std::vector<Structs::DataPoint>& points,
                   std::vector<TimestampedValue>& values) override;
    bool WriteValue(const Structs::DataPoint& point, 
                   const Structs::DataValue& value) override;
    
    ProtocolType GetProtocolType() const override;
    Structs::DriverStatus GetStatus() const override;
    ErrorInfo GetLastError() const override;
    const DriverStatistics& GetStatistics() const override;
    
    // 🔥 하이브리드 에러 시스템
    std::string GetErrorJson() const;
    
    // Modbus 전용 에러 메서드
    int GetModbusErrorCode() const;
    std::string GetModbusErrorName() const;
    
    std::map<std::string, std::string> GetDiagnostics() const;

    // ==========================================================================
    // 🚀 스케일링 및 로드밸런싱 API
    // ==========================================================================
    
    /**
     * @brief 스케일링 기능 활성화
     * @param config 스케일링 설정
     * @return 성공 시 true
     */
    bool EnableScaling(const ScalingConfig& config);
    
    /**
     * @brief 스케일링 기능 비활성화 (기존 단일 연결로 복귀)
     */
    void DisableScaling();
    
    /**
     * @brief 스케일링 활성화 여부 확인
     */
    bool IsScalingEnabled() const;
    
    /**
     * @brief 연결 풀 상태 조회
     */
    PoolStatus GetPoolStatus() const;
    
    /**
     * @brief 로드 밸런싱 전략 변경
     */
    bool SetLoadBalancingStrategy(LoadBalancingStrategy strategy);
    
    /**
     * @brief 현재 로드 밸런싱 전략 조회
     */
    LoadBalancingStrategy GetCurrentStrategy() const;
    
    /**
     * @brief 수동 스케일링 (연결 추가)
     */
    bool ScaleUp(size_t additional_connections = 1);
    
    /**
     * @brief 수동 스케일링 (연결 제거)
     */
    bool ScaleDown(size_t connections_to_remove = 1);
    
    /**
     * @brief 연결 풀 최적화 (불량 연결 제거 및 교체)
     */
    void OptimizePool();
    
    /**
     * @brief 특정 연결 강제 재연결
     */
    bool ReconnectConnection(int connection_id);
    
    /**
     * @brief 모든 연결 재연결
     */
    bool ReconnectAllConnections();
    
    /**
     * @brief 연결 풀 통계 초기화
     */
    void ResetPoolStatistics();
    
    /**
     * @brief 고급 통계 정보 (풀 포함)
     */
    std::string GetAdvancedStatistics() const;
    
    /**
     * @brief 스케일링 이벤트 이력 조회
     */
    std::vector<ScalingEvent> GetScalingHistory(size_t max_events = 100) const;
    
    /**
     * @brief 연결 풀 성능 보고서
     */
    std::string GetPoolPerformanceReport() const;

    // ==========================================================================
    // 기존 진단 기능
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
    // ConnectionPool 관련
    bool PerformReadWithConnectionPool(const std::vector<Structs::DataPoint>& points,
                                     std::vector<TimestampedValue>& values);
    bool PerformWriteWithConnectionPool(const Structs::DataPoint& point, 
                                      const Structs::DataValue& value);
    
    // 진단 및 통계
    void UpdateRegisterAccessPattern(uint16_t address, bool is_read, bool is_write);
    void UpdateResponseTimeHistogram(double response_time_ms);
    
    // 대량 읽기
    bool ReadHoldingRegistersBulk(int slave_id, uint16_t start_addr, uint16_t count,
                                std::vector<uint16_t>& values, int max_retries = 3);
    
    // 새로운 에러 API
    std::string GetDetailedErrorInfo() const;
    DriverErrorCode GetDriverErrorCode() const;
    
    // 연결 관리
    bool PerformReadWithConnection(ModbusConnection* conn,
                                 const std::vector<Structs::DataPoint>& points,
                                 std::vector<TimestampedValue>& values);
private:
    // ==========================================================================
    // 기존 멤버 변수들
    // ==========================================================================
    
    // Modbus 연결 관련 (기존)
    modbus_t* modbus_ctx_;
    std::atomic<bool> is_connected_;
    std::mutex connection_mutex_;
    std::mutex operation_mutex_;
    int current_slave_id_;
    
    // 드라이버 설정 및 상태 (기존)
    DriverConfig config_;
    mutable DriverStatistics statistics_;
    ErrorInfo last_error_;
    std::chrono::steady_clock::time_point last_successful_operation_;
    mutable std::mutex stats_mutex_;
    
    // 진단 기능 관련 (기존)
    std::atomic<bool> diagnostics_enabled_;
    std::atomic<bool> packet_logging_enabled_;
    std::atomic<bool> console_output_enabled_;
    
    // 외부 의존성 (기존)
    LogManager* log_manager_;
    DatabaseManager* db_manager_;
    std::string device_name_;
    
    // 데이터 포인트 정보 관리 (기존)
    std::map<int, ModbusDataPointInfo> point_info_map_;
    mutable std::mutex points_mutex_;
    
    // 패킷 로깅 (기존)
    std::vector<ModbusPacketLog> packet_history_;
    mutable std::mutex packet_log_mutex_;
    static constexpr size_t MAX_PACKET_HISTORY = 1000;
    
    // 드라이버 로거 (기존)
    std::unique_ptr<DriverLogger> logger_;
    PulseOne::Drivers::ModbusConfig modbus_config_;
    
    // 진단 데이터 (기존)
    mutable std::mutex diagnostics_mutex_;
    std::map<uint8_t, std::atomic<uint64_t>> exception_counters_;
    std::atomic<uint64_t> total_crc_checks_{0};
    std::atomic<uint64_t> crc_errors_{0};
    std::array<std::atomic<uint64_t>, 5> response_time_buckets_;

    struct RegisterAccessPattern {
        std::atomic<uint64_t> read_count{0};
        std::atomic<uint64_t> write_count{0};
        std::chrono::system_clock::time_point last_access;
        std::atomic<uint32_t> avg_response_time_ms{0};
        RegisterAccessPattern() : last_access(std::chrono::system_clock::now()) {}
    };
    std::map<uint16_t, RegisterAccessPattern> register_access_patterns_;
    std::map<int, SlaveHealthInfo> slave_health_map_;

    // ==========================================================================
    // 🔥 새로운 스케일링 멤버 변수들
    // ==========================================================================
    
    // 연결 풀 관리
    std::vector<std::unique_ptr<ModbusConnection>> connection_pool_;
    std::queue<int> available_connections_;
    mutable std::mutex pool_mutex_;
    std::condition_variable pool_cv_;
    
    // 스케일링 설정 및 상태
    ScalingConfig scaling_config_;
    std::atomic<bool> scaling_enabled_{false};
    std::atomic<size_t> current_connection_index_{0};
    
    // 모니터링 스레드
    std::thread scaling_monitor_thread_;
    std::thread health_check_thread_;
    std::atomic<bool> scaling_monitor_running_{false};
    std::atomic<bool> health_check_running_{false};
    
    // 성능 모니터링
    std::atomic<double> pool_avg_response_time_{0.0};
    std::atomic<double> pool_success_rate_{100.0};
    std::atomic<uint64_t> pool_total_operations_{0};
    std::atomic<uint64_t> pool_successful_operations_{0};
    
    // 스케일링 이벤트 이력
    std::vector<ScalingEvent> scaling_history_;
    mutable std::mutex scaling_history_mutex_;
    static constexpr size_t MAX_SCALING_HISTORY = 1000;
    
    // 연결 가중치 (Weighted Round Robin용)
    std::map<int, double> connection_weights_;
    std::mutex weights_mutex_;

    // ==========================================================================
    // 기존 Private 메서드들
    // ==========================================================================
    
    // 기존 메서드들 (그대로 유지)
    bool LoadDataPointsFromDB();
    std::string GetPointName(int address) const;
    std::string GetPointDescription(int address) const;
    void LogModbusPacket(const std::string& direction, int slave_id, uint8_t function_code,
                        uint16_t start_addr, uint16_t count, const std::vector<uint16_t>& values = {},
                        bool success = true, const std::string& error_msg = "", double response_time_ms = 0.0);
    std::string FormatPointValue(int address, uint16_t raw_value) const;
    std::string FormatMultipleValues(uint16_t start_addr, const std::vector<uint16_t>& values) const;
    std::string FormatRawPacket(const std::vector<uint8_t>& packet) const;
    std::string GetFunctionName(uint8_t function_code) const;
    std::string FormatPacketForConsole(const ModbusPacketLog& log) const;
    void TrimPacketHistory();
    bool QueryDataPoints(const std::string& device_id);
    std::string QueryDeviceName(const std::string& device_id);
    void SetError(ErrorCode code, const std::string& message);
    void UpdateStatistics(bool success, double response_time_ms);
    Structs::DataValue ConvertModbusValue(const Structs::DataPoint& point, uint16_t raw_value) const;
    uint16_t ConvertToModbusValue(const Structs::DataPoint& point, const Structs::DataValue& value) const;
    
    // 🔥 에러 처리 메서드 (하이브리드 시스템)
    void HandleModbusError(int modbus_error, const std::string& context = "");

    // ==========================================================================
    // 🔥 새로운 스케일링 Private 메서드들
    // ==========================================================================
    
    // 연결 풀 관리
    bool InitializeConnectionPool();
    std::unique_ptr<ModbusConnection> CreateConnection(int connection_id);
    bool EstablishConnection(ModbusConnection* conn);
    ModbusConnection* AcquireConnection(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    void ReleaseConnection(ModbusConnection* conn);
    bool IsConnectionHealthy(const ModbusConnection* conn) const;
    
    // 로드 밸런싱
    ModbusConnection* SelectConnectionByStrategy();
    ModbusConnection* SelectRoundRobin();
    ModbusConnection* SelectLeastConnections();
    ModbusConnection* SelectHealthBased();
    ModbusConnection* SelectAdaptive();
    ModbusConnection* SelectWeightedRoundRobin();
    
    // 스케일링 로직
    void ScalingMonitorThread();
    void HealthCheckThread();
    bool ShouldScaleUp() const;
    bool ShouldScaleDown() const;
    void PerformScaleUp(size_t count, const std::string& reason);
    void PerformScaleDown(size_t count, const std::string& reason);
    void ReplaceUnhealthyConnections();
    
    // 성능 모니터링
    void UpdatePoolStatistics();
    void UpdateConnectionWeights();
    double CalculateConnectionScore(const ModbusConnection* conn) const;
    
    // 실제 작업 수행 (풀 지원)
    bool PerformWriteWithConnection(ModbusConnection* conn, const Structs::DataPoint& point,
                                   const Structs::DataValue& value);
    
    // 기존 단일 연결 방식 (호환성)
    bool PerformReadWithSingleConnection(const std::vector<Structs::DataPoint>& points,
                                        std::vector<TimestampedValue>& values);
    bool PerformWriteWithSingleConnection(const Structs::DataPoint& point,
                                         const Structs::DataValue& value);
    
    // 스케일링 이벤트 기록
    void RecordScalingEvent(ScalingEvent::Type type, const std::string& reason,
                           int connections_before, int connections_after, double trigger_metric = 0.0);
    void TrimScalingHistory();

protected:
    // ==========================================================================
    // 기존 Protected 메서드들
    // ==========================================================================
    
    void RecordExceptionCode(uint8_t exception_code);
    void RecordCrcCheck(bool crc_valid);
    void RecordResponseTime(int slave_id, uint32_t response_time_ms);
    void RecordRegisterAccess(uint16_t register_address, bool is_write, uint32_t response_time_ms);
    void RecordSlaveRequest(int slave_id, bool success, uint32_t response_time_ms);
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_MODBUS_DRIVER_H