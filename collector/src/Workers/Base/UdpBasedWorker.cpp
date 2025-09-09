/**
 * @file UdpBasedWorker.cpp - Windows 크로스 플랫폼 호환성 완전 수정
 * @brief UDP 기반 디바이스 워커 클래스 구현 - MinGW 완전 대응
 * @author PulseOne Development Team  
 * @date 2025-01-23
 * @version 1.2.0 - 헤더와 완전 동기화
 */

// =============================================================================
// UniqueId 충돌 방지 및 플랫폼 호환성 (가장 먼저!)
// =============================================================================
#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    // UniqueId 충돌 방지
    #define UniqueId _WIN32_UniqueId_BACKUP
#endif

#include "Platform/PlatformCompat.h"
#include "Workers/Base/UdpBasedWorker.h"

#ifdef _WIN32
    #undef UniqueId
#endif

#include "Utils/LogManager.h"
#include "Common/Enums.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <thread>

// =============================================================================
// 플랫폼별 네트워크 헤더 (조건부 include)
// =============================================================================
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    // Windows용 select 정의는 이미 winsock2.h에 포함됨
#else
    #include <sys/select.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <ifaddrs.h>
#endif

using namespace std::chrono;
using LogLevel = PulseOne::Enums::LogLevel;

namespace PulseOne {
namespace Workers {

// =============================================================================
// 정적 멤버 초기화 (Windows 전용)
// =============================================================================
#if PULSEONE_WINDOWS
bool UdpBasedWorker::winsock_initialized_ = false;
std::mutex UdpBasedWorker::winsock_mutex_;
#endif

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

UdpBasedWorker::UdpBasedWorker(const PulseOne::Structs::DeviceInfo& device_info)
    : BaseDeviceWorker(device_info)    
    , receive_thread_running_(false) {
    
    // UDP 통계 초기화
    udp_stats_.start_time = system_clock::now();
    udp_stats_.last_reset = udp_stats_.start_time;
    
    // UDP 연결 정보 초기화  
    udp_connection_.last_activity = system_clock::now();
    
#ifdef _WIN32
    // Windows에서 Winsock 초기화
    if (!InitializeWinsock()) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to initialize Winsock for UDP");
    }
#endif
    
    LogMessage(LogLevel::INFO, "UdpBasedWorker created for device: " + device_info.name);
    
    // device_info에서 UDP 설정 파싱
    if (!ParseUdpConfig()) {
        LogMessage(LogLevel::WARN, "Failed to parse UDP config, using defaults");
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
    
#ifdef _WIN32
    // Windows에서 Winsock 정리 (전역)
    CleanupWinsock();
#endif
    
    LogMessage(LogLevel::INFO, "UdpBasedWorker destroyed for device: " + device_info_.name);
}

// =============================================================================
// UDP 설정 관리 (공개 인터페이스)
// =============================================================================

void UdpBasedWorker::ConfigureUdp(const UdpConfig& config) {
    std::lock_guard<std::mutex> lock(udp_config_mutex_);
    udp_config_ = config;
    LogMessage(LogLevel::INFO, "UDP configuration updated");
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
    {
        std::lock_guard<std::mutex> lock(udp_config_mutex_);
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
    }
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
    
    LogMessage(LogLevel::INFO, "UDP statistics reset");
}

// =============================================================================
// BaseDeviceWorker 순수 가상 함수 구현
// =============================================================================

bool UdpBasedWorker::EstablishConnection() {
    LogMessage(LogLevel::INFO, "Establishing UDP connection...");
    
    try {
        // 1. UDP 소켓 생성 및 설정
        if (!CreateUdpSocket()) {
            LogMessage(LogLevel::LOG_ERROR, "Failed to create UDP socket");
            return false;
        }
        
        // 2. UDP 소켓 바인딩
        if (!BindUdpSocket()) {
            LogMessage(LogLevel::LOG_ERROR, "Failed to bind UDP socket");
            CloseUdpSocket();
            return false;
        }
        
        // 3. 수신 스레드 시작
        receive_thread_running_ = true;
        receive_thread_ = std::make_unique<std::thread>(&UdpBasedWorker::ReceiveThreadFunction, this);
        
        // 4. 프로토콜별 연결 수립
        if (!EstablishProtocolConnection()) {
            LogMessage(LogLevel::LOG_ERROR, "Failed to establish protocol connection");
            receive_thread_running_ = false;
            if (receive_thread_->joinable()) {
                receive_thread_->join();
            }
            CloseUdpSocket();
            return false;
        }
        
        LogMessage(LogLevel::INFO, "UDP connection established successfully");
        SetConnectionState(true);
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::LOG_ERROR, "Exception in EstablishConnection: " + std::string(e.what()));
        CloseConnection();
        return false;
    }
}

bool UdpBasedWorker::CloseConnection() {
    LogMessage(LogLevel::INFO, "Closing UDP connection...");
    
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
        
        LogMessage(LogLevel::INFO, "UDP connection closed successfully");
        SetConnectionState(false);
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::LOG_ERROR, "Exception in CloseConnection: " + std::string(e.what()));
        return false;
    }
}

bool UdpBasedWorker::CheckConnection() {
    // UDP 소켓 상태 확인
#ifdef _WIN32
    if (udp_connection_.socket_fd == INVALID_SOCKET || !udp_connection_.is_bound) {
        return false;
    }
#else
    if (udp_connection_.socket_fd == -1 || !udp_connection_.is_bound) {
        return false;
    }
#endif
    
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
        LogMessage(LogLevel::WARN, "Cannot send keep-alive: connection not established");
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
#ifdef _WIN32
    udp_connection_.socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_connection_.socket_fd == INVALID_SOCKET) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to create UDP socket: " + SocketErrorToString(WSAGetLastError()));
        return false;
    }
#else
    udp_connection_.socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_connection_.socket_fd == -1) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to create UDP socket: " + std::string(strerror(errno)));
        return false;
    }
#endif
    
    LogMessage(LogLevel::DEBUG_LEVEL, "UDP socket created (fd: " + std::to_string(udp_connection_.socket_fd) + ")");
    
    // 소켓 옵션 설정
    if (!SetSocketOptions()) {
        CloseUdpSocket();
        return false;
    }
    
    return true;
}

bool UdpBasedWorker::BindUdpSocket() {
#ifdef _WIN32
    if (udp_connection_.socket_fd == INVALID_SOCKET) {
        LogMessage(LogLevel::LOG_ERROR, "Socket not created before binding");
        return false;
    }
#else
    if (udp_connection_.socket_fd == -1) {
        LogMessage(LogLevel::LOG_ERROR, "Socket not created before binding");
        return false;
    }
#endif
    
    // 로컬 주소 설정
    memset(&udp_connection_.local_addr, 0, sizeof(udp_connection_.local_addr));
    udp_connection_.local_addr.sin_family = AF_INET;
    
    // UDP 설정 안전하게 접근
    uint16_t local_port;
    std::string local_interface;
    {
        std::lock_guard<std::mutex> lock(udp_config_mutex_);
        local_port = udp_config_.local_port;
        local_interface = udp_config_.local_interface;
    }
    
    udp_connection_.local_addr.sin_port = htons(local_port);
    
    // 바인딩 인터페이스 설정
    if (local_interface == "0.0.0.0" || local_interface.empty()) {
        udp_connection_.local_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        // MinGW 호환성을 위해 inet_addr 사용
#ifdef _WIN32
        udp_connection_.local_addr.sin_addr.s_addr = inet_addr(local_interface.c_str());
        if (udp_connection_.local_addr.sin_addr.s_addr == INADDR_NONE) {
            LogMessage(LogLevel::LOG_ERROR, "Invalid local interface: " + local_interface);
            return false;
        }
#else
        if (inet_pton(AF_INET, local_interface.c_str(), 
                     &udp_connection_.local_addr.sin_addr) != 1) {
            LogMessage(LogLevel::LOG_ERROR, "Invalid local interface: " + local_interface);
            return false;
        }
#endif
    }
    
    // 바인딩 수행
#ifdef _WIN32
    if (bind(udp_connection_.socket_fd, 
             reinterpret_cast<struct sockaddr*>(&udp_connection_.local_addr),
             sizeof(udp_connection_.local_addr)) == SOCKET_ERROR) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to bind UDP socket: " + SocketErrorToString(WSAGetLastError()));
        return false;
    }
#else
    if (bind(udp_connection_.socket_fd, 
             reinterpret_cast<struct sockaddr*>(&udp_connection_.local_addr),
             sizeof(udp_connection_.local_addr)) == -1) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to bind UDP socket: " + std::string(strerror(errno)));
        return false;
    }
#endif
    
    udp_connection_.is_bound = true;
    
    // 실제 바인딩된 주소 확인 (포트가 자동 할당된 경우)
    socklen_t addr_len = sizeof(udp_connection_.local_addr);
    if (getsockname(udp_connection_.socket_fd,
                   reinterpret_cast<struct sockaddr*>(&udp_connection_.local_addr),
                   &addr_len) == 0) {
        LogMessage(LogLevel::INFO, 
                  "UDP socket bound to: " + SockAddrToString(udp_connection_.local_addr));
    }
    
    return true;
}

void UdpBasedWorker::CloseUdpSocket() {
#ifdef _WIN32
    if (udp_connection_.socket_fd != INVALID_SOCKET) {
        closesocket(udp_connection_.socket_fd);
        LogMessage(LogLevel::DEBUG_LEVEL, "UDP socket closed (fd: " + std::to_string(udp_connection_.socket_fd) + ")");
        udp_connection_.socket_fd = INVALID_SOCKET;
    }
#else
    if (udp_connection_.socket_fd != -1) {
        close(udp_connection_.socket_fd);
        LogMessage(LogLevel::DEBUG_LEVEL, "UDP socket closed (fd: " + std::to_string(udp_connection_.socket_fd) + ")");
        udp_connection_.socket_fd = -1;
    }
#endif
    
    udp_connection_.is_bound = false;
    udp_connection_.is_connected = false;
}

// =============================================================================
// 데이터 송수신 (플랫폼별 반환 타입)
// =============================================================================

#if PULSEONE_WINDOWS
int UdpBasedWorker::SendUdpData(const std::vector<uint8_t>& data, 
                                const struct sockaddr_in& target_addr) {
#else
ssize_t UdpBasedWorker::SendUdpData(const std::vector<uint8_t>& data, 
                                    const struct sockaddr_in& target_addr) {
#endif

#ifdef _WIN32
    if (udp_connection_.socket_fd == INVALID_SOCKET) {
        LogMessage(LogLevel::LOG_ERROR, "Socket not created for sending data");
        UpdateErrorStats(true);
        return -1;
    }
#else
    if (udp_connection_.socket_fd == -1) {
        LogMessage(LogLevel::LOG_ERROR, "Socket not created for sending data");
        UpdateErrorStats(true);
        return -1;
    }
#endif
    
    if (data.empty()) {
        LogMessage(LogLevel::WARN, "Attempted to send empty data");
        return 0;
    }
    
#ifdef _WIN32
    int bytes_sent = sendto(udp_connection_.socket_fd,
                           reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0,
                           reinterpret_cast<const struct sockaddr*>(&target_addr),
                           sizeof(target_addr));
    
    if (bytes_sent == SOCKET_ERROR) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to send UDP data: " + SocketErrorToString(WSAGetLastError()));
        UpdateErrorStats(true);
        return -1;
    }
#else
    ssize_t bytes_sent = sendto(udp_connection_.socket_fd,
                               data.data(), data.size(), 0,
                               reinterpret_cast<const struct sockaddr*>(&target_addr),
                               sizeof(target_addr));
    
    if (bytes_sent == -1) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to send UDP data: " + std::string(strerror(errno)));
        UpdateErrorStats(true);
        return -1;
    }
#endif
    
    // 통계 업데이트
    UpdateSendStats(static_cast<size_t>(bytes_sent));
    udp_connection_.last_activity = system_clock::now();
    
    LogMessage(LogLevel::DEBUG_LEVEL, 
              "Sent " + std::to_string(bytes_sent) + " bytes to " + 
              SockAddrToString(target_addr));
    
    return bytes_sent;
}

#if PULSEONE_WINDOWS
int UdpBasedWorker::SendUdpData(const std::string& data, 
                                const std::string& target_host, uint16_t target_port) {
#else
ssize_t UdpBasedWorker::SendUdpData(const std::string& data, 
                                    const std::string& target_host, uint16_t target_port) {
#endif
    struct sockaddr_in target_addr;
    if (!StringToSockAddr(target_host, target_port, target_addr)) {
        LogMessage(LogLevel::LOG_ERROR, "Invalid target address: " + target_host + ":" + std::to_string(target_port));
        return -1;
    }
    
    std::vector<uint8_t> data_bytes(data.begin(), data.end());
    return SendUdpData(data_bytes, target_addr);
}

#if PULSEONE_WINDOWS
int UdpBasedWorker::SendBroadcast(const std::vector<uint8_t>& data, uint16_t port) {
#else
ssize_t UdpBasedWorker::SendBroadcast(const std::vector<uint8_t>& data, uint16_t port) {
#endif
    bool broadcast_enabled;
    {
        std::lock_guard<std::mutex> lock(udp_config_mutex_);
        broadcast_enabled = udp_config_.broadcast_enabled;
    }
    
    if (!broadcast_enabled) {
        LogMessage(LogLevel::LOG_ERROR, "Broadcast is disabled");
        return -1;
    }
    
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(port);
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
    
    auto result = SendUdpData(data, broadcast_addr);
    if (result > 0) {
        UpdateSendStats(static_cast<size_t>(result), true, false);
    }
    
    return result;
}

#if PULSEONE_WINDOWS
int UdpBasedWorker::SendMulticast(const std::vector<uint8_t>& data, 
                                  const std::string& multicast_group, uint16_t port) {
#else
ssize_t UdpBasedWorker::SendMulticast(const std::vector<uint8_t>& data, 
                                      const std::string& multicast_group, uint16_t port) {
#endif
    bool multicast_enabled;
    {
        std::lock_guard<std::mutex> lock(udp_config_mutex_);
        multicast_enabled = udp_config_.multicast_enabled;
    }
    
    if (!multicast_enabled) {
        LogMessage(LogLevel::LOG_ERROR, "Multicast is disabled");
        return -1;
    }
    
    struct sockaddr_in multicast_addr;
    if (!StringToSockAddr(multicast_group, port, multicast_addr)) {
        LogMessage(LogLevel::LOG_ERROR, "Invalid multicast address: " + multicast_group);
        return -1;
    }
    
    auto result = SendUdpData(data, multicast_addr);
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
    
    // MinGW 호환성을 위해 inet_addr 사용
#ifdef _WIN32
    addr.sin_addr.s_addr = inet_addr(ip_str.c_str());
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        return false;
    }
#else
    if (inet_pton(AF_INET, ip_str.c_str(), &addr.sin_addr) != 1) {
        return false;
    }
#endif
    
    return true;
}

std::string UdpBasedWorker::SockAddrToString(const struct sockaddr_in& addr) {
#ifdef _WIN32
    // Windows에서는 inet_ntoa 사용
    std::string ip = inet_ntoa(addr.sin_addr);
#else
    char ip_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &addr.sin_addr, ip_str, INET_ADDRSTRLEN) == nullptr) {
        return "invalid";
    }
    std::string ip = ip_str;
#endif
    
    return ip + ":" + std::to_string(ntohs(addr.sin_port));
}

std::string UdpBasedWorker::CalculateBroadcastAddress(const std::string& interface_ip, 
                                                      const std::string& subnet_mask) {
    struct in_addr ip_addr, mask_addr, broadcast_addr;
    
#ifdef _WIN32
    // Windows: inet_addr 사용
    ip_addr.s_addr = inet_addr(interface_ip.c_str());
    mask_addr.s_addr = inet_addr(subnet_mask.c_str());
    
    if (ip_addr.s_addr == INADDR_NONE || mask_addr.s_addr == INADDR_NONE) {
        return "";
    }
#else
    if (inet_pton(AF_INET, interface_ip.c_str(), &ip_addr) != 1 ||
        inet_pton(AF_INET, subnet_mask.c_str(), &mask_addr) != 1) {
        return "";
    }
#endif
    
    // 브로드캐스트 주소 계산: IP | (~mask)
    broadcast_addr.s_addr = ip_addr.s_addr | (~mask_addr.s_addr);
    
#ifdef _WIN32
    return std::string(inet_ntoa(broadcast_addr));
#else
    char broadcast_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &broadcast_addr, broadcast_str, INET_ADDRSTRLEN) == nullptr) {
        return "";
    }
    return std::string(broadcast_str);
#endif
}

// =============================================================================
// 내부 메서드 (private)
// =============================================================================

bool UdpBasedWorker::ParseUdpConfig() {
    try {
        // 1. endpoint에서 정보 파싱
        if (device_info_.endpoint.empty()) {
            LogMessage(LogLevel::WARN, "No UDP endpoint configured, using defaults");
            return true;
        }
        
        // 2. endpoint 파싱 (IP:PORT 형식)
        std::string endpoint = device_info_.endpoint;
        size_t colon_pos = endpoint.find(':');
        
        {
            std::lock_guard<std::mutex> lock(udp_config_mutex_);
            if (colon_pos != std::string::npos) {
                udp_config_.remote_ip = endpoint.substr(0, colon_pos);
                udp_config_.remote_port = static_cast<uint16_t>(std::stoi(endpoint.substr(colon_pos + 1)));
            } else {
                udp_config_.remote_ip = endpoint;
                udp_config_.remote_port = 502;  // 기본 포트
            }
            
            // 3. DeviceInfo의 int 필드 직접 사용
            udp_config_.socket_timeout_ms = static_cast<uint32_t>(device_info_.timeout_ms);
            udp_config_.polling_interval_ms = static_cast<uint32_t>(device_info_.polling_interval_ms);
            udp_config_.max_retries = static_cast<uint32_t>(device_info_.retry_count);
            
            // 4. 기본값 검증 및 보정
            if (udp_config_.socket_timeout_ms == 0) {
                udp_config_.socket_timeout_ms = 5000;  // 5초 기본값
            }
            
            if (udp_config_.polling_interval_ms == 0) {
                udp_config_.polling_interval_ms = 1000;  // 1초 기본값
            }
        }
        
        LogMessage(LogLevel::INFO, 
                  "UDP config parsed - timeout: " + std::to_string(udp_config_.socket_timeout_ms) + 
                  "ms, polling: " + std::to_string(udp_config_.polling_interval_ms) + "ms");
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to parse UDP config: " + std::string(e.what()));
        return false;
    }
}

bool UdpBasedWorker::SetSocketOptions() {
#ifdef _WIN32
    if (udp_connection_.socket_fd == INVALID_SOCKET) {
        return false;
    }
#else
    if (udp_connection_.socket_fd == -1) {
        return false;
    }
#endif
    
    int opt = 1;
    
    // SO_REUSEADDR 설정
    bool reuse_address;
    {
        std::lock_guard<std::mutex> lock(udp_config_mutex_);
        reuse_address = udp_config_.reuse_address;
    }
    
    if (reuse_address) {
#ifdef _WIN32
        if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_REUSEADDR, 
                      reinterpret_cast<const char*>(&opt), sizeof(opt)) == SOCKET_ERROR) {
            LogMessage(LogLevel::WARN, "Failed to set SO_REUSEADDR: " + SocketErrorToString(WSAGetLastError()));
        }
#else
        if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_REUSEADDR, 
                      &opt, sizeof(opt)) == -1) {
            LogMessage(LogLevel::WARN, "Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
        }
#endif
    }
    
    // SO_REUSEPORT 설정 (Linux 전용)
#ifdef SO_REUSEPORT
    bool reuse_port;
    {
        std::lock_guard<std::mutex> lock(udp_config_mutex_);
        reuse_port = udp_config_.reuse_port;
    }
    if (reuse_port) {
        if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_REUSEPORT, 
                      &opt, sizeof(opt)) == -1) {
            LogMessage(LogLevel::WARN, "Failed to set SO_REUSEPORT: " + std::string(strerror(errno)));
        }
    }
#endif
    
    // 브로드캐스트 허용
    bool broadcast_enabled;
    {
        std::lock_guard<std::mutex> lock(udp_config_mutex_);
        broadcast_enabled = udp_config_.broadcast_enabled;
    }
    
    if (broadcast_enabled) {
#ifdef _WIN32
        if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_BROADCAST, 
                      reinterpret_cast<const char*>(&opt), sizeof(opt)) == SOCKET_ERROR) {
            LogMessage(LogLevel::WARN, "Failed to enable broadcast: " + SocketErrorToString(WSAGetLastError()));
        }
#else
        if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_BROADCAST, 
                      &opt, sizeof(opt)) == -1) {
            LogMessage(LogLevel::WARN, "Failed to enable broadcast: " + std::string(strerror(errno)));
        }
#endif
    }
    
    // 버퍼 크기 설정
    uint32_t recv_buffer_size, send_buffer_size, socket_timeout_ms;
    {
        std::lock_guard<std::mutex> lock(udp_config_mutex_);
        recv_buffer_size = udp_config_.receive_buffer_size;
        send_buffer_size = udp_config_.send_buffer_size;
        socket_timeout_ms = udp_config_.socket_timeout_ms;
    }
    
    // 수신 버퍼 크기 설정
    int recv_buf_size = static_cast<int>(recv_buffer_size);
#ifdef _WIN32
    if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_RCVBUF, 
                  reinterpret_cast<const char*>(&recv_buf_size), sizeof(recv_buf_size)) == SOCKET_ERROR) {
        LogMessage(LogLevel::WARN, "Failed to set receive buffer size: " + SocketErrorToString(WSAGetLastError()));
    }
#else
    if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_RCVBUF, 
                  &recv_buf_size, sizeof(recv_buf_size)) == -1) {
        LogMessage(LogLevel::WARN, "Failed to set receive buffer size: " + std::string(strerror(errno)));
    }
#endif
    
    // 송신 버퍼 크기 설정
    int send_buf_size = static_cast<int>(send_buffer_size);
#ifdef _WIN32
    if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_SNDBUF, 
                  reinterpret_cast<const char*>(&send_buf_size), sizeof(send_buf_size)) == SOCKET_ERROR) {
        LogMessage(LogLevel::WARN, "Failed to set send buffer size: " + SocketErrorToString(WSAGetLastError()));
    }
#else
    if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_SNDBUF, 
                  &send_buf_size, sizeof(send_buf_size)) == -1) {
        LogMessage(LogLevel::WARN, "Failed to set send buffer size: " + std::string(strerror(errno)));
    }
#endif
    
    // 소켓 타임아웃 설정
#ifdef _WIN32
    DWORD timeout = socket_timeout_ms;
    if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_RCVTIMEO, 
                  reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == SOCKET_ERROR) {
        LogMessage(LogLevel::WARN, "Failed to set receive timeout: " + SocketErrorToString(WSAGetLastError()));
    }
    
    if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_SNDTIMEO, 
                  reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == SOCKET_ERROR) {
        LogMessage(LogLevel::WARN, "Failed to set send timeout: " + SocketErrorToString(WSAGetLastError()));
    }
#else
    struct timeval timeout;
    timeout.tv_sec = socket_timeout_ms / 1000;
    timeout.tv_usec = (socket_timeout_ms % 1000) * 1000;
    
    if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_RCVTIMEO, 
                  &timeout, sizeof(timeout)) == -1) {
        LogMessage(LogLevel::WARN, "Failed to set receive timeout: " + std::string(strerror(errno)));
    }
    
    if (setsockopt(udp_connection_.socket_fd, SOL_SOCKET, SO_SNDTIMEO, 
                  &timeout, sizeof(timeout)) == -1) {
        LogMessage(LogLevel::WARN, "Failed to set send timeout: " + std::string(strerror(errno)));
    }
#endif
    
    LogMessage(LogLevel::DEBUG_LEVEL, "Socket options configured successfully");
    return true;
}

void UdpBasedWorker::ReceiveThreadFunction() {
    LogMessage(LogLevel::INFO, "UDP receive thread started");
    
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
        
#ifdef _WIN32
        int select_result = select(0, &read_fds, nullptr, nullptr, &select_timeout);
        
        if (select_result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error != WSAEINTR) {
                LogMessage(LogLevel::LOG_ERROR, "Select error in receive thread: " + SocketErrorToString(error));
                UpdateErrorStats(false);
            }
            continue;
        }
#else
        int select_result = select(udp_connection_.socket_fd + 1, &read_fds, nullptr, nullptr, &select_timeout);
        
        if (select_result == -1) {
            if (errno != EINTR) {
                LogMessage(LogLevel::LOG_ERROR, "Select error in receive thread: " + std::string(strerror(errno)));
                UpdateErrorStats(false);
            }
            continue;
        }
#endif
        
        if (select_result == 0) {
            // 타임아웃, 계속 진행
            continue;
        }
        
        if (FD_ISSET(udp_connection_.socket_fd, &read_fds)) {
            // 데이터 수신
#ifdef _WIN32
            int bytes_received = recvfrom(udp_connection_.socket_fd,
                                         reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                                         reinterpret_cast<struct sockaddr*>(&sender_addr),
                                         &sender_addr_len);
            
            if (bytes_received == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error != WSAEWOULDBLOCK) {
                    LogMessage(LogLevel::LOG_ERROR, "Receive error: " + SocketErrorToString(error));
                    UpdateErrorStats(false);
                }
                continue;
            }
#else
            ssize_t bytes_received = recvfrom(udp_connection_.socket_fd,
                                             buffer, sizeof(buffer), 0,
                                             reinterpret_cast<struct sockaddr*>(&sender_addr),
                                             &sender_addr_len);
            
            if (bytes_received == -1) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    LogMessage(LogLevel::LOG_ERROR, "Receive error: " + std::string(strerror(errno)));
                    UpdateErrorStats(false);
                }
                continue;
            }
#endif
            
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
                
                LogMessage(LogLevel::DEBUG_LEVEL, 
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
                        LogMessage(LogLevel::WARN, "Receive queue full, dropping packet");
                    }
                }
            }
        }
    }
    
    LogMessage(LogLevel::INFO, "UDP receive thread stopped");
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

// =============================================================================
// 플랫폼별 메서드 구현
// =============================================================================

#ifdef _WIN32
std::string UdpBasedWorker::SocketErrorToString(int error_code) const {
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
        case WSAEINTR: return "Interrupted system call";
        default: return "Winsock error " + std::to_string(error_code);
    }
}

bool UdpBasedWorker::InitializeWinsock() {
    std::lock_guard<std::mutex> lock(winsock_mutex_);
    
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

void UdpBasedWorker::CleanupWinsock() {
    // 정적 정리는 프로세스 종료 시에만 수행 (개별 인스턴스에서는 호출하지 않음)
    // 여러 UdpBasedWorker 인스턴스가 공유하므로
}
#endif

} // namespace Workers
} // namespace PulseOne