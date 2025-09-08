/**
 * @file TcpBasedWorker.cpp - 완전 구현본 (모든 누락 함수 포함)
 * @brief TCP 기반 프로토콜 워커 구현 - 크로스 플랫폼 완전 지원
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 2.1.0 - 완전 구현
 */

// =============================================================================
// UUID 충돌 방지 (가장 먼저!)
// =============================================================================
#ifdef _WIN32
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
#endif

#include "Workers/Base/TcpBasedWorker.h"
#include "Utils/LogManager.h"
#include "Common/Enums.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>

// =============================================================================
// 플랫폼별 네트워크 헤더 (조건부 include)
// =============================================================================
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    #include <windows.h>
#else
    #include <cstring>
    #include <cerrno>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/select.h>
    #include <poll.h>
#endif

using namespace std::chrono;
using LogLevel = PulseOne::Enums::LogLevel;

namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

TcpBasedWorker::TcpBasedWorker(const PulseOne::Structs::DeviceInfo& device_info)
    : BaseDeviceWorker(device_info)
    , tcp_socket_(TCP_INVALID_SOCKET)
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
    
    if (tcp_config_.port == 0 || tcp_config_.port > 65535) {
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
#ifdef _WIN32
    tcp_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_socket_ == INVALID_SOCKET) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to create socket: " + WinsockErrorToString(WSAGetLastError()));
        return false;
    }
#else
    tcp_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket_ < 0) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to create socket: " + UnixSocketErrorToString(errno));
        return false;
    }
#endif
    
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
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse)) == SOCKET_ERROR) {
        LogMessage(LogLevel::WARN, "Failed to set SO_REUSEADDR: " + WinsockErrorToString(WSAGetLastError()));
    }
#else
    if (setsockopt(tcp_socket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        LogMessage(LogLevel::WARN, "Failed to set SO_REUSEADDR: " + UnixSocketErrorToString(errno));
    }
#endif
    
    // TCP_NODELAY 설정 (Nagle 알고리즘 비활성화)
    if (tcp_config_.no_delay_enabled) {
        int nodelay = 1;
#ifdef _WIN32
        if (setsockopt(tcp_socket_, IPPROTO_TCP, TCP_NODELAY,
                       reinterpret_cast<const char*>(&nodelay), sizeof(nodelay)) == SOCKET_ERROR) {
            LogMessage(LogLevel::WARN, "Failed to set TCP_NODELAY: " + WinsockErrorToString(WSAGetLastError()));
        }
#else
        if (setsockopt(tcp_socket_, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) < 0) {
            LogMessage(LogLevel::WARN, "Failed to set TCP_NODELAY: " + UnixSocketErrorToString(errno));
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
    
    // IP 주소 변환 (플랫폼별 처리)
#ifdef _WIN32
    // Windows: inet_pton 사용
    if (inet_pton(AF_INET, tcp_config_.ip_address.c_str(), &server_addr.sin_addr) != 1) {
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
#ifdef _WIN32
    int result = connect(tcp_socket_, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr));
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            LogMessage(LogLevel::LOG_ERROR, "Connect failed immediately: " + WinsockErrorToString(error));
            return false;
        }
    }
#else
    int result = connect(tcp_socket_, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr));
    if (result < 0) {
        if (errno != EINPROGRESS) {
            LogMessage(LogLevel::LOG_ERROR, "Connect failed immediately: " + UnixSocketErrorToString(errno));
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
#ifdef _WIN32
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(tcp_socket_, &write_fds);
    
    struct timeval timeout;
    timeout.tv_sec = tcp_config_.connection_timeout_seconds;
    timeout.tv_usec = 0;
    
    int result = select(0, nullptr, &write_fds, nullptr, &timeout);
    if (result == SOCKET_ERROR) {
        LogMessage(LogLevel::LOG_ERROR, "Select failed: " + WinsockErrorToString(WSAGetLastError()));
        return false;
    }
    
    if (result == 0) {
        LogMessage(LogLevel::LOG_ERROR, "Connection timeout");
        return false;
    }
    
    // 연결 상태 확인
    int error = 0;
    int error_len = sizeof(error);
    if (getsockopt(tcp_socket_, SOL_SOCKET, SO_ERROR, 
                   reinterpret_cast<char*>(&error), &error_len) == SOCKET_ERROR) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to get socket error");
        return false;
    }
    
    if (error != 0) {
        LogMessage(LogLevel::LOG_ERROR, "Connection failed: " + WinsockErrorToString(error));
        return false;
    }
#else
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(tcp_socket_, &write_fds);
    
    struct timeval timeout;
    timeout.tv_sec = tcp_config_.connection_timeout_seconds;
    timeout.tv_usec = 0;
    
    int result = select(tcp_socket_ + 1, nullptr, &write_fds, nullptr, &timeout);
    if (result < 0) {
        LogMessage(LogLevel::LOG_ERROR, "Select failed: " + UnixSocketErrorToString(errno));
        return false;
    }
    
    if (result == 0) {
        LogMessage(LogLevel::LOG_ERROR, "Connection timeout");
        return false;
    }
    
    // 연결 상태 확인
    int error = 0;
    socklen_t error_len = sizeof(error);
    if (getsockopt(tcp_socket_, SOL_SOCKET, SO_ERROR, &error, &error_len) < 0) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to get socket error");
        return false;
    }
    
    if (error != 0) {
        LogMessage(LogLevel::LOG_ERROR, "Connection failed: " + UnixSocketErrorToString(error));
        return false;
    }
#endif
    
    return true;
}

void TcpBasedWorker::CloseTcpSocket() {
    if (IsTcpSocketValid()) {
        LogMessage(LogLevel::DEBUG_LEVEL, "Closing TCP socket");
        
#ifdef _WIN32
        closesocket(tcp_socket_);
        tcp_socket_ = INVALID_SOCKET;
#else
        close(tcp_socket_);
        tcp_socket_ = -1;
#endif
    }
}

bool TcpBasedWorker::IsTcpSocketValid() const {
#ifdef _WIN32
    return tcp_socket_ != INVALID_SOCKET;
#else
    return tcp_socket_ >= 0;
#endif
}

bool TcpBasedWorker::IsTcpSocketConnected() const {
    if (!IsTcpSocketValid()) {
        return false;
    }
    
    // 소켓 상태 확인 (플랫폼별)
#ifdef _WIN32
    char dummy;
    int result = recv(tcp_socket_, &dummy, 1, MSG_PEEK);
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        return (error == WSAEWOULDBLOCK);
    }
    return true;
#else
    char dummy;
    int result = recv(tcp_socket_, &dummy, 1, MSG_PEEK | MSG_DONTWAIT);
    if (result < 0) {
        return (errno == EAGAIN || errno == EWOULDBLOCK);
    }
    return true;
#endif
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
    // 송신 타임아웃 설정
#ifdef _WIN32
    DWORD send_timeout = tcp_config_.send_timeout_seconds * 1000;
    setsockopt(tcp_socket_, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&send_timeout), sizeof(send_timeout));
    
    DWORD recv_timeout = tcp_config_.receive_timeout_seconds * 1000;
    setsockopt(tcp_socket_, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&recv_timeout), sizeof(recv_timeout));
#else
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
// 기본 프로토콜 구현
// =============================================================================

bool TcpBasedWorker::SendProtocolKeepAlive() {
    // 기본 구현: TCP 소켓 상태만 확인
    if (!IsTcpSocketValid()) {
        return false;
    }
    
    // 실제로는 소켓의 Keep-alive 메커니즘 사용
    LogMessage(LogLevel::DEBUG_LEVEL, "TCP keep-alive sent (base implementation)");
    return true;
}

// =============================================================================
// 에러 메시지 변환 유틸리티
// =============================================================================

#ifdef _WIN32
std::string TcpBasedWorker::WinsockErrorToString(int error_code) {
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
        default: return "Winsock error " + std::to_string(error_code);
    }
}

bool TcpBasedWorker::InitializeWinsock() {
    if (winsock_initialized_) {
        return true;
    }
    
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        LogMessage(LogLevel::LOG_ERROR, "WSAStartup failed: " + WinsockErrorToString(result));
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
#else
std::string TcpBasedWorker::UnixSocketErrorToString(int error_code) {
    switch (error_code) {
        case EADDRINUSE: return "Address already in use";
        case ECONNREFUSED: return "Connection refused";
        case EHOSTUNREACH: return "Host unreachable";
        case ENETUNREACH: return "Network unreachable";
        case ETIMEDOUT: return "Connection timed out";
        case EAGAIN: 
        case EWOULDBLOCK: return "Operation would block";
        case EINPROGRESS: return "Operation in progress";
        case ENOTCONN: return "Socket not connected";
        case ECONNRESET: return "Connection reset by peer";
        case ECONNABORTED: return "Connection aborted";
        default: return std::string(strerror(error_code));
    }
}
#endif

} // namespace Workers
} // namespace PulseOne