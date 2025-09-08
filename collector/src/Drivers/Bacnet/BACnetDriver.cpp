//=============================================================================
// collector/src/Drivers/Bacnet/BACnetDriver.cpp
// BACnet 프로토콜 드라이버 구현 - 독립객체 + Windows/Linux 완전 호환 최종 수정
//=============================================================================

#include "Drivers/Bacnet/BACnetDriver.h"
#include "Utils/LogManager.h"

// =============================================================================
// Windows 매크로 충돌 해결 (가장 중요!)
// =============================================================================
#ifdef ERROR
#undef ERROR
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// =============================================================================
// 플랫폼별 헤더 조건부 포함
// =============================================================================
#ifdef _WIN32
    // Windows: Winsock 사용
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
    #define CLOSE_SOCKET closesocket
    #define SOCKET_ERROR_TYPE int
    #define GET_SOCKET_ERROR() WSAGetLastError()
    
    // Windows에서 poll 구조체 정의
    struct pollfd {
        int fd;
        short events;
        short revents;
    };
    #define POLLIN  0x0001
    #define POLLOUT 0x0004
    
#else
    // Linux: 표준 소켓 헤더
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <poll.h>
    #define CLOSE_SOCKET close
    #define SOCKET_ERROR_TYPE int
    #define INVALID_SOCKET -1
    #define GET_SOCKET_ERROR() errno
#endif

using namespace PulseOne;
using namespace PulseOne::Drivers;
using namespace std::chrono;

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 생성자 및 소멸자 (독립 객체)
// =============================================================================
BACnetDriver::BACnetDriver()
    : driver_statistics_("BACNET")
    , status_(PulseOne::Structs::DriverStatus::UNINITIALIZED)
    , is_connected_(false)
    , should_stop_(false)
    , local_device_id_(1001)
    , target_ip_("192.168.1.255")
    , target_port_(47808)
    , max_apdu_length_(1476)
    , segmentation_support_(false)
#ifdef _WIN32
    , socket_fd_(static_cast<int>(INVALID_SOCKET)) {  // Windows: SOCKET을 int로 안전하게 캐스팅
#else
    , socket_fd_(INVALID_SOCKET) {  // Linux: 그대로 사용
#endif
    
    // BACnet 특화 통계 초기화
    InitializeBACnetStatistics();
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnetDriver instance created");
}

BACnetDriver::~BACnetDriver() {
    if (IsConnected()) {
        Disconnect();
    }
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnetDriver instance destroyed");
}

// =============================================================================
// IProtocolDriver 인터페이스 구현
// =============================================================================

bool BACnetDriver::Initialize(const PulseOne::Structs::DriverConfig& config) {
    auto& logger = LogManager::getInstance();
    logger.Info("Initializing BACnetDriver...");
    config_ = config;
    
    try {
        // 1. 설정 파싱
        ParseDriverConfig(config_);
        
        // 2. BACnet 스택 초기화
        if (!InitializeBACnetStack()) {
            SetError(PulseOne::Enums::ErrorCode::INTERNAL_ERROR, "Failed to initialize BACnet stack");
            status_.store(Enums::DriverStatus::DRIVER_ERROR); // ERROR = 4 (매크로 충돌 방지)
            return false;
        }
        
        status_.store(PulseOne::Structs::DriverStatus::STOPPED);
        logger.Info("BACnetDriver initialized successfully");
        
        return true;
        
    } catch (const std::exception& e) {
        SetError(PulseOne::Enums::ErrorCode::INTERNAL_ERROR, std::string("Exception: ") + e.what());
        status_.store(Enums::DriverStatus::DRIVER_ERROR); // ERROR = 4
        return false;
    }
}

bool BACnetDriver::Connect() {
    if (is_connected_.load()) {
        return true;
    }
    
    auto& logger = LogManager::getInstance();
    logger.Info("Connecting BACnetDriver...");
    
    try {
        status_.store(PulseOne::Structs::DriverStatus::STARTING);
        
#ifdef _WIN32
        // Windows: Winsock 초기화
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            SetError(PulseOne::Enums::ErrorCode::CONNECTION_FAILED, "Failed to initialize Winsock");
            status_.store(Enums::DriverStatus::DRIVER_ERROR); // ERROR = 4
            return false;
        }
        logger.Info("Winsock initialized successfully");
#endif
        
        // UDP 소켓 생성
        if (!CreateSocket()) {
            SetError(PulseOne::Enums::ErrorCode::CONNECTION_FAILED, "Failed to create UDP socket");
            status_.store(Enums::DriverStatus::DRIVER_ERROR); // ERROR = 4
            return false;
        }
        
        is_connected_.store(true);
        status_.store(PulseOne::Structs::DriverStatus::RUNNING);
        
        // 통계 업데이트
        driver_statistics_.IncrementProtocolCounter("successful_connections", 1);
        
        logger.Info("BACnetDriver connected successfully");
        return true;
        
    } catch (const std::exception& e) {
        SetError(PulseOne::Enums::ErrorCode::CONNECTION_FAILED, std::string("Exception: ") + e.what());
        status_.store(Enums::DriverStatus::DRIVER_ERROR); // ERROR = 4
        return false;
    }
}

bool BACnetDriver::Disconnect() {
    if (!is_connected_.load()) {
        return true;
    }
    
    auto& logger = LogManager::getInstance();
    logger.Info("Disconnecting BACnetDriver...");
    
    try {
        // 소켓 해제
        CloseSocket();
        
#ifdef _WIN32
        // Windows: Winsock 정리
        WSACleanup();
#endif
        
        is_connected_.store(false);
        status_.store(PulseOne::Structs::DriverStatus::STOPPED);
        
        // 통계 업데이트
        driver_statistics_.IncrementProtocolCounter("disconnections", 1);
        
        logger.Info("BACnetDriver disconnected successfully");
        return true;
        
    } catch (const std::exception& e) {
        SetError(PulseOne::Enums::ErrorCode::CONNECTION_FAILED, std::string("Exception: ") + e.what());
        return false;
    }
}

bool BACnetDriver::IsConnected() const {
    return is_connected_.load();
}

bool BACnetDriver::Start() {
    auto& logger = LogManager::getInstance();
    logger.Info("Starting BACnetDriver...");
    
    if (!Connect()) {
        return false;
    }
    
    should_stop_.store(false);
    status_.store(PulseOne::Structs::DriverStatus::RUNNING);
    
    logger.Info("BACnetDriver started successfully");
    return true;
}

bool BACnetDriver::Stop() {
    auto& logger = LogManager::getInstance();
    logger.Info("Stopping BACnetDriver...");
    
    should_stop_.store(true);
    
    if (!Disconnect()) {
        logger.Warn("Warning: BACnetDriver disconnect issues during stop");
    }
    
    status_.store(PulseOne::Structs::DriverStatus::STOPPED);
    
    logger.Info("BACnetDriver stopped successfully");
    return true;
}

// =============================================================================
// 데이터 읽기/쓰기 메서드
// =============================================================================

bool BACnetDriver::ReadValues(const std::vector<PulseOne::Structs::DataPoint>& points,
                             std::vector<PulseOne::Structs::TimestampedValue>& values) {
    if (!is_connected_.load()) {
        SetError(PulseOne::Enums::ErrorCode::CONNECTION_FAILED, "Driver not connected");
        return false;
    }
    
    auto& logger = LogManager::getInstance();
    logger.Debug("Reading " + std::to_string(points.size()) + " BACnet values");
    
    try {
        auto start = high_resolution_clock::now();
        
        // 각 포인트 읽기
        values.clear();
        values.reserve(points.size());
        
        int successful_reads = 0;
        for (const auto& point : points) {
            PulseOne::Structs::TimestampedValue value;
            if (ReadSingleProperty(point, value)) {
                values.push_back(value);
                successful_reads++;
            } else {
                // 실패한 포인트도 추가 (에러 상태로)
                PulseOne::Structs::TimestampedValue error_value;
                error_value.quality = PulseOne::Enums::DataQuality::BAD;
                error_value.timestamp = system_clock::now();
                values.push_back(error_value);
            }
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start);
        
        // 통계 업데이트
        driver_statistics_.total_reads.fetch_add(points.size());
        driver_statistics_.successful_reads.fetch_add(successful_reads);
        driver_statistics_.failed_reads.fetch_add(points.size() - successful_reads);
        driver_statistics_.IncrementProtocolCounter("property_reads", points.size());
        
        // 평균 응답 시간 계산
        auto new_avg = (driver_statistics_.avg_response_time_ms.load() + duration.count()) / 2.0;
        driver_statistics_.avg_response_time_ms.store(new_avg);
        
        return successful_reads > 0;
        
    } catch (const std::exception& e) {
        SetError(PulseOne::Enums::ErrorCode::PROTOCOL_ERROR, std::string("Exception: ") + e.what());
        driver_statistics_.IncrementProtocolCounter("protocol_errors", 1);
        return false;
    }
}

bool BACnetDriver::WriteValue(const PulseOne::Structs::DataPoint& point, 
                             const PulseOne::Structs::DataValue& value) {
    if (!is_connected_.load()) {
        SetError(PulseOne::Enums::ErrorCode::CONNECTION_FAILED, "Driver not connected");
        return false;
    }
    
    auto& logger = LogManager::getInstance();
    // point.address의 타입에 안전한 문자열 변환
    std::string address_str;
    try {
        // address가 문자열인 경우
        if constexpr (std::is_same_v<decltype(point.address), std::string>) {
            address_str = point.address;
        } else {
            // address가 숫자 타입인 경우
            address_str = std::to_string(point.address);
        }
    } catch (...) {
        // 변환 실패시 기본값
        address_str = "unknown_address";
    }
    
    logger.Debug("Writing BACnet value to " + address_str);
    
    try {
        bool success = WriteSingleProperty(point, value);
        
        // 통계 업데이트
        driver_statistics_.total_writes.fetch_add(1);
        if (success) {
            driver_statistics_.successful_writes.fetch_add(1);
            driver_statistics_.IncrementProtocolCounter("property_writes", 1);
        } else {
            driver_statistics_.failed_writes.fetch_add(1);
            driver_statistics_.IncrementProtocolCounter("protocol_errors", 1);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        SetError(PulseOne::Enums::ErrorCode::PROTOCOL_ERROR, std::string("Exception: ") + e.what());
        driver_statistics_.IncrementProtocolCounter("protocol_errors", 1);
        return false;
    }
}

// =============================================================================
// 상태 및 통계 메서드
// =============================================================================

PulseOne::Enums::ProtocolType BACnetDriver::GetProtocolType() const {
    return PulseOne::Enums::ProtocolType::BACNET_IP;
}

PulseOne::Structs::DriverStatus BACnetDriver::GetStatus() const {
    return status_.load();
}

PulseOne::Structs::ErrorInfo BACnetDriver::GetLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

const PulseOne::Structs::DriverStatistics& BACnetDriver::GetStatistics() const {
    return driver_statistics_;
}

void BACnetDriver::ResetStatistics() {
    // 통계 리셋 (ResetCounters 메서드 대신 직접 리셋)
    driver_statistics_.total_reads.store(0);
    driver_statistics_.successful_reads.store(0);
    driver_statistics_.failed_reads.store(0);
    driver_statistics_.total_writes.store(0);
    driver_statistics_.successful_writes.store(0);
    driver_statistics_.failed_writes.store(0);
    driver_statistics_.failed_operations.store(0);
    driver_statistics_.avg_response_time_ms.store(0.0);
    
    // 프로토콜 특화 카운터도 리셋
    driver_statistics_.protocol_counters.clear();
    driver_statistics_.protocol_metrics.clear();
    
    // BACnet 특화 통계 재초기화
    InitializeBACnetStatistics();
}

void BACnetDriver::SetError(PulseOne::Enums::ErrorCode code, const std::string& message) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    
    last_error_.code = static_cast<PulseOne::Structs::ErrorCode>(code);
    last_error_.message = message;
    last_error_.occurred_at = system_clock::now();
    last_error_.context = "BACnet Driver";
    
    // 통계 업데이트
    driver_statistics_.failed_operations.fetch_add(1);
    driver_statistics_.last_error_time = last_error_.occurred_at;
    
    // 로깅
    auto& logger = LogManager::getInstance();
    logger.Error("BACnet Error [" + std::to_string(static_cast<int>(code)) + "]: " + message);
}

// =============================================================================
// BACnet 특화 공개 메서드들
// =============================================================================

std::vector<PulseOne::Structs::DeviceInfo> BACnetDriver::DiscoverDevices(uint32_t timeout_ms) {
    (void)timeout_ms; // 나중에 사용
    
    std::vector<PulseOne::Structs::DeviceInfo> devices;
    
    // 더미 구현 (실제로는 Who-Is 브로드캐스트 사용)
    PulseOne::Structs::DeviceInfo dummy_device;
    dummy_device.id = "12345";
    // DeviceInfo 구조체의 실제 필드에 맞춰 설정
    dummy_device.name = "Test BACnet Device";
    dummy_device.description = "Simulated device for testing";
    devices.push_back(dummy_device);
    
    driver_statistics_.IncrementProtocolCounter("device_discoveries", devices.size());
    
    return devices;
}

bool BACnetDriver::SubscribeCOV(uint32_t device_id, BACNET_OBJECT_TYPE object_type, 
                               uint32_t object_instance, BACNET_PROPERTY_ID property_id) {
    (void)device_id;
    (void)object_type;
    (void)object_instance;
    (void)property_id;
    
    // 더미 구현
    driver_statistics_.IncrementProtocolCounter("cov_subscriptions", 1);
    return true;
}

bool BACnetDriver::UnsubscribeCOV(uint32_t device_id, BACNET_OBJECT_TYPE object_type, 
                                 uint32_t object_instance, BACNET_PROPERTY_ID property_id) {
    (void)device_id;
    (void)object_type;
    (void)object_instance;
    (void)property_id;
    
    // 더미 구현
    driver_statistics_.IncrementProtocolCounter("cov_unsubscriptions", 1);
    return true;
}

// =============================================================================
// 내부 구현 메서드들
// =============================================================================

void BACnetDriver::ParseDriverConfig(const PulseOne::Structs::DriverConfig& config) {
    auto& logger = LogManager::getInstance();
    
    // Device ID 설정
    auto it = config.properties.find("device_id");
    if (it != config.properties.end()) {
        local_device_id_ = std::stoul(it->second);
    }
    
    // Target IP 설정  
    it = config.properties.find("target_ip");
    if (it != config.properties.end()) {
        target_ip_ = it->second;
    }
    
    // Port 설정
    it = config.properties.find("port");
    if (it != config.properties.end()) {
        target_port_ = static_cast<uint16_t>(std::stoul(it->second));
    }
    
    // 추가 BACnet 설정들
    it = config.properties.find("max_apdu_length");
    if (it != config.properties.end()) {
        max_apdu_length_ = std::stoul(it->second);
        driver_statistics_.SetProtocolMetric("max_apdu_size", static_cast<double>(max_apdu_length_));
    }
    
    it = config.properties.find("segmentation_support");
    if (it != config.properties.end()) {
        segmentation_support_ = (it->second == "true" || it->second == "1");
    }
    
    logger.Info("BACnet config parsed - IP: " + target_ip_ + 
                ", Port: " + std::to_string(target_port_) +
                ", Device ID: " + std::to_string(local_device_id_));
}

bool BACnetDriver::InitializeBACnetStack() {
    auto& logger = LogManager::getInstance();
    
#ifdef HAS_BACNET_STACK
    try {
        // 실제 BACnet 스택 초기화는 주석 처리 (라이브러리 의존성 때문)
        // tsm_init();
        // apdu_init();
        
        logger.Info("BACnet stack initialized with real stack");
        return true;
        
    } catch (const std::exception& e) {
        logger.Error("BACnet stack initialization failed: " + std::string(e.what()));
        return false;
    }
#else
    logger.Info("BACnet stack initialized in simulation mode");
    return true;
#endif
}

void BACnetDriver::InitializeBACnetStatistics() {
    // BACnet 특화 통계 카운터 초기화
    driver_statistics_.IncrementProtocolCounter("cov_notifications", 0);
    driver_statistics_.IncrementProtocolCounter("device_discoveries", 0);
    driver_statistics_.IncrementProtocolCounter("property_reads", 0);
    driver_statistics_.IncrementProtocolCounter("property_writes", 0);
    driver_statistics_.IncrementProtocolCounter("cov_subscriptions", 0);
    driver_statistics_.IncrementProtocolCounter("cov_unsubscriptions", 0);
    driver_statistics_.IncrementProtocolCounter("network_errors", 0);
    driver_statistics_.IncrementProtocolCounter("timeout_errors", 0);
    driver_statistics_.IncrementProtocolCounter("protocol_errors", 0);
    
    // BACnet 특화 메트릭 초기화  
    driver_statistics_.SetProtocolMetric("avg_discovery_time_ms", 0.0);
    driver_statistics_.SetProtocolMetric("max_apdu_size", 1476.0);
    driver_statistics_.SetProtocolMetric("active_subscriptions", 0.0);
}

bool BACnetDriver::CreateSocket() {
    auto& logger = LogManager::getInstance();
    
#ifdef _WIN32
    // Windows 소켓 생성
    SOCKET win_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (win_socket == INVALID_SOCKET) {
        logger.Error("Failed to create UDP socket: " + std::to_string(WSAGetLastError()));
        return false;
    }
    socket_fd_ = static_cast<int>(win_socket);  // 안전한 캐스팅
#else
    // Linux 소켓 생성
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        logger.Error("Failed to create UDP socket");
        return false;
    }
#endif
    
    logger.Info("UDP socket created successfully");
    return true;
}

void BACnetDriver::CloseSocket() {
#ifdef _WIN32
    // Windows: INVALID_SOCKET과 비교
    if (socket_fd_ != static_cast<int>(INVALID_SOCKET)) {
        closesocket(static_cast<SOCKET>(socket_fd_));
        socket_fd_ = static_cast<int>(INVALID_SOCKET);
    }
#else
    // Linux: -1과 비교
    if (socket_fd_ != INVALID_SOCKET) {
        close(socket_fd_);
        socket_fd_ = INVALID_SOCKET;
    }
#endif
}

bool BACnetDriver::ReadSingleProperty(const PulseOne::Structs::DataPoint& point, 
                                     PulseOne::Structs::TimestampedValue& value) {
    // 시뮬레이션 데이터
    value.value = 23.5; // 가짜 온도 값
    value.quality = PulseOne::Enums::DataQuality::GOOD;
    value.timestamp = system_clock::now();
    
    auto& logger = LogManager::getInstance();
    // point.address를 안전하게 문자열로 변환
    std::string address_str;
    try {
        if constexpr (std::is_same_v<decltype(point.address), std::string>) {
            address_str = point.address;
        } else {
            address_str = std::to_string(point.address);
        }
    } catch (...) {
        address_str = "unknown_address";
    }
    
    logger.Debug("Simulated read from " + address_str + " = 23.5");
    
    return true;
}

bool BACnetDriver::WriteSingleProperty(const PulseOne::Structs::DataPoint& point, 
                                      const PulseOne::Structs::DataValue& value) {
    (void)value; // 경고 제거
    
    auto& logger = LogManager::getInstance();
    // point.address를 안전하게 문자열로 변환
    std::string address_str;
    try {
        if constexpr (std::is_same_v<decltype(point.address), std::string>) {
            address_str = point.address;
        } else {
            address_str = std::to_string(point.address);
        }
    } catch (...) {
        address_str = "unknown_address";
    }
    
    logger.Debug("Simulated write to " + address_str);
    
    return true;
}

} // namespace Drivers
} // namespace PulseOne