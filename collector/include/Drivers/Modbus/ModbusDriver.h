// =============================================================================
// collector/include/Drivers/Modbus/ModbusDriver.h
// Modbus 프로토콜 드라이버 헤더 (선언만 포함)
// =============================================================================

#ifndef PULSEONE_DRIVERS_MODBUS_DRIVER_H
#define PULSEONE_DRIVERS_MODBUS_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Drivers/Common/DriverLogger.h"
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

namespace PulseOne {
namespace Drivers {

// =============================================================================
// Modbus 특화 타입 정의들
// =============================================================================

/**
 * @brief Modbus 데이터 포인트 정보
 */
struct ModbusDataPointInfo {
    std::string name;              // 포인트 이름
    std::string description;       // 포인트 설명
    std::string unit;              // 단위 (°C, bar, L/min 등)
    double scaling_factor;         // 스케일링 계수
    double scaling_offset;         // 스케일링 오프셋
    std::string data_type;         // 데이터 타입 (bool, int16, float 등)
    double min_value;              // 최소값
    double max_value;              // 최대값
    
    ModbusDataPointInfo();
};

/**
 * @brief Modbus 패킷 로그
 */
struct ModbusPacketLog {
    std::chrono::system_clock::time_point timestamp;
    std::string direction;         // "TX" or "RX"
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
 * @brief Modbus 프로토콜 드라이버
 * 
 * libmodbus 라이브러리를 사용하여 Modbus TCP/RTU 통신을 구현합니다.
 * 스레드 안전하며 진단 기능을 포함합니다.
 */
class ModbusDriver : public IProtocolDriver {
public:
    // ==========================================================================
    // 생성자 및 소멸자
    // ==========================================================================
    
    ModbusDriver();
    virtual ~ModbusDriver();
    
    // ==========================================================================
    // IProtocolDriver 인터페이스 구현 (선언만)
    // ==========================================================================
    
    bool Initialize(const DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    bool ReadValues(const std::vector<DataPoint>& points,
                   std::vector<TimestampedValue>& values) override;
    bool WriteValue(const DataPoint& point, const DataValue& value) override;
    
    ProtocolType GetProtocolType() const override;
    DriverStatus GetStatus() const override;
    ErrorInfo GetLastError() const override;
    const DriverStatistics& GetStatistics() const override;
    
    std::map<std::string, std::string> GetDiagnostics() const;
    
    // ==========================================================================
    // Modbus 특화 진단 기능 (선언만)
    // ==========================================================================
    
    bool EnableDiagnostics(DatabaseManager& db_manager,
                          bool enable_packet_logging = true,
                          bool enable_console_output = false);
    void DisableDiagnostics();
    void ToggleConsoleMonitoring();
    void TogglePacketLogging();
    std::string GetDiagnosticsJSON() const;
    std::string GetRecentPacketsJSON(int count = 100) const;

    // ==========================================================================
    // 🆕 Modbus 특화 진단 함수들 (기존 public: 섹션에 추가)
    // ==========================================================================
        // 슬레이브별 건강상태 추적
    // ==============================================================================
// std::atomic 복사 문제 해결 패치
// 파일: collector/include/Drivers/Modbus/ModbusDriver.h
// ==============================================================================

public:
    // ==========================================================================
    // 🔧 수정된 SlaveHealthInfo 구조체 (복사 가능하도록 수정)
    // ==========================================================================
    
    /**
     * @brief 슬레이브 건강상태 정보 (복사 가능한 버전)
     * @details std::atomic 대신 일반 타입 사용 + 뮤텍스로 동기화
     */
    struct SlaveHealthInfo {
        uint64_t successful_requests = 0;
        uint64_t failed_requests = 0;
        uint32_t avg_response_time_ms = 0;
        std::chrono::system_clock::time_point last_response_time;
        bool is_online = false;
        
        // 기본 생성자
        SlaveHealthInfo() : last_response_time(std::chrono::system_clock::now()) {}
        
        // 복사 생성자 (명시적 정의)
        SlaveHealthInfo(const SlaveHealthInfo& other) 
            : successful_requests(other.successful_requests)
            , failed_requests(other.failed_requests)
            , avg_response_time_ms(other.avg_response_time_ms)
            , last_response_time(other.last_response_time)
            , is_online(other.is_online) {}
        
        // 복사 할당 연산자
        SlaveHealthInfo& operator=(const SlaveHealthInfo& other) {
            if (this != &other) {
                successful_requests = other.successful_requests;
                failed_requests = other.failed_requests;
                avg_response_time_ms = other.avg_response_time_ms;
                last_response_time = other.last_response_time;
                is_online = other.is_online;
            }
            return *this;
        }
        
        // 성공률 계산
        double GetSuccessRate() const {
            uint64_t total = successful_requests + failed_requests;
            return total > 0 ? (double)successful_requests / total * 100.0 : 0.0;
        }
    };

    std::map<int, SlaveHealthInfo> slave_health_map_;
    /**
     * @brief Exception Code별 발생 통계 조회
     * @return Exception Code와 발생 횟수 맵
     */
    std::map<uint8_t, uint64_t> GetExceptionCodeStats() const;
    
    /**
     * @brief CRC 에러율 조회
     * @return CRC 에러율 (0.0 ~ 100.0)
     */
    double GetCrcErrorRate() const;
    
    /**
     * @brief 슬레이브별 건강상태 조회
     * @return 슬레이브 ID와 건강상태 정보 맵
     */
    std::map<int, SlaveHealthInfo> GetSlaveHealthStatus() const;
    
    /**
     * @brief 응답시간 히스토그램 조회
     * @return 응답시간 구간별 요청 수 배열
     */
    std::vector<uint64_t> GetResponseTimeHistogram() const;
    
    /**
     * @brief 레지스터 접근 패턴 분석 보고서
     * @return JSON 형태의 레지스터 접근 패턴 분석
     */
    std::string GetRegisterAccessReport() const;
    
    /**
     * @brief Modbus 종합 진단 보고서
     * @return JSON 형태의 종합 진단 정보
     */
    std::string GetModbusHealthReport() const;



private:
    // ==========================================================================
    // 멤버 변수들
    // ==========================================================================
    
    // Modbus 연결 관련
    modbus_t* modbus_ctx_;
    std::atomic<bool> is_connected_;
    std::mutex connection_mutex_;
    std::mutex operation_mutex_;
    int current_slave_id_;
    
    // 드라이버 설정 및 상태
    DriverConfig config_;
    mutable DriverStatistics statistics_;
    ErrorInfo last_error_;
    std::chrono::steady_clock::time_point last_successful_operation_;
    mutable std::mutex stats_mutex_;
    
    // 진단 기능 관련
    std::atomic<bool> diagnostics_enabled_;
    std::atomic<bool> packet_logging_enabled_;
    std::atomic<bool> console_output_enabled_;
    
    // 외부 의존성
    PulseOne::LogManager* log_manager_;
    DatabaseManager* db_manager_;
    std::string device_name_;
    
    // 데이터 포인트 정보 관리
    std::map<int, ModbusDataPointInfo> point_info_map_;
    mutable std::mutex points_mutex_;
    
    // 패킷 로깅
    std::vector<ModbusPacketLog> packet_history_;
    mutable std::mutex packet_log_mutex_;
    static constexpr size_t MAX_PACKET_HISTORY = 1000;
    
    // 드라이버 로거
    std::unique_ptr<DriverLogger> logger_;

    // ==========================================================================
    // Private 메소드 선언들
    // ==========================================================================
    
    bool LoadDataPointsFromDB();
    std::string GetPointName(int address) const;
    std::string GetPointDescription(int address) const;
    
    void LogModbusPacket(const std::string& direction,
                        int slave_id,
                        uint8_t function_code,
                        uint16_t start_addr,
                        uint16_t count,
                        const std::vector<uint16_t>& values = {},
                        bool success = true,
                        const std::string& error_msg = "",
                        double response_time_ms = 0.0);
    
    std::string FormatPointValue(int address, uint16_t raw_value) const;
    std::string FormatMultipleValues(uint16_t start_addr, 
                                    const std::vector<uint16_t>& values) const;
    std::string FormatRawPacket(const std::vector<uint8_t>& packet) const;
    std::string GetFunctionName(uint8_t function_code) const;
    std::string FormatPacketForConsole(const ModbusPacketLog& log) const;
    void TrimPacketHistory();
    
    bool QueryDataPoints(const std::string& device_id);
    std::string QueryDeviceName(const std::string& device_id);
    
    void SetError(ErrorCode code, const std::string& message);
    void UpdateStatistics(bool success, double response_time_ms);
    
    DataValue ConvertModbusValue(const DataPoint& point, uint16_t raw_value) const;
    uint16_t ConvertToModbusValue(const DataPoint& point, const DataValue& value) const;

    // ==========================================================================
    // 🆕 Modbus 특화 진단 데이터 (기존 private: 섹션에 추가)
    // ==========================================================================
    mutable std::mutex diagnostics_mutex_;
    
    // Exception Code별 통계
    std::map<uint8_t, std::atomic<uint64_t>> exception_counters_;
    
    // CRC 에러 추적
    std::atomic<uint64_t> total_crc_checks_{0};
    std::atomic<uint64_t> crc_errors_{0};
    
    // 응답시간 히스토그램 (0-10ms, 10-50ms, 50-100ms, 100-500ms, 500ms+)
    std::array<std::atomic<uint64_t>, 5> response_time_buckets_;
    
    // 레지스터 접근 패턴 추적
    struct RegisterAccessPattern {
        std::atomic<uint64_t> read_count{0};
        std::atomic<uint64_t> write_count{0};
        std::chrono::system_clock::time_point last_access;
        std::atomic<uint32_t> avg_response_time_ms{0};
        
        RegisterAccessPattern() : last_access(std::chrono::system_clock::now()) {}
    };
    std::map<uint16_t, RegisterAccessPattern> register_access_patterns_;

protected:
    // ==========================================================================
    // 🆕 내부 진단 업데이트 헬퍼 함수들 (기존 protected: 섹션에 추가)
    // ==========================================================================
    
    /**
     * @brief Exception Code 발생 기록
     * @param exception_code Modbus Exception Code
     */
    void RecordExceptionCode(uint8_t exception_code);
    
    /**
     * @brief CRC 검사 결과 기록
     * @param crc_valid CRC 검사 통과 여부
     */
    void RecordCrcCheck(bool crc_valid);
    
    /**
     * @brief 응답시간 기록
     * @param slave_id 슬레이브 ID
     * @param response_time_ms 응답시간 (밀리초)
     */
    void RecordResponseTime(int slave_id, uint32_t response_time_ms);
    
    /**
     * @brief 레지스터 접근 기록
     * @param register_address 레지스터 주소
     * @param is_write 쓰기 여부 (false면 읽기)
     * @param response_time_ms 응답시간
     */
    void RecordRegisterAccess(uint16_t register_address, bool is_write, uint32_t response_time_ms);
    
    /**
     * @brief 슬레이브 요청 결과 기록
     * @param slave_id 슬레이브 ID
     * @param success 성공 여부
     * @param response_time_ms 응답시간
     */
    void RecordSlaveRequest(int slave_id, bool success, uint32_t response_time_ms);    

};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_MODBUS_DRIVER_H