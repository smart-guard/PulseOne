// collector/include/Workers/Protocol/ModbusRtuWorker.h
#ifndef MODBUS_RTU_WORKER_H
#define MODBUS_RTU_WORKER_H

/**
 * @file ModbusRtuWorker.h - 실제 구조에 맞춘 최종 수정
 * @brief Modbus RTU 워커 클래스 헤더 (실제 타입과 맞춤)
 * @author PulseOne Development Team
 * @date 2025-08-06
 * 
 * 🔥 실제 문제 해결:
 * - 존재하지 않는 열거형 제거 (WorkerState, ModbusRegisterType 등)
 * - DriverConfig.properties 기반으로 수정
 * - 기존 구조 100% 유지하되 타입만 올바르게 수정
 */

// 기본 시스템 헤더들
#include <string>
#include <sstream>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <queue>
#include <future>

// PulseOne 헤더들 (기존과 동일)
#include "Workers/Base/SerialBasedWorker.h"
#include "Common/Structs.h"              // ✅ DriverConfig 포함

// ✅ 네임스페이스
namespace PulseOne {
namespace Workers {

// 🔥 실제 존재하는 타입들만 사용
using DeviceInfo = PulseOne::Structs::DeviceInfo;
using DataPoint = PulseOne::Structs::DataPoint;
// WorkerState는 BaseDeviceWorker에서 상속됨 (중복 정의 제거)

// 🔥 ModbusRegisterType만 정의 (실제 파일에 없으므로 여기서 정의)
enum class ModbusRegisterType : uint8_t {
    COIL = 0,              ///< 코일 (0x01, 0x05, 0x0F)
    DISCRETE_INPUT = 1,    ///< 접점 입력 (0x02)
    HOLDING_REGISTER = 2,  ///< 홀딩 레지스터 (0x03, 0x06, 0x10)
    INPUT_REGISTER = 3     ///< 입력 레지스터 (0x04)
};

/**
 * @brief Modbus RTU 폴링 그룹
 */
struct ModbusRtuPollingGroup {
    uint32_t group_id;                               ///< 그룹 ID
    std::string group_name;                          ///< 그룹 이름
    int slave_id;                                    ///< 슬레이브 ID
    ModbusRegisterType register_type;                ///< 레지스터 타입
    uint16_t start_address;                          ///< 시작 주소
    uint16_t register_count;                         ///< 레지스터 개수
    uint32_t polling_interval_ms;                    ///< 폴링 주기 (밀리초)
    bool enabled;                                    ///< 활성화 여부
    
    std::vector<DataPoint> data_points;              ///< 이 그룹에 속한 데이터 포인트들
    
    // 실행 시간 추적
    std::chrono::system_clock::time_point last_poll_time;
    std::chrono::system_clock::time_point next_poll_time;
    
    ModbusRtuPollingGroup() 
        : group_id(0), group_name(""), slave_id(1)
        , register_type(ModbusRegisterType::HOLDING_REGISTER)
        , start_address(0), register_count(1), polling_interval_ms(1000), enabled(true)
        , last_poll_time(std::chrono::system_clock::now())
        , next_poll_time(std::chrono::system_clock::now()) {}
};

/**
 * @brief Modbus RTU 워커 클래스
 * @details SerialBasedWorker 기반의 Modbus RTU 프로토콜 구현
 */
class ModbusRtuWorker : public SerialBasedWorker {
public:
    /**
     * @brief 생성자
     */
    explicit ModbusRtuWorker(const DeviceInfo& device_info);
    
    /**
     * @brief 소멸자
     */
    virtual ~ModbusRtuWorker();

    // =============================================================================
    // BaseDeviceWorker 인터페이스 구현
    // =============================================================================
    
    std::future<bool> Start() override;
    std::future<bool> Stop() override;
    WorkerState GetState() const override;

    // =============================================================================
    // SerialBasedWorker 인터페이스 구현 (RTU 특화)
    // =============================================================================
    
    bool EstablishProtocolConnection() override;
    bool CloseProtocolConnection() override;
    bool CheckProtocolConnection() override;
    bool SendProtocolKeepAlive() override;

    // =============================================================================
    // 설정 API
    // =============================================================================
    
    /**
     * @brief RTU 설정 구성
     */
    void ConfigureModbusRtu(const PulseOne::Structs::DriverConfig& config);
    
    /**
     * @brief 현재 설정 반환
     */
    const PulseOne::Structs::DriverConfig& GetModbusConfig() const { return modbus_config_; }
    
    // 편의 메서드들 (properties 기반)
    void SetSlaveId(int slave_id);
    void SetResponseTimeout(int timeout_ms);

    // =============================================================================
    // RTU 특화 슬레이브 관리
    // =============================================================================
    
    bool AddSlave(int slave_id, const std::string& device_name = "");
    bool RemoveSlave(int slave_id);
    std::shared_ptr<DeviceInfo> GetSlaveInfo(int slave_id) const;
    int ScanSlaves(int start_id = 1, int end_id = 247, int timeout_ms = 2000);

    // =============================================================================
    // 폴링 그룹 관리
    // =============================================================================
    
    uint32_t AddPollingGroup(const std::string& group_name,
                            int slave_id,
                            ModbusRegisterType register_type,
                            uint16_t start_address,
                            uint16_t register_count,
                            int polling_interval_ms = 1000);
    
    bool RemovePollingGroup(uint32_t group_id);
    bool EnablePollingGroup(uint32_t group_id, bool enabled);
    bool AddDataPointToGroup(uint32_t group_id, const DataPoint& data_point);

    // =============================================================================
    // 데이터 읽기/쓰기
    // =============================================================================
    
    bool ReadHoldingRegisters(int slave_id, uint16_t start_address, 
                             uint16_t register_count, std::vector<uint16_t>& values);
    bool ReadInputRegisters(int slave_id, uint16_t start_address, 
                           uint16_t register_count, std::vector<uint16_t>& values);
    bool ReadCoils(int slave_id, uint16_t start_address, 
                   uint16_t coil_count, std::vector<bool>& values);
    bool ReadDiscreteInputs(int slave_id, uint16_t start_address, 
                           uint16_t input_count, std::vector<bool>& values);
    bool WriteSingleRegister(int slave_id, uint16_t address, uint16_t value);
    bool WriteSingleCoil(int slave_id, uint16_t address, bool value);
    bool WriteMultipleRegisters(int slave_id, uint16_t start_address, 
                               const std::vector<uint16_t>& values);
    bool WriteMultipleCoils(int slave_id, uint16_t start_address, 
                           const std::vector<bool>& values);

    // =============================================================================
    // 상태 조회 API
    // =============================================================================
    
    std::string GetModbusStats() const;
    std::string GetSerialBusStatus() const;
    std::string GetSlaveStatusList() const;
    std::string GetPollingGroupStatus() const;

    // 🔥 properties 기반 접근자들
    int GetSlaveId() const;
    int GetBaudRate() const;
    char GetParity() const;
    int GetDataBits() const;
    int GetStopBits() const;
    int GetFrameDelay() const;
    int GetResponseTimeout() const;
    int GetByteTimeout() const;
    int GetMaxRetries() const;    

protected:
    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    // 🔥 ModbusDriver는 실제로 존재하지 않을 수 있으므로 주석 처리
    // std::shared_ptr<PulseOne::Drivers::ModbusDriver> modbus_driver_;
    
    // 🔥 핵심: DriverConfig 사용
    PulseOne::Structs::DriverConfig modbus_config_;   ///< 설정 저장
    
    // 기존 멤버 변수들
    std::atomic<uint32_t> next_group_id_;
    std::atomic<bool> polling_thread_running_;
    std::unique_ptr<std::thread> polling_thread_;
    
    // 시리얼 버스 액세스 제어 (RTU 고유)
    mutable std::mutex bus_mutex_;
    
    // RTU 특화: 슬레이브 관리
    std::map<int, std::shared_ptr<DeviceInfo>> slaves_;
    mutable std::shared_mutex slaves_mutex_;
    
    // 폴링 그룹 관리
    std::map<uint32_t, ModbusRtuPollingGroup> polling_groups_;
    mutable std::shared_mutex polling_groups_mutex_;

    // =============================================================================
    // 헬퍼 메서드들
    // =============================================================================
    
    void PollingWorkerThread();
    bool ProcessPollingGroup(ModbusRtuPollingGroup& group);
    void UpdateSlaveStatus(int slave_id, int response_time_ms, bool success);
    int CheckSlaveStatus(int slave_id);
    
    void LockBus();    // RTU 고유
    void UnlockBus();  // RTU 고유
    
    std::vector<DataPoint> CreateDataPoints(int slave_id, 
                                           ModbusRegisterType register_type,
                                           uint16_t start_address, 
                                           uint16_t count);

private:
    // =============================================================================
    // 설정 및 초기화 메서드들
    // =============================================================================
    bool ParseModbusConfig();
    bool InitializeModbusDriver();
    void SetupDriverCallbacks();
    
    // =============================================================================
    // 헬퍼 메서드 (const로 수정)
    // =============================================================================
    std::string GetPropertyValue(const std::map<std::string, std::string>& properties, 
                               const std::string& key, 
                               const std::string& default_value = "") const;
};

// =============================================================================
// ✅ 인라인 구현 (properties 기반)
// =============================================================================

inline void ModbusRtuWorker::SetSlaveId(int slave_id) {
    modbus_config_.properties["slave_id"] = std::to_string(slave_id);
}

inline void ModbusRtuWorker::SetResponseTimeout(int timeout_ms) {
    modbus_config_.properties["response_timeout_ms"] = std::to_string(timeout_ms);
}

// properties 기반 접근자들
inline int ModbusRtuWorker::GetSlaveId() const {
    return std::stoi(GetPropertyValue(modbus_config_.properties, "slave_id", "1"));
}

inline int ModbusRtuWorker::GetBaudRate() const {
    return std::stoi(GetPropertyValue(modbus_config_.properties, "baud_rate", "9600"));
}

inline char ModbusRtuWorker::GetParity() const {
    std::string parity = GetPropertyValue(modbus_config_.properties, "parity", "N");
    return parity.empty() ? 'N' : parity[0];
}

inline int ModbusRtuWorker::GetDataBits() const {
    return std::stoi(GetPropertyValue(modbus_config_.properties, "data_bits", "8"));
}

inline int ModbusRtuWorker::GetStopBits() const {
    return std::stoi(GetPropertyValue(modbus_config_.properties, "stop_bits", "1"));
}

inline int ModbusRtuWorker::GetFrameDelay() const {
    return std::stoi(GetPropertyValue(modbus_config_.properties, "frame_delay_ms", "50"));
}

inline int ModbusRtuWorker::GetResponseTimeout() const {
    return std::stoi(GetPropertyValue(modbus_config_.properties, "response_timeout_ms", "1000"));
}

inline int ModbusRtuWorker::GetByteTimeout() const {
    return std::stoi(GetPropertyValue(modbus_config_.properties, "byte_timeout_ms", "100"));
}

inline int ModbusRtuWorker::GetMaxRetries() const {
    return std::stoi(GetPropertyValue(modbus_config_.properties, "max_retries", "3"));
}

} // namespace Workers  
} // namespace PulseOne

#endif // MODBUS_RTU_WORKER_H