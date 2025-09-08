/**
 * @file TcpBasedWorker.h - 구현부와 100% 일치하도록 완전 수정
 * @brief TCP 기반 프로토콜 워커 헤더 - 컴파일 에러 완전 해결
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 2.0.0 - 구현부 완전 동기화
 */

#ifndef WORKERS_TCP_BASED_WORKER_H
#define WORKERS_TCP_BASED_WORKER_H

// =============================================================================
// UUID 충돌 방지 (가장 먼저!)
// =============================================================================
#ifdef _WIN32
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
#endif

// =============================================================================
// 시스템 헤더들
// =============================================================================
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <future>

// =============================================================================
// 플랫폼별 네트워크 헤더 (조건부)
// =============================================================================
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    
    typedef SOCKET TcpSocket;
    typedef int socklen_t;
    #define TCP_INVALID_SOCKET INVALID_SOCKET
    #define TCP_SOCKET_ERROR SOCKET_ERROR
#else
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
#endif

// =============================================================================
// PulseOne 헤더들
// =============================================================================
#include "Workers/Base/BaseDeviceWorker.h"

namespace PulseOne {
namespace Workers {

/**
 * @brief TCP 연결 설정 구조체
 */
struct TcpConfig {
    std::string ip_address;              ///< IP 주소
    uint16_t port;                       ///< 포트 번호
    int connection_timeout_seconds;       ///< 연결 타임아웃 (초)
    int send_timeout_seconds;            ///< 송신 타임아웃 (초)
    int receive_timeout_seconds;         ///< 수신 타임아웃 (초)
    bool keep_alive_enabled;             ///< TCP Keep-alive 사용 여부
    bool no_delay_enabled;               ///< TCP_NODELAY 사용 여부
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
 * @details BaseDeviceWorker를 상속받아 TCP 연결 관리 기능 제공
 */
class TcpBasedWorker : public BaseDeviceWorker {
public:
    // =============================================================================
    // 생성자 및 소멸자 (구현부와 정확히 일치)
    // =============================================================================
    
    /**
     * @brief 생성자
     * @param device_info 디바이스 정보 (구현부와 타입 일치)
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
     * @return 성공 시 true (기본 구현 제공)
     */
    virtual bool SendProtocolKeepAlive();
    
    // =============================================================================
    // TCP 소켓 관리 (파생 클래스에서 사용 가능)
    // =============================================================================
    
    /**
     * @brief TCP 설정 검증
     * @return 유효한 설정인 경우 true
     */
    bool ValidateTcpConfig() const;
    
    /**
     * @brief TCP 소켓 생성 및 연결
     * @return 성공 시 true
     */
    bool CreateTcpSocket();
    
    /**
     * @brief TCP 소켓 닫기
     */
    void CloseTcpSocket();
    
    /**
     * @brief TCP 소켓 유효성 확인 (구현부 함수명과 일치)
     * @return 유효한 소켓인 경우 true
     */
    bool IsTcpSocketValid() const;
    
    /**
     * @brief TCP 소켓 연결 상태 확인
     * @return 연결된 경우 true
     */
    bool IsTcpSocketConnected() const;

private:
    // =============================================================================
    // 내부 멤버 변수들 (구현부와 완전 일치)
    // =============================================================================
    
    TcpSocket tcp_socket_;              ///< TCP 소켓 핸들 (플랫폼별)
    TcpConfig tcp_config_;              ///< TCP 설정
    
#ifdef _WIN32
    bool winsock_initialized_;          ///< Windows Winsock 초기화 상태
#endif

    // =============================================================================
    // 내부 헬퍼 메서드들 (구현부에 있는 모든 함수들)
    // =============================================================================
    
    /**
     * @brief 소켓 옵션 설정
     * @return 성공 시 true
     */
    bool SetSocketOptions();
    
    /**
     * @brief 서버에 연결
     * @return 성공 시 true
     */
    bool ConnectToServer();
    
    /**
     * @brief 연결 완료 대기
     * @return 성공 시 true
     */
    bool WaitForConnection();
    
    /**
     * @brief 소켓을 논블로킹/블로킹 모드로 설정
     * @param non_blocking true면 논블로킹, false면 블로킹
     */
    void SetSocketNonBlocking(bool non_blocking);
    
    /**
     * @brief 소켓 타임아웃 설정
     */
    void SetSocketTimeouts();
    
    /**
     * @brief endpoint 파싱 (ip:port 형태)
     */
    void ParseEndpoint();

#ifdef _WIN32
    /**
     * @brief Windows Winsock 초기화
     * @return 성공 시 true
     */
    bool InitializeWinsock();
    
    /**
     * @brief Windows Winsock 정리
     */
    void CleanupWinsock();
#endif
};

} // namespace Workers
} // namespace PulseOne

#endif // WORKERS_TCP_BASED_WORKER_H