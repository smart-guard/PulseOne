/**
 * @file ModbusRtuWorker.h
 * @brief Modbus RTU 프로토콜 전용 워커 클래스
 * @details SerialBasedWorker를 상속받아 Modbus RTU 시리얼 통신 기능 제공
 * @author PulseOne Development Team
 * @date 2025-01-21
 * @version 1.0.0
 */

#ifndef WORKERS_PROTOCOL_MODBUS_RTU_WORKER_H
#define WORKERS_PROTOCOL_MODBUS_RTU_WORKER_H

#include "Workers/Base/SerialBasedWorker.h"
#include "Drivers/Common/CommonTypes.h"
#include <memory>
#include <queue>
#include <thread>
#include <atomic>
#include <map>
#include <chrono>
#include <modbus/modbus.h>
#include <cstring>  // strerror 함수용

namespace PulseOne {
namespace Workers {
namespace Protocol {

/**
 * @brief Modbus 레지스터 타입
 */
enum class ModbusRegisterType {
    COIL = 0,              ///< 코일 (0x01, 0x05, 0x0F)
    DISCRETE_INPUT = 1,    ///< 접점 입력 (0x02)
    HOLDING_REGISTER = 2,  ///< 홀딩 레지스터 (0x03, 0x06, 0x10)
    INPUT_REGISTER = 3     ///< 입력 레지스터 (0x04)
};

/**
 * @brief Modbus RTU 슬레이브 정보
 */
struct ModbusSlaveInfo {
    int slave_id;                                    ///< 슬레이브 ID (1-247)
    bool is_online;                                  ///< 온라인 상태
    std::chrono::system_clock::time_point last_response; ///< 마지막 응답 시간
    std::atomic<uint32_t> response_time_ms{0};       ///< 평균 응답 시간
    std::atomic<uint64_t> total_requests{0};         ///< 총 요청 수
    std::atomic<uint64_t> successful_requests{0};    ///< 성공 요청 수
    std::string last_error;                          ///< 마지막 에러 메시지
    
    ModbusSlaveInfo(int id = 1) 
        : slave_id(id), is_online(false)
        , last_response(std::chrono::system_clock::now()) {}
};

/**
 * @brief Modbus RTU 폴링 그룹
 */
struct ModbusPollingGroup {
    uint32_t group_id;                               ///< 그룹 고유 ID
    std::string group_name;                          ///< 그룹 이름
    int slave_id;                                    ///< 대상 슬레이브 ID
    ModbusRegisterType register_type;                ///< 레지스터 타입
    uint16_t start_address;                          ///< 시작 주소
    uint16_t register_count;                         ///< 읽을 레지스터 수
    int polling_interval_ms;                         ///< 폴링 주기 (밀리초)
    bool enabled;                                    ///< 활성화 여부
    
    std::vector<Drivers::DataPoint> data_points;     ///< 이 그룹에 속한 데이터 포인트들
    
    // 실행 시간 추적 (단순 타입만 사용)
    std::chrono::system_clock::time_point last_poll_time;
    std::chrono::system_clock::time_point next_poll_time;
    
    ModbusPollingGroup() 
        : group_id(0), slave_id(1), register_type(ModbusRegisterType::HOLDING_REGISTER)
        , start_address(0), register_count(1), polling_interval_ms(1000), enabled(true)
        , last_poll_time(std::chrono::system_clock::now())
        , next_poll_time(std::chrono::system_clock::now()) {}
};

/**
 * @brief Modbus RTU 워커 클래스
 * @details SerialBasedWorker를 상속받아 Modbus RTU 시리얼 통신 특화 기능 제공
 * 
 * 주요 기능:
 * - Modbus RTU 시리얼 통신 관리
 * - 슬레이브 디바이스 자동 검색
 * - 폴링 그룹 기반 효율적 데이터 수집
 * - 읽기/쓰기 작업 큐 관리
 * - CRC 에러 검출 및 재시도
 * - 시리얼 버스 충돌 방지
 */
class ModbusRtuWorker : public SerialBasedWorker {
public:
    /**
     * @brief 생성자
     * @param device_info 디바이스 정보
     * @param redis_client Redis 클라이언트 (선택적)
     * @param influx_client InfluxDB 클라이언트 (선택적)
     */
    explicit ModbusRtuWorker(
        const Drivers::DeviceInfo& device_info,
        std::shared_ptr<RedisClient> redis_client = nullptr,
        std::shared_ptr<InfluxClient> influx_client = nullptr
    );
    
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
    // SerialBasedWorker 인터페이스 구현
    // =============================================================================
    
    bool EstablishProtocolConnection() override;
    bool CloseProtocolConnection() override;  // bool 타입으로 수정
    bool CheckProtocolConnection() override;
    bool SendProtocolKeepAlive() override;

    // =============================================================================
    // Modbus RTU 특화 설정 관리
    // =============================================================================
    
    /**
     * @brief Modbus RTU 기본 설정
     * @param default_slave_id 기본 슬레이브 ID (1-247)
     * @param response_timeout 응답 타임아웃 (밀리초)
     * @param byte_timeout 바이트 타임아웃 (밀리초)
     * @return 설정 성공 시 true
     */
    bool ConfigureModbusRtu(int default_slave_id = 1, 
                           int response_timeout = 1000,
                           int byte_timeout = 1000);
    
    /**
     * @brief 슬레이브 추가
     * @param slave_id 슬레이브 ID (1-247)
     * @param description 슬레이브 설명
     * @return 추가 성공 시 true
     */
    bool AddSlave(int slave_id, const std::string& description = "");
    
    /**
     * @brief 슬레이브 제거
     * @param slave_id 슬레이브 ID
     * @return 제거 성공 시 true
     */
    bool RemoveSlave(int slave_id);

    // =============================================================================
    // 폴링 그룹 관리
    // =============================================================================
    
    /**
     * @brief 폴링 그룹 추가
     * @param group_name 그룹 이름
     * @param slave_id 슬레이브 ID
     * @param register_type 레지스터 타입
     * @param start_address 시작 주소
     * @param register_count 레지스터 개수
     * @param polling_interval_ms 폴링 주기 (밀리초)
     * @return 그룹 ID (실패 시 0)
     */
    uint32_t AddPollingGroup(const std::string& group_name,
                            int slave_id,
                            ModbusRegisterType register_type,
                            uint16_t start_address,
                            uint16_t register_count,
                            int polling_interval_ms = 1000);
    
    /**
     * @brief 폴링 그룹 제거
     * @param group_id 그룹 ID
     * @return 제거 성공 시 true
     */
    bool RemovePollingGroup(uint32_t group_id);
    
    /**
     * @brief 폴링 그룹 활성화/비활성화
     * @param group_id 그룹 ID
     * @param enabled 활성화 여부
     * @return 설정 성공 시 true
     */
    bool SetPollingGroupEnabled(uint32_t group_id, bool enabled);

    // =============================================================================
    // 동기 읽기/쓰기 작업
    // =============================================================================
    
    /**
     * @brief 레지스터 읽기 (동기)
     * @param slave_id 슬레이브 ID
     * @param register_type 레지스터 타입
     * @param address 레지스터 주소
     * @param count 읽을 개수
     * @param values 읽은 값들 (출력)
     * @return 읽기 성공 시 true
     */
    bool ReadRegisters(int slave_id,
                      ModbusRegisterType register_type,
                      uint16_t address,
                      uint16_t count,
                      std::vector<uint16_t>& values);
    
    /**
     * @brief 단일 레지스터 쓰기
     * @param slave_id 슬레이브 ID
     * @param register_type 레지스터 타입 (COIL 또는 HOLDING_REGISTER만 가능)
     * @param address 레지스터 주소
     * @param value 쓸 값
     * @return 쓰기 성공 시 true
     */
    bool WriteRegister(int slave_id,
                      ModbusRegisterType register_type,
                      uint16_t address,
                      uint16_t value);
    
    /**
     * @brief 다중 레지스터 쓰기
     * @param slave_id 슬레이브 ID
     * @param register_type 레지스터 타입 (COIL 또는 HOLDING_REGISTER만 가능)
     * @param address 시작 주소
     * @param values 쓸 값들
     * @return 쓰기 성공 시 true
     */
    bool WriteRegisters(int slave_id,
                       ModbusRegisterType register_type,
                       uint16_t address,
                       const std::vector<uint16_t>& values);

    // =============================================================================
    // 진단 및 모니터링
    // =============================================================================
    
    /**
     * @brief 슬레이브 스캔 (연결된 슬레이브들 자동 검색)
     * @param start_id 스캔 시작 ID (기본: 1)
     * @param end_id 스캔 종료 ID (기본: 247)
     * @param timeout_ms 각 슬레이브별 타임아웃 (기본: 500ms)
     * @return 발견된 슬레이브 ID 목록
     */
    std::vector<int> ScanSlaves(int start_id = 1, int end_id = 247, int timeout_ms = 500);
    
    /**
     * @brief 개별 슬레이브 상태 확인
     * @param slave_id 슬레이브 ID
     * @return 응답 시간 (밀리초, -1이면 오프라인)
     */
    int CheckSlaveStatus(int slave_id);
    
    /**
     * @brief Modbus RTU 통계 정보 반환
     * @return JSON 형태의 통계 정보
     */
    std::string GetModbusStatistics() const;
    
    /**
     * @brief 슬레이브 목록 반환
     * @return JSON 형태의 슬레이브 정보
     */
    std::string GetSlaveList() const;
    
    /**
     * @brief 폴링 그룹 목록 반환
     * @return JSON 형태의 폴링 그룹 정보
     */
    std::string GetPollingGroupList() const;

    // =============================================================================
    // 고급 기능들
    // =============================================================================
    
    /**
     * @brief 브로드캐스트 쓰기 (모든 슬레이브에게 동일한 값 전송)
     * @param register_type 레지스터 타입
     * @param address 레지스터 주소
     * @param value 쓸 값
     * @return 전송 성공 시 true
     */
    bool BroadcastWrite(ModbusRegisterType register_type, uint16_t address, uint16_t value);
    
    /**
     * @brief 버스 락 시간 설정
     * @param lock_time_ms 버스 락 시간 (밀리초)
     */
    void SetBusLockTime(int lock_time_ms);

protected:
    // =============================================================================
    // 내부 구조체들
    // =============================================================================
    
    /**
     * @brief 비동기 작업 요청
     */
    struct AsyncRequest {
        uint64_t request_id;
        int slave_id;
        ModbusRegisterType register_type;
        uint16_t address;
        uint16_t count;
        std::vector<uint16_t> write_values;
        bool is_write_operation;
        std::chrono::system_clock::time_point created_at;
        std::chrono::system_clock::time_point deadline;
        
        AsyncRequest() : request_id(0), slave_id(1), register_type(ModbusRegisterType::HOLDING_REGISTER)
                       , address(0), count(1), is_write_operation(false)
                       , created_at(std::chrono::system_clock::now())
                       , deadline(std::chrono::system_clock::now() + std::chrono::seconds(30)) {}
    };

private:
    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    // Modbus RTU 컨텍스트
    modbus_t* modbus_ctx_;
    
    // 설정
    int default_slave_id_;
    int response_timeout_ms_;
    int byte_timeout_ms_;
    int bus_lock_time_ms_;  // 시리얼 버스 락 시간
    
    // 슬레이브 관리
    mutable std::mutex slaves_mutex_;
    std::map<int, std::unique_ptr<ModbusSlaveInfo>> slaves_;
    
    // 폴링 관리
    mutable std::mutex polling_groups_mutex_;
    std::map<uint32_t, ModbusPollingGroup> polling_groups_;
    std::atomic<uint32_t> next_group_id_;
    
    // 작업 스레드들
    std::thread polling_thread_;
    std::atomic<bool> stop_workers_;
    
    // 버스 동기화 (시리얼 버스는 한 번에 하나의 작업만 가능)
    mutable std::mutex bus_mutex_;
    
    // 통계 (atomic 사용하지 않고 mutex로 보호)
    mutable std::mutex stats_mutex_;
    uint64_t total_reads_;
    uint64_t successful_reads_;
    uint64_t total_writes_;
    uint64_t successful_writes_;
    uint64_t crc_errors_;
    uint64_t timeout_errors_;
    uint64_t bus_errors_;

    // =============================================================================
    // 내부 헬퍼 메소드들
    // =============================================================================
    
    /**
     * @brief 폴링 워커 루프
     */
    void PollingWorkerLoop();
    
    /**
     * @brief 개별 폴링 그룹 처리
     * @param group 폴링 그룹
     * @return 처리 성공 시 true
     */
    bool ProcessPollingGroup(ModbusPollingGroup& group);
    
    /**
     * @brief Modbus 에러 코드를 문자열로 변환
     * @param error_code 에러 코드
     * @return 에러 메시지
     */
    std::string ModbusErrorToString(int error_code) const;
    
    /**
     * @brief 통계 업데이트
     * @param operation 연산 타입 ("read", "write")
     * @param success 성공 여부
     * @param error_type 에러 타입 ("crc", "timeout", "bus")
     */
    void UpdateModbusStats(const std::string& operation, bool success, 
                          const std::string& error_type = "");
    
    /**
     * @brief 슬레이브 응답 시간 업데이트
     * @param slave_id 슬레이브 ID
     * @param response_time_ms 응답 시간
     * @param success 성공 여부
     */
    void UpdateSlaveStatus(int slave_id, int response_time_ms, bool success);
    
    /**
     * @brief 시리얼 버스 락 (충돌 방지)
     */
    void LockBus();
    
    /**
     * @brief 시리얼 버스 언락
     */
    void UnlockBus();
    
    /**
     * @brief Modbus 로그 메시지 출력
     * @param level 로그 레벨
     * @param message 메시지
     */
    void LogModbusMessage(LogLevel level, const std::string& message) const;
    
    /**
     * @brief 폴링 그룹 검증
     * @param slave_id 슬레이브 ID
     * @param register_type 레지스터 타입
     * @param start_address 시작 주소
     * @param register_count 레지스터 개수
     * @return 유효한 설정인지 여부
     */
    bool ValidatePollingGroup(int slave_id, ModbusRegisterType register_type,
                             uint16_t start_address, uint16_t register_count) const;
};

} // namespace Protocol
} // namespace Workers
} // namespace PulseOne

#endif // WORKERS_PROTOCOL_MODBUS_RTU_WORKER_H