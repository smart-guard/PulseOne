/**
 * @file ModbusTcpWorker.h - 구현부와 완전 일치하는 헤더파일
 * @brief Modbus TCP 워커 클래스 - TcpBasedWorker 상속 + 모든 구현 메서드 선언
 * @author PulseOne Development Team
 * @date 2025-08-30
 * @version 8.0.1 - 구현부 완전 동기화 버전
 */

#ifndef MODBUS_TCP_WORKER_H
#define MODBUS_TCP_WORKER_H

// 시스템 헤더들
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <future>
#include <optional>
#include <queue>

// PulseOne 헤더들 - 구현부와 정확히 일치
#include "Workers/Base/TcpBasedWorker.h"  // BaseDeviceWorker가 아닌 TcpBasedWorker!
#include "Common/Structs.h"
#include "Common/Enums.h"
#include "Drivers/Modbus/ModbusDriver.h"

namespace PulseOne {
namespace Workers {

// =============================================================================
// 타입 별칭들 - 구현부와 완전 일치
// =============================================================================
using DeviceInfo = PulseOne::Structs::DeviceInfo;
using DataPoint = PulseOne::Structs::DataPoint;
using DataValue = PulseOne::Structs::DataValue;
using TimestampedValue = PulseOne::Structs::TimestampedValue;
using ModbusRegisterType = PulseOne::Enums::ModbusRegisterType;
using DataQuality = PulseOne::Enums::DataQuality;

// WorkerState는 BaseDeviceWorker.h에서 정의됨 (TcpBasedWorker를 통해 상속)

// =============================================================================
// 🔥 구현부에 있는 ModbusTcpPollingGroup 구조체 선언
// =============================================================================

/**
 * @brief Modbus TCP 폴링 그룹
 */
struct ModbusTcpPollingGroup {
    uint32_t group_id;                               ///< 그룹 ID
    uint8_t slave_id;                                ///< 슬레이브 ID
    ModbusRegisterType register_type;                ///< 레지스터 타입
    uint16_t start_address;                          ///< 시작 주소
    uint16_t register_count;                         ///< 레지스터 개수
    uint32_t polling_interval_ms;                    ///< 폴링 간격 (ms)
    bool enabled;                                    ///< 활성화 상태
    
    std::vector<DataPoint> data_points;              ///< 연관된 데이터 포인트들
    
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

// =============================================================================
// ModbusTcpWorker 클래스 정의 - 구현부와 완전 일치
// =============================================================================

/**
 * @brief Modbus TCP 워커 클래스 (구현부 완전 동기화)
 * @details TcpBasedWorker를 상속받아 Modbus TCP 프로토콜 구현
 */
class ModbusTcpWorker : public TcpBasedWorker {
public:
    // =============================================================================
    // 생성자 및 소멸자
    // =============================================================================
    explicit ModbusTcpWorker(const DeviceInfo& device_info);
    virtual ~ModbusTcpWorker();

    // =============================================================================
    // BaseDeviceWorker 인터페이스 구현 (TcpBasedWorker를 통해 상속됨)
    // =============================================================================
    std::future<bool> Start() override;
    std::future<bool> Stop() override;

    // =============================================================================
    // TcpBasedWorker 추상 메서드 구현
    // =============================================================================
    bool EstablishProtocolConnection() override;
    bool CloseProtocolConnection() override;
    bool CheckProtocolConnection() override;
    bool SendProtocolKeepAlive();
    // SendProtocolKeepAlive()는 TcpBasedWorker에서 기본 구현 제공 (override 불필요)

    // =============================================================================
    // 🔥 BaseDeviceWorker Write 인터페이스 구현 (구현부와 완전 일치)
    // =============================================================================
    
    /**
     * @brief 데이터 포인트에 값 쓰기
     */
    virtual bool WriteDataPoint(const std::string& point_id, const DataValue& value) override;
    
    /**
     * @brief 아날로그 출력 제어
     */
    virtual bool WriteAnalogOutput(const std::string& output_id, double value) override;
    
    /**
     * @brief 디지털 출력 제어
     */
    virtual bool WriteDigitalOutput(const std::string& output_id, bool value) override;
    
    /**
     * @brief 세트포인트 설정
     */
    virtual bool WriteSetpoint(const std::string& setpoint_id, double value) override;
    
    /**
     * @brief 디지털 장비 제어 (펌프, 팬, 모터 등)
     */
    virtual bool ControlDigitalDevice(const std::string& device_id, bool enable) override;
    
    /**
     * @brief 아날로그 장비 제어 (밸브, 댐퍼 등)
     */
    virtual bool ControlAnalogDevice(const std::string& device_id, double value) override;

    // =============================================================================
    // 🔥 폴링 그룹 관리 (구현부에 있는 모든 메서드들)
    // =============================================================================
    
    /**
     * @brief 폴링 그룹 추가
     */
    bool AddPollingGroup(const ModbusTcpPollingGroup& group);
    
    /**
     * @brief 폴링 그룹 제거
     */
    bool RemovePollingGroup(uint32_t group_id);
    
    /**
     * @brief 폴링 그룹 목록 조회
     */
    std::vector<ModbusTcpPollingGroup> GetPollingGroups() const;
    
    /**
     * @brief 폴링 그룹 활성화/비활성화
     */
    bool SetPollingGroupEnabled(uint32_t group_id, bool enabled);

    // =============================================================================
    // 🔥 진단 및 테스트 메서드들 (구현부에 있는 모든 메서드들)
    // =============================================================================
    
    /**
     * @brief 연결 테스트
     */
    bool TestConnection(int slave_id = 1);
    
    /**
     * @brief 레지스터 스캔
     */
    std::map<uint16_t, uint16_t> ScanRegisters(int slave_id, uint16_t start_address, 
                                              uint16_t end_address, 
                                              const std::string& register_type = "holding");
    
    /**
     * @brief 디바이스 정보 읽기
     */
    std::string ReadDeviceInfo(int slave_id = 1);
    
    /**
     * @brief 레지스터 모니터링
     */
    std::string MonitorRegisters(int slave_id, 
                                const std::vector<uint16_t>& addresses,
                                const std::string& register_type = "holding",
                                int duration_seconds = 10);
    
    /**
     * @brief 진단 실행
     */
    std::string RunDiagnostics(int slave_id = 1);
    
    /**
     * @brief Modbus 통계 조회
     */
    std::string GetModbusStats() const;

    // =============================================================================
    // 🔥 Modbus 읽기/쓰기 메서드들 (구현부에 있는 모든 메서드들)
    // =============================================================================
    
    // 단일 쓰기 메서드들
    bool WriteSingleHoldingRegister(int slave_id, uint16_t address, uint16_t value);
    bool WriteSingleCoil(int slave_id, uint16_t address, bool value);
    
    // 다중 쓰기 메서드들
    bool WriteMultipleHoldingRegisters(int slave_id, uint16_t start_address, 
                                      const std::vector<uint16_t>& values);
    bool WriteMultipleCoils(int slave_id, uint16_t start_address, 
                           const std::vector<bool>& values);
    
    // 단일 읽기 메서드들
    bool ReadSingleHoldingRegister(int slave_id, uint16_t address, uint16_t& value);
    bool ReadSingleInputRegister(int slave_id, uint16_t address, uint16_t& value);
    bool ReadSingleCoil(int slave_id, uint16_t address, bool& value);
    bool ReadSingleDiscreteInput(int slave_id, uint16_t address, bool& value);
    
    // 다중 읽기 메서드들
    bool ReadHoldingRegisters(int slave_id, uint16_t start_address, uint16_t count, 
                             std::vector<uint16_t>& values);
    bool ReadInputRegisters(int slave_id, uint16_t start_address, uint16_t count,
                           std::vector<uint16_t>& values);
    bool ReadCoils(int slave_id, uint16_t start_address, uint16_t count,
                  std::vector<bool>& values);
    bool ReadDiscreteInputs(int slave_id, uint16_t start_address, uint16_t count,
                           std::vector<bool>& values);

    // =============================================================================
    // 🔥 고수준 DataPoint 메서드들 (구현부에 있는 모든 메서드들)
    // =============================================================================
    
    /**
     * @brief DataPoint 기반 값 쓰기
     */
    bool WriteDataPointValue(const std::string& point_id, const DataValue& value);
    
    /**
     * @brief DataPoint 기반 값 읽기
     */
    bool ReadDataPointValue(const std::string& point_id, TimestampedValue& value);
    
    /**
     * @brief 다중 DataPoint 읽기
     */
    bool ReadMultipleDataPoints(const std::vector<std::string>& point_ids,
                               std::vector<TimestampedValue>& values);

    // =============================================================================
    // 🔥 테스트용 Getter 메서드들 (구현부와 완전 일치)
    // =============================================================================
    
    /**
     * @brief 슬레이브 ID 조회
     */
    int GetSlaveId() const;
    
    /**
     * @brief IP 주소 조회
     */
    std::string GetIpAddress() const;
    
    /**
     * @brief 포트 조회
     */
    int GetPort() const;
    
    /**
     * @brief 연결 타임아웃 조회
     */
    int GetConnectionTimeout() const;
    
    /**
     * @brief Keep Alive 설정 조회
     */
    bool GetKeepAlive() const;
    
    /**
     * @brief 응답 타임아웃 조회
     */
    int GetResponseTimeout() const;
    
    /**
     * @brief 바이트 타임아웃 조회
     */
    int GetByteTimeout() const;
    
    /**
     * @brief 최대 재시도 횟수 조회
     */
    int GetMaxRetries() const;
    
    /**
     * @brief 연결 상태 확인
     */
    bool IsConnected() const;
    
    /**
     * @brief 연결 상태 문자열
     */
    std::string GetConnectionStatus() const;
    
    /**
     * @brief 엔드포인트 조회
     */
    std::string GetEndpoint() const;
    
    /**
     * @brief 디바이스 이름 조회
     */
    std::string GetDeviceName() const;
    
    /**
     * @brief 디바이스 ID 조회
     */
    std::string GetDeviceId() const;
    
    /**
     * @brief 디바이스 활성화 상태 조회
     */
    bool IsDeviceEnabled() const;
    
    /**
     * @brief TCP 연결 상태 조회
     */
    std::string GetTcpConnectionStatus() const;

    // =============================================================================
    // 유틸리티 메서드들
    // =============================================================================
    
    /**
     * @brief ModbusDriver 인스턴스 접근
     */
    PulseOne::Drivers::ModbusDriver* GetModbusDriver() const {
        return modbus_driver_.get();
    }
    
    /**
     * @brief 편의 래퍼 - 펌프 제어
     */
    inline bool ControlPump(const std::string& pump_id, bool enable) {
        return ControlDigitalDevice(pump_id, enable);
    }
    
    /**
     * @brief 편의 래퍼 - 팬 제어
     */
    inline bool ControlFan(const std::string& fan_id, bool enable) {
        return ControlDigitalDevice(fan_id, enable);
    }
    
    /**
     * @brief 편의 래퍼 - 모터 제어
     */
    inline bool ControlMotor(const std::string& motor_id, bool enable) {
        return ControlDigitalDevice(motor_id, enable);
    }
    
    /**
     * @brief 편의 래퍼 - 밸브 제어
     */
    inline bool ControlValve(const std::string& valve_id, double position) {
        return ControlAnalogDevice(valve_id, position);
    }
    
    /**
     * @brief 편의 래퍼 - 댐퍼 제어
     */
    inline bool ControlDamper(const std::string& damper_id, double position) {
        return ControlAnalogDevice(damper_id, position);
    }

private:
    // =============================================================================
    // 🔥 내부 멤버 변수들 (구현부와 완전 일치)
    // =============================================================================
    
    // ModbusDriver 인스턴스
    std::unique_ptr<PulseOne::Drivers::ModbusDriver> modbus_driver_;
    
    // 설정 및 상태
    PulseOne::Structs::DriverConfig modbus_config_;
    
    // 🔥 구현부에 있는 폴링 관련 멤버 변수들
    std::atomic<bool> polling_thread_running_;
    std::unique_ptr<std::thread> polling_thread_;
    uint32_t next_group_id_;
    uint32_t default_polling_interval_ms_;
    uint32_t max_registers_per_group_;
    bool auto_group_creation_enabled_;
    
    // 폴링 그룹 관리
    std::map<uint32_t, ModbusTcpPollingGroup> polling_groups_;
    mutable std::mutex polling_groups_mutex_;

    // =============================================================================
    // 🔥 내부 메서드들 (구현부에 있는 모든 메서드들 선언)
    // =============================================================================
    
    // 설정 및 초기화
    bool ParseModbusConfig();
    bool InitializeModbusDriver();
    void SetupDriverCallbacks();
    
    // 폴링 관련 메서드들
    void PollingThreadFunction();
    bool ProcessPollingGroup(const ModbusTcpPollingGroup& group);
    size_t CreatePollingGroupsFromDataPoints(const std::vector<DataPoint>& data_points);
    size_t OptimizePollingGroups();
    
    // 주소 파싱 메서드들
    bool ParseAddressString(const std::string& address_str, uint8_t& slave_id, uint16_t& address);
    bool DetermineRegisterType(uint16_t address, ModbusRegisterType& register_type, uint16_t& adjusted_address);
    bool ParseModbusAddress(const DataPoint& data_point, uint8_t& slave_id, 
                           ModbusRegisterType& register_type, uint16_t& address);
    
    // 검증 메서드들
    bool ValidatePollingGroup(const ModbusTcpPollingGroup& group);
    bool CanMergePollingGroups(const ModbusTcpPollingGroup& group1, const ModbusTcpPollingGroup& group2);
    ModbusTcpPollingGroup MergePollingGroups(const ModbusTcpPollingGroup& group1, const ModbusTcpPollingGroup& group2);
    
    // 콜백 메서드들
    static void OnConnectionStatusChanged(void* worker_ptr, bool connected, const std::string& error_message);
    static void OnModbusError(void* worker_ptr, uint8_t slave_id, uint8_t function_code,
                             int error_code, const std::string& error_message);
    static void OnStatisticsUpdate(void* worker_ptr, const std::string& operation,
                                  bool success, uint32_t response_time_ms);
    
    // 유틸리티 메서드들
    std::string GetPropertyValue(const std::map<std::string, std::string>& properties, 
                               const std::string& key, 
                               const std::string& default_value = "") const;
    
    std::optional<DataPoint> FindDataPointById(const std::string& point_id);
    void LogWriteOperation(int slave_id, uint16_t address, const DataValue& value,
                          const std::string& register_type, bool success);
    
    // 파이프라인 전송 메서드들
    bool SendReadResultToPipeline(const std::vector<bool>& values, uint16_t start_address,
                                 const std::string& register_type, int slave_id);
    bool SendSingleValueToPipeline(const DataValue& value, uint16_t address,
                                  const std::string& register_type, int slave_id);
};

} // namespace Workers
} // namespace PulseOne

#endif // MODBUS_TCP_WORKER_H