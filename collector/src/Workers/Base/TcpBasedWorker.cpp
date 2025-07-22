/**
 * @file TcpBasedWorker.cpp
 * @brief TCP 기반 프로토콜 워커 구현 (중복 제거 완성본)
 * @details BaseDeviceWorker의 재연결/Keep-alive 기능 활용
 * @author PulseOne Development Team
 * @date 2025-01-20
 * @version 3.0.0
 */

#include "Workers/Base/TcpBasedWorker.h"
#include "Utils/LogManager.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sstream>
#include <algorithm>

using namespace std::chrono;

namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

TcpBasedWorker::TcpBasedWorker(const Drivers::DeviceInfo& device_info,
                               std::shared_ptr<RedisClient> redis_client,
                               std::shared_ptr<InfluxClient> influx_client)
    : BaseDeviceWorker(device_info, redis_client, influx_client) {
    
    // 디바이스 정보에서 TCP 설정 추출
    if (!device_info_.endpoint.empty()) {
        size_t colon_pos = device_info_.endpoint.find(':');
        if (colon_pos != std::string::npos) {
            ip_address_ = device_info_.endpoint.substr(0, colon_pos);
            port_ = static_cast<uint16_t>(std::stoi(device_info_.endpoint.substr(colon_pos + 1)));
        }
    }
    
    LogMessage(LogLevel::INFO, "TcpBasedWorker created for " + FormatTcpEndpoint());
    LogTcpActivity("TCP worker initialized");
}

TcpBasedWorker::~TcpBasedWorker() {
    // TCP 소켓 정리
    CloseTcpSocket();
    
    LogMessage(LogLevel::INFO, "TcpBasedWorker destroyed for " + FormatTcpEndpoint());
}

// =============================================================================
// TCP 특화 설정 메서드들
// =============================================================================

void TcpBasedWorker::ConfigureTcpConnection(const std::string& ip, 
                                           uint16_t port,
                                           int timeout_seconds) {
    ip_address_ = ip;
    port_ = port;
    connection_timeout_seconds_ = timeout_seconds;
    
    LogMessage(LogLevel::INFO, "TCP connection configured: " + FormatTcpEndpoint() + 
              " (timeout: " + std::to_string(timeout_seconds) + "s)");
    LogTcpActivity("TCP configuration updated");
}

std::string TcpBasedWorker::GetTcpConnectionInfo() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"tcp_info\": {\n";
    ss << "    \"endpoint\": \"" << FormatTcpEndpoint() << "\",\n";
    ss << "    \"timeout_seconds\": " << connection_timeout_seconds_ << ",\n";
    ss << "    \"socket_fd\": " << socket_fd_ << ",\n";
    ss << "    \"socket_connected\": " << (IsTcpSocketConnected() ? "true" : "false") << "\n";
    ss << "  },\n";
    ss << "  \"connection_quality\": " << GetTcpConnectionQuality() << "\n";
    ss << "}";
    return ss.str();
}

int TcpBasedWorker::GetTcpConnectionQuality() const {
    if (!IsConnected()) {
        return 0; // 연결 안됨
    }
    
    if (!IsTcpSocketConnected()) {
        return 10; // TCP 소켓 문제
    }
    
    // BaseDeviceWorker의 재연결 통계 활용
    auto stats_json = GetReconnectionStats();
    
    // 간단한 품질 평가 (실제로는 더 복잡한 로직 가능)
    int base_score = 70; // 기본 점수
    
    // 최근 연결 실패가 없으면 점수 향상
    if (stats_json.find("\"failed_connections\": 0") != std::string::npos) {
        base_score += 20;
    }
    
    // Keep-alive 성공률 반영
    if (stats_json.find("\"keep_alive_failed\": 0") != std::string::npos) {
        base_score += 10;
    }
    
    return std::min(100, base_score);
}

// =============================================================================
// BaseDeviceWorker 순수 가상 함수 구현
// =============================================================================

bool TcpBasedWorker::EstablishConnection() {
    LogTcpActivity("Attempting TCP connection to " + FormatTcpEndpoint());
    
    // TCP 설정 유효성 검사
    if (!ValidateTcpConfig()) {
        LogTcpActivity("Invalid TCP configuration", LogLevel::ERROR);
        return false;
    }
    
    // TCP 소켓 생성 및 연결
    if (!CreateTcpSocket()) {
        LogTcpActivity("Failed to create TCP socket", LogLevel::ERROR);
        return false;
    }
    
    // 프로토콜별 연결 수립
    if (!EstablishProtocolConnection()) {
        LogTcpActivity("Protocol connection failed", LogLevel::ERROR);
        CloseTcpSocket();
        return false;
    }
    
    LogTcpActivity("TCP connection established successfully");
    return true;
}

bool TcpBasedWorker::CloseConnection() {
    LogTcpActivity("Closing TCP connection");
    
    // 프로토콜별 연결 해제
    CloseProtocolConnection();
    
    // TCP 소켓 해제
    CloseTcpSocket();
    
    LogTcpActivity("TCP connection closed");
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
    // TCP 소켓 상태 확인
    if (!IsTcpSocketConnected()) {
        LogTcpActivity("Keep-alive failed: TCP socket disconnected", LogLevel::WARN);
        return false;
    }
    
    // 프로토콜별 Keep-alive 전송
    bool protocol_result = SendProtocolKeepAlive();
    
    if (protocol_result) {
        LogTcpActivity("TCP Keep-alive successful", LogLevel::DEBUG);
    } else {
        LogTcpActivity("TCP Keep-alive failed", LogLevel::WARN);
    }
    
    return protocol_result;
}

// =============================================================================
// 내부 도우미 메서드들
// =============================================================================

bool TcpBasedWorker::ValidateTcpConfig() const {
    if (ip_address_.empty()) {
        LogMessage(LogLevel::ERROR, "TCP IP address is empty");
        return false;
    }
    
    if (port_ == 0 || port_ > 65535) {
        LogMessage(LogLevel::ERROR, "TCP port is invalid: " + std::to_string(port_));
        return false;
    }
    
    if (connection_timeout_seconds_ <= 0 || connection_timeout_seconds_ > 300) {
        LogMessage(LogLevel::ERROR, "TCP timeout is invalid: " + std::to_string(connection_timeout_seconds_));
        return false;
    }
    
    return true;
}

bool TcpBasedWorker::CreateTcpSocket() {
    // 기존 소켓이 있으면 닫기
    CloseTcpSocket();
    
    // TCP 소켓 생성
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        LogMessage(LogLevel::ERROR, "Failed to create TCP socket: " + std::string(strerror(errno)));
        return false;
    }
    
    // 소켓 옵션 설정
    int opt = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LogMessage(LogLevel::WARN, "Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
    }
    
    // 논블로킹 모드 설정 (타임아웃 처리용)
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    if (flags < 0 || fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
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
    int connect_result = connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    if (connect_result < 0) {
        if (errno == EINPROGRESS) {
            // 논블로킹 모드에서는 EINPROGRESS가 정상
            fd_set write_fds;
            FD_ZERO(&write_fds);
            FD_SET(socket_fd_, &write_fds);
            
            struct timeval timeout;
            timeout.tv_sec = connection_timeout_seconds_;
            timeout.tv_usec = 0;
            
            int select_result = select(socket_fd_ + 1, nullptr, &write_fds, nullptr, &timeout);
            
            if (select_result <= 0) {
                LogMessage(LogLevel::ERROR, "TCP connection timeout or error");
                CloseTcpSocket();
                return false;
            }
            
            // 연결 상태 확인
            int error;
            socklen_t error_len = sizeof(error);
            if (getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &error, &error_len) < 0 || error != 0) {
                LogMessage(LogLevel::ERROR, "TCP connection failed: " + std::string(strerror(error)));
                CloseTcpSocket();
                return false;
            }
        } else {
            LogMessage(LogLevel::ERROR, "TCP connection failed immediately: " + std::string(strerror(errno)));
            CloseTcpSocket();
            return false;
        }
    }
    
    // 블로킹 모드로 복원
    flags = fcntl(socket_fd_, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(socket_fd_, F_SETFL, flags & ~O_NONBLOCK);
    }
    
    LogTcpActivity("TCP socket created and connected (fd: " + std::to_string(socket_fd_) + ")");
    return true;
}

void TcpBasedWorker::CloseTcpSocket() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        LogTcpActivity("TCP socket closed (fd: " + std::to_string(socket_fd_) + ")");
        socket_fd_ = -1;
    }
}

std::string TcpBasedWorker::FormatTcpEndpoint() const {
    return ip_address_ + ":" + std::to_string(port_);
}

bool TcpBasedWorker::IsTcpSocketConnected() const {
    if (socket_fd_ < 0) {
        return false;
    }
    
    // 소켓 상태 확인 (간단한 구현)
    int error;
    socklen_t error_len = sizeof(error);
    
    if (getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &error, &error_len) < 0) {
        return false;
    }
    
    return error == 0;
}

void TcpBasedWorker::LogTcpActivity(const std::string& activity, LogLevel level) {
    std::string full_message = "[TCP:" + FormatTcpEndpoint() + "] " + activity;
    LogMessage(level, full_message);
}

} // namespace Workers
} // namespace PulseOne