/**
 * @file ModbusTcpWorker.h (수정됨)
 * @brief Modbus TCP 디바이스 워커 클래스 - ModbusDriver를 통신 매체로 사용
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 2.0.0 (수정됨)
 * 
 * @details
 * 올바른 아키텍처: ModbusDriver가 순수 통신 담당, Worker가 객체 관리 담당
 */

#ifndef MODBUS_TCP_WORKER_H
#define MODBUS_TCP_WORKER_H

#include "Workers/Base/TcpBasedWorker.h"
#include "Common/BasicTypes.h"           // PulseOne::BasicTypes::DataVariant
#include "Common/Enums.h"                // PulseOne::Enums 타입들
#include "Common/Structs.h"              // PulseOne::Structs::DataValue
#include "Drivers/Modbus/ModbusDriver.h"
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <queue>

namespace PulseOne {
namespace Workers {

    // 🔥 타입 별칭 명시적 선언 (충돌 방지의 핵심!)
    using DataValue = PulseOne::Structs::DataValue;          // ✅ 메인 타입
    using TimestampedValue = PulseOne::Structs::TimestampedValue;
    using DataPoint = PulseOne::Structs::DataPoint;
    using DeviceInfo = PulseOne::Structs::DeviceInfo;
    using ErrorInfo = PulseOne::Structs::ErrorInfo;
    using DriverStatistics = PulseOne::Structs::DriverStatistics;
    
    // 열거형 타입들
    using DataQuality = PulseOne::Enums::DataQuality;
    using ConnectionStatus = PulseOne::Enums::ConnectionStatus;
    using ProtocolType = PulseOne::Enums::ProtocolType;
    using WorkerStatus = PulseOne::Enums::WorkerStatus;
    
    // 기본 타입들
    using UUID = PulseOne::BasicTypes::UUID;
    using Timestamp = PulseOne::BasicTypes::Timestamp;

    // Modbus 특화 타입들
    using ModbusDriver = PulseOne::Drivers::ModbusDriver;
    using ModbusRegisterType = PulseOne::Enums::ModbusRegisterType;
/**
 * @brief Modbus 레지스터 타입 (Worker에서 사용)
 */
enum class ModbusRegisterType {
    COIL = 0,              ///< 코일 (0x01, 0x05, 0x0F)
    DISCRETE_INPUT = 1,    ///< 접점 입력 (0x02)
    HOLDING_REGISTER = 2,  ///< 홀딩 레지스터 (0x03, 0x06, 0x10)
    INPUT_REGISTER = 3     ///< 입력 레지스터 (0x04)
};

/**
 * @brief Modbus TCP 폴링 그룹 (Worker가 관리)
 */
struct ModbusTcpPollingGroup {
    uint32_t group_id;                               ///< 그룹 ID
    uint8_t slave_id;                                ///< 슬레이브 ID
    ModbusRegisterType register_type;                ///< 레지스터 타입
    uint16_t start_address;                          ///< 시작 주소
    uint16_t register_count;                         ///< 레지스터 개수
    uint32_t polling_interval_ms;                    ///< 폴링 주기 (밀리초)
    bool enabled;                                    ///< 활성화 여부
    
    std::vector<PulseOne::DataPoint> data_points;     ///< 이 그룹에 속한 데이터 포인트들
    
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
 * @brief Modbus TCP 워커 클래스 (수정됨)
 * @details TcpBasedWorker를 상속받아 ModbusDriver를 통신 매체로 사용
 * 
 * 책임 분리:
 * - ModbusDriver: 순수 Modbus 통신 (연결, 읽기/쓰기, 에러 처리)
 * - ModbusTcpWorker: 객체 관리 (폴링 그룹, 스케줄링, 데이터 변환, DB 저장)
 */
class ModbusTcpWorker : public TcpBasedWorker {
public:
    /**
     * @brief 생성자
     * @param device_info 디바이스 정보
     * @param redis_client Redis 클라이언트 (선택적)
     * @param influx_client InfluxDB 클라이언트 (선택적)
     */
    explicit ModbusTcpWorker(const PulseOne::DeviceInfo& device_info);
    
    /**
     * @brief 소멸자
     */
    virtual ~ModbusTcpWorker();

    // =============================================================================
    // BaseDeviceWorker 인터페이스 구현
    // =============================================================================
    
    std::future<bool> Start() override;
    std::future<bool> Stop() override;

    // =============================================================================
    // TcpBasedWorker 인터페이스 구현 (Driver 위임)
    // =============================================================================
    
    bool EstablishProtocolConnection() override;
    bool CloseProtocolConnection() override;
    bool CheckProtocolConnection() override;
    /**
     * @brief TcpBasedWorker 인터페이스 구현 (Driver 위임)
     * @details SendProtocolKeepAlive는 TcpBasedWorker에서 virtual이므로 override 제거
     * @return 성공 시 true
     */
    bool SendProtocolKeepAlive();

    // =============================================================================
    // Modbus TCP 특화 객체 관리 (Worker 고유 기능)
    // =============================================================================
    
    /**
     * @brief 폴링 그룹 추가
     * @param group 폴링 그룹
     * @return 성공 시 true
     */
    bool AddPollingGroup(const ModbusTcpPollingGroup& group);
    
    /**
     * @brief 폴링 그룹 제거
     * @param group_id 그룹 ID
     * @return 성공 시 true
     */
    bool RemovePollingGroup(uint32_t group_id);
    
    /**
     * @brief 모든 폴링 그룹 조회
     * @return 폴링 그룹 목록
     */
    std::vector<ModbusTcpPollingGroup> GetPollingGroups() const;
    
    /**
     * @brief 폴링 그룹 활성화/비활성화
     * @param group_id 그룹 ID
     * @param enabled 활성화 여부
     * @return 성공 시 true
     */
    bool SetPollingGroupEnabled(uint32_t group_id, bool enabled);
    
    /**
     * @brief Modbus 통계 정보 조회 (Driver에서 가져옴)
     * @return JSON 형태의 통계 정보
     */
    std::string GetModbusStats() const;
    // ==========================================================================
    // 🔥 운영용 쓰기/제어 함수들 (필수!)
    // ==========================================================================
    
    bool WriteSingleHoldingRegister(int slave_id, uint16_t address, uint16_t value);
    bool WriteSingleCoil(int slave_id, uint16_t address, bool value);
    bool WriteMultipleHoldingRegisters(int slave_id, uint16_t start_address, 
                                      const std::vector<uint16_t>& values);
    bool WriteMultipleCoils(int slave_id, uint16_t start_address,
                           const std::vector<bool>& values);

    // ==========================================================================
    // 🔥 디버깅용 개별 읽기 함수들
    // ==========================================================================
    
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

    // ==========================================================================
    // 🔥 고수준 제어 함수들 (DataPoint 기반)
    // ==========================================================================
    
    bool WriteDataPointValue(const std::string& point_id, const DataValue& value);
    bool ReadDataPointValue(const std::string& point_id, TimestampedValue& value);

    
    /**
     * @brief 여러 DataPoint 한번에 읽기 (배치 읽기)
     * @param point_ids 데이터 포인트 ID 목록
     * @param values 읽은 값들 (출력)
     * @return 성공 시 true
     */
    bool ReadMultipleDataPoints(const std::vector<std::string>& point_ids,
                               std::vector<TimestampedValue>& values);

    // ==========================================================================
    // 🔥 실시간 테스트/디버깅 함수들
    // ==========================================================================
    
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
protected:
    // =============================================================================
    // 데이터 포인트 처리 (Worker 고유 로직)
    // =============================================================================
    
    /**
     * @brief 데이터 포인트들을 폴링 그룹으로 자동 구성
     * @param data_points 데이터 포인트 목록
     * @return 생성된 폴링 그룹 수
     */
    size_t CreatePollingGroupsFromDataPoints(const std::vector<PulseOne::DataPoint>& data_points);
    
    /**
     * @brief 폴링 그룹 최적화 (연속된 주소 병합)
     * @return 최적화된 그룹 수
     */
    size_t OptimizePollingGroups();

private:
    // =============================================================================
    // Modbus TCP 전용 멤버 변수
    // =============================================================================
    
    /// Modbus 드라이버 (순수 통신 담당)
    std::unique_ptr<Drivers::ModbusDriver> modbus_driver_;
    
    /// 폴링 그룹 맵 (Group ID → 폴링 그룹)
    std::map<uint32_t, ModbusTcpPollingGroup> polling_groups_;
    
    /// 폴링 그룹 뮤텍스
    mutable std::mutex polling_groups_mutex_;
    
    /// 폴링 스레드
    std::unique_ptr<std::thread> polling_thread_;
    
    /// 폴링 스레드 실행 플래그
    std::atomic<bool> polling_thread_running_;
    
    /// 다음 그룹 ID (자동 증가)
    std::atomic<uint32_t> next_group_id_;
    
    /// 기본 설정
    uint32_t default_polling_interval_ms_;
    uint16_t max_registers_per_group_;
    bool auto_group_creation_enabled_;

    PulseOne::Drivers::ModbusConfig modbus_config_;

    // =============================================================================
    // 내부 메서드 (Worker 고유 로직)
    // =============================================================================
    
    /**
     * @brief Modbus 설정 파싱
     * @details device_info의 protocol_config에서 Modbus 설정 추출
     * @return 성공 시 true
     */
    bool ParseModbusConfig();
    
    /**
     * @brief ModbusDriver 초기화 및 설정
     * @return 성공 시 true
     */
    bool InitializeModbusDriver();
    
    /**
     * @brief 폴링 스레드 함수 (Worker 고유)
     */
    void PollingThreadFunction();
    
    /**
     * @brief 단일 폴링 그룹 처리 (Driver 위임)
     * @param group 폴링 그룹
     * @return 성공 시 true
     */
    bool ProcessPollingGroup(const ModbusTcpPollingGroup& group);
    
    /**
     * @brief Modbus 값들을 TimestampedValue로 변환
     * @param group 폴링 그룹
     * @param values Modbus에서 읽은 원시 값들
     * @return 변환된 TimestampedValue 목록
     */
    std::vector<PulseOne::TimestampedValue> ConvertModbusValues(
        const ModbusTcpPollingGroup& group,
        const std::vector<uint16_t>& values);
      
    /**
     * @brief 데이터 포인트에서 Modbus 주소 파싱
     * @param data_point 데이터 포인트
     * @param slave_id 슬레이브 ID (출력)
     * @param register_type 레지스터 타입 (출력)
     * @param address 주소 (출력)
     * @return 성공 시 true
     */
    bool ParseModbusAddress(const PulseOne::DataPoint& data_point,
                           uint8_t& slave_id,
                           ModbusRegisterType& register_type,
                           uint16_t& address);
    
    /**
     * @brief 폴링 그룹 유효성 검사
     * @param group 폴링 그룹
     * @return 유효하면 true
     */
    bool ValidatePollingGroup(const ModbusTcpPollingGroup& group);
    
    /**
     * @brief 폴링 그룹 병합 가능성 체크
     * @param group1 첫 번째 그룹
     * @param group2 두 번째 그룹
     * @return 병합 가능하면 true
     */
    bool CanMergePollingGroups(const ModbusTcpPollingGroup& group1,
                              const ModbusTcpPollingGroup& group2);
    
    /**
     * @brief 두 폴링 그룹 병합
     * @param group1 첫 번째 그룹
     * @param group2 두 번째 그룹
     * @return 병합된 그룹
     */
    ModbusTcpPollingGroup MergePollingGroups(const ModbusTcpPollingGroup& group1,
                                            const ModbusTcpPollingGroup& group2);

    // =============================================================================
    // ModbusDriver 콜백 메서드들 (Driver → Worker)
    // =============================================================================
    
    /**
     * @brief ModbusDriver 콜백 설정
     * @details Driver에서 Worker로의 콜백 함수들 등록
     */
    void SetupDriverCallbacks();
    
    /**
     * @brief 연결 상태 변경 콜백 (Driver → Worker)
     * @param worker_ptr Worker 포인터
     * @param connected 연결 상태
     * @param error_message 에러 메시지 (연결 실패 시)
     */
    static void OnConnectionStatusChanged(void* worker_ptr, bool connected,
                                         const std::string& error_message);
    
    /**
     * @brief Modbus 에러 콜백 (Driver → Worker)
     * @param worker_ptr Worker 포인터
     * @param slave_id 슬레이브 ID
     * @param function_code 함수 코드
     * @param error_code 에러 코드
     * @param error_message 에러 메시지
     */
    static void OnModbusError(void* worker_ptr, uint8_t slave_id, uint8_t function_code,
                             int error_code, const std::string& error_message);
    
    /**
     * @brief 통계 업데이트 콜백 (Driver → Worker)
     * @param worker_ptr Worker 포인터
     * @param operation 작업 유형 ("read", "write")
     * @param success 성공 여부
     * @param response_time_ms 응답 시간 (밀리초)
     */
    static void OnStatisticsUpdate(void* worker_ptr, const std::string& operation,
                                  bool success, uint32_t response_time_ms);

    /**
     * @brief Modbus 원시 데이터를 TimestampedValue로 변환 후 파이프라인 전송
     * @param raw_values 원시 uint16_t 값들 (Holding/Input Register)
     * @param start_address 시작 주소
     * @param register_type 레지스터 타입 ("holding", "input", "coil", "discrete")
     * @param priority 파이프라인 우선순위 (기본: 0)
     * @return 전송 성공 시 true
     */
    bool SendModbusDataToPipeline(const std::vector<uint16_t>& raw_values, 
                                  uint16_t start_address,
                                  const std::string& register_type,
                                  uint32_t priority = 0);
    
    /**
     * @brief Modbus 원시 bool 데이터를 TimestampedValue로 변환 후 파이프라인 전송
     * @param raw_values 원시 uint8_t 값들 (Coil/Discrete Input)
     * @param start_address 시작 주소
     * @param register_type 레지스터 타입 ("coil", "discrete")
     * @param priority 파이프라인 우선순위 (기본: 0)
     * @return 전송 성공 시 true
     */
    bool SendModbusBoolDataToPipeline(const std::vector<uint8_t>& raw_values,
                                      uint16_t start_address,
                                      const std::string& register_type,
                                      uint32_t priority = 0);
    
    /**
     * @brief TimestampedValue 배열을 직접 파이프라인 전송 (로깅 포함)
     * @param values TimestampedValue 배열
     * @param context 컨텍스트 (로깅용)
     * @param priority 파이프라인 우선순위 (기본: 0)
     * @return 전송 성공 시 true
     */
    bool SendValuesToPipelineWithLogging(const std::vector<TimestampedValue>& values,
                                         const std::string& context,
                                         uint32_t priority = 0);                                  

    // ==========================================================================
    // 🔥 공통 헬퍼 함수들
    // ==========================================================================
    
    /**
     * @brief 쓰기 결과를 파이프라인에 전송 (제어 이력 기록)
     * @param slave_id 슬레이브 ID
     * @param address 주소
     * @param value 쓴 값
     * @param register_type 레지스터 타입
     * @param success 쓰기 성공 여부
     */
    void LogWriteOperation(int slave_id, uint16_t address, const DataValue& value,
                          const std::string& register_type, bool success);
    
    /**
     * @brief DataPoint ID로 실제 DataPoint 찾기
     * @param point_id 포인트 ID
     * @return DataPoint (없으면 빈 optional)
     */
    std::optional<DataPoint> FindDataPointById(const std::string& point_id);
    
    /**
     * @brief 타입별 파이프라인 전송 헬퍼
     */
    bool SendReadResultToPipeline(const std::vector<uint16_t>& values, uint16_t start_address,
                                 const std::string& register_type, int slave_id);
    bool SendReadResultToPipeline(const std::vector<bool>& values, uint16_t start_address,
                                 const std::string& register_type, int slave_id);
    bool SendSingleValueToPipeline(const DataValue& value, uint16_t address,
                                  const std::string& register_type, int slave_id);


};

} // namespace Workers
} // namespace PulseOne

#endif // MODBUS_TCP_WORKER_H