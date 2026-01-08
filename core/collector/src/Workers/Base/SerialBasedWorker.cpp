// =============================================================================
// collector/src/Workers/Base/SerialBasedWorker.cpp - Windows 크로스 컴파일 완전 대응
// =============================================================================

/**
 * @file SerialBasedWorker.cpp
 * @brief 크로스 플랫폼 시리얼 통신 기반 프로토콜 워커의 구현
 * @author PulseOne Development Team
 * @date 2025-09-08
 * @version 8.0.0 - Windows 크로스 컴파일 완전 대응
 */

#include "Workers/Base/SerialBasedWorker.h"
#include "Logging/LogManager.h"
#include "Common/Enums.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>

// =============================================================================
// 플랫폼별 시스템 헤더 (조건부 include)
// =============================================================================
#if PULSEONE_WINDOWS
    #include <windows.h>
    #include <winbase.h>
    #include <setupapi.h>
    #include <devguid.h>
    #include <regstr.h>
    #pragma comment(lib, "setupapi.lib")
#else
    #include <cstring>      // strerror, memset 함수용
    #include <cerrno>       // errno 변수용
    #include <fcntl.h>      // open, O_RDWR, O_NOCTTY
    #include <unistd.h>     // close, read, write
    #include <sys/ioctl.h>  // ioctl
    #include <sys/select.h> // select
#endif

using namespace std::chrono;
using LogLevel = PulseOne::Enums::LogLevel;

namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

SerialBasedWorker::SerialBasedWorker(const PulseOne::Structs::DeviceInfo& device_info)
    : BaseDeviceWorker(device_info)
    , serial_handle_(SERIAL_INVALID_HANDLE) {
    
    // endpoint에서 시리얼 설정 파싱
    ParseEndpoint();
    
    // BaseDeviceWorker 재연결 설정 활용
    ReconnectionSettings settings;
    settings.auto_reconnect_enabled = true;
    settings.retry_interval_ms = 3000;          // 시리얼은 빠른 재시도
    settings.max_retries_per_cycle = 20;        // 시리얼은 더 많은 재시도
    settings.keep_alive_enabled = true;
    settings.keep_alive_interval_seconds = 45;  // 시리얼 Keep-alive
    UpdateReconnectionSettings(settings);
    
    LogMessage(LogLevel::INFO, "SerialBasedWorker created for device: " + device_info.name);
}

SerialBasedWorker::~SerialBasedWorker() {
    CloseSerialPort();
    LogMessage(LogLevel::INFO, "SerialBasedWorker destroyed");
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현 (시리얼 특화)
// =============================================================================

bool SerialBasedWorker::EstablishConnection() {
    LogMessage(LogLevel::INFO, "Establishing serial connection to " + serial_config_.port_name);
    
    // 시리얼 설정 검증
    if (!ValidateSerialConfig()) {
        LogMessage(LogLevel::LOG_ERROR, "Invalid serial configuration");
        return false;
    }
    
    // 시리얼 포트 열기
    if (!OpenSerialPort()) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to open serial port");
        return false;
    }
    
    // 파생 클래스의 프로토콜별 연결 수립
    if (!EstablishProtocolConnection()) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to establish protocol connection");
        CloseSerialPort();
        return false;
    }
    
    LogMessage(LogLevel::INFO, "Serial connection established successfully");
    return true;
}

bool SerialBasedWorker::CloseConnection() {
    LogMessage(LogLevel::INFO, "Closing serial connection");
    
    // 프로토콜별 연결 해제 먼저
    CloseProtocolConnection();
    
    // 시리얼 포트 해제
    CloseSerialPort();
    
    LogMessage(LogLevel::INFO, "Serial connection closed");
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
        LogMessage(LogLevel::WARN, "Serial port not open for keep-alive");
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
    
    LogMessage(LogLevel::INFO, 
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
#if PULSEONE_WINDOWS
    ss << "    \"serial_handle\": " << reinterpret_cast<uintptr_t>(serial_handle_) << ",\n";
#else
    ss << "    \"serial_fd\": " << serial_handle_ << ",\n";
#endif
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
#if PULSEONE_WINDOWS
    ss << "    \"serial_handle\": " << reinterpret_cast<uintptr_t>(serial_handle_) << ",\n";
#else
    ss << "    \"serial_fd\": " << serial_handle_ << ",\n";
#endif
    ss << "    \"connected\": " << (IsSerialPortOpen() ? "true" : "false") << "\n";
    ss << "  },\n";
    ss << "  \"base_worker_stats\": " << GetReconnectionStats() << "\n";
    ss << "}";
    return ss.str();
}

// =============================================================================
// 크로스 플랫폼 시리얼 포트 관리 (실제 구현)
// =============================================================================

bool SerialBasedWorker::ValidateSerialConfig() const {
    if (!ValidateSerialPort(serial_config_.port_name)) {
        LogMessage(LogLevel::LOG_ERROR, "Invalid serial port name: " + serial_config_.port_name);
        return false;
    }
    
    if (!ValidateBaudRate(serial_config_.baud_rate)) {
        LogMessage(LogLevel::LOG_ERROR, "Invalid baud rate: " + std::to_string(serial_config_.baud_rate));
        return false;
    }
    
    if (serial_config_.data_bits != 7 && serial_config_.data_bits != 8) {
        LogMessage(LogLevel::LOG_ERROR, "Invalid data bits: " + std::to_string(serial_config_.data_bits));
        return false;
    }
    
    if (serial_config_.stop_bits != 1 && serial_config_.stop_bits != 2) {
        LogMessage(LogLevel::LOG_ERROR, "Invalid stop bits: " + std::to_string(serial_config_.stop_bits));
        return false;
    }
    
    if (serial_config_.parity != 'N' && serial_config_.parity != 'E' && serial_config_.parity != 'O') {
        LogMessage(LogLevel::LOG_ERROR, "Invalid parity: " + std::string(1, serial_config_.parity));
        return false;
    }
    
    return true;
}

bool SerialBasedWorker::OpenSerialPort() {
    // 기존 포트가 열려있으면 닫기
    CloseSerialPort();
    
#if PULSEONE_WINDOWS
    // =======================================================================
    // Windows 시리얼 포트 열기
    // =======================================================================
    
    // COM 포트명 변환 (COM10 이상은 특별한 형태 필요)
    std::string port_name = serial_config_.port_name;
    if (port_name.find("COM") == 0 && port_name.length() > 4) {
        port_name = "\\\\.\\" + port_name;
    }
    
    // 포트 열기
    serial_handle_ = CreateFileA(
        port_name.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,                    // exclusive access
        NULL,                 // default security attributes
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (serial_handle_ == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        LogMessage(LogLevel::LOG_ERROR, 
                   "Failed to open serial port " + serial_config_.port_name + 
                   " (Error: " + std::to_string(error) + ")");
        return false;
    }
    
    // DCB 설정
    if (!ConfigureWindowsDCB(win_serial_config_.dcb)) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to configure DCB");
        CloseSerialPort();
        return false;
    }
    
    if (!SetCommState(serial_handle_, &win_serial_config_.dcb)) {
        DWORD error = GetLastError();
        LogMessage(LogLevel::LOG_ERROR, 
                   "Failed to set comm state (Error: " + std::to_string(error) + ")");
        CloseSerialPort();
        return false;
    }
    
    // 타임아웃 설정
    if (!ConfigureWindowsTimeouts(win_serial_config_.timeouts)) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to configure timeouts");
        CloseSerialPort();
        return false;
    }
    
    if (!SetCommTimeouts(serial_handle_, &win_serial_config_.timeouts)) {
        DWORD error = GetLastError();
        LogMessage(LogLevel::LOG_ERROR, 
                   "Failed to set comm timeouts (Error: " + std::to_string(error) + ")");
        CloseSerialPort();
        return false;
    }
    
    // 버퍼 플러시
    FlushSerialPort(true, true);
    
#else
    // =======================================================================
    // Linux 시리얼 포트 열기
    // =======================================================================
    
    // 시리얼 포트 열기
    serial_handle_ = open(serial_config_.port_name.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_handle_ < 0) {
        LogMessage(LogLevel::LOG_ERROR, 
                   "Failed to open serial port " + serial_config_.port_name + 
                   ": " + std::string(strerror(errno)));
        return false;
    }
    
    // 블로킹 모드로 설정
    int flags = fcntl(serial_handle_, F_GETFL, 0);
    fcntl(serial_handle_, F_SETFL, flags & ~O_NONBLOCK);
    
    // 기존 터미널 설정 백업
    if (tcgetattr(serial_handle_, &original_termios_) != 0) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to get terminal attributes: " + std::string(strerror(errno)));
        CloseSerialPort();
        return false;
    }
    
    // 새로운 터미널 설정
    struct termios tty;
    if (!ConfigureTermios(tty)) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to configure terminal attributes");
        CloseSerialPort();
        return false;
    }
    
    // 터미널 설정 적용
    if (tcsetattr(serial_handle_, TCSANOW, &tty) != 0) {
        LogMessage(LogLevel::LOG_ERROR, "Failed to set terminal attributes: " + std::string(strerror(errno)));
        CloseSerialPort();
        return false;
    }
    
    // 버퍼 플러시
    FlushSerialPort(true, true);
    
#endif
    
    LogMessage(LogLevel::INFO, "Serial port opened successfully: " + serial_config_.port_name);
    return true;
}

void SerialBasedWorker::CloseSerialPort() {
#if PULSEONE_WINDOWS
    if (serial_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(serial_handle_);
        serial_handle_ = INVALID_HANDLE_VALUE;
        LogMessage(LogLevel::DEBUG_LEVEL, "Serial port closed");
    }
#else
    if (serial_handle_ >= 0) {
        // 원본 터미널 설정 복구
        tcsetattr(serial_handle_, TCSANOW, &original_termios_);
        
        // 포트 닫기
        close(serial_handle_);
        serial_handle_ = -1;
        LogMessage(LogLevel::DEBUG_LEVEL, "Serial port closed");
    }
#endif
}

bool SerialBasedWorker::IsSerialPortOpen() const {
#if PULSEONE_WINDOWS
    if (serial_handle_ == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    // Windows에서 포트 상태 확인
    DWORD errors;
    COMSTAT status;
    if (!ClearCommError(serial_handle_, &errors, &status)) {
        return false;
    }
    
    return true;
#else
    if (serial_handle_ < 0) {
        return false;
    }
    
    // 포트 상태 확인
    int status;
    if (ioctl(serial_handle_, TIOCMGET, &status) < 0) {
        return false;
    }
    
    return true;
#endif
}

ssize_t SerialBasedWorker::WriteSerialData(const void* data, size_t length) {
    if (serial_handle_ == SERIAL_INVALID_HANDLE) {
        LogMessage(LogLevel::LOG_ERROR, "Cannot write data: serial port not open");
        return -1;
    }
    
#if PULSEONE_WINDOWS
    DWORD bytes_written = 0;
    if (!WriteFile(serial_handle_, data, static_cast<DWORD>(length), &bytes_written, NULL)) {
        DWORD error = GetLastError();
        LogMessage(LogLevel::LOG_ERROR, "Serial write failed (Error: " + std::to_string(error) + ")");
        return -1;
    }
    
    // 전송 완료 대기
    FlushFileBuffers(serial_handle_);
    
    LogMessage(LogLevel::DEBUG_LEVEL, "Sent " + std::to_string(bytes_written) + " bytes via serial");
    return static_cast<ssize_t>(bytes_written);
#else
    ssize_t written = write(serial_handle_, data, length);
    if (written < 0) {
        LogMessage(LogLevel::LOG_ERROR, "Serial write failed: " + std::string(strerror(errno)));
        return -1;
    }
    
    // 전송 완료 대기
    tcdrain(serial_handle_);
    
    LogMessage(LogLevel::DEBUG_LEVEL, "Sent " + std::to_string(written) + " bytes via serial");
    return written;
#endif
}

ssize_t SerialBasedWorker::ReadSerialData(void* buffer, size_t buffer_size) {
    if (serial_handle_ == SERIAL_INVALID_HANDLE) {
        LogMessage(LogLevel::LOG_ERROR, "Cannot read data: serial port not open");
        return -1;
    }
    
#if PULSEONE_WINDOWS
    DWORD bytes_read = 0;
    if (!ReadFile(serial_handle_, buffer, static_cast<DWORD>(buffer_size), &bytes_read, NULL)) {
        DWORD error = GetLastError();
        LogMessage(LogLevel::LOG_ERROR, "Serial read failed (Error: " + std::to_string(error) + ")");
        return -1;
    }
    
    LogMessage(LogLevel::DEBUG_LEVEL, "Received " + std::to_string(bytes_read) + " bytes via serial");
    return static_cast<ssize_t>(bytes_read);
#else
    // 타임아웃 설정
    fd_set read_fds;
    struct timeval timeout;
    
    FD_ZERO(&read_fds);
    FD_SET(serial_handle_, &read_fds);
    
    timeout.tv_sec = serial_config_.read_timeout_ms / 1000;
    timeout.tv_usec = (serial_config_.read_timeout_ms % 1000) * 1000;
    
    int select_result = select(serial_handle_ + 1, &read_fds, NULL, NULL, &timeout);
    
    if (select_result < 0) {
        LogMessage(LogLevel::LOG_ERROR, "Serial select failed: " + std::string(strerror(errno)));
        return -1;
    }
    
    if (select_result == 0) {
        // 타임아웃
        return 0;
    }
    
    ssize_t received = read(serial_handle_, buffer, buffer_size);
    if (received < 0) {
        LogMessage(LogLevel::LOG_ERROR, "Serial read failed: " + std::string(strerror(errno)));
        return -1;
    }
    
    LogMessage(LogLevel::DEBUG_LEVEL, "Received " + std::to_string(received) + " bytes via serial");
    return received;
#endif
}

bool SerialBasedWorker::FlushSerialPort(bool flush_input, bool flush_output) {
    if (serial_handle_ == SERIAL_INVALID_HANDLE) {
        return false;
    }
    
#if PULSEONE_WINDOWS
    DWORD flags = 0;
    if (flush_input) flags |= PURGE_RXCLEAR;
    if (flush_output) flags |= PURGE_TXCLEAR;
    
    if (!PurgeComm(serial_handle_, flags)) {
        DWORD error = GetLastError();
        LogMessage(LogLevel::WARN, "Failed to flush serial port (Error: " + std::to_string(error) + ")");
        return false;
    }
#else
    int queue_selector = 0;
    if (flush_input) queue_selector |= TCIFLUSH;
    if (flush_output) queue_selector |= TCOFLUSH;
    
    if (tcflush(serial_handle_, queue_selector) != 0) {
        LogMessage(LogLevel::WARN, "Failed to flush serial port: " + std::string(strerror(errno)));
        return false;
    }
#endif
    
    LogMessage(LogLevel::DEBUG_LEVEL, "Serial port flushed");
    return true;
}

SerialHandle SerialBasedWorker::GetSerialHandle() const {
    return serial_handle_;
}

// =============================================================================
// 기본 프로토콜 구현
// =============================================================================

bool SerialBasedWorker::SendProtocolKeepAlive() {
    // 기본 구현: 시리얼 포트 상태만 확인
    if (serial_handle_ == SERIAL_INVALID_HANDLE) {
        return false;
    }
    
#if PULSEONE_WINDOWS
    // Windows에서 포트 상태 확인
    DWORD errors;
    COMSTAT status;
    if (!ClearCommError(serial_handle_, &errors, &status)) {
        LogMessage(LogLevel::WARN, "Failed to get serial status for keep-alive");
        return false;
    }
#else
    // DTR 신호 토글 (간단한 Keep-alive)
    int status;
    if (ioctl(serial_handle_, TIOCMGET, &status) < 0) {
        LogMessage(LogLevel::WARN, "Failed to get serial status for keep-alive");
        return false;
    }
#endif
    
    LogMessage(LogLevel::DEBUG_LEVEL, "Serial keep-alive sent");
    return true;
}

// =============================================================================
// 내부 유틸리티 메서드들
// =============================================================================

void SerialBasedWorker::ParseEndpoint() {
    if (!device_info_.endpoint.empty()) {
        // endpoint 형식: "/dev/ttyUSB0:9600:8:N:1" 또는 "COM1:9600:8:N:1"
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
            
            LogMessage(LogLevel::DEBUG_LEVEL, 
                      "Parsed serial endpoint: " + serial_config_.port_name + 
                      " " + std::to_string(serial_config_.baud_rate) + 
                      "-" + std::to_string(serial_config_.data_bits) + 
                      "-" + std::string(1, serial_config_.parity) + 
                      "-" + std::to_string(serial_config_.stop_bits));
        } else {
            // 기본 설정으로 포트만 설정
            serial_config_.port_name = endpoint;
            LogMessage(LogLevel::DEBUG_LEVEL, "Using default serial settings for port: " + serial_config_.port_name);
        }
    }
}

#if PULSEONE_WINDOWS
// =============================================================================
// Windows 전용 구현
// =============================================================================

DWORD SerialBasedWorker::BaudRateToWinBaud(int baud_rate) const {
    switch (baud_rate) {
        case 1200:    return CBR_1200;
        case 2400:    return CBR_2400;
        case 4800:    return CBR_4800;
        case 9600:    return CBR_9600;
        case 19200:   return CBR_19200;
        case 38400:   return CBR_38400;
        case 57600:   return CBR_57600;
        case 115200:  return CBR_115200;
        default:      return CBR_9600;  // 기본값
    }
}

bool SerialBasedWorker::ConfigureWindowsDCB(DCB& dcb) const {
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    
    // 보드레이트 설정
    dcb.BaudRate = BaudRateToWinBaud(serial_config_.baud_rate);
    
    // 데이터 비트
    dcb.ByteSize = static_cast<BYTE>(serial_config_.data_bits);
    
    // 스톱 비트
    if (serial_config_.stop_bits == 2) {
        dcb.StopBits = TWOSTOPBITS;
    } else {
        dcb.StopBits = ONESTOPBIT;
    }
    
    // 패리티
    switch (serial_config_.parity) {
        case 'E':  // Even
            dcb.Parity = EVENPARITY;
            dcb.fParity = TRUE;
            break;
        case 'O':  // Odd
            dcb.Parity = ODDPARITY;
            dcb.fParity = TRUE;
            break;
        case 'N':  // None
        default:
            dcb.Parity = NOPARITY;
            dcb.fParity = FALSE;
            break;
    }
    
    // 흐름 제어
    if (serial_config_.hardware_flow_control) {
        dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
        dcb.fOutxCtsFlow = TRUE;
    } else {
        dcb.fRtsControl = RTS_CONTROL_DISABLE;
        dcb.fOutxCtsFlow = FALSE;
    }
    
    if (serial_config_.software_flow_control) {
        dcb.fOutX = TRUE;
        dcb.fInX = TRUE;
        dcb.XonChar = 0x11;  // XON
        dcb.XoffChar = 0x13; // XOFF
    } else {
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;
    }
    
    // 기타 설정
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fAbortOnError = FALSE;
    
    return true;
}

bool SerialBasedWorker::ConfigureWindowsTimeouts(COMMTIMEOUTS& timeouts) const {
    memset(&timeouts, 0, sizeof(timeouts));
    
    // 읽기 타임아웃 설정
    timeouts.ReadIntervalTimeout = 50;  // 문자 간 타임아웃
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.ReadTotalTimeoutConstant = serial_config_.read_timeout_ms;
    
    // 쓰기 타임아웃 설정
    timeouts.WriteTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = serial_config_.write_timeout_ms;
    
    return true;
}

#else
// =============================================================================
// Linux 전용 구현
// =============================================================================

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
#ifdef B460800
        case 460800:  return B460800;
#endif
#ifdef B921600
        case 921600:  return B921600;
#endif
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

#endif

} // namespace Workers
} // namespace PulseOne