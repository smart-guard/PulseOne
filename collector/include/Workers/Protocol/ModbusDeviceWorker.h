/**
 * @file ModbusDeviceWorker.h
 * @brief Modbus 프로토콜 전용 디바이스 워커
 * @details TcpBasedWorker를 상속받아 Modbus TCP/RTU 특화 기능 제공
 * @author PulseOne Development Team
 * @date 2025-01-21
 * @version 1.0.0
 */

#ifndef WORKERS_PROTOCOL_MODBUS_DEVICE_WORKER_H
#define WORKERS_PROTOCOL_MODBUS_DEVICE_WORKER_H

#include "Workers/Base/TcpBasedWorker.h"
#include "Drivers/Modbus/ModbusDriver.h"
#include "Drivers/Common/CommonTypes.h"
#include <memory>
#include <queue>
#include <thread>
#include <atomic>
#include <map>
#include <chrono>

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
 * @brief Modbus 폴링 그룹
 * @details 같은 주기로 읽을 레지스터들을 그룹화
 */
struct ModbusPollingGroup {
    uint32_t group_id;                    ///< 그룹 고유 ID
    std::string group_name;               ///< 그룹 이름
    ModbusRegisterType register_type;     ///< 레지스터 타입
    uint16_t start_address;               ///< 시작 주소
    uint16_t register_count;              ///< 읽을 레지스터 수
    int polling_interval_ms;              ///< 폴링 주기 (밀리초)
    int slave_id;                         ///< 슬레이브 ID
    bool enabled;                         ///< 활성화 여부
    
    std::vector<Drivers::DataPoint> data_points; ///< 이 그룹에 속한 데이터 포인트들
    
    // 실행 시간 추적
    std::chrono::system_clock::time_point last_poll_time;     ///< 마지막 폴링 시간
    std::chrono::system_clock::time_point next_poll_time;     ///< 다음 폴링 시간
    std::atomic<uint64_t> poll_count{0};                      ///< 폴링 횟수
    std::atomic<uint64_t> success_count{0};                   ///< 성공 횟수
    std::atomic<uint32_t> last_response_time_ms{0};           ///< 마지막 응답 시간
    
    ModbusPollingGroup() 
        : group_id(0), register_type(ModbusRegisterType::HOLDING_REGISTER)
        , start_address(0), register_count(1), polling_interval_ms(1000)
        , slave_id(1), enabled(true)
        , last_poll_time(std::chrono::system_clock::now())
        , next_poll_time(std::chrono::system_clock::now()) {}
};

/**
 * @brief Modbus 디바이스 워커 클래스
 * @details Modbus TCP/RTU 프로토콜 전용 워커로 다음 기능을 제공합니다:
 * 
 * 주요 기능:
 * - Modbus TCP/RTU 통신 관리
 * - 폴링 그룹 기반 효율적 데이터 수집
 * - 읽기/쓰기 큐 관리
 * - Modbus 에러 코드 처리
 * - 레지스터 맵 관리
 * - 슬레이브 디바이스 관리
 */
class ModbusDeviceWorker : public Base::TcpBasedWorker {
public:
    /**
     * @brief 생성자
     * @param device_info 디바이스 정보
     * @param redis_client Redis 클라이언트 (선택적)
     * @param influx_client InfluxDB 클라이언트 (선택적)
     * @param logger 로거 (선택적)
     */
    explicit ModbusDeviceWorker(
        const Drivers::DeviceInfo& device_info,
        std::shared_ptr<RedisClient> redis_client = nullptr,
        std::shared_ptr<InfluxClient> influx_client = nullptr,
        std::shared_ptr<LogManager> logger = nullptr
    );
    
    /**
     * @brief 소멸자
     */
    virtual ~ModbusDeviceWorker();

    // =============================================================================
    // BaseDeviceWorker 인터페이스 구현
    // =============================================================================
    
    std::future<bool> Start() override;
    std::future<bool> Stop() override;
    WorkerState GetState() const override;

    // =============================================================================
    // Modbus 특화 설정 관리
    // =============================================================================
    
    /**
     * @brief Modbus 기본 설정
     * @param slave_id 기본 슬레이브 ID
     * @param response_timeout 응답 타임아웃 (밀리초)
     * @param byte_timeout 바이트 타임아웃 (밀리초)
     * @return 설정 성공 시 true
     */
    bool ConfigureModbus(int slave_id = 1, 
                        int response_timeout = 1000,
                        int byte_timeout = 1000);
    
    /**
     * @brief RTU 모드 설정 (시리얼 통신용)
     * @param port 시리얼 포트 ("/dev/ttyUSB0" 등)
     * @param baud_rate 보드레이트
     * @param parity 패리티 ('N', 'E', 'O')
     * @param data_bits 데이터 비트 (7 또는 8)
     * @param stop_bits 스톱 비트 (1 또는 2)
     * @return 설정 성공 시 true
     */
    bool ConfigureRTU(const std::string& port,
                     int baud_rate = 9600,
                     char parity = 'N',
                     int data_bits = 8,
                     int stop_bits = 1);

    // =============================================================================
    // 폴링 그룹 관리
    // =============================================================================
    
    /**
     * @brief 폴링 그룹 추가
     * @param group 폴링 그룹 정보
     * @return 성공 시 그룹 ID, 실패 시 0
     */
    uint32_t AddPollingGroup(const ModbusPollingGroup& group);
    
    /**
     * @brief 폴링 그룹 삭제
     * @param group_id 그룹 ID
     * @return 삭제 성공 시 true
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
     * @brief 폴링 그룹 주기 변경
     * @param group_id 그룹 ID
     * @param interval_ms 새로운 주기 (밀리초)
     * @return 성공 시 true
     */
    bool SetPollingInterval(uint32_t group_id, int interval_ms);
    
    /**
     * @brief 전체 폴링 그룹 정보 반환
     * @return 폴링 그룹 목록
     */
    std::vector<ModbusPollingGroup> GetPollingGroups() const;

    // =============================================================================
    // 수동 읽기/쓰기 작업
    // =============================================================================
    
    /**
     * @brief 개별 레지스터 읽기
     * @param slave_id 슬레이브 ID
     * @param register_type 레지스터 타입
     * @param address 레지스터 주소
     * @param count 읽을 개수
     * @return 읽은 값들 (실패 시 empty vector)
     */
    std::vector<uint16_t> ReadRegisters(int slave_id,
                                       ModbusRegisterType register_type,
                                       uint16_t address,
                                       uint16_t count);
    
    /**
     * @brief 개별 레지스터 쓰기
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
    // 비동기 작업 관리
    // =============================================================================
    
    /**
     * @brief 비동기 읽기 요청
     * @param slave_id 슬레이브 ID
     * @param register_type 레지스터 타입
     * @param address 레지스터 주소
     * @param count 읽을 개수
     * @param callback 완료 콜백 함수
     * @return 요청 ID (실패 시 0)
     */
    uint64_t ReadRegistersAsync(int slave_id,
                               ModbusRegisterType register_type,
                               uint16_t address,
                               uint16_t count,
                               std::function<void(bool, const std::vector<uint16_t>&)> callback);
    
    /**
     * @brief 비동기 쓰기 요청
     * @param slave_id 슬레이브 ID
     * @param register_type 레지스터 타입
     * @param address 레지스터 주소
     * @param value 쓸 값
     * @param callback 완료 콜백 함수
     * @return 요청 ID (실패 시 0)
     */
    uint64_t WriteRegisterAsync(int slave_id,
                               ModbusRegisterType register_type,
                               uint16_t address,
                               uint16_t value,
                               std::function<void(bool)> callback);

    // =============================================================================
    // 진단 및 모니터링
    // =============================================================================
    
    /**
     * @brief Modbus 통계 정보 반환
     * @return JSON 형태의 통계 정보
     */
    std::string GetModbusStatistics() const;
    
    /**
     * @brief 슬레이브 상태 확인
     * @param slave_id 슬레이브 ID
     * @return 연결 상태 (응답 시간, -1이면 오프라인)
     */
    int CheckSlaveStatus(int slave_id);
    
    /**
     * @brief 레지스터 맵 진단
     * @return 레지스터 맵 정보 (JSON)
     */
    std::string GetRegisterMapDiagnostics() const;

protected:
    // =============================================================================
    // TcpBasedWorker 인터페이스 구현
    // =============================================================================
    
    bool EstablishTcpConnection() override;
    bool CloseTcpConnection() override;
    bool CheckTcpConnectionStatus() override;

private:
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
        
        // 콜백 함수들
        std::function<void(bool, const std::vector<uint16_t>&)> read_callback;
        std::function<void(bool)> write_callback;
        
        AsyncRequest() : request_id(0), slave_id(1), register_type(ModbusRegisterType::HOLDING_REGISTER)
                       , address(0), count(1), is_write_operation(false)
                       , created_at(std::chrono::system_clock::now())
                       , deadline(std::chrono::system_clock::now() + std::chrono::seconds(30)) {}
    };

    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    // Modbus 드라이버
    std::unique_ptr<Drivers::ModbusDriver> modbus_driver_;
    
    // 설정
    int default_slave_id_;
    int response_timeout_ms_;
    int byte_timeout_ms_;
    bool is_rtu_mode_;
    std::string rtu_port_;
    int rtu_baud_rate_;
    char rtu_parity_;
    int rtu_data_bits_;
    int rtu_stop_bits_;
    
    // 폴링 관리
    mutable std::mutex polling_groups_mutex_;
    std::map<uint32_t, ModbusPollingGroup> polling_groups_;
    std::atomic<uint32_t> next_group_id_;
    
    // 작업 스레드들
    std::thread polling_thread_;
    std::thread async_worker_thread_;
    std::atomic<bool> stop_workers_;
    
    // 비동기 작업 큐
    mutable std::mutex async_queue_mutex_;
    std::condition_variable async_queue_cv_;
    std::queue<std::unique_ptr<AsyncRequest>> async_queue_;
    std::atomic<uint64_t> next_request_id_;
    
    // 통계
    mutable std::mutex stats_mutex_;
    std::atomic<uint64_t> total_reads_{0};
    std::atomic<uint64_t> successful_reads_{0};
    std::atomic<uint64_t> total_writes_{0};
    std::atomic<uint64_t> successful_writes_{0};
    std::atomic<uint64_t> modbus_errors_{0};
    
    // 슬레이브 상태 추적
    mutable std::mutex slave_status_mutex_;
    std::map<int, std::chrono::system_clock::time_point> slave_last_response_;
    std::map<int, int> slave_response_times_;

    // =============================================================================
    // 내부 헬퍼 메소드들
    // =============================================================================
    
    /**
     * @brief 폴링 워커 루프
     */
    void PollingWorkerLoop();
    
    /**
     * @brief 비동기 작업 워커 루프
     */
    void AsyncWorkerLoop();
    
    /**
     * @brief 개별 폴링 그룹 처리
     * @param group 폴링 그룹
     * @return 처리 성공 시 true
     */
    bool ProcessPollingGroup(ModbusPollingGroup& group);
    
    /**
     * @brief 비동기 요청 처리
     * @param request 요청
     */
    void ProcessAsyncRequest(std::unique_ptr<AsyncRequest> request);
    
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
     */
    void UpdateModbusStats(const std::string& operation, bool success);
    
    /**
     * @brief 슬레이브 응답 시간 업데이트
     * @param slave_id 슬레이브 ID
     * @param response_time_ms 응답 시간
     */
    void UpdateSlaveStatus(int slave_id, int response_time_ms);
    
    /**
     * @brief Modbus 로그 메시지 출력
     * @param level 로그 레벨
     * @param message 메시지
     */
    void LogModbusMessage(LogLevel level, const std::string& message);
};

} // namespace Protocol
} // namespace Workers
} // namespace PulseOne

#endif // WORKERS_PROTOCOL_MODBUS_DEVICE_WORKER_H