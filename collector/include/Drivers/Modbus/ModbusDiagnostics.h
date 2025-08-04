// =============================================================================
// collector/include/Drivers/Modbus/ModbusDiagnostics.h
// Modbus 진단 및 모니터링 기능 (선택적 활성화)
// =============================================================================

#ifndef PULSEONE_MODBUS_DIAGNOSTICS_H
#define PULSEONE_MODBUS_DIAGNOSTICS_H

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <deque>
#include <array>
#include <chrono>

namespace PulseOne {
namespace Drivers {

// 전방 선언
class ModbusDriver;

/**
 * @brief Slave 건강 상태 정보
 */
struct SlaveHealthInfo {
    int slave_id;
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_requests{0};
    std::atomic<uint64_t> failed_requests{0};
    std::atomic<uint32_t> average_response_time_ms{0};
    std::chrono::system_clock::time_point last_success;
    std::chrono::system_clock::time_point last_failure;
    
    SlaveHealthInfo(int id = 0) : slave_id(id) {
        auto now = std::chrono::system_clock::now();
        last_success = now;
        last_failure = now;
    }
    
    // 복사 생성자 (atomic 값들을 load해서 복사)
    SlaveHealthInfo(const SlaveHealthInfo& other) 
        : slave_id(other.slave_id)
        , total_requests(other.total_requests.load())
        , successful_requests(other.successful_requests.load())
        , failed_requests(other.failed_requests.load())
        , average_response_time_ms(other.average_response_time_ms.load())
        , last_success(other.last_success)
        , last_failure(other.last_failure) {}
    
    // 할당 연산자 (atomic 값들을 store해서 할당)
    SlaveHealthInfo& operator=(const SlaveHealthInfo& other) {
        if (this != &other) {
            slave_id = other.slave_id;
            total_requests.store(other.total_requests.load());
            successful_requests.store(other.successful_requests.load());
            failed_requests.store(other.failed_requests.load());
            average_response_time_ms.store(other.average_response_time_ms.load());
            last_success = other.last_success;
            last_failure = other.last_failure;
        }
        return *this;
    }
};

/**
 * @brief 레지스터 접근 패턴 정보
 */
struct RegisterAccessPattern {
    uint16_t address;
    std::atomic<uint64_t> read_count{0};
    std::atomic<uint64_t> write_count{0};
    std::chrono::system_clock::time_point last_access;
    
    RegisterAccessPattern(uint16_t addr = 0) : address(addr) {
        last_access = std::chrono::system_clock::now();
    }
    
    // 복사 생성자 (atomic 값들을 load해서 복사)
    RegisterAccessPattern(const RegisterAccessPattern& other)
        : address(other.address)
        , read_count(other.read_count.load())
        , write_count(other.write_count.load())
        , last_access(other.last_access) {}
    
    // 할당 연산자 (atomic 값들을 store해서 할당)
    RegisterAccessPattern& operator=(const RegisterAccessPattern& other) {
        if (this != &other) {
            address = other.address;
            read_count.store(other.read_count.load());
            write_count.store(other.write_count.load());
            last_access = other.last_access;
        }
        return *this;
    }
};

/**
 * @brief Modbus 패킷 로그 정보
 */
struct ModbusPacketLog {
    std::chrono::system_clock::time_point timestamp;
    int slave_id;
    uint8_t function_code;
    uint16_t start_address;
    uint16_t count;
    bool is_request;
    bool success;
    uint32_t response_time_ms;
    std::string error_message;
    
    ModbusPacketLog() {
        timestamp = std::chrono::system_clock::now();
        slave_id = 0;
        function_code = 0;
        start_address = 0;
        count = 0;
        is_request = true;
        success = false;
        response_time_ms = 0;
    }
};

/**
 * @brief Modbus 진단 및 모니터링 기능
 * @details ModbusDriver에서 선택적으로 활성화되는 고급 진단 기능
 * 
 * 🎯 기능:
 * - 예외 코드 추적
 * - CRC 에러 모니터링  
 * - 응답 시간 히스토그램
 * - Slave 건강 상태 추적
 * - 레지스터 접근 패턴 분석
 * - 패킷 로깅 (선택적)
 */
class ModbusDiagnostics {
public:
    explicit ModbusDiagnostics(ModbusDriver* parent_driver);
    ~ModbusDiagnostics();
    
    // 복사/이동 방지
    ModbusDiagnostics(const ModbusDiagnostics&) = delete;
    ModbusDiagnostics& operator=(const ModbusDiagnostics&) = delete;
    
    // =======================================================================
    // 진단 기능 활성화/비활성화
    // =======================================================================
    
    bool EnableDiagnostics(bool packet_logging = true, bool console_output = false);
    void DisableDiagnostics();
    bool IsDiagnosticsEnabled() const;
    
    void EnablePacketLogging(bool enable = true);
    void EnableConsoleOutput(bool enable = true);
    bool IsPacketLoggingEnabled() const;
    bool IsConsoleOutputEnabled() const;
    
    // =======================================================================
    // 진단 정보 수집 (ModbusDriver에서 호출)
    // =======================================================================
    
    void RecordExceptionCode(uint8_t exception_code);
    void RecordCrcCheck(bool crc_valid);
    void RecordResponseTime(int slave_id, uint32_t response_time_ms);
    void RecordRegisterAccess(uint16_t address, bool is_read, bool is_write);
    void RecordSlaveRequest(int slave_id, bool success, uint32_t response_time_ms);
    
    void LogPacket(int slave_id, uint8_t function_code, uint16_t start_addr, 
                   uint16_t count, bool is_request, bool success, 
                   uint32_t response_time_ms = 0, const std::string& error = "");
    
    // =======================================================================
    // 진단 정보 조회
    // =======================================================================
    
    std::string GetDiagnosticsJSON() const;
    std::map<std::string, std::string> GetDiagnostics() const;
    
    // 상세 통계 정보
    std::vector<uint64_t> GetResponseTimeHistogram() const;
    std::map<uint8_t, uint64_t> GetExceptionCodeStats() const;
    double GetCrcErrorRate() const;
    std::map<int, SlaveHealthInfo> GetSlaveHealthStatus() const;
    std::map<uint16_t, RegisterAccessPattern> GetRegisterAccessPatterns() const;
    
    // 패킷 로그 조회
    std::string GetRecentPacketsJSON(int count = 100) const;
    std::vector<ModbusPacketLog> GetRecentPackets(int count = 100) const;
    
    // 진단 제어
    void ResetDiagnostics();
    void TogglePacketLogging();
    void ToggleConsoleMonitoring();

private:
    // =======================================================================
    // 멤버 변수
    // =======================================================================
    
    ModbusDriver* parent_driver_;         // 부모 드라이버 (raw pointer - 순환 참조 방지)
    
    // 활성화 상태
    std::atomic<bool> diagnostics_enabled_;
    std::atomic<bool> packet_logging_enabled_;
    std::atomic<bool> console_output_enabled_;
    
    // 기본 통계
    std::atomic<uint64_t> total_requests_;
    std::atomic<uint64_t> successful_requests_;
    std::atomic<uint64_t> failed_requests_;
    std::atomic<uint64_t> total_crc_checks_;
    std::atomic<uint64_t> crc_errors_;
    
    // 예외 코드 통계 (Modbus 표준 예외 코드: 1-8)
    std::array<std::atomic<uint64_t>, 16> exception_code_counts_;
    
    // 응답 시간 히스토그램 (0-10ms, 10-50ms, 50-100ms, 100-500ms, 500ms+)
    std::array<std::atomic<uint64_t>, 5> response_time_histogram_;
    
    // Slave 건강 상태 (slave_id -> SlaveHealthInfo)
    std::unordered_map<int, SlaveHealthInfo> slave_health_map_;
    mutable std::mutex slave_health_mutex_;
    
    // 레지스터 접근 패턴 (최대 1000개 레지스터 추적)
    std::unordered_map<uint16_t, RegisterAccessPattern> register_access_map_;
    mutable std::mutex register_access_mutex_;
    
    // 패킷 로그 (순환 버퍼, 최대 1000개)
    std::deque<ModbusPacketLog> packet_log_queue_;
    mutable std::mutex packet_log_mutex_;
    static constexpr size_t MAX_PACKET_LOGS = 1000;
    
    // 진단 시작 시간
    std::chrono::system_clock::time_point diagnostics_start_time_;
    
    // =======================================================================
    // 내부 메서드
    // =======================================================================
    
    void InitializeDiagnostics();
    void CleanupDiagnostics();
    void UpdateStatistics();
    
    // 응답 시간을 히스토그램 인덱스로 변환
    size_t GetResponseTimeHistogramIndex(uint32_t response_time_ms) const;
    
    // JSON 생성 헬퍼
    std::string CreateDiagnosticsJSON() const;
    std::string CreatePacketLogJSON(const std::vector<ModbusPacketLog>& logs) const;
    
    // 콘솔 출력 (선택적)
    void PrintDiagnosticInfo(const std::string& message) const;
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_MODBUS_DIAGNOSTICS_H