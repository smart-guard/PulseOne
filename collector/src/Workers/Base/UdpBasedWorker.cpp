/**
 * @file UdpBasedWorker.cpp
 * @brief UDP 기반 디바이스 워커 클래스 구현
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 1.0.0
 */

#include "Workers/Base/UdpBasedWorker.h"
#include "Utils/LogManager.h"
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <thread>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>

using namespace std::chrono;

namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

UdpBasedWorker::UdpBasedWorker(
    const PulseOne::DeviceInfo& device_info,
    std::shared_ptr<RedisClient> redis_client,
    std::shared_ptr<InfluxClient> influx_client)
    : BaseDeviceWorker(device_info, redis_client, influx_client)
    , receive_thread_running_(false) {
    
    // UDP 통계 초기화
    udp_stats_.start_time = system_clock::now();
    udp_stats_.last_reset = udp_stats_.start_time;
    
    // UDP 연결 정보 초기화  
    udp_connection_.last_activity = system_clock::now();
    
    LogMessage(PulseOne::LogLevel::INFO, "UdpBasedWorker created for device: " + device_info.name);
    
    // device_info에서 UDP 설정 파싱
    if (!ParseUdpConfig()) {
        LogMessage(PulseOne::LogLevel::WARN, "Failed to parse UDP config, using defaults");
    }
}

UdpBasedWorker::~UdpBasedWorker() {
    // 수신 스레드 정리
    if (receive_thread_running_.load()) {
        receive_thread_running_ = false;
        if (receive_thread_ && receive_thread_->joinable()) {
            receive_thread_->join();
        }
    }
    
    // UDP 소켓 정리
    CloseUdpSocket();
    
    LogMessage(PulseOne::LogLevel::INFO, "UdpBasedWorker destroyed for device: " + device_info_.name);
}

// =============================================================================
// UDP 설정 관리 (공개 인터페이스)
// =============================================================================

void UdpBasedWorker::ConfigureUdp(const UdpConfig& config) {
    udp_config_ = config;
    LogMessage(PulseOne::LogLevel::INFO, "UDP configuration updated");
}

std::string UdpBasedWorker::GetUdpConnectionInfo() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"socket_fd\": " << udp_connection_.socket_fd << ",\n";
    ss << "  \"is_bound\": " << (udp_connection_.is_bound ? "true" : "false") << ",\n";
    ss << "  \"is_connected\": " << (udp_connection_.is_connected ? "true" : "false") << ",\n";
    
    if (udp_connection_.is_bound) {
        ss << "  \"local_address\": \"" << SockAddrToString(udp_connection_.local_addr) << "\",\n";
    }
    
    if (udp_connection_.is_connected) {
        ss << "  \"remote_address\": \"" << SockAddrToString(udp_connection_.remote_addr) << "\",\n";
    }
    
    // 마지막 활동 시간 (Unix timestamp)
    auto last_activity_time = duration_cast<seconds>(
        udp_connection_.last_activity.time_since_epoch()).count();
    ss << "  \"last_activity_timestamp\": " << last_activity_time << ",\n";
    
    // UDP 설정 정보
    ss << "  \"config\": {\n";
    ss << "    \"local_interface\": \"" << udp_config_.local_interface << "\",\n";
    ss << "    \"local_port\": " << udp_config_.local_port << ",\n";
    ss << "    \"remote_host\": \"" << udp_config_.remote_host << "\",\n";
    ss << "    \"remote_port\": " << udp_config_.remote_port << ",\n";
    ss << "    \"broadcast_enabled\": " << (udp_config_.broadcast_enabled ? "true" : "false") << ",\n";
    ss << "    \"multicast_enabled\": " << (udp_config_.multicast_enabled ? "true" : "false") << ",\n";
    ss << "    \"socket_timeout_ms\": " << udp_config_.socket_timeout_ms << ",\n";
    ss << "    \"receive_buffer_size\": " << udp_config_.receive_buffer_size << ",\n";
    ss << "    \"send_buffer_size\": " << udp_config_.send_buffer_size << "\n";
    ss << "  }\n";
    ss << "}";
    
    return ss.str();
}

std::string UdpBasedWorker::GetUdpStats() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"udp_statistics\": {\n";
    ss << "    \"packets_sent\": " << udp_stats_.packets_sent.load() << ",\n";
    ss << "    \"packets_received\": " << udp_stats_.packets_received.load() << ",\n";
    ss << "    \"bytes_sent\": " << udp_stats_.bytes_sent.load() << ",\n";
    ss << "    \"bytes_received\": " << udp_stats_.bytes_received.load() << ",\n";
    ss << "    \"send_errors\": " << udp_stats_.send_errors.load() << ",\n";
    ss << "    \"receive_errors\": " << udp_stats_.receive_errors.load() << ",\n";
    ss << "    \"timeouts\": " << udp_stats_.timeouts.load() << ",\n";
    ss << "    \"broadcast_packets\": " << udp_stats_.broadcast_packets.load() << ",\n";
    ss << "    \"multicast_packets\": " << udp_stats_.multicast_packets.load() << ",\n";
    
    // 통계 시작/리셋 시간 (Unix timestamp)
    auto start_time = duration_cast<seconds>(udp_stats_.start_time.time_since_epoch()).count();
    auto reset_time = duration_cast<seconds>(udp_stats_.last_reset.time_since_epoch()).count();
    ss << "    \"start_timestamp\": " << start_time << ",\n";
    ss << "    \"last_reset_timestamp\": " << reset_time << ",\n";
    
    // 현재 큐 상태
    {
        std::lock_guard<std::mutex> lock(receive_queue_mutex_);
        ss << "    \"receive_queue_size\": " << receive_queue_.size() << "\n";
    }
    
    ss << "  },\n";
    
    // BaseDeviceWorker 통계 추가
    ss << "  \"base_statistics\": " << GetStatusJson() << "\n";
    ss << "}";
    
    return ss.str();
}

void UdpBasedWorker::ResetUdpStats() {
    udp_stats_.packets_sent = 0;
    udp_stats_.packets_received = 0;
    udp_stats_.bytes_sent = 0;
    udp_stats_.bytes_received = 0;
    udp_stats_.send_errors = 0;
    udp_stats_.receive_errors = 0;
    udp_stats_.timeouts = 0;
    udp_stats_.broadcast_packets = 0;
    udp_stats_.multicast_packets = 0;
    udp_stats_.last_reset = system_clock::now();
    
    LogMessage(PulseOne::LogLevel::INFO, "UDP statistics reset");
}

// =============================================================================
// BaseDeviceWorker 순수 가상 함수 구현
// =============================================================================

bool UdpBasedWorker::EstablishConnection() {
    LogMessage(PulseOne::LogLevel::INFO, "Establishing UDP connection...");
    
    try {
        // 1. UDP 소켓 생성 및 설정
        if (!CreateUdpSocket()) {
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to create UDP socket");
            return false;
        }
        
        // 2. UDP 소켓 바인딩
        if (!BindUdpSocket()) {
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to bind UDP socket");
            CloseUdpSocket();
            return false;
        }
        
        // 3. 수신 스레드 시작
        receive_thread_running_ = true;
        receive_thread_ = std::make_unique<std::thread>(&UdpBasedWorker::ReceiveThreadFunction, this);
        
        // 4. 프로토콜별 연결 수립
        if (!EstablishProtocolConnection()) {
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to establish protocol connection");
            receive_thread_running_ = false;
            if (receive_thread_->joinable()) {
                receive_thread_->join();
            }
            CloseUdpSocket();
            return false;
        }
        
        LogMessage(PulseOne::LogLevel::INFO, "UDP connection established successfully");
        SetConnectionState(true);
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in EstablishConnection: " + std::string(e.what()));
        CloseConnection();
        return false;
    }
}

bool UdpBasedWorker::CloseConnection() {
    LogMessage(PulseOne::LogLevel::INFO, "Closing UDP connection...");
    
    try {
        // 1. 프로토콜별 연결 해제
        CloseProtocolConnection();
        
        // 2. 수신 스레드 정리
        if (receive_thread_running_.load()) {
            receive_thread_running_ = false;
            if (receive_thread_ && receive_thread_->joinable()) {
                receive_thread_->join();
            }
        }
        
        // 3. UDP 소켓 해제
        CloseUdpSocket();
        
        // 4. 수신 큐 정리
        {
            std::lock_guard<std::mutex> lock(receive_queue_mutex_);
            while (!receive_queue_.empty()) {
                receive_queue_.pop();
            }
        }
        
        LogMessage(PulseOne::LogLevel::INFO, "UDP connection closed successfully");
        SetConnectionState(false);
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in CloseConnection: " + std::string(e.what()));
        return false;
    }
}

bool UdpBasedWorker::CheckConnection() {
    // UDP 소켓 상태 확인
    if (udp_connection_.socket_fd == -1 || !udp_connection_.is_bound) {
        return false;
    }
    
    // 프로토콜별 연결 상태 확인
    if (!CheckProtocolConnection()) {
        return false;
    }
    
    // 수신 스레드 상태 확인
    if (!receive_thread_running_.load()) {
        return false;
    }
    
    return true;
}

bool UdpBasedWorker::SendKeepAlive() {
    // UDP 소켓 상태 확인
    if (!CheckConnection()) {
        LogMessage(PulseOne::LogLevel::WARN, "Cannot send keep-alive: connection not established");
        return false;
    }
    
    // 프로토콜별 Keep-alive 전송
    return SendProtocolKeepAlive();
}

// =============================================================================
// UDP 소켓 관리
// =============================================================================

bool UdpBasedWorker::CreateUdpSocket() {
    // 기존 소켓이 있으면 정리
    CloseUdpSocket();
    
    // UDP 소켓 생성
    udp_connection_.socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_connection_.socket_fd == -1) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to create UDP socket: " + std::string(strerror(errno)));
        return false;
    }
    
    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "UDP socket created (fd: " + std::to_string(udp_connection_.socket_fd) + ")");
    
    // 소켓 옵션 설정
    if (!SetSocketOptions()) {
        CloseUdpSocket();
        return false;
    }
    
    return true;
}

bool UdpBasedWorker::BindUdpSocket() {
    if (udp_connection_.socket_fd == -1) {
        LogMessage(PulseOne::LogLevel::ERROR, "Socket not created before binding");
        return false;
    }
    
    // 로컬 주소 설정
    memset(&udp_connection_.local_addr, 0, sizeof(udp_connection_.local_addr));
    udp_connection_.local_addr.sin_family = AF_INET;
    udp_connection_.local_addr.sin_port = htons(udp_config_.local_port);
    
    // 바인딩 인터페이스 설정
    if (udp_config_.local_interface == "0.0.0.0" || udp_config_.local_interface.empty()) {
        udp_connection_.local_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, udp_config_.local_interface.c_str(), 
                     &udp_connection_.local_addr.sin_addr) != 1) {
            LogMessage(PulseOne::LogLevel::ERROR, "Invalid local interface: " + udp_config_.local_interface);
            return false;
        }
    }
    
    // 바인딩 수행
    if (bind(udp_connection_.socket_fd, 
             reinterpret_cast<struct sockaddr*>(&udp_connection_.local_addr),
             sizeof(udp_connection_.local_addr)) == -1) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to bind UDP socket: " + std::string(strerror(errno)));
        return false;
    }
    
    udp_connection_.is_bound = true;
    
    // 실제 바인딩된 주소 확인 (포트가 자동 할당된 경우)
    socklen_t addr_len = sizeof(udp_connection_.local_addr);
    if (getsockname(udp_connection_.socket_fd,
                   reinterpret_cast<struct sockaddr*>(&udp_connection_.local_addr),
                   &addr_len) == 0) {
        LogMessage(PulseOne::LogLevel::INFO, 
                  "UDP socket bound to: " + SockAddrToString(udp_connection_.local_addr));
    }
    
    return true;
}

void UdpBasedWorker::CloseUdpSocket() {
    if (udp_connection_.socket_fd != -1) {
        close(udp_connection_.socket_fd);
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "UDP socket closed (fd: " + std::to_string(udp_connection_.socket_fd) + ")");
        udp_connection_.socket_fd = -1;
    }
    
    udp_connection_.is_bound = false;
    udp_connection_.is_connected = false;
}

// =============================================================================
// 데이터 송수신
// =============================================================================

ssize_t UdpBasedWorker::SendUdpData(const std::vector<uint8_t>& data, 
                                    const struct sockaddr_in& target_addr) {
    if (udp_connection_.socket_fd == -1) {
        LogMessage(PulseOne::LogLevel::ERROR, "Socket not created for sending data");
        UpdateErrorStats(true);
        return -1;
    }
    
    if (data.empty()) {
        LogMessage(PulseOne::LogLevel::WARN, "Attempted to send empty data");
        return 0;
    }
    
    ssize_t bytes_sent = sendto(udp_connection_.socket_fd,
                               data.data(), data.size(), 0,
                               reinterpret_cast<const struct sockaddr*>(&target_addr),
                               sizeof(target_addr));
    
    if (bytes_sent == -1) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to send UDP data: " + std::string(strerror(errno)));
        UpdateErrorStats(true);
        return -1;
    }
    
    // 통계 업데이트
    UpdateSendStats(static_cast<size_t>(bytes_sent));
    udp_connection_.last_activity = system_clock::now();
    
    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
              "Sent " + std::to_string(bytes_sent) + " bytes to " + 
              SockAddrToString(target_addr));
    
    return bytes_sent;
}

ssize_t UdpBasedWorker::SendUdpData(const std::string& data, 
                                    const std::string& target_host, uint16_t target_port) {
    struct sockaddr_in target_addr;
    if (!StringToSockAddr(target_host, target_port, target_addr)) {
        LogMessage(PulseOne::LogLevel::ERROR, "Invalid target address: " + target_host + ":" + std::to_string(target_port));
        return -1;
    }
    
    std::vector<uint8_t> data_bytes(data.begin(), data.end());
    return SendUdpData(data_bytes, target_addr);
}

ssize_t UdpBasedWorker::SendBroadcast(const std::vector<uint8_t>& data, uint16_t port) {
    if (!udp_config_.broadcast_enabled) {
        LogMessage(PulseOne::LogLevel::ERROR, "Broadcast is disabled");
        return -1;
    }
    
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(port);
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
    
    ssize_t result = SendUdpData(data, broadcast_addr);
    if (result > 0) {
        UpdateSendStats(static_cast<size_t>(result), true, false);
    }
    
    return result;
}

ssize_t UdpBasedWorker::SendMulticast(const std::vector<uint8_t>& data, 
                                      const std::string& multicast_group, uint16_t port) {
    if (!udp_config_.multicast_enabled) {
        LogMessage(PulseOne::LogLevel::ERROR, "Multicast is disabled");
        return -1;
    }
    
    struct sockaddr_in multicast_addr;
    if (!StringToSockAddr(multicast_group, port, multicast_addr)) {
        LogMessage(PulseOne::LogLevel::ERROR, "Invalid multicast address: " + multicast_group);
        return -1;
    }
    
    ssize_t result = SendUdpData(data, multicast_addr);
    if (result > 0) {
        UpdateSendStats(static_cast<size_t>(result), false, true);
    }
    
    return result;
}

bool UdpBasedWorker::ReceiveUdpData(UdpPacket& packet, uint32_t timeout_ms) {
    // 큐에서 패킷 확인
    {
        std::lock_guard<std::mutex> lock(receive_queue_mutex_);
        if (!receive_queue_.empty()) {
            packet = std::move(receive_queue_.front());
            receive_queue_.pop();
            return true;
        }
    }
    
    // 타임아웃이 0이면 즉시 반환
    if (timeout_ms == 0) {
        return false;
    }
    
    // 지정된 시간만큼 대기하며 큐 확인
    auto start_time = steady_clock::now();
    while (duration_cast<milliseconds>(steady_clock::now() - start_time).count() < timeout_ms) {
        std::this_thread::sleep_for(milliseconds(10));
        
        std::lock_guard<std::mutex> lock(receive_queue_mutex_);
        if (!receive_queue_.empty()) {
            packet = std::move(receive_queue_.front());
            receive_queue_.pop();
            return true;
        }
    }
    
    // 타임아웃
    udp_stats_.timeouts++;
    return false;
}

size_t UdpBasedWorker::GetPendingPacketCount() const {
    std::lock_guard<std::mutex> lock(receive_queue_mutex_);
    return receive_queue_.size();
}

// =============================================================================
// 유틸리티 메서드
// =============================================================================

bool UdpBasedWorker::StringToSockAddr(const std::string& ip_str, uint16_t port, 
                                      struct sockaddr_in& addr) {
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip_str.c_str(), &addr.sin_addr) != 1) {
        return false;
    }
    
    return true;
}

std::string UdpBasedWorker::SockAddrToString(const struct sockaddr_in& addr) {
    char ip_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &addr.sin_addr, ip_str, INET_ADDRSTRLEN) == nullptr) {
        return "invalid";
    }
    
    return std::string(ip_str) + ":" + std::to_string(ntohs(addr.sin_port));
}

std::string UdpBasedWorker::CalculateBroadcastAddress(const std::string& interface_ip, 
                                                      const std::string& subnet_mask) {
    struct in_addr ip_addr, mask_addr, broadcast_addr;
    
    if (inet_pton(AF_INET, interface_ip.c_str(), &ip_addr) != 1 ||
        inet_pton(AF_INET, subnet_mask.c_str(), &mask_addr) != 1) {
        return "";
    }
    
    // 브로드캐스트 주소 계산: IP | (~mask)
    broadcast_addr.s_addr = ip_addr.s_addr | (~mask_addr.s_addr);
    
    char broadcast_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &broadcast_addr, broadcast_str, INET_ADDRSTRLEN) == nullptr) {
        return "";
    }
    
    return std::string(broadcast_str);
}

// =============================================================================
// 내부 메서드 (private)
// =============================================================================

bool UdpBasedWorker::ParseUdpConfig() {
    try {
         // 1. endpoint에서 정보 파싱 (config_json 대신)
        if (device_info_.endpoint.empty()) {
            LogMessage(PulseOne::LogLevel::WARN, "No UDP endpoint configured, using defaults");
            return true;
        }
        
        // 2. endpoint 파싱
        std::string endpoint = device_info_.endpoint;
        size_t colon_pos = endpoint.find(':');
        
        if (colon_pos != std::string::npos) {
            udp_config_.remote_ip = endpoint.substr(0, colon_pos);
            udp_config_.remote_port = static_cast<uint16_t>(std::stoi(endpoint.substr(colon_pos + 1)));
        } else {
            udp_config_.remote_ip = endpoint;
            udp_config_.remote_port = 502;  // 기본 포트
        }
        
        // 3. Duration을 밀리초로 변환 (timeout_ms 대신)
        udp_config_.socket_timeout_ms = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(device_info_.timeout).count()
        );
        
        // 4. 폴링 간격도 Duration에서 변환 (polling_interval_ms 대신)
        udp_config_.polling_interval_ms = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(device_info_.polling_interval).count()
        );
        
        // 5. 재시도 횟수는 그대로 사용
        udp_config_.max_retries = static_cast<uint32_t>(device_info_.retry_count);
        
        LogMessage(PulseOne::LogLevel::INFO, "UDP config parsed from DeviceInfo successfully");
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to parse UDP config: " + std::string(e.what()));
        return false;
    }
}

bool UdpBasedWorker::SetSocketOptions() {
    if (udp_connection_.socket_fd == -1) {
        return false;
    }
    
    int opt = 1;
    
    // SO_REUSEADDR 설정
    if (udp_config_.reuse_address) {
        if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_REUSEADDR, 
                      &opt, sizeof(opt)) == -1) {
            LogMessage(PulseOne::LogLevel::WARN, "Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
        }
    }
    
    // SO_REUSEPORT 설정 (Linux 전용)
#ifdef SO_REUSEPORT
    if (udp_config_.reuse_port) {
        if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_REUSEPORT, 
                      &opt, sizeof(opt)) == -1) {
            LogMessage(PulseOne::LogLevel::WARN, "Failed to set SO_REUSEPORT: " + std::string(strerror(errno)));
        }
    }
#endif
    
    // 브로드캐스트 허용
    if (udp_config_.broadcast_enabled) {
        if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_BROADCAST, 
                      &opt, sizeof(opt)) == -1) {
            LogMessage(PulseOne::LogLevel::WARN, "Failed to enable broadcast: " + std::string(strerror(errno)));
        }
    }
    
    // 수신 버퍼 크기 설정
    int recv_buffer_size = static_cast<int>(udp_config_.receive_buffer_size);
    if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_RCVBUF, 
                  &recv_buffer_size, sizeof(recv_buffer_size)) == -1) {
        LogMessage(PulseOne::LogLevel::WARN, "Failed to set receive buffer size: " + std::string(strerror(errno)));
    }
    
    // 송신 버퍼 크기 설정
    int send_buffer_size = static_cast<int>(udp_config_.send_buffer_size);
    if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_SNDBUF, 
                  &send_buffer_size, sizeof(send_buffer_size)) == -1) {
        LogMessage(PulseOne::LogLevel::WARN, "Failed to set send buffer size: " + std::string(strerror(errno)));
    }
    
    // 소켓 타임아웃 설정
    struct timeval timeout;
    timeout.tv_sec = udp_config_.socket_timeout_ms / 1000;
    timeout.tv_usec = (udp_config_.socket_timeout_ms % 1000) * 1000;
    
    if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_RCVTIMEO, 
                  &timeout, sizeof(timeout)) == -1) {
        LogMessage(PulseOne::LogLevel::WARN, "Failed to set receive timeout: " + std::string(strerror(errno)));
    }
    
    if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_SNDTIMEO, 
                  &timeout, sizeof(timeout)) == -1) {
        LogMessage(PulseOne::LogLevel::WARN, "Failed to set send timeout: " + std::string(strerror(errno)));
    }
    
    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Socket options configured successfully");
    return true;
}

void UdpBasedWorker::ReceiveThreadFunction() {
    LogMessage(PulseOne::LogLevel::INFO, "UDP receive thread started");
    
    uint8_t buffer[65536]; // 최대 UDP 패킷 크기
    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);
    
    while (receive_thread_running_.load()) {
        // select()를 사용하여 논블로킹 수신
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(udp_connection_.socket_fd, &read_fds);
        
        struct timeval select_timeout;
        select_timeout.tv_sec = 0;
        select_timeout.tv_usec = 100000; // 100ms
        
        int select_result = select(udp_connection_.socket_fd + 1, &read_fds, nullptr, nullptr, &select_timeout);
        
        if (select_result == -1) {
            if (errno != EINTR) {
                LogMessage(PulseOne::LogLevel::ERROR, "Select error in receive thread: " + std::string(strerror(errno)));
                UpdateErrorStats(false);
            }
            continue;
        }
        
        if (select_result == 0) {
            // 타임아웃, 계속 진행
            continue;
        }
        
        if (FD_ISSET(udp_connection_.socket_fd, &read_fds)) {
            // 데이터 수신
            ssize_t bytes_received = recvfrom(udp_connection_.socket_fd,
                                             buffer, sizeof(buffer), 0,
                                             reinterpret_cast<struct sockaddr*>(&sender_addr),
                                             &sender_addr_len);
            
            if (bytes_received == -1) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    LogMessage(PulseOne::LogLevel::ERROR, "Receive error: " + std::string(strerror(errno)));
                    UpdateErrorStats(false);
                }
                continue;
            }
            
            if (bytes_received > 0) {
                // 패킷 생성
                UdpPacket packet;
                packet.data.assign(buffer, buffer + bytes_received);
                packet.sender_addr = sender_addr;
                packet.timestamp = system_clock::now();
                packet.data_length = static_cast<size_t>(bytes_received);
                
                // 통계 업데이트
                UpdateReceiveStats(static_cast<size_t>(bytes_received));
                udp_connection_.last_activity = system_clock::now();
                
                LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                          "Received " + std::to_string(bytes_received) + " bytes from " + 
                          SockAddrToString(sender_addr));
                
                // 프로토콜별 처리
                ProcessReceivedPacket(packet);
                
                // 큐에 추가 (큐 크기 제한)
                {
                    std::lock_guard<std::mutex> lock(receive_queue_mutex_);
                    if (receive_queue_.size() < 1000) { // 최대 큐 크기
                        receive_queue_.push(std::move(packet));
                    } else {
                        LogMessage(PulseOne::LogLevel::WARN, "Receive queue full, dropping packet");
                    }
                }
            }
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "UDP receive thread stopped");
}

void UdpBasedWorker::UpdateSendStats(size_t bytes_sent, bool is_broadcast, bool is_multicast) {
    udp_stats_.packets_sent++;
    udp_stats_.bytes_sent += bytes_sent;
    
    if (is_broadcast) {
        udp_stats_.broadcast_packets++;
    }
    
    if (is_multicast) {
        udp_stats_.multicast_packets++;
    }
}

void UdpBasedWorker::UpdateReceiveStats(size_t bytes_received, bool is_broadcast, bool is_multicast) {
    udp_stats_.packets_received++;
    udp_stats_.bytes_received += bytes_received;
    
    if (is_broadcast) {
        udp_stats_.broadcast_packets++;
    }
    
    if (is_multicast) {
        udp_stats_.multicast_packets++;
    }
}

void UdpBasedWorker::UpdateErrorStats(bool is_send_error) {
    if (is_send_error) {
        udp_stats_.send_errors++;
    } else {
        udp_stats_.receive_errors++;
    }
}

} // namespace Workers
} // namespace PulseOne