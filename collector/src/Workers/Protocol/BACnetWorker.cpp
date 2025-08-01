
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
        LogMessage(PulseOne::LogLevel::INFO, "Parsing BACnet worker configuration...");
        
        const auto& device_info = GetDeviceInfo();
        
        // 기본값 설정
        worker_config_.device_id = 260001;
        worker_config_.port = 47808;
        worker_config_.discovery_interval = 30000;  // 30초
        worker_config_.polling_interval = 5000;     // 5초
        worker_config_.auto_discovery = true;
        worker_config_.apdu_timeout = 6000;
        worker_config_.apdu_retries = 3;
        worker_config_.max_retries = 3;
        worker_config_.target_port = 47808;
        worker_config_.device_instance = 260001;
        worker_config_.max_apdu_length = 1476;
        
        // endpoint에서 IP와 포트 파싱 (ModbusTcp 패턴)
        if (!device_info.endpoint.empty()) {
            std::string endpoint = device_info.endpoint;
            
            // "bacnet://192.168.1.100:47808/device_id=12345" 형식 파싱
            size_t protocol_pos = endpoint.find("://");
            if (protocol_pos != std::string::npos) {
                std::string addr_part = endpoint.substr(protocol_pos + 3);
                size_t colon_pos = addr_part.find(':');
                
                if (colon_pos != std::string::npos) {
                    worker_config_.target_ip = addr_part.substr(0, colon_pos);
                    
                    // 포트 추출
                    size_t slash_pos = addr_part.find('/', colon_pos);
                    if (slash_pos != std::string::npos) {
                        std::string port_str = addr_part.substr(colon_pos + 1, slash_pos - colon_pos - 1);
                        try {
                            worker_config_.target_port = static_cast<uint16_t>(std::stoi(port_str));
                            worker_config_.port = worker_config_.target_port;
                        } catch (const std::exception& e) {
                            LogMessage(PulseOne::LogLevel::WARN, "Invalid port in endpoint: " + port_str);
                        }
                        
                        // device_id 파라미터 추출
                        std::string params = addr_part.substr(slash_pos + 1);
                        size_t device_id_pos = params.find("device_id=");
                        if (device_id_pos != std::string::npos) {
                            std::string device_id_str = params.substr(device_id_pos + 10);
                            size_t param_end = device_id_str.find('&');
                            if (param_end != std::string::npos) {
                                device_id_str = device_id_str.substr(0, param_end);
                            }
                            
                            try {
                                worker_config_.device_id = static_cast<uint32_t>(std::stoul(device_id_str));
                                worker_config_.device_instance = worker_config_.device_id;
                            } catch (const std::exception& e) {
                                LogMessage(PulseOne::LogLevel::WARN, "Invalid device_id: " + device_id_str);
                            }
                        }
                    }
                }
            }
        }
        
        // connection_string에서 추가 설정 파싱 (key=value 형태)
        if (!device_info.connection_string.empty()) {
            std::string conn_str = device_info.connection_string;
            
            // discovery_interval 파싱
            size_t discovery_pos = conn_str.find("discovery_interval=");
            if (discovery_pos != std::string::npos) {
                size_t start = discovery_pos + 19;
                size_t end = std::min({
                    conn_str.find(',', start),
                    conn_str.find(';', start),
                    conn_str.find('&', start),
                    conn_str.length()
                });
                
                std::string interval_str = conn_str.substr(start, end - start);
                try {
                    worker_config_.discovery_interval = static_cast<uint32_t>(std::stoul(interval_str));
                } catch (const std::exception& e) {
                    LogMessage(PulseOne::LogLevel::WARN, "Invalid discovery_interval: " + interval_str);
                }
            }
            
            // polling_interval 파싱
            size_t polling_pos = conn_str.find("polling_interval=");
            if (polling_pos != std::string::npos) {
                size_t start = polling_pos + 17;
                size_t end = std::min({
                    conn_str.find(',', start),
                    conn_str.find(';', start),
                    conn_str.find('&', start),
                    conn_str.length()
                });
                
                std::string interval_str = conn_str.substr(start, end - start);
                try {
                    worker_config_.polling_interval = static_cast<uint32_t>(std::stoul(interval_str));
                } catch (const std::exception& e) {
                    LogMessage(PulseOne::LogLevel::WARN, "Invalid polling_interval: " + interval_str);
                }
            }
        }
        
        LogMessage(PulseOne::LogLevel::INFO, 
                  "✅ BACnet config parsed - Device ID: " + std::to_string(worker_config_.device_id) +
                  ", Target: " + worker_config_.target_ip + ":" + std::to_string(worker_config_.target_port) +
                  ", Discovery: " + std::to_string(worker_config_.discovery_interval) + "ms" +
                  ", Polling: " + std::to_string(worker_config_.polling_interval) + "ms");
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "ParseBACnetWorkerConfig failed: " + std::string(e.what()));
        return false;
    }
}

// 2. 드라이버 설정 생성 메서드 (ModbusTcpWorker 패턴)
PulseOne::Structs::DriverConfig BACnetWorker::CreateDriverConfig() {
    PulseOne::Structs::DriverConfig config;
    
    try {
        // 기본 설정
        config.device_id = std::to_string(worker_config_.device_id);
        config.protocol = ProtocolType::BACNET_IP;
        config.timeout_ms = worker_config_.apdu_timeout;
        config.retry_count = static_cast<int>(worker_config_.apdu_retries);
        config.polling_interval_ms = static_cast<int>(worker_config_.polling_interval);
        
        // BACnet 특화 설정을 connection_params에 JSON 형태로 저장
        std::stringstream ss;
        ss << "{"
           << "\"target_ip\":\"" << worker_config_.target_ip << "\","
           << "\"target_port\":" << worker_config_.target_port << ","
           << "\"device_id\":" << worker_config_.device_id << ","
           << "\"device_instance\":" << worker_config_.device_instance << ","
           << "\"max_apdu_length\":" << worker_config_.max_apdu_length << ","
           << "\"discovery_interval\":" << worker_config_.discovery_interval << ","  
           << "\"polling_interval\":" << worker_config_.polling_interval << ","
           << "\"auto_discovery\":" << (worker_config_.auto_discovery ? "true" : "false")
           << "}";
        
        config.properties["target_ip"] = worker_config_.target_ip;
        config.properties["target_port"] = std::to_string(worker_config_.target_port);

        
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "BACnet driver config created");
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "CreateDriverConfig failed: " + std::string(e.what()));
    }
    
    return config;
}

// 3. Discovery 스레드 함수 (MQTTWorker 패턴, DB 저장 없이)
void BACnetWorker::DiscoveryThreadFunction() {
    LogMessage(PulseOne::LogLevel::INFO, "🔍 BACnet Discovery thread started");
    
    while (threads_running_.load()) {
        try {
            // 연결 상태 확인
            if (!CheckProtocolConnection()) {
                LogMessage(PulseOne::LogLevel::WARN, "BACnet not connected, skipping discovery");
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }
            
            // Discovery 수행 (순수 스캔만, DB 저장 없음)
            if (PerformDiscovery()) {
                worker_stats_.discovery_attempts++;
                LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Discovery cycle completed successfully");
            } else {
                LogMessage(PulseOne::LogLevel::WARN, "Discovery cycle failed");
            }
            
            // discovery_interval 만큼 대기
            std::this_thread::sleep_for(std::chrono::milliseconds(worker_config_.discovery_interval));
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Exception in discovery thread: " + std::string(e.what()));
            // 에러 시 더 긴 대기 시간
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "🔍 BACnet Discovery thread stopped");
}

// 4. Polling 스레드 함수 (MQTTWorker 패턴, DB 저장 없이)  
void BACnetWorker::PollingThreadFunction() {
    LogMessage(PulseOne::LogLevel::INFO, "📊 BACnet Polling thread started");
    
    while (threads_running_.load()) {
        try {
            // 연결 상태 확인
            if (!CheckProtocolConnection()) {
                LogMessage(PulseOne::LogLevel::WARN, "BACnet not connected, skipping polling");
                std::this_thread::sleep_for(std::chrono::seconds(5));
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
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }
            
            // Polling 수행 (순수 데이터 수집만, DB 저장 없음)
            if (PerformPolling()) {
                worker_stats_.polling_cycles++;
                LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                          "Polling cycle completed for " + std::to_string(device_count) + " devices");
            } else {
                LogMessage(PulseOne::LogLevel::WARN, "Polling cycle failed");
            }
            
            // polling_interval 만큼 대기
            std::this_thread::sleep_for(std::chrono::milliseconds(worker_config_.polling_interval));
            
        } catch (const std::exception& e) {
            LogMessage(PulseOne::LogLevel::ERROR, "Exception in polling thread: " + std::string(e.what()));
            // 에러 시 더 짧은 대기 시간 (폴링은 더 자주 시도)
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    LogMessage(PulseOne::LogLevel::INFO, "📊 BACnet Polling thread stopped");
}

// =============================================================================
// 핵심 비즈니스 로직 (순수 기능만, DB 저장 없음)
// =============================================================================

// 5. Discovery 비즈니스 로직 (메모리 저장만)
bool BACnetWorker::PerformDiscovery() {
    try {
        if (!bacnet_driver_) {
            LogMessage(PulseOne::LogLevel::ERROR, "BACnet driver not available");
            return false;
        }
        
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "🔍 Performing BACnet device discovery...");
        
        // Who-Is 요청 전송 (BACnetDriver의 더미 구현 활용)
        if (!bacnet_driver_->SendWhoIs()) {
            LogMessage(PulseOne::LogLevel::ERROR, "Failed to send Who-Is request");
            return false;
        }
        
        // 발견된 디바이스 조회 (더미에서 5개 디바이스 생성됨)
        auto driver_devices = bacnet_driver_->GetDiscoveredDevices();
        
        // 🔥 핵심: 메모리에만 저장, DB 저장 절대 없음
        {
            std::lock_guard<std::mutex> lock(devices_mutex_);
            size_t new_devices = 0;
            
            for (const auto& device : driver_devices) {
                if (discovered_devices_.find(device.device_id) == discovered_devices_.end()) {
                    discovered_devices_[device.device_id] = device;
                    new_devices++;
                    
                    LogMessage(PulseOne::LogLevel::INFO, 
                              "🆕 New BACnet device discovered: " + device.device_name + 
                              " (ID: " + std::to_string(device.device_id) + 
                              ", IP: " + device.ip_address + ")");
                    
                    discovered_devices_[device.device_id] = device;
                    new_devices++;
                    
                    LogMessage(PulseOne::LogLevel::INFO, 
                              "🆕 New BACnet device discovered: " + device.device_name + 
                              " (ID: " + std::to_string(device.device_id) + 
                              ", IP: " + device.ip_address + ")");
                    
                    // 🔥 콜백 호출 추가
                    if (on_device_discovered_) {
                        try {
                            on_device_discovered_(device);
                        } catch (const std::exception& e) {
                            LogMessage(PulseOne::LogLevel::ERROR, 
                                      "Device discovered callback failed: " + std::string(e.what()));
                        }
                    }
                }
            }
            
            if (new_devices > 0) {
                worker_stats_.devices_discovered += new_devices;
                LogMessage(PulseOne::LogLevel::INFO, 
                          "✅ Discovery completed. Total devices: " + std::to_string(discovered_devices_.size()) +
                          ", New devices: " + std::to_string(new_devices));
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in PerformDiscovery: " + std::string(e.what()));
        return false;
    }
}

// 6. Polling 비즈니스 로직 (메모리 저장만)
bool BACnetWorker::PerformPolling() {
    try {
        if (!bacnet_driver_) {
            LogMessage(PulseOne::LogLevel::ERROR, "BACnet driver not available for polling");
            return false;
        }
        
        std::vector<uint32_t> device_ids;
        
        // 발견된 디바이스 ID 목록 가져오기
        {
            std::lock_guard<std::mutex> lock(devices_mutex_);
            for (const auto& [device_id, device_info] : discovered_devices_) {
                device_ids.push_back(device_id);
            }
        }
        
        if (device_ids.empty()) {
            return true; // 디바이스가 없어도 성공으로 간주
        }
        
        LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                  "📊 Polling " + std::to_string(device_ids.size()) + " BACnet devices...");
        
        size_t total_objects = 0;
        size_t successful_reads = 0;
        
        // 각 디바이스별로 객체 조회 및 데이터 읽기
        for (uint32_t device_id : device_ids) {
            try {
                // 디바이스의 객체 목록 조회 (더미 구현에서 가상 객체들 반환)
                auto objects = bacnet_driver_->GetDeviceObjects(device_id);
                total_objects += objects.size();
                
                if (!objects.empty()) {
                    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                              "Device " + std::to_string(device_id) + " has " + 
                              std::to_string(objects.size()) + " objects");
                    
                    // 🔥 핵심: 메모리에만 저장, DB 저장 절대 없음
                    for (size_t i = 0; i < objects.size(); ++i) {
                        // 객체 정보를 내부 맵에 저장 (메모리만)
                        // TODO: 필요시 discovered_objects_ 맵 추가
                        
                        successful_reads++;
                        
                        auto objects = bacnet_driver_->GetDeviceObjects(device_id);
                        total_objects += objects.size();
                        successful_reads += objects.size();
                        
                        if (!objects.empty()) {
                            LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                                    "Device " + std::to_string(device_id) + " has " + 
                                    std::to_string(objects.size()) + " objects");
                            
                            // 🔥 객체 저장 및 콜백 호출
                            {
                                std::lock_guard<std::mutex> lock(devices_mutex_);
                                
                                // 기존 객체와 비교하여 새로운 객체만 콜백 호출
                                auto& existing_objects = discovered_objects_[device_id];
                                bool objects_changed = (existing_objects.size() != objects.size());
                                
                                if (objects_changed) {
                                    existing_objects = objects;
                                    
                                    // 객체 발견 콜백 호출
                                    if (on_object_discovered_) {
                                        try {
                                            on_object_discovered_(device_id, objects);
                                        } catch (const std::exception& e) {
                                            LogMessage(PulseOne::LogLevel::ERROR, 
                                                    "Object discovered callback failed: " + std::string(e.what()));
                                        }
                                    }
                                }
                                
                                // 각 객체의 값 변경 확인 및 콜백 호출
                                for (const auto& obj : objects) {
                                    std::string object_id = CreateObjectId(device_id, obj);
                                    
                                    // 값이 변경된 경우 콜백 호출
                                    auto it = current_values_.find(object_id);
                                    bool value_changed = false;
                                    
                                    if (it == current_values_.end()) {
                                        // 새로운 객체
                                        value_changed = true;
                                    } else {
                                        // 기존 객체 - 값 비교 (간단히 타임스탬프로 비교)
                                        value_changed = (obj.timestamp != it->second.timestamp);
                                    }
                                    
                                    if (value_changed) {
                                        // TimestampedValue 생성 (BACnet 값을 DataValue로 변환)
                                        PulseOne::TimestampedValue timestamped_value;
                                        timestamped_value.timestamp = obj.timestamp;
                                        timestamped_value.quality = obj.quality;
                                        
                                        // BACnet 값을 DataValue로 간단 변환
                                        switch (obj.value.tag) {
                                            case BACNET_APPLICATION_TAG_BOOLEAN:
                                                timestamped_value.value = PulseOne::Structs::DataValue(static_cast<bool>(obj.value.type.Boolean));
                                                break;
                                            case BACNET_APPLICATION_TAG_UNSIGNED_INT:
                                                timestamped_value.value = PulseOne::Structs::DataValue(static_cast<uint32_t>(obj.value.type.Unsigned_Int));
                                                break;
                                            case BACNET_APPLICATION_TAG_SIGNED_INT:
                                                timestamped_value.value = PulseOne::Structs::DataValue(static_cast<int32_t>(obj.value.type.Signed_Int));
                                                break;
                                            case BACNET_APPLICATION_TAG_REAL:
                                                timestamped_value.value = PulseOne::Structs::DataValue(static_cast<float>(obj.value.type.Real));
                                                break;
                                            default:
                                                timestamped_value.value = PulseOne::Structs::DataValue(std::string("BACnet_Value"));
                                                break;
                                        }
                                        
                                        current_values_[object_id] = timestamped_value;
                                        
                                        // 값 변경 콜백 호출
                                        if (on_value_changed_) {
                                            try {
                                                on_value_changed_(object_id, timestamped_value);
                                            } catch (const std::exception& e) {
                                                LogMessage(PulseOne::LogLevel::ERROR, 
                                                        "Value changed callback failed: " + std::string(e.what()));
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                
            } catch (const std::exception& e) {
                LogMessage(PulseOne::LogLevel::WARN, 
                          "Failed to poll device " + std::to_string(device_id) + ": " + e.what());
            }
        }
        
        // 통계 업데이트
        worker_stats_.read_operations += total_objects;
        worker_stats_.successful_reads += successful_reads;
        
        if (total_objects > 0) {
            LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, 
                      "✅ Polling completed. Objects: " + std::to_string(total_objects) +
                      ", Successful reads: " + std::to_string(successful_reads));
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "Exception in PerformPolling: " + std::string(e.what()));
        return false;
    }
}

// 7. 통계 업데이트 헬퍼 메서드
void BACnetWorker::UpdateWorkerStats(const std::string& operation, bool success) {
    try {
        if (operation == "discovery") {
            worker_stats_.discovery_attempts++;
        } else if (operation == "polling") {
            worker_stats_.polling_cycles++;
        } else if (operation == "read") {
            worker_stats_.read_operations++;
            if (success) {
                worker_stats_.successful_reads++;
            } else {
                worker_stats_.failed_operations++;
            }
        }
        
    } catch (const std::exception& e) {
        LogMessage(PulseOne::LogLevel::ERROR, "UpdateWorkerStats failed: " + std::string(e.what()));
    }
}


// =============================================================================
// 콜백 인터페이스 구현
// =============================================================================

void BACnetWorker::SetDeviceDiscoveredCallback(DeviceDiscoveredCallback callback) {
    on_device_discovered_ = callback;
    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Device discovery callback registered");
}

void BACnetWorker::SetObjectDiscoveredCallback(ObjectDiscoveredCallback callback) {
    on_object_discovered_ = callback;
    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Object discovery callback registered");
}

void BACnetWorker::SetValueChangedCallback(ValueChangedCallback callback) {
    on_value_changed_ = callback;
    LogMessage(PulseOne::LogLevel::DEBUG_LEVEL, "Value changed callback registered");
}

std::vector<Drivers::BACnetDeviceInfo> BACnetWorker::GetDiscoveredDevices() const {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    std::vector<Drivers::BACnetDeviceInfo> devices;
    for (const auto& [device_id, device_info] : discovered_devices_) {
        devices.push_back(device_info);
    }
    
    return devices;
}

std::vector<Drivers::BACnetObjectInfo> BACnetWorker::GetDiscoveredObjects(uint32_t device_id) const {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    auto it = discovered_objects_.find(device_id);
    if (it != discovered_objects_.end()) {
        return it->second;
    }
    
    return {};
}

std::string BACnetWorker::CreateObjectId(uint32_t device_id, const Drivers::BACnetObjectInfo& object_info) const {
    return std::to_string(device_id) + ":" + 
           std::to_string(static_cast<int>(object_info.object_type)) + ":" + 
           std::to_string(object_info.object_instance);
}

} // namespace Workers
} // namespace PulseOne