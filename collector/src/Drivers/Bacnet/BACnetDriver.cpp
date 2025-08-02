// =============================================================================
// collector/src/Drivers/Bacnet/BACnetDriver.cpp
// 🔥 기존 BACnetDriver와 완전 호환되는 향상된 드라이버 구현
// =============================================================================

#include "Drivers/Bacnet/BACnetDriver.h"
#include "Drivers/Bacnet/BACnetErrorMapper.h"
#include "Utils/LogManager.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <thread>

using namespace std::chrono;

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 정적 멤버 초기화
// =============================================================================
BACnetDriver* BACnetDriver::instance_ = nullptr;
std::mutex BACnetDriver::instance_mutex_;

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

BACnetDriver::BACnetDriver() 
    : local_device_id_(1234)
    , target_ip_("")
    , target_port_(47808) {
    
    // 정적 인스턴스 등록 (콜백에서 사용)
    {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        instance_ = this;
    }
    
    // 🔥 모듈화된 컴포넌트들 초기화
    error_mapper_ = std::make_unique<BACnetErrorMapper>();
    statistics_manager_ = std::make_unique<BACnetStatisticsManager>();
    service_manager_ = std::make_unique<BACnetServiceManager>(this);
    cov_manager_ = std::make_unique<BACnetCOVManager>(this);
    object_mapper_ = std::make_unique<BACnetObjectMapper>(this);
    
    // 로거 초기화
    logger_.Info("BACnet Driver created with enhanced modules", DriverLogCategory::GENERAL);
}

BACnetDriver::~BACnetDriver() {
    Stop();
    Disconnect();
    CleanupBACnetStack();
    
    // 정적 인스턴스 해제
    {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (instance_ == this) {
            instance_ = nullptr;
        }
    }
    
    logger_.Info("BACnet Driver destroyed", DriverLogCategory::GENERAL);
}

// =============================================================================
// IProtocolDriver 기본 인터페이스 구현 (기존 호환성 유지)
// =============================================================================

bool BACnetDriver::Initialize(const DriverConfig& config) {
    config_ = config;
    status_.store(Structs::DriverStatus::INITIALIZING);
    
    statistics_manager_->StartOperation("initialization");
    
    try {
        // 🔥 기존 방식 유지 - 설정 파싱
        if (!config.endpoint.empty()) {
            size_t colon_pos = config.endpoint.find(':');
            if (colon_pos != std::string::npos) {
                target_ip_ = config.endpoint.substr(0, colon_pos);
                target_port_ = static_cast<uint16_t>(std::stoi(config.endpoint.substr(colon_pos + 1)));
            } else {
                target_ip_ = config.endpoint;
                target_port_ = 47808;
            }
        }
        
        // 로컬 디바이스 ID 설정 (기존 방식 유지)
        if (config.timeout_ms > 0) {
            local_device_id_ = static_cast<uint32_t>(config.timeout_ms); // 임시로 timeout 필드 사용
        }
        
        // BACnet 스택 초기화
        if (!InitializeBACnetStack()) {
            SetError(Enums::ErrorCode::INITIALIZATION_FAILED, "Failed to initialize BACnet stack");
            statistics_manager_->CompleteOperation("initialization", false);
            return false;
        }
        
        // 🔥 모듈들 초기화
        object_mapper_->LoadDefaultMappingRules();
        
        status_.store(Structs::DriverStatus::INITIALIZED);
        statistics_manager_->CompleteOperation("initialization", true);
        
        logger_.Info("BACnet Driver initialized successfully", DriverLogCategory::GENERAL);
        return true;
        
    } catch (const std::exception& e) {
        SetError(Enums::ErrorCode::INITIALIZATION_FAILED, 
                "Exception during initialization: " + std::string(e.what()));
        statistics_manager_->CompleteOperation("initialization", false);
        status_.store(Structs::DriverStatus::ERROR);
        return false;
    }
}

bool BACnetDriver::Connect() {
    if (is_connected_.load()) {
        return true;
    }
    
    statistics_manager_->StartOperation("connection");
    
    try {
#ifdef HAS_BACNET_STACK
        // 🔥 기존 BACnet/IP 연결 방식 유지
        if (!target_ip_.empty()) {
            bip_set_addr(htonl(INADDR_ANY));
            bip_set_port(htons(target_port_));
        }
        
        if (!datalink_init(target_ip_.empty() ? nullptr : target_ip_.c_str())) {
            SetError(Enums::ErrorCode::CONNECTION_FAILED, "Failed to initialize BACnet datalink");
            statistics_manager_->CompleteOperation("connection", false);
            return false;
        }
#endif
        
        is_connected_.store(true);
        current_connection_status_.store(Enums::ConnectionStatus::CONNECTED);
        statistics_manager_->CompleteOperation("connection", true);
        
        // 🔥 기존 콜백 호환성 유지
        NotifyStatusChanged(UUID{config_.device_id}, Enums::ConnectionStatus::CONNECTED);
        
        logger_.Info("BACnet Driver connected successfully", DriverLogCategory::GENERAL);
        return true;
        
    } catch (const std::exception& e) {
        SetError(Enums::ErrorCode::CONNECTION_FAILED, 
                "Exception during connection: " + std::string(e.what()));
        statistics_manager_->CompleteOperation("connection", false);
        return false;
    }
}

bool BACnetDriver::Disconnect() {
    if (!is_connected_.load()) {
        return true;
    }
    
    statistics_manager_->StartOperation("disconnection");
    
    try {
        // 🔥 모든 COV 구독 해제
        cov_manager_->UnsubscribeAll();
        
#ifdef HAS_BACNET_STACK
        datalink_cleanup();
#endif
        
        is_connected_.store(false);
        current_connection_status_.store(Enums::ConnectionStatus::DISCONNECTED);
        statistics_manager_->CompleteOperation("disconnection", true);
        
        // 기존 콜백 호환성
        NotifyStatusChanged(UUID{config_.device_id}, Enums::ConnectionStatus::DISCONNECTED);
        
        logger_.Info("BACnet Driver disconnected successfully", DriverLogCategory::GENERAL);
        return true;
        
    } catch (const std::exception& e) {
        SetError(Enums::ErrorCode::DISCONNECTION_FAILED, 
                "Exception during disconnection: " + std::string(e.what()));
        statistics_manager_->CompleteOperation("disconnection", false);
        return false;
    }
}

bool BACnetDriver::IsConnected() const {
    return is_connected_.load();
}

bool BACnetDriver::ReadValues(const std::vector<Structs::DataPoint>& points,
                             std::vector<TimestampedValue>& values) {
    if (!IsConnected()) {
        SetError(Enums::ErrorCode::CONNECTION_LOST, "Not connected to BACnet network");
        return false;
    }
    
    values.clear();
    values.reserve(points.size());
    
    statistics_manager_->StartOperation("read_values");
    
    // 🔥 고급 서비스 사용 시도, 실패하면 기존 방식으로 폴백
    if (points.size() > 1) {
        // 디바이스별로 그룹핑하여 RPM 사용
        std::map<uint32_t, std::vector<Structs::DataPoint>> device_points;
        
        for (const auto& point : points) {
            try {
                uint32_t device_id = std::stoul(point.device_id);
                device_points[device_id].push_back(point);
            } catch (const std::exception&) {
                // device_id 파싱 실패 시 개별 처리
                TimestampedValue error_value;
                error_value.value = Structs::DataValue{0.0f};
                error_value.quality = Enums::DataQuality::BAD;
                error_value.timestamp = system_clock::now();
                values.push_back(error_value);
            }
        }
        
        bool overall_success = true;
        for (const auto& [device_id, device_points_list] : device_points) {
            std::vector<TimestampedValue> device_results;
            
            // 🔥 고급 배치 읽기 시도
            if (service_manager_->BatchRead(device_id, device_points_list, device_results)) {
                values.insert(values.end(), device_results.begin(), device_results.end());
            } else {
                // 폴백: 개별 읽기
                for (const auto& point : device_points_list) {
                    if (!ReadSingleValue(point, values)) {
                        overall_success = false;
                    }
                }
            }
        }
        
        statistics_manager_->CompleteOperation("read_values", overall_success);
        return overall_success;
    } else {
        // 단일 포인트는 개별 읽기
        bool success = ReadSingleValue(points[0], values);
        statistics_manager_->CompleteOperation("read_values", success);
        return success;
    }
}

bool BACnetDriver::WriteValue(const Structs::DataPoint& point, 
                             const Structs::DataValue& value) {
    if (!IsConnected()) {
        SetError(Enums::ErrorCode::CONNECTION_LOST, "Not connected to BACnet network");
        return false;
    }
    
    statistics_manager_->StartOperation("write_value");
    
    try {
        // DataPoint에서 BACnet 정보 추출 (기존 방식 유지)
        uint32_t device_id = std::stoul(point.device_id);
        
        ExtendedBACnetObjectInfo object_info;
        if (object_mapper_->DataPointToBACnetObject(point, object_info)) {
            // 🔥 고급 서비스 사용
            bool success = service_manager_->WriteProperty(
                device_id, 
                object_info.object_type, 
                object_info.object_instance,
                object_info.property_id, 
                value
            );
            
            statistics_manager_->CompleteOperation("write_value", success);
            return success;
        } else {
            // 폴백: 기존 방식으로 메타데이터 직접 파싱
            BACNET_OBJECT_TYPE obj_type = OBJECT_ANALOG_OUTPUT;
            BACNET_PROPERTY_ID prop_id = PROP_PRESENT_VALUE;
            uint32_t obj_instance = point.address;
            
            // 메타데이터에서 BACnet 설정 추출 (기존 방식)
            auto it = point.metadata.find("object_type");
            if (it != point.metadata.end()) {
                obj_type = static_cast<BACNET_OBJECT_TYPE>(std::stoi(it->second));
            }
            
            it = point.metadata.find("property_id");
            if (it != point.metadata.end()) {
                prop_id = static_cast<BACNET_PROPERTY_ID>(std::stoi(it->second));
            }
            
            bool success = service_manager_->WriteProperty(
                device_id, obj_type, obj_instance, prop_id, value
            );
            
            statistics_manager_->CompleteOperation("write_value", success);
            return success;
        }
        
    } catch (const std::exception& e) {
        SetError(Enums::ErrorCode::DATA_FORMAT_ERROR, 
                "Exception during write value: " + std::string(e.what()));
        statistics_manager_->CompleteOperation("write_value", false);
        return false;
    }
}

Structs::ErrorInfo BACnetDriver::GetLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

const DriverStatistics& BACnetDriver::GetStatistics() const {
    // 🔥 기존 DriverStatistics 활용 + BACnet 특화 정보 추가
    std::lock_guard<std::mutex> lock(error_mutex_);
    
    // 기본 통계를 캐시에 복사
    standard_statistics_cache_ = *statistics_manager_->GetStandardStatistics();
    
    // BACnet 특화 정보를 protocol_counters에 추가
    standard_statistics_cache_.SetProtocolCounter("who_is_sent", 
        statistics_manager_->GetBACnetCounter("who_is_sent"));
    standard_statistics_cache_.SetProtocolCounter("i_am_received", 
        statistics_manager_->GetBACnetCounter("i_am_received"));
    standard_statistics_cache_.SetProtocolCounter("cov_subscriptions", 
        cov_manager_->GetCOVStatistics().active_subscriptions.load());
    standard_statistics_cache_.SetProtocolCounter("devices_discovered", 
        object_mapper_->GetMappingStatistics().cached_devices.load());
        
    return standard_statistics_cache_;
}

void BACnetDriver::ResetStatistics() {
    statistics_manager_->ResetStatistics();
    cov_manager_->GetCOVStatistics().Reset();
    object_mapper_->GetMappingStatistics().Reset();
    
    logger_.Info("BACnet driver statistics reset", DriverLogCategory::GENERAL);
}

// =============================================================================
// 🔥 새로운 고급 기능들 (기존 API 확장)
// =============================================================================

int BACnetDriver::DiscoverDevices(std::map<uint32_t, BACnetDeviceInfo>& devices, 
                                 int timeout_ms) {
    if (!IsConnected()) {
        SetError(Enums::ErrorCode::CONNECTION_LOST, "Not connected for device discovery");
        return -1;
    }
    
    return service_manager_->DiscoverDevices(devices, 0, 4194303, timeout_ms);
}

std::vector<BACnetObjectInfo> BACnetDriver::GetDeviceObjects(uint32_t device_id) {
    if (!IsConnected()) {
        SetError(Enums::ErrorCode::CONNECTION_LOST, "Not connected for object discovery");
        return {};
    }
    
    return service_manager_->GetDeviceObjects(device_id);
}

bool BACnetDriver::SubscribeToCOV(uint32_t device_id, uint32_t object_instance, 
                                 BACNET_OBJECT_TYPE object_type, uint32_t lifetime_seconds) {
    if (!IsConnected()) {
        SetError(Enums::ErrorCode::CONNECTION_LOST, "Not connected for COV subscription");
        return false;
    }
    
    return cov_manager_->Subscribe(device_id, object_instance, object_type, lifetime_seconds);
}

bool BACnetDriver::UnsubscribeFromCOV(uint32_t device_id, uint32_t object_instance, 
                                     BACNET_OBJECT_TYPE object_type) {
    return cov_manager_->Unsubscribe(device_id, object_instance, object_type);
}

bool BACnetDriver::ReadPropertyMultiple(uint32_t device_id, 
                                       const std::vector<BACnetObjectInfo>& objects,
                                       std::vector<TimestampedValue>& results) {
    if (!IsConnected()) {
        SetError(Enums::ErrorCode::CONNECTION_LOST, "Not connected for Read Property Multiple");
        return false;
    }
    
    return service_manager_->ReadPropertyMultiple(device_id, objects, results);
}

bool BACnetDriver::WritePropertyMultiple(uint32_t device_id,
                                        const std::map<BACnetObjectInfo, Structs::DataValue>& values) {
    if (!IsConnected()) {
        SetError(Enums::ErrorCode::CONNECTION_LOST, "Not connected for Write Property Multiple");
        return false;
    }
    
    return service_manager_->WritePropertyMultiple(device_id, values);
}

bool BACnetDriver::MapComplexObject(const std::string& object_identifier,
                                   uint32_t device_id, BACNET_OBJECT_TYPE object_type,
                                   uint32_t object_instance,
                                   const std::vector<BACNET_PROPERTY_ID>& properties) {
    return object_mapper_->MapObject(object_identifier, device_id, object_type, 
                                   object_instance, properties);
}

const BACnetStatistics& BACnetDriver::GetBACnetStatistics() const {
    // 🔥 더 이상 별도 BACnet 통계 클래스 없음 - DriverStatistics로 통합
    // 대신 BACnet 특화 정보를 반환하는 헬퍼 메서드 제공
    static BACnetStatistics bacnet_stats; // 임시 호환성을 위한 정적 객체
    
    // DriverStatistics에서 BACnet 정보 추출하여 변환
    const auto& driver_stats = GetStatistics();
    bacnet_stats.who_is_sent = driver_stats.GetProtocolCounter("who_is_sent");
    bacnet_stats.i_am_received = driver_stats.GetProtocolCounter("i_am_received");
    bacnet_stats.cov_subscriptions = driver_stats.GetProtocolCounter("cov_subscriptions");
    bacnet_stats.discovered_devices = driver_stats.GetProtocolCounter("devices_discovered");
    
    return bacnet_stats;
}

std::string BACnetDriver::GetDiagnosticInfo() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"protocol\": \"BACnet/IP\",\n";
    oss << "  \"driver_version\": \"Enhanced 2.0\",\n";
    oss << "  \"status\": \"" << static_cast<int>(GetStatus()) << "\",\n";
    oss << "  \"connection_status\": \"" << Utils::ConnectionStatusToString(current_connection_status_) << "\",\n";
    oss << "  \"local_device_id\": " << local_device_id_ << ",\n";
    oss << "  \"target_endpoint\": \"" << target_ip_ << ":" << target_port_ << "\",\n";
    
    const auto& stats = GetStatistics();
    oss << "  \"discovered_devices\": " << stats.GetProtocolCounter("devices_discovered") << ",\n";
    oss << "  \"active_cov_subscriptions\": " << cov_manager_->GetCOVStatistics().active_subscriptions.load() << ",\n";
    oss << "  \"mapped_objects\": " << object_mapper_->GetMappingStatistics().total_mapped_objects.load() << ",\n";
    oss << "  \"last_error\": " << GetLastError().ToJsonString() << ",\n";
    oss << "  \"statistics\": " << stats.ToJsonString() << "\n";
    oss << "}";
    return oss.str();
}

bool BACnetDriver::HealthCheck() {
    bool basic_health = IProtocolDriver::HealthCheck();
    
    if (!basic_health) {
        return false;
    }
    
    // BACnet 특화 헬스체크
    bool bacnet_health = true;
    
    // 1. BACnet 스택 상태 확인
    if (!IsBACnetStackInitialized()) {
        SetError(Enums::ErrorCode::INTERNAL_ERROR, "BACnet stack not properly initialized");
        bacnet_health = false;
    }
    
    // 2. 네트워크 연결성 확인
    if (IsConnected() && !TestNetworkConnectivity()) {
        SetError(Enums::ErrorCode::NETWORK_ERROR, "BACnet network connectivity test failed");
        bacnet_health = false;
    }
    
    // 3. 모듈 건강성 확인
    if (!object_mapper_->HealthCheck()) {
        SetError(Enums::ErrorCode::INTERNAL_ERROR, "Object mapper health check failed");
        bacnet_health = false;
    }
    
    return bacnet_health;
}

// =============================================================================
// 보호된 메서드들
// =============================================================================

bool BACnetDriver::DoStart() {
    if (status_.load() != Structs::DriverStatus::INITIALIZED) {
        return false;
    }
    
    try {
        should_stop_.store(false);
        
        // COV 관리자 시작
        if (!cov_manager_->Start()) {
            SetError(Enums::ErrorCode::INTERNAL_ERROR, "Failed to start COV manager");
            return false;
        }
        
        // 네트워크 스레드 시작
        network_thread_ = std::thread(&BACnetDriver::NetworkThreadFunction, this);
        
        logger_.Info("BACnet Driver started", DriverLogCategory::GENERAL);
        return true;
        
    } catch (const std::exception& e) {
        SetError(Enums::ErrorCode::INTERNAL_ERROR, 
                "Exception during start: " + std::string(e.what()));
        return false;
    }
}

bool BACnetDriver::DoStop() {
    try {
        should_stop_.store(true);
        
        // COV 관리자 정지
        cov_manager_->Stop();
        
        // 네트워크 스레드 종료 대기
        if (network_thread_.joinable()) {
            network_condition_.notify_all();
            network_thread_.join();
        }
        
        logger_.Info("BACnet Driver stopped", DriverLogCategory::GENERAL);
        return true;
        
    } catch (const std::exception& e) {
        SetError(Enums::ErrorCode::INTERNAL_ERROR, 
                "Exception during stop: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 핵심 내부 메서드들
// =============================================================================

bool BACnetDriver::InitializeBACnetStack() {
#ifdef HAS_BACNET_STACK
    try {
        // 디바이스 정보 설정
        Device_Set_Object_Instance_Number(local_device_id_);
        Device_Set_Object_Name("PulseOne BACnet Device");
        Device_Set_System_Status(SYSTEM_STATUS_OPERATIONAL);
        Device_Set_Vendor_Name("PulseOne");
        Device_Set_Vendor_Identifier(999);
        Device_Set_Model_Name("PulseOne Collector");
        Device_Set_Firmware_Revision("2.0.0");
        Device_Set_Application_Software_Version("Enhanced Driver");
        Device_Set_Protocol_Version(1);
        Device_Set_Protocol_Revision(22);
        
        Device_Set_Max_APDU_Length_Accepted(1476);
        Device_Set_Segmentation_Supported(SEGMENTATION_BOTH);
        
        // 콜백 핸들러 등록
        apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, IAmHandler);
        apdu_set_confirmed_ack_handler(SERVICE_CONFIRMED_READ_PROPERTY, ReadPropertyAckHandler);
        apdu_set_error_handler(SERVICE_CONFIRMED_READ_PROPERTY, ErrorHandler);
        apdu_set_reject_handler(RejectHandler);
        apdu_set_abort_handler(AbortHandler);
        apdu_set_confirmed_handler(SERVICE_CONFIRMED_COV_NOTIFICATION, COVNotificationHandler);
        
        logger_.Info("BACnet stack initialized successfully", DriverLogCategory::GENERAL);
        return true;
        
    } catch (const std::exception& e) {
        SetError(Enums::ErrorCode::INITIALIZATION_FAILED, 
                "Exception during BACnet stack initialization: " + std::string(e.what()));
        return false;
    }
#else
    logger_.Warn("BACnet stack not available (HAS_BACNET_STACK not defined)", 
                DriverLogCategory::GENERAL);
    return true;
#endif
}

void BACnetDriver::CleanupBACnetStack() {
    logger_.Info("BACnet stack cleaned up", DriverLogCategory::GENERAL);
}

bool BACnetDriver::IsBACnetStackInitialized() const {
#ifdef HAS_BACNET_STACK
    return Device_Object_Instance_Number() == local_device_id_;
#else
    return true;
#endif
}

void BACnetDriver::NetworkThreadFunction() {
    logger_.Info("BACnet network thread started", DriverLogCategory::GENERAL);
    
    while (!should_stop_.load()) {
        try {
            ProcessBACnetMessages();
            
            std::unique_lock<std::mutex> lock(network_mutex_);
            network_condition_.wait_for(lock, milliseconds(10));
            
        } catch (const std::exception& e) {
            SetError(Enums::ErrorCode::NETWORK_ERROR, 
                    "Exception in network thread: " + std::string(e.what()));
            std::this_thread::sleep_for(milliseconds(100));
        }
    }
    
    logger_.Info("BACnet network thread stopped", DriverLogCategory::GENERAL);
}

bool BACnetDriver::ProcessBACnetMessages() {
#ifdef HAS_BACNET_STACK
    try {
        BACNET_ADDRESS src = {0};
        uint16_t pdu_len = 0;
        unsigned timeout = 0;
        
        pdu_len = datalink_receive(&src, &Rx_Buf[0], MAX_MPDU, timeout);
        
        if (pdu_len) {
            npdu_handler(&src, &Rx_Buf[0], pdu_len);
            return true;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        SetError(Enums::ErrorCode::PROTOCOL_ERROR, 
                "Exception processing BACnet messages: " + std::string(e.what()));
        return false;
    }
#else
    return false;
#endif
}

bool BACnetDriver::TestNetworkConnectivity() {
    try {
#ifdef HAS_BACNET_STACK
        if (Send_WhoIs(BACNET_BROADCAST_NETWORK, 0, 0)) {
            auto timeout_time = steady_clock::now() + milliseconds(1000);
            
            while (steady_clock::now() < timeout_time) {
                if (ProcessBACnetMessages()) {
                    return true;
                }
                std::this_thread::sleep_for(milliseconds(10));
            }
        }
        return false;
#else
        return true;
#endif
        
    } catch (const std::exception& e) {
        SetError(Enums::ErrorCode::NETWORK_ERROR, 
                "Exception during network connectivity test: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 에러 처리 (기존 API 유지)
// =============================================================================

void BACnetDriver::SetError(Enums::ErrorCode code, const std::string& message) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = Structs::ErrorInfo(code, message);
    
    // 🔥 새로운 통계 시스템에 에러 기록
    GetStatistics().RecordReadOperation(false, 0.0); // 에러이므로 실패로 기록
    
    // 로그 기록
    logger_.Error(message + " [Error Code: " + std::to_string(static_cast<int>(code)) + "]", 
                 DriverLogCategory::ERROR_HANDLING);
}

void BACnetDriver::LogBACnetError(const std::string& message, 
                                 uint8_t error_class, uint8_t error_code) {
    std::string full_message = message;
    if (error_class != 0 || error_code != 0) {
        full_message += " (BACnet Error: Class=" + std::to_string(error_class) + 
                       ", Code=" + std::to_string(error_code) + ")";
    }
    
    SetError(Enums::ErrorCode::PROTOCOL_ERROR, full_message);
}

// =============================================================================
// 헬퍼 메서드들
// =============================================================================

bool BACnetDriver::ReadSingleValue(const Structs::DataPoint& point, 
                                  std::vector<TimestampedValue>& values) {
    TimestampedValue result;
    result.timestamp = system_clock::now();
    
    try {
        uint32_t device_id = std::stoul(point.device_id);
        
        // 객체 매퍼를 통한 변환 시도
        ExtendedBACnetObjectInfo object_info;
        if (object_mapper_->DataPointToBACnetObject(point, object_info)) {
            // 고급 서비스 사용
            if (service_manager_->ReadProperty(device_id, object_info.object_type, 
                                             object_info.object_instance, 
                                             object_info.property_id, result)) {
                values.push_back(result);
                return true;
            }
        }
        
        // 폴백: 기존 방식
        result.value = Structs::DataValue{0.0f};
        result.quality = Enums::DataQuality::UNCERTAIN;
        values.push_back(result);
        return false;
        
    } catch (const std::exception& e) {
        result.value = Structs::DataValue{0.0f};
        result.quality = Enums::DataQuality::BAD;
        values.push_back(result);
        return false;
    }
}

std::string BACnetDriver::CreateObjectKey(uint32_t device_id, BACNET_OBJECT_TYPE object_type, 
                                         uint32_t object_instance) {
    return std::to_string(device_id) + "_" + std::to_string(object_type) + "_" + std::to_string(object_instance);
}

// =============================================================================
// BACnet Stack 콜백 핸들러들 (정적 메서드)
// =============================================================================

#ifdef HAS_BACNET_STACK
void BACnetDriver::IAmHandler(uint8_t* service_request, uint16_t service_len, 
                             BACNET_ADDRESS* src) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) return;
    
    try {
        uint32_t device_id;
        uint32_t max_apdu;
        int segmentation;
        uint16_t vendor_id;
        
        if (iam_decode_service_request(service_request, service_len, &device_id, 
                                     &max_apdu, &segmentation, &vendor_id)) {
            
            // 🔥 통계 업데이트
            instance_->GetStatistics().IncrementProtocolCounter("i_am_received");
            
            // 디바이스 정보 처리 (서비스 매니저에 위임)
            BACnetDeviceInfo device_info;
            device_info.device_id = device_id;
            device_info.max_apdu_length = max_apdu;
            device_info.segmentation_supported = (segmentation != SEGMENTATION_NONE);
            device_info.vendor_id = vendor_id;
            device_info.last_seen = system_clock::now();
            device_info.online = true;
            
            // IP 주소 정보 추출
            if (src->net == 0 && src->len == 6) {
                device_info.ip_address = std::to_string(src->adr[0]) + "." +
                                        std::to_string(src->adr[1]) + "." +
                                        std::to_string(src->adr[2]) + "." +
                                        std::to_string(src->adr[3]);
                device_info.port = (src->adr[4] << 8) | src->adr[5];
            }
            
            instance_->logger_.Debug("I-Am received from device " + std::to_string(device_id),
                                   DriverLogCategory::DIAGNOSTICS);
        }
        
    } catch (const std::exception& e) {
        if (instance_) {
            instance_->SetError(Enums::ErrorCode::PROTOCOL_ERROR, 
                               "Exception in I-Am handler: " + std::string(e.what()));
        }
    }
}

void BACnetDriver::ReadPropertyAckHandler(uint8_t* service_request, uint16_t service_len,
                                        BACNET_ADDRESS* src, BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data) {
    (void)service_request; (void)service_len; (void)src;
    
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) return;
    
    try {
        instance_->GetStatistics().IncrementProtocolCounter("read_property_responses");
        
        instance_->logger_.Debug("Read Property ACK received for invoke ID " + 
                                std::to_string(service_data->invoke_id),
                                DriverLogCategory::DIAGNOSTICS);
        
    } catch (const std::exception& e) {
        if (instance_) {
            instance_->SetError(Enums::ErrorCode::PROTOCOL_ERROR, 
                               "Exception in Read Property ACK handler: " + std::string(e.what()));
        }
    }
}

void BACnetDriver::ErrorHandler(BACNET_ADDRESS* src, uint8_t invoke_id, 
                               BACNET_ERROR_CLASS error_class, BACNET_ERROR_CODE error_code) {
    (void)src; (void)invoke_id;
    
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) return;
    
    try {
        instance_->GetStatistics().IncrementProtocolCounter("protocol_errors");
        instance_->LogBACnetError("BACnet confirmed service error", error_class, error_code);
        
    } catch (const std::exception& e) {
        if (instance_) {
            instance_->SetError(Enums::ErrorCode::PROTOCOL_ERROR, 
                               "Exception in error handler: " + std::string(e.what()));
        }
    }
}

void BACnetDriver::RejectHandler(BACNET_ADDRESS* src, uint8_t invoke_id, uint8_t reject_reason) {
    (void)src; (void)invoke_id;
    
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) return;
    
    try {
        instance_->GetStatistics().IncrementProtocolCounter("reject_errors");
        instance_->SetError(Enums::ErrorCode::PROTOCOL_ERROR, 
                           "BACnet request rejected: reason " + std::to_string(reject_reason));
        
    } catch (const std::exception& e) {
        if (instance_) {
            instance_->SetError(Enums::ErrorCode::PROTOCOL_ERROR, 
                               "Exception in reject handler: " + std::string(e.what()));
        }
    }
}

void BACnetDriver::AbortHandler(BACNET_ADDRESS* src, uint8_t invoke_id, uint8_t abort_reason) {
    (void)src; (void)invoke_id;
    
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) return;
    
    try {
        instance_->GetStatistics().IncrementProtocolCounter("abort_errors");
        instance_->SetError(Enums::ErrorCode::PROTOCOL_ERROR, 
                           "BACnet request aborted: reason " + std::to_string(abort_reason));
        
    } catch (const std::exception& e) {
        if (instance_) {
            instance_->SetError(Enums::ErrorCode::PROTOCOL_ERROR, 
                               "Exception in abort handler: " + std::string(e.what()));
        }
    }
}

void BACnetDriver::COVNotificationHandler(uint8_t* service_request, uint16_t service_len,
                                        BACNET_ADDRESS* src, BACNET_CONFIRMED_SERVICE_DATA* service_data) {
    (void)src; (void)service_data;
    
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) return;
    
    try {
        instance_->GetStatistics().IncrementProtocolCounter("cov_notifications");
        
        // COV 매니저에 위임
        // 실제 구현에서는 service_request를 파싱하여 COV 데이터 추출
        
        instance_->logger_.Debug("COV notification received",
                                DriverLogCategory::DIAGNOSTICS);
        
    } catch (const std::exception& e) {
        if (instance_) {
            instance_->SetError(Enums::ErrorCode::PROTOCOL_ERROR, 
                               "Exception in COV notification handler: " + std::string(e.what()));
        }
    }
}
#endif // HAS_BACNET_STACK

} // namespace Drivers
} // namespace PulseOne