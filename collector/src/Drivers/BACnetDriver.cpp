// =============================================================================
// collector/src/Drivers/BACnetDriver.cpp
// BACnet 프로토콜 드라이버 구현 - 진단 기능 완전 통합 버전
// =============================================================================

#include "Drivers/BACnetDriver.h"
#include <future>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace PulseOne {
namespace Drivers {

// 정적 멤버 초기화
BACnetDriver* BACnetDriver::instance_ = nullptr;

// =============================================================================
// ✅ 생성자 - 진단 관련 초기화 포함
// =============================================================================
BACnetDriver::BACnetDriver()
    : initialized_(false)
    , connected_(false)
    , status_(DriverStatus::DISCONNECTED)
    , stop_threads_(false)
    , stack_initialized_(false)
    // ✅ 진단 관련 초기화
    , diagnostics_enabled_(false)
    , packet_logging_enabled_(false)
    , console_output_enabled_(false)
    , log_manager_(nullptr)
    , db_manager_(nullptr)
{
    instance_ = this;
    
    // 통계 초기화
    statistics_ = {};
    statistics_.start_time = std::chrono::system_clock::now();
    
    // 에러 초기화
    last_error_ = ErrorInfo(ErrorCode::SUCCESS, "Driver created");
    
    // 패킷 히스토리 예약
    packet_history_.reserve(1000); // MAX_PACKET_HISTORY
}

BACnetDriver::~BACnetDriver() {
    Disconnect();
    instance_ = nullptr;
}

// =============================================================================
// 기본 드라이버 메소드들
// =============================================================================

bool BACnetDriver::Initialize(const DriverConfig& config) {
    if (initialized_.load()) {
        return true;
    }
    
    try {
        driver_config_ = config;
        
        // 로거 초기화
        logger_ = std::make_unique<DriverLogger>(
            config.device_id, 
            ProtocolType::BACNET_IP, 
            config.endpoint
        );
        
        // BACnet 설정 파싱
        if (!config.protocol_config.empty()) {
            ParseBACnetConfig(config.protocol_config);
        }
        
        logger_->Info("BACnet driver initializing", DriverLogCategory::GENERAL);
        logger_->Info("Device ID: " + std::to_string(config_.device_id), DriverLogCategory::GENERAL);
        logger_->Info("Interface: " + config_.interface_name, DriverLogCategory::GENERAL);
        logger_->Info("Port: " + std::to_string(config_.port), DriverLogCategory::GENERAL);
        
        // BACnet 스택 초기화
        if (!InitializeBACnetStack()) {
            SetError(ErrorCode::INITIALIZATION_FAILED, "Failed to initialize BACnet stack");
            return false;
        }
        
        status_.store(DriverStatus::INITIALIZED);
        initialized_.store(true);
        
        logger_->Info("BACnet driver initialized successfully", DriverLogCategory::GENERAL);
        return true;
        
    } catch (const std::exception& e) {
        SetError(ErrorCode::INITIALIZATION_FAILED, 
                "BACnet driver initialization failed: " + std::string(e.what()));
        logger_->Error("Initialization failed: " + std::string(e.what()), DriverLogCategory::GENERAL);
        return false;
    }
}

bool BACnetDriver::Connect() {
    if (!initialized_.load()) {
        SetError(ErrorCode::NOT_INITIALIZED, "Driver not initialized");
        return false;
    }
    
    if (connected_.load()) {
        return true;
    }
    
    try {
        logger_->Info("Connecting to BACnet network", DriverLogCategory::CONNECTION);
        
        auto start_time = std::chrono::steady_clock::now();
        
        // 네트워크 인터페이스 초기화
        datalink_set_interface(const_cast<char*>(config_.interface_name.c_str()));
        
        if (!datalink_init(const_cast<char*>(config_.interface_name.c_str()))) {
            SetError(ErrorCode::CONNECTION_FAILED, "Failed to initialize datalink");
            return false;
        }
        
        // 로컬 디바이스 설정
        Device_Set_Object_Instance_Number(config_.device_id);
        
        // 서비스 핸들러 등록
        apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, IAmHandler);
        apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_COV_NOTIFICATION, COVNotificationHandler);
        
        // 워커 스레드 시작
        stop_threads_.store(false);
        worker_thread_ = std::thread(&BACnetDriver::WorkerThread, this);
        discovery_thread_ = std::thread(&BACnetDriver::DiscoveryThread, this);
        
        connected_.store(true);
        status_.store(DriverStatus::CONNECTED);
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        
        UpdateStatistics("connect", true, duration);
        
        logger_->Info("Connected to BACnet network", DriverLogCategory::CONNECTION);
        
        // 초기 Who-Is 전송
        if (config_.who_is_enabled) {
            SendWhoIs();
        }
        
        return true;
        
    } catch (const std::exception& e) {
        SetError(ErrorCode::CONNECTION_FAILED, 
                "BACnet connection failed: " + std::string(e.what()));
        logger_->Error("Connection failed: " + std::string(e.what()), DriverLogCategory::CONNECTION);
        return false;
    }
}

bool BACnetDriver::Disconnect() {
    if (!connected_.load()) {
        return true;
    }
    
    try {
        logger_->Info("Disconnecting from BACnet network", DriverLogCategory::CONNECTION);
        
        // 스레드 중지
        stop_threads_.store(true);
        request_cv_.notify_all();
        
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        
        if (discovery_thread_.joinable()) {
            discovery_thread_.join();
        }
        
        // BACnet 스택 정리
        ShutdownBACnetStack();
        
        connected_.store(false);
        status_.store(DriverStatus::DISCONNECTED);
        
        logger_->Info("Disconnected from BACnet network", DriverLogCategory::CONNECTION);
        
        return true;
        
    } catch (const std::exception& e) {
        SetError(ErrorCode::DISCONNECTION_FAILED, 
                "BACnet disconnection failed: " + std::string(e.what()));
        logger_->Error("Disconnection failed: " + std::string(e.what()), DriverLogCategory::CONNECTION);
        return false;
    }
}

bool BACnetDriver::IsConnected() const {
    return connected_.load() && status_.load() == DriverStatus::CONNECTED;
}

bool BACnetDriver::ReadValues(const std::vector<DataPoint>& points,
                             std::vector<TimestampedValue>& values) {
    if (!IsConnected()) {
        SetError(ErrorCode::NOT_CONNECTED, "BACnet driver not connected");
        return false;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        values.clear();
        values.reserve(points.size());
        
        for (const auto& point : points) {
            uint32_t device_id;
            BACNET_OBJECT_TYPE object_type;
            uint32_t object_instance;
            BACNET_PROPERTY_ID property_id;
            uint32_t array_index;
            
            if (!ParseDataPoint(point, device_id, object_type, object_instance, 
                              property_id, array_index)) {
                values.emplace_back(DataValue(), DataQuality::BAD);
                continue;
            }
            
            BACNET_APPLICATION_DATA_VALUE bacnet_value;
            if (ReadProperty(device_id, object_type, object_instance, 
                           property_id, array_index, bacnet_value)) {
                DataValue data_value;
                if (ConvertFromBACnetValue(bacnet_value, data_value)) {
                    values.emplace_back(data_value, DataQuality::GOOD);
                } else {
                    values.emplace_back(DataValue(), DataQuality::BAD);
                }
            } else {
                values.emplace_back(DataValue(), DataQuality::BAD);
            }
        }
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        
        UpdateStatistics("read_values", true, duration);
        
        logger_->Debug("Read " + std::to_string(points.size()) + " values", 
                      DriverLogCategory::DATA_PROCESSING);
        
        return true;
        
    } catch (const std::exception& e) {
        SetError(ErrorCode::READ_FAILED, 
                "BACnet read failed: " + std::string(e.what()));
        logger_->Error("Read values failed: " + std::string(e.what()), 
                      DriverLogCategory::DATA_PROCESSING);
        return false;
    }
}

bool BACnetDriver::WriteValue(const DataPoint& point, const DataValue& value) {
    if (!IsConnected()) {
        SetError(ErrorCode::NOT_CONNECTED, "BACnet driver not connected");
        return false;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        uint32_t device_id;
        BACNET_OBJECT_TYPE object_type;
        uint32_t object_instance;
        BACNET_PROPERTY_ID property_id;
        uint32_t array_index;
        
        if (!ParseDataPoint(point, device_id, object_type, object_instance, 
                          property_id, array_index)) {
            SetError(ErrorCode::INVALID_PARAMETER, "Invalid data point format");
            return false;
        }
        
        BACNET_APPLICATION_DATA_VALUE bacnet_value;
        if (!ConvertToBACnetValue(value, point.data_type, bacnet_value)) {
            SetError(ErrorCode::INVALID_PARAMETER, "Failed to convert value");
            return false;
        }
        
        bool success = WriteProperty(device_id, object_type, object_instance, 
                                   property_id, array_index, bacnet_value);
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        
        UpdateStatistics("write_value", success, duration);
        
        if (success) {
            logger_->Info("Write value successful", DriverLogCategory::DATA_PROCESSING);
        } else {
            logger_->Error("Write value failed", DriverLogCategory::DATA_PROCESSING);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        SetError(ErrorCode::WRITE_FAILED, 
                "BACnet write failed: " + std::string(e.what()));
        logger_->Error("Write value failed: " + std::string(e.what()), 
                      DriverLogCategory::DATA_PROCESSING);
        return false;
    }
}

ProtocolType BACnetDriver::GetProtocolType() const {
    return ProtocolType::BACNET_IP;
}

DriverStatus BACnetDriver::GetStatus() const {
    return status_.load();
}

ErrorInfo BACnetDriver::GetLastError() const {
    return last_error_;
}

DriverStatistics BACnetDriver::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

std::future<std::vector<TimestampedValue>> BACnetDriver::ReadValuesAsync(
    const std::vector<DataPoint>& points) {
    
    return std::async(std::launch::async, [this, points]() {
        std::vector<TimestampedValue> values;
        ReadValues(points, values);
        return values;
    });
}

std::future<bool> BACnetDriver::WriteValueAsync(
    const DataPoint& point, const DataValue& value) {
    
    return std::async(std::launch::async, [this, point, value]() {
        return WriteValue(point, value);
    });
}

// =============================================================================
// BACnet 특화 기능들
// =============================================================================

bool BACnetDriver::SendWhoIs(uint32_t low_device_id, uint32_t high_device_id) {
    if (!IsConnected()) {
        return false;
    }
    
    try {
        BACNET_ADDRESS dest;
        
        // 브로드캐스트 주소 설정
        datalink_get_broadcast_address(&dest);
        
        // Who-Is 요청 전송
        Send_WhoIs_Global(low_device_id, high_device_id);
        
        logger_->Info("Who-Is request sent (Device ID range: " + 
                     std::to_string(low_device_id) + "-" + std::to_string(high_device_id) + ")",
                     DriverLogCategory::DISCOVERY);
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("Failed to send Who-Is: " + std::string(e.what()), 
                      DriverLogCategory::DISCOVERY);
        return false;
    }
}

std::vector<BACnetDeviceInfo> BACnetDriver::GetDiscoveredDevices() const {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    std::vector<BACnetDeviceInfo> devices;
    devices.reserve(discovered_devices_.size());
    
    for (const auto& pair : discovered_devices_) {
        devices.push_back(pair.second);
    }
    
    return devices;
}

std::vector<BACnetObjectInfo> BACnetDriver::GetDeviceObjects(uint32_t device_id) {
    std::vector<BACnetObjectInfo> objects;
    
    // Object-List 속성 읽기를 통해 디바이스의 모든 객체 조회
    BACNET_APPLICATION_DATA_VALUE object_list;
    if (ReadProperty(device_id, OBJECT_DEVICE, device_id, PROP_OBJECT_LIST, 
                    BACNET_ARRAY_ALL, object_list)) {
        // Object-List 파싱하여 개별 객체 정보 수집
        // 실제 구현에서는 배열의 각 요소를 순회하며 객체 정보를 수집
    }
    
    return objects;
}

bool BACnetDriver::SubscribeCOV(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                               uint32_t object_instance, uint32_t lifetime) {
    if (!IsConnected()) {
        return false;
    }
    
    try {
        BACNET_SUBSCRIBE_COV_DATA cov_data;
        cov_data.subscriberProcessIdentifier = config_.device_id;
        cov_data.monitoredObjectIdentifier.type = object_type;
        cov_data.monitoredObjectIdentifier.instance = object_instance;
        cov_data.cancellationRequest = false;
        cov_data.lifetime = lifetime;
        
        // COV 구독 요청 전송
        uint8_t invoke_id = tsm_next_free_invokeID();
        BACNET_ADDRESS dest;
        
        // 대상 디바이스 주소 조회
        address_get_by_device(device_id, NULL, &dest);
        
        Send_COV_Subscribe(&dest, &cov_data);
        
        logger_->Info("COV subscription requested for device " + std::to_string(device_id) +
                     " object " + std::to_string(object_type) + ":" + std::to_string(object_instance),
                     DriverLogCategory::SUBSCRIPTION);
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("COV subscription failed: " + std::string(e.what()), 
                      DriverLogCategory::SUBSCRIPTION);
        return false;
    }
}

bool BACnetDriver::UnsubscribeCOV(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                                 uint32_t object_instance) {
    if (!IsConnected()) {
        return false;
    }
    
    try {
        BACNET_SUBSCRIBE_COV_DATA cov_data;
        cov_data.subscriberProcessIdentifier = config_.device_id;
        cov_data.monitoredObjectIdentifier.type = object_type;
        cov_data.monitoredObjectIdentifier.instance = object_instance;
        cov_data.cancellationRequest = true;  // 구독 취소
        cov_data.lifetime = 0;
        
        BACNET_ADDRESS dest;
        address_get_by_device(device_id, NULL, &dest);
        
        Send_COV_Subscribe(&dest, &cov_data);
        
        logger_->Info("COV unsubscription requested for device " + std::to_string(device_id) +
                     " object " + std::to_string(object_type) + ":" + std::to_string(object_instance),
                     DriverLogCategory::SUBSCRIPTION);
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("COV unsubscription failed: " + std::string(e.what()), 
                      DriverLogCategory::SUBSCRIPTION);
        return false;
    }
}

// =============================================================================
// ✅ 진단 기능들 - 완전 구현
// =============================================================================

bool BACnetDriver::EnableDiagnostics(DatabaseManager& db_manager,
                                     bool enable_packet_logging,
                                     bool enable_console_output) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    // 기존 시스템 참조 설정
    log_manager_ = &PulseOne::LogManager::getInstance();
    db_manager_ = &db_manager;
    
    // 설정 적용
    packet_logging_enabled_ = enable_packet_logging;
    console_output_enabled_ = enable_console_output;
    
    // 디바이스 이름 조회
    if (!driver_config_.device_id.empty()) {
        device_name_ = QueryDeviceName(driver_config_.device_id);
        if (device_name_.empty()) {
            device_name_ = "bacnet_device_" + driver_config_.device_id.substr(0, 8);
        }
    }
    
    // 데이터베이스에서 포인트 정보 로드
    bool success = LoadBACnetPointsFromDB();
    
    if (success) {
        diagnostics_enabled_ = true;
        
        log_manager_->logDriver("bacnet_diagnostics",
            "Diagnostics enabled for device: " + device_name_ + 
            " (" + std::to_string(point_info_map_.size()) + " points loaded)");
        
        if (console_output_enabled_) {
            std::cout << "🏢 BACnet diagnostics enabled for " << device_name_ 
                      << " (" << point_info_map_.size() << " points)" << std::endl;
        }
    }
    
    return success;
}

void BACnetDriver::DisableDiagnostics() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    
    diagnostics_enabled_ = false;
    packet_logging_enabled_ = false;
    console_output_enabled_ = false;
    
    // 패킷 히스토리 정리
    {
        std::lock_guard<std::mutex> packet_lock(packet_log_mutex_);
        packet_history_.clear();
    }
    
    // 포인트 정보 정리
    {
        std::lock_guard<std::mutex> points_lock(points_mutex_);
        point_info_map_.clear();
    }
    
    if (log_manager_) {
        log_manager_->logDriver("bacnet_diagnostics",
            "Diagnostics disabled for device: " + device_name_);
    }
    
    if (console_output_enabled_) {
        std::cout << "🏢 BACnet diagnostics disabled for " << device_name_ << std::endl;
    }
}

std::string BACnetDriver::GetDiagnosticsJSON() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"device_id\": \"" << driver_config_.device_id << "\",\n";
    oss << "  \"device_name\": \"" << device_name_ << "\",\n";
    oss << "  \"protocol\": \"BACnet/IP\",\n";
    oss << "  \"diagnostics_enabled\": " << (diagnostics_enabled_ ? "true" : "false") << ",\n";
    oss << "  \"packet_logging_enabled\": " << (packet_logging_enabled_ ? "true" : "false") << ",\n";
    oss << "  \"console_output_enabled\": " << (console_output_enabled_ ? "true" : "false") << ",\n";
    
    {
        std::lock_guard<std::mutex> lock(points_mutex_);
        oss << "  \"data_points_count\": " << point_info_map_.size() << ",\n";
    }
    
    {
        std::lock_guard<std::mutex> lock(packet_log_mutex_);
        oss << "  \"packet_history_count\": " << packet_history_.size() << ",\n";
    }
    
    oss << "  \"connection_status\": \"" << (IsConnected() ? "connected" : "disconnected") << "\",\n";
    oss << "  \"local_device_id\": " << config_.device_id << ",\n";
    oss << "  \"interface\": \"" << config_.interface_name << "\",\n";
    oss << "  \"port\": " << config_.port << ",\n";
    oss << "  \"apdu_timeout\": " << config_.apdu_timeout << ",\n";
    oss << "  \"who_is_enabled\": " << (config_.who_is_enabled ? "true" : "false") << "\n";
    oss << "}";
    
    return oss.str();
}

std::string BACnetDriver::GetPointName(uint32_t device_id, BACNET_OBJECT_TYPE type, 
                                      uint32_t instance) const {
    std::string point_key = std::to_string(device_id) + ":" + 
                           std::to_string(static_cast<int>(type)) + ":" + 
                           std::to_string(instance);
    
    std::lock_guard<std::mutex> lock(points_mutex_);
    auto it = point_info_map_.find(point_key);
    if (it != point_info_map_.end()) {
        return it->second.name;
    }
    
    return GetObjectTypeName(type) + ":" + std::to_string(instance);
}

// =============================================================================
// 내부 메서드들
// =============================================================================

bool BACnetDriver::InitializeBACnetStack() {
    if (stack_initialized_) {
        return true;
    }
    
    try {
        // BACnet 스택 초기화
        Device_Init(NULL);
        
        // 로컬 디바이스 설정
        Device_Set_Object_Instance_Number(config_.device_id);
        Device_Set_Object_Name("PulseOne BACnet Gateway");
        Device_Set_System_Status(STATUS_OPERATIONAL, false);
        Device_Set_Vendor_Name("PulseOne Systems");
        Device_Set_Vendor_Identifier(999);  // 실제 벤더 ID로 변경 필요
        Device_Set_Model_Name("PulseOne Gateway v1.0");
        Device_Set_Application_Software_Version("1.0.0");
        
        // 네트워크 포트 설정
        datalink_set_interface(const_cast<char*>(config_.interface_name.c_str()));
        
        stack_initialized_ = true;
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("BACnet stack initialization failed: " + std::string(e.what()),
                      DriverLogCategory::GENERAL);
        return false;
    }
}

void BACnetDriver::ShutdownBACnetStack() {
    if (!stack_initialized_) {
        return;
    }
    
    try {
        // 활성 TSM 정리
        tsm_free_all_invokeID();
        
        // 데이터링크 정리
        datalink_cleanup();
        
        stack_initialized_ = false;
        
    } catch (const std::exception& e) {
        logger_->Error("BACnet stack shutdown failed: " + std::string(e.what()),
                      DriverLogCategory::GENERAL);
    }
}

void BACnetDriver::WorkerThread() {
    logger_->Info("BACnet worker thread started", DriverLogCategory::GENERAL);
    
    while (!stop_threads_.load()) {
        try {
            // BACnet 메시지 처리
            BACNET_ADDRESS src;
            uint16_t pdu_len;
            unsigned timeout = 100;  // 100ms 타임아웃
            uint8_t Rx_Buf[MAX_MPDU];
            
            pdu_len = datalink_receive(&src, &Rx_Buf[0], MAX_MPDU, timeout);
            if (pdu_len) {
                npdu_handler(&src, &Rx_Buf[0], pdu_len);
            }
            
            // TSM 타임아웃 처리
            tsm_timer_milliseconds(timeout);
            
            // 대기 중인 요청 처리
            std::unique_lock<std::mutex> lock(request_mutex_);
            if (request_cv_.wait_for(lock, std::chrono::milliseconds(10), 
                                   [this] { return !request_queue_.empty() || stop_threads_.load(); })) {
                
                while (!request_queue_.empty() && !stop_threads_.load()) {
                    auto request = request_queue_.front();
                    request_queue_.pop();
                    lock.unlock();
                    
                    request();
                    
                    lock.lock();
                }
            }
            
        } catch (const std::exception& e) {
            logger_->Error("Worker thread error: " + std::string(e.what()),
                          DriverLogCategory::GENERAL);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    logger_->Info("BACnet worker thread stopped", DriverLogCategory::GENERAL);
}

void BACnetDriver::DiscoveryThread() {
    logger_->Info("BACnet discovery thread started", DriverLogCategory::DISCOVERY);
    
    while (!stop_threads_.load()) {
        try {
            if (config_.who_is_enabled && IsConnected()) {
                SendWhoIs();
            }
            
            // 설정된 간격만큼 대기
            auto wait_time = std::chrono::milliseconds(config_.who_is_interval);
            std::this_thread::sleep_for(wait_time);
            
        } catch (const std::exception& e) {
            logger_->Error("Discovery thread error: " + std::string(e.what()),
                          DriverLogCategory::DISCOVERY);
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        }
    }
    
    logger_->Info("BACnet discovery thread stopped", DriverLogCategory::DISCOVERY);
}

// =============================================================================
// ✅ ReadProperty - 진단 로깅 통합 버전
// =============================================================================

bool BACnetDriver::ReadProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                               uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                               uint32_t array_index, BACNET_APPLICATION_DATA_VALUE& value) {
    auto start_time = std::chrono::steady_clock::now();
    
    // ✅ 진단: 송신 패킷 로깅
    if (diagnostics_enabled_) {
        LogBACnetPacket("TX", device_id, object_type, object_instance, property_id, true);
    }
    
    try {
        BACNET_ADDRESS target_address;
        
        // 디바이스 주소 조회
        if (!address_get_by_device(device_id, NULL, &target_address)) {
            logger_->Warn("Device address not found: " + std::to_string(device_id),
                         DriverLogCategory::COMMUNICATION);
                         
            // ✅ 진단: 에러 로깅
            if (diagnostics_enabled_) {
                auto end_time = std::chrono::steady_clock::now();
                auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>
                                  (end_time - start_time).count();
                LogBACnetPacket("RX", device_id, object_type, object_instance, 
                              property_id, false, "Device address not found", duration_ms);
            }
            return false;
        }
        
        uint8_t invoke_id = tsm_next_free_invokeID();
        if (invoke_id == 0) {
            logger_->Warn("No free invoke ID available", DriverLogCategory::COMMUNICATION);
            
            // ✅ 진단: 에러 로깅
            if (diagnostics_enabled_) {
                auto end_time = std::chrono::steady_clock::now();
                auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>
                                  (end_time - start_time).count();
                LogBACnetPacket("RX", device_id, object_type, object_instance, 
                              property_id, false, "No free invoke ID", duration_ms);
            }
            return false;
        }
        
        // Read Property 요청 전송
        if (!Send_Read_Property_Request(invoke_id, &target_address, object_type,
                                       object_instance, property_id, array_index)) {
            logger_->Error("Failed to send Read Property request", DriverLogCategory::COMMUNICATION);
            
            // ✅ 진단: 에러 로깅
            if (diagnostics_enabled_) {
                auto end_time = std::chrono::steady_clock::now();
                auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>
                                  (end_time - start_time).count();
                LogBACnetPacket("RX", device_id, object_type, object_instance, 
                              property_id, false, "Failed to send request", duration_ms);
            }
            return false;
        }
        
        // 응답 대기
        auto timeout = std::chrono::milliseconds(config_.apdu_timeout);
        auto request_start = std::chrono::steady_clock::now();
        
        while (std::chrono::steady_clock::now() - request_start < timeout) {
            if (tsm_invoke_id_free(invoke_id)) {
                // 응답을 받았음, 결과 확인
                BACNET_TSM_DATA* tsm = tsm_get_transaction(invoke_id);
                if (tsm && tsm->state == TSM_STATE_COMPLETED) {
                    // 성공적으로 읽기 완료
                    auto end_time = std::chrono::steady_clock::now();
                    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>
                                      (end_time - start_time).count();
                    
                    // ✅ 진단: 성공 로깅
                    if (diagnostics_enabled_) {
                        LogBACnetPacket("RX", device_id, object_type, object_instance, 
                                      property_id, true, "", duration_ms);
                    }
                    
                    // 실제 구현에서는 응답 데이터를 파싱하여 value에 저장
                    return true;
                }
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // 타임아웃 또는 실패
        tsm_free_invoke_id(invoke_id);
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>
                          (end_time - start_time).count();
        
        // ✅ 진단: 타임아웃 로깅
        if (diagnostics_enabled_) {
            LogBACnetPacket("RX", device_id, object_type, object_instance, 
                          property_id, false, "Timeout after " + std::to_string(duration_ms) + "ms", duration_ms);
        }
        
        return false;
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>
                          (end_time - start_time).count();
        
        // ✅ 진단: 예외 로깅
        if (diagnostics_enabled_) {
            LogBACnetPacket("RX", device_id, object_type, object_instance, 
                          property_id, false, "Exception: " + std::string(e.what()), duration_ms);
        }
        
        logger_->Error("Read property error: " + std::string(e.what()),
                      DriverLogCategory::COMMUNICATION);
        return false;
    }
}

// =============================================================================
// ✅ WriteProperty - 진단 로깅 통합 버전
// =============================================================================

bool BACnetDriver::WriteProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                                uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                                uint32_t array_index, const BACNET_APPLICATION_DATA_VALUE& value) {
    auto start_time = std::chrono::steady_clock::now();
    
    // ✅ 진단: 송신 패킷 로깅
    if (diagnostics_enabled_) {
        LogBACnetPacket("TX", device_id, object_type, object_instance, property_id, true);
    }
    
    try {
        BACNET_ADDRESS target_address;
        
        if (!address_get_by_device(device_id, NULL, &target_address)) {
            // ✅ 진단: 에러 로깅
            if (diagnostics_enabled_) {
                auto end_time = std::chrono::steady_clock::now();
                auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>
                                  (end_time - start_time).count();
                LogBACnetPacket("RX", device_id, object_type, object_instance, 
                              property_id, false, "Device address not found", duration_ms);
            }
            return false;
        }
        
        uint8_t invoke_id = tsm_next_free_invokeID();
        if (invoke_id == 0) {
            // ✅ 진단: 에러 로깅
            if (diagnostics_enabled_) {
                auto end_time = std::chrono::steady_clock::now();
                auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>
                                  (end_time - start_time).count();
                LogBACnetPacket("RX", device_id, object_type, object_instance, 
                              property_id, false, "No free invoke ID", duration_ms);
            }
            return false;
        }
        
        BACNET_WRITE_PROPERTY_DATA wp_data;
        wp_data.object_type = object_type;
        wp_data.object_instance = object_instance;
        wp_data.object_property = property_id;
        wp_data.array_index = array_index;
        wp_data.priority = BACNET_NO_PRIORITY;
        wp_data.application_data_len = 0;  // 실제 구현에서는 value를 인코딩
        
        if (!Send_Write_Property_Request(invoke_id, &target_address, &wp_data)) {
            // ✅ 진단: 에러 로깅
            if (diagnostics_enabled_) {
                auto end_time = std::chrono::steady_clock::now();
                auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>
                                  (end_time - start_time).count();
                LogBACnetPacket("RX", device_id, object_type, object_instance, 
                              property_id, false, "Failed to send write request", duration_ms);
            }
            return false;
        }
        
        // 응답 대기 로직 (ReadProperty와 유사)
        auto timeout = std::chrono::milliseconds(config_.apdu_timeout);
        auto request_start = std::chrono::steady_clock::now();
        
        while (std::chrono::steady_clock::now() - request_start < timeout) {
            if (tsm_invoke_id_free(invoke_id)) {
                BACNET_TSM_DATA* tsm = tsm_get_transaction(invoke_id);
                if (tsm && tsm->state == TSM_STATE_COMPLETED) {
                    auto end_time = std::chrono::steady_clock::now();
                    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>
                                      (end_time - start_time).count();
                    
                    // ✅ 진단: 성공 로깅
                    if (diagnostics_enabled_) {
                        LogBACnetPacket("RX", device_id, object_type, object_instance, 
                                      property_id, true, "", duration_ms);
                    }
                    
                    return true;
                }
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        tsm_free_invoke_id(invoke_id);
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>
                          (end_time - start_time).count();
        
        // ✅ 진단: 타임아웃 로깅
        if (diagnostics_enabled_) {
            LogBACnetPacket("RX", device_id, object_type, object_instance, 
                          property_id, false, "Write timeout after " + std::to_string(duration_ms) + "ms", duration_ms);
        }
        
        return false;
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>
                          (end_time - start_time).count();
        
        // ✅ 진단: 예외 로깅
        if (diagnostics_enabled_) {
            LogBACnetPacket("RX", device_id, object_type, object_instance, 
                          property_id, false, "Exception: " + std::string(e.what()), duration_ms);
        }
        
        logger_->Error("Write property error: " + std::string(e.what()),
                      DriverLogCategory::COMMUNICATION);
        return false;
    }
}

// =============================================================================
// 정적 핸들러들
// =============================================================================

void BACnetDriver::IAmHandler(uint8_t* service_request, uint16_t service_len,
                             BACNET_ADDRESS* src) {
    if (!instance_) return;
    
    try {
        uint32_t device_id;
        unsigned max_apdu;
        int segmentation;
        uint16_t vendor_id;
        
        if (iam_decode_service_request(service_request, service_len,
                                     &device_id, &max_apdu, &segmentation, &vendor_id)) {
            
            BACnetDeviceInfo device_info;
            device_info.device_id = device_id;
            device_info.max_apdu_length = max_apdu;
            device_info.segmentation_supported = (segmentation != SEGMENTATION_NONE);
            device_info.last_seen = std::chrono::system_clock::now();
            
            // IP 주소 추출
            if (src->net == 0) {  // 로컬 네트워크
                // BACnet/IP의 경우 MAC 주소에서 IP 추출
                if (src->len == 6) {  // IP + Port
                    std::ostringstream ip_str;
                    ip_str << static_cast<int>(src->adr[0]) << "."
                          << static_cast<int>(src->adr[1]) << "."
                          << static_cast<int>(src->adr[2]) << "."
                          << static_cast<int>(src->adr[3]);
                    device_info.ip_address = ip_str.str();
                    device_info.port = (src->adr[4] << 8) | src->adr[5];
                }
            }
            
            // 디바이스 목록에 추가
            {
                std::lock_guard<std::mutex> lock(instance_->devices_mutex_);
                instance_->discovered_devices_[device_id] = device_info;
            }
            
            instance_->logger_->Info("Discovered device: " + std::to_string(device_id) +
                                   " at " + device_info.ip_address,
                                   DriverLogCategory::DISCOVERY);
        }
        
    } catch (const std::exception& e) {
        if (instance_->logger_) {
            instance_->logger_->Error("I-Am handler error: " + std::string(e.what()),
                                     DriverLogCategory::DISCOVERY);
        }
    }
}

void BACnetDriver::COVNotificationHandler(uint8_t* service_request, uint16_t service_len,
                                         BACNET_ADDRESS* src) {
    if (!instance_) return;
    
    try {
        BACNET_COV_DATA cov_data;
        
        if (cov_notify_decode_service_request(service_request, service_len, &cov_data)) {
            instance_->logger_->Info("COV notification received from device " +
                                   std::to_string(cov_data.subscriberProcessIdentifier),
                                   DriverLogCategory::SUBSCRIPTION);
            
            // COV 데이터 처리 (실제 구현에서는 값 변경 이벤트 처리)
        }
        
    } catch (const std::exception& e) {
        if (instance_->logger_) {
            instance_->logger_->Error("COV notification handler error: " + std::string(e.what()),
                                     DriverLogCategory::SUBSCRIPTION);
        }
    }
}

// =============================================================================
// 유틸리티 메서드들
// =============================================================================

bool BACnetDriver::ConvertToBACnetValue(const DataValue& data_value, DataType data_type,
                                       BACNET_APPLICATION_DATA_VALUE& bacnet_value) {
    try {
        switch (data_type) {
            case DataType::BOOL:
                if (std::holds_alternative<bool>(data_value)) {
                    bacnet_value.tag = BACNET_APPLICATION_TAG_BOOLEAN;
                    bacnet_value.type.Boolean = std::get<bool>(data_value);
                    return true;
                }
                break;
                
            case DataType::INT16:
            case DataType::INT32:
                if (std::holds_alternative<int>(data_value)) {
                    bacnet_value.tag = BACNET_APPLICATION_TAG_SIGNED_INT;
                    bacnet_value.type.Signed_Int = std::get<int>(data_value);
                    return true;
                }
                break;
                
            case DataType::UINT16:
            case DataType::UINT32:
                if (std::holds_alternative<unsigned int>(data_value)) {
                    bacnet_value.tag = BACNET_APPLICATION_TAG_UNSIGNED_INT;
                    bacnet_value.type.Unsigned_Int = std::get<unsigned int>(data_value);
                    return true;
                }
                break;
                
            case DataType::FLOAT:
            case DataType::DOUBLE:
                if (std::holds_alternative<double>(data_value)) {
                    bacnet_value.tag = BACNET_APPLICATION_TAG_REAL;
                    bacnet_value.type.Real = static_cast<float>(std::get<double>(data_value));
                    return true;
                }
                break;
                
            case DataType::STRING:
                if (std::holds_alternative<std::string>(data_value)) {
                    bacnet_value.tag = BACNET_APPLICATION_TAG_CHARACTER_STRING;
                    characterstring_init_ansi(&bacnet_value.type.Character_String,
                                            const_cast<char*>(std::get<std::string>(data_value).c_str()));
                    return true;
                }
                break;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("Value conversion error: " + std::string(e.what()),
                      DriverLogCategory::DATA_PROCESSING);
        return false;
    }
}

bool BACnetDriver::ConvertFromBACnetValue(const BACNET_APPLICATION_DATA_VALUE& bacnet_value,
                                         DataValue& data_value) {
    try {
        switch (bacnet_value.tag) {
            case BACNET_APPLICATION_TAG_BOOLEAN:
                data_value = bacnet_value.type.Boolean;
                return true;
                
            case BACNET_APPLICATION_TAG_UNSIGNED_INT:
                data_value = bacnet_value.type.Unsigned_Int;
                return true;
                
            case BACNET_APPLICATION_TAG_SIGNED_INT:
                data_value = bacnet_value.type.Signed_Int;
                return true;
                
            case BACNET_APPLICATION_TAG_REAL:
                data_value = static_cast<double>(bacnet_value.type.Real);
                return true;
                
            case BACNET_APPLICATION_TAG_DOUBLE:
                data_value = bacnet_value.type.Double;
                return true;
                
            case BACNET_APPLICATION_TAG_CHARACTER_STRING:
                data_value = std::string(characterstring_value(&bacnet_value.type.Character_String));
                return true;
                
            default:
                return false;
        }
        
    } catch (const std::exception& e) {
        logger_->Error("Value conversion error: " + std::string(e.what()),
                      DriverLogCategory::DATA_PROCESSING);
        return false;
    }
}

bool BACnetDriver::ParseDataPoint(const DataPoint& point, uint32_t& device_id,
                                 BACNET_OBJECT_TYPE& object_type, uint32_t& object_instance,
                                 BACNET_PROPERTY_ID& property_id, uint32_t& array_index) {
    try {
        // address 필드에서 BACnet 객체 정보 파싱
        // 형식: "DeviceID:ObjectType:ObjectInstance:PropertyID[:ArrayIndex]"
        std::string addr_str = std::to_string(point.address);
        std::vector<std::string> parts;
        
        // 콜론으로 분할
        size_t start = 0;
        size_t end = addr_str.find(':');
        while (end != std::string::npos) {
            parts.push_back(addr_str.substr(start, end - start));
            start = end + 1;
            end = addr_str.find(':', start);
        }
        parts.push_back(addr_str.substr(start));
        
        if (parts.size() < 4) {
            return false;
        }
        
        device_id = std::stoul(parts[0]);
        object_type = static_cast<BACNET_OBJECT_TYPE>(std::stoul(parts[1]));
        object_instance = std::stoul(parts[2]);
        property_id = static_cast<BACNET_PROPERTY_ID>(std::stoul(parts[3]));
        array_index = (parts.size() > 4) ? std::stoul(parts[4]) : BACNET_ARRAY_ALL;
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("DataPoint parsing error: " + std::string(e.what()),
                      DriverLogCategory::DATA_PROCESSING);
        return false;
    }
}

void BACnetDriver::SetError(ErrorCode code, const std::string& message) {
    last_error_ = ErrorInfo(code, message);
    if (code != ErrorCode::SUCCESS) {
        status_.store(DriverStatus::ERROR);
        if (logger_) {
            logger_->Error(message, DriverLogCategory::GENERAL);
        }
    }
}

void BACnetDriver::UpdateStatistics(const std::string& operation, bool success,
                                   std::chrono::milliseconds duration) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    statistics_.total_operations++;
    if (success) {
        statistics_.successful_operations++;
    } else {
        statistics_.failed_operations++;
    }
    
    statistics_.total_response_time += duration;
    if (duration > statistics_.max_response_time) {
        statistics_.max_response_time = duration;
    }
    
    if (statistics_.min_response_time == std::chrono::milliseconds::zero() ||
        duration < statistics_.min_response_time) {
        statistics_.min_response_time = duration;
    }
    
    statistics_.last_operation_time = std::chrono::system_clock::now();
}

void BACnetDriver::ParseBACnetConfig(const std::string& config_str) {
    // 간단한 키=값 형식 파싱 (실제로는 JSON 파서 사용 권장)
    std::istringstream iss(config_str);
    std::string line;
    
    while (std::getline(iss, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            if (key == "device_id") {
                config_.device_id = std::stoul(value);
            } else if (key == "interface") {
                config_.interface_name = value;
            } else if (key == "port") {
                config_.port = std::stoul(value);
            } else if (key == "timeout") {
                config_.apdu_timeout = std::stoul(value);
            } else if (key == "retries") {
                config_.apdu_retries = std::stoul(value);
            } else if (key == "who_is_enabled") {
                config_.who_is_enabled = (value == "true" || value == "1");
            } else if (key == "who_is_interval") {
                config_.who_is_interval = std::stoul(value);
            } else if (key == "scan_interval") {
                config_.scan_interval = std::stoul(value);
            }
        }
    }
}

// =============================================================================
// ✅ 진단 헬퍼 함수들 구현
// =============================================================================

bool BACnetDriver::LoadBACnetPointsFromDB() {
    if (!db_manager_) {
        return false;
    }
    
    std::string query = R"(
        SELECT dp.name, dp.description, dp.unit, dp.scaling_factor, dp.scaling_offset,
               bp.device_id, bp.object_type, bp.object_instance, bp.property_id
        FROM data_points dp
        JOIN bacnet_points bp ON dp.id = bp.data_point_id
        WHERE bp.device_id = $1 AND dp.is_enabled = true
        ORDER BY bp.object_instance
    )";
    
    try {
        auto result = db_manager_->ExecuteQuery(query, {std::to_string(config_.device_id)});
        
        {
            std::lock_guard<std::mutex> lock(points_mutex_);
            point_info_map_.clear();
            
            for (const auto& row : result) {
                BACnetDataPointInfo point;
                point.name = row["name"].as<std::string>();
                point.description = row["description"].as<std::string>("");
                point.unit = row["unit"].as<std::string>("");
                point.scaling_factor = row["scaling_factor"].as<double>(1.0);
                point.scaling_offset = row["scaling_offset"].as<double>(0.0);
                
                // Key: "DeviceID:ObjectType:ObjectInstance"
                std::string key = row["device_id"].as<std::string>() + ":" +
                                 row["object_type"].as<std::string>() + ":" +
                                 row["object_instance"].as<std::string>();
                
                point_info_map_[key] = point;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        if (log_manager_) {
            log_manager_->logError("Failed to load BACnet points: " + std::string(e.what()));
        }
        return false;
    }
}

std::string BACnetDriver::QueryDeviceName(const std::string& device_id) {
    if (!db_manager_) {
        return "";
    }
    
    std::string query = "SELECT name FROM devices WHERE id = $1";
    
    try {
        auto result = db_manager_->ExecuteQuery(query, {device_id});
        if (!result.empty()) {
            return result[0]["name"].as<std::string>();
        }
    } catch (const std::exception& e) {
        if (log_manager_) {
            log_manager_->logError("Failed to query device name: " + std::string(e.what()));
        }
    }
    
    return "";
}

void BACnetDriver::LogBACnetPacket(const std::string& direction, uint32_t device_id,
                                  BACNET_OBJECT_TYPE type, uint32_t instance,
                                  BACNET_PROPERTY_ID prop, bool success,
                                  const std::string& error, double response_time_ms) {
    if (!diagnostics_enabled_) {
        return;
    }
    
    BACnetPacketLog log;
    log.direction = direction;
    log.timestamp = std::chrono::system_clock::now();
    log.device_id = device_id;
    log.object_type = type;
    log.object_instance = instance;
    log.property_id = prop;
    log.success = success;
    log.error_message = error;
    log.response_time_ms = response_time_ms;
    
    // 포인트 이름 조회 및 값 디코딩
    std::string point_key = std::to_string(device_id) + ":" + 
                           std::to_string(static_cast<int>(type)) + ":" + 
                           std::to_string(instance);
    
    {
        std::lock_guard<std::mutex> lock(points_mutex_);
        auto it = point_info_map_.find(point_key);
        if (it != point_info_map_.end()) {
            log.decoded_value = it->second.name;
            if (!it->second.unit.empty()) {
                log.decoded_value += " (" + it->second.unit + ")";
            }
        } else {
            log.decoded_value = GetObjectTypeName(type) + ":" + std::to_string(instance);
        }
    }
    
    // 원시 APDU 데이터 (실제로는 BACnet 스택에서 추출해야 함)
    log.raw_data = "APDU[" + std::to_string(device_id) + ":" + 
                   std::to_string(static_cast<int>(type)) + ":" + 
                   std::to_string(instance) + ":" + 
                   std::to_string(static_cast<int>(prop)) + "]";
    
    // 패킷 히스토리에 추가
    {
        std::lock_guard<std::mutex> lock(packet_log_mutex_);
        packet_history_.push_back(log);
        TrimPacketHistory();
    }
    
    // 콘솔 출력
    if (console_output_enabled_) {
        std::cout << FormatPacketForConsole(log) << std::endl;
    }
    
    // 파일 로깅
    if (packet_logging_enabled_ && log_manager_) {
        std::string log_msg = FormatPacketForFile(log);
        log_manager_->logPacket("bacnet", device_name_, log_msg);
    }
}

std::string BACnetDriver::GetObjectTypeName(BACNET_OBJECT_TYPE type) const {
    switch (type) {
        case OBJECT_ANALOG_INPUT: return "AnalogInput";
        case OBJECT_ANALOG_OUTPUT: return "AnalogOutput";
        case OBJECT_ANALOG_VALUE: return "AnalogValue";
        case OBJECT_BINARY_INPUT: return "BinaryInput";
        case OBJECT_BINARY_OUTPUT: return "BinaryOutput";
        case OBJECT_BINARY_VALUE: return "BinaryValue";
        case OBJECT_DEVICE: return "Device";
        case OBJECT_MULTI_STATE_INPUT: return "MultiStateInput";
        case OBJECT_MULTI_STATE_OUTPUT: return "MultiStateOutput";
        case OBJECT_MULTI_STATE_VALUE: return "MultiStateValue";
        default: return "Unknown(" + std::to_string(static_cast<int>(type)) + ")";
    }
}

std::string BACnetDriver::GetPropertyName(BACNET_PROPERTY_ID prop) const {
    switch (prop) {
        case PROP_PRESENT_VALUE: return "PresentValue";
        case PROP_OBJECT_NAME: return "ObjectName";
        case PROP_DESCRIPTION: return "Description";
        case PROP_UNITS: return "Units";
        case PROP_OUT_OF_SERVICE: return "OutOfService";
        case PROP_STATUS_FLAGS: return "StatusFlags";
        case PROP_RELIABILITY: return "Reliability";
        case PROP_PRIORITY_ARRAY: return "PriorityArray";
        case PROP_RELINQUISH_DEFAULT: return "RelinquishDefault";
        default: return "Property(" + std::to_string(static_cast<int>(prop)) + ")";
    }
}

void BACnetDriver::TrimPacketHistory() {
    const size_t MAX_PACKET_HISTORY = 1000;
    if (packet_history_.size() > MAX_PACKET_HISTORY) {
        packet_history_.erase(packet_history_.begin(), 
                             packet_history_.begin() + (packet_history_.size() - MAX_PACKET_HISTORY));
    }
}

std::string BACnetDriver::FormatPacketForConsole(const BACnetPacketLog& log) const {
    std::ostringstream oss;
    
    auto time_t = std::chrono::system_clock::to_time_t(log.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>
             (log.timestamp.time_since_epoch()) % 1000;
    
    oss << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S") << "."
        << std::setfill('0') << std::setw(3) << ms.count() << "] ";
    
    if (log.direction == "TX") {
        oss << "🏢 TX -> Device " << log.device_id << ": Read Property"
            << "\n  📍 Object: " << GetObjectTypeName(log.object_type) 
            << ":" << log.object_instance
            << "\n  🔍 Property: " << GetPropertyName(log.property_id)
            << "\n  📊 Point: " << log.decoded_value;
            
    } else { // RX
        oss << "🏢 RX <- Device " << log.device_id << ": ";
        if (log.success) {
            oss << "✅ SUCCESS (" << std::fixed << std::setprecision(1) 
                << log.response_time_ms << "ms)";
            if (!log.decoded_value.empty()) {
                oss << "\n  📊 Value: " << log.decoded_value;
            }
        } else {
            oss << "❌ FAILED: " << log.error_message;
        }
    }
    
    return oss.str();
}

std::string BACnetDriver::FormatPacketForFile(const BACnetPacketLog& log) const {
    std::ostringstream oss;
    
    auto time_t = std::chrono::system_clock::to_time_t(log.timestamp);
    
    oss << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "]"
        << " " << log.direction 
        << " Device:" << log.device_id
        << " " << GetObjectTypeName(log.object_type) << ":" << log.object_instance
        << " " << GetPropertyName(log.property_id);
    
    if (log.direction == "RX") {
        if (log.success) {
            oss << " SUCCESS (" << log.response_time_ms << "ms)";
            if (!log.decoded_value.empty()) {
                oss << " [" << log.decoded_value << "]";
            }
        } else {
            oss << " FAILED: " << log.error_message;
        }
    }
    
    if (!log.raw_data.empty()) {
        oss << " RAW: " << log.raw_data;
    }
    
    return oss.str();
}

// 드라이버 자동 등록
REGISTER_DRIVER(ProtocolType::BACNET_IP, BACnetDriver)

} // namespace Drivers
} // namespace PulseOne