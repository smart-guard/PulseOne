// =============================================================================
// collector/src/Drivers/Bacnet/BACnetDriver.cpp
// 실제 BACnet Stack 라이브러리 연동 구현
// =============================================================================

#include "Drivers/Bacnet/BACnetDriver.h"
#include "Utils/LogManager.h"
#include <iostream>
#include <chrono>
#include <cstring>
#include <algorithm>

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

BACnetDriver::BACnetDriver() {
    // 통계 초기화 (기존 패턴 유지)
    statistics_.total_operations = 0;
    statistics_.successful_operations = 0;
    statistics_.failed_operations = 0;
    statistics_.success_rate = 0.0;
    statistics_.avg_response_time_ms = 0.0;
    statistics_.last_connection_time = system_clock::now();
    
    // 에러 초기화
    last_error_.code = Enums::ErrorCode::SUCCESS;
    last_error_.message = "";
    
    // 정적 인스턴스 등록
    {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        instance_ = this;
    }
}

BACnetDriver::~BACnetDriver() {
    Disconnect();
    CleanupBACnetStack();
    
    // 정적 인스턴스 해제
    {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (instance_ == this) {
            instance_ = nullptr;
        }
    }
}

// =============================================================================
// IProtocolDriver 인터페이스 구현
// =============================================================================

bool BACnetDriver::Initialize(const DriverConfig& config) {
    config_ = config;
    
    try {
        // 설정에서 타겟 정보 파싱
        if (!config.endpoint.empty()) {
            size_t colon_pos = config.endpoint.find(':');
            if (colon_pos != std::string::npos) {
                target_ip_ = config.endpoint.substr(0, colon_pos);
                target_port_ = static_cast<uint16_t>(std::stoi(config.endpoint.substr(colon_pos + 1)));
            } else {
                target_ip_ = config.endpoint;
                target_port_ = 47808;  // BACnet 기본 포트
            }
        }
        
        // 로컬 디바이스 ID 설정 (config에서 지정된 경우)
        auto config_json = nlohmann::json::parse(config.additional_config);
        if (config_json.contains("local_device_id")) {
            local_device_id_ = config_json["local_device_id"];
        }
        
        // BACnet Stack 초기화
        if (!InitializeBACnetStack()) {
            SetError(Enums::ErrorCode::INTERNAL_ERROR, "Failed to initialize BACnet Stack");
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        SetError(Enums::ErrorCode::INVALID_PARAMETER, 
                std::string("Configuration error: ") + e.what());
        return false;
    }
}

bool BACnetDriver::Connect() {
    if (is_connected_.load()) {
        return true;
    }
    
#ifdef HAS_BACNET_STACK
    try {
        // BACnet 네트워크 스레드 시작
        should_stop_.store(false);
        network_thread_ = std::make_unique<std::thread>(&BACnetDriver::NetworkThreadFunction, this);
        
        // 연결 성공
        is_connected_.store(true);
        
        // 통계 업데이트
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            statistics_.last_connection_time = system_clock::now();
            statistics_.total_operations++;
            statistics_.successful_operations++;
        }
        
        PulseOne::Utils::LogManager::Instance().Info(
            "BACnet Driver connected to " + target_ip_ + ":" + std::to_string(target_port_)
        );
        
        return true;
        
    } catch (const std::exception& e) {
        SetError(Enums::ErrorCode::CONNECTION_FAILED, 
                std::string("Connection failed: ") + e.what());
        return false;
    }
#else
    SetError(Enums::ErrorCode::INTERNAL_ERROR, "BACnet Stack library not available");
    return false;
#endif
}

bool BACnetDriver::Disconnect() {
    if (!is_connected_.load()) {
        return true;
    }
    
    // 스레드 종료 신호
    should_stop_.store(true);
    
    // 대기 중인 요청들 모두 완료 처리
    {
        std::lock_guard<std::mutex> lock(response_mutex_);
        for (auto& [invoke_id, request] : pending_requests_) {
            request->promise.set_value(false);
        }
        pending_requests_.clear();
    }
    
    // 조건 변수 알림
    response_cv_.notify_all();
    
    // 네트워크 스레드 종료 대기
    if (network_thread_ && network_thread_->joinable()) {
        network_thread_->join();
    }
    
    is_connected_.store(false);
    
    PulseOne::Utils::LogManager::Instance().Info("BACnet Driver disconnected");
    
    return true;
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
    
    bool all_success = true;
    auto start_time = steady_clock::now();
    
    for (const auto& point : points) {
        TimestampedValue result;
        
        try {
            // DataPoint에서 BACnet 정보 추출
            uint32_t device_id = std::stoul(point.device_id);
            
            // address에서 object_instance 추출
            uint32_t obj_instance = point.address;
            
            // data_type에서 object_type과 property_id 추출 (JSON 파싱)
            auto point_config = nlohmann::json::parse(point.metadata);
            BACNET_OBJECT_TYPE obj_type = static_cast<BACNET_OBJECT_TYPE>(
                point_config.value("object_type", OBJECT_ANALOG_INPUT)
            );
            BACNET_PROPERTY_ID prop_id = static_cast<BACNET_PROPERTY_ID>(
                point_config.value("property_id", PROP_PRESENT_VALUE)
            );
            uint32_t array_index = point_config.value("array_index", BACNET_ARRAY_ALL);
            
            // BACnet 프로퍼티 읽기
            if (ReadBACnetProperty(device_id, obj_type, obj_instance, prop_id, result, array_index)) {
                values.push_back(result);
            } else {
                // 실패한 경우 UNKNOWN 품질로 추가
                result.value = Structs::DataValue{0.0f};
                result.quality = Enums::DataQuality::UNKNOWN;
                result.timestamp = system_clock::now();
                values.push_back(result);
                all_success = false;
            }
            
        } catch (const std::exception& e) {
            // 에러 발생 시 UNKNOWN 품질로 추가
            result.value = Structs::DataValue{0.0f};
            result.quality = Enums::DataQuality::UNKNOWN;
            result.timestamp = system_clock::now();
            values.push_back(result);
            all_success = false;
            
            PulseOne::Utils::LogManager::Instance().Error(
                "Failed to read BACnet point " + point.name + ": " + e.what()
            );
        }
    }
    
    // 통계 업데이트
    auto end_time = steady_clock::now();
    auto duration_ms = duration_cast<milliseconds>(end_time - start_time).count();
    UpdateStatistics(all_success, static_cast<double>(duration_ms));
    
    return all_success;
}

bool BACnetDriver::WriteValue(const Structs::DataPoint& point, 
                             const Structs::DataValue& value) {
    if (!IsConnected()) {
        SetError(Enums::ErrorCode::CONNECTION_LOST, "Not connected to BACnet network");
        return false;
    }
    
    try {
        // DataPoint에서 BACnet 정보 추출
        uint32_t device_id = std::stoul(point.device_id);
        uint32_t obj_instance = point.address;
        
        // metadata에서 BACnet 객체 정보 추출
        auto point_config = nlohmann::json::parse(point.metadata);
        BACNET_OBJECT_TYPE obj_type = static_cast<BACNET_OBJECT_TYPE>(
            point_config.value("object_type", OBJECT_ANALOG_OUTPUT)
        );
        BACNET_PROPERTY_ID prop_id = static_cast<BACNET_PROPERTY_ID>(
            point_config.value("property_id", PROP_PRESENT_VALUE)
        );
        uint8_t priority = point_config.value("priority", 0);
        uint32_t array_index = point_config.value("array_index", BACNET_ARRAY_ALL);
        
        // BACnet 프로퍼티 쓰기
        auto start_time = steady_clock::now();
        bool success = WriteBACnetProperty(device_id, obj_type, obj_instance, 
                                          prop_id, value, priority, array_index);
        auto end_time = steady_clock::now();
        
        // 통계 업데이트
        auto duration_ms = duration_cast<milliseconds>(end_time - start_time).count();
        UpdateStatistics(success, static_cast<double>(duration_ms));
        
        return success;
        
    } catch (const std::exception& e) {
        SetError(Enums::ErrorCode::DATA_FORMAT_ERROR, 
                std::string("Write error: ") + e.what());
        return false;
    }
}

Enums::ProtocolType BACnetDriver::GetProtocolType() const {
    return Enums::ProtocolType::BACNET_IP;
}

Structs::DriverStatus BACnetDriver::GetStatus() const {
    Structs::DriverStatus status;
    status.is_connected = is_connected_.load();
    status.last_error = last_error_;
    status.connection_time = statistics_.last_connection_time;
    
    // BACnet 특화 상태 정보
    status.additional_info["local_device_id"] = std::to_string(local_device_id_);
    status.additional_info["target_endpoint"] = target_ip_ + ":" + std::to_string(target_port_);
    status.additional_info["discovered_devices"] = std::to_string(discovered_devices_.size());
    
    return status;
}

Structs::ErrorInfo BACnetDriver::GetLastError() const {
    return last_error_;
}

const DriverStatistics& BACnetDriver::GetStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

void BACnetDriver::ResetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    statistics_.total_operations = 0;
    statistics_.successful_operations = 0;
    statistics_.failed_operations = 0;
    statistics_.success_rate = 0.0;
    statistics_.avg_response_time_ms = 0.0;
}

// =============================================================================
// BACnet 특화 메서드 구현
// =============================================================================

int BACnetDriver::DiscoverDevices(std::map<uint32_t, BACnetDeviceInfo>& discovered_devices, 
                                 int timeout_ms) {
    if (!IsConnected()) {
        SetError(Enums::ErrorCode::CONNECTION_LOST, "Not connected to BACnet network");
        return 0;
    }
    
#ifdef HAS_BACNET_STACK
    try {
        // 기존 발견된 디바이스 목록 클리어
        {
            std::lock_guard<std::mutex> lock(response_mutex_);
            discovered_devices_.clear();
        }
        
        // Who-Is 브로드캐스트 전송
        uint8_t invoke_id = SendWhoIsRequest();
        if (invoke_id == 0) {
            SetError(Enums::ErrorCode::PROTOCOL_ERROR, "Failed to send Who-Is request");
            return 0;
        }
        
        // 지정된 시간만큼 대기하며 I-Am 응답 수집
        auto start_time = steady_clock::now();
        while (duration_cast<milliseconds>(steady_clock::now() - start_time).count() < timeout_ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // 네트워크 메시지 처리는 NetworkThreadFunction에서 수행됨
        }
        
        // 발견된 디바이스들 복사
        {
            std::lock_guard<std::mutex> lock(response_mutex_);
            discovered_devices = discovered_devices_;
        }
        
        PulseOne::Utils::LogManager::Instance().Info(
            "BACnet device discovery completed. Found " + 
            std::to_string(discovered_devices.size()) + " devices"
        );
        
        return static_cast<int>(discovered_devices.size());
        
    } catch (const std::exception& e) {
        SetError(Enums::ErrorCode::INTERNAL_ERROR, 
                std::string("Device discovery error: ") + e.what());
        return 0;
    }
#else
    SetError(Enums::ErrorCode::INTERNAL_ERROR, "BACnet Stack library not available");
    return 0;
#endif
}

bool BACnetDriver::ReadBACnetProperty(uint32_t device_id,
                                     BACNET_OBJECT_TYPE obj_type, 
                                     uint32_t obj_instance, 
                                     BACNET_PROPERTY_ID prop_id, 
                                     TimestampedValue& result,
                                     uint32_t array_index) {
    if (!IsConnected()) {
        SetError(Enums::ErrorCode::CONNECTION_LOST, "Not connected to BACnet network");
        return false;
    }
    
#ifdef HAS_BACNET_STACK
    try {
        // Read Property 요청 전송
        uint8_t invoke_id = SendReadPropertyRequest(device_id, obj_type, obj_instance, 
                                                   prop_id, array_index);
        if (invoke_id == 0) {
            SetError(Enums::ErrorCode::PROTOCOL_ERROR, "Failed to send Read Property request");
            return false;
        }
        
        // 응답 대기
        if (!WaitForResponse(invoke_id, 5000)) {  // 5초 타임아웃
            SetError(Enums::ErrorCode::CONNECTION_TIMEOUT, "Read Property request timeout");
            return false;
        }
        
        // 응답에서 결과 값 추출 (pending_requests_에서 결과 확인)
        {
            std::lock_guard<std::mutex> lock(response_mutex_);
            auto it = pending_requests_.find(invoke_id);
            if (it != pending_requests_.end() && it->second->result_value) {
                result.value = ConvertBACnetValue(*(it->second->result_value));
                result.quality = Enums::DataQuality::GOOD;
                result.timestamp = system_clock::now();
                
                pending_requests_.erase(it);
                return true;
            }
        }
        
        SetError(Enums::ErrorCode::DATA_FORMAT_ERROR, "Invalid response data");
        return false;
        
    } catch (const std::exception& e) {
        SetError(Enums::ErrorCode::INTERNAL_ERROR, 
                std::string("Read Property error: ") + e.what());
        return false;
    }
#else
    // BACnet Stack이 없는 경우 스텁 구현 (개발용)
    result.value = Structs::DataValue{25.5f};  // 더미 값
    result.quality = Enums::DataQuality::GOOD;
    result.timestamp = system_clock::now();
    return true;
#endif
}

bool BACnetDriver::WriteBACnetProperty(uint32_t device_id,
                                      BACNET_OBJECT_TYPE obj_type,
                                      uint32_t obj_instance,
                                      BACNET_PROPERTY_ID prop_id,
                                      const Structs::DataValue& value,
                                      uint8_t priority,
                                      uint32_t array_index) {
    if (!IsConnected()) {
        SetError(Enums::ErrorCode::CONNECTION_LOST, "Not connected to BACnet network");
        return false;
    }
    
#ifdef HAS_BACNET_STACK
    try {
        // PulseOne DataValue를 BACnet 값으로 변환
        BACNET_APPLICATION_DATA_VALUE bacnet_value;
        if (!ConvertToBACnetValue(value, bacnet_value)) {
            SetError(Enums::ErrorCode::DATA_FORMAT_ERROR, "Failed to convert value format");
            return false;
        }
        
        // Write Property 요청 전송
        uint8_t invoke_id = SendWritePropertyRequest(device_id, obj_type, obj_instance,
                                                    prop_id, bacnet_value, priority, array_index);
        if (invoke_id == 0) {
            SetError(Enums::ErrorCode::PROTOCOL_ERROR, "Failed to send Write Property request");
            return false;
        }
        
        // 응답 대기
        if (!WaitForResponse(invoke_id, 5000)) {  // 5초 타임아웃
            SetError(Enums::ErrorCode::CONNECTION_TIMEOUT, "Write Property request timeout");
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        SetError(Enums::ErrorCode::INTERNAL_ERROR, 
                std::string("Write Property error: ") + e.what());
        return false;
    }
#else
    // BACnet Stack이 없는 경우 성공으로 처리 (개발용)
    return true;
#endif
}

// =============================================================================
// BACnet Stack 초기화 및 관리
// =============================================================================

bool BACnetDriver::InitializeBACnetStack() {
#ifdef HAS_BACNET_STACK
    try {
        // BACnet 디바이스 설정
        Device_Set_Object_Instance_Number(local_device_id_);
        Device_Init(nullptr);
        
        // 데이터링크 레이어 초기화 (BACnet/IP)
        bip_set_port(target_port_);
        if (!datalink_init(nullptr)) {
            return false;
        }
        
        // 핸들러 등록
        RegisterBACnetHandlers();
        
        PulseOne::Utils::LogManager::Instance().Info(
            "BACnet Stack initialized. Local Device ID: " + std::to_string(local_device_id_)
        );
        
        return true;
        
    } catch (const std::exception& e) {
        PulseOne::Utils::LogManager::Instance().Error(
            "BACnet Stack initialization failed: " + std::string(e.what())
        );
        return false;
    }
#else
    PulseOne::Utils::LogManager::Instance().Warn(
        "BACnet Stack library not available. Using stub implementation."
    );
    return true;  // 개발용으로 성공 처리
#endif
}

void BACnetDriver::CleanupBACnetStack() {
#ifdef HAS_BACNET_STACK
    try {
        datalink_cleanup();
        PulseOne::Utils::LogManager::Instance().Info("BACnet Stack cleaned up");
    } catch (const std::exception& e) {
        PulseOne::Utils::LogManager::Instance().Error(
            "BACnet Stack cleanup error: " + std::string(e.what())
        );
    }
#endif
}

void BACnetDriver::NetworkThreadFunction() {
    PulseOne::Utils::LogManager::Instance().Info("BACnet network thread started");
    
    while (!should_stop_.load()) {
        try {
            ProcessBACnetMessages();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 10ms 간격
        } catch (const std::exception& e) {
            PulseOne::Utils::LogManager::Instance().Error(
                "BACnet network thread error: " + std::string(e.what())
            );
        }
    }
    
    PulseOne::Utils::LogManager::Instance().Info("BACnet network thread stopped");
}

void BACnetDriver::ProcessBACnetMessages() {
#ifdef HAS_BACNET_STACK
    BACNET_ADDRESS src;
    uint16_t pdu_len;
    unsigned timeout = 1;  // 1ms 타임아웃
    
    pdu_len = datalink_receive(&src, Rx_Buf, MAX_MPDU, timeout);
    if (pdu_len) {
        npdu_handler(&src, Rx_Buf, pdu_len);
    }
#endif
}

// =============================================================================
// BACnet 서비스 헬퍼 메서드들
// =============================================================================

uint8_t BACnetDriver::SendWhoIsRequest(uint32_t low_limit, uint32_t high_limit) {
#ifdef HAS_BACNET_STACK
    return Send_WhoIs(low_limit, high_limit);
#else
    return 1;  // 스텁
#endif
}

uint8_t BACnetDriver::SendReadPropertyRequest(uint32_t device_id,
                                             BACNET_OBJECT_TYPE obj_type,
                                             uint32_t obj_instance,
                                             BACNET_PROPERTY_ID prop_id,
                                             uint32_t array_index) {
#ifdef HAS_BACNET_STACK
    BACNET_READ_PROPERTY_DATA rpdata;
    rpdata.object_type = obj_type;
    rpdata.object_instance = obj_instance;
    rpdata.object_property = prop_id;
    rpdata.array_index = array_index;
    
    uint8_t invoke_id = Send_Read_Property_Request(device_id, &rpdata);
    
    // 펜딩 요청 등록
    if (invoke_id > 0) {
        std::lock_guard<std::mutex> lock(response_mutex_);
        auto request = std::make_unique<PendingRequest>();
        request->invoke_id = invoke_id;
        request->timeout = system_clock::now() + std::chrono::seconds(5);
        request->service_choice = SERVICE_CONFIRMED_READ_PROPERTY;
        request->target_device_id = device_id;
        
        pending_requests_[invoke_id] = std::move(request);
    }
    
    return invoke_id;
#else
    return 1;  // 스텁
#endif
}

// =============================================================================
// 콜백 핸들러 등록
// =============================================================================

void BACnetDriver::RegisterBACnetHandlers() {
#ifdef HAS_BACNET_STACK
    // I-Am 핸들러 등록
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, StaticIAmHandler);
    
    // Read Property Ack 핸들러 등록
    apdu_set_confirmed_ack_handler(SERVICE_CONFIRMED_READ_PROPERTY, StaticReadPropertyAckHandler);
    
    // Write Property Ack 핸들러 등록
    apdu_set_confirmed_ack_handler(SERVICE_CONFIRMED_WRITE_PROPERTY, StaticWritePropertyAckHandler);
    
    // 에러 핸들러들 등록
    apdu_set_error_handler(SERVICE_CONFIRMED_READ_PROPERTY, StaticErrorHandler);
    apdu_set_error_handler(SERVICE_CONFIRMED_WRITE_PROPERTY, StaticErrorHandler);
    apdu_set_abort_handler(StaticAbortHandler);
    apdu_set_reject_handler(StaticRejectHandler);
#endif
}

// =============================================================================
// 정적 콜백 함수들 (BACnet Stack에서 호출)
// =============================================================================

void BACnetDriver::StaticIAmHandler(uint8_t* service_request, uint16_t service_len, 
                                   BACNET_ADDRESS* src) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (instance_) {
        instance_->HandleIAm(service_request, service_len, src);
    }
}

void BACnetDriver::StaticReadPropertyAckHandler(uint8_t* service_request, uint16_t service_len,
                                               BACNET_ADDRESS* src,
                                               BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (instance_) {
        instance_->HandleReadPropertyAck(service_request, service_len, src, service_data);
    }
}

// =============================================================================
// 실제 핸들러 구현 (인스턴스 메서드)
// =============================================================================

void BACnetDriver::HandleIAm(uint8_t* service_request, uint16_t service_len, BACNET_ADDRESS* src) {
#ifdef HAS_BACNET_STACK
    try {
        uint32_t device_id;
        unsigned max_apdu;
        int segmentation;
        uint16_t vendor_id;
        
        if (iam_decode_service_request(service_request, &device_id, &max_apdu, 
                                      &segmentation, &vendor_id)) {
            // 새 디바이스 정보 생성
            BACnetDeviceInfo device_info;
            device_info.device_id = device_id;
            device_info.max_apdu_length = max_apdu;
            device_info.segmentation_supported = (segmentation != SEGMENTATION_NONE);
            device_info.vendor_id = vendor_id;
            
            // IP 주소 추출
            if (src->mac_len == 6) {  // BACnet/IP
                device_info.ip_address = std::to_string(src->mac[0]) + "." +
                                        std::to_string(src->mac[1]) + "." +
                                        std::to_string(src->mac[2]) + "." +
                                        std::to_string(src->mac[3]);
                device_info.port = (src->mac[4] << 8) | src->mac[5];
            }
            
            // 발견된 디바이스 목록에 추가
            {
                std::lock_guard<std::mutex> lock(response_mutex_);
                discovered_devices_[device_id] = device_info;
            }
            
            PulseOne::Utils::LogManager::Instance().Debug(
                "Received I-Am from device " + std::to_string(device_id) + 
                " at " + device_info.ip_address + ":" + std::to_string(device_info.port)
            );
        }
    } catch (const std::exception& e) {
        PulseOne::Utils::LogManager::Instance().Error(
            "I-Am handler error: " + std::string(e.what())
        );
    }
#endif
}

void BACnetDriver::HandleReadPropertyAck(uint8_t* service_request, uint16_t service_len,
                                        BACNET_ADDRESS* src, 
                                        BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data) {
#ifdef HAS_BACNET_STACK
    try {
        BACNET_READ_PROPERTY_DATA rpdata;
        
        if (rp_ack_decode_service_request(service_request, service_len, &rpdata)) {
            // 해당 invoke_id의 펜딩 요청 찾기
            std::lock_guard<std::mutex> lock(response_mutex_);
            auto it = pending_requests_.find(service_data->invoke_id);
            if (it != pending_requests_.end()) {
                // 응답 데이터 저장
                it->second->result_value = new BACNET_APPLICATION_DATA_VALUE(rpdata.application_data);
                
                // Promise 완료
                it->second->promise.set_value(true);
                
                PulseOne::Utils::LogManager::Instance().Debug(
                    "Read Property Ack received for invoke_id " + 
                    std::to_string(service_data->invoke_id)
                );
            }
        }
    } catch (const std::exception& e) {
        PulseOne::Utils::LogManager::Instance().Error(
            "Read Property Ack handler error: " + std::string(e.what())
        );
    }
#endif
}

// =============================================================================
// 유틸리티 메서드들
// =============================================================================

void BACnetDriver::SetError(Enums::ErrorCode code, const std::string& message) {
    last_error_.code = code;
    last_error_.message = message;
    last_error_.timestamp = system_clock::now();
    
    PulseOne::Utils::LogManager::Instance().Error(
        "BACnet Driver Error [" + std::to_string(static_cast<int>(code)) + "]: " + message
    );
}

void BACnetDriver::UpdateStatistics(bool success, double response_time_ms) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    statistics_.total_operations++;
    
    if (success) {
        statistics_.successful_operations++;
    } else {
        statistics_.failed_operations++;
    }
    
    // 성공률 계산
    if (statistics_.total_operations > 0) {
        statistics_.success_rate = static_cast<double>(statistics_.successful_operations) / 
                                  statistics_.total_operations * 100.0;
    }
    
    // 평균 응답 시간 업데이트 (이동 평균)
    if (statistics_.avg_response_time_ms == 0.0) {
        statistics_.avg_response_time_ms = response_time_ms;
    } else {
        statistics_.avg_response_time_ms = (statistics_.avg_response_time_ms * 0.9) + 
                                          (response_time_ms * 0.1);
    }
}

Structs::DataValue BACnetDriver::ConvertBACnetValue(const BACNET_APPLICATION_DATA_VALUE& bacnet_value) {
#ifdef HAS_BACNET_STACK
    switch (bacnet_value.tag) {
        case BACNET_APPLICATION_TAG_BOOLEAN:
            return Structs::DataValue{bacnet_value.type.Boolean};
            
        case BACNET_APPLICATION_TAG_UNSIGNED_INT:
            return Structs::DataValue{static_cast<uint32_t>(bacnet_value.type.Unsigned_Int)};
            
        case BACNET_APPLICATION_TAG_SIGNED_INT:
            return Structs::DataValue{static_cast<int32_t>(bacnet_value.type.Signed_Int)};
            
        case BACNET_APPLICATION_TAG_REAL:
            return Structs::DataValue{bacnet_value.type.Real};
            
        case BACNET_APPLICATION_TAG_DOUBLE:
            return Structs::DataValue{bacnet_value.type.Double};
            
        case BACNET_APPLICATION_TAG_CHARACTER_STRING:
            return Structs::DataValue{std::string(characterstring_value(&bacnet_value.type.Character_String))};
            
        case BACNET_APPLICATION_TAG_ENUMERATED:
            return Structs::DataValue{static_cast<uint32_t>(bacnet_value.type.Enumerated)};
            
        default:
            return Structs::DataValue{0.0f};  // 기본값
    }
#else
    return Structs::DataValue{25.5f};  // 스텁
#endif
}

bool BACnetDriver::WaitForResponse(uint8_t invoke_id, int timeout_ms) {
    std::unique_lock<std::mutex> lock(response_mutex_);
    
    auto it = pending_requests_.find(invoke_id);
    if (it == pending_requests_.end()) {
        return false;
    }
    
    auto future = it->second->promise.get_future();
    lock.unlock();
    
    auto status = future.wait_for(std::chrono::milliseconds(timeout_ms));
    return (status == std::future_status::ready && future.get());
}

} // namespace PulseOne::Drivers
} // namespace PulseOne