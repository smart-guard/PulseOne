/**
 * @file ModbusRtuWorker.h - 완전히 정리된 최종 버전
 * @brief Modbus RTU 워커 클래스 (중복 완전 제거)
 */

#ifndef WORKERS_PROTOCOL_MODBUS_RTU_WORKER_H
#define WORKERS_PROTOCOL_MODBUS_RTU_WORKER_H

#include "Workers/Base/SerialBasedWorker.h"
#include "Drivers/Modbus/ModbusDriver.h"
#include "Drivers/Modbus/ModbusConfig.h"
#include <memory>
#include <queue>
#include <thread>
#include <atomic>
#include <map>
#include <chrono>
#include <shared_mutex>
#include <vector>

namespace PulseOne {
namespace Workers {
namespace Protocol {

/**
 * @brief RTU 통계 구조체 (헤더 파일에 추가 필요)
 * ModbusRtuWorker.h 에 추가해야 하는 구조체
 */

/**
 * @brief RTU 통계 구조체 (모든 통계 통합)
 */
struct ModbusRtuStats {
    std::atomic<uint64_t> total_reads{0};
    std::atomic<uint64_t> successful_reads{0};
    std::atomic<uint64_t> total_writes{0};
    std::atomic<uint64_t> successful_writes{0};
    std::atomic<uint64_t> crc_errors{0};
    std::atomic<uint64_t> timeout_errors{0};
    std::atomic<uint64_t> frame_errors{0};
    
    // 성공률 계산
    double GetReadSuccessRate() const {
        uint64_t total = total_reads.load();
        return total > 0 ? (static_cast<double>(successful_reads.load()) / total * 100.0) : 0.0;
    }
    
    double GetWriteSuccessRate() const {
        uint64_t total = total_writes.load();
        return total > 0 ? (static_cast<double>(successful_writes.load()) / total * 100.0) : 0.0;
    }
    
    // JSON 출력
    std::string ToJson() const {
        std::stringstream ss;
        ss << "{\n";
        ss << "  \"total_reads\": " << total_reads.load() << ",\n";
        ss << "  \"successful_reads\": " << successful_reads.load() << ",\n";
        ss << "  \"total_writes\": " << total_writes.load() << ",\n";
        ss << "  \"successful_writes\": " << successful_writes.load() << ",\n";
        ss << "  \"crc_errors\": " << crc_errors.load() << ",\n";
        ss << "  \"timeout_errors\": " << timeout_errors.load() << ",\n";
        ss << "  \"frame_errors\": " << frame_errors.load() << ",\n";
        ss << "  \"read_success_rate_percent\": " << std::fixed << std::setprecision(2) << GetReadSuccessRate() << ",\n";
        ss << "  \"write_success_rate_percent\": " << std::fixed << std::setprecision(2) << GetWriteSuccessRate() << "\n";
        ss << "}";
        return ss.str();
    }
};

// 헤더 파일의 protected 섹션에 추가:
// ModbusRtuStats rtu_stats_;               ///< ✅ 통합된 통계
// mutable std::mutex stats_mutex_;         ///< 통계 보호용 뮤텍스
/**
 * @brief Modbus RTU 워커 클래스 (완전 정리된 버전)
 */
class ModbusRtuWorker : public SerialBasedWorker {
public:
    explicit ModbusRtuWorker(
        const PulseOne::DeviceInfo& device_info,
        std::shared_ptr<RedisClient> redis_client = nullptr,
        std::shared_ptr<InfluxClient> influx_client = nullptr
    );
    
    virtual ~ModbusRtuWorker();

    // =============================================================================
    // BaseDeviceWorker 인터페이스 구현
    // =============================================================================
    
    std::future<bool> Start() override;
    std::future<bool> Stop() override;
    WorkerState GetState() const override;

    // =============================================================================
    // SerialBasedWorker 인터페이스 구현
    // =============================================================================
    
    bool EstablishProtocolConnection() override;
    bool CloseProtocolConnection() override;
    bool CheckProtocolConnection() override;
    bool SendProtocolKeepAlive() override;

    // =============================================================================
    // ✅ 통합된 설정 API (중복 제거)
    // =============================================================================
    
    /**
     * @brief Modbus RTU 전체 설정 (한 번에 모든 설정)
     * @param config Modbus RTU 설정 구조체
     */
    void ConfigureModbusRtu(const PulseOne::Drivers::ModbusConfig& config);
    
    /**
     * @brief 현재 Modbus 설정 조회
     * @return 현재 설정
     */
    const PulseOne::Drivers::ModbusConfig& GetModbusConfig() const { return modbus_config_; }
    
    /**
     * @brief 슬레이브 ID 설정 (빠른 접근)
     * @param slave_id 슬레이브 ID
     */
    void SetSlaveId(int slave_id) { modbus_config_.slave_id = slave_id; }
    
    /**
     * @brief 응답 타임아웃 설정 (빠른 접근)
     * @param timeout_ms 타임아웃 (밀리초)
     */
    void SetResponseTimeout(int timeout_ms) { modbus_config_.response_timeout_ms = timeout_ms; }

    // =============================================================================
    // 슬레이브 관리 (기존과 동일)
    // =============================================================================
    
    bool AddSlave(int slave_id, const std::string& device_name = "");
    bool RemoveSlave(int slave_id);
    std::shared_ptr<ModbusRtuSlaveInfo> GetSlaveInfo(int slave_id) const;
    int ScanSlaves(int start_id = 1, int end_id = 247, int timeout_ms = 2000);

    // =============================================================================
    // 폴링 그룹 관리 (기존과 동일)
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
    // 데이터 읽기/쓰기 (기존과 동일)
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
    // ✅ 간소화된 상태 조회 API
    // =============================================================================
    
    /**
     * @brief RTU 통계 조회 (구조체 반환)
     * @return RTU 통계 구조체
     */
    const ModbusRtuStats& GetRtuStats() const { return rtu_stats_; }
    
    /**
     * @brief RTU 통계 JSON 조회
     * @return JSON 형태의 통계
     */
    std::string GetModbusRtuStats() const { return rtu_stats_.ToJson(); }
    
    /**
     * @brief 시리얼 버스 상태 조회 (구조체 기반)
     * @return JSON 형태의 버스 상태
     */
    std::string GetSerialBusStatus() const;
    
    std::string GetSlaveStatusList() const;
    std::string GetPollingGroupStatus() const;

protected:
    // =============================================================================
    // ✅ 정리된 멤버 변수들 (중복 완전 제거)
    // =============================================================================
    
    // ModbusDriver 인스턴스
    std::unique_ptr<Drivers::ModbusDriver> modbus_driver_;
    
    // ✅ 통합된 설정 (개별 변수들 모두 제거)
    PulseOne::Drivers::ModbusConfig modbus_config_;
    
    // ✅ 통합된 통계 (개별 변수들 모두 제거)
    mutable ModbusRtuStats rtu_stats_;
    mutable std::mutex stats_mutex_;
    
    // 시리얼 버스 액세스 제어
    mutable std::mutex bus_mutex_;
    
    // 슬레이브 관리
    std::map<int, std::shared_ptr<ModbusRtuSlaveInfo>> slaves_;
    mutable std::shared_mutex slaves_mutex_;
    
    // 폴링 그룹 관리
    std::map<uint32_t, ModbusRtuPollingGroup> polling_groups_;
    mutable std::shared_mutex polling_groups_mutex_;
    uint32_t next_group_id_;
    
    // 폴링 워커 스레드
    std::thread polling_thread_;
    std::atomic<bool> stop_workers_;

    // =============================================================================
    // ✅ 간소화된 헬퍼 메서드들
    // =============================================================================
    
    void PollingWorkerThread();
    bool ProcessPollingGroup(ModbusRtuPollingGroup& group);
    void UpdateSlaveStatus(int slave_id, int response_time_ms, bool success);
    int CheckSlaveStatus(int slave_id);
    
    /**
     * @brief RTU 통계 업데이트 (구조체 기반)
     * @param operation 작업 타입 ("read", "write")
     * @param success 성공 여부
     * @param error_type 에러 타입
     */
    void UpdateRtuStats(const std::string& operation, bool success, 
                        const std::string& error_type = "");
    
    void LockBus();
    void UnlockBus();
    void LogRtuMessage(LogLevel level, const std::string& message);
    std::vector<PulseOne::DataPoint> CreateDataPoints(int slave_id, 
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
};

} // namespace Protocol
} // namespace Workers  
} // namespace PulseOne

#endif // WORKERS_PROTOCOL_MODBUS_RTU_WORKER_H