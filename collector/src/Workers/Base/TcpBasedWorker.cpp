// =============================================================================
// collector/src/Workers/Base/TcpBasedWorker.cpp - Windows 크로스 컴파일 완전 대응
// =============================================================================

/**
 * @file TcpBasedWorker.cpp
 * @brief 크로스 플랫폼 TCP 기반 프로토콜 워커의 구현
 * @author PulseOne Development Team
 * @date 2025-09-08
 * @version 8.0.0 - Windows 크로스 컴파일 완전 대응
 */

#include "Workers/Base/TcpBasedWorker.h"
#include "Utils/LogManager.h"
#include "Common/Enums.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>

// =============================================================================
// 플랫폼별 시스템 헤더 (조건부 include)
// =============================================================================
#if PULSEONE_WINDOWS
    // Windows 추가 네트워크 헤더들
    #include <iphlpapi.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "iphlpapi.lib")
#else
    #include <cstring>      // strerror, memset 함수용
    #include <cerrno>       // errno 변수용
    #include <sys/select.h> // select
    #include <poll.h>       // poll
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
#if PULSEONE_WINDOWS
    , winsock_initialized_(false)
#endif
{
    // endpoint에서 TCP 설정 파싱
    ParseEndpoint();
    
#if PULSEONE_WINDOWS
    // Windows에서 Winsock 초기화
    if (!InitializeWinsock()) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to initialize Winsock");
        return;
    }
#endif
    
    // BaseDeviceWorker 재연결 설정 활용
    ReconnectionSettings settings;
    settings.auto_reconnect_enabled = true;
    settings.retry_interval_ms = 5000;          // TCP는 조금 더 긴 재시도 간격
    settings.max_retries_per_cycle = 10;        // TCP는 적은 재시도
    settings.keep_alive_enabled = true;
    settings.keep_alive_interval_seconds = 60;  // TCP Keep-alive
    UpdateReconnectionSettings(settings);
    
    LogMessage(LogLevel::INFO, "TcpBasedWorker created for device: " + device_info.name);
}

TcpBasedWorker::~TcpBasedWorker() {
    CloseTcpSocket();
    
#if PULSEONE_WINDOWS
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
    
    // 프로토콜별 연결 상태 확인
    return CheckProtocolConnection();
}

bool TcpBasedWorker::SendKeepAlive() {
    // TCP 소켓 상태 먼저 확인
    if (!IsTcpSocketConnected()) {
        LogMessage(LogLevel::WARN, "TCP socket not connected for keep-alive");
        return false;
    }
    
    // 프로토콜별 Keep-alive 전송
    return SendProtocolKeepAlive();
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
    
    LogMessage(LogLevel::INFO, "TCP connection configured - " + ip + ":" + std::to_string(port));
}

void TcpBasedWorker::ConfigureTcp(const TcpConfig& config) {
    tcp_config_ = config;
    
    LogMessage(LogLevel::INFO, 
               "TCP configured - " + config.ip_address + 
               ":" + std::to_string(config.port) + 
               " (timeout: " + std::to_string(config.connection_timeout_seconds) + "s)");
}

std::string TcpBasedWorker::GetTcpConnectionInfo() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"tcp_connection\": {\n";
    ss << "    \"ip_address\": \"" << tcp_config_.ip_address << "\",\n";
    ss << "    \"port\": " << tcp_config_.port << ",\n";
    ss << "    \"connection_timeout_seconds\": " << tcp_config_.connection_timeout_seconds << ",\n";
    ss << "    \"send_timeout_seconds\": " << tcp_config_.send_timeout_seconds << ",\n";
    ss << "    \"receive_timeout_seconds\": " << tcp_config_.receive_timeout_seconds << ",\n";
    ss << "    \"keep_alive_enabled\": " << (tcp_config_.keep_alive_enabled ? "true" : "false") << ",\n";
    ss << "    \"no_delay_enabled\": " << (tcp_config_.no_delay_enabled ? "true" : "false") << ",\n";
#if PULSEONE_WINDOWS
    ss << "    \"tcp_socket\": " << reinterpret_cast<uintptr_t>(tcp_socket_) << ",\n";
#else
    ss << "    \"tcp_socket\": " << tcp_socket_ << ",\n";
#endif
    ss << "    \"connected\": " << (IsTcpSocketConnected() ? "true" : "false") << "\n";
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

std::string TcpBasedWorker::GetTcpStats() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"tcp_socket\": {\n";
    ss << "    \"ip_address\": \"" << tcp_config_.ip_address << "\",\n";
    ss << "    \"port\": " << tcp_config_.port << ",\n";
#if PULSEONE_WINDOWS
    ss << "    \"tcp_socket\": " << reinterpret_cast<uintptr_t>(tcp_socket_) << ",\n";
#else
    ss << "    \"tcp_socket\": " << tcp_socket_ << ",\n";
#endif
    ss << "    \"connected\": " << (IsTcpSocketConnected() ? "true" : "false") << "\n";
    ss << "  },\n";
    ss << "  \"base_worker_stats\": " << GetReconnectionStats() << "\n";
    ss << "}";
    return ss.str();
}

// =============================================================================
// 크로스 플랫폼 TCP 소켓 관리 (실제 구현)
// =============================================================================

bool TcpBasedWorker::ValidateTcpConfig() const {
    if (tcp_config_.ip_address.empty()) {
        LogMessage(LogLevel::LOG_ERROR, "IP address is empty");
        return false;
    }
    
    if (tcp_config_.port == 0) {
        LogMessage(LogLevel::LOG_ERROR, "Port is zero");
        return false;
    }
    
    if (tcp_config_.connection_timeout_seconds <= 0) {
        LogMessage(LogLevel::LOG_ERROR, "Invalid timeout value: " + 
                   std::to_string(tcp_config_.connection_timeout_seconds));
        return false;
    }
    
    return true;
}

bool TcpBasedWorker::CreateTcpSocket() {
    // 기존 소켓이 있으면 닫기
    CloseTcpSocket();
    
#if PULSEONE_WINDOWS
    // =======================================================================
    // Windows 소켓 생성
    // =======================================================================
    
    // 소켓 생성
    tcp_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_socket_ == INVALID_SOCKET) {
        int error = WSAGetLastError();
        LogMessage(LogLevel::LOG_ERROR, "Failed to create TCP socket: " + WinsockErrorToString(error));
        return false;
    }
    
    // 소켓 옵션 설정
    if (!ConfigureSocketOptions()) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to configure socket options");
        CloseTcpSocket();
        return false;
    }
    
    // 서버 주소 설정
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(tcp_config_.port);
    
    // IP 주소 변환
    if (InetPtonA(AF_INET, tcp_config_.ip_address.c_str(), &server_addr.sin_addr) != 1) {
        LogMessage(LogLevel::LOG_ERROR, "Invalid IP address: " + tcp_config_.ip_address);
        CloseTcpSocket();
        return false;
    }
    
    // 연결 시도
    if (connect(tcp_socket_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        LogMessage(LogLevel::LOG_ERROR, "TCP connection failed: " + WinsockErrorToString(error));
        CloseTcpSocket();
        return false;
    }
    
#else
    // =======================================================================
    // Linux 소켓 생성
    // =======================================================================
    
    // 소켓 생성
    tcp_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket_ < 0) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to create TCP socket: " + UnixSocketErrorToString(errno));
        return false;
    }
    
    // 소켓 옵션 설정
    if (!ConfigureSocketOptions()) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to configure socket options");
        CloseTcpSocket();
        return false;
    }
    
    // 서버 주소 설정
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(tcp_config_.port);
    
    // IP 주소 변환
    if (inet_pton(AF_INET, tcp_config_.ip_address.c_str(), &server_addr.sin_addr) <= 0) {
        LogMessage(LogLevel::LOG_ERROR, "Invalid IP address: " + tcp_config_.ip_address);
        CloseTcpSocket();
        return false;
    }
    
    // 연결 시도
    if (connect(tcp_socket_, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        LogMessage(LogLevel::LOG_ERROR, "TCP connection failed: " + UnixSocketErrorToString(errno));
        CloseTcpSocket();
        return false;
    }
    
#endif
    
    LogMessage(LogLevel::INFO, "TCP socket connected successfully");
    return true;
}

void TcpBasedWorker::CloseTcpSocket() {
    if (tcp_socket_ != TCP_INVALID_SOCKET) {
        CLOSE_SOCKET(tcp_socket_);
        tcp_socket_ = TCP_INVALID_SOCKET;
        LogMessage(LogLevel::DEBUG_LEVEL, "TCP socket closed");
    }
}

bool TcpBasedWorker::IsTcpSocketConnected() const {
    if (tcp_socket_ == TCP_INVALID_SOCKET) {
        return false;
    }
    
#if PULSEONE_WINDOWS
    // Windows에서 소켓 상태 확인
    int error = 0;
    int len = sizeof(error);
    if (getsockopt(tcp_socket_, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len) == SOCKET_ERROR) {
        return false;
    }
    return error == 0;
#else
    // Linux에서 소켓 상태 확인
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(tcp_socket_, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        return false;
    }
    return error == 0;
#endif
}

ssize_t TcpBasedWorker::SendTcpData(const void* data, size_t length) {
    if (tcp_socket_ == TCP_INVALID_SOCKET) {
        LogMessage(LogLevel::LOG_ERROR, "Cannot send data: TCP socket not connected");
        return -1;
    }
    
#if PULSEONE_WINDOWS
    int sent = send(tcp_socket_, static_cast<const char*>(data), static_cast<int>(length), 0);
    if (sent == SOCKET_ERROR) {
        int error = WSAGetLastError();
        LogMessage(LogLevel::LOG_ERROR, "TCP send failed: " + WinsockErrorToString(error));
        return -1;
    }
    
    LogMessage(LogLevel::DEBUG_LEVEL, "Sent " + std::to_string(sent) + " bytes via TCP");
    return static_cast<ssize_t>(sent);
#else
    ssize_t sent = send(tcp_socket_, data, length, 0);
    if (sent < 0) {
        LogMessage(LogLevel::LOG_ERROR, "TCP send failed: " + UnixSocketErrorToString(errno));
        return -1;
    }
    
    LogMessage(LogLevel::DEBUG_LEVEL, "Sent " + std::to_string(sent) + " bytes via TCP");
    return sent;
#endif
}

ssize_t TcpBasedWorker::ReceiveTcpData(void* buffer, size_t buffer_size) {
    if (tcp_socket_ == TCP_INVALID_SOCKET) {
        LogMessage(LogLevel::LOG_ERROR, "Cannot receive data: TCP socket not connected");
        return -1;
    }
    
#if PULSEONE_WINDOWS
    // Windows에서 타임아웃이 이미 소켓 옵션으로 설정됨
    int received = recv(tcp_socket_, static_cast<char*>(buffer), static_cast<int>(buffer_size), 0);
    if (received == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAETIMEDOUT) {
            return 0; // 타임아웃
        }
        LogMessage(LogLevel::LOG_ERROR, "TCP receive failed: " + WinsockErrorToString(error));
        return -1;
    }
    
    LogMessage(LogLevel::DEBUG_LEVEL, "Received " + std::to_string(received) + " bytes via TCP");
    return static_cast<ssize_t>(received);
#else
    // Linux에서 select를 사용한 타임아웃 처리
    fd_set read_fds;
    struct timeval timeout;
    
    FD_ZERO(&read_fds);
    FD_SET(tcp_socket_, &read_fds);
    
    timeout.tv_sec = tcp_config_.receive_timeout_seconds;
    timeout.tv_usec = 0;
    
    int select_result = select(tcp_socket_ + 1, &read_fds, NULL, NULL, &timeout);
    
    if (select_result < 0) {
        LogMessage(LogLevel::LOG_ERROR, "TCP select failed: " + UnixSocketErrorToString(errno));
        return -1;
    }
    
    if (select_result == 0) {
        // 타임아웃
        return 0;
    }
    
    ssize_t received = recv(tcp_socket_, buffer, buffer_size, 0);
    if (received < 0) {
        LogMessage(LogLevel::LOG_ERROR, "TCP receive failed: " + UnixSocketErrorToString(errno));
        return -1;
    }
    
    LogMessage(LogLevel::DEBUG_LEVEL, "Received " + std::to_string(received) + " bytes via TCP");
    return received;
#endif
}

TcpSocket TcpBasedWorker::GetTcpSocket() const {
    return tcp_socket_;
}

bool TcpBasedWorker::ConfigureSocketOptions() {
    if (tcp_socket_ == TCP_INVALID_SOCKET) {
        return false;
    }
    
#if PULSEONE_WINDOWS
    // Windows 소켓 옵션 설정
    
    // SO_REUSEADDR 설정
    BOOL opt = TRUE;
    if (setsockopt(tcp_socket_, SOL_SOCKET, SO_REUSEADDR, 
                   reinterpret_cast<const char*>(&opt), sizeof(opt)) == SOCKET_ERROR) {
        LogMessage(LogLevel::WARN, "Failed to set SO_REUSEADDR: " + WinsockErrorToString(WSAGetLastError()));
    }
    
    // 타임아웃 설정
    DWORD timeout_ms = tcp_config_.send_timeout_seconds * 1000;
    setsockopt(tcp_socket_, SOL_SOCKET, SO_SNDTIMEO, 
               reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    
    timeout_ms = tcp_config_.receive_timeout_seconds * 1000;
    setsockopt(tcp_socket_, SOL_SOCKET, SO_RCVTIMEO, 
               reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    
    // TCP_NODELAY 설정 (Nagle 알고리즘 비활성화)
    if (tcp_config_.no_delay_enabled) {
        BOOL no_delay = TRUE;
        if (setsockopt(tcp_socket_, IPPROTO_TCP, TCP_NODELAY, 
                       reinterpret_cast<const char*>(&no_delay), sizeof(no_delay)) == SOCKET_ERROR) {
            LogMessage(LogLevel::WARN, "Failed to set TCP_NODELAY: " + WinsockErrorToString(WSAGetLastError()));
        }
    }
    
    // Keep-alive 설정
    if (tcp_config_.keep_alive_enabled) {
        BOOL keep_alive = TRUE;
        if (setsockopt(tcp_socket_, SOL_SOCKET, SO_KEEPALIVE, 
                       reinterpret_cast<const char*>(&keep_alive), sizeof(keep_alive)) == SOCKET_ERROR) {
            LogMessage(LogLevel::WARN, "Failed to set SO_KEEPALIVE: " + WinsockErrorToString(WSAGetLastError()));
        }
    }
    
#else
    // Linux 소켓 옵션 설정
    
    // SO_REUSEADDR 설정
    int opt = 1;
    if (setsockopt(tcp_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LogMessage(LogLevel::WARN, "Failed to set SO_REUSEADDR: " + UnixSocketErrorToString(errno));
    }
    
    // 타임아웃 설정
    struct timeval timeout;
    timeout.tv_sec = tcp_config_.send_timeout_seconds;
    timeout.tv_usec = 0;
    setsockopt(tcp_socket_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    timeout.tv_sec = tcp_config_.receive_timeout_seconds;
    timeout.tv_usec = 0;
    setsockopt(tcp_socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // TCP_NODELAY 설정 (Nagle 알고리즘 비활성화)
    if (tcp_config_.no_delay_enabled) {
        int no_delay = 1;
        if (setsockopt(tcp_socket_, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay)) < 0) {
            LogMessage(LogLevel::WARN, "Failed to set TCP_NODELAY: " + UnixSocketErrorToString(errno));
        }
    }
    
    // Keep-alive 설정
    if (tcp_config_.keep_alive_enabled) {
        int keep_alive = 1;
        if (setsockopt(tcp_socket_, SOL_SOCKET, SO_KEEPALIVE, &keep_alive, sizeof(keep_alive)) < 0) {
            LogMessage(LogLevel::WARN, "Failed to set SO_KEEPALIVE: " + UnixSocketErrorToString(errno));
        }
    }
    
#endif
    
    return true;
}

// =============================================================================
// 기본 프로토콜 구현
// =============================================================================

bool TcpBasedWorker::SendProtocolKeepAlive() {
    // 기본 구현: TCP 소켓 상태만 확인
    if (tcp_socket_ == TCP_INVALID_SOCKET) {
        return false;
    }
    
    // 실제로는 소켓의 Keep-alive 메커니즘 사용
    LogMessage(LogLevel::DEBUG_LEVEL, "TCP keep-alive sent");
    return true;
}

// =============================================================================
// 내부 유틸리티 메서드들
// =============================================================================

void TcpBasedWorker::ParseEndpoint() {
    if (!device_info_.endpoint.empty()) {
        // endpoint 형식: "192.168.1.100:502" 또는 "localhost:1883"
        std::string endpoint = device_info_.endpoint;
        size_t colon_pos = endpoint.find(':');
        
        if (colon_pos != std::string::npos) {
            tcp_config_.ip_address = endpoint.substr(0, colon_pos);
            tcp_config_.port = static_cast<uint16_t>(std::stoi(endpoint.substr(colon_pos + 1)));
            
            LogMessage(LogLevel::DEBUG_LEVEL, 
                      "Parsed TCP endpoint: " + tcp_config_.ip_address + 
                      ":" + std::to_string(tcp_config_.port));
        } else {
            // 포트가 없으면 IP만 설정 (기본 포트 사용)
            tcp_config_.ip_address = endpoint;
            LogMessage(LogLevel::DEBUG_LEVEL, "Using default port for IP: " + tcp_config_.ip_address);
        }
    }
}

#if PULSEONE_WINDOWS
// =============================================================================
// Windows 전용 구현
// =============================================================================

bool TcpBasedWorker::InitializeWinsock() {
    if (winsock_initialized_) {
        return true;
    }
    
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        LogMessage(LogLevel::LOG_ERROR, "WSAStartup failed: " + std::to_string(result));
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

std::string TcpBasedWorker::WinsockErrorToString(int error_code) const {
    switch (error_code) {
        case WSAEWOULDBLOCK:    return "Operation would block";
        case WSAECONNRESET:     return "Connection reset by peer";
        case WSAECONNABORTED:   return "Connection aborted";
        case WSAECONNREFUSED:   return "Connection refused";
        case WSAENETDOWN:       return "Network is down";
        case WSAENETUNREACH:    return "Network unreachable";
        case WSAEHOSTDOWN:      return "Host is down";
        case WSAEHOSTUNREACH:   return "Host unreachable";
        case WSAETIMEDOUT:      return "Connection timed out";
        case WSAEINVAL:         return "Invalid argument";
        case WSAEACCES:         return "Permission denied";
        case WSAEADDRINUSE:     return "Address already in use";
        case WSAEADDRNOTAVAIL:  return "Address not available";
        default:
            return "Winsock error " + std::to_string(error_code);
    }
}

#else
// =============================================================================
// Linux 전용 구현
// =============================================================================

std::string TcpBasedWorker::UnixSocketErrorToString(int error_code) const {
    return std::string(strerror(error_code));
}

bool TcpBasedWorker::SetSocketNonBlocking(TcpSocket socket) {
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    
    if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) < 0) {
        return false;
    }
    
    return true;
}

#endif

std::string TcpBasedWorker::GetLastSocketError() const {
#if PULSEONE_WINDOWS
    return WinsockErrorToString(WSAGetLastError());
#else
    return UnixSocketErrorToString(errno);
#endif
}

} // namespace Workers
} // namespace PulseOne