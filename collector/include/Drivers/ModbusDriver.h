// =============================================================================
// collector/include/Drivers/ModbusDriver.h
// Modbus 프로토콜 드라이버 헤더
// =============================================================================

#ifndef PULSEONE_DRIVERS_MODBUS_DRIVER_H
#define PULSEONE_DRIVERS_MODBUS_DRIVER_H

#include "IProtocolDriver.h"
#include "DriverLogger.h"
#include <modbus.h>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>

namespace PulseOne {
namespace Drivers {

/**
 * @brief Modbus 프로토콜 드라이버
 * 
 * libmodbus 라이브러리를 사용하여 Modbus TCP/RTU 통신을 구현합니다.
 * 스레드 안전하며 비동기 읽기/쓰기를 지원합니다.
 */
class ModbusDriver : public IProtocolDriver {
public:
    ModbusDriver();
    virtual ~ModbusDriver();
    
    // IProtocolDriver 인터페이스 구현
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
    
    // Modbus 특화 메소드들
    
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
    // 내부 구조체 및 열거형
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
    
    // ==========================================================================
    // 내부 메소드들
    // ==========================================================================
    
    bool InitializeTCP(const std::string& host, int port);
    bool InitializeRTU(const std::string& device, int baud, char parity, int data_bits, int stop_bits);
    
    void SetError(ErrorCode code, const std::string& message);
    void UpdateStatistics(bool success, double response_time_ms);
    
    // 연결 관리
    bool EstablishConnection();
    void CloseConnection();
    bool ValidateConnection();
    
    // 데이터 변환 유틸리티
    DataValue ConvertModbusValue(const DataPoint& point, uint16_t raw_value);
    uint16_t ConvertToModbusValue(const DataPoint& point, const DataValue& value);
    ModbusFunction GetReadFunction(const DataPoint& point);
    ModbusFunction GetWriteFunction(const DataPoint& point);
    
    // 통신 헬퍼
    bool ExecuteRead(int slave_id, ModbusFunction function, int start_addr, 
                     int count, std::vector<uint16_t>& values);
    bool ExecuteWrite(int slave_id, ModbusFunction function, int addr, 
                      uint16_t value);
    bool ExecuteWriteMultiple(int slave_id, ModbusFunction function, 
                              int start_addr, const std::vector<uint16_t>& values);
    
    // 에러 처리
    ErrorCode TranslateModbusError(int modbus_errno);
    std::string GetModbusErrorString(int modbus_errno);
    
    // ==========================================================================
    // 멤버 변수들
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
    void WatchdogLoop();
    
    // 타임아웃 관리
    std::chrono::steady_clock::time_point last_successful_operation_;
    
    // 진단 정보
    mutable std::mutex diagnostics_mutex_;
    std::map<std::string, std::string> diagnostics_;
    void UpdateDiagnostics();
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_MODBUS_DRIVER_H