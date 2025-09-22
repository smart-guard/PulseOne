/**
 * @file TcpBasedWorker.cpp - 크로스 플랫폼 완전 호환 버전
 * @brief TCP 기반 프로토콜 워커 구현 - Windows/Linux 완전 지원
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 3.0.0 - 크로스 플랫폼 완성
 */

// =============================================================================
// 플랫폼 감지 및 전처리 정의
// =============================================================================
#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    // PulseOne UniqueId와 Windows UniqueId 충돌 방지
    #define UniqueId _WIN32_UniqueId_BACKUP
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #undef UniqueId
    
    // Windows 타입 정의
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
    #define CLOSE_SOCKET(s) closesocket(s)
    #define GET_SOCKET_ERROR() WSAGetLastError()
    #define SOCKET_WOULD_BLOCK WSAEWOULDBLOCK
    #define SOCKET_IN_PROGRESS WSAEINPROGRESS
    
#else
    // Linux/Unix 헤더
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/select.h>
    #include <poll.h>
    #include <netdb.h>
    #include <cstring>
    #include <cerrno>
    
    // Linux 타입 정의
    typedef int socket_t;
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
    #define CLOSE_SOCKET(s) close(s)
    #define GET_SOCKET_ERROR() errno
    #define SOCKET_WOULD_BLOCK EAGAIN
    #define SOCKET_IN_PROGRESS EINPROGRESS
#endif

#include "Workers/Base/TcpBasedWorker.h"
#include "Utils/LogManager.h"
#include "Common/Enums.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>

using namespace std::chrono;
using LogLevel = PulseOne::Enums::LogLevel;

namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

TcpBasedWorker::TcpBasedWorker(const PulseOne::Structs::DeviceInfo& device_info)
    : BaseDeviceWorker(device_info)
    , tcp_socket_(INVALID_SOCKET_VALUE)
#ifdef _WIN32
    , winsock_initialized_(false)
#endif
{
    // endpoint에서 TCP 설정 파싱
    ParseEndpoint();
    
#ifdef _WIN32
    // Windows에서 Winsock 초기화
    if (!InitializeWinsock()) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to initialize Winsock");
        return;
    }
#endif
    
    // BaseDeviceWorker 재연결 설정 활용
    ReconnectionSettings settings;
    settings.auto_reconnect_enabled = true;
    settings.retry_interval_ms = 5000;
    settings.max_retries_per_cycle = 10;
    settings.keep_alive_enabled = true;
    settings.keep_alive_interval_seconds = 60;
    UpdateReconnectionSettings(settings);
    
    LogMessage(LogLevel::INFO, "TcpBasedWorker created for device: " + device_info.name);
}

TcpBasedWorker::~TcpBasedWorker() {
    CloseTcpSocket();
    
#ifdef _WIN32
    CleanupWinsock();
#endif
    
    LogMessage(LogLevel::INFO, "TcpBasedWorker destroyed");
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현 (TCP 특화)
// =============================================================================

bool TcpBasedWorker::EstablishConnection() {
    LogMessage(LogLevel::INFO, "Establishing TCP connection to " + 
               tcp_config_.ip_address + ":" + std::to_string(tcp_config_.port));
    
    // TCP 설정 검증
    if (!ValidateTcpConfig()) {
        LogMessage(LogLevel::LOG_ERROR, "Invalid TCP configuration");
        return false;
    }
    
    // TCP 소켓 생성 및 연결
    if (!CreateTcpSocket()) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to create TCP socket");
        return false;
    }
    
    // 파생 클래스의 프로토콜별 연결 수립
    if (!EstablishProtocolConnection()) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to establish protocol connection");
        CloseTcpSocket();
        return false;
    }
    
    LogMessage(LogLevel::INFO, "TCP connection established successfully");
    return true;
}

bool TcpBasedWorker::CloseConnection() {
    LogMessage(LogLevel::INFO, "Closing TCP connection");
    
    // 프로토콜별 연결 해제 먼저
    CloseProtocolConnection();
    
    // TCP 소켓 해제
    CloseTcpSocket();
    
    LogMessage(LogLevel::INFO, "TCP connection closed");
    return true;
}

bool TcpBasedWorker::CheckConnection() {
    // TCP 소켓 상태 확인
    if (!IsTcpSocketConnected()) {
        return false;
    }
    
    // 파생 클래스의 프로토콜별 연결 확인
    return CheckProtocolConnection();
}

bool TcpBasedWorker::SendKeepAlive() {
    if (!IsTcpSocketConnected()) {
        LogMessage(LogLevel::WARN, "TCP socket not connected for keep-alive");
        return false;
    }
    
    // 파생 클래스의 프로토콜별 Keep-alive 전송
    bool protocol_result = SendProtocolKeepAlive();
    
    if (protocol_result) {
        LogMessage(LogLevel::DEBUG_LEVEL, "TCP keep-alive sent successfully");
    } else {
        LogMessage(LogLevel::WARN, "TCP keep-alive failed");
    }
    
    return protocol_result;
}

// =============================================================================
// TCP 설정 관리
// =============================================================================

void TcpBasedWorker::ConfigureTcpConnection(const std::string& ip, 
                                           uint16_t port,
                                           int timeout_seconds) {
    tcp_config_.ip_address = ip;
    tcp_config_.port = port;
    tcp_config_.connection_timeout_seconds = timeout_seconds;
    
    LogMessage(LogLevel::INFO, "TCP configuration updated: " + ip + ":" + std::to_string(port));
}

void TcpBasedWorker::ConfigureTcp(const TcpConfig& config) {
    tcp_config_ = config;
    
    LogMessage(LogLevel::INFO, "TCP configuration updated with advanced settings");
}

std::string TcpBasedWorker::GetTcpConnectionInfo() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"ip_address\": \"" << tcp_config_.ip_address << "\",\n";
    oss << "  \"port\": " << tcp_config_.port << ",\n";
    oss << "  \"connection_timeout_seconds\": " << tcp_config_.connection_timeout_seconds << ",\n";
    oss << "  \"keep_alive_enabled\": " << (tcp_config_.keep_alive_enabled ? "true" : "false") << ",\n";
    oss << "  \"is_connected\": " << (IsTcpSocketConnected() ? "true" : "false") << "\n";
    oss << "}";
    return oss.str();
}

// =============================================================================
// 내부 TCP 소켓 관리
// =============================================================================

bool TcpBasedWorker::ValidateTcpConfig() const {
    if (tcp_config_.ip_address.empty()) {
        LogMessage(LogLevel::LOG_ERROR, "IP address is empty");
        return false;
    }
    
    if (tcp_config_.port == 0) {
        LogMessage(LogLevel::LOG_ERROR, "Invalid port: " + std::to_string(tcp_config_.port));
        return false;
    }
    
    if (tcp_config_.connection_timeout_seconds <= 0) {
        LogMessage(LogLevel::LOG_ERROR, "Invalid connection timeout");
        return false;
    }
    
    return true;
}

bool TcpBasedWorker::CreateTcpSocket() {
    // 기존 소켓이 있다면 먼저 닫기
    if (IsTcpSocketValid()) {
        CloseTcpSocket();
    }
    
    // 소켓 생성
    tcp_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_socket_ == INVALID_SOCKET_VALUE) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to create socket: " + GetLastSocketErrorString());
        return false;
    }
    
    // 소켓 옵션 설정
    if (!ConfigureSocketOptions()) {
        CloseTcpSocket();
        return false;
    }
    
    // 연결 시도
    if (!ConnectToServer()) {
        CloseTcpSocket();
        return false;
    }
    
    LogMessage(LogLevel::INFO, "TCP socket connected successfully");
    return true;
}

bool TcpBasedWorker::ConfigureSocketOptions() {
    // SO_REUSEADDR 설정
    int reuse = 1;
#ifdef _WIN32
    if (setsockopt(tcp_socket_, SOL_SOCKET, SO_REUSEADDR, 
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse)) == SOCKET_ERROR_VALUE) {
        LogMessage(LogLevel::WARN, "Failed to set SO_REUSEADDR: " + GetLastSocketErrorString());
    }
#else
    if (setsockopt(tcp_socket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        LogMessage(LogLevel::WARN, "Failed to set SO_REUSEADDR: " + GetLastSocketErrorString());
    }
#endif
    
    // TCP_NODELAY 설정 (Nagle 알고리즘 비활성화)
    if (tcp_config_.no_delay_enabled) {
        int nodelay = 1;
#ifdef _WIN32
        if (setsockopt(tcp_socket_, IPPROTO_TCP, TCP_NODELAY,
                       reinterpret_cast<const char*>(&nodelay), sizeof(nodelay)) == SOCKET_ERROR_VALUE) {
            LogMessage(LogLevel::WARN, "Failed to set TCP_NODELAY: " + GetLastSocketErrorString());
        }
#else
        if (setsockopt(tcp_socket_, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) < 0) {
            LogMessage(LogLevel::WARN, "Failed to set TCP_NODELAY: " + GetLastSocketErrorString());
        }
#endif
    }
    
    // 송수신 타임아웃 설정
    SetSocketTimeouts();
    
    return true;
}

bool TcpBasedWorker::ConnectToServer() {
    // 서버 주소 구조체 설정
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(tcp_config_.port);
    
    // IP 주소 변환 (크로스 플랫폼)
#ifdef _WIN32
    // Windows: inet_addr 사용 (MinGW 호환)
    server_addr.sin_addr.s_addr = inet_addr(tcp_config_.ip_address.c_str());
    if (server_addr.sin_addr.s_addr == INADDR_NONE) {
        LogMessage(LogLevel::LOG_ERROR, "Invalid IP address: " + tcp_config_.ip_address);
        return false;
    }
#else
    // Linux: inet_pton 사용
    if (inet_pton(AF_INET, tcp_config_.ip_address.c_str(), &server_addr.sin_addr) != 1) {
        LogMessage(LogLevel::LOG_ERROR, "Invalid IP address: " + tcp_config_.ip_address);
        return false;
    }
#endif
    
    // 논블로킹 모드로 설정 (타임아웃 구현용)
    SetSocketNonBlocking(true);
    
    // 연결 시도
    int result = connect(tcp_socket_, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr));
    
#ifdef _WIN32
    if (result == SOCKET_ERROR_VALUE) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            LogMessage(LogLevel::LOG_ERROR, "Connect failed immediately: " + SocketErrorToString(error));
            return false;
        }
    }
#else
    if (result < 0) {
        if (errno != EINPROGRESS) {
            LogMessage(LogLevel::LOG_ERROR, "Connect failed immediately: " + SocketErrorToString(errno));
            return false;
        }
    }
#endif
    
    // 연결 완료 대기
    if (!WaitForConnection()) {
        return false;
    }
    
    // 블로킹 모드로 복원
    SetSocketNonBlocking(false);
    
    LogMessage(LogLevel::INFO, "Connected to " + tcp_config_.ip_address + ":" + std::to_string(tcp_config_.port));
    return true;
}

bool TcpBasedWorker::WaitForConnection() {
    // select를 사용하여 연결 완료 대기
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(tcp_socket_, &write_fds);
    
    struct timeval timeout;
    timeout.tv_sec = tcp_config_.connection_timeout_seconds;
    timeout.tv_usec = 0;
    
#ifdef _WIN32
    int result = select(0, nullptr, &write_fds, nullptr, &timeout);
    if (result == SOCKET_ERROR_VALUE) {
        LogMessage(LogLevel::LOG_ERROR, "Select failed: " + GetLastSocketErrorString());
        return false;
    }
#else
    int result = select(tcp_socket_ + 1, nullptr, &write_fds, nullptr, &timeout);
    if (result < 0) {
        LogMessage(LogLevel::LOG_ERROR, "Select failed: " + GetLastSocketErrorString());
        return false;
    }
#endif
    
    if (result == 0) {
        LogMessage(LogLevel::LOG_ERROR, "Connection timeout");
        return false;
    }
    
    // 연결 상태 확인
    int error = 0;
#ifdef _WIN32
    int error_len = sizeof(error);
    if (getsockopt(tcp_socket_, SOL_SOCKET, SO_ERROR, 
                   reinterpret_cast<char*>(&error), &error_len) == SOCKET_ERROR_VALUE) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to get socket error");
        return false;
    }
#else
    socklen_t error_len = sizeof(error);
    if (getsockopt(tcp_socket_, SOL_SOCKET, SO_ERROR, &error, &error_len) < 0) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to get socket error");
        return false;
    }
#endif
    
    if (error != 0) {
        LogMessage(LogLevel::LOG_ERROR, "Connection failed: " + SocketErrorToString(error));
        return false;
    }
    
    return true;
}

void TcpBasedWorker::CloseTcpSocket() {
    if (IsTcpSocketValid()) {
        LogMessage(LogLevel::DEBUG_LEVEL, "Closing TCP socket");
        CLOSE_SOCKET(tcp_socket_);
        tcp_socket_ = INVALID_SOCKET_VALUE;
    }
}

bool TcpBasedWorker::IsTcpSocketValid() const {
    return tcp_socket_ != INVALID_SOCKET_VALUE;
}

bool TcpBasedWorker::IsTcpSocketConnected() const {
    if (!IsTcpSocketValid()) {
        return false;
    }
    
    // 소켓 상태 확인 (크로스 플랫폼)
    char dummy;
#ifdef _WIN32
    int result = recv(tcp_socket_, &dummy, 1, MSG_PEEK);
    if (result == SOCKET_ERROR_VALUE) {
        int error = WSAGetLastError();
        return (error == WSAEWOULDBLOCK);
    }
#else
    int result = recv(tcp_socket_, &dummy, 1, MSG_PEEK | MSG_DONTWAIT);
    if (result < 0) {
        return (errno == EAGAIN || errno == EWOULDBLOCK);
    }
#endif
    return true;
}

void TcpBasedWorker::SetSocketNonBlocking(bool non_blocking) {
#ifdef _WIN32
    u_long mode = non_blocking ? 1 : 0;
    ioctlsocket(tcp_socket_, FIONBIO, &mode);
#else
    int flags = fcntl(tcp_socket_, F_GETFL, 0);
    if (non_blocking) {
        fcntl(tcp_socket_, F_SETFL, flags | O_NONBLOCK);
    } else {
        fcntl(tcp_socket_, F_SETFL, flags & ~O_NONBLOCK);
    }
#endif
}

void TcpBasedWorker::SetSocketTimeouts() {
#ifdef _WIN32
    // Windows: 밀리초 단위
    DWORD send_timeout = tcp_config_.send_timeout_seconds * 1000;
    setsockopt(tcp_socket_, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&send_timeout), sizeof(send_timeout));
    
    DWORD recv_timeout = tcp_config_.receive_timeout_seconds * 1000;
    setsockopt(tcp_socket_, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&recv_timeout), sizeof(recv_timeout));
#else
    // Linux: timeval 구조체
    struct timeval send_timeout;
    send_timeout.tv_sec = tcp_config_.send_timeout_seconds;
    send_timeout.tv_usec = 0;
    setsockopt(tcp_socket_, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout));
    
    struct timeval recv_timeout;
    recv_timeout.tv_sec = tcp_config_.receive_timeout_seconds;
    recv_timeout.tv_usec = 0;
    setsockopt(tcp_socket_, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));
#endif
}

void TcpBasedWorker::ParseEndpoint() {
    const std::string& endpoint = device_info_.endpoint;
    
    // endpoint 형식: "ip:port" 또는 "ip"
    size_t colon_pos = endpoint.find(':');
    
    if (colon_pos != std::string::npos) {
        tcp_config_.ip_address = endpoint.substr(0, colon_pos);
        try {
            tcp_config_.port = static_cast<uint16_t>(std::stoi(endpoint.substr(colon_pos + 1)));
        } catch (const std::exception& e) {
            LogMessage(LogLevel::WARN, "Invalid port in endpoint, using default 502");
            tcp_config_.port = 502;
        }
    } else {
        tcp_config_.ip_address = endpoint;
        tcp_config_.port = 502; // 기본 Modbus TCP 포트
    }
    
    LogMessage(LogLevel::INFO, "Parsed endpoint: " + tcp_config_.ip_address + ":" + std::to_string(tcp_config_.port));
}

// =============================================================================
// 기본 프로토콜 구현 (파생 클래스에서 오버라이드)
// =============================================================================

bool TcpBasedWorker::EstablishProtocolConnection() {
    // 기본 구현: TCP 연결만으로 충분
    LogMessage(LogLevel::DEBUG_LEVEL, "Protocol connection established (base implementation)");
    return true;
}

void TcpBasedWorker::CloseProtocolConnection() {
    // 기본 구현: 특별한 프로토콜 정리 없음
    LogMessage(LogLevel::DEBUG_LEVEL, "Protocol connection closed (base implementation)");
}

bool TcpBasedWorker::CheckProtocolConnection() {
    // 기본 구현: TCP 소켓 상태만 확인
    return IsTcpSocketValid();
}

bool TcpBasedWorker::SendProtocolKeepAlive() {
    // 기본 구현: TCP 소켓 상태만 확인
    if (!IsTcpSocketValid()) {
        return false;
    }
    
    LogMessage(LogLevel::DEBUG_LEVEL, "TCP keep-alive sent (base implementation)");
    return true;
}

// =============================================================================
// 에러 메시지 변환 유틸리티 (크로스 플랫폼 통합)
// =============================================================================

std::string TcpBasedWorker::GetLastSocketErrorString() {
    return SocketErrorToString(GET_SOCKET_ERROR());
}

std::string TcpBasedWorker::SocketErrorToString(int error_code) {
#ifdef _WIN32
    // Windows 에러 코드
    switch (error_code) {
        case WSAEADDRINUSE: return "Address already in use";
        case WSAECONNREFUSED: return "Connection refused";
        case WSAEHOSTUNREACH: return "Host unreachable";
        case WSAENETUNREACH: return "Network unreachable";
        case WSAETIMEDOUT: return "Connection timed out";
        case WSAEWOULDBLOCK: return "Operation would block";
        case WSAEINPROGRESS: return "Operation in progress";
        case WSAENOTCONN: return "Socket not connected";
        case WSAECONNRESET: return "Connection reset by peer";
        case WSAECONNABORTED: return "Connection aborted";
        case WSAENOTSOCK: return "Not a socket";
        case WSAEFAULT: return "Bad address";
        default: return "Winsock error " + std::to_string(error_code);
    }
#else
    // Linux/Unix 에러 코드 (EAGAIN/EWOULDBLOCK 중복 해결)
    switch (error_code) {
        case EADDRINUSE: return "Address already in use";
        case ECONNREFUSED: return "Connection refused";
        case EHOSTUNREACH: return "Host unreachable";
        case ENETUNREACH: return "Network unreachable";
        case ETIMEDOUT: return "Connection timed out";
        case EAGAIN: return "Operation would block";
#if EAGAIN != EWOULDBLOCK
        case EWOULDBLOCK: return "Operation would block";
#endif
        case EINPROGRESS: return "Operation in progress";
        case ENOTCONN: return "Socket not connected";
        case ECONNRESET: return "Connection reset by peer";
        case ECONNABORTED: return "Connection aborted";
        case ENOTSOCK: return "Not a socket";
        case EFAULT: return "Bad address";
        case EBADF: return "Bad file descriptor";
        default: return std::string(strerror(error_code));
    }
#endif
}

#ifdef _WIN32
// =============================================================================
// Windows 전용 Winsock 관리
// =============================================================================

bool TcpBasedWorker::InitializeWinsock() {
    if (winsock_initialized_) {
        return true;
    }
    
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        LogMessage(LogLevel::LOG_ERROR, "WSAStartup failed: " + SocketErrorToString(result));
        return false;
    }
    
    winsock_initialized_ = true;
    LogMessage(LogLevel::DEBUG_LEVEL, "Winsock initialized successfully");
    return true;
}

void TcpBasedWorker::CleanupWinsock() {
    if (winsock_initialized_) {
        WSACleanup();
        winsock_initialized_ = false;
        LogMessage(LogLevel::DEBUG_LEVEL, "Winsock cleaned up");
    }
}
#endif

} // namespace Workers
} // namespace PulseOne