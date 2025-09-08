// =============================================================================
// collector/include/Workers/Base/TcpBasedWorker.h - Windows 크로스 컴파일 완전 대응
// =============================================================================

#ifndef WORKERS_TCP_BASED_WORKER_H
#define WORKERS_TCP_BASED_WORKER_H

/**
 * @file TcpBasedWorker.h - 크로스 플랫폼 TCP 통신 워커
 * @brief Windows/Linux TCP 소켓 지원 기반 워커 클래스
 * @author PulseOne Development Team
 * @date 2025-09-08
 * @version 8.0.0 - Windows 크로스 컴파일 완전 대응
 */

// =============================================================================
// 플랫폼 호환성 헤더 (가장 먼저!)
// =============================================================================
#include "Platform/PlatformCompat.h"

// =============================================================================
// 시스템 헤더들 (플랫폼별 조건부)
// =============================================================================
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <future>

#if PULSEONE_WINDOWS
    // Windows 소켓 지원
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    
    // Windows 소켓 상수들
    #define TCP_INVALID_SOCKET INVALID_SOCKET
    #define TCP_SOCKET_ERROR SOCKET_ERROR
    typedef SOCKET TcpSocket;
    typedef int socklen_t;
    
    // Windows 전용 에러 매크로
    #define GET_SOCKET_ERROR() WSAGetLastError()
    #define CLOSE_SOCKET(s) closesocket(s)
    
#else
    // Linux/Unix 소켓 지원
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <cstring>
    #include <cerrno>
    
    typedef int TcpSocket;
    
    #define TCP_INVALID_SOCKET -1
    #define TCP_SOCKET_ERROR -1
    #define GET_SOCKET_ERROR() errno
    #define CLOSE_SOCKET(s) close(s)
#endif

// =============================================================================
// PulseOne 헤더들
// =============================================================================
#include "Workers/Base/BaseDeviceWorker.h"

namespace PulseOne {
namespace Workers {

/**
 * @brief 크로스 플랫폼 TCP 연결 설정
 */
struct TcpConfig {
    std::string ip_address;              ///< IP 주소 ("192.168.1.100", "localhost" 등)
    uint16_t port;                       ///< 포트 번호 (502, 1883, 47808 등)
    int connection_timeout_seconds;       ///< 연결 타임아웃 (초)
    int send_timeout_seconds;            ///< 송신 타임아웃 (초)
    int receive_timeout_seconds;         ///< 수신 타임아웃 (초)
    bool keep_alive_enabled;             ///< TCP Keep-alive 사용 여부
    bool no_delay_enabled;               ///< TCP_NODELAY 사용 여부 (Nagle 알고리즘 비활성화)
    int send_buffer_size;                ///< 송신 버퍼 크기 (바이트)
    int receive_buffer_size;             ///< 수신 버퍼 크기 (바이트)
    
    TcpConfig() 
        : ip_address("127.0.0.1")
        , port(502)
        , connection_timeout_seconds(5)
        , send_timeout_seconds(30)
        , receive_timeout_seconds(30)
        , keep_alive_enabled(true)
        , no_delay_enabled(true)
        , send_buffer_size(8192)
        , receive_buffer_size(8192) {}
};

/**
 * @brief TCP 기반 프로토콜 워커의 기반 클래스
 * @details BaseDeviceWorker의 완전한 재연결/Keep-alive 시스템 활용
 *          TCP 연결 관리만 담당하는 명확한 역할 분담
 */
class TcpBasedWorker : public BaseDeviceWorker {
public:
    /**
     * @brief 생성자 (BaseDeviceWorker와 일치)
     * @param device_info 디바이스 정보
     */
    TcpBasedWorker(const PulseOne::Structs::DeviceInfo& device_info);
    
    /**
     * @brief 가상 소멸자
     */
    virtual ~TcpBasedWorker();
    
    // =============================================================================
    // TCP 설정 관리 (공개 인터페이스)
    // =============================================================================
    
    /**
     * @brief TCP 연결 설정
     * @param ip IP 주소
     * @param port 포트 번호
     * @param timeout_seconds 연결 타임아웃 (초)
     */
    void ConfigureTcpConnection(const std::string& ip, 
                               uint16_t port,
                               int timeout_seconds = 5);
    
    /**
     * @brief TCP 연결 설정 (고급)
     * @param config TCP 설정
     */
    void ConfigureTcp(const TcpConfig& config);
    
    /**
     * @brief TCP 연결 정보 조회
     * @return JSON 형태의 연결 정보
     */
    std::string GetTcpConnectionInfo() const;
    
    /**
     * @brief TCP 통계 정보 조회
     * @return JSON 형태의 TCP 통계 + BaseDeviceWorker 통계
     */
    std::string GetTcpStats() const;
    
    // =============================================================================
    // BaseDeviceWorker 순수 가상 함수 구현 (TCP 특화)
    // =============================================================================
    
    /**
     * @brief TCP 기반 연결 수립
     * @details TCP 소켓 생성 → 프로토콜 연결 순서로 진행
     * @return 성공 시 true
     */
    bool EstablishConnection() override;
    
    /**
     * @brief TCP 기반 연결 해제
     * @details 프로토콜 해제 → TCP 소켓 해제 순서로 진행
     * @return 성공 시 true
     */
    bool CloseConnection() override;
    
    /**
     * @brief TCP 기반 연결 상태 확인
     * @details TCP 소켓 상태 + 프로토콜 상태 모두 확인
     * @return 연결 상태
     */
    bool CheckConnection() override;
    
    /**
     * @brief TCP 기반 Keep-alive 전송
     * @details TCP 소켓 확인 → 프로토콜별 Keep-alive 호출
     * @return 성공 시 true
     */
    bool SendKeepAlive() override;

protected:
    // =============================================================================
    // 파생 클래스에서 구현해야 하는 프로토콜별 메서드들 (순수 가상)
    // =============================================================================
    
    /**
     * @brief 프로토콜별 연결 수립 (Modbus TCP, MQTT 등에서 구현)
     * @details TCP 소켓이 이미 연결된 상태에서 호출됨
     * @return 성공 시 true
     */
    virtual bool EstablishProtocolConnection() = 0;
    
    /**
     * @brief 프로토콜별 연결 해제 (Modbus TCP, MQTT 등에서 구현)
     * @details TCP 소켓 해제 전에 호출됨
     * @return 성공 시 true
     */
    virtual bool CloseProtocolConnection() = 0;
    
    /**
     * @brief 프로토콜별 연결 상태 확인 (Modbus TCP, MQTT 등에서 구현)
     * @details TCP 소켓이 연결된 상태에서 호출됨
     * @return 연결 상태
     */
    virtual bool CheckProtocolConnection() = 0;
    
    /**
     * @brief 프로토콜별 Keep-alive 전송 (파생 클래스에서 선택적 구현)
     * @details TCP 소켓이 연결된 상태에서 호출됨
     * @return 성공 시 true
     */
    virtual bool SendProtocolKeepAlive();
    
    // =============================================================================
    // 크로스 플랫폼 TCP 소켓 관리 (파생 클래스에서 사용 가능)
    // =============================================================================
    
    /**
     * @brief TCP 설정 검증
     * @return 유효한 설정인 경우 true
     */
    bool ValidateTcpConfig() const;
    
    /**
     * @brief TCP 소켓 생성 및 연결 (크로스 플랫폼)
     * @return 성공 시 true
     */
    bool CreateTcpSocket();
    
    /**
     * @brief TCP 소켓 닫기 (크로스 플랫폼)
     */
    void CloseTcpSocket();
    
    /**
     * @brief TCP 소켓 연결 상태 확인 (크로스 플랫폼)
     * @return 연결된 경우 true
     */
    bool IsTcpSocketConnected() const;
    
    /**
     * @brief TCP 데이터 전송 (크로스 플랫폼)
     * @param data 전송할 데이터
     * @param length 데이터 길이
     * @return 전송된 바이트 수 (실패 시 -1)
     */
    ssize_t SendTcpData(const void* data, size_t length);
    
    /**
     * @brief TCP 데이터 수신 (크로스 플랫폼)
     * @param buffer 수신 버퍼
     * @param buffer_size 버퍼 크기
     * @return 수신된 바이트 수 (실패 시 -1, 타임아웃 시 0)
     */
    ssize_t ReceiveTcpData(void* buffer, size_t buffer_size);
    
    /**
     * @brief 플랫폼별 소켓 핸들 조회
     * @return TCP 소켓 핸들 (연결되지 않은 경우 TCP_INVALID_SOCKET)
     */
    TcpSocket GetTcpSocket() const;
    
    /**
     * @brief 소켓 옵션 설정 (크로스 플랫폼)
     * @return 성공 시 true
     */
    bool ConfigureSocketOptions();
    
    // =============================================================================
    // 파생 클래스에서 사용할 수 있는 보호된 멤버들
    // =============================================================================
    
    TcpConfig tcp_config_;                                ///< TCP 설정
    TcpSocket tcp_socket_;                                ///< 크로스 플랫폼 TCP 소켓

#if PULSEONE_WINDOWS
    bool winsock_initialized_;                            ///< Winsock 초기화 여부
#endif

private:
    // =============================================================================
    // 내부 유틸리티 메서드들 (크로스 플랫폼)
    // =============================================================================
    
    /**
     * @brief endpoint 문자열에서 TCP 설정 파싱
     */
    void ParseEndpoint();
    
#if PULSEONE_WINDOWS
    /**
     * @brief Winsock 초기화
     * @return 성공 시 true
     */
    bool InitializeWinsock();
    
    /**
     * @brief Winsock 정리
     */
    void CleanupWinsock();
    
    /**
     * @brief Windows 소켓 에러 메시지 변환
     * @param error_code WSA 에러 코드
     * @return 에러 메시지
     */
    std::string WinsockErrorToString(int error_code) const;
    
#else
    /**
     * @brief Unix 소켓 에러 메시지 변환
     * @param error_code errno 값
     * @return 에러 메시지
     */
    std::string UnixSocketErrorToString(int error_code) const;
    
    /**
     * @brief 소켓을 논블로킹 모드로 설정
     * @param socket 소켓
     * @return 성공 시 true
     */
    bool SetSocketNonBlocking(TcpSocket socket);
#endif

    /**
     * @brief 플랫폼별 에러 메시지 조회
     * @return 에러 메시지
     */
    std::string GetLastSocketError() const;
};

} // namespace Workers
} // namespace PulseOne

#endif // WORKERS_TCP_BASED_WORKER_H