/**
 * @file SerialBasedWorker.cpp
 * @brief 시리얼 통신 기반 프로토콜 워커의 구현
 * @author PulseOne Development Team
 * @date 2025-01-22
 * @version 1.0.0
 */

#include "Workers/Base/SerialBasedWorker.h"
#include "Utils/LogManager.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>      // strerror, memset 함수용
#include <cerrno>       // errno 변수용
#include <fcntl.h>      // open, O_RDWR, O_NOCTTY
#include <unistd.h>     // close, read, write
#include <sys/ioctl.h>  // ioctl

using namespace std::chrono;

namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

SerialBasedWorker::SerialBasedWorker(const PulseOne::DeviceInfo& device_info,
                                     std::shared_ptr<RedisClient> redis_client,
                                     std::shared_ptr<InfluxClient> influx_client)
    : BaseDeviceWorker(device_info, redis_client, influx_client)
    , serial_fd_(-1) {
    
    // endpoint에서 시리얼 설정 파싱 (device_info_는 이제 protected)
    ParseEndpoint();
    
    // BaseDeviceWorker 재연결 설정 활용
    ReconnectionSettings settings;
    settings.auto_reconnect_enabled = true;
    settings.retry_interval_ms = 3000;          // 시리얼은 빠른 재시도
    settings.max_retries_per_cycle = 20;        // 시리얼은 더 많은 재시도
    settings.keep_alive_enabled = true;
    settings.keep_alive_interval_seconds = 45;  // 시리얼 Keep-alive
    UpdateReconnectionSettings(settings);
    
    LogMessage(PulseOne::LogLevel::INFO, "SerialBasedWorker created for device: " + device_info_.name);
}

SerialBasedWorker::~SerialBasedWorker() {
    CloseSerialPort();
    LogMessage(PulseOne::LogLevel::INFO, "SerialBasedWorker destroyed");
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현 (시리얼 특화)
// =============================================================================

bool SerialBasedWorker::EstablishConnection() {
    LogMessage(PulseOne::LogLevel::INFO, "Establishing serial connection to " + serial_config_.port_name);
    
    // 시리얼 설정 검증
    if (!ValidateSerialConfig()) {
        LogMessage(PulseOne::LogLevel::ERROR, "Invalid serial configuration");
        return false;
    }
    
    // 시리얼 포트 열기
    if (!OpenSerialPort()) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to open serial port");
        return false;
    }
    
    // 파생 클래스의 프로토콜별 연결 수립
    if (!EstablishProtocolConnection()) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to establish protocol connection");
        CloseSerialPort();
        return false;
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "Serial connection established successfully");
    return true;
}

bool SerialBasedWorker::CloseConnection() {
    LogMessage(PulseOne::LogLevel::INFO, "Closing serial connection");
    
    // 프로토콜별 연결 해제 먼저
    CloseProtocolConnection();
    
    // 시리얼 포트 해제
    CloseSerialPort();
    
    LogMessage(PulseOne::LogLevel::INFO, "Serial connection closed");
    return true;
}

bool SerialBasedWorker::CheckConnection() {
    // 시리얼 포트 상태 확인
    if (!IsSerialPortOpen()) {
        return false;
    }
    
    // 프로토콜별 연결 상태 확인
    return CheckProtocolConnection();
}

bool SerialBasedWorker::SendKeepAlive() {
    // 시리얼 포트 상태 먼저 확인
    if (!IsSerialPortOpen()) {
        LogMessage(PulseOne::LogLevel::WARN, "Serial port not open for keep-alive");
        return false;
    }
    
    // 프로토콜별 Keep-alive 전송
    return SendProtocolKeepAlive();
}

// =============================================================================
// 시리얼 설정 관리
// =============================================================================

void SerialBasedWorker::ConfigureSerial(const SerialConfig& config) {
    serial_config_ = config;
    
    LogMessage(PulseOne::LogLevel::INFO, 
               "Serial configured - " + config.port_name + 
               " " + std::to_string(config.baud_rate) + 
               "-" + std::to_string(config.data_bits) + 
               "-" + std::string(1, config.parity) + 
               "-" + std::to_string(config.stop_bits));
}

std::string SerialBasedWorker::GetSerialConnectionInfo() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"serial_connection\": {\n";
    ss << "    \"port_name\": \"" << serial_config_.port_name << "\",\n";
    ss << "    \"baud_rate\": " << serial_config_.baud_rate << ",\n";
    ss << "    \"data_bits\": " << serial_config_.data_bits << ",\n";
    ss << "    \"stop_bits\": " << serial_config_.stop_bits << ",\n";
    ss << "    \"parity\": \"" << serial_config_.parity << "\",\n";
    ss << "    \"hardware_flow_control\": " << (serial_config_.hardware_flow_control ? "true" : "false") << ",\n";
    ss << "    \"software_flow_control\": " << (serial_config_.software_flow_control ? "true" : "false") << ",\n";
    ss << "    \"read_timeout_ms\": " << serial_config_.read_timeout_ms << ",\n";
    ss << "    \"write_timeout_ms\": " << serial_config_.write_timeout_ms << ",\n";
    ss << "    \"serial_fd\": " << serial_fd_ << ",\n";
    ss << "    \"connected\": " << (IsSerialPortOpen() ? "true" : "false") << "\n";
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

std::string SerialBasedWorker::GetSerialStats() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"serial_port\": {\n";
    ss << "    \"port_name\": \"" << serial_config_.port_name << "\",\n";
    ss << "    \"baud_rate\": " << serial_config_.baud_rate << ",\n";
    ss << "    \"serial_fd\": " << serial_fd_ << ",\n";
    ss << "    \"connected\": " << (IsSerialPortOpen() ? "true" : "false") << "\n";
    ss << "  },\n";
    ss << "  \"base_worker_stats\": " << GetReconnectionStats() << "\n";
    ss << "}";
    return ss.str();
}

// =============================================================================
// 시리얼 포트 관리 (실제 구현)
// =============================================================================

bool SerialBasedWorker::ValidateSerialConfig() const {
    if (!ValidateSerialPort(serial_config_.port_name)) {
        LogMessage(PulseOne::LogLevel::ERROR, "Invalid serial port name: " + serial_config_.port_name);
        return false;
    }
    
    if (!ValidateBaudRate(serial_config_.baud_rate)) {
        LogMessage(PulseOne::LogLevel::ERROR, "Invalid baud rate: " + std::to_string(serial_config_.baud_rate));
        return false;
    }
    
    if (serial_config_.data_bits != 7 && serial_config_.data_bits != 8) {
        LogMessage(PulseOne::LogLevel::ERROR, "Invalid data bits: " + std::to_string(serial_config_.data_bits));
        return false;
    }
    
    if (serial_config_.stop_bits != 1 && serial_config_.stop_bits != 2) {
        LogMessage(PulseOne::LogLevel::ERROR, "Invalid stop bits: " + std::to_string(serial_config_.stop_bits));
        return false;
    }
    
    if (serial_config_.parity != 'N' && serial_config_.parity != 'E' && serial_config_.parity != 'O') {
        LogMessage(PulseOne::LogLevel::ERROR, "Invalid parity: " + std::string(1, serial_config_.parity));
        return false;
    }
    
    return true;
}

bool SerialBasedWorker::OpenSerialPort() {
    // 기존 포트가 열려있으면 닫기
    CloseSerialPort();
    
    // 시리얼 포트 열기
    serial_fd_ = open(serial_config_.port_name.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_fd_ < 0) {
        LogMessage(PulseOne::LogLevel::ERROR, 
                   "Failed to open serial port " + serial_config_.port_name + 
                   ": " + std::string(strerror(errno)));
        return false;
    }
    
    // 블로킹 모드로 설정
    int flags = fcntl(serial_fd_, F_GETFL, 0);
    fcntl(serial_fd_, F_SETFL, flags & ~O_NONBLOCK);
    
    // 기존 터미널 설정 백업
    if (tcgetattr(serial_fd_, &original_termios_) != 0) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to get terminal attributes: " + std::string(strerror(errno)));
        CloseSerialPort();
        return false;
    }
    
    // 새로운 터미널 설정
    struct termios tty;
    if (!ConfigureTermios(tty)) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to configure terminal attributes");
        CloseSerialPort();
        return false;
    }
    
    // 터미널 설정 적용
    if (tcsetattr(serial_fd_, TCSANOW, &tty) != 0) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to set terminal attributes: " + std::string(strerror(errno)));
        CloseSerialPort();
        return false;
    }
    
    // 버퍼 플러시
    FlushSerialPort(true, true);
    
    LogMessage(PulseOne::LogLevel::INFO, "Serial port opened successfully: " + serial_config_.port_name);
    return true;
}

void SerialBasedWorker::CloseSerialPort() {
    if (serial_fd_ >= 0) {
        // 원본 터미널 설정 복구
        tcsetattr(serial_fd_, TCSANOW, &original_termios_);
        
        // 포트 닫기
        close(serial_fd_);
        serial_fd_ = -1;
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Serial port closed");
    }
}

bool SerialBasedWorker::IsSerialPortOpen() const {
    if (serial_fd_ < 0) {
        return false;
    }
    
    // 포트 상태 확인
    int status;
    if (ioctl(serial_fd_, TIOCMGET, &status) < 0) {
        return false;
    }
    
    return true;
}

ssize_t SerialBasedWorker::WriteSerialData(const void* data, size_t length) {
    if (serial_fd_ < 0) {
        LogMessage(PulseOne::LogLevel::ERROR, "Cannot write data: serial port not open");
        return -1;
    }
    
    ssize_t written = write(serial_fd_, data, length);
    if (written < 0) {
        LogMessage(PulseOne::LogLevel::ERROR, "Serial write failed: " + std::string(strerror(errno)));
        return -1;
    }
    
    // 전송 완료 대기
    tcdrain(serial_fd_);
    
    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Sent " + std::to_string(written) + " bytes via serial");
    return written;
}

ssize_t SerialBasedWorker::ReadSerialData(void* buffer, size_t buffer_size) {
    if (serial_fd_ < 0) {
        LogMessage(PulseOne::LogLevel::ERROR, "Cannot read data: serial port not open");
        return -1;
    }
    
    // 타임아웃 설정
    fd_set read_fds;
    struct timeval timeout;
    
    FD_ZERO(&read_fds);
    FD_SET(serial_fd_, &read_fds);
    
    timeout.tv_sec = serial_config_.read_timeout_ms / 1000;
    timeout.tv_usec = (serial_config_.read_timeout_ms % 1000) * 1000;
    
    int select_result = select(serial_fd_ + 1, &read_fds, NULL, NULL, &timeout);
    
    if (select_result < 0) {
        LogMessage(PulseOne::LogLevel::ERROR, "Serial select failed: " + std::string(strerror(errno)));
        return -1;
    }
    
    if (select_result == 0) {
        // 타임아웃
        return 0;
    }
    
    ssize_t received = read(serial_fd_, buffer, buffer_size);
    if (received < 0) {
        LogMessage(PulseOne::LogLevel::ERROR, "Serial read failed: " + std::string(strerror(errno)));
        return -1;
    }
    
    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Received " + std::to_string(received) + " bytes via serial");
    return received;
}

bool SerialBasedWorker::FlushSerialPort(bool flush_input, bool flush_output) {
    if (serial_fd_ < 0) {
        return false;
    }
    
    int queue_selector = 0;
    if (flush_input) queue_selector |= TCIFLUSH;
    if (flush_output) queue_selector |= TCOFLUSH;
    
    if (tcflush(serial_fd_, queue_selector) != 0) {
        LogMessage(PulseOne::LogLevel::WARN, "Failed to flush serial port: " + std::string(strerror(errno)));
        return false;
    }
    
    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Serial port flushed");
    return true;
}

int SerialBasedWorker::GetSerialFd() const {
    return serial_fd_;
}

// =============================================================================
// 기본 프로토콜 구현
// =============================================================================

bool SerialBasedWorker::SendProtocolKeepAlive() {
    // 기본 구현: 시리얼 포트 상태만 확인
    if (serial_fd_ < 0) {
        return false;
    }
    
    // DTR 신호 토글 (간단한 Keep-alive)
    int status;
    if (ioctl(serial_fd_, TIOCMGET, &status) < 0) {
        LogMessage(PulseOne::LogLevel::WARN, "Failed to get serial status for keep-alive");
        return false;
    }
    
    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Serial keep-alive sent");
    return true;
}

// =============================================================================
// 내부 유틸리티 메서드들
// =============================================================================

void SerialBasedWorker::ParseEndpoint() {
    if (!device_info_.endpoint.empty()) {
        // endpoint 형식: "/dev/ttyUSB0:9600:8:N:1" 또는 "/dev/ttyUSB0"
        std::string endpoint = device_info_.endpoint;
        size_t colon_pos = endpoint.find(':');
        
        if (colon_pos != std::string::npos) {
            // 상세 설정 파싱
            serial_config_.port_name = endpoint.substr(0, colon_pos);
            
            std::string remaining = endpoint.substr(colon_pos + 1);
            std::vector<std::string> parts;
            
            // ':' 기준으로 분할
            size_t start = 0;
            size_t end = 0;
            while ((end = remaining.find(':', start)) != std::string::npos) {
                parts.push_back(remaining.substr(start, end - start));
                start = end + 1;
            }
            parts.push_back(remaining.substr(start));
            
            // 파싱된 값들 적용
            if (parts.size() >= 1) serial_config_.baud_rate = std::stoi(parts[0]);
            if (parts.size() >= 2) serial_config_.data_bits = std::stoi(parts[1]);
            if (parts.size() >= 3) serial_config_.parity = parts[2][0];
            if (parts.size() >= 4) serial_config_.stop_bits = std::stoi(parts[3]);
            
            LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                      "Parsed serial endpoint: " + serial_config_.port_name + 
                      " " + std::to_string(serial_config_.baud_rate) + 
                      "-" + std::to_string(serial_config_.data_bits) + 
                      "-" + std::string(1, serial_config_.parity) + 
                      "-" + std::to_string(serial_config_.stop_bits));
        } else {
            // 기본 설정으로 포트만 설정
            serial_config_.port_name = endpoint;
            LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Using default serial settings for port: " + serial_config_.port_name);
        }
    }
}

speed_t SerialBasedWorker::BaudRateToSpeed(int baud_rate) const {
    switch (baud_rate) {
        case 1200:    return B1200;
        case 2400:    return B2400;
        case 4800:    return B4800;
        case 9600:    return B9600;
        case 19200:   return B19200;
        case 38400:   return B38400;
        case 57600:   return B57600;
        case 115200:  return B115200;
        case 230400:  return B230400;
        case 460800:  return B460800;
        case 921600:  return B921600;
        default:      return B9600;  // 기본값
    }
}

bool SerialBasedWorker::ConfigureTermios(struct termios& tty) const {
    memset(&tty, 0, sizeof(tty));
    
    // 입력/출력 보드레이트 설정
    speed_t speed = BaudRateToSpeed(serial_config_.baud_rate);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);
    
    // 제어 플래그 설정
    tty.c_cflag = CREAD | CLOCAL;  // 수신 활성화, 모뎀 제어 라인 무시
    
    // 데이터 비트
    tty.c_cflag &= ~CSIZE;
    if (serial_config_.data_bits == 7) {
        tty.c_cflag |= CS7;
    } else {
        tty.c_cflag |= CS8;
    }
    
    // 스톱 비트
    if (serial_config_.stop_bits == 2) {
        tty.c_cflag |= CSTOPB;
    } else {
        tty.c_cflag &= ~CSTOPB;
    }
    
    // 패리티
    switch (serial_config_.parity) {
        case 'E':  // Even
            tty.c_cflag |= PARENB;
            tty.c_cflag &= ~PARODD;
            break;
        case 'O':  // Odd
            tty.c_cflag |= PARENB;
            tty.c_cflag |= PARODD;
            break;
        case 'N':  // None
        default:
            tty.c_cflag &= ~PARENB;
            break;
    }
    
    // 흐름 제어
    if (serial_config_.hardware_flow_control) {
        tty.c_cflag |= CRTSCTS;
    } else {
        tty.c_cflag &= ~CRTSCTS;
    }
    
    // 입력 플래그 설정
    tty.c_iflag = 0;
    if (serial_config_.software_flow_control) {
        tty.c_iflag |= IXON | IXOFF | IXANY;
    }
    
    // 출력 플래그 설정 (Raw 모드)
    tty.c_oflag = 0;
    
    // 로컬 플래그 설정 (Raw 모드)
    tty.c_lflag = 0;
    
    // 타임아웃 설정
    tty.c_cc[VTIME] = serial_config_.read_timeout_ms / 100;  // 0.1초 단위
    tty.c_cc[VMIN] = 0;   // 논블로킹 읽기
    
    return true;
}

} // namespace Workers
} // namespace PulseOne