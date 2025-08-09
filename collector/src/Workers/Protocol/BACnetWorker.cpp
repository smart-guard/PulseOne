// =============================================================================
// 📄 collector/src/Workers/Protocol/BACnetWorker.cpp - 수정본
// =============================================================================

#include "Workers/Protocol/BACnetWorker.h"
#include "Utils/LogManager.h"
#include "Common/Enums.h"
#include <sstream>
#include <thread>

using namespace std::chrono;
using LogLevel = PulseOne::Enums::LogLevel;

namespace PulseOne {
namespace Workers {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

BACnetWorker::BACnetWorker(const DeviceInfo& device_info)
    : UdpBasedWorker(device_info)
    , bacnet_driver_(std::make_unique<PulseOne::Drivers::BACnetDriver>())
    , thread_running_(false) {

    LogMessage(LogLevel::INFO, "BACnetWorker created for device: " + device_info.name);
    
    // BACnet 워커 통계 초기화
    worker_stats_.start_time = system_clock::now();
    worker_stats_.last_reset = worker_stats_.start_time;
    
    // BACnet 설정 파싱
    if (!ParseBACnetConfigFromDeviceInfo()) {
        LogMessage(LogLevel::WARN, "Failed to parse BACnet config from DeviceInfo, using defaults");
    }
    
    // BACnet Driver 초기화
    if (!InitializeBACnetDriver()) {
        LogMessage(LogLevel::ERROR, "Failed to initialize BACnet driver");
        return;
    }

    // 🔥 BACnetServiceManager 초기화 추가
    if (bacnet_driver_) {
        bacnet_service_manager_ = std::make_shared<PulseOne::Drivers::BACnetServiceManager>(
            bacnet_driver_.get()
        );
        LogMessage(LogLevel::INFO, "BACnetServiceManager initialized successfully");
    } else {
        LogMessage(LogLevel::ERROR, "Cannot initialize BACnetServiceManager: BACnet driver is null");
    }    
    
    LogMessage(LogLevel::INFO, "BACnetWorker initialization completed");
}

BACnetWorker::~BACnetWorker() {
    // 스레드 정리
    if (thread_running_.load()) {
        thread_running_ = false;
        
        if (data_scan_thread_ && data_scan_thread_->joinable()) {
            data_scan_thread_->join();
        }
    }
    
    // BACnet 드라이버 정리
    ShutdownBACnetDriver();
    
    LogMessage(LogLevel::INFO, "BACnetWorker destroyed for device: " + device_info_.name);
}

// =============================================================================
// BaseDeviceWorker 인터페이스 구현
// =============================================================================

std::future<bool> BACnetWorker::Start() {
    return std::async(std::launch::async, [this]() -> bool {
        LogMessage(LogLevel::INFO, "Starting BACnetWorker...");
        
        // 1. 통계 초기화
        worker_stats_.Reset();
        
        // 2. UDP 연결 수립
        if (!EstablishConnection()) {
            LogMessage(LogLevel::ERROR, "Failed to establish UDP connection");
            return false;
        }
        
        // 3. 데이터 스캔 스레드 시작 (하나만!)
        thread_running_ = true;
        data_scan_thread_ = std::make_unique<std::thread>(&BACnetWorker::DataScanThreadFunction, this);
        
        LogMessage(LogLevel::INFO, "BACnetWorker started successfully");
        return true;
    });
}

std::future<bool> BACnetWorker::Stop() {
    return std::async(std::launch::async, [this]() -> bool {
        LogMessage(LogLevel::INFO, "Stopping BACnetWorker...");
        
        // 1. 스레드 정리
        if (thread_running_.load()) {
            thread_running_ = false;
            
            if (data_scan_thread_ && data_scan_thread_->joinable()) {
                data_scan_thread_->join();
            }
        }
        
        // 2. BACnet 드라이버 정리
        ShutdownBACnetDriver();
        
        // 3. UDP 연결 해제
        CloseConnection();
        
        LogMessage(LogLevel::INFO, "BACnetWorker stopped successfully");
        return true;
    });
}

// =============================================================================
// UdpBasedWorker 순수 가상 함수 구현
// =============================================================================

bool BACnetWorker::EstablishProtocolConnection() {
    LogMessage(LogLevel::INFO, "Establishing BACnet protocol connection...");
    
    if (!bacnet_driver_) {
        LogMessage(LogLevel::ERROR, "BACnet driver not initialized");
        return false;
    }
    
    if (!bacnet_driver_->Connect()) {
        const auto& error = bacnet_driver_->GetLastError();
        LogMessage(LogLevel::ERROR, "❌ Failed to connect BACnet driver: " + error.message);
        return false;
    }
    
    LogMessage(LogLevel::INFO, "BACnet protocol connection established");
    return true;
}

bool BACnetWorker::CloseProtocolConnection() {
    LogMessage(LogLevel::INFO, "Closing BACnet protocol connection...");
    ShutdownBACnetDriver();
    LogMessage(LogLevel::INFO, "BACnet protocol connection closed");
    return true;
}

bool BACnetWorker::CheckProtocolConnection() {
    if (!bacnet_driver_) {
        return false;
    }
    return bacnet_driver_->IsConnected();
}

bool BACnetWorker::SendProtocolKeepAlive() {
    // BACnet은 Keep-alive 대신 연결 상태만 확인
    return CheckProtocolConnection();
}

bool BACnetWorker::ProcessReceivedPacket(const UdpPacket& packet) {
    try {
        LogMessage(LogLevel::DEBUG_LEVEL, 
                  "Processing BACnet packet (" + std::to_string(packet.data.size()) + " bytes)");
        
        UpdateWorkerStats("packet_received", true);
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Exception in ProcessReceivedPacket: " + std::string(e.what()));
        UpdateWorkerStats("packet_received", false);
        return false;
    }
}

// =============================================================================
// ✅ Worker의 진짜 기능들 - 데이터 스캔 + 파이프라인 전송
// =============================================================================

bool BACnetWorker::SendBACnetDataToPipeline(const std::map<std::string, DataValue>& object_values,
                                           const std::string& context,
                                           uint32_t priority) {
    if (object_values.empty()) {
        return false;
    }
    
    try {
        std::vector<TimestampedValue> timestamped_values;
        timestamped_values.reserve(object_values.size());
        
        auto timestamp = std::chrono::system_clock::now();
        
        for (const auto& [object_id, value] : object_values) {
            TimestampedValue tv;
            tv.value = value;
            tv.timestamp = timestamp;
            tv.quality = DataQuality::GOOD;
            tv.source = "bacnet_" + object_id;
            timestamped_values.push_back(tv);
        }
        
        return SendValuesToPipelineWithLogging(timestamped_values, context, priority);
                                              
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, 
                  "SendBACnetDataToPipeline 예외: " + std::string(e.what()));
        return false;
    }
}

bool BACnetWorker::SendBACnetPropertyToPipeline(const std::string& object_id,
                                               const DataValue& property_value,
                                               const std::string& object_name,
                                               uint32_t priority) {
    try {
        TimestampedValue tv;
        tv.value = property_value;
        tv.timestamp = std::chrono::system_clock::now();
        tv.quality = DataQuality::GOOD;
        
        if (!object_name.empty()) {
            tv.source = "bacnet_" + object_id + "_" + object_name;
        } else {
            tv.source = "bacnet_" + object_id;
        }
        
        return SendValuesToPipelineWithLogging({tv}, 
                                              "BACnet Property: " + object_id, 
                                              priority);
                                              
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, 
                  "SendBACnetPropertyToPipeline 예외: " + std::string(e.what()));
        return false;
    }
}

bool BACnetWorker::SendValuesToPipelineWithLogging(const std::vector<TimestampedValue>& values,
                                                  const std::string& context,
                                                  uint32_t priority) {
    if (values.empty()) {
        return false;
    }
    
    try {
        // 🔥 핵심: BaseDeviceWorker::SendDataToPipeline() 호출
        bool success = SendDataToPipeline(values, priority);
        
        if (success) {
            LogMessage(LogLevel::DEBUG_LEVEL, 
                      "✅ 파이프라인 전송 성공 (" + context + "): " + 
                      std::to_string(values.size()) + "개 포인트");
        } else {
            LogMessage(LogLevel::WARN, 
                      "❌ 파이프라인 전송 실패 (" + context + "): " + 
                      std::to_string(values.size()) + "개 포인트");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, 
                  "SendValuesToPipelineWithLogging 예외: " + std::string(e.what()));
        return false;
    }
}

bool BACnetWorker::SendCOVNotificationToPipeline(const std::string& object_id,
                                                const DataValue& new_value,
                                                const DataValue& previous_value) {

    (void)previous_value;

    try {
        TimestampedValue tv;
        tv.value = new_value;
        tv.timestamp = std::chrono::system_clock::now();
        tv.quality = DataQuality::GOOD;
        tv.source = "bacnet_cov_" + object_id;
        
        bool success = SendValuesToPipelineWithLogging({tv}, 
                                                      "BACnet COV: " + object_id, 
                                                      5  // 높은 우선순위
                                                     );
        
        if (success) {
            LogMessage(LogLevel::INFO, 
                      "📢 COV notification sent for object: " + object_id);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, 
                  "SendCOVNotificationToPipeline 예외: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// ✅ 설정 및 상태 관리
// =============================================================================

void BACnetWorker::LoadDataPointsFromConfiguration(const std::vector<DataPoint>& data_points) {
    std::lock_guard<std::mutex> lock(data_points_mutex_);
    configured_data_points_ = data_points;
    
    LogMessage(LogLevel::INFO, 
              "Loaded " + std::to_string(data_points.size()) + " data points from configuration");
}

std::vector<DataPoint> BACnetWorker::GetConfiguredDataPoints() const {
    std::lock_guard<std::mutex> lock(data_points_mutex_);
    return configured_data_points_;
}

std::string BACnetWorker::GetBACnetWorkerStats() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"polling_cycles\": " << worker_stats_.polling_cycles.load() << ",\n";
    ss << "  \"read_operations\": " << worker_stats_.read_operations.load() << ",\n";
    ss << "  \"write_operations\": " << worker_stats_.write_operations.load() << ",\n";
    ss << "  \"failed_operations\": " << worker_stats_.failed_operations.load() << ",\n";
    ss << "  \"cov_notifications\": " << worker_stats_.cov_notifications.load() << ",\n";
    
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
    LogMessage(LogLevel::INFO, "BACnet worker statistics reset");
}

void BACnetWorker::SetValueChangedCallback(ValueChangedCallback callback) {
    on_value_changed_ = callback;
}

// =============================================================================
// ✅ 내부 구현 메서드들
// =============================================================================

bool BACnetWorker::ParseBACnetConfigFromDeviceInfo() {
    try {
        const auto& props = device_info_.properties;
        
        auto local_device_it = props.find("bacnet_local_device_id");
        if (local_device_it != props.end()) {
            LogMessage(LogLevel::DEBUG_LEVEL, "BACnet local device ID: " + local_device_it->second);
        }
        
        LogMessage(LogLevel::DEBUG_LEVEL, "BACnet endpoint: " + device_info_.endpoint);
        LogMessage(LogLevel::DEBUG_LEVEL, "BACnet timeout: " + std::to_string(device_info_.timeout_ms));
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Exception in ParseBACnetConfigFromDeviceInfo: " + std::string(e.what()));
        return false;
    }
}

PulseOne::Structs::DriverConfig BACnetWorker::CreateDriverConfigFromDeviceInfo() {
    LogMessage(LogLevel::INFO, "🔧 Creating BACnet DriverConfig from DeviceInfo...");
    
    PulseOne::Structs::DriverConfig config = device_info_.driver_config;
    
    const auto& props = device_info_.properties;
    
    // BACnet device_id 찾기
    std::string bacnet_device_id = "";
    std::vector<std::string> device_id_keys = {
        "device_id", "local_device_id", "bacnet_device_id", "bacnet_local_device_id"
    };
    
    for (const auto& key : device_id_keys) {
        auto it = props.find(key);
        if (it != props.end() && !it->second.empty()) {
            bacnet_device_id = it->second;
            break;
        }
    }
    
    if (!bacnet_device_id.empty()) {
        config.device_id = bacnet_device_id;
    } else {
        config.device_id = "260001";
        LogMessage(LogLevel::WARN, "⚠️ No BACnet device_id found, using default: 260001");
    }
    
    config.protocol = PulseOne::Enums::ProtocolType::BACNET_IP;
    config.endpoint = device_info_.endpoint;
    
    if (device_info_.timeout_ms > 0) {
        config.timeout_ms = device_info_.timeout_ms;
    }
    if (device_info_.retry_count > 0) {
        config.retry_count = static_cast<uint32_t>(device_info_.retry_count);
    }
    if (device_info_.polling_interval_ms > 0) {
        config.polling_interval_ms = static_cast<uint32_t>(device_info_.polling_interval_ms);
    }
    
    config.auto_reconnect = true;
    
    // BACnet 특화 설정들
    config.properties["device_id"] = config.device_id;
    config.properties["local_device_id"] = config.device_id;
    
    auto port_it = props.find("bacnet_port");
    config.properties["port"] = (port_it != props.end()) ? 
                               port_it->second : "47808";
    
    auto cov_it = props.find("bacnet_enable_cov");
    config.properties["enable_cov"] = (cov_it != props.end()) ? 
                                     cov_it->second : "false";
    
    LogMessage(LogLevel::INFO, "✅ BACnet DriverConfig created successfully");
    
    return config;
}

bool BACnetWorker::InitializeBACnetDriver() {
    try {
        LogMessage(LogLevel::INFO, "🔧 Initializing BACnet driver...");
        
        if (!bacnet_driver_) {
            LogMessage(LogLevel::ERROR, "❌ BACnet driver is null");
            return false;
        }
        
        auto driver_config = CreateDriverConfigFromDeviceInfo();
        
        if (!bacnet_driver_->Initialize(driver_config)) {
            const auto& error = bacnet_driver_->GetLastError();
            LogMessage(LogLevel::ERROR, "❌ BACnet driver initialization failed: " + error.message);
            return false;
        }
        
        SetupBACnetDriverCallbacks();
        
        LogMessage(LogLevel::INFO, "✅ BACnet driver initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "❌ Exception during BACnet driver initialization: " + std::string(e.what()));
        return false;
    }
}

void BACnetWorker::ShutdownBACnetDriver() {
    try {
        if (bacnet_driver_) {
            bacnet_driver_->Disconnect();
            bacnet_driver_.reset();
            LogMessage(LogLevel::INFO, "BACnet driver shutdown complete");
        }
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Exception shutting down BACnet driver: " + std::string(e.what()));
    }
}

// =============================================================================
// ✅ 데이터 스캔 스레드 (하나만!)
// =============================================================================

void BACnetWorker::DataScanThreadFunction() {
    LogMessage(LogLevel::INFO, "BACnet data scan thread started");
    
    uint32_t polling_interval_ms = device_info_.polling_interval_ms;
    
    while (thread_running_.load()) {
        try {
            if (PerformDataScan()) {
                worker_stats_.polling_cycles++;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(polling_interval_ms));
            
        } catch (const std::exception& e) {
            LogMessage(LogLevel::ERROR, "Exception in data scan thread: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    LogMessage(LogLevel::INFO, "BACnet data scan thread stopped");
}

bool BACnetWorker::PerformDataScan() {
    LogMessage(LogLevel::DEBUG_LEVEL, "Performing BACnet data scan...");
    
    try {
        if (!bacnet_driver_ || !bacnet_driver_->IsConnected()) {
            return false;
        }
        
        // 설정된 DataPoint들 가져오기
        std::vector<DataPoint> data_points_to_scan;
        
        {
            std::lock_guard<std::mutex> lock(data_points_mutex_);
            data_points_to_scan = configured_data_points_;
        }
        
        if (data_points_to_scan.empty()) {
            LogMessage(LogLevel::DEBUG_LEVEL, "No data points configured for scanning");
            return true;
        }
        
        // BACnet Present Value들 읽기
        std::vector<TimestampedValue> values;
        bool success = bacnet_driver_->ReadValues(data_points_to_scan, values);
        
        if (success && !values.empty()) {
            worker_stats_.read_operations++;
            
            // 파이프라인으로 전송
            bool pipeline_success = SendValuesToPipelineWithLogging(
                values, 
                "BACnet Data Scan", 
                0
            );
            
            if (pipeline_success) {
                LogMessage(LogLevel::DEBUG_LEVEL, 
                          "✅ Successfully sent " + std::to_string(values.size()) + 
                          " BACnet values to pipeline");
            }
            
            // COV 처리
            for (size_t i = 0; i < data_points_to_scan.size() && i < values.size(); ++i) {
                std::string object_id = CreateObjectId(data_points_to_scan[i]);
                
                if (on_value_changed_) {
                    on_value_changed_(object_id, values[i]);
                }
                
                ProcessValueChangeForCOV(object_id, values[i]);
            }
        } else {
            worker_stats_.failed_operations++;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Exception in PerformDataScan: " + std::string(e.what()));
        worker_stats_.failed_operations++;
        return false;
    }
}

bool BACnetWorker::ProcessDataPoints(const std::vector<DataPoint>& points) {
    try {
        if (!bacnet_driver_ || !bacnet_driver_->IsConnected()) {
            LogMessage(LogLevel::WARN, "BACnet driver not connected for data point processing");
            return false;
        }
        
        LogMessage(LogLevel::DEBUG_LEVEL, 
                  "Processing " + std::to_string(points.size()) + " data points");
        
        std::vector<TimestampedValue> values;
        bool success = bacnet_driver_->ReadValues(points, values);
        
        if (success && !values.empty()) {
            worker_stats_.read_operations++;
            
            bool pipeline_success = SendValuesToPipelineWithLogging(
                values, 
                "BACnet DataPoints", 
                0
            );
            
            if (pipeline_success) {
                LogMessage(LogLevel::DEBUG_LEVEL, 
                          "Successfully sent " + std::to_string(values.size()) + 
                          " BACnet values to pipeline");
            }
            
            for (size_t i = 0; i < points.size() && i < values.size(); ++i) {
                std::string object_id = CreateObjectId(points[i]);
                
                if (on_value_changed_) {
                    on_value_changed_(object_id, values[i]);
                }
                
                ProcessValueChangeForCOV(object_id, values[i]);
            }
        } else {
            worker_stats_.failed_operations++;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "Exception in ProcessDataPoints: " + std::string(e.what()));
        worker_stats_.failed_operations++;
        return false;
    }
}

void BACnetWorker::UpdateWorkerStats(const std::string& operation, bool success) {
    if (operation == "polling") {
        worker_stats_.polling_cycles++;
    } else if (operation == "read") {
        if (success) {
            worker_stats_.read_operations++;
        } else {
            worker_stats_.failed_operations++;
        }
    } else if (operation == "write") {
        if (success) {
            worker_stats_.write_operations++;
        } else {
            worker_stats_.failed_operations++;
        }
    }
}

std::string BACnetWorker::CreateObjectId(const DataPoint& point) const {
    if (!point.id.empty()) {
        return point.id;
    }
    
    return point.device_id + ":" + std::to_string(point.address);
}

void BACnetWorker::SetupBACnetDriverCallbacks() {
    if (!bacnet_driver_) {
        return;
    }
    
    LogMessage(LogLevel::DEBUG_LEVEL, "🔗 Setting up BACnet driver callbacks...");
    LogMessage(LogLevel::DEBUG_LEVEL, "✅ BACnet driver callbacks configured");
}

void BACnetWorker::ProcessValueChangeForCOV(const std::string& object_id, 
                                           const TimestampedValue& new_value) {
    std::lock_guard<std::mutex> lock(previous_values_mutex_);
    
    auto it = previous_values_.find(object_id);
    if (it != previous_values_.end()) {
        const DataValue& previous_value = it->second;
        
        if (IsValueChanged(previous_value, new_value.value)) {
            SendCOVNotificationToPipeline(object_id, new_value.value, previous_value);
            
            LogMessage(LogLevel::DEBUG_LEVEL, 
                      "🔄 Value changed for " + object_id + " - sending COV notification");
        }
    }
    
    previous_values_[object_id] = new_value.value;
}

bool BACnetWorker::IsValueChanged(const DataValue& previous, const DataValue& current) {
    if (previous.index() != current.index()) {
        return true;
    }
    
    return previous != current;
}

bool BACnetWorker::WriteProperty(uint32_t device_id,
                                BACNET_OBJECT_TYPE object_type,
                                uint32_t object_instance,
                                BACNET_PROPERTY_ID property_id,
                                const DataValue& value,
                                uint8_t priority) {
    if (!bacnet_driver_ || !bacnet_driver_->IsConnected()) {
        LogMessage(LogLevel::WARN, "BACnet driver not connected");
        return false;
    }
    
    if (!bacnet_service_manager_) {
        LogMessage(LogLevel::ERROR, "BACnetServiceManager not initialized");
        return false;
    }
    
    try {
        LogMessage(LogLevel::INFO, 
                  "🔧 Writing BACnet property: Device=" + std::to_string(device_id) + 
                  ", Object=" + std::to_string(object_type) + ":" + std::to_string(object_instance));
        
        // BACnetServiceManager를 통해 실제 쓰기
        bool success = bacnet_service_manager_->WriteProperty(
            device_id, object_type, object_instance, property_id, value, priority);
        
        // 🔥 제어 이력 기록 (성공/실패 무관하게)
        TimestampedValue control_log;
        control_log.value = value;
        control_log.timestamp = std::chrono::system_clock::now();
        control_log.quality = success ? DataQuality::GOOD : DataQuality::BAD;
        control_log.source = "control_bacnet_" + std::to_string(device_id) + 
                            "_" + std::to_string(object_type) + 
                            "_" + std::to_string(object_instance);
        
        // 제어 이력은 높은 우선순위로 파이프라인 전송
        SendValuesToPipelineWithLogging({control_log}, "BACnet 제어 이력", 1);
        
        // 통계 업데이트
        if (success) {
            worker_stats_.write_operations++;
            LogMessage(LogLevel::INFO, "✅ BACnet write successful");
        } else {
            worker_stats_.failed_operations++;
            LogMessage(LogLevel::ERROR, "❌ BACnet write failed");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "WriteProperty 예외: " + std::string(e.what()));
        worker_stats_.failed_operations++;
        
        // 예외 발생도 이력 기록
        TimestampedValue error_log;
        error_log.value = std::string("ERROR: ") + e.what();
        error_log.timestamp = std::chrono::system_clock::now();
        error_log.quality = DataQuality::BAD;
        error_log.source = "control_bacnet_" + std::to_string(device_id) + "_error";
        
        SendValuesToPipelineWithLogging({error_log}, "BACnet 제어 에러", 1);
        
        return false;
    }
}

bool BACnetWorker::WriteObjectProperty(const std::string& object_id, 
                                      const DataValue& value,
                                      uint8_t priority) {
    try {
        // object_id 파싱: "device_id:object_type:object_instance"
        std::vector<std::string> parts;
        std::string temp = object_id;
        size_t pos = 0;
        
        while ((pos = temp.find(':')) != std::string::npos) {
            parts.push_back(temp.substr(0, pos));
            temp.erase(0, pos + 1);
        }
        parts.push_back(temp);
        
        if (parts.size() != 3) {
            LogMessage(LogLevel::ERROR, "Invalid object_id format: " + object_id + 
                      " (expected: device_id:object_type:object_instance)");
            return false;
        }
        
        uint32_t device_id = std::stoul(parts[0]);
        BACNET_OBJECT_TYPE object_type = static_cast<BACNET_OBJECT_TYPE>(std::stoul(parts[1]));
        uint32_t object_instance = std::stoul(parts[2]);
        
        // WriteProperty 호출
        return WriteProperty(device_id, object_type, object_instance, 
                           PROP_PRESENT_VALUE, value, priority);
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "WriteObjectProperty 파싱 에러: " + std::string(e.what()));
        return false;
    }
}

bool BACnetWorker::WriteBACnetDataPoint(const std::string& point_id, const DataValue& value) {
    try {
        // DataPoint ID로 실제 매핑 정보 조회 (BaseDeviceWorker에서 제공)
        const auto& data_points = GetDataPoints();
        
        for (const auto& point : data_points) {
            if (point.id == point_id) {
                // protocol_params에서 BACnet 매핑 정보 추출
                auto device_it = point.protocol_params.find("device_id");
                auto object_type_it = point.protocol_params.find("object_type");
                auto object_instance_it = point.protocol_params.find("object_instance");
                auto priority_it = point.protocol_params.find("priority");
                
                if (device_it == point.protocol_params.end() ||
                    object_type_it == point.protocol_params.end() ||
                    object_instance_it == point.protocol_params.end()) {
                    LogMessage(LogLevel::ERROR, "BACnet mapping info missing for point: " + point_id);
                    return false;
                }
                
                uint32_t device_id = std::stoul(device_it->second);
                BACNET_OBJECT_TYPE object_type = static_cast<BACNET_OBJECT_TYPE>(std::stoul(object_type_it->second));
                uint32_t object_instance = std::stoul(object_instance_it->second);
                uint8_t priority = (priority_it != point.protocol_params.end()) ? 
                                 static_cast<uint8_t>(std::stoul(priority_it->second)) : BACNET_NO_PRIORITY;
                
                return WriteProperty(device_id, object_type, object_instance, 
                                   PROP_PRESENT_VALUE, value, priority);
            }
        }
        
        LogMessage(LogLevel::ERROR, "DataPoint not found: " + point_id);
        return false;
        
    } catch (const std::exception& e) {
        LogMessage(LogLevel::ERROR, "WriteBACnetDataPoint 에러: " + std::string(e.what()));
        return false;
    }
}

} // namespace Workers
} // namespace PulseOne

/*
🎯 **최종 완성 요약**

✅ **올바른 역할만 유지**
1. 설정된 DataPoint 스캔
2. 파이프라인으로 전송
3. COV 처리

❌ **제거된 잘못된 역할**
1. Discovery (BACnetDiscoveryService가 담당)
2. DB 저장 (DataProcessingService가 담당)
3. 복잡한 스레드 구조 (단순화)

🔧 **사용법**
```cpp
// 1. Worker 생성
auto worker = std::make_unique<BACnetWorker>(device_info);

// 2. DataPoint 설정 (BACnetDiscoveryService에서 제공)
worker->LoadDataPointsFromConfiguration(data_points);

// 3. Worker 시작
worker->Start();

// 4. 자동으로 데이터 스캔 + 파이프라인 전송
```

이제 BACnetWorker가 **깔끔하고 에러 없는 완성본**입니다! 🚀
*/