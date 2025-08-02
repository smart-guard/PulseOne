/**
 * @file BACnetWorker.cpp
 * @brief BACnet 프로토콜 워커 클래스 구현 - 완전한 버전
 * @author PulseOne Development Team
 * @date 2025-01-23
 * @version 1.0.0
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
    ss << "    \"failed_operations\": " << worker_stats_.failed_operations.load() << ",\n";
    ss << "    \"cov_notifications\": " << worker_stats_.cov_notifications.load() << ",\n";
    
    // 시간 계산
    auto now = system_clock::now();
    auto uptime_seconds = duration_cast<seconds>(now - worker_stats_.start_time).count();
    auto since_reset_seconds = duration_cast<seconds>(now - worker_stats_.last_reset).count();
    
    ss << "    \"uptime_seconds\": " << uptime_seconds << ",\n";
    ss << "    \"since_reset_seconds\": " << since_reset_seconds << "\n";
    ss << "  },\n";
    
    // 디바이스 통계
    {
        std::lock_guard<std::mutex> lock(devices_mutex_);
        ss << "  \"devices_statistics\": {\n";
        ss << "    \"current_devices_count\": " << discovered_devices_.size() << "\n";
        ss << "  },\n";
    }
    
    // BACnet 드라이버 통계 - atomic 문제 해결
    if (bacnet_driver_) {
        try {
            // 참조로 받아서 복사 방지
            const auto& driver_stats = bacnet_driver_->GetStatistics();
            ss << "  \"driver_statistics\": {\n";
            ss << "    \"total_operations\": " << driver_stats.total_operations.load() << ",\n";
            ss << "    \"successful_operations\": " << driver_stats.successful_operations.load() << ",\n";
            ss << "    \"failed_operations\": " << driver_stats.failed_operations.load() << "\n";
            ss << "  },\n";
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::WARN, "Failed to get driver statistics: " + std::string(e.what()));
        }
    }
    
    // UDP 통계
    ss << "  \"udp_statistics\": " << GetUdpStats() << "\n";
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
    ss << "  \"discovered_devices\": [\n";
    
    bool first = true;
    for (const auto& [device_id, device] : discovered_devices_) {
        if (!first) ss << ",\n";
        first = false;
        
        ss << "    {\n";
        ss << "      \"device_id\": " << device_id << ",\n";
        ss << "      \"device_name\": \"" << device.device_name << "\",\n";
        ss << "      \"vendor_name\": \"" << device.vendor_name << "\",\n";
        ss << "      \"model_name\": \"" << device.model_name << "\",\n";
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
                SaveToInfluxDB(points[i].id, values[i]);
            }
            
            LogMessage(PulseOne::LogLevel::DEBUG_LEVEL,
                      "Successfully processed " + std::to_string(values.size()) + " data points");
            UpdateWorkerStats("data_processing", true);
            return true;
        } else {
            worker_stats_.read_operations++;
            auto error = bacnet_driver_->GetLastError();
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to read data points: " + error.message);
            UpdateWorkerStats("data_processing", false);
            return false;
        }
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in ProcessDataPoints: " + std::string(e.what()));
        worker_stats_.failed_operations++;
        UpdateWorkerStats("data_processing", false);
        return false;
    }
}

// =============================================================================
// BACnet 워커 핵심 기능
// =============================================================================

bool BACnetWorker::ParseBACnetWorkerConfig() {
    try {
        LogMessage(PulseOne::LogLevel::INFO, "Parsing BACnet worker configuration...");
        
        const auto& device_info = device_info_;  // ✅ 직접 멤버 접근
        
        // 기본값 설정 (BACnetDriverConfig 구조체 사용)
        worker_config_.local_device_id = 260001;
        worker_config_.local_port = 47808;
        worker_config_.target_port = 47808;
        worker_config_.timeout_ms = 5000;
        worker_config_.retry_count = 3;
        worker_config_.discovery_interval_seconds = 300;  // 5분
        worker_config_.auto_device_discovery = true;
        worker_config_.enable_cov = false;
        worker_config_.verbose_logging = false;
        
        // device_info에서 설정 추출 (향후 확장)
        // 현재는 기본값 사용
        
        if (!worker_config_.Validate()) {
            LogMessage(PulseOne::LogLevel::ERROR, "Invalid BACnet worker configuration");
            return false;
        }
        
        LogMessage(PulseOne::LogLevel::INFO, 
                  "BACnet worker configuration parsed - Device ID: " + 
                  std::to_string(worker_config_.local_device_id) + 
                  ", Port: " + std::to_string(worker_config_.local_port));
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "ParseBACnetWorkerConfig failed: " + std::string(e.what()));
        return false;
    }
}

PulseOne::Structs::DriverConfig BACnetWorker::CreateDriverConfig() {
    try {
        PulseOne::Structs::DriverConfig config;
        
        // BACnet 드라이버 설정 구성 (올바른 멤버 사용)
        config.device_id = std::to_string(worker_config_.local_device_id);
        config.properties["local_port"] = std::to_string(worker_config_.local_port);
        config.properties["timeout_ms"] = std::to_string(worker_config_.timeout_ms);
        config.properties["retry_count"] = std::to_string(worker_config_.retry_count);
        config.properties["enable_cov"] = worker_config_.enable_cov ? "true" : "false";
        config.properties["verbose_logging"] = worker_config_.verbose_logging ? "true" : "false";
        
        // DriverConfig 구조체에 있는 멤버들만 사용
        config.timeout_ms = worker_config_.timeout_ms;
        config.retry_count = worker_config_.retry_count;
        
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "BACnet driver config created");
        return config;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "CreateDriverConfig failed: " + std::string(e.what()));
        return PulseOne::Structs::DriverConfig{};
    }
}

bool BACnetWorker::InitializeBACnetDriver() {
    try {
        if (!bacnet_driver_) {
            LogMessage(PulseOne::LogLevel::ERROR, "BACnet driver not created");
            return false;
        }
        
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

void BACnetWorker::DiscoveryThreadFunction() {
    LogMessage(PulseOne::LogLevel::INFO, "🔍 BACnet Discovery thread started");
    
    while (threads_running_.load()) {
        try {
            // GetState() 메서드 사용 (public 접근)
            if (GetState() != WorkerState::RUNNING) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                continue;
            }
            
            // 디바이스 발견 수행
            if (PerformDiscovery()) {
                worker_stats_.discovery_attempts++;
                LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Discovery cycle completed successfully");
            } else {
                LogMessage(PulseOne::LogLevel::WARN, "Discovery cycle failed");
            }
            
            // 발견 간격만큼 대기 (초 단위를 밀리초로 변환)
            std::this_thread::sleep_for(std::chrono::seconds(worker_config_.discovery_interval_seconds));
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Exception in DiscoveryThreadFunction: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "🔍 BACnet Discovery thread stopped");
}

void BACnetWorker::PollingThreadFunction() {
    LogMessage(PulseOne::LogLevel::INFO, "📊 BACnet Polling thread started");
    
    while (threads_running_.load()) {
        try {
            // GetState() 메서드 사용 (public 접근)
            if (GetState() != WorkerState::RUNNING) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                continue;
            }
            
            // 발견된 디바이스가 있는지 확인
            size_t device_count = 0;
            {
                std::lock_guard<std::mutex> lock(devices_mutex_);
                device_count = discovered_devices_.size();
            }
            
            if (device_count == 0) {
                LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "No devices discovered yet, skipping polling");
                std::this_thread::sleep_for(std::chrono::seconds(5)); // 기본 5초 대기
                continue;
            }
            
            // 데이터 폴링 수행
            if (PerformPolling()) {
                worker_stats_.polling_cycles++;
                LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Polling cycle completed successfully");
            } else {
                LogMessage(PulseOne::LogLevel::WARN, "Polling cycle failed");
            }
            
            // 폴링 간격만큼 대기 (기본 5초)
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Exception in PollingThreadFunction: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "📊 BACnet Polling thread stopped");
}

bool BACnetWorker::PerformDiscovery() {
    try {
        if (!bacnet_driver_) {
            LogMessage(PulseOne::LogLevel::ERROR, "BACnet driver not available");
            return false;
        }
        
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "🔍 Performing BACnet device discovery...");
        
        // 드라이버를 통해 디바이스 발견
        std::map<uint32_t, BACnetDeviceInfo> driver_devices;
        if (!(bacnet_driver_->DiscoverDevices(driver_devices, 3000) > 0)) {
            LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "No new devices discovered");
            return true;  // 실패가 아님
        }
        
        // 발견된 디바이스들을 내부 맵에 추가
        size_t new_devices = 0;
        {
            std::lock_guard<std::mutex> lock(devices_mutex_);
            for (const auto& [device_id, device] : driver_devices) {
                if (discovered_devices_.find(device.device_id) == discovered_devices_.end()) {
                    discovered_devices_[device.device_id] = device;
                    new_devices++;
                    
                    LogMessage(PulseOne::LogLevel::INFO, 
                              "🆕 New device discovered: " + device.device_name + 
                              " (ID: " + std::to_string(device.device_id) + 
                              ", IP: " + device.ip_address + ")");
                    
                    // 콜백 호출
                    if (on_device_discovered_) {
                        try {
                            on_device_discovered_(device);
                        } catch (const std::exception& e) {
                            LogMessage(PulseOne::LogLevel::ERROR, "Exception in device discovered callback: " + std::string(e.what()));
                        }
                    }
                }
            }
            
            if (new_devices > 0) {
                worker_stats_.devices_discovered += new_devices;
                LogMessage(PulseOne::LogLevel::INFO, 
                          "✅ Discovery completed. Total devices: " + std::to_string(discovered_devices_.size()) +
                          " (+" + std::to_string(new_devices) + " new)");
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in PerformDiscovery: " + std::string(e.what()));
        return false;
    }
}

bool BACnetWorker::PerformPolling() {
    try {
        if (!bacnet_driver_) {
            LogMessage(PulseOne::LogLevel::ERROR, "BACnet driver not available for polling");
            return false;
        }
        
        // 발견된 디바이스들에 대해 객체 발견 및 데이터 읽기
        std::vector<uint32_t> device_ids;
        {
            std::lock_guard<std::mutex> lock(devices_mutex_);
            for (const auto& [device_id, device_info] : discovered_devices_) {
                device_ids.push_back(device_id);
            }
        }
        
        if (device_ids.empty()) {
            return true; // 디바이스가 없으면 성공으로 간주
        }
        
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL,
                  "📊 Polling " + std::to_string(device_ids.size()) + " devices for object discovery and data reading");
        
        size_t total_objects = 0;
        
        // 각 디바이스에 대해 객체 발견 및 값 읽기
        for (uint32_t device_id : device_ids) {
            try {
                // 디바이스의 객체 목록 가져오기
                auto objects = bacnet_driver_->GetDeviceObjects(device_id);
                
                if (!objects.empty()) {
                    total_objects += objects.size();
                    
                    // 새로 발견된 객체들 저장
                    bool has_new_objects = false;
                    {
                        std::lock_guard<std::mutex> lock(devices_mutex_);
                        auto& existing_objects = discovered_objects_[device_id];
                        
                        for (const auto& obj : objects) {
                            // 기존에 없는 객체인지 확인
                            bool is_new = std::find_if(existing_objects.begin(), existing_objects.end(),
                                [&obj](const auto& existing) {
                                    return existing.object_type == obj.object_type && 
                                           existing.object_instance == obj.object_instance;
                                }) == existing_objects.end();
                            
                            if (is_new) {
                                existing_objects.push_back(obj);
                                has_new_objects = true;
                                
                                // 객체 발견 콜백 호출
                                if (on_object_discovered_) {
                                    try {
                                        on_object_discovered_(device_id, obj);
                                    } catch (const std::exception& e) {
                                        LogMessage(PulseOne::LogLevel::ERROR, "Exception in object discovered callback: " + std::string(e.what()));
                                    }
                                }
                            }
                        }
                    }
                    
                    // 객체들의 현재 값 읽기
                    for (const auto& obj : objects) {
                        try {
                            std::string object_id = CreateObjectId(device_id, obj);
                            
                            // 기존 값과 비교하기 위해 현재 값 조회
                            auto it = current_values_.find(object_id);
                            
                            // 새로운 값 읽기 (간단한 주소 생성 후 읽기)
                            std::string read_address = std::to_string(device_id) + "." + 
                                                     BACnetObjectInfo::ObjectTypeToString(obj.object_type) + "." +
                                                     std::to_string(obj.object_instance) + ".PV";
                            
                            PulseOne::Structs::DataValue new_value = bacnet_driver_->ReadData(read_address);
                            
                            if (!new_value.valueless_by_exception()) { // variant가 유효한지 확인
                                PulseOne::Structs::TimestampedValue timestamped_value;
                                timestamped_value.value = new_value;
                                timestamped_value.timestamp = std::chrono::system_clock::now();
                                timestamped_value.quality = PulseOne::Structs::DataQuality::GOOD;
                                
                                // 값이 변경되었는지 확인
                                bool value_changed = false;
                                if (it == current_values_.end()) {
                                    // 새로운 값
                                    current_values_[object_id] = timestamped_value;
                                    value_changed = true;
                                } else {
                                    // 기존 값과 비교 (간단한 비교)
                                    if (it->second.value.index() != new_value.index()) {
                                        current_values_[object_id] = timestamped_value;
                                        value_changed = true;
                                    }
                                }
                                
                                // 값 변경 콜백 호출
                                if (value_changed && on_value_changed_) {
                                    try {
                                        on_value_changed_(object_id, timestamped_value);
                                    } catch (const std::exception& e) {
                                        LogMessage(PulseOne::LogLevel::ERROR, "Exception in value changed callback: " + std::string(e.what()));
                                    }
                                }
                            }
                        } catch (const std::exception& e) {
                            LogMessage(PulseOne::LogLevel::WARN, "Failed to read object value: " + std::string(e.what()));
                        }
                    }
                }
                
            } catch (const std::exception& e) {
                LogMessage(PulseOne::LogLevel::WARN, "Failed to poll device " + std::to_string(device_id) + ": " + std::string(e.what()));
            }
        }
        
        // 통계 업데이트
        worker_stats_.read_operations += total_objects;
        
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL,
                  "✅ Polling completed. Processed " + std::to_string(total_objects) + 
                  " objects from " + std::to_string(device_ids.size()) + " devices");
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in PerformPolling: " + std::string(e.what()));
        return false;
    }
}

void BACnetWorker::UpdateWorkerStats(const std::string& operation, bool success) {
    try {
        if (operation == "discovery") {
            worker_stats_.discovery_attempts++;
        } else if (operation == "polling") {
            worker_stats_.polling_cycles++;
        } else if (operation == "data_processing" || operation == "packet_received") {
            worker_stats_.read_operations++;
        }
        
        if (!success) {
            worker_stats_.failed_operations++;
        }
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "UpdateWorkerStats failed: " + std::string(e.what()));
    }
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
// 발견된 정보 조회
// =============================================================================

std::vector<BACnetDeviceInfo> BACnetWorker::GetDiscoveredDevices() const {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    std::vector<BACnetDeviceInfo> devices;
    for (const auto& [device_id, device_info] : discovered_devices_) {
        devices.push_back(device_info);
    }
    
    return devices;
}

std::vector<BACnetObjectInfo> BACnetWorker::GetDiscoveredObjects(uint32_t device_id) const {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    auto it = discovered_objects_.find(device_id);
    if (it != discovered_objects_.end()) {
        return it->second;
    }
    
    return {};
}

std::string BACnetWorker::CreateObjectId(uint32_t device_id, const BACnetObjectInfo& object_info) const {
    return std::to_string(device_id) + "." + 
           object_info.ObjectTypeToString(object_info.object_type) + "." + 
           std::to_string(object_info.object_instance);
}

} // namespace Workers
} // namespace PulseOne