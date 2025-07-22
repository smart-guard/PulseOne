/**
 * @file ModbusTcpWorker.h
 * @brief Modbus TCP 프로토콜 전용 워커 클래스
 * @details TcpBasedWorker를 상속받아 Modbus TCP 네트워크 통신 기능 제공
 * @author PulseOne Development Team
 * @date 2025-01-21
 * @version 1.0.0
 */

#ifndef WORKERS_PROTOCOL_MODBUS_TCP_WORKER_H
#define WORKERS_PROTOCOL_MODBUS_TCP_WORKER_H

#include "Workers/Base/TcpBasedWorker.h"
#include "Drivers/Common/CommonTypes.h"
#include <memory>
#include <queue>
#include <thread>
#include <atomic>
#include <map>
#include <chrono>
#include <modbus/modbus.h>
#include <cstring>  // strerror 함수용
#include <shared_mutex>
#include <sys/socket.h>

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
 * @brief Modbus TCP 슬레이브 정보
 */
struct ModbusTcpSlaveInfo {
    int slave_id;                                    ///< 슬레이브 ID (1-247)
    std::string ip_address;                          ///< IP 주소
    int port;                                        ///< TCP 포트 (기본 502)
    bool is_online;                                  ///< 온라인 상태
    std::chrono::system_clock::time_point last_response; ///< 마지막 응답 시간
    std::atomic<uint32_t> response_time_ms{0};       ///< 평균 응답 시간
    std::atomic<uint64_t> total_requests{0};         ///< 총 요청 수
    std::atomic<uint64_t> successful_requests{0};    ///< 성공 요청 수
    std::string last_error;                          ///< 마지막 에러 메시지
    
    ModbusTcpSlaveInfo(int id = 1, const std::string& ip = "127.0.0.1", int p = 502) 
        : slave_id(id), ip_address(ip), port(p), is_online(false)
        , last_response(std::chrono::system_clock::now()) {}
};

/**
 * @brief Modbus TCP 폴링 그룹
 */
struct ModbusTcpPollingGroup {
    uint32_t group_id;                               ///< 그룹 고유 ID
    std::string group_name;                          ///< 그룹 이름
    int slave_id;                                    ///< 대상 슬레이브 ID
    std::string target_ip;                           ///< 대상 IP 주소
    int target_port;                                 ///< 대상 포트
    ModbusRegisterType register_type;                ///< 레지스터 타입
    uint16_t start_address;                          ///< 시작 주소
    uint16_t register_count;                         ///< 읽을 레지스터 수
    int polling_interval_ms;                         ///< 폴링 주기 (밀리초)
    bool enabled;                                    ///< 활성화 여부
    
    std::vector<Drivers::DataPoint> data_points;     ///< 이 그룹에 속한 데이터 포인트들
    
    // 실행 시간 추적 (단순 타입만 사용)
    std::chrono::system_clock::time_point last_poll_time;
    std::chrono::system_clock::time_point next_poll_time;
    
    ModbusTcpPollingGroup() 
        : group_id(0), slave_id(1), target_ip("127.0.0.1"), target_port(502)
        , register_type(ModbusRegisterType::HOLDING_REGISTER)
        , start_address(0), register_count(1), polling_interval_ms(1000), enabled(true)
        , last_poll_time(std::chrono::system_clock::now())
        , next_poll_time(std::chrono::system_clock::now()) {}
};

/**
 * @brief Modbus TCP 워커 클래스
 * @details TcpBasedWorker를 상속받아 Modbus TCP 네트워크 통신 특화 기능 제공
 * 
 * 주요 기능:
 * - Modbus TCP 네트워크 통신 관리
 * - 다중 슬레이브 디바이스 연결 관리
 * - 폴링 그룹 기반 효율적 데이터 수집
 * - 읽기/쓰기 작업 큐 관리
 * - 네트워크 에러 검출 및 재시도
 * - TCP 연결 풀링 및 재사용
 */
class ModbusTcpWorker : public TcpBasedWorker {
public:
    /**
     * @brief 생성자
     * @param device_info 디바이스 정보
     * @param redis_client Redis 클라이언트 (선택적)
     * @param influx_client InfluxDB 클라이언트 (선택적)
     */
    explicit ModbusTcpWorker(
        const Drivers::DeviceInfo& device_info,
        std::shared_ptr<RedisClient> redis_client = nullptr,
        std::shared_ptr<InfluxClient> influx_client = nullptr
    );
    
    /**
     * @brief 소멸자
     */
    virtual ~ModbusTcpWorker();

    // =============================================================================
    // BaseDeviceWorker 인터페이스 구현
    // =============================================================================
    
    std::future<bool> Start() override;
    std::future<bool> Stop() override;
    WorkerState GetState() const override;

    // =============================================================================
    // TcpBasedWorker 인터페이스 구현
    // =============================================================================
    
    bool EstablishProtocolConnection() override;
    bool CloseProtocolConnection() override;
    bool CheckProtocolConnection() override;
    bool SendProtocolKeepAlive();

    // =============================================================================
    // Modbus TCP 특화 설정 관리
    // =============================================================================
    
    /**
     * @brief Modbus TCP 기본 설정
     * @param default_slave_id 기본 슬레이브 ID
     * @param response_timeout_ms 응답 타임아웃 (밀리초)
     * @param byte_timeout_ms 바이트 타임아웃 (밀리초)
     */
    void ConfigureModbusTcp(int default_slave_id = 1, 
                           int response_timeout_ms = 1000,
                           int byte_timeout_ms = 1000);
    
    /**
     * @brief 연결 풀 설정
     * @param max_connections 최대 동시 연결 수
     * @param connection_reuse_enabled 연결 재사용 활성화
     */
    void ConfigureConnectionPool(int max_connections = 10, 
                                bool connection_reuse_enabled = true);

    // =============================================================================
    // 폴링 그룹 관리
    // =============================================================================
    
    /**
     * @brief 폴링 그룹 추가
     * @param group_name 그룹 이름
     * @param slave_id 슬레이브 ID
     * @param target_ip 대상 IP 주소
     * @param target_port 대상 포트
     * @param register_type 레지스터 타입
     * @param start_address 시작 주소
     * @param register_count 레지스터 개수
     * @param polling_interval_ms 폴링 주기
     * @return 그룹 ID (실패 시 0)
     */
    uint32_t AddPollingGroup(const std::string& group_name,
                            int slave_id,
                            const std::string& target_ip,
                            int target_port,
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
    // 슬레이브 관리
    // =============================================================================
    
    /**
     * @brief 슬레이브 추가
     * @param slave_id 슬레이브 ID
     * @param ip_address IP 주소
     * @param port 포트 번호
     * @return 성공 시 true
     */
    bool AddSlave(int slave_id, const std::string& ip_address, int port = 502);
    
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
    std::shared_ptr<ModbusTcpSlaveInfo> GetSlaveInfo(int slave_id) const;
    
    /**
     * @brief 모든 슬레이브 스캔
     * @param ip_range IP 범위 (예: "192.168.1.1-192.168.1.100")
     * @param port 포트 번호
     * @param timeout_ms 스캔 타임아웃
     * @return 발견된 슬레이브 수
     */
    int ScanSlaves(const std::string& ip_range, int port = 502, int timeout_ms = 2000);

    // =============================================================================
    // 데이터 읽기/쓰기
    // =============================================================================
    
    /**
     * @brief 홀딩 레지스터 읽기
     * @param slave_id 슬레이브 ID
     * @param target_ip 대상 IP
     * @param target_port 대상 포트
     * @param start_address 시작 주소
     * @param register_count 레지스터 개수
     * @param values 읽은 값들 (출력)
     * @return 성공 시 true
     */
    bool ReadHoldingRegisters(int slave_id, const std::string& target_ip, int target_port,
                             uint16_t start_address, uint16_t register_count, 
                             std::vector<uint16_t>& values);
    
    /**
     * @brief 입력 레지스터 읽기
     * @param slave_id 슬레이브 ID
     * @param target_ip 대상 IP
     * @param target_port 대상 포트
     * @param start_address 시작 주소
     * @param register_count 레지스터 개수
     * @param values 읽은 값들 (출력)
     * @return 성공 시 true
     */
    bool ReadInputRegisters(int slave_id, const std::string& target_ip, int target_port,
                           uint16_t start_address, uint16_t register_count, 
                           std::vector<uint16_t>& values);
    
    /**
     * @brief 코일 읽기
     * @param slave_id 슬레이브 ID
     * @param target_ip 대상 IP
     * @param target_port 대상 포트
     * @param start_address 시작 주소
     * @param coil_count 코일 개수
     * @param values 읽은 값들 (출력)
     * @return 성공 시 true
     */
    bool ReadCoils(int slave_id, const std::string& target_ip, int target_port,
                   uint16_t start_address, uint16_t coil_count, 
                   std::vector<bool>& values);
    
    /**
     * @brief 단일 홀딩 레지스터 쓰기
     * @param slave_id 슬레이브 ID
     * @param target_ip 대상 IP
     * @param target_port 대상 포트
     * @param address 주소
     * @param value 쓸 값
     * @return 성공 시 true
     */
    bool WriteSingleRegister(int slave_id, const std::string& target_ip, int target_port,
                            uint16_t address, uint16_t value);
    
    /**
     * @brief 다중 홀딩 레지스터 쓰기
     * @param slave_id 슬레이브 ID
     * @param target_ip 대상 IP
     * @param target_port 대상 포트
     * @param start_address 시작 주소
     * @param values 쓸 값들
     * @return 성공 시 true
     */
    bool WriteMultipleRegisters(int slave_id, const std::string& target_ip, int target_port,
                               uint16_t start_address, const std::vector<uint16_t>& values);

    // =============================================================================
    // 상태 및 통계 조회
    // =============================================================================
    
    /**
     * @brief Modbus TCP 통계 조회
     * @return JSON 형태의 통계 정보
     */
    std::string GetModbusTcpStats() const;
    
    /**
     * @brief 모든 폴링 그룹 상태 조회
     * @return JSON 형태의 그룹 상태
     */
    std::string GetPollingGroupStatus() const;

protected:
    // =============================================================================
    // 멤버 변수들
    // =============================================================================
    
    // Modbus TCP 설정
    int default_slave_id_;
    int response_timeout_ms_;
    int byte_timeout_ms_;
    
    // 연결 풀 설정
    int max_connections_;
    bool connection_reuse_enabled_;
    
    // Modbus 컨텍스트 관리
    std::map<std::string, modbus_t*> connection_pool_;  // "ip:port" -> modbus_t*
    mutable std::mutex connection_mutex_;
    
    // 슬레이브 관리
    std::map<int, std::shared_ptr<ModbusTcpSlaveInfo>> slaves_;
    mutable std::shared_mutex slaves_mutex_;
    
    // 폴링 그룹 관리
    std::map<uint32_t, ModbusTcpPollingGroup> polling_groups_;
    mutable std::shared_mutex polling_groups_mutex_;
    uint32_t next_group_id_;
    
    // 폴링 워커 스레드
    std::thread polling_thread_;
    std::atomic<bool> stop_workers_;
    
    // 통계 (atomic 사용하지 않고 mutex로 보호)
    mutable std::mutex stats_mutex_;
    uint64_t total_reads_;
    uint64_t successful_reads_;
    uint64_t total_writes_;
    uint64_t successful_writes_;
    uint64_t network_errors_;
    uint64_t timeout_errors_;
    uint64_t connection_errors_;

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
    bool ProcessPollingGroup(ModbusTcpPollingGroup& group);
    
    /**
     * @brief Modbus 연결 가져오기 (연결 풀에서)
     * @param ip_address IP 주소
     * @param port 포트
     * @param slave_id 슬레이브 ID
     * @return Modbus 컨텍스트 (실패 시 nullptr)
     */
    modbus_t* GetModbusConnection(const std::string& ip_address, int port, int slave_id);
    
    /**
     * @brief Modbus 연결 반환 (연결 풀로)
     * @param connection_key 연결 키 ("ip:port")
     * @param ctx Modbus 컨텍스트
     */
    void ReturnModbusConnection(const std::string& connection_key, modbus_t* ctx);
    
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
     * @param error_type 에러 타입 ("network", "timeout", "connection")
     */
    void UpdateModbusTcpStats(const std::string& operation, bool success, 
                             const std::string& error_type = "");
    
    /**
     * @brief 슬레이브 응답 시간 업데이트
     * @param slave_id 슬레이브 ID
     * @param response_time_ms 응답 시간
     * @param success 성공 여부
     */
    void UpdateSlaveStatus(int slave_id, int response_time_ms, bool success);
    
    /**
     * @brief Modbus TCP 로그 메시지 출력
     * @param level 로그 레벨
     * @param message 메시지
     */
    void LogModbusTcpMessage(LogLevel level, const std::string& message) const;
    
    /**
     * @brief 폴링 그룹 검증
     * @param slave_id 슬레이브 ID
     * @param target_ip 대상 IP
     * @param target_port 대상 포트
     * @param register_type 레지스터 타입
     * @param start_address 시작 주소
     * @param register_count 레지스터 개수
     * @return 유효한 설정인지 여부
     */
    bool ValidatePollingGroup(int slave_id, const std::string& target_ip, int target_port,
                             ModbusRegisterType register_type,
                             uint16_t start_address, uint16_t register_count) const;
    
    /**
     * @brief IP 주소 범위 파싱
     * @param ip_range IP 범위 문자열
     * @return IP 주소 리스트
     */
    std::vector<std::string> ParseIpRange(const std::string& ip_range) const;
};

} // namespace Protocol
} // namespace Workers
} // namespace PulseOne

#endif // WORKERS_PROTOCOL_MODBUS_TCP_WORKER_H