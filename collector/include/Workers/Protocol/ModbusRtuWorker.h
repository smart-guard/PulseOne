/**
 * @file ModbusRtuWorker.h
 * @brief Modbus RTU 프로토콜 전용 워커 클래스
 * @details SerialBasedWorker를 상속받아 Modbus RTU 시리얼 통신 기능 제공
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 1.0.0
 */

#ifndef WORKERS_PROTOCOL_MODBUS_RTU_WORKER_H
#define WORKERS_PROTOCOL_MODBUS_RTU_WORKER_H

#include "Workers/Base/SerialBasedWorker.h"
#include "Drivers/Modbus/ModbusDriver.h"
#include "Drivers/Common/CommonTypes.h"
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
struct ModbusRtuSlaveInfo {
    int slave_id;                                    ///< 슬레이브 ID (1-247)
    std::string device_name;                         ///< 디바이스 이름
    bool is_online;                                  ///< 온라인 상태
    std::chrono::system_clock::time_point last_response; ///< 마지막 응답 시간
    std::atomic<uint32_t> response_time_ms{0};       ///< 평균 응답 시간
    std::atomic<uint64_t> total_requests{0};         ///< 총 요청 수
    std::atomic<uint64_t> successful_requests{0};    ///< 성공 요청 수
    std::atomic<uint64_t> crc_errors{0};             ///< CRC 에러 수
    std::atomic<uint64_t> timeout_errors{0};        ///< 타임아웃 에러 수
    std::string last_error;                          ///< 마지막 에러 메시지
    
    ModbusRtuSlaveInfo(int id = 1, const std::string& name = "Unknown") 
        : slave_id(id), device_name(name), is_online(false)
        , last_response(std::chrono::system_clock::now()) {}
};

/**
 * @brief 시리얼 버스 설정
 */
struct SerialBusConfig {
    std::string port_name;           ///< 시리얼 포트 ("/dev/ttyUSB0", "COM1")
    int baud_rate;                   ///< 보드레이트 (9600, 19200, 38400, 115200)
    int data_bits;                   ///< 데이터 비트 (7, 8)
    int stop_bits;                   ///< 스톱 비트 (1, 2)
    char parity;                     ///< 패리티 ('N', 'E', 'O')
    int response_timeout_ms;         ///< 응답 타임아웃 (밀리초)
    int byte_timeout_ms;             ///< 바이트 간 타임아웃 (밀리초)
    int inter_frame_delay_ms;        ///< 프레임 간 지연 (밀리초)
    
    SerialBusConfig() 
        : port_name("/dev/ttyUSB0"), baud_rate(9600), data_bits(8), stop_bits(1)
        , parity('N'), response_timeout_ms(1000), byte_timeout_ms(100)
        , inter_frame_delay_ms(50) {}
};

/**
 * @brief Modbus RTU 폴링 그룹
 */
struct ModbusRtuPollingGroup {
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
    
    ModbusRtuPollingGroup() 
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
 * - ModbusDriver를 통한 RTU 통신 관리
 * - 시리얼 버스 액세스 제어 (한 번에 하나의 트랜잭션)
 * - 슬레이브 디바이스 자동 검색
 * - 폴링 그룹 기반 효율적 데이터 수집
 * - CRC 에러 검출 및 재시도
 * - 프레임 간 지연 관리
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
    bool CloseProtocolConnection() override;
    bool CheckProtocolConnection() override;
    bool SendProtocolKeepAlive() override;

    // =============================================================================
    // Modbus RTU 특화 설정 관리
    // =============================================================================
    
    /**
     * @brief Modbus RTU 기본 설정
     * @param default_slave_id 기본 슬레이브 ID
     * @param response_timeout_ms 응답 타임아웃
     * @param byte_timeout_ms 바이트 간 타임아웃
     * @param inter_frame_delay_ms 프레임 간 지연
     */
    void ConfigureModbusRtu(int default_slave_id = 1,
                           int response_timeout_ms = 1000,
                           int byte_timeout_ms = 100,
                           int inter_frame_delay_ms = 50);
    
    /**
     * @brief 시리얼 버스 설정
     * @param config 시리얼 버스 설정
     */
    void ConfigureSerialBus(const SerialBusConfig& config);
    
    /**
     * @brief 재연결 설정
     * @param max_retry_attempts 최대 재시도 횟수
     * @param retry_delay_ms 재시도 간격
     * @param exponential_backoff 지수 백오프 사용 여부
     */
    void ConfigureReconnection(int max_retry_attempts = 3,
                              int retry_delay_ms = 5000,
                              bool exponential_backoff = true);

    // =============================================================================
    // 슬레이브 관리
    // =============================================================================
    
    /**
     * @brief 슬레이브 추가
     * @param slave_id 슬레이브 ID (1-247)
     * @param device_name 디바이스 이름
     * @return 성공 시 true
     */
    bool AddSlave(int slave_id, const std::string& device_name = "");
    
    /**
     * @brief 슬레이브 제거
     * @param slave_id 슬레이브 ID
     * @return 성공 시 true
     */
    bool RemoveSlave(int slave_id);
    
    /**
     * @brief 슬레이브 상태 조회
     * @param slave_id 슬레이브 ID
     * @return 슬레이브 정보 (없으면 nullptr)
     */
    std::shared_ptr<ModbusRtuSlaveInfo> GetSlaveInfo(int slave_id) const;
    
    /**
     * @brief 슬레이브 스캔 (시리얼 버스에서 응답하는 슬레이브 찾기)
     * @param start_id 시작 슬레이브 ID
     * @param end_id 끝 슬레이브 ID
     * @param timeout_ms 각 슬레이브별 스캔 타임아웃
     * @return 발견된 슬레이브 수
     */
    int ScanSlaves(int start_id = 1, int end_id = 247, int timeout_ms = 2000);

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
     * @param polling_interval_ms 폴링 주기
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
     * @return 성공 시 true
     */
    bool RemovePollingGroup(uint32_t group_id);
    
    /**
     * @brief 폴링 그룹 활성화/비활성화
     * @param group_id 그룹 ID
     * @param enabled 활성화 여부
     * @return 성공 시 true
     */
    bool EnablePollingGroup(uint32_t group_id, bool enabled);
    
    /**
     * @brief 폴링 그룹에 데이터 포인트 추가
     * @param group_id 그룹 ID
     * @param data_point 데이터 포인트
     * @return 성공 시 true
     */
    bool AddDataPointToGroup(uint32_t group_id, const Drivers::DataPoint& data_point);

    // =============================================================================
    // 데이터 읽기/쓰기 (ModbusDriver 사용)
    // =============================================================================
    
    /**
     * @brief 홀딩 레지스터 읽기
     * @param slave_id 슬레이브 ID
     * @param start_address 시작 주소
     * @param register_count 레지스터 개수
     * @param values 읽은 값들 (출력)
     * @return 성공 시 true
     */
    bool ReadHoldingRegisters(int slave_id, uint16_t start_address, 
                             uint16_t register_count, std::vector<uint16_t>& values);
    
    /**
     * @brief 입력 레지스터 읽기
     * @param slave_id 슬레이브 ID
     * @param start_address 시작 주소
     * @param register_count 레지스터 개수
     * @param values 읽은 값들 (출력)
     * @return 성공 시 true
     */
    bool ReadInputRegisters(int slave_id, uint16_t start_address, 
                           uint16_t register_count, std::vector<uint16_t>& values);
    
    /**
     * @brief 코일 읽기
     * @param slave_id 슬레이브 ID
     * @param start_address 시작 주소
     * @param coil_count 코일 개수
     * @param values 읽은 값들 (출력)
     * @return 성공 시 true
     */
    bool ReadCoils(int slave_id, uint16_t start_address, 
                   uint16_t coil_count, std::vector<bool>& values);
    
    /**
     * @brief 접점 입력 읽기
     * @param slave_id 슬레이브 ID
     * @param start_address 시작 주소
     * @param input_count 입력 개수
     * @param values 읽은 값들 (출력)
     * @return 성공 시 true
     */
    bool ReadDiscreteInputs(int slave_id, uint16_t start_address, 
                           uint16_t input_count, std::vector<bool>& values);
    
    /**
     * @brief 단일 홀딩 레지스터 쓰기
     * @param slave_id 슬레이브 ID
     * @param address 주소
     * @param value 쓸 값
     * @return 성공 시 true
     */
    bool WriteSingleRegister(int slave_id, uint16_t address, uint16_t value);
    
    /**
     * @brief 단일 코일 쓰기
     * @param slave_id 슬레이브 ID
     * @param address 주소
     * @param value 쓸 값
     * @return 성공 시 true
     */
    bool WriteSingleCoil(int slave_id, uint16_t address, bool value);
    
    /**
     * @brief 다중 홀딩 레지스터 쓰기
     * @param slave_id 슬레이브 ID
     * @param start_address 시작 주소
     * @param values 쓸 값들
     * @return 성공 시 true
     */
    bool WriteMultipleRegisters(int slave_id, uint16_t start_address, 
                               const std::vector<uint16_t>& values);
    
    /**
     * @brief 다중 코일 쓰기
     * @param slave_id 슬레이브 ID
     * @param start_address 시작 주소
     * @param values 쓸 값들
     * @return 성공 시 true
     */
    bool WriteMultipleCoils(int slave_id, uint16_t start_address, 
                           const std::vector<bool>& values);

    // =============================================================================
    // 상태 및 통계 조회
    // =============================================================================
    
    /**
     * @brief Modbus RTU 통계 조회
     * @return JSON 형태의 통계 정보
     */
    std::string GetModbusRtuStats() const;
    
    /**
     * @brief 시리얼 버스 상태 조회
     * @return JSON 형태의 버스 상태
     */
    std::string GetSerialBusStatus() const;
    
    /**
     * @brief 모든 슬레이브 상태 조회
     * @return JSON 형태의 슬레이브 상태 목록
     */
    std::string GetSlaveStatusList() const;
    
    /**
     * @brief 모든 폴링 그룹 상태 조회
     * @return JSON 형태의 그룹 상태
     */
    std::string GetPollingGroupStatus() const;

protected:
    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    // ModbusDriver 인스턴스
    std::unique_ptr<Drivers::ModbusDriver> modbus_driver_;
    
    // RTU 설정
    int default_slave_id_;
    int response_timeout_ms_;
    int byte_timeout_ms_;
    int inter_frame_delay_ms_;
    
    // 시리얼 버스 설정
    SerialBusConfig serial_bus_config_;
    
    // 시리얼 버스 액세스 제어 (RTU는 반이중 통신)
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
    
    // 통계 (mutex로 보호)
    mutable std::mutex stats_mutex_;
    uint64_t total_reads_;
    uint64_t successful_reads_;
    uint64_t total_writes_;
    uint64_t successful_writes_;
    uint64_t crc_errors_;
    uint64_t timeout_errors_;
    uint64_t frame_errors_;

    // =============================================================================
    // 내부 헬퍼 메소드들
    // =============================================================================
    
    /**
     * @brief 폴링 워커 스레드 실행 함수
     */
    void PollingWorkerThread();
    
    /**
     * @brief 폴링 그룹 처리
     * @param group 폴링 그룹
     * @return 성공 시 true
     */
    bool ProcessPollingGroup(ModbusRtuPollingGroup& group);
    
    /**
     * @brief 슬레이브 상태 업데이트
     * @param slave_id 슬레이브 ID
     * @param response_time_ms 응답 시간
     * @param success 성공 여부
     */
    void UpdateSlaveStatus(int slave_id, int response_time_ms, bool success);
    
    /**
     * @brief 슬레이브 상태 확인 (단순 핑)
     * @param slave_id 슬레이브 ID
     * @return 응답 시간 (실패 시 -1)
     */
    int CheckSlaveStatus(int slave_id);
    
    /**
     * @brief RTU 통계 업데이트
     * @param operation 작업 타입 ("read", "write")
     * @param success 성공 여부
     * @param error_type 에러 타입 ("crc", "timeout", "frame" 등)
     */
    void UpdateRtuStats(const std::string& operation, bool success, 
                        const std::string& error_type = "");
    
    /**
     * @brief 시리얼 버스 잠금 (프레임 간 지연 포함)
     */
    void LockBus();
    
    /**
     * @brief 시리얼 버스 해제
     */
    void UnlockBus();
    
    /**
     * @brief RTU 전용 로그 메시지
     * @param level 로그 레벨
     * @param message 메시지
     */
    void LogRtuMessage(LogLevel level, const std::string& message);
    
    /**
     * @brief DataPoint를 ModbusDriver용 포맷으로 변환
     * @param slave_id 슬레이브 ID
     * @param register_type 레지스터 타입
     * @param start_address 시작 주소
     * @param count 개수
     * @return DataPoint 벡터
     */
    std::vector<Drivers::DataPoint> CreateDataPoints(int slave_id, 
                                                    ModbusRegisterType register_type,
                                                    uint16_t start_address, 
                                                    uint16_t count);
};

} // namespace Protocol
} // namespace Workers
} // namespace PulseOne

#endif // WORKERS_PROTOCOL_MODBUS_RTU_WORKER_H