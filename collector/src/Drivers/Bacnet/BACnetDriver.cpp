#include "Drivers/Bacnet/BACnetDriver.h"
#include <future>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

BACnetDriver::BACnetDriver()
    : initialized_(false)
    , connected_(false)
    , status_(DriverStatus::UNINITIALIZED)
    , stop_threads_(false)
{
    // 통계 초기화
    statistics_ = {};
    statistics_.start_time = std::chrono::system_clock::now();
    
    // 에러 초기화
    last_error_ = ErrorInfo(ErrorCode::SUCCESS, "Driver created");
    
    // BACnet 설정 기본값
    config_.device_id = 260001;
    config_.interface_name = "eth0";
    config_.port = 47808;
    config_.apdu_timeout = 6000;
    config_.apdu_retries = 3;
    config_.who_is_enabled = true;
    config_.who_is_interval = 30000;
    config_.scan_interval = 5000;
    device_addresses_.clear();
}

BACnetDriver::~BACnetDriver() {
    Disconnect();
}

// =============================================================================
// IProtocolDriver 인터페이스 구현
// =============================================================================

bool BACnetDriver::Initialize(const DriverConfig& config) {
    if (initialized_.load()) {
        return true;
    }
    
    try {
        driver_config_ = config;
        
        // 로거 초기화
        logger_ = std::make_unique<DriverLogger>(
            std::to_string(config.device_id), 
            ProtocolType::BACNET_IP, 
            config.endpoint
        );
        
        // BACnet 설정 파싱
        auto it = config.properties.find("protocol_config");
        if (it != config.properties.end() && !it->second.empty()) {
            ParseBACnetConfig(it->second);
        }
        
        logger_->Info("BACnet driver initializing", DriverLogCategory::GENERAL);
        logger_->Info("Device ID: " + std::to_string(config_.device_id), DriverLogCategory::GENERAL);
        logger_->Info("Interface: " + config_.interface_name, DriverLogCategory::GENERAL);
        logger_->Info("Port: " + std::to_string(config_.port), DriverLogCategory::GENERAL);
        
        // BACnet 스택 초기화
        if (!InitializeBACnetStack()) {
            SetError(ErrorCode::CONFIGURATION_ERROR, "Failed to initialize BACnet stack");
            return false;
        }
        
        status_.store(DriverStatus::INITIALIZED);
        initialized_.store(true);
        
        logger_->Info("BACnet driver initialized successfully", DriverLogCategory::GENERAL);
        return true;
        
    } catch (const std::exception& e) {
        SetError(ErrorCode::CONFIGURATION_ERROR, 
                "BACnet driver initialization failed: " + std::string(e.what()));
        if (logger_) {
            logger_->Error("Initialization failed: " + std::string(e.what()), DriverLogCategory::GENERAL);
        }
        return false;
    }
}

bool BACnetDriver::Connect() {
    if (!initialized_.load()) {
        SetError(ErrorCode::CONFIGURATION_ERROR, "Driver not initialized");
        return false;
    }
    
    if (connected_.load()) {
        return true;
    }
    
    try {
        logger_->Info("Connecting to BACnet network", DriverLogCategory::CONNECTION);
        
        auto start_time = std::chrono::steady_clock::now();
        
        // 워커 스레드 시작
        stop_threads_.store(false);
        worker_thread_ = std::thread(&BACnetDriver::WorkerThread, this);
        discovery_thread_ = std::thread(&BACnetDriver::DiscoveryThread, this);
        
        connected_.store(true);
        status_.store(DriverStatus::RUNNING);
        
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
        if (logger_) {
            logger_->Error("Connection failed: " + std::string(e.what()), DriverLogCategory::CONNECTION);
        }
        return false;
    }
}

bool BACnetDriver::Disconnect() {
    if (!connected_.load()) {
        return true;
    }
    
    try {
        if (logger_) {
            logger_->Info("Disconnecting from BACnet network", DriverLogCategory::CONNECTION);
        }
        
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
        status_.store(DriverStatus::STOPPED);
        
        if (logger_) {
            logger_->Info("Disconnected from BACnet network", DriverLogCategory::CONNECTION);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        SetError(ErrorCode::CONNECTION_FAILED, 
                "BACnet disconnection failed: " + std::string(e.what()));
        if (logger_) {
            logger_->Error("Disconnection failed: " + std::string(e.what()), DriverLogCategory::CONNECTION);
        }
        return false;
    }
}

bool BACnetDriver::IsConnected() const {
    return connected_.load() && status_.load() == DriverStatus::RUNNING;
}

bool BACnetDriver::ReadValues(const std::vector<DataPoint>& points,
                             std::vector<TimestampedValue>& values) {
    if (!IsConnected()) {
        SetError(ErrorCode::CONNECTION_FAILED, "BACnet driver not connected");
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
        
        if (logger_) {
            logger_->Debug("Read " + std::to_string(points.size()) + " values", 
                          DriverLogCategory::DATA_PROCESSING);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        SetError(ErrorCode::CONNECTION_FAILED, 
                "BACnet read failed: " + std::string(e.what()));
        if (logger_) {
            logger_->Error("Read values failed: " + std::string(e.what()), 
                          DriverLogCategory::DATA_PROCESSING);
        }
        return false;
    }
}

bool BACnetDriver::WriteValue(const DataPoint& point, const DataValue& value) {
    if (!IsConnected()) {
        SetError(ErrorCode::CONNECTION_FAILED, "BACnet driver not connected");
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
        
        if (logger_) {
            if (success) {
                logger_->Info("Write value successful", DriverLogCategory::DATA_PROCESSING);
            } else {
                logger_->Error("Write value failed", DriverLogCategory::DATA_PROCESSING);
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        SetError(ErrorCode::CONNECTION_FAILED, 
                "BACnet write failed: " + std::string(e.what()));
        if (logger_) {
            logger_->Error("Write value failed: " + std::string(e.what()), 
                          DriverLogCategory::DATA_PROCESSING);
        }
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

const DriverStatistics& BACnetDriver::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

std::future<std::vector<TimestampedValue>> BACnetDriver::ReadValuesAsync(
    const std::vector<DataPoint>& points, int timeout_ms) {
    
    (void)timeout_ms; // 경고 제거
    
    return std::async(std::launch::async, [this, points]() {
        std::vector<TimestampedValue> values;
        ReadValues(points, values);
        return values;
    });
}

std::future<bool> BACnetDriver::WriteValueAsync(
    const DataPoint& point, const DataValue& value, int priority) {
    
    (void)priority; // 경고 제거
    
    return std::async(std::launch::async, [this, point, value]() {
        return WriteValue(point, value);
    });
}

// =============================================================================
// BACnet 특화 기능
// =============================================================================



std::vector<BACnetDeviceInfo> BACnetDriver::GetDiscoveredDevices() const {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    std::vector<BACnetDeviceInfo> devices;
    
    for (const auto& pair : discovered_devices_) {
        devices.push_back(pair.second);
    }
    
    return devices;
}

// =============================================================================
// 내부 헬퍼 함수들
// =============================================================================

bool BACnetDriver::InitializeBACnetStack() {
    try {
        // BACnet 스택 초기화 (실제 함수가 존재할 때)
        // Device_Init(NULL);
        // Device_Set_Object_Instance_Number(config_.device_id);
        
        if (logger_) {
            logger_->Info("BACnet stack initialized", DriverLogCategory::GENERAL);
        }
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("BACnet stack initialization failed: " + std::string(e.what()),
                          DriverLogCategory::GENERAL);
        }
        return false;
    }
}

void BACnetDriver::ShutdownBACnetStack() {
    try {
        // BACnet 스택 정리 (실제 함수가 있을 때 활성화)
        
        if (logger_) {
            logger_->Info("BACnet stack shutdown", DriverLogCategory::GENERAL);
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("BACnet stack shutdown failed: " + std::string(e.what()),
                          DriverLogCategory::GENERAL);
        }
    }
}

void BACnetDriver::WorkerThread() {
    if (logger_) {
        logger_->Info("BACnet worker thread started", DriverLogCategory::GENERAL);
    }
    
    while (!stop_threads_.load()) {
        try {
            // BACnet 메시지 처리 루프
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->Error("Worker thread error: " + std::string(e.what()), 
                              DriverLogCategory::GENERAL);
            }
        }
    }
    
    if (logger_) {
        logger_->Info("BACnet worker thread stopped", DriverLogCategory::GENERAL);
    }
}

void BACnetDriver::DiscoveryThread() {
    if (logger_) {
        logger_->Info("BACnet discovery thread started", DriverLogCategory::DIAGNOSTICS);
    }
    
    while (!stop_threads_.load()) {
        try {
            if (config_.who_is_enabled) {
                SendWhoIs();
            }
            
            std::unique_lock<std::mutex> lock(request_mutex_);
            request_cv_.wait_for(lock, std::chrono::milliseconds(config_.who_is_interval));
            
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->Error("Discovery thread error: " + std::string(e.what()), 
                              DriverLogCategory::DIAGNOSTICS);
            }
        }
    }
    
    if (logger_) {
        logger_->Info("BACnet discovery thread stopped", DriverLogCategory::DIAGNOSTICS);
    }
}





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
                if (std::holds_alternative<int32_t>(data_value)) {
                    bacnet_value.tag = BACNET_APPLICATION_TAG_SIGNED_INT;
                    bacnet_value.type.Signed_Int = std::get<int32_t>(data_value);
                    return true;
                }
                break;
                
            case DataType::UINT16:
            case DataType::UINT32:
                if (std::holds_alternative<uint32_t>(data_value)) {
                    bacnet_value.tag = BACNET_APPLICATION_TAG_UNSIGNED_INT;
                    bacnet_value.type.Unsigned_Int = std::get<uint32_t>(data_value);
                    return true;
                }
                break;
                
            case DataType::FLOAT32:
                if (std::holds_alternative<float>(data_value)) {
                    bacnet_value.tag = BACNET_APPLICATION_TAG_REAL;
                    bacnet_value.type.Real = std::get<float>(data_value);
                    return true;
                } else if (std::holds_alternative<double>(data_value)) {
                    bacnet_value.tag = BACNET_APPLICATION_TAG_REAL;
                    bacnet_value.type.Real = static_cast<float>(std::get<double>(data_value));
                    return true;
                }
                break;
                
            case DataType::STRING:
                if (std::holds_alternative<std::string>(data_value)) {
                    bacnet_value.tag = BACNET_APPLICATION_TAG_CHARACTER_STRING;
                    std::string str_val = std::get<std::string>(data_value);
                    strncpy(bacnet_value.type.Character_String.value, str_val.c_str(), 255);
                    bacnet_value.type.Character_String.length = (str_val.length() < 255) ? str_val.length() : 255;
                    return true;
                }
                break;
                
            default:
                break;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Value conversion error: " + std::string(e.what()),
                          DriverLogCategory::DATA_PROCESSING);
        }
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
                // 실제 BACnet 함수 사용시 활성화
                // data_value = std::string(characterstring_value(&bacnet_value.type.Character_String));
                data_value = std::string(bacnet_value.type.Character_String.value);
                return true;
                
            default:
                return false;
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Value conversion error: " + std::string(e.what()),
                          DriverLogCategory::DATA_PROCESSING);
        }
        return false;
    }
}

bool BACnetDriver::ParseDataPoint(const DataPoint& point, uint32_t& device_id,
                                 BACNET_OBJECT_TYPE& object_type, uint32_t& object_instance,
                                 BACNET_PROPERTY_ID& property_id, uint32_t& array_index) {
    try {
        // address 필드에서 BACnet 객체 정보 파싱
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
        if (logger_) {
            logger_->Error("Data point parsing error: " + std::string(e.what()),
                          DriverLogCategory::DATA_PROCESSING);
        }
        return false;
    }
}

void BACnetDriver::SetError(ErrorCode code, const std::string& message) {
    last_error_ = ErrorInfo(code, message);
    if (logger_) {
        logger_->Error(message, DriverLogCategory::GENERAL);
    }
}

void BACnetDriver::UpdateStatistics(const std::string& /*operation*/, bool success,
                                   std::chrono::milliseconds duration) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    statistics_.total_operations++;
    
    if (success) {
        statistics_.successful_operations++;
    } else {
        statistics_.failed_operations++;
    }
    
    // 응답 시간 통계 업데이트
    double duration_ms = duration.count();
    statistics_.avg_response_time_ms = 
        (statistics_.avg_response_time_ms * (statistics_.total_operations - 1) + duration_ms) /
        statistics_.total_operations;
    
    if (duration_ms > statistics_.max_response_time_ms) {
        statistics_.max_response_time_ms = duration_ms;
    }
    
    if (duration_ms < statistics_.min_response_time_ms || statistics_.min_response_time_ms == 0.0) {
        statistics_.min_response_time_ms = duration_ms;
    }
    
    if (success) {
        statistics_.last_success_time = std::chrono::system_clock::now();
        statistics_.consecutive_failures = 0;
    } else {
        statistics_.last_error_time = std::chrono::system_clock::now();
        statistics_.consecutive_failures++;
    }
}

void BACnetDriver::ParseBACnetConfig(const std::string& config_str) {
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
            }
        }
    }
}

bool BACnetDriver::FindDeviceAddress(uint32_t device_id, BACNET_ADDRESS& address) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    auto it = device_addresses_.find(device_id);
    if (it != device_addresses_.end()) {
        address = it->second;
        return true;
    }
    
    // 디바이스 주소를 찾을 수 없으면 브로드캐스트 주소 사용
    if (logger_) {
        logger_->Warn("Device address not found for ID " + std::to_string(device_id) + 
                     ", using broadcast", DriverLogCategory::COMMUNICATION);
    }
    
    // 기본 브로드캐스트 주소 설정
    address.mac_len = 6;
    address.mac[0] = 255;  // 브로드캐스트
    address.mac[1] = 255;
    address.mac[2] = 255;
    address.mac[3] = 255;
    address.mac[4] = 0xBA;
    address.mac[5] = 0xC0;
    address.net = 0;       // 로컬 네트워크
    address.len = 0;
    
    return true;  // 브로드캐스트로라도 시도
}

bool BACnetDriver::CollectIAmResponses() {
    // I-Am 응답 수집 (3초 대기)
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(3);
    
    while (std::chrono::steady_clock::now() - start_time < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 실제로는 datalink_receive()로 들어오는 I-Am 메시지를 처리해야 함
        // 현재는 단순히 대기만 함
    }
    
    if (logger_) {
        std::lock_guard<std::mutex> lock(devices_mutex_);
        logger_->Info("Device discovery completed. Found " + 
                     std::to_string(discovered_devices_.size()) + " devices",
                     DriverLogCategory::DIAGNOSTICS);
    }
    
    return true;
}

bool BACnetDriver::WaitForReadResponse(uint8_t invoke_id, BACNET_APPLICATION_DATA_VALUE& value, 
                                      uint32_t timeout_ms) {
    // 응답 대기 로직 (단순화된 버전)
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(timeout_ms);
    
    while (std::chrono::steady_clock::now() - start_time < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // 실제로는 TSM (Transaction State Machine)에서 응답을 확인해야 함
        // 현재는 임시로 더미 값 반환
        if (std::chrono::steady_clock::now() - start_time > std::chrono::milliseconds(100)) {
            // 100ms 후 성공한 것으로 가정하고 더미 값 반환
            value.tag = BACNET_APPLICATION_TAG_REAL;
            value.type.Real = 25.5f + (invoke_id % 10); // 약간의 변화를 위해
            
            if (logger_) {
                logger_->Debug("Read response received for invoke_id " + std::to_string(invoke_id),
                              DriverLogCategory::COMMUNICATION);
            }
            return true;
        }
    }
    
    // 타임아웃
    if (logger_) {
        logger_->Error("Read response timeout for invoke_id " + std::to_string(invoke_id),
                      DriverLogCategory::COMMUNICATION);
    }
    return false;
}

bool BACnetDriver::WaitForWriteResponse(uint8_t invoke_id, uint32_t timeout_ms) {
    // 쓰기 응답 대기 (단순화된 버전)
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(timeout_ms);
    
    while (std::chrono::steady_clock::now() - start_time < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // 100ms 후 성공한 것으로 가정
        if (std::chrono::steady_clock::now() - start_time > std::chrono::milliseconds(100)) {
            if (logger_) {
                logger_->Debug("Write response received for invoke_id " + std::to_string(invoke_id),
                              DriverLogCategory::COMMUNICATION);
            }
            return true;
        }
    }
    
    // 타임아웃
    if (logger_) {
        logger_->Error("Write response timeout for invoke_id " + std::to_string(invoke_id),
                      DriverLogCategory::COMMUNICATION);
    }
    return false;
}

uint8_t PulseOne::Drivers::BACnetDriver::GetNextInvokeID() {
    static std::atomic<uint8_t> invoke_id_counter{1};
    uint8_t id = invoke_id_counter.fetch_add(1);
    if (id == 0) {
        id = invoke_id_counter.fetch_add(1); // 0은 사용하지 않음
    }
    return id;
}

bool BACnetDriver::SendWhoIs(uint32_t low_device_id, uint32_t high_device_id) {
    if (!IsConnected()) {
        return false;
    }
    
    try {
        if (logger_) {
            logger_->Info("Sending Who-Is request (range: " + 
                         std::to_string(low_device_id) + "-" + std::to_string(high_device_id) + ")",
                         DriverLogCategory::DIAGNOSTICS);
        }
        
        // TODO: 실제 BACnet Who-Is 전송
        // Send_WhoIs(low_device_id, high_device_id);
        
        // 더미 디바이스 몇 개 추가 (테스트용)
        std::lock_guard<std::mutex> lock(devices_mutex_);
        uint32_t max_id = (low_device_id + 2 < high_device_id) ? low_device_id + 2 : high_device_id;
        for (uint32_t i = low_device_id; i <= max_id; i++) {
            BACnetDeviceInfo device;
            device.device_id = i;
            device.device_name = "Device_" + std::to_string(i);
            device.ip_address = "192.168.1." + std::to_string(100 + i % 50);
            device.port = 47808;
            device.max_apdu_length = 1476;
            device.segmentation_supported = true;
            device.last_seen = std::chrono::system_clock::now();
            
            discovered_devices_[i] = device;
        }
        
        if (logger_) {
            logger_->Info("Who-Is completed. Found " + std::to_string(discovered_devices_.size()) + " devices",
                         DriverLogCategory::DIAGNOSTICS);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Who-Is failed: " + std::string(e.what()), DriverLogCategory::DIAGNOSTICS);
        }
        return false;
    }
}

bool BACnetDriver::ReadProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                               uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                               uint32_t array_index, BACNET_APPLICATION_DATA_VALUE& value) {
    try {
        if (logger_) {
            logger_->Debug("Reading property: Device=" + std::to_string(device_id) + 
                          " Object=" + std::to_string(object_type) + ":" + std::to_string(object_instance) +
                          " Property=" + std::to_string(property_id),
                          DriverLogCategory::COMMUNICATION);
        }
        
        // TODO: 실제 BACnet Read Property 구현
        // uint8_t invoke_id = GetNextInvokeID();
        // int result = Send_Read_Property_Request(device_id, object_type, object_instance, property_id, array_index);
        
        // 더미 값 생성 (객체 타입에 따라)
        switch (object_type) {
            case OBJECT_ANALOG_INPUT:
            case OBJECT_ANALOG_OUTPUT:
            case OBJECT_ANALOG_VALUE:
                value.tag = BACNET_APPLICATION_TAG_REAL;
                value.type.Real = 20.0f + (device_id % 10) + (object_instance % 5);
                break;
                
            case OBJECT_BINARY_INPUT:
            case OBJECT_BINARY_OUTPUT:
            case OBJECT_BINARY_VALUE:
                value.tag = BACNET_APPLICATION_TAG_BOOLEAN;
                value.type.Boolean = (device_id + object_instance) % 2;
                break;
                
            default:
                value.tag = BACNET_APPLICATION_TAG_UNSIGNED_INT;
                value.type.Unsigned_Int = device_id * 1000 + object_instance;
                break;
        }
        
        (void)property_id; (void)array_index; // 경고 제거
        
        if (logger_) {
            logger_->Debug("Read property successful", DriverLogCategory::COMMUNICATION);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Read property failed: " + std::string(e.what()),
                          DriverLogCategory::DATA_PROCESSING);
        }
        return false;
    }
}

bool BACnetDriver::WriteProperty(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                                uint32_t object_instance, BACNET_PROPERTY_ID property_id,
                                uint32_t array_index, const BACNET_APPLICATION_DATA_VALUE& value) {
    try {
        if (logger_) {
            logger_->Debug("Writing property: Device=" + std::to_string(device_id) + 
                          " Object=" + std::to_string(object_type) + ":" + std::to_string(object_instance) +
                          " Property=" + std::to_string(property_id),
                          DriverLogCategory::COMMUNICATION);
        }
        
        // TODO: 실제 BACnet Write Property 구현
        // uint8_t invoke_id = GetNextInvokeID();
        // int result = Send_Write_Property_Request(device_id, object_type, object_instance, property_id, &value, BACNET_NO_PRIORITY, array_index);
        
        // 더미 구현 - 항상 성공
        (void)device_id; (void)object_type; (void)object_instance; 
        (void)property_id; (void)array_index; (void)value; // 경고 제거
        
        if (logger_) {
            logger_->Debug("Write property successful", DriverLogCategory::COMMUNICATION);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Write property failed: " + std::string(e.what()),
                          DriverLogCategory::DATA_PROCESSING);
        }
        return false;
    }
}


// =============================================================================
// BACnet GetDeviceObjects 메서드 구현
// src/Drivers/Bacnet/BACnetDriver.cpp에 추가할 코드
// =============================================================================

std::vector<BACnetObjectInfo> BACnetDriver::GetDeviceObjects(uint32_t device_id) {
    std::vector<BACnetObjectInfo> objects;
    
    try {
        if (logger_) {
            logger_->Info("Starting object discovery for device " + std::to_string(device_id),
                         DriverLogCategory::DIAGNOSTICS);
        }
        
        // 1. 디바이스가 발견된 목록에 있는지 확인
        {
            std::lock_guard<std::mutex> lock(devices_mutex_);
            auto device_iter = discovered_devices_.find(device_id);
            if (device_iter == discovered_devices_.end()) {
                SetError(ErrorCode::DEVICE_NOT_FOUND, 
                        "Device " + std::to_string(device_id) + " not found. Run Who-Is first.");
                return objects;
            }
        }
        
        // 2. Device Object의 Object_List Property 읽기
        BACNET_APPLICATION_DATA_VALUE object_list_value;
        bool success = ReadProperty(device_id, OBJECT_DEVICE, device_id, 
                                   PROP_OBJECT_LIST, BACNET_ARRAY_ALL, object_list_value);
        
        if (!success) {
            // Object_List를 지원하지 않는 경우, 표준 객체 타입들을 스캔
            return ScanStandardObjects(device_id);
        }
        
        // 3. Object_List에서 객체들 파싱
        objects = ParseObjectList(device_id, object_list_value);
        
        // 4. 각 객체의 기본 속성들 읽기 (비동기로 처리)
        EnrichObjectProperties(objects);
        
        if (logger_) {
            logger_->Info("Object discovery completed for device " + std::to_string(device_id) + 
                         ". Found " + std::to_string(objects.size()) + " objects",
                         DriverLogCategory::DIAGNOSTICS);
        }
        
        return objects;
        
    } catch (const std::exception& e) {
        SetError(ErrorCode::DEVICE_ERROR, 
                "Object discovery failed for device " + std::to_string(device_id) + 
                ": " + std::string(e.what()));
        if (logger_) {
            logger_->Error("Object discovery exception: " + std::string(e.what()),
                          DriverLogCategory::DIAGNOSTICS);
        }
        return objects;
    }
}
// =============================================================================
// 헬퍼 메서드들 (private 섹션에 추가)
// =============================================================================

std::vector<BACnetObjectInfo> BACnetDriver::ScanStandardObjects(uint32_t device_id) {
    std::vector<BACnetObjectInfo> objects;
    
    // 표준 BACnet 객체 타입들 (존재하는 것들만)
    std::vector<BACNET_OBJECT_TYPE> standard_types = {
        OBJECT_ANALOG_INPUT,
        OBJECT_ANALOG_OUTPUT,
        OBJECT_ANALOG_VALUE,
        OBJECT_BINARY_INPUT,
        OBJECT_BINARY_OUTPUT,
        OBJECT_BINARY_VALUE,
        OBJECT_MULTI_STATE_INPUT,
        OBJECT_MULTI_STATE_OUTPUT,
        OBJECT_MULTI_STATE_VALUE,
        OBJECT_DEVICE,
        OBJECT_SCHEDULE,
        OBJECT_CALENDAR,
        OBJECT_NOTIFICATION_CLASS,
        OBJECT_LOOP,
        OBJECT_PROGRAM,
        OBJECT_FILE,
        OBJECT_AVERAGING,
        OBJECT_TRENDLOG,  // OBJECT_TREND_LOG → OBJECT_TRENDLOG
        OBJECT_LIFE_SAFETY_POINT,
        OBJECT_LIFE_SAFETY_ZONE
    };
    
    if (logger_) {
        logger_->Debug("Scanning standard object types for device " + std::to_string(device_id),
                      DriverLogCategory::DIAGNOSTICS);
    }
    
    // 각 객체 타입에 대해 인스턴스 스캔 (1-1000 범위)
    for (auto object_type : standard_types) {
        ScanObjectInstances(device_id, object_type, objects);
    }
    
    return objects;
}

void BACnetDriver::ScanObjectInstances(uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                                      std::vector<BACnetObjectInfo>& objects) {
    const uint32_t MAX_INSTANCE_SCAN = 1000;  // 실용적인 스캔 범위
    const uint32_t BATCH_SIZE = 50;           // 배치 크기
    
    for (uint32_t instance = 1; instance <= MAX_INSTANCE_SCAN; instance += BATCH_SIZE) {
        uint32_t end_instance = (instance + BATCH_SIZE - 1 < MAX_INSTANCE_SCAN) ? 
                                instance + BATCH_SIZE - 1 : MAX_INSTANCE_SCAN;
        
        // 배치 단위로 객체 존재 여부 확인
        for (uint32_t i = instance; i <= end_instance; ++i) {
            BACNET_APPLICATION_DATA_VALUE value;
            
            // Object_Name Property 읽기 시도 (가장 기본적인 속성)
            bool exists = ReadProperty(device_id, object_type, i, PROP_OBJECT_NAME, 
                                     BACNET_ARRAY_ALL, value);
            
            if (exists) {
                BACnetObjectInfo obj_info;
                obj_info.object_type = object_type;
                obj_info.object_instance = i;
                obj_info.property_id = PROP_PRESENT_VALUE;  // 기본값
                obj_info.array_index = BACNET_ARRAY_ALL;
                obj_info.quality = DataQuality::GOOD;
                obj_info.timestamp = std::chrono::system_clock::now();
                
                // Object_Name에서 이름 추출
                if (value.tag == BACNET_APPLICATION_TAG_CHARACTER_STRING) {
                    obj_info.object_name = std::string(value.type.Character_String.value,
                                                      value.type.Character_String.length);
                } else {
                    obj_info.object_name = GetObjectTypeName(object_type) + "_" + std::to_string(i);
                }
                
                objects.push_back(obj_info);
                
                if (logger_) {
                    logger_->Debug("Found object: " + obj_info.object_name + 
                                  " (" + GetObjectTypeName(object_type) + ":" + std::to_string(i) + ")",
                                  DriverLogCategory::DIAGNOSTICS);
                }
            }
        }
        
        // CPU 부하 방지를 위한 짧은 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

std::vector<BACnetObjectInfo> BACnetDriver::ParseObjectList(uint32_t device_id,
                                                           const BACNET_APPLICATION_DATA_VALUE& object_list) {
    std::vector<BACnetObjectInfo> objects;
    
    try {
        // Object_List는 Object_Identifier의 배열
        if (object_list.tag != BACNET_APPLICATION_TAG_OBJECT_ID) {
            if (logger_) {
                logger_->Warn("Object_List property has unexpected data type",
                              DriverLogCategory::DIAGNOSTICS);
            }
            return objects;
        }
        
        // 단일 객체 식별자인 경우
        BACnetObjectInfo obj_info;
        obj_info.object_type = (BACNET_OBJECT_TYPE)object_list.type.Object_Id.type;
        obj_info.object_instance = object_list.type.Object_Id.instance;
        obj_info.property_id = PROP_PRESENT_VALUE;
        obj_info.array_index = BACNET_ARRAY_ALL;
        obj_info.quality = DataQuality::GOOD;
        obj_info.timestamp = std::chrono::system_clock::now();
        
        // 객체 이름 읽기
        BACNET_APPLICATION_DATA_VALUE name_value;
        if (ReadProperty(device_id, obj_info.object_type, obj_info.object_instance,
                        PROP_OBJECT_NAME, BACNET_ARRAY_ALL, name_value)) {
            if (name_value.tag == BACNET_APPLICATION_TAG_CHARACTER_STRING) {
                obj_info.object_name = std::string(name_value.type.Character_String.value,
                                                  name_value.type.Character_String.length);
            }
        }
        
        if (obj_info.object_name.empty()) {
            obj_info.object_name = GetObjectTypeName(obj_info.object_type) + "_" + 
                                  std::to_string(obj_info.object_instance);
        }
        
        objects.push_back(obj_info);
        
        if (logger_) {
            logger_->Debug("Parsed object from Object_List: " + obj_info.object_name,
                          DriverLogCategory::DIAGNOSTICS);
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Error parsing Object_List: " + std::string(e.what()),
                          DriverLogCategory::DIAGNOSTICS);
        }
    }
    
    return objects;
}

void BACnetDriver::EnrichObjectProperties(std::vector<BACnetObjectInfo>& objects) {
    const size_t MAX_CONCURRENT_READS = 10;  // 동시 읽기 제한
    
    if (logger_) {
        logger_->Debug("Enriching properties for " + std::to_string(objects.size()) + " objects",
                      DriverLogCategory::DIAGNOSTICS);
    }
    
    // 객체들을 배치로 처리
    for (size_t i = 0; i < objects.size(); i += MAX_CONCURRENT_READS) {
        size_t end_idx = (i + MAX_CONCURRENT_READS < objects.size()) ? 
                         i + MAX_CONCURRENT_READS : objects.size();
        
        // 현재 배치의 객체들에 대해 Present_Value 읽기
        for (size_t j = i; j < end_idx; ++j) {
            auto& obj = objects[j];
            
            // Present_Value 속성 읽기 시도
            BACNET_APPLICATION_DATA_VALUE present_value;
            if (ReadProperty(obj.object_instance, obj.object_type, obj.object_instance,
                           PROP_PRESENT_VALUE, BACNET_ARRAY_ALL, present_value)) {
                obj.value = present_value;
                obj.property_id = PROP_PRESENT_VALUE;
                obj.quality = DataQuality::GOOD;
            } else {
                // Present_Value가 없는 경우 Object_Name으로 대체
                BACNET_APPLICATION_DATA_VALUE name_value;
                if (ReadProperty(obj.object_instance, obj.object_type, obj.object_instance,
                               PROP_OBJECT_NAME, BACNET_ARRAY_ALL, name_value)) {
                    obj.value = name_value;
                    obj.property_id = PROP_OBJECT_NAME;
                    obj.quality = DataQuality::GOOD;
                } else {
                    obj.quality = DataQuality::BAD;
                }
            }
            
            obj.timestamp = std::chrono::system_clock::now();
        }
        
        // 배치 간 대기 (네트워크 부하 방지)
        if (end_idx < objects.size()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

std::string BACnetDriver::GetObjectTypeName(BACNET_OBJECT_TYPE type) const {
    switch (type) {
        case OBJECT_ANALOG_INPUT:       return "Analog Input";
        case OBJECT_ANALOG_OUTPUT:      return "Analog Output";
        case OBJECT_ANALOG_VALUE:       return "Analog Value";
        case OBJECT_BINARY_INPUT:       return "Binary Input";
        case OBJECT_BINARY_OUTPUT:      return "Binary Output";
        case OBJECT_BINARY_VALUE:       return "Binary Value";
        case OBJECT_MULTI_STATE_INPUT:  return "Multi-state Input";
        case OBJECT_MULTI_STATE_OUTPUT: return "Multi-state Output";
        case OBJECT_MULTI_STATE_VALUE:  return "Multi-state Value";
        case OBJECT_DEVICE:             return "Device";
        case OBJECT_SCHEDULE:           return "Schedule";
        case OBJECT_CALENDAR:           return "Calendar";
        case OBJECT_NOTIFICATION_CLASS: return "Notification Class";
        case OBJECT_LOOP:               return "Loop";
        case OBJECT_PROGRAM:            return "Program";
        case OBJECT_FILE:               return "File";
        case OBJECT_AVERAGING:          return "Averaging";
        case OBJECT_TRENDLOG:           return "Trend Log";
        case OBJECT_LIFE_SAFETY_POINT:  return "Life Safety Point";
        case OBJECT_LIFE_SAFETY_ZONE:   return "Life Safety Zone";
        default:                        return "Unknown Object (" + std::to_string(type) + ")";
    }
}

std::string BACnetDriver::GetPropertyName(BACNET_PROPERTY_ID prop) const {
    switch (prop) {
        case PROP_PRESENT_VALUE:        return "Present Value";
        case PROP_OBJECT_NAME:          return "Object Name";
        case PROP_OBJECT_TYPE:          return "Object Type";
        case PROP_OBJECT_IDENTIFIER:    return "Object Identifier";
        case PROP_DESCRIPTION:          return "Description";
        case PROP_UNITS:                return "Units";
        case PROP_OUT_OF_SERVICE:       return "Out of Service";
        case PROP_STATUS_FLAGS:         return "Status Flags";
        case PROP_RELIABILITY:          return "Reliability";
        case PROP_PRIORITY_ARRAY:       return "Priority Array";
        case PROP_RELINQUISH_DEFAULT:   return "Relinquish Default";
        case PROP_COV_INCREMENT:        return "COV Increment";
        case PROP_TIME_DELAY:           return "Time Delay";
        case PROP_NOTIFICATION_CLASS:   return "Notification Class";
        case PROP_HIGH_LIMIT:           return "High Limit";
        case PROP_LOW_LIMIT:            return "Low Limit";
        case PROP_DEADBAND:             return "Deadband";
        case PROP_LIMIT_ENABLE:         return "Limit Enable";
        case PROP_EVENT_ENABLE:         return "Event Enable";
        case PROP_ACKED_TRANSITIONS:    return "Acked Transitions";
        case PROP_NOTIFY_TYPE:          return "Notify Type";
        case PROP_EVENT_TIME_STAMPS:    return "Event Time Stamps";
        case PROP_OBJECT_LIST:          return "Object List";
        case PROP_MAX_APDU_LENGTH_ACCEPTED: return "Max APDU Length Accepted";
        case PROP_SEGMENTATION_SUPPORTED:   return "Segmentation Supported";
        case PROP_VENDOR_NAME:          return "Vendor Name";
        case PROP_VENDOR_IDENTIFIER:    return "Vendor Identifier";
        case PROP_MODEL_NAME:           return "Model Name";
        case PROP_FIRMWARE_REVISION:    return "Firmware Revision";
        case PROP_APPLICATION_SOFTWARE_VERSION: return "Application Software Version";
        case PROP_PROTOCOL_VERSION:     return "Protocol Version";
        case PROP_PROTOCOL_REVISION:    return "Protocol Revision";
        case PROP_PROTOCOL_SERVICES_SUPPORTED: return "Protocol Services Supported";
        case PROP_PROTOCOL_OBJECT_TYPES_SUPPORTED: return "Protocol Object Types Supported";
        case PROP_SYSTEM_STATUS:        return "System Status";
        case PROP_DATABASE_REVISION:    return "Database Revision";
        default:                        return "Unknown Property (" + std::to_string(prop) + ")";
    }
}

} // namespace Drivers
} // namespace PulseOne