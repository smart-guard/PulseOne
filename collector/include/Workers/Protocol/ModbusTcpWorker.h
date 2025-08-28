/**
 * @file ModbusTcpWorker.h (완성본)
 * @brief Modbus TCP 디바이스 워커 클래스 - BaseDeviceWorker Write 인터페이스 구현
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 2.1.0 (완성본)
 */

#ifndef MODBUS_TCP_WORKER_H
#define MODBUS_TCP_WORKER_H

#include "Workers/Base/TcpBasedWorker.h"
#include "Common/BasicTypes.h"           
#include "Common/Enums.h"                
#include "Common/Structs.h"              
#include "Drivers/Modbus/ModbusDriver.h"
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <queue>

namespace PulseOne {
namespace Workers {

    // 타입 별칭 명시적 선언
    using DataValue = PulseOne::Structs::DataValue;          
    using TimestampedValue = PulseOne::Structs::TimestampedValue;
    using DataPoint = PulseOne::Structs::DataPoint;
    using DeviceInfo = PulseOne::Structs::DeviceInfo;
    using ErrorInfo = PulseOne::Structs::ErrorInfo;
    using DriverStatistics = PulseOne::Structs::DriverStatistics;
    
    // 열거형 타입들
    using DataQuality = PulseOne::Enums::DataQuality;
    using ConnectionStatus = PulseOne::Enums::ConnectionStatus;
    using ProtocolType = PulseOne::Enums::ProtocolType;
    // 기본 타입들
    using UUID = PulseOne::BasicTypes::UUID;
    using Timestamp = PulseOne::BasicTypes::Timestamp;

    // Modbus 특화 타입들
    using ModbusDriver = PulseOne::Drivers::ModbusDriver;
    using ModbusRegisterType = PulseOne::Enums::ModbusRegisterType;

/**
 * @brief Modbus TCP 폴링 그룹
 */
struct ModbusTcpPollingGroup {
    uint32_t group_id;                               
    uint8_t slave_id;                                
    ModbusRegisterType register_type;                
    uint16_t start_address;                          
    uint16_t register_count;                         
    uint32_t polling_interval_ms;                    
    bool enabled;                                    
    
    std::vector<PulseOne::DataPoint> data_points;     
    
    // 실행 시간 추적
    std::chrono::system_clock::time_point last_poll_time;
    std::chrono::system_clock::time_point next_poll_time;
    
    ModbusTcpPollingGroup() 
        : group_id(0), slave_id(1)
        , register_type(ModbusRegisterType::HOLDING_REGISTER)
        , start_address(0), register_count(1), polling_interval_ms(1000), enabled(true)
        , last_poll_time(std::chrono::system_clock::now())
        , next_poll_time(std::chrono::system_clock::now()) {}
};

/**
 * @brief Modbus TCP 워커 클래스 (완성본)
 */
class ModbusTcpWorker : public TcpBasedWorker {
public:
    explicit ModbusTcpWorker(const PulseOne::DeviceInfo& device_info);
    virtual ~ModbusTcpWorker();

    // =============================================================================
    // BaseDeviceWorker 인터페이스 구현
    // =============================================================================
    
    std::future<bool> Start() override;
    std::future<bool> Stop() override;
    
    // =============================================================================
    // BaseDeviceWorker Write 인터페이스 구현 (완성!)
    // =============================================================================
    
    /**
     * @brief DataPoint를 통한 값 쓰기 (BaseDeviceWorker 인터페이스 구현)
     * @param point_id DataPoint ID
     * @param value 쓸 값
     * @return 성공 시 true
     */
    bool WriteDataPoint(const std::string& point_id, const DataValue& value) override;
    
    /**
     * @brief 아날로그 출력 제어 (BaseDeviceWorker 인터페이스 구현)
     * @param output_id 출력 ID (Modbus에서는 DataPoint ID 또는 "slave:address" 형식)
     * @param value 아날로그 값
     * @return 성공 시 true
     */
    bool WriteAnalogOutput(const std::string& output_id, double value) override;
    
    /**
     * @brief 디지털 출력 제어 (BaseDeviceWorker 인터페이스 구현)
     * @param output_id 출력 ID (Modbus에서는 DataPoint ID 또는 "slave:address" 형식)
     * @param value 디지털 값 (true/false)
     * @return 성공 시 true
     */
    bool WriteDigitalOutput(const std::string& output_id, bool value) override;
    
    /**
     * @brief 설정값 변경 (BaseDeviceWorker 인터페이스 구현)
     * @param setpoint_id 설정값 ID
     * @param value 새로운 설정값
     * @return 성공 시 true
     */
    bool WriteSetpoint(const std::string& setpoint_id, double value) override;
    
    /**
     * @brief 펌프 제어 (BaseDeviceWorker 인터페이스 구현)
     * @param pump_id 펌프 ID (Modbus Coil 주소)
     * @param enable true=시작, false=정지
     * @return 성공 시 true
     */
    bool ControlDigitalDevice(const std::string& pump_id, bool enable) override;
    
    /**
     * @brief 밸브 제어 (BaseDeviceWorker 인터페이스 구현)
     * @param valve_id 밸브 ID (Modbus Holding Register 주소)
     * @param position 밸브 위치 (0.0~100.0 %)
     * @return 성공 시 true
     */
    bool ControlAnalogDevice(const std::string& valve_id, double position) override;

    // =============================================================================
    // TcpBasedWorker 인터페이스 구현 (Driver 위임)
    // =============================================================================
    
    bool EstablishProtocolConnection() override;
    bool CloseProtocolConnection() override;
    bool CheckProtocolConnection() override;
    bool SendProtocolKeepAlive();
    PulseOne::Drivers::ModbusDriver* GetModbusDriver() const {
        return modbus_driver_.get();
    }
    
    // =============================================================================
    // Modbus TCP 특화 객체 관리
    // =============================================================================
    
    bool AddPollingGroup(const ModbusTcpPollingGroup& group);
    bool RemovePollingGroup(uint32_t group_id);
    std::vector<ModbusTcpPollingGroup> GetPollingGroups() const;
    bool SetPollingGroupEnabled(uint32_t group_id, bool enabled);
    std::string GetModbusStats() const;
    
    // =============================================================================
    // 운영용 쓰기/제어 함수들 (기존 구현 유지)
    // =============================================================================
    
    bool WriteSingleHoldingRegister(int slave_id, uint16_t address, uint16_t value);
    bool WriteSingleCoil(int slave_id, uint16_t address, bool value);
    bool WriteMultipleHoldingRegisters(int slave_id, uint16_t start_address, 
                                      const std::vector<uint16_t>& values);
    bool WriteMultipleCoils(int slave_id, uint16_t start_address,
                           const std::vector<bool>& values);

    // =============================================================================
    // 디버깅용 개별 읽기 함수들 (기존 구현 유지)
    // =============================================================================
    
     bool ReadSingleHoldingRegister(int slave_id, uint16_t address, uint16_t& value);
    bool ReadSingleInputRegister(int slave_id, uint16_t address, uint16_t& value);
    bool ReadSingleCoil(int slave_id, uint16_t address, bool& value);
    bool ReadSingleDiscreteInput(int slave_id, uint16_t address, bool& value);
    
    bool ReadHoldingRegisters(int slave_id, uint16_t start_address, uint16_t count, 
                             std::vector<uint16_t>& values);
    bool ReadInputRegisters(int slave_id, uint16_t start_address, uint16_t count,
                           std::vector<uint16_t>& values);
    bool ReadCoils(int slave_id, uint16_t start_address, uint16_t count,
                  std::vector<bool>& values);
    bool ReadDiscreteInputs(int slave_id, uint16_t start_address, uint16_t count,
                           std::vector<bool>& values);

    // =============================================================================
    // 고수준 제어 함수들 (DataPoint 기반)
    // =============================================================================
    
    bool WriteDataPointValue(const std::string& point_id, const DataValue& value);
    bool ReadDataPointValue(const std::string& point_id, TimestampedValue& value);
    bool ReadMultipleDataPoints(const std::vector<std::string>& point_ids,
                               std::vector<TimestampedValue>& values);

    // =============================================================================
    // 실시간 테스트/디버깅 함수들 (선택적 구현 완성)
    // =============================================================================
    
    /**
     * @brief 연결 테스트 (ping)
     * @param slave_id 테스트할 슬레이브 ID
     * @return 연결 성공 시 true
     */
    bool TestConnection(int slave_id = 1);
    
    /**
     * @brief 레지스터 스캔 (연속 주소 범위 테스트)
     * @param slave_id 슬레이브 ID
     * @param start_address 시작 주소
     * @param end_address 끝 주소
     * @param register_type 레지스터 타입
     * @return 스캔 결과 맵 (주소 -> 값)
     */
    std::map<uint16_t, uint16_t> ScanRegisters(int slave_id, uint16_t start_address, 
                                              uint16_t end_address, 
                                              const std::string& register_type = "holding");
    
    /**
     * @brief 디바이스 정보 읽기 (벤더 정보 등)
     * @param slave_id 슬레이브 ID
     * @return 디바이스 정보 JSON
     */
    std::string ReadDeviceInfo(int slave_id = 1);
    
    /**
     * @brief 레지스터 값들을 실시간으로 모니터링
     * @param slave_id 슬레이브 ID
     * @param addresses 모니터링할 주소 목록
     * @param register_type 레지스터 타입 ("holding", "input", "coil", "discrete")
     * @param duration_seconds 모니터링 지속 시간 (초)
     * @return 모니터링 결과 JSON
     */
    std::string MonitorRegisters(int slave_id, 
                                const std::vector<uint16_t>& addresses,
                                const std::string& register_type = "holding",
                                int duration_seconds = 10);
    
    /**
     * @brief Modbus 진단 정보 수집
     * @param slave_id 슬레이브 ID
     * @return 진단 결과 JSON
     */
    std::string RunDiagnostics(int slave_id = 1);
    
    std::string GetPropertyValue(const std::map<std::string, std::string>& properties, 
                            const std::string& key, 
                            const std::string& default_value) const {
        auto it = properties.find(key);
        return (it != properties.end()) ? it->second : default_value;
    }

protected:
    // =============================================================================
    // 데이터 포인트 처리
    // =============================================================================
    
    size_t CreatePollingGroupsFromDataPoints(const std::vector<PulseOne::DataPoint>& data_points);
    size_t OptimizePollingGroups();

private:
    // =============================================================================
    // Modbus TCP 전용 멤버 변수
    // =============================================================================
    
    std::unique_ptr<Drivers::ModbusDriver> modbus_driver_;
    std::map<uint32_t, ModbusTcpPollingGroup> polling_groups_;
    mutable std::mutex polling_groups_mutex_;
    std::unique_ptr<std::thread> polling_thread_;
    std::atomic<bool> polling_thread_running_;
    std::atomic<uint32_t> next_group_id_;
    
    uint32_t default_polling_interval_ms_;
    uint16_t max_registers_per_group_;
    bool auto_group_creation_enabled_;

    PulseOne::Drivers::DriverConfig modbus_config_;

    // =============================================================================
    // 내부 메서드
    // =============================================================================
    
    bool ParseModbusConfig();
    bool InitializeModbusDriver();
    void PollingThreadFunction();
    bool ProcessPollingGroup(const ModbusTcpPollingGroup& group);
    
    bool ParseModbusAddress(const PulseOne::DataPoint& data_point,
                           uint8_t& slave_id,
                           ModbusRegisterType& register_type,
                           uint16_t& address);
    
    bool ValidatePollingGroup(const ModbusTcpPollingGroup& group);
    bool CanMergePollingGroups(const ModbusTcpPollingGroup& group1,
                              const ModbusTcpPollingGroup& group2);
    ModbusTcpPollingGroup MergePollingGroups(const ModbusTcpPollingGroup& group1,
                                            const ModbusTcpPollingGroup& group2);

    // =============================================================================
    // ModbusDriver 콜백 메서드들
    // =============================================================================
    
    void SetupDriverCallbacks();
    static void OnConnectionStatusChanged(void* worker_ptr, bool connected,
                                         const std::string& error_message);
    static void OnModbusError(void* worker_ptr, uint8_t slave_id, uint8_t function_code,
                             int error_code, const std::string& error_message);
    static void OnStatisticsUpdate(void* worker_ptr, const std::string& operation,
                                  bool success, uint32_t response_time_ms);

    // =============================================================================
    // 파이프라인 전송 헬퍼들
    // =============================================================================
    
    bool SendModbusDataToPipeline(const std::vector<uint16_t>& raw_values, 
                                  uint16_t start_address,
                                  const std::string& register_type,
                                  uint32_t priority = 0);
    
    bool SendModbusBoolDataToPipeline(const std::vector<uint8_t>& raw_values,
                                      uint16_t start_address,
                                      const std::string& register_type,
                                      uint32_t priority = 0);
    
    void LogWriteOperation(int slave_id, uint16_t address, const DataValue& value,
                          const std::string& register_type, bool success);
    
    std::optional<DataPoint> FindDataPointById(const std::string& point_id);
    
    bool SendReadResultToPipeline(const std::vector<uint16_t>& values, uint16_t start_address,
                                 const std::string& register_type, int slave_id);
    bool SendReadResultToPipeline(const std::vector<bool>& values, uint16_t start_address,
                                 const std::string& register_type, int slave_id);
    bool SendSingleValueToPipeline(const DataValue& value, uint16_t address,
                                  const std::string& register_type, int slave_id);
    
    // =============================================================================
    // Write 인터페이스 구현을 위한 헬퍼 메서드들
    // =============================================================================
    
    /**
     * @brief 주소 문자열 파싱 ("slave:address" 또는 "address")
     * @param address_str 주소 문자열
     * @param slave_id 파싱된 슬레이브 ID (출력)
     * @param address 파싱된 주소 (출력)
     * @return 성공 시 true
     */
    bool ParseAddressString(const std::string& address_str, uint8_t& slave_id, uint16_t& address);
    
    /**
     * @brief Modbus 주소를 레지스터 타입으로 변환
     * @param address Modbus 주소 (1-based 또는 0-based)
     * @param register_type 출력 레지스터 타입
     * @param adjusted_address 조정된 주소 (0-based)
     * @return 성공 시 true
     */
    bool DetermineRegisterType(uint16_t address, ModbusRegisterType& register_type, uint16_t& adjusted_address);
};

} // namespace Workers
} // namespace PulseOne

#endif // MODBUS_TCP_WORKER_H