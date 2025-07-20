// =============================================================================
// collector/include/Drivers/ModbusDriver.h
// Modbus 프로토콜 드라이버 헤더 (진단 기능 통합)
// =============================================================================

#ifndef PULSEONE_DRIVERS_MODBUS_DRIVER_H
#define PULSEONE_DRIVERS_MODBUS_DRIVER_H

#include "IProtocolDriver.h"
#include "DriverLogger.h"
#include "Utils/LogManager.h"           // ✅ 추가: 기존 LogManager
#include "Database/DatabaseManager.h"   // ✅ 추가: DB 연결
#include <modbus.h>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>

namespace PulseOne {
namespace Drivers {

// ✅ 진단용 구조체 추가
struct ModbusDataPointInfo {
    std::string name;              // 포인트 이름
    std::string description;       // 포인트 설명
    std::string unit;              // 단위 (°C, bar, L/min 등)
    double scaling_factor;         // 스케일링 계수
    double scaling_offset;         // 스케일링 오프셋
    std::string data_type;         // 데이터 타입 (bool, int16, float 등)
    double min_value;              // 최소값
    double max_value;              // 최대값
};

/**
 * @brief Modbus 프로토콜 드라이버 (진단 기능 통합)
 * 
 * libmodbus 라이브러리를 사용하여 Modbus TCP/RTU 통신을 구현합니다.
 * 스레드 안전하며 비동기 읽기/쓰기를 지원합니다.
 * ✅ 데이터베이스 연동 진단 기능 포함
 */
class ModbusDriver : public IProtocolDriver {
public:
    ModbusDriver();
    virtual ~ModbusDriver();
    
    // ==========================================================================
    // IProtocolDriver 인터페이스 구현 (기존과 동일)
    // ==========================================================================
    bool Initialize(const DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    bool ReadValues(
        const std::vector<DataPoint>& points,
        std::vector<TimestampedValue>& values
    ) override;
    
    bool WriteValue(
        const DataPoint& point,
        const DataValue& value
    ) override;
    
    ProtocolType GetProtocolType() const override;
    DriverStatus GetStatus() const override;
    ErrorInfo GetLastError() const override;
    DriverStatistics GetStatistics() const override;
    void ResetStatistics() override;
    std::map<std::string, std::string> GetDiagnostics() const override;
    
    // ==========================================================================
    // ✅ 새로 추가: 진단 기능 (기존 인터페이스와 충돌 없음)
    // ==========================================================================
    
    /**
     * @brief 진단 기능 활성화
     * @param db_manager 데이터베이스 매니저 참조
     * @param enable_packet_logging 패킷 로깅 활성화
     * @param enable_console_output 콘솔 실시간 출력
     * @return 성공 시 true
     */
    bool EnableDiagnostics(DatabaseManager& db_manager,
                          bool enable_packet_logging = true,
                          bool enable_console_output = false);
    
    /**
     * @brief 진단 기능 비활성화
     */
    void DisableDiagnostics();
    
    /**
     * @brief 실시간 콘솔 모니터링 토글
     */
    void ToggleConsoleMonitoring();
    
    /**
     * @brief 패킷 로깅 토글
     */
    void TogglePacketLogging();
    
    /**
     * @brief 웹 인터페이스용 진단 정보 (JSON)
     * @return JSON 형태의 진단 정보
     */
    std::string GetDiagnosticsJSON() const;
    
    /**
     * @brief 최근 패킷 로그 조회 (웹용)
     * @param count 조회할 패킷 수
     * @return JSON 형태의 패킷 로그
     */
    std::string GetRecentPacketsJSON(int count = 20) const;
    
    /**
     * @brief 포인트 이름/설명 조회
     * @param address Modbus 주소
     * @return 포인트 이름
     */
    std::string GetPointName(int address) const;
    std::string GetPointDescription(int address) const;
    
    // ==========================================================================
    // Modbus 특화 메소드들 (기존과 동일)
    // ==========================================================================
    
    /**
     * @brief Holding Register 읽기
     * @param slave_id 슬레이브 ID
     * @param start_addr 시작 주소
     * @param count 읽을 레지스터 수
     * @param values 읽은 값들
     * @return 성공 시 true
     */
    bool ReadHoldingRegisters(
        int slave_id, 
        int start_addr, 
        int count, 
        std::vector<uint16_t>& values
    );
    
    /**
     * @brief Input Register 읽기
     * @param slave_id 슬레이브 ID
     * @param start_addr 시작 주소
     * @param count 읽을 레지스터 수
     * @param values 읽은 값들
     * @return 성공 시 true
     */
    bool ReadInputRegisters(
        int slave_id, 
        int start_addr, 
        int count, 
        std::vector<uint16_t>& values
    );
    
    /**
     * @brief Coil 읽기
     * @param slave_id 슬레이브 ID
     * @param start_addr 시작 주소
     * @param count 읽을 코일 수
     * @param values 읽은 값들
     * @return 성공 시 true
     */
    bool ReadCoils(
        int slave_id, 
        int start_addr, 
        int count, 
        std::vector<bool>& values
    );
    
    /**
     * @brief Discrete Input 읽기
     * @param slave_id 슬레이브 ID
     * @param start_addr 시작 주소
     * @param count 읽을 입력 수
     * @param values 읽은 값들
     * @return 성공 시 true
     */
    bool ReadDiscreteInputs(
        int slave_id, 
        int start_addr, 
        int count, 
        std::vector<bool>& values
    );
    
    /**
     * @brief Single Holding Register 쓰기
     * @param slave_id 슬레이브 ID
     * @param addr 주소
     * @param value 쓸 값
     * @return 성공 시 true
     */
    bool WriteSingleRegister(int slave_id, int addr, uint16_t value);
    
    /**
     * @brief Multiple Holding Register 쓰기
     * @param slave_id 슬레이브 ID
     * @param start_addr 시작 주소
     * @param values 쓸 값들
     * @return 성공 시 true
     */
    bool WriteMultipleRegisters(
        int slave_id, 
        int start_addr, 
        const std::vector<uint16_t>& values
    );
    
    /**
     * @brief Single Coil 쓰기
     * @param slave_id 슬레이브 ID
     * @param addr 주소
     * @param value 쓸 값
     * @return 성공 시 true
     */
    bool WriteSingleCoil(int slave_id, int addr, bool value);
    
    /**
     * @brief Multiple Coil 쓰기
     * @param slave_id 슬레이브 ID
     * @param start_addr 시작 주소
     * @param values 쓸 값들
     * @return 성공 시 true
     */
    bool WriteMultipleCoils(
        int slave_id, 
        int start_addr, 
        const std::vector<bool>& values
    );

private:
    // ==========================================================================
    // 기존 내부 구조체 및 열거형 (변경 없음)
    // ==========================================================================
    
    enum class ModbusFunction {
        READ_COILS = 0x01,
        READ_DISCRETE_INPUTS = 0x02,
        READ_HOLDING_REGISTERS = 0x03,
        READ_INPUT_REGISTERS = 0x04,
        WRITE_SINGLE_COIL = 0x05,
        WRITE_SINGLE_REGISTER = 0x06,
        WRITE_MULTIPLE_COILS = 0x0F,
        WRITE_MULTIPLE_REGISTERS = 0x10
    };
    
    struct ModbusRequest {
        int slave_id;
        ModbusFunction function;
        int start_addr;
        int count;
        std::vector<uint16_t> write_values;
        std::promise<bool> promise;
        std::chrono::steady_clock::time_point request_time;
        
        ModbusRequest(int sid, ModbusFunction func, int addr, int cnt = 1)
            : slave_id(sid), function(func), start_addr(addr), count(cnt),
              request_time(std::chrono::steady_clock::now()) {}
    };
    
    // ✅ 새로 추가: 패킷 로그 구조체
    struct PacketLog {
        std::chrono::system_clock::time_point timestamp;
        std::string direction;         // "TX" / "RX"
        int slave_id;
        uint8_t function_code;
        uint16_t start_address;
        uint16_t data_count;
        std::vector<uint8_t> raw_packet;
        std::vector<uint16_t> values;
        bool success;
        std::string error_message;
        double response_time_ms;
        std::string decoded_values;    // 엔지니어 친화적 값들
    };
    
    // ==========================================================================
    // 기존 멤버 변수들 (변경 없음)
    // ==========================================================================
    
    // 설정
    DriverConfig config_;
    ProtocolType protocol_type_;
    
    // libmodbus 컨텍스트
    modbus_t* modbus_ctx_;
    
    // 상태 관리
    std::atomic<DriverStatus> status_;
    std::atomic<bool> is_connected_;
    
    // 에러 정보
    mutable std::mutex error_mutex_;
    ErrorInfo last_error_;
    
    // 통계 정보
    mutable std::mutex stats_mutex_;
    DriverStatistics statistics_;
    
    // 로깅
    std::unique_ptr<DriverLogger> logger_;
    
    // 동기화
    mutable std::mutex connection_mutex_;
    mutable std::mutex operation_mutex_;
    
    // 연결 감시 및 재연결
    std::thread watchdog_thread_;
    std::atomic<bool> stop_watchdog_;
    
    // 타임아웃 관리
    std::chrono::steady_clock::time_point last_successful_operation_;
    
    // 진단 정보
    mutable std::mutex diagnostics_mutex_;
    std::map<std::string, std::string> diagnostics_;
    
    // ==========================================================================
    // ✅ 새로 추가: 진단 관련 멤버들
    // ==========================================================================
    
    // 진단 설정
    bool diagnostics_enabled_;
    bool packet_logging_enabled_;
    bool console_output_enabled_;
    
    // 기존 시스템 참조
    LogManager* log_manager_;              // ✅ 기존 LogManager 사용
    DatabaseManager* db_manager_;          // ✅ 기존 DB 매니저 사용
    
    // 데이터베이스에서 로드한 포인트 정보
    mutable std::mutex points_mutex_;
    std::map<int, ModbusDataPointInfo> point_info_map_; // address -> 정보
    std::string device_name_;
    
    // 패킷 로그 히스토리 (최근 N개 보관)
    mutable std::mutex packet_log_mutex_;
    std::deque<PacketLog> packet_history_;
    static const size_t MAX_PACKET_HISTORY = 1000;
    
    // ==========================================================================
    // 기존 내부 메소드들 (변경 없음)
    // ==========================================================================
    
    bool InitializeTCP(const std::string& host, int port);
    bool InitializeRTU(const std::string& device, int baud, char parity, int data_bits, int stop_bits);
    
    void SetError(ErrorCode code, const std::string& message);
    void UpdateStatistics(bool success, double response_time_ms);
    
    // 연결 관리
    bool EstablishConnection();
    void CloseConnection();
    bool ValidateConnection();
    
    // 데이터 변환 유틸리티 (기존)
    DataValue ConvertModbusValue(const DataPoint& point, uint16_t raw_value);
    uint16_t ConvertToModbusValue(const DataPoint& point, const DataValue& value);
    ModbusFunction GetReadFunction(const DataPoint& point);
    ModbusFunction GetWriteFunction(const DataPoint& point);
    
    // 통신 헬퍼 (기존)
    bool ExecuteRead(int slave_id, ModbusFunction function, int start_addr, 
                     int count, std::vector<uint16_t>& values);
    bool ExecuteWrite(int slave_id, ModbusFunction function, int addr, 
                      uint16_t value);
    bool ExecuteWriteMultiple(int slave_id, ModbusFunction function, 
                              int start_addr, const std::vector<uint16_t>& values);
    
    // 에러 처리 (기존)
    ErrorCode TranslateModbusError(int modbus_errno);
    std::string GetModbusErrorString(int modbus_errno);
    
    // Watchdog (기존)
    void WatchdogLoop();
    void UpdateDiagnostics();
    
    // ==========================================================================
    // ✅ 새로 추가: 진단 관련 헬퍼 함수들
    // ==========================================================================
    
    /**
     * @brief 데이터베이스에서 포인트 정보 로드
     * @return 성공 시 true
     */
    bool LoadDataPointsFromDB();
    
    /**
     * @brief 패킷 로깅 (기존 LogManager 사용)
     */
    void LogModbusPacket(const std::string& direction,
                        int slave_id, uint8_t function_code,
                        uint16_t start_addr, uint16_t count,
                        const std::vector<uint16_t>& values = {},
                        bool success = true,
                        const std::string& error_msg = "",
                        double response_time_ms = 0.0);
    
    /**
     * @brief 엔지니어 친화적 값 포매팅
     * @param address Modbus 주소
     * @param raw_value 원시 값
     * @return 포맷된 값 (예: "25.0°C")
     */
    std::string FormatPointValue(int address, uint16_t raw_value) const;
    
    /**
     * @brief 다중 값 포매팅
     * @param start_addr 시작 주소
     * @param values 값들
     * @return 포맷된 문자열 (예: "TEMP_01: 25.0°C, PRESSURE: 6.25bar")
     */
    std::string FormatMultipleValues(uint16_t start_addr, 
                                    const std::vector<uint16_t>& values) const;
    
    /**
     * @brief 패킷 바이트 포매팅
     * @param packet 패킷 바이트들
     * @return 16진수 문자열 (예: "01 03 9C 40 00 05 8E 5C")
     */
    std::string FormatRawPacket(const std::vector<uint8_t>& packet) const;
    
    /**
     * @brief Modbus 함수 코드 이름 반환
     * @param function_code 함수 코드
     * @return 함수 이름 (예: "Read Holding Registers")
     */
    std::string GetFunctionName(uint8_t function_code) const;
    
    /**
     * @brief 콘솔용 패킷 포매팅
     * @param log 패킷 로그
     * @return 콘솔 출력용 문자열
     */
    std::string FormatPacketForConsole(const PacketLog& log) const;
    
    /**
     * @brief 패킷 히스토리 관리 (최대 개수 유지)
     */
    void TrimPacketHistory();
    
    /**
     * @brief 데이터베이스 쿼리 헬퍼들
     */
    bool QueryDataPoints(const std::string& device_id);
    std::string QueryDeviceName(const std::string& device_id);
    
    // ==========================================================================
    // 기존 ExecuteRead/Write 메소드들을 진단 기능과 함께 오버로드
    // ==========================================================================
    
    /**
     * @brief 진단 기능이 포함된 읽기 실행
     */
    bool ExecuteReadWithDiagnostics(int slave_id, ModbusFunction function, 
                                   int start_addr, int count, 
                                   std::vector<uint16_t>& values);
    
    /**
     * @brief 진단 기능이 포함된 쓰기 실행
     */
    bool ExecuteWriteWithDiagnostics(int slave_id, ModbusFunction function, 
                                    int addr, uint16_t value);
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_MODBUS_DRIVER_H