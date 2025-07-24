/**
 * @file BACnetWorker.cpp
 * @brief BACnet 프로토콜 워커 클래스 구현
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 1.0.0
 */

#include "Workers/Protocol/BACnetWorker.h"
#include "Utils/LogManager.h"
#include <sstream>
#include <iomanip>
#include <thread>

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
// BaseDeviceWorker 인터페이스 구현
// =============================================================================

std::future<bool> BACnetWorker::Start() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    try {
        LogMessage(PulseOne::LogLevel::INFO, "Starting BACnet worker...");
        
        // 상태를 STARTING으로 변경
        ChangeState(WorkerState::STARTING);
        
        // UDP 연결 수립 (부모 클래스)
        if (!EstablishConnection()) {
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to establish UDP connection");
            ChangeState(WorkerState::ERROR);
            promise->set_value(false);
            return future;
        }
        
        // BACnet 워커 스레드들 시작
        threads_running_ = true;
        discovery_thread_ = std::make_unique<std::thread>(&BACnetWorker::DiscoveryThreadFunction, this);
        polling_thread_ = std::make_unique<std::thread>(&BACnetWorker::PollingThreadFunction, this);
        
        // 상태를 RUNNING으로 변경
        ChangeState(WorkerState::RUNNING);
        
        LogMessage(PulseOne::LogLevel::INFO, "BACnet worker started successfully");
        promise->set_value(true);
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in BACnet Start: " + std::string(e.what()));
        ChangeState(WorkerState::ERROR);
        promise->set_value(false);
    }
    
    return future;
}

std::future<bool> BACnetWorker::Stop() {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    try {
        LogMessage(PulseOne::LogLevel::INFO, "Stopping BACnet worker...");
        
        // 상태를 STOPPING으로 변경
        ChangeState(WorkerState::STOPPING);
        
        // BACnet 워커 스레드들 정지
        if (threads_running_.load()) {
            threads_running_ = false;
            
            if (discovery_thread_ && discovery_thread_->joinable()) {
                discovery_thread_->join();
            }
            if (polling_thread_ && polling_thread_->joinable()) {
                polling_thread_->join();
            }
        }
        
        // UDP 연결 해제 (부모 클래스)
        CloseConnection();
        
        // 상태를 STOPPED로 변경
        ChangeState(WorkerState::STOPPED);
        
        LogMessage(PulseOne::LogLevel::INFO, "BACnet worker stopped");
        promise->set_value(true);
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in BACnet Stop: " + std::string(e.what()));
        ChangeState(WorkerState::ERROR);
        promise->set_value(false);
    }
    
    return future;
}

// =============================================================================
// BACnet 공개 인터페이스
// =============================================================================

void BACnetWorker::ConfigureBACnetWorker(const BACnetWorkerConfig& config) {
    worker_config_ = config;
    LogMessage(PulseOne::LogLevel::INFO, "BACnet worker configuration updated");
}

std::string BACnetWorker::GetBACnetWorkerStats() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"bacnet_worker_statistics\": {\n";
    ss << "    \"discovery_attempts\": " << worker_stats_.discovery_attempts.load() << ",\n";
    ss << "    \"devices_discovered\": " << worker_stats_.devices_discovered.load() << ",\n";
    ss << "    \"polling_cycles\": " << worker_stats_.polling_cycles.load() << ",\n";
    ss << "    \"read_operations\": " << worker_stats_.read_operations.load() << ",\n";
    ss << "    \"write_operations\": " << worker_stats_.write_operations.load() << ",\n";
    ss << "    \"successful_reads\": " << worker_stats_.successful_reads.load() << ",\n";
    ss << "    \"successful_writes\": " << worker_stats_.successful_writes.load() << ",\n";
    ss << "    \"failed_operations\": " << worker_stats_.failed_operations.load() << ",\n";
    
    // 통계 시간 정보
    auto start_time = duration_cast<seconds>(worker_stats_.start_time.time_since_epoch()).count();
    auto reset_time = duration_cast<seconds>(worker_stats_.last_reset.time_since_epoch()).count();
    ss << "    \"start_timestamp\": " << start_time << ",\n";
    ss << "    \"last_reset_timestamp\": " << reset_time << ",\n";
    
    // 발견된 디바이스 수
    {
        std::lock_guard<std::mutex> lock(devices_mutex_);
        ss << "    \"current_devices_count\": " << discovered_devices_.size() << "\n";
    }
    
    ss << "  },\n";
    
    // BACnet 드라이버 통계 추가
    if (bacnet_driver_) {
        try {
            auto driver_stats = bacnet_driver_->GetStatistics();
            ss << "  \"bacnet_driver_statistics\": {\n";
            ss << "    \"successful_connections\": " << driver_stats.successful_connections << ",\n";
            ss << "    \"failed_connections\": " << driver_stats.failed_connections << ",\n";
            ss << "    \"total_operations\": " << driver_stats.total_operations << ",\n";
            ss << "    \"successful_operations\": " << driver_stats.successful_operations << ",\n";
            ss << "    \"failed_operations\": " << driver_stats.failed_operations << ",\n";
            ss << "    \"avg_response_time_ms\": " << driver_stats.avg_response_time_ms << ",\n";
            ss << "    \"success_rate\": " << driver_stats.success_rate << ",\n";
            ss << "    \"consecutive_failures\": " << driver_stats.consecutive_failures << "\n";
            ss << "  },\n";
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::WARN, "Failed to get driver statistics: " + std::string(e.what()));
        }
    }
    
    // UDP 통계 추가
    ss << "  \"udp_statistics\": " << GetUdpStats() << "\n";
    ss << "}";
    
    return ss.str();
}

void BACnetWorker::ResetBACnetWorkerStats() {
    worker_stats_.discovery_attempts = 0;
    worker_stats_.devices_discovered = 0;
    worker_stats_.polling_cycles = 0;
    worker_stats_.read_operations = 0;
    worker_stats_.write_operations = 0;
    worker_stats_.successful_reads = 0;
    worker_stats_.successful_writes = 0;
    worker_stats_.failed_operations = 0;
    worker_stats_.last_reset = system_clock::now();
    
    LogMessage(PulseOne::LogLevel::INFO, "BACnet worker statistics reset");
}

std::string BACnetWorker::GetDiscoveredDevices() const {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"discovered_devices\": [\n";
    
    bool first = true;
    for (const auto& [device_id, device] : discovered_devices_) {
        if (!first) ss << ",\n";
        first = false;
        
        ss << "    {\n";
        ss << "      \"device_id\": " << device_id << ",\n";
        ss << "      \"device_name\": \"" << device.device_name << "\",\n";
        ss << "      \"ip_address\": \"" << device.ip_address << "\",\n";
        ss << "      \"port\": " << device.port << ",\n";
        ss << "      \"max_apdu_length\": " << device.max_apdu_length << ",\n";
        ss << "      \"segmentation_supported\": " << (device.segmentation_supported ? "true" : "false") << ",\n";
        
        // 마지막 발견 시간
        auto last_seen = duration_cast<seconds>(device.last_seen.time_since_epoch()).count();
        ss << "      \"last_seen_timestamp\": " << last_seen << "\n";
        ss << "    }";
    }
    
    ss << "\n  ],\n";
    ss << "  \"total_count\": " << discovered_devices_.size() << "\n";
    ss << "}";
    
    return ss.str();
}

bool BACnetWorker::PerformDiscovery() {
    try {
        LogMessage(PulseOne::LogLevel::INFO, "Performing manual BACnet discovery...");
        
        if (!bacnet_driver_ || !bacnet_driver_->IsConnected()) {
            LogMessage(PulseOne::LogLevel::ERROR, "BACnet driver not connected");
            return false;
        }
        
        // Who-Is 전송
        bool success = bacnet_driver_->SendWhoIs();
        
        if (success) {
            worker_stats_.discovery_attempts++;
            
            // 잠시 대기 후 발견된 디바이스 업데이트
            std::this_thread::sleep_for(milliseconds(2000));
            
            // 드라이버에서 발견된 디바이스 정보 가져오기
            try {
                auto driver_devices = bacnet_driver_->GetDiscoveredDevices();
                
                std::lock_guard<std::mutex> lock(devices_mutex_);
                for (const auto& device : driver_devices) {
                    discovered_devices_[device.device_id] = device;
                }
                
                worker_stats_.devices_discovered = discovered_devices_.size();
                
                LogMessage(PulseOne::LogLevel::INFO, 
                          "Discovery completed. Found " + std::to_string(driver_devices.size()) + " devices");
                
                UpdateWorkerStats("discovery", true);
                return true;
                
            } catch (const std::exception& e) {
                LogMessage(PulseOne::LogLevel::ERROR, "Failed to get discovered devices: " + std::string(e.what()));
                UpdateWorkerStats("discovery", false);
                return false;
            }
        } else {
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to send Who-Is request");
            UpdateWorkerStats("discovery", false);
            return false;
        }
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in PerformDiscovery: " + std::string(e.what()));
        UpdateWorkerStats("discovery", false);
        return false;
    }
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
    if (worker_config_.auto_discovery) {
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
// BACnet 워커 핵심 기능
// =============================================================================

bool BACnetWorker::InitializeBACnetDriver() {
    try {
        if (!bacnet_driver_) {
            LogMessage(PulseOne::LogLevel::ERROR, "BACnet driver not created");
            return false;
        }
        
        // 드라이버 설정 생성
        auto driver_config = CreateDriverConfig();
        
        // 드라이버 초기화
        if (!bacnet_driver_->Initialize(driver_config)) {
            auto error = bacnet_driver_->GetLastError();
            LogMessage(PulseOne::LogLevel::ERROR, "BACnet driver initialization failed: " + error.message);
            return false;
        }
        
        // 드라이버 연결
        if (!bacnet_driver_->Connect()) {
            auto error = bacnet_driver_->GetLastError();
            LogMessage(PulseOne::LogLevel::ERROR, "BACnet driver connection failed: " + error.message);
            return false;
        }
        
        LogMessage(PulseOne::LogLevel::INFO, "BACnet driver initialized and connected successfully");
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in InitializeBACnetDriver: " + std::string(e.what()));
        return false;
    }
}

void BACnetWorker::ShutdownBACnetDriver() {
    try {
        if (bacnet_driver_) {
            bacnet_driver_->Disconnect();
            LogMessage(PulseOne::LogLevel::INFO, "BACnet driver shutdown complete");
        }
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in ShutdownBACnetDriver: " + std::string(e.what()));
    }
}

bool BACnetWorker::ProcessDataPoints(const std::vector<PulseOne::DataPoint>& points) {
    try {
        if (!bacnet_driver_ || !bacnet_driver_->IsConnected()) {
            LogMessage(PulseOne::LogLevel::WARN, "BACnet driver not connected for data point processing");
            return false;
        }
        
        if (points.empty()) {
            return true;
        }
        
        // 데이터 포인트 읽기
        std::vector<PulseOne::TimestampedValue> values;
        bool success = bacnet_driver_->ReadValues(points, values);
        
        if (success) {
            worker_stats_.read_operations++;
            worker_stats_.successful_reads++;
            
            // 각 값을 InfluxDB에 저장
            for (size_t i = 0; i < points.size() && i < values.size(); ++i) {
                SaveToInfluxDB(points[i].id, values[i]);
            }
            
            LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                      "Successfully processed " + std::to_string(points.size()) + " data points");
            UpdateWorkerStats("data_processing", true);
            
        } else {
            worker_stats_.read_operations++;
            worker_stats_.failed_operations++;
            
            auto error = bacnet_driver_->GetLastError();
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to read data points: " + error.message);
            UpdateWorkerStats("data_processing", false);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in ProcessDataPoints: " + std::string(e.what()));
        worker_stats_.failed_operations++;
        UpdateWorkerStats("data_processing", false);
        return false;
    }
}

// =============================================================================
// 내부 메서드 (private)
// =============================================================================

bool BACnetWorker::ParseBACnetWorkerConfig() {
    try {
        // ❌ Before: 없는 필드 사용
        /*
        if (device_info_.config_json.empty()) {
            LogMessage(LogLevel::INFO, "No BACnet worker config in device_info, using defaults");
            return true;
        }
        std::string config = device_info_.config_json;
        // JSON 파싱...
        worker_config_.apdu_timeout = static_cast<uint32_t>(device_info_.timeout_ms);
        worker_config_.polling_interval = static_cast<uint32_t>(device_info_.polling_interval_ms);
        */
        
        // ✅ After: 기존 DeviceInfo 필드들 직접 사용 (함수 추가 없이)
        
        // 1. 기본 유효성 검사
        if (device_info_.name.empty() || device_info_.endpoint.empty()) {
            LogMessage(PulseOne::LogLevel::ERROR, "Device info incomplete for BACnet configuration");
            return false;
        }
        
        LogMessage(PulseOne::LogLevel::INFO, "Configuring BACnet worker from device info");
        
        // 2. Duration을 밀리초로 변환 (간단하게 인라인)
        worker_config_.apdu_timeout = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(device_info_.timeout).count()
        );
        
        worker_config_.polling_interval = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(device_info_.polling_interval).count()
        );
        
        // 3. 재시도 설정
        worker_config_.max_retries = static_cast<uint32_t>(device_info_.retry_count);
        
        // 4. endpoint 파싱 (인라인으로 간단하게)
        std::string endpoint = device_info_.endpoint;
        size_t colon_pos = endpoint.find(':');
        
        if (colon_pos != std::string::npos) {
            // IP 추출
            std::string ip = endpoint.substr(0, colon_pos);
            worker_config_.target_ip = ip;
            
            // 포트 추출  
            std::string port_str = endpoint.substr(colon_pos + 1);
            worker_config_.target_port = static_cast<uint16_t>(std::stoi(port_str));
        } else {
            // IP만 있는 경우
            worker_config_.target_ip = endpoint;
            worker_config_.target_port = 47808;  // BACnet 기본 포트
        }
        
        // 5. 기본 BACnet 설정
        worker_config_.device_instance = PulseOne::Constants::BACNET_DEFAULT_DEVICE_INSTANCE;
        worker_config_.max_apdu_length = PulseOne::Constants::BACNET_MAX_APDU_LENGTH;
        
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                  "BACnet Config - IP: " + worker_config_.target_ip + 
                  ", Port: " + std::to_string(worker_config_.target_port) +
                  ", Timeout: " + std::to_string(worker_config_.apdu_timeout) + "ms");
        
        LogMessage(PulseOne::LogLevel::INFO, "BACnet worker config parsed successfully");
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Failed to parse BACnet worker config: " + std::string(e.what()));
        return false;
    }
}

void BACnetWorker::DiscoveryThreadFunction() {
    LogMessage(PulseOne::LogLevel::INFO, "BACnet discovery thread started");
    
    while (threads_running_.load()) {
        try {
            if (worker_config_.auto_discovery) {
                PerformDiscovery();
                
                // 다음 디스커버리까지 대기
                auto next_discovery = steady_clock::now() + 
                                     milliseconds(worker_config_.discovery_interval);
                
                while (threads_running_.load() && steady_clock::now() < next_discovery) {
                    std::this_thread::sleep_for(milliseconds(1000));
                }
            } else {
                // 자동 디스커버리가 비활성화된 경우 대기
                std::this_thread::sleep_for(milliseconds(5000));
            }
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Exception in discovery thread: " + std::string(e.what()));
            std::this_thread::sleep_for(milliseconds(5000));
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "BACnet discovery thread stopped");
}

void BACnetWorker::PollingThreadFunction() {
    LogMessage(PulseOne::LogLevel::INFO, "BACnet polling thread started");
    
    while (threads_running_.load()) {
        try {
            // 데이터 포인트들 폴링
            auto data_points = GetDataPoints();
            
            if (!data_points.empty()) {
                worker_stats_.polling_cycles++;
                ProcessDataPoints(data_points);
            }
            
            // 다음 폴링까지 대기
            std::this_thread::sleep_for(milliseconds(worker_config_.polling_interval));
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Exception in polling thread: " + std::string(e.what()));
            std::this_thread::sleep_for(milliseconds(5000));
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "BACnet polling thread stopped");
}

PulseOne::Structs::DriverConfig BACnetWorker::CreateDriverConfig() {
    PulseOne::Structs::DriverConfig config;
    
    config.device_id = device_info_.id;  // UUID 사용
    config.name = device_info_.name;
    config.protocol = PulseOne::ProtocolType::BACNET_IP;
    config.endpoint = device_info_.endpoint;
    config.timeout = std::chrono::milliseconds(worker_config_.apdu_timeout);
    config.retry_count = worker_config_.apdu_retries;
    config.polling_interval = std::chrono::milliseconds(worker_config_.polling_interval);
    
    // BACnet 특화 설정
    config.custom_settings["device_id"] = std::to_string(worker_config_.device_id);
    config.custom_settings["port"] = std::to_string(worker_config_.port);
    config.custom_settings["auto_discovery"] = worker_config_.auto_discovery ? "true" : "false";
    
    return config;
}

void BACnetWorker::UpdateWorkerStats(const std::string& operation, bool success) {
    try {
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                  "BACnet worker operation: " + operation + 
                  " (success: " + (success ? "true" : "false") + ")");
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in UpdateWorkerStats: " + std::string(e.what()));
    }
}

} // namespace Workers
} // namespace PulseOne