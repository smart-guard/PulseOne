/**
 * @file BACnetWorker.cpp
 * @brief BACnet 프로토콜 워커 클래스 구현 - 🔥 모든 컴파일 에러 완전 해결
 * @author PulseOne Development Team
 * @date 2025-08-03
 * @version 1.0.0
 * 
 * 🔥 주요 수정사항:
 * 1. BaseDeviceWorker Start/Stop 인터페이스로 변경
 * 2. BACnetDeviceInfo 멤버명 정확히 매칭
 * 3. BACnetObjectInfo 멤버명 정확히 매칭
 * 4. UUID vs uint32_t 타입 불일치 해결
 * 5. BACNET_ADDRESS 출력 문제 해결
 */

#include "Workers/Protocol/BACnetWorker.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <iomanip>
#include <thread>

// Windows 매크로 충돌 방지
#ifdef min
#undef min
#endif
#ifdef max  
#undef max
#endif

using namespace std::chrono;

namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

BACnetWorker::BACnetWorker(
    const PulseOne::DeviceInfo& device_info,
    std::shared_ptr<RedisClient> redis_client,
    std::shared_ptr<InfluxClient> influx_client)
    : UdpBasedWorker(device_info, redis_client, influx_client)
    , threads_running_(false) {
    
    // BACnet 워커 통계 초기화
    worker_stats_.start_time = system_clock::now();
    worker_stats_.last_reset = worker_stats_.start_time;
    
    LogMessage(PulseOne::LogLevel::INFO, "BACnetWorker created for device: " + device_info.name);
    
    // device_info에서 BACnet 워커 설정 파싱
    if (!ParseBACnetWorkerConfig()) {
        LogMessage(PulseOne::LogLevel::WARN, "Failed to parse BACnet worker config, using defaults");
    }
    
    // BACnet 드라이버 생성
    bacnet_driver_ = std::make_unique<Drivers::BACnetDriver>();
}

BACnetWorker::~BACnetWorker() {
    // 스레드 정리
    if (threads_running_.load()) {
        threads_running_ = false;
        
        if (discovery_thread_ && discovery_thread_->joinable()) {
            discovery_thread_->join();
        }
        if (polling_thread_ && polling_thread_->joinable()) {
            polling_thread_->join();
        }
    }
    
    // BACnet 드라이버 정리
    ShutdownBACnetDriver();
    
    LogMessage(PulseOne::LogLevel::INFO, "BACnetWorker destroyed for device: " + device_info_.name);
}

// =============================================================================
// 🔥 BaseDeviceWorker 인터페이스 구현 (Start/Stop만)
// =============================================================================

std::future<bool> BACnetWorker::Start() {
    return std::async(std::launch::async, [this]() -> bool {
        LogMessage(PulseOne::LogLevel::INFO, "Starting BACnetWorker...");
        
        // 1. BACnet 워커 설정 파싱
        if (!ParseBACnetWorkerConfig()) {
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to parse BACnet worker configuration");
            return false;
        }
        
        // 2. 통계 초기화
        worker_stats_.Reset();
        
        // 3. UDP 연결 수립 (부모 클래스)
        if (!EstablishConnection()) {
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to establish UDP connection");
            return false;
        }
        
        // 4. 스레드들 시작
        if (!threads_running_.load()) {
            threads_running_ = true;
            
            // 디스커버리 스레드 시작 (설정에 따라)
            if (worker_config_.auto_device_discovery) {
                discovery_thread_ = std::make_unique<std::thread>(&BACnetWorker::DiscoveryThreadFunction, this);
            }
            
            // 폴링 스레드 시작
            polling_thread_ = std::make_unique<std::thread>(&BACnetWorker::PollingThreadFunction, this);
        }
        
        LogMessage(PulseOne::LogLevel::INFO, "BACnetWorker started successfully");
        return true;
    });
}

std::future<bool> BACnetWorker::Stop() {
    return std::async(std::launch::async, [this]() -> bool {
        LogMessage(PulseOne::LogLevel::INFO, "Stopping BACnetWorker...");
        
        // 1. 스레드들 정리
        if (threads_running_.load()) {
            threads_running_ = false;
            
            if (discovery_thread_ && discovery_thread_->joinable()) {
                discovery_thread_->join();
            }
            if (polling_thread_ && polling_thread_->joinable()) {
                polling_thread_->join();
            }
        }
        
        // 2. BACnet 드라이버 정리
        ShutdownBACnetDriver();
        
        // 3. UDP 연결 해제 (부모 클래스)
        CloseConnection();
        
        LogMessage(PulseOne::LogLevel::INFO, "BACnetWorker stopped successfully");
        return true;
    });
}

// =============================================================================
// UdpBasedWorker 순수 가상 함수 구현
// =============================================================================

bool BACnetWorker::EstablishProtocolConnection() {
    LogMessage(PulseOne::LogLevel::INFO, "Establishing BACnet protocol connection...");
    
    if (!InitializeBACnetDriver()) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to initialize BACnet driver");
        return false;
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "BACnet protocol connection established");
    return true;
}

bool BACnetWorker::CloseProtocolConnection() {
    LogMessage(PulseOne::LogLevel::INFO, "Closing BACnet protocol connection...");
    
    ShutdownBACnetDriver();
    
    LogMessage(PulseOne::LogLevel::INFO, "BACnet protocol connection closed");
    return true;
}

bool BACnetWorker::CheckProtocolConnection() {
    if (!bacnet_driver_) {
        return false;
    }
    
    return bacnet_driver_->IsConnected();
}

bool BACnetWorker::SendProtocolKeepAlive() {
    // BACnet은 명시적인 Keep-alive가 없으므로 주기적 Who-Is로 대체
    if (worker_config_.auto_device_discovery) {
        return PerformDiscovery();
    }
    return true;
}

bool BACnetWorker::ProcessReceivedPacket(const UdpPacket& packet) {
    try {
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                  "Processing BACnet packet (" + std::to_string(packet.data.size()) + " bytes)");
        
        // BACnet 드라이버가 UDP 패킷 처리를 담당하지 않으므로
        // 여기서는 기본적인 통계만 업데이트
        UpdateWorkerStats("packet_received", true);
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in ProcessReceivedPacket: " + std::string(e.what()));
        UpdateWorkerStats("packet_received", false);
        return false;
    }
}

// =============================================================================
// BACnet 워커 설정 및 관리
// =============================================================================

void BACnetWorker::ConfigureBACnetWorker(const BACnetWorkerConfig& config) {
    worker_config_ = config;
    LogMessage(PulseOne::LogLevel::INFO, "BACnet worker configuration updated");
}

std::string BACnetWorker::GetBACnetWorkerStats() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"discovery_attempts\": " << worker_stats_.discovery_attempts.load() << ",\n";
    ss << "  \"devices_discovered\": " << worker_stats_.devices_discovered.load() << ",\n";
    ss << "  \"polling_cycles\": " << worker_stats_.polling_cycles.load() << ",\n";
    ss << "  \"read_operations\": " << worker_stats_.read_operations.load() << ",\n";
    ss << "  \"write_operations\": " << worker_stats_.write_operations.load() << ",\n";
    ss << "  \"failed_operations\": " << worker_stats_.failed_operations.load() << ",\n";
    ss << "  \"cov_notifications\": " << worker_stats_.cov_notifications.load() << ",\n";
    
    // 시간 정보
    auto start_time = std::chrono::duration_cast<std::chrono::seconds>(
        worker_stats_.start_time.time_since_epoch()).count();
    auto last_reset = std::chrono::duration_cast<std::chrono::seconds>(
        worker_stats_.last_reset.time_since_epoch()).count();
    
    ss << "  \"start_time\": " << start_time << ",\n";
    ss << "  \"last_reset\": " << last_reset << "\n";
    ss << "}";
    
    return ss.str();
}

void BACnetWorker::ResetBACnetWorkerStats() {
    worker_stats_.Reset();
    LogMessage(PulseOne::LogLevel::INFO, "BACnet worker statistics reset");
}

std::string BACnetWorker::GetDiscoveredDevicesAsJson() const {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"devices\": [\n";
    
    bool first = true;
    for (const auto& [device_id, device] : discovered_devices_) {
        if (!first) ss << ",\n";
        first = false;
        
        ss << "    {\n";
        ss << "      \"device_id\": " << device_id << ",\n";
        // ✅ 올바른 멤버명 사용
        ss << "      \"device_name\": \"" << device.device_name << "\",\n";
        ss << "      \"vendor_id\": " << device.vendor_id << ",\n";  // ✅ vendor_name → vendor_id
        ss << "      \"ip_address\": \"" << device.ip_address << "\",\n";
        ss << "      \"port\": " << device.port << ",\n";
        ss << "      \"max_apdu_length\": " << device.max_apdu_length << ",\n";
        ss << "      \"segmentation_support\": " << static_cast<int>(device.segmentation_support) << ",\n";  // ✅ segmentation_supported → segmentation_support
        
        // 마지막 발견 시간
        auto last_seen = std::chrono::duration_cast<std::chrono::seconds>(
            device.last_seen.time_since_epoch()).count();
        ss << "      \"last_seen_timestamp\": " << last_seen << "\n";
        ss << "    }";
    }
    
    ss << "\n  ],\n";
    ss << "  \"total_count\": " << discovered_devices_.size() << "\n";
    ss << "}";
    
    return ss.str();
}

// =============================================================================
// BACnet 디바이스 발견 기능
// =============================================================================

bool BACnetWorker::StartDiscovery() {
    LogMessage(PulseOne::LogLevel::INFO, "Starting BACnet device discovery...");
    
    if (!bacnet_driver_ || !bacnet_driver_->IsConnected()) {
        LogMessage(PulseOne::LogLevel::ERROR, "BACnet driver not connected");
        return false;
    }
    
    // 디스커버리 활성화
    worker_config_.auto_device_discovery = true;
    
    // 디스커버리 스레드가 실행 중이 아니면 시작
    if (!discovery_thread_ || !discovery_thread_->joinable()) {
        threads_running_ = true;
        discovery_thread_ = std::make_unique<std::thread>(&BACnetWorker::DiscoveryThreadFunction, this);
    }
    
    return true;
}

void BACnetWorker::StopDiscovery() {
    LogMessage(PulseOne::LogLevel::INFO, "Stopping BACnet device discovery...");
    
    worker_config_.auto_device_discovery = false;
    
    // 디스커버리 스레드 정리는 소멸자에서 처리
}

std::vector<BACnetDeviceInfo> BACnetWorker::GetDiscoveredDevices() const {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    std::vector<BACnetDeviceInfo> devices;
    devices.reserve(discovered_devices_.size());
    
    for (const auto& [device_id, device] : discovered_devices_) {
        devices.push_back(device);
    }
    
    return devices;
}

std::vector<BACnetObjectInfo> BACnetWorker::GetDiscoveredObjects(uint32_t device_id) const {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    // TODO: 실제 객체 목록 반환 구현
    std::vector<BACnetObjectInfo> objects;
    
    auto it = discovered_devices_.find(device_id);
    if (it != discovered_devices_.end()) {
        // TODO: 디바이스별 객체 목록 구현
        objects = it->second.objects;  // BACnetDeviceInfo에 objects 벡터가 있다고 가정
    }
    
    return objects;
}

// =============================================================================
// 콜백 함수 설정
// =============================================================================

void BACnetWorker::SetDeviceDiscoveredCallback(DeviceDiscoveredCallback callback) {
    on_device_discovered_ = callback;
}

void BACnetWorker::SetObjectDiscoveredCallback(ObjectDiscoveredCallback callback) {
    on_object_discovered_ = callback;
}

void BACnetWorker::SetValueChangedCallback(ValueChangedCallback callback) {
    on_value_changed_ = callback;
}

// =============================================================================
// 내부 구현 메서드들
// =============================================================================

bool BACnetWorker::ParseBACnetWorkerConfig() {
    try {
        LogMessage(PulseOne::LogLevel::INFO, "Parsing BACnet worker configuration...");
        
        // ✅ 사용하지 않는 변수 주석 처리
        // const auto& device_info = device_info_;  // 직접 멤버 접근
        
        // 기본값 설정 (구조체 초기화)
        worker_config_.local_device_id = 260001;
        worker_config_.target_port = 47808;
        worker_config_.timeout_ms = 5000;
        worker_config_.retry_count = 3;
        worker_config_.discovery_interval_seconds = 300;  // 5분
        worker_config_.auto_device_discovery = true;
        worker_config_.polling_interval_ms = 1000;
        worker_config_.verbose_logging = false;
        
        // properties에서 BACnet 특화 설정 읽기 (안전하게)
        // TODO: device_info_.properties 파싱 로직 추가
        
        // 설정 유효성 검사
        if (!worker_config_.Validate()) {
            LogMessage(PulseOne::LogLevel::ERROR, "Invalid BACnet worker configuration");
            return false;
        }
        
        LogMessage(PulseOne::LogLevel::INFO, 
                  "BACnet worker configured - Device ID: " + 
                  std::to_string(worker_config_.local_device_id) +
                  ", Port: " + std::to_string(worker_config_.target_port));
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception parsing BACnet config: " + std::string(e.what()));
        return false;
    }
}

PulseOne::Structs::DriverConfig BACnetWorker::CreateDriverConfig() {
    PulseOne::Structs::DriverConfig config;
    
    // 기본 설정
    config.device_id = device_info_.id;
    config.endpoint = device_info_.endpoint;
    config.timeout = device_info_.timeout;
    config.retry_count = static_cast<int>(worker_config_.retry_count);
    
    // BACnet 특화 설정들을 properties에 추가
    config.properties["device_id"] = std::to_string(worker_config_.local_device_id);
    config.properties["target_port"] = std::to_string(worker_config_.target_port);
    config.properties["timeout_ms"] = std::to_string(worker_config_.timeout_ms);
    config.properties["enable_cov"] = worker_config_.enable_cov ? "true" : "false";
    config.properties["enable_bulk_read"] = worker_config_.enable_bulk_read ? "true" : "false";
    config.properties["max_apdu_length"] = std::to_string(worker_config_.max_apdu_length);
    config.properties["verbose_logging"] = worker_config_.verbose_logging ? "true" : "false";
    
    return config;
}

bool BACnetWorker::InitializeBACnetDriver() {
    try {
        LogMessage(PulseOne::LogLevel::INFO, "Initializing BACnet driver...");
        
        if (!bacnet_driver_) {
            LogMessage(PulseOne::LogLevel::ERROR, "BACnet driver is null");
            return false;
        }
        
        // 드라이버 설정 생성
        auto driver_config = CreateDriverConfig();
        
        // 드라이버 초기화
        if (!bacnet_driver_->Initialize(driver_config)) {
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to initialize BACnet driver");
            return false;
        }
        
        // 드라이버 연결
        if (!bacnet_driver_->Connect()) {
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to connect BACnet driver");
            return false;
        }
        
        LogMessage(PulseOne::LogLevel::INFO, "BACnet driver initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception initializing BACnet driver: " + std::string(e.what()));
        return false;
    }
}

void BACnetWorker::ShutdownBACnetDriver() {
    try {
        if (bacnet_driver_) {
            bacnet_driver_->Disconnect();
            bacnet_driver_.reset();
            LogMessage(PulseOne::LogLevel::INFO, "BACnet driver shutdown complete");
        }
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception shutting down BACnet driver: " + std::string(e.what()));
    }
}

// =============================================================================
// 스레드 함수들
// =============================================================================

void BACnetWorker::DiscoveryThreadFunction() {
    LogMessage(PulseOne::LogLevel::INFO, "BACnet discovery thread started");
    
    while (threads_running_.load()) {
        try {
            if (PerformDiscovery()) {
                worker_stats_.discovery_attempts++;
                LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Discovery cycle completed");
            }
            
            // 설정된 간격만큼 대기
            std::this_thread::sleep_for(std::chrono::seconds(worker_config_.discovery_interval_seconds));
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Exception in discovery thread: " + std::string(e.what()));
            worker_stats_.failed_operations++;
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "BACnet discovery thread stopped");
}

void BACnetWorker::PollingThreadFunction() {
    LogMessage(PulseOne::LogLevel::INFO, "BACnet polling thread started");
    
    while (threads_running_.load()) {
        try {
            if (PerformPolling()) {
                worker_stats_.polling_cycles++;
                LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Polling cycle completed");
            }
            
            // 설정된 간격만큼 대기
            std::this_thread::sleep_for(std::chrono::milliseconds(worker_config_.polling_interval_ms));
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Exception in polling thread: " + std::string(e.what()));
            worker_stats_.failed_operations++;
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "BACnet polling thread stopped");
}

bool BACnetWorker::PerformDiscovery() {
    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Performing BACnet device discovery...");
    
    try {
        if (!bacnet_driver_ || !bacnet_driver_->IsConnected()) {
            return false;
        }
        
        // Who-Is 브로드캐스트 전송
        // TODO: BACnet 스택 연동 구현
        
        worker_stats_.discovery_attempts++;
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in PerformDiscovery: " + std::string(e.what()));
        return false;
    }
}

bool BACnetWorker::PerformPolling() {
    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Performing BACnet data polling...");
    
    try {
        if (!bacnet_driver_ || !bacnet_driver_->IsConnected()) {
            return false;
        }
        
        // 설정된 데이터 포인트들 읽기
        // TODO: DataPoint 목록 가져와서 처리
        
        worker_stats_.polling_cycles++;
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in PerformPolling: " + std::string(e.what()));
        return false;
    }
}

bool BACnetWorker::ProcessDataPoints(const std::vector<PulseOne::DataPoint>& points) {
    try {
        if (!bacnet_driver_ || !bacnet_driver_->IsConnected()) {
            LogMessage(PulseOne::LogLevel::WARN, "BACnet driver not connected for data point processing");
            return false;
        }
        
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                  "Processing " + std::to_string(points.size()) + " data points");
        
        // 여러 포인트를 한 번에 읽기
        std::vector<PulseOne::Structs::TimestampedValue> values;
        std::vector<PulseOne::Structs::DataPoint> struct_points(points.begin(), points.end());
        bool success = bacnet_driver_->ReadValues(struct_points, values);
        
        if (success) {
            worker_stats_.read_operations++;
            
            // 읽은 값들을 InfluxDB에 저장
            for (size_t i = 0; i < points.size() && i < values.size(); ++i) {
                // TODO: InfluxDB 저장 로직 구현
                if (on_value_changed_) {
                    // ✅ UUID (string) 타입으로 CreateObjectId 호출
                    std::string object_id = CreateObjectId(points[i].device_id, 
                        BACnetObjectInfo{/* TODO: 객체 정보 생성 */});
                    on_value_changed_(object_id, values[i]);
                }
            }
        } else {
            worker_stats_.failed_operations++;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception processing data points: " + std::string(e.what()));
        worker_stats_.failed_operations++;
        return false;
    }
}

void BACnetWorker::UpdateWorkerStats(const std::string& operation, bool success) {
    if (operation == "discovery") {
        worker_stats_.discovery_attempts++;
        if (success) {
            worker_stats_.devices_discovered++;
        }
    } else if (operation == "read") {
        worker_stats_.read_operations++;
        if (!success) {
            worker_stats_.failed_operations++;
        }
    } else if (operation == "write") {
        worker_stats_.write_operations++;
        if (!success) {
            worker_stats_.failed_operations++;
        }
    } else if (operation == "packet_received" || operation == "data_received") {
        // 패킷/데이터 수신 통계는 별도 처리 가능
        if (!success) {
            worker_stats_.failed_operations++;
        }
    }
}

std::string BACnetWorker::CreateObjectId(const std::string& device_id, const BACnetObjectInfo& object_info) const {
    // ✅ UUID string과 올바른 멤버명 사용
    return device_id + ":" + 
           std::to_string(static_cast<int>(object_info.object_type)) + ":" + 
           std::to_string(object_info.object_instance);
}

} // namespace Workers
} // namespace PulseOne