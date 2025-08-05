/**
 * @file TcpBasedWorker.cpp
 * @brief TCP 기반 프로토콜 워커의 구현 (수정됨 - 헤더 추가 및 접근 방식 변경)
 * @author PulseOne Development Team
 * @date 2025-01-20
 * @version 1.1.0
 */

#include "Workers/Base/TcpBasedWorker.h"
#include "Utils/LogManager.h"
#include "Common/Enums.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>      // strerror, memset 함수용
#include <cerrno>       // errno 변수용
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

using namespace std::chrono;

using LogLevel = PulseOne::Enums::LogLevel;
namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

TcpBasedWorker::TcpBasedWorker(const PulseOne::DeviceInfo& device_info,
                               std::shared_ptr<RedisClient> redis_client,
                               std::shared_ptr<InfluxClient> influx_client)
    : BaseDeviceWorker(device_info, redis_client, influx_client)
    , ip_address_("127.0.0.1")
    , port_(502)
    , connection_timeout_seconds_(5)
    , socket_fd_(-1) {
    
    ReconnectionSettings settings;
    settings.auto_reconnect_enabled = true;
    settings.retry_interval_ms = 5000;
    settings.keep_alive_enabled = true;
    UpdateReconnectionSettings(settings);
    
    LogMessage(LogLevel::INFO, "TcpBasedWorker created for device: " + device_info_.name);
}

TcpBasedWorker::~TcpBasedWorker() {
    CloseTcpSocket();
    LogMessage(LogLevel::INFO, "TcpBasedWorker destroyed");
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현
// =============================================================================

bool TcpBasedWorker::EstablishConnection() {
    LogMessage(LogLevel::INFO, "Establishing TCP connection to " + ip_address_ + ":" + std::to_string(port_));
    
    // TCP 설정 검증
    if (!ValidateTcpConfig()) {
        LogMessage(LogLevel::ERROR, "Invalid TCP configuration");
        return false;
    }
    
    // TCP 소켓 생성 및 연결
    if (!CreateTcpSocket()) {
        LogMessage(LogLevel::ERROR, "Failed to create TCP socket");
        return false;
    }
    
    // 파생 클래스의 프로토콜별 연결 수립
    if (!EstablishProtocolConnection()) {
        LogMessage(LogLevel::ERROR, "Failed to establish protocol connection");
        CloseTcpSocket();
        return false;
    }
    
    LogMessage(LogLevel::INFO, "TCP connection established successfully");
    return true;
}

bool TcpBasedWorker::CloseConnection() {
    LogMessage(LogLevel::INFO, "Closing TCP connection");
    
    // 프로토콜별 연결 해제
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
    ip_address_ = ip;
    port_ = port;
    connection_timeout_seconds_ = timeout_seconds;
    
    LogMessage(LogLevel::INFO, "TCP connection configured - " + ip + ":" + std::to_string(port));
}

std::string TcpBasedWorker::GetTcpConnectionInfo() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"tcp_connection\": {\n";
    ss << "    \"ip_address\": \"" << ip_address_ << "\",\n";
    ss << "    \"port\": " << port_ << ",\n";
    ss << "    \"timeout_seconds\": " << connection_timeout_seconds_ << ",\n";
    ss << "    \"socket_fd\": " << socket_fd_ << ",\n";
    ss << "    \"connected\": " << (IsTcpSocketConnected() ? "true" : "false") << "\n";
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

// =============================================================================
// TCP 소켓 관리 (보호된 메서드들)
// =============================================================================

bool TcpBasedWorker::ValidateTcpConfig() const {
    if (ip_address_.empty()) {
        LogMessage(LogLevel::ERROR, "IP address is empty");
        return false;
    }
    
    if (port_ == 0) {
        LogMessage(LogLevel::ERROR, "Port is zero");
        return false;
    }
    
    if (connection_timeout_seconds_ <= 0) {
        LogMessage(LogLevel::ERROR, "Invalid timeout value: " + std::to_string(connection_timeout_seconds_));
        return false;
    }
    
    return true;
}

bool TcpBasedWorker::CreateTcpSocket() {
    // 기존 소켓이 있으면 닫기
    CloseTcpSocket();
    
    // 새 소켓 생성
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        LogMessage(LogLevel::ERROR, "Failed to create TCP socket: " + std::string(strerror(errno)));
        return false;
    }
    
    // SO_REUSEADDR 설정
    int opt = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LogMessage(LogLevel::WARN, "Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
    }

    struct timeval tv;
    tv.tv_sec = connection_timeout_seconds_;
    tv.tv_usec = 0;
    setsockopt(socket_fd_, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    
    // 논블로킹 모드 설정
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    if (fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        LogMessage(LogLevel::WARN, "Failed to set non-blocking mode: " + std::string(strerror(errno)));
    }
    
    // 서버 주소 설정
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    
    if (inet_pton(AF_INET, ip_address_.c_str(), &server_addr.sin_addr) <= 0) {
        LogMessage(LogLevel::ERROR, "Invalid IP address: " + ip_address_);
        CloseTcpSocket();
        return false;
    }
    
    // 연결 시도
    if (connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LogMessage(LogLevel::ERROR, "TCP connection failed: " + std::string(strerror(errno)));
        CloseTcpSocket();
        return false;
    }
    
    LogMessage(LogLevel::INFO, "TCP socket connected successfully");
    return true;
}

void TcpBasedWorker::CloseTcpSocket() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        LogMessage(LogLevel::DEBUG_LEVEL, "TCP socket closed");
    }
}

bool TcpBasedWorker::IsTcpSocketConnected() const {
    if (socket_fd_ < 0) {
        return false;
    }
    
    // 소켓 상태 확인 (간단한 방법)
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        return false;
    }
    
    return error == 0;
}

// =============================================================================
// 기본 프로토콜 구현 (파생 클래스에서 오버라이드)
// =============================================================================

bool TcpBasedWorker::SendProtocolKeepAlive() {
    // 기본 구현: TCP 레벨 Keep-alive (SO_KEEPALIVE 소켓 옵션 사용)
    if (socket_fd_ < 0) {
        return false;
    }
    
    int keep_alive = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_KEEPALIVE, &keep_alive, sizeof(keep_alive)) < 0) {
        LogMessage(LogLevel::WARN, "Failed to set TCP keep-alive: " + std::string(strerror(errno)));
        return false;
    }
    
    LogMessage(LogLevel::DEBUG_LEVEL, "TCP keep-alive sent");
    return true;
}

} // namespace Workers
} // namespace PulseOne