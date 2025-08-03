/**
 * @file BACnetDriver.cpp
 * @brief BACnet 프로토콜 드라이버 구현 - 🔥 순환 의존성 해결
 * @author PulseOne Development Team
 * @date 2025-08-03
 * @version 1.0.0
 * 
 * 🔥 주요 수정사항:
 * 1. BACnetWorker.h include 제거 (순환 의존성 방지)
 * 2. 표준 DriverStatistics 사용
 * 3. IProtocolDriver 인터페이스 완전 준수
 * 4. BACnet 스택 연동 준비
 */

#include "Drivers/Bacnet/BACnetDriver.h"
#include "Utils/LogManager.h"
#include "Common/Utils.h"
#include <chrono>
#include <algorithm>
#include <thread>
#include <sstream>
#include <cstring>

// ✅ BACnetWorker.h include 제거 - 순환 의존성 방지
// BACnetWorker → BACnetDriver (OK)
// BACnetDriver → BACnetWorker (❌ 불필요)

#ifdef HAS_BACNET_STACK
extern "C" {
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>
}
#endif

using namespace std::chrono;

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 정적 멤버 초기화
// =============================================================================
BACnetDriver* BACnetDriver::instance_ = nullptr;
std::mutex BACnetDriver::instance_mutex_;

#ifdef HAS_BACNET_STACK
// BACnet 스택 전역 변수들
static uint8_t Rx_Buf[1476] = {0};  // ✅ MAX_MPDU → 1476 (표준 크기)
static uint8_t Tx_Buf[1476] = {0};  // ✅ MAX_MPDU → 1476 (표준 크기)
static BACNET_ADDRESS Target_Address;
#endif

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

BACnetDriver::BACnetDriver() 
    : driver_statistics_("BACNET")  // ✅ 표준 통계 구조 직접 초기화
    , local_device_id_(1234)
    , target_ip_("")
    , target_port_(47808)
    , max_apdu_length_(1476)
    , segmentation_support_(true)
    , socket_fd_(-1)
    , is_connected_(false)
    , should_stop_(false)
    , is_bacnet_initialized_(false) {
    
    {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        instance_ = this;
    }
    
    // ✅ BACnet 특화 통계 초기화
    InitializeBACnetStatistics();
    
    auto& logger = LogManager::getInstance();
    logger.Info("🔥 BACnet Driver created with standard statistics");
}

BACnetDriver::~BACnetDriver() {
    Stop();
    Disconnect();
    CleanupBACnetStack();
    
    {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (instance_ == this) {
            instance_ = nullptr;
        }
    }
    
    auto& logger = LogManager::getInstance();
    logger.Info("BACnet Driver destroyed");
}

// =============================================================================
// IProtocolDriver 핵심 메서드 구현
// =============================================================================

bool BACnetDriver::Initialize(const PulseOne::Structs::DriverConfig& config) {
    config_ = config;
    status_.store(PulseOne::Structs::DriverStatus::INITIALIZING);
    
    auto& logger = LogManager::getInstance();
    logger.Info("🔧 Initializing BACnet Driver");
    
    try {
        // 1. 설정 파싱
        ParseDriverConfig(config);
        
        // 2. BACnet 스택 초기화
        if (!InitializeBACnetStack()) {
            SetError(PulseOne::Enums::ErrorCode::INITIALIZATION_FAILED, "Failed to initialize BACnet stack");
            status_.store(PulseOne::Structs::DriverStatus::ERROR);
            return false;
        }
        
        status_.store(PulseOne::Structs::DriverStatus::DISCONNECTED);
        logger.Info("✅ BACnet Driver initialized successfully");
        
        return true;
        
    } catch (const std::exception& e) {
        SetError(PulseOne::Enums::ErrorCode::INITIALIZATION_FAILED, std::string("Exception: ") + e.what());
        status_.store(PulseOne::Structs::DriverStatus::ERROR);
        return false;
    }
}

bool BACnetDriver::Connect() {
    if (is_connected_.load()) {
        return true;
    }
    
    auto& logger = LogManager::getInstance();
    logger.Info("🔗 Connecting BACnet Driver...");
    
    try {
        status_.store(PulseOne::Structs::DriverStatus::CONNECTING);
        
        // 1. UDP 소켓 생성
        if (!CreateSocket()) {
            SetError(PulseOne::Enums::ErrorCode::CONNECTION_FAILED, "Failed to create UDP socket");
            status_.store(PulseOne::Structs::DriverStatus::ERROR);
            return false;
        }
        
        is_connected_.store(true);
        status_.store(PulseOne::Structs::DriverStatus::CONNECTED);
        
        // 통계 업데이트
        driver_statistics_.IncrementProtocolCounter("successful_connections", 1);
        
        logger.Info("✅ BACnet Driver connected successfully");
        return true;
        
    } catch (const std::exception& e) {
        SetError(PulseOne::Enums::ErrorCode::CONNECTION_FAILED, std::string("Exception: ") + e.what());
        status_.store(PulseOne::Structs::DriverStatus::ERROR);
        return false;
    }
}

bool BACnetDriver::Disconnect() {
    if (!is_connected_.load()) {
        return true;
    }
    
    auto& logger = LogManager::getInstance();
    logger.Info("🔌 Disconnecting BACnet Driver...");
    
    try {
        // 1. 네트워크 스레드 정지 (시뮬레이션)
        // StopNetworkThread();
        
        // 2. 소켓 해제
        CloseSocket();
        
        is_connected_.store(false);
        status_.store(PulseOne::Structs::DriverStatus::DISCONNECTED);
        
        // 통계 업데이트
        driver_statistics_.IncrementProtocolCounter("disconnections", 1);
        
        logger.Info("✅ BACnet Driver disconnected successfully");
        return true;
        
    } catch (const std::exception& e) {
        SetError(PulseOne::Enums::ErrorCode::DISCONNECTION_FAILED, std::string("Exception: ") + e.what());
        return false;
    }
}

bool BACnetDriver::IsConnected() const {
    return is_connected_.load();
}

bool BACnetDriver::Start() {
    auto& logger = LogManager::getInstance();
    logger.Info("▶️ Starting BACnet Driver...");
    
    if (!Connect()) {
        return false;
    }
    
    should_stop_.store(false);
    status_.store(PulseOne::Structs::DriverStatus::RUNNING);
    
    logger.Info("✅ BACnet Driver started successfully");
    return true;
}

bool BACnetDriver::Stop() {
    auto& logger = LogManager::getInstance();
    logger.Info("⏹️ Stopping BACnet Driver...");
    
    should_stop_.store(true);
    
    if (!Disconnect()) {
        logger.Warn("Warning: BACnet Driver disconnect issues during stop");
    }
    
    status_.store(PulseOne::Structs::DriverStatus::STOPPED);
    
    logger.Info("✅ BACnet Driver stopped successfully");
    return true;
}

// =============================================================================
// 데이터 읽기/쓰기 메서드
// =============================================================================

bool BACnetDriver::ReadValues(const std::vector<PulseOne::Structs::DataPoint>& points,
                             std::vector<PulseOne::Structs::TimestampedValue>& values) {
    if (!is_connected_.load()) {
        SetError(PulseOne::Enums::ErrorCode::NOT_CONNECTED, "Driver not connected");
        return false;
    }
    
    auto& logger = LogManager::getInstance();
    logger.Debug("📖 Reading " + std::to_string(points.size()) + " BACnet values");
    
    try {
        values.clear();
        values.reserve(points.size());
        
        size_t successful_reads = 0;
        auto start_time = steady_clock::now();
        
        for (const auto& point : points) {
            PulseOne::Structs::TimestampedValue value;
            value.timestamp = system_clock::now();
            
            if (ReadSingleProperty(point, value)) {
                successful_reads++;
            } else {
                // 실패 시에도 기본값으로 추가
                value.timestamp = system_clock::now();
                value.quality = PulseOne::Enums::DataQuality::BAD;
            }
            
            values.push_back(value);
        }
        
        auto duration = duration_cast<milliseconds>(steady_clock::now() - start_time);
        
        // 통계 업데이트
        driver_statistics_.total_reads.fetch_add(points.size());
        driver_statistics_.successful_reads.fetch_add(successful_reads);
        driver_statistics_.failed_reads.fetch_add(points.size() - successful_reads);
        driver_statistics_.UpdateResponseTime(static_cast<double>(duration.count()));
        driver_statistics_.IncrementProtocolCounter("property_reads", points.size());
        
        logger.Debug("✅ Read completed: " + std::to_string(successful_reads) + "/" + std::to_string(points.size()));
        
        return successful_reads > 0;
        
    } catch (const std::exception& e) {
        SetError(PulseOne::Enums::ErrorCode::READ_FAILED, std::string("Exception: ") + e.what());
        driver_statistics_.IncrementProtocolCounter("protocol_errors", 1);
        return false;
    }
}

bool BACnetDriver::WriteValue(const PulseOne::Structs::DataPoint& point, 
                             const PulseOne::Structs::DataValue& value) {
    if (!is_connected_.load()) {
        SetError(PulseOne::Enums::ErrorCode::NOT_CONNECTED, "Driver not connected");
        return false;
    }
    
    auto& logger = LogManager::getInstance();
    logger.Debug("✏️ Writing BACnet value to " + point.address);
    
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
        SetError(PulseOne::Enums::ErrorCode::WRITE_FAILED, std::string("Exception: ") + e.what());
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

void BACnetDriver::SetError(PulseOne::Enums::ErrorCode code, const std::string& message) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    
    last_error_.code = static_cast<PulseOne::Structs::ErrorCode>(code);  // ✅ 타입 변환
    last_error_.message = message;
    last_error_.timestamp = system_clock::now();  // ✅ 직접 사용
    
    auto& logger = LogManager::getInstance();
    logger.Error("BACnet Driver Error [" + std::to_string(static_cast<int>(code)) + "]: " + message);
}

// =============================================================================
// 내부 구현 메서드들
// =============================================================================

void BACnetDriver::ParseDriverConfig(const PulseOne::Structs::DriverConfig& config) {
    auto& logger = LogManager::getInstance();
    logger.Info("🔧 Parsing BACnet driver configuration");
    
    // Device ID 설정
    auto it = config.properties.find("device_id");
    if (it != config.properties.end()) {
        local_device_id_ = std::stoul(it->second);
    } else {
        logger.Warn("device_id not specified, using default: " + std::to_string(local_device_id_));
    }
    
    // Target IP 설정  
    it = config.properties.find("target_ip");
    if (it != config.properties.end()) {
        target_ip_ = it->second;
    } else {
        target_ip_ = "192.168.1.255"; // 브로드캐스트 기본값
        logger.Warn("target_ip not specified, using broadcast: " + target_ip_);
    }
    
    // Port 설정
    it = config.properties.find("port");
    if (it != config.properties.end()) {
        target_port_ = static_cast<uint16_t>(std::stoul(it->second));
    } else {
        target_port_ = 47808; // BACnet 표준 포트
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
    
    logger.Info("BACnet config: Device=" + std::to_string(local_device_id_) + 
               ", Target=" + target_ip_ + ":" + std::to_string(target_port_) +
               ", APDU=" + std::to_string(max_apdu_length_));
}

void BACnetDriver::InitializeBACnetStatistics() {
    // ✅ BACnet 특화 통계 카운터 초기화 (표준 구조 사용)
    driver_statistics_.IncrementProtocolCounter("property_reads", 0);
    driver_statistics_.IncrementProtocolCounter("property_writes", 0);
    driver_statistics_.IncrementProtocolCounter("who_is_sent", 0);
    driver_statistics_.IncrementProtocolCounter("i_am_received", 0);
    driver_statistics_.IncrementProtocolCounter("cov_notifications", 0);
    driver_statistics_.IncrementProtocolCounter("devices_discovered", 0);
    driver_statistics_.IncrementProtocolCounter("protocol_errors", 0);
    driver_statistics_.IncrementProtocolCounter("network_errors", 0);
    driver_statistics_.IncrementProtocolCounter("timeout_errors", 0);
    driver_statistics_.IncrementProtocolCounter("successful_connections", 0);
    driver_statistics_.IncrementProtocolCounter("disconnections", 0);
    
    // ✅ BACnet 특화 메트릭 초기화
    driver_statistics_.SetProtocolMetric("max_apdu_size", static_cast<double>(max_apdu_length_));
    driver_statistics_.SetProtocolMetric("avg_discovery_time_ms", 0.0);
    driver_statistics_.SetProtocolMetric("network_utilization", 0.0);
    
    // ✅ BACnet 특화 상태 정보
    driver_statistics_.SetProtocolStatus("device_instance", std::to_string(local_device_id_));
    driver_statistics_.SetProtocolStatus("network_number", "0");
    driver_statistics_.SetProtocolStatus("segmentation_support", segmentation_support_ ? "true" : "false");
}

bool BACnetDriver::InitializeBACnetStack() {
    auto& logger = LogManager::getInstance();
    logger.Info("🔧 Initializing BACnet stack");
    
    try {
        is_bacnet_initialized_.store(false);
        
#ifdef HAS_BACNET_STACK
        // BACnet 스택 기본 초기화
        // Device 객체 설정은 SetupLocalDevice에서 수행
        
        // TSM (Transaction State Machine) 초기화
        tsm_init();
        
        // 기본 주소 테이블 초기화
        address_init();
        
        logger.Info("✅ BACnet stack initialized");
#else
        logger.Info("✅ BACnet simulation mode initialized");
#endif
        
        is_bacnet_initialized_.store(true);
        return true;
        
    } catch (const std::exception& e) {
        logger.Error("❌ BACnet stack initialization failed: " + std::string(e.what()));
        return false;
    }
}

void BACnetDriver::CleanupBACnetStack() {
    auto& logger = LogManager::getInstance();
    logger.Info("🧹 Cleaning up BACnet stack");
    
    is_bacnet_initialized_.store(false);
    
    // 추가 정리 작업이 필요하면 여기에 추가
}

bool BACnetDriver::SetupLocalDevice() {
    auto& logger = LogManager::getInstance();
    logger.Info("🔧 Setting up local BACnet device");
    
    try {
#ifdef HAS_BACNET_STACK
        // 로컬 디바이스 ID 설정
        Device_Set_Object_Instance_Number(local_device_id_);
        
        // 디바이스 속성 설정
        Device_Set_Object_Name("PulseOne BACnet Collector");
        Device_Set_Model_Name("PulseOne Collector v1.0");
        Device_Set_Vendor_Name("PulseOne Systems");
        Device_Set_Vendor_Identifier(555); // 예시 벤더 ID
        Device_Set_Description("PulseOne Industrial Data Collector");
        
        // BACnet 서비스 활성화
        Device_Set_System_Status(STATUS_OPERATIONAL, false);
        
        logger.Info("✅ Local BACnet device setup completed");
        logger.Info("   Device ID: " + std::to_string(local_device_id_));
        logger.Info("   Device Name: PulseOne BACnet Collector");
#else
        logger.Info("✅ Local BACnet device simulation setup completed");
#endif
        
        return true;
        
    } catch (const std::exception& e) {
        logger.Error("❌ Local device setup failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 네트워크 관리 메서드들 (기본 구현)
// =============================================================================

bool BACnetDriver::CreateSocket() {
    // TODO: UDP 소켓 생성 구현
    auto& logger = LogManager::getInstance();
    logger.Info("🔌 Creating BACnet UDP socket");
    
    // 시뮬레이션: 소켓 생성 성공
    socket_fd_ = 1; // 가짜 소켓 FD
    
    logger.Info("✅ BACnet UDP socket created");
    return true;
}

void BACnetDriver::CloseSocket() {
    auto& logger = LogManager::getInstance();
    logger.Info("🔌 Closing BACnet UDP socket");
    
    socket_fd_ = -1;
    
    logger.Info("✅ BACnet UDP socket closed");
}

bool BACnetDriver::StartNetworkThread() {
    // TODO: 네트워크 스레드 시작 구현
    auto& logger = LogManager::getInstance();
    logger.Info("🧵 Starting BACnet network thread");
    
    logger.Info("✅ BACnet network thread started");
    return true;
}

void BACnetDriver::StopNetworkThread() {
    // TODO: 네트워크 스레드 정지 구현
    auto& logger = LogManager::getInstance();
    logger.Info("🧵 Stopping BACnet network thread");
    
    logger.Info("✅ BACnet network thread stopped");
}

// =============================================================================
// BACnet 프로토콜 메서드들 (기본 구현)
// =============================================================================

bool BACnetDriver::ReadSingleProperty(const PulseOne::Structs::DataPoint& point,
                                     PulseOne::Structs::TimestampedValue& value) {
    // TODO: 실제 BACnet ReadProperty 구현
    auto& logger = LogManager::getInstance();
    logger.Debug("📖 Reading BACnet property: " + point.address);
    
    // 시뮬레이션: 성공적인 읽기
    value.timestamp = system_clock::now();
    value.data_value.type = PulseOne::Enums::DataType::DOUBLE;
    value.data_value.double_value = 23.5; // 가짜 온도 값
    value.quality = PulseOne::Enums::DataQuality::GOOD;
    
    return true;
}

bool BACnetDriver::WriteSingleProperty(const PulseOne::Structs::DataPoint& point,
                                      const PulseOne::Structs::DataValue& value) {
    // TODO: 실제 BACnet WriteProperty 구현
    auto& logger = LogManager::getInstance();
    logger.Debug("✏️ Writing BACnet property: " + point.address);
    
    // 시뮬레이션: 성공적인 쓰기
    return true;
}

} // namespace Drivers
} // namespace PulseOne