/**
 * @file SerialBasedWorker.h
 * @brief 시리얼 통신 기반 프로토콜들의 공통 기능을 제공하는 중간 클래스
 * @details Modbus RTU, DNP3 Serial 등이 상속받아 사용 (재연결은 BaseDeviceWorker에서 처리)
 * @author PulseOne Development Team
 * @date 2025-01-22
 * @version 1.0.0
 */

#ifndef WORKERS_SERIAL_BASED_WORKER_H
#define WORKERS_SERIAL_BASED_WORKER_H

#include "Workers/Base/BaseDeviceWorker.h"
#include <string>
#include <atomic>
#include <chrono>
#include <memory>
#include <termios.h>

namespace PulseOne {
namespace Workers {

/**
 * @brief 시리얼 포트 설정
 */
struct SerialConfig {
    std::string port_name;           ///< 포트 이름 ("/dev/ttyUSB0", "COM1" 등)
    int baud_rate;                   ///< 보드레이트 (9600, 19200, 38400, 115200 등)
    int data_bits;                   ///< 데이터 비트 (7, 8)
    int stop_bits;                   ///< 스톱 비트 (1, 2)
    char parity;                     ///< 패리티 ('N'=None, 'E'=Even, 'O'=Odd)
    bool hardware_flow_control;      ///< 하드웨어 흐름 제어 (RTS/CTS)
    bool software_flow_control;      ///< 소프트웨어 흐름 제어 (XON/XOFF)
    int read_timeout_ms;             ///< 읽기 타임아웃 (밀리초)
    int write_timeout_ms;            ///< 쓰기 타임아웃 (밀리초)
    
    SerialConfig() 
        : port_name("/dev/ttyUSB0")
        , baud_rate(9600)
        , data_bits(8)
        , stop_bits(1)
        , parity('N')
        , hardware_flow_control(false)
        , software_flow_control(false)
        , read_timeout_ms(1000)
        , write_timeout_ms(1000) {}
};

/**
 * @brief 시리얼 통신 기반 프로토콜 워커의 기반 클래스
 * @details BaseDeviceWorker의 완전한 재연결/Keep-alive 시스템 활용
 *          시리얼 포트 관리만 담당하는 명확한 역할 분담
 */
class SerialBasedWorker : public BaseDeviceWorker {
public:
    /**
     * @brief 생성자 (BaseDeviceWorker와 일치)
     * @param device_info 디바이스 정보
     * @param redis_client Redis 클라이언트
     * @param influx_client InfluxDB 클라이언트
     */
    SerialBasedWorker(const PulseOne::DeviceInfo& device_info,
                      std::shared_ptr<RedisClient> redis_client,
                      std::shared_ptr<InfluxClient> influx_client);
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~SerialBasedWorker();
    
    // =============================================================================
    // 시리얼 설정 관리 (공개 인터페이스)
    // =============================================================================
    
    /**
     * @brief 시리얼 포트 설정
     * @param config 시리얼 설정
     */
    void ConfigureSerial(const SerialConfig& config);
    
    /**
     * @brief 시리얼 연결 정보 조회
     * @return JSON 형태의 연결 정보
     */
    std::string GetSerialConnectionInfo() const;
    
    /**
     * @brief 시리얼 통계 정보 조회
     * @return JSON 형태의 시리얼 통계 + BaseDeviceWorker 통계
     */
    std::string GetSerialStats() const;
    
    // =============================================================================
    // BaseDeviceWorker 순수 가상 함수 구현 (시리얼 특화)
    // =============================================================================
    
    /**
     * @brief 시리얼 기반 연결 수립
     * @details 시리얼 포트 열기 → 프로토콜 연결 순서로 진행
     * @return 성공 시 true
     */
    bool EstablishConnection() override;
    
    /**
     * @brief 시리얼 기반 연결 해제
     * @details 프로토콜 해제 → 시리얼 포트 해제 순서로 진행
     * @return 성공 시 true
     */
    bool CloseConnection() override;
    
    /**
     * @brief 시리얼 기반 연결 상태 확인
     * @details 시리얼 포트 상태 + 프로토콜 상태 모두 확인
     * @return 연결 상태
     */
    bool CheckConnection() override;
    
    /**
     * @brief 시리얼 기반 Keep-alive 전송
     * @details 시리얼 포트 확인 → 프로토콜별 Keep-alive 호출
     * @return 성공 시 true
     */
    bool SendKeepAlive() override;

protected:
    // =============================================================================
    // 파생 클래스에서 구현해야 하는 프로토콜별 메서드들 (순수 가상)
    // =============================================================================
    
    /**
     * @brief 프로토콜별 연결 수립 (Modbus RTU, DNP3 등에서 구현)
     * @details 시리얼 포트가 이미 열린 상태에서 호출됨
     * @return 성공 시 true
     */
    virtual bool EstablishProtocolConnection() = 0;
    
    /**
     * @brief 프로토콜별 연결 해제 (Modbus RTU, DNP3 등에서 구현)
     * @details 시리얼 포트 해제 전에 호출됨
     * @return 성공 시 true
     */
    virtual bool CloseProtocolConnection() = 0;
    
    /**
     * @brief 프로토콜별 연결 상태 확인 (Modbus RTU, DNP3 등에서 구현)
     * @details 시리얼 포트가 열린 상태에서 호출됨
     * @return 연결 상태
     */
    virtual bool CheckProtocolConnection() = 0;
    
    /**
     * @brief 프로토콜별 Keep-alive 전송 (파생 클래스에서 선택적 구현)
     * @details 시리얼 포트가 열린 상태에서 호출됨
     * @return 성공 시 true
     */
    virtual bool SendProtocolKeepAlive();
    
    // =============================================================================
    // 시리얼 포트 관리 (파생 클래스에서 사용 가능)
    // =============================================================================
    
    /**
     * @brief 시리얼 설정 검증
     * @return 유효한 설정인 경우 true
     */
    bool ValidateSerialConfig() const;
    
    /**
     * @brief 시리얼 포트 열기
     * @return 성공 시 true
     */
    bool OpenSerialPort();
    
    /**
     * @brief 시리얼 포트 닫기
     */
    void CloseSerialPort();
    
    /**
     * @brief 시리얼 포트 연결 상태 확인
     * @return 연결된 경우 true
     */
    bool IsSerialPortOpen() const;
    
    /**
     * @brief 시리얼 데이터 전송
     * @param data 전송할 데이터
     * @param length 데이터 길이
     * @return 전송된 바이트 수 (실패 시 -1)
     */
    ssize_t WriteSerialData(const void* data, size_t length);
    
    /**
     * @brief 시리얼 데이터 수신
     * @param buffer 수신 버퍼
     * @param buffer_size 버퍼 크기
     * @return 수신된 바이트 수 (실패 시 -1, 타임아웃 시 0)
     */
    ssize_t ReadSerialData(void* buffer, size_t buffer_size);
    
    /**
     * @brief 시리얼 포트 플러시 (버퍼 비우기)
     * @param flush_input 입력 버퍼 플러시 여부
     * @param flush_output 출력 버퍼 플러시 여부
     * @return 성공 시 true
     */
    bool FlushSerialPort(bool flush_input = true, bool flush_output = true);
    
    /**
     * @brief 파일 디스크립터 조회
     * @return 시리얼 포트 FD (열리지 않은 경우 -1)
     */
    int GetSerialFd() const;
    
    // =============================================================================
    // 파생 클래스에서 사용할 수 있는 보호된 멤버들
    // =============================================================================
    
    SerialConfig serial_config_;                          ///< 시리얼 설정
    int serial_fd_;                                       ///< 시리얼 포트 파일 디스크립터
    struct termios original_termios_;                     ///< 원본 터미널 설정 (복구용)

private:
    // =============================================================================
    // 내부 유틸리티 메서드들
    // =============================================================================
    
    /**
     * @brief endpoint 문자열에서 시리얼 설정 파싱
     */
    void ParseEndpoint();
    
    /**
     * @brief 보드레이트를 termios 상수로 변환
     * @param baud_rate 보드레이트
     * @return termios 보드레이트 상수
     */
    speed_t BaudRateToSpeed(int baud_rate) const;
    
    /**
     * @brief termios 구조체 설정
     * @param tty 설정할 termios 구조체
     * @return 성공 시 true
     */
    bool ConfigureTermios(struct termios& tty) const;
};

// =============================================================================
// 헬퍼 함수들 (네임스페이스 내)
// =============================================================================

/**
 * @brief 시리얼 포트 유효성 검사
 * @param port_name 포트 이름
 * @return 유효한 경우 true
 */
inline bool ValidateSerialPort(const std::string& port_name) {
    if (port_name.empty()) {
        return false;
    }
    
    // Linux/Unix 시리얼 포트 패턴 검사
    if (port_name.find("/dev/tty") == 0 || 
        port_name.find("/dev/serial") == 0) {
        return true;
    }
    
    // Windows COM 포트 패턴 검사
    if (port_name.find("COM") == 0) {
        return true;
    }
    
    return false;
}

/**
 * @brief 보드레이트 유효성 검사
 * @param baud_rate 보드레이트
 * @return 지원되는 보드레이트인 경우 true
 */
inline bool ValidateBaudRate(int baud_rate) {
    switch (baud_rate) {
        case 1200:
        case 2400:
        case 4800:
        case 9600:
        case 19200:
        case 38400:
        case 57600:
        case 115200:
        case 230400:
        case 460800:
        case 921600:
            return true;
        default:
            return false;
    }
}

} // namespace Workers
} // namespace PulseOne

#endif // WORKERS_SERIAL_BASED_WORKER_H