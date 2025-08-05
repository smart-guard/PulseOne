/**
 * @file ModbusRtuWorker.h - 네임스페이스 완전 수정 버전
 * @brief Modbus RTU 워커 클래스 헤더 (네임스페이스 중첩 문제 해결)
 * @author PulseOne Development Team
 * @date 2025-08-01
 * @version 4.0.0
 * 
 * 🔥 해결된 문제:
 * - 네임스페이스 중첩 문제 완전 해결
 * - PulseOne::Workers 네임스페이스로 통일
 * - std 타입들 올바른 선언
 */

#ifndef MODBUS_RTU_WORKER_H
#define MODBUS_RTU_WORKER_H

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

// PulseOne 헤더들
#include "Workers/Base/SerialBasedWorker.h"
#include "Drivers/Modbus/ModbusDriver.h"
#include "Drivers/Modbus/ModbusConfig.h"

// ✅ 올바른 네임스페이스 - 중첩 없음
namespace PulseOne {
namespace Workers {

/**
 * @brief Modbus 레지스터 타입 (RTU Worker에서 사용)
 */
enum class ModbusRegisterType {
    COIL = 0,              ///< 코일 (0x01, 0x05, 0x0F)
    DISCRETE_INPUT = 1,    ///< 접점 입력 (0x02)
    HOLDING_REGISTER = 2,  ///< 홀딩 레지스터 (0x03, 0x06, 0x10)
    INPUT_REGISTER = 3     ///< 입력 레지스터 (0x04)
};

/**
 * @brief Modbus RTU 슬레이브 정보 (RTU 특화)
 */
struct DeviceInfo {
    int slave_id;                                    ///< 슬레이브 ID
    std::string device_name;                         ///< 디바이스 이름
    bool is_online;                                  ///< 온라인 상태
    std::atomic<uint32_t> response_time_ms;          ///< 평균 응답 시간
    std::chrono::system_clock::time_point last_response;  ///< 마지막 응답 시간
    
    // 통계
    std::atomic<uint32_t> total_requests;            ///< 총 요청 수
    std::atomic<uint32_t> successful_requests;       ///< 성공한 요청 수
    std::atomic<uint32_t> timeout_errors;            ///< 타임아웃 에러 수
    std::atomic<uint32_t> crc_errors;                ///< CRC 에러 수
    std::string last_error;                          ///< 마지막 에러 메시지
    
    DeviceInfo(int id = 1, const std::string& name = "")
        : slave_id(id), device_name(name), is_online(false)
        , response_time_ms(0), total_requests(0), successful_requests(0)
        , timeout_errors(0)
        , last_response(std::chrono::system_clock::now())
        , crc_errors(0) {}
};

/**
 * @brief Modbus RTU 폴링 그룹 (TCP와 유사한 구조)
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
    
    std::vector<PulseOne::DataPoint> data_points;    ///< 이 그룹에 속한 데이터 포인트들
    
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
 * @brief Modbus RTU 워커 클래스 (TCP와 완전 일관성)
 * @details SerialBasedWorker 기반의 Modbus RTU 프로토콜 구현
 */
class ModbusRtuWorker : public SerialBasedWorker {
public:
    /**
     * @brief 생성자
     */
    explicit ModbusRtuWorker(
        const PulseOne::DeviceInfo& device_info,
        std::shared_ptr<RedisClient> redis_client = nullptr,
        std::shared_ptr<InfluxClient> influx_client = nullptr
    );
    
    /**
     * @brief 소멸자
     */
    virtual ~ModbusRtuWorker();

    // =============================================================================
    // BaseDeviceWorker 인터페이스 구현 (TCP와 동일)
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
    // 통합된 설정 API (TCP와 일관성)
    // =============================================================================
    
    void ConfigureModbusRtu(const PulseOne::Drivers::ModbusConfig& config);
    const PulseOne::Drivers::ModbusConfig& GetModbusConfig() const { return modbus_config_; }
    void SetSlaveId(int slave_id) { modbus_config_.slave_id = slave_id; }
    void SetResponseTimeout(int timeout_ms) { modbus_config_.response_timeout_ms = timeout_ms; }

    // =============================================================================
    // RTU 특화 슬레이브 관리 (TCP에는 없는 RTU 고유 기능)
    // =============================================================================
    
    bool AddSlave(int slave_id, const std::string& device_name = "");
    bool RemoveSlave(int slave_id);
    std::shared_ptr<DeviceInfo> GetSlaveInfo(int slave_id) const;
    int ScanSlaves(int start_id = 1, int end_id = 247, int timeout_ms = 2000);

    // =============================================================================
    // 폴링 그룹 관리 (TCP와 동일한 패턴)
    // =============================================================================
    
    uint32_t AddPollingGroup(const std::string& group_name,
                            int slave_id,
                            ModbusRegisterType register_type,
                            uint16_t start_address,
                            uint16_t register_count,
                            int polling_interval_ms = 1000);
    
    bool RemovePollingGroup(uint32_t group_id);
    bool EnablePollingGroup(uint32_t group_id, bool enabled);
    bool AddDataPointToGroup(uint32_t group_id, const PulseOne::DataPoint& data_point);

    // =============================================================================
    // 데이터 읽기/쓰기 (TCP와 동일한 패턴)
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
    // 상태 조회 API (TCP와 완전 일관성)
    // =============================================================================
    
    std::string GetModbusStats() const;
    std::string GetSerialBusStatus() const;
    std::string GetSlaveStatusList() const;
    std::string GetPollingGroupStatus() const;

protected:
    // =============================================================================
    // 멤버 변수들 (TCP와 일관성 통일)
    // =============================================================================
    
    // ModbusDriver 인스턴스 (TCP와 동일)
    std::unique_ptr<PulseOne::Drivers::ModbusDriver> modbus_driver_;
    
    // 통합된 설정 (TCP와 동일)
    PulseOne::Drivers::ModbusConfig modbus_config_;
    
    // 시리얼 버스 액세스 제어 (RTU 고유)
    mutable std::mutex bus_mutex_;
    
    // RTU 특화: 슬레이브 관리 (TCP에는 없음)
    std::map<int, std::shared_ptr<DeviceInfo>> slaves_;
    mutable std::shared_mutex slaves_mutex_;
    
    // 폴링 그룹 관리 (TCP와 동일한 패턴)
    std::map<uint32_t, ModbusRtuPollingGroup> polling_groups_;
    mutable std::shared_mutex polling_groups_mutex_;
    uint32_t next_group_id_;
    
    // 폴링 워커 스레드 (TCP와 완전 일관성)
    std::unique_ptr<std::thread> polling_thread_;
    std::atomic<bool> polling_thread_running_;

    // =============================================================================
    // 헬퍼 메서드들 (TCP와 일관성)
    // =============================================================================
    
    void PollingWorkerThread();
    bool ProcessPollingGroup(ModbusRtuPollingGroup& group);
    void UpdateSlaveStatus(int slave_id, int response_time_ms, bool success);
    int CheckSlaveStatus(int slave_id);
    
    void LockBus();    // RTU 고유
    void UnlockBus();  // RTU 고유
    void LogRtuMessage(PulseOne::LogLevel level, const std::string& message);
    std::vector<PulseOne::Structs::DataPoint> CreateDataPoints(int slave_id, 
                                                    ModbusRegisterType register_type,
                                                    uint16_t start_address, 
                                                    uint16_t count);

private:
    // =============================================================================
    // 설정 및 초기화 메서드들 (TCP와 동일한 패턴)
    // =============================================================================
    bool ParseModbusConfig();
    bool InitializeModbusDriver();
    void SetupDriverCallbacks();
};

} // namespace Workers  
} // namespace PulseOne

#endif // MODBUS_RTU_WORKER_H