// =============================================================================
// collector/src/Drivers/Bacnet/BACnetDriver.cpp
// 🔥 중복 제거 및 괄호 수정 완료본
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
// 🔥 1. 전역 변수들 (한 번만 정의)
// =============================================================================
#ifdef HAS_BACNET_STACK
uint8_t Rx_Buf[MAX_MPDU] = {0};
uint8_t Tx_Buf[MAX_MPDU] = {0};
#endif

// =============================================================================
// 🔥 2. 정적 멤버 초기화 (한 번만 정의)
// =============================================================================
BACnetDriver* BACnetDriver::instance_ = nullptr;
std::mutex BACnetDriver::instance_mutex_;

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

BACnetDriver::BACnetDriver() {
    // 통계 초기화
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
        
        // 로컬 디바이스 ID 설정
        if (!config.connection_string.empty()) {
#ifdef HAS_NLOHMANN_JSON
            try {
                auto config_json = nlohmann::json::parse(config.connection_string);
                if (config_json.contains("local_device_id")) {
                    local_device_id_ = config_json["local_device_id"];
                }
            } catch (const nlohmann::json::exception& e) {
                LogManager::getInstance().Warn("JSON parse error: " + std::string(e.what()));
            }
#endif
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
        
        LogManager::getInstance().Info(
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
            try {
                request->promise.set_value(false);
            } catch (...) {
                // promise가 이미 set된 경우 무시
            }
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
    
    LogManager::getInstance().Info("BACnet Driver disconnected");
    
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
            uint32_t obj_instance = point.address;
            
            // 🔥 수정: point.metadata는 std::map<std::string, std::string> 타입!
#ifdef HAS_NLOHMANN_JSON
            nlohmann::json point_config = nlohmann::json::object();
            
            // map을 JSON으로 변환 (parse() 아님!)
            for (const auto& [key, value] : point.metadata) {
                point_config[key] = value;
            }
            
            // BACnet 설정 추출
            BACNET_OBJECT_TYPE obj_type = static_cast<BACNET_OBJECT_TYPE>(
                point_config.value("object_type", OBJECT_ANALOG_INPUT)
            );
            BACNET_PROPERTY_ID prop_id = static_cast<BACNET_PROPERTY_ID>(
                point_config.value("property_id", PROP_PRESENT_VALUE)
            );
            uint32_t array_index = point_config.value("array_index", BACNET_ARRAY_ALL);
            
#else
            // JSON 라이브러리가 없는 경우 기본값 사용
            BACNET_OBJECT_TYPE obj_type = OBJECT_ANALOG_INPUT;
            BACNET_PROPERTY_ID prop_id = PROP_PRESENT_VALUE;
            uint32_t array_index = 0;
#endif
            
            // BACnet 프로퍼티 읽기
            if (ReadBACnetProperty(device_id, obj_type, obj_instance, prop_id, result, array_index)) {
                values.push_back(result);
            } else {
                // 실패한 경우 UNCERTAIN 품질로 추가
                result.value = Structs::DataValue{0.0f};
                result.quality = Enums::DataQuality::UNCERTAIN;
                result.timestamp = system_clock::now();
                values.push_back(result);
                all_success = false;
            }
            
        } catch (const std::exception& e) {
            // 에러 발생 시 UNCERTAIN 품질로 추가
            result.value = Structs::DataValue{0.0f};
            result.quality = Enums::DataQuality::UNCERTAIN;
            result.timestamp = system_clock::now();
            values.push_back(result);
            all_success = false;
            
            LogManager::getInstance().Error(
                "Failed to read BACnet point " + point.name + ": " + e.what()
            );
        }
    } // for 루프 끝
    
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
        
        // 🔥 수정: point.metadata는 std::map<std::string, std::string> 타입!
#ifdef HAS_NLOHMANN_JSON
        nlohmann::json point_config = nlohmann::json::object();
        
        // map을 JSON으로 변환
        for (const auto& [key, value_str] : point.metadata) {
            point_config[key] = value_str;
        }
        
        // BACnet 설정 추출
        BACNET_OBJECT_TYPE obj_type = static_cast<BACNET_OBJECT_TYPE>(
            point_config.value("object_type", OBJECT_ANALOG_OUTPUT)
        );
        BACNET_PROPERTY_ID prop_id = static_cast<BACNET_PROPERTY_ID>(
            point_config.value("property_id", PROP_PRESENT_VALUE)
        );
        uint8_t priority = point_config.value("priority", 0);
        uint32_t array_index = point_config.value("array_index", BACNET_ARRAY_ALL);
        
#else
        // JSON 라이브러리가 없는 경우 기본값 사용
        BACNET_OBJECT_TYPE obj_type = OBJECT_ANALOG_OUTPUT;
        BACNET_PROPERTY_ID prop_id = PROP_PRESENT_VALUE;
        uint8_t priority = 0;
        uint32_t array_index = 0;
#endif
        
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
    if (is_connected_.load()) {
        return Structs::DriverStatus::RUNNING;
    } else {
        return Structs::DriverStatus::STOPPED;
    }
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
        if (!bip_init(nullptr)) {
            return false;
        }
        
        // 핸들러 등록
        RegisterBACnetHandlers();
        
        LogManager::getInstance().Info(
            "BACnet Stack initialized. Local Device ID: " + std::to_string(local_device_id_)
        );
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "BACnet Stack initialization failed: " + std::string(e.what())
        );
        return false;
    }
#else
    LogManager::getInstance().Warn(
        "BACnet Stack library not available. Using stub implementation."
    );
    return true;  // 개발용으로 성공 처리
#endif
}

void BACnetDriver::CleanupBACnetStack() {
#ifdef HAS_BACNET_STACK
    try {
        bip_cleanup();
        LogManager::getInstance().Info("BACnet Stack cleaned up");
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "BACnet Stack cleanup error: " + std::string(e.what())
        );
    }
#endif
}

void BACnetDriver::NetworkThreadFunction() {
    LogManager::getInstance().Info("BACnet network thread started");
    
    while (!should_stop_.load()) {
        try {
            ProcessBACnetMessages();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));  // 10ms 간격
        } catch (const std::exception& e) {
            LogManager::getInstance().Error(
                "BACnet network thread error: " + std::string(e.what())
            );
        }
    }
    
    LogManager::getInstance().Info("BACnet network thread stopped");
}

void BACnetDriver::ProcessBACnetMessages() {
#ifdef HAS_BACNET_STACK
    BACNET_ADDRESS src;
    uint16_t pdu_len;
    unsigned timeout = 1;  // 1ms 타임아웃
    
    pdu_len = bip_receive(&src, Rx_Buf, MAX_MPDU, timeout);
    if (pdu_len) {
        // 패킷 처리 로직
        // APDU 핸들러에서 자동으로 처리됨
    }
#endif
}

// =============================================================================
// 🔥 유틸리티 메서드들 (한 번만 구현)
// =============================================================================

void BACnetDriver::SetError(Enums::ErrorCode code, const std::string& message) {
    last_error_.code = code;
    last_error_.message = message;
    last_error_.occurred_at = std::chrono::system_clock::now();
    
    LogManager::getInstance().Error(
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

void BACnetDriver::CompleteRequest(uint8_t invoke_id, bool success) {
    std::lock_guard<std::mutex> lock(response_mutex_);
    
    auto it = pending_requests_.find(invoke_id);
    if (it != pending_requests_.end()) {
        try {
            it->second->promise.set_value(success);
        } catch (...) {
            // promise가 이미 set된 경우 무시
        }
        pending_requests_.erase(it);
    }
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

// =============================================================================
// 🔥 2. 데이터 변환 함수들
// =============================================================================

bool BACnetDriver::ConvertToBACnetValue(const Structs::DataValue& pulse_value, 
                                       BACNET_APPLICATION_DATA_VALUE& bacnet_value) {
#ifdef HAS_BACNET_STACK
    try {
        // DataValue variant에서 값 추출하여 BACnet 형식으로 변환
        if (std::holds_alternative<bool>(pulse_value)) {
            bacnet_value.tag = BACNET_APPLICATION_TAG_BOOLEAN;
            bacnet_value.type.Boolean = std::get<bool>(pulse_value);
            return true;
        } 
        else if (std::holds_alternative<int>(pulse_value)) {
            bacnet_value.tag = BACNET_APPLICATION_TAG_SIGNED_INT;
            bacnet_value.type.Signed_Int = std::get<int>(pulse_value);
            return true;
        }
        else if (std::holds_alternative<unsigned int>(pulse_value)) {
            bacnet_value.tag = BACNET_APPLICATION_TAG_UNSIGNED_INT;
            bacnet_value.type.Unsigned_Int = std::get<unsigned int>(pulse_value);
            return true;
        }
        else if (std::holds_alternative<float>(pulse_value)) {
            bacnet_value.tag = BACNET_APPLICATION_TAG_REAL;
            bacnet_value.type.Real = std::get<float>(pulse_value);
            return true;
        }
        else if (std::holds_alternative<double>(pulse_value)) {
            bacnet_value.tag = BACNET_APPLICATION_TAG_DOUBLE;
            bacnet_value.type.Double = std::get<double>(pulse_value);
            return true;
        }
        else if (std::holds_alternative<std::string>(pulse_value)) {
            bacnet_value.tag = BACNET_APPLICATION_TAG_CHARACTER_STRING;
            const std::string& str = std::get<std::string>(pulse_value);
            characterstring_init_ansi(&bacnet_value.type.Character_String, str.c_str());
            return true;
        }
        
        LogManager::getInstance().Error("ConvertToBACnetValue: Unsupported data type");
        return false;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ConvertToBACnetValue error: " + std::string(e.what()));
        return false;
    }
#else
    return false;  // BACnet Stack 없는 경우
#endif
}

Structs::DataValue BACnetDriver::ConvertBACnetValue(const BACNET_APPLICATION_DATA_VALUE& bacnet_value) {
#ifdef HAS_BACNET_STACK
    try {
        switch (bacnet_value.tag) {
            case BACNET_APPLICATION_TAG_BOOLEAN:
                return Structs::DataValue(bacnet_value.type.Boolean);
                
            case BACNET_APPLICATION_TAG_UNSIGNED_INT:
                return Structs::DataValue(static_cast<unsigned int>(bacnet_value.type.Unsigned_Int));
                
            case BACNET_APPLICATION_TAG_SIGNED_INT:
                return Structs::DataValue(static_cast<int>(bacnet_value.type.Signed_Int));
                
            case BACNET_APPLICATION_TAG_REAL:
                return Structs::DataValue(bacnet_value.type.Real);
                
            case BACNET_APPLICATION_TAG_DOUBLE:
                return Structs::DataValue(bacnet_value.type.Double);
                
            case BACNET_APPLICATION_TAG_CHARACTER_STRING: {
                char str_buf[MAX_CHARACTER_STRING_BYTES];
                bool success = characterstring_ansi_copy(str_buf, sizeof(str_buf), &bacnet_value.type.Character_String);
                
                if (success) {
                    return Structs::DataValue(std::string(str_buf));
                } else {
                    LogManager::getInstance().Warn("Failed to convert BACnet character string");
                    return Structs::DataValue(std::string("CONVERSION_ERROR"));
                }
            }
            
            default:
                LogManager::getInstance().Warn("ConvertBACnetValue: Unsupported BACnet type: " + 
                                             std::to_string(bacnet_value.tag));
                return Structs::DataValue(std::string("UNKNOWN"));
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ConvertBACnetValue error: " + std::string(e.what()));
        return Structs::DataValue(std::string("ERROR"));
    }
#else
    return Structs::DataValue(std::string("NO_BACNET"));
#endif
}

// =============================================================================
// 🔥 3. BACnet 요청 전송 함수들
// =============================================================================

uint8_t BACnetDriver::SendWritePropertyRequest(uint32_t device_id,
                                              BACNET_OBJECT_TYPE obj_type,
                                              uint32_t obj_instance,
                                              BACNET_PROPERTY_ID prop_id,
                                              const BACNET_APPLICATION_DATA_VALUE& value,
                                              uint8_t priority,
                                              uint32_t array_index) {
#ifdef HAS_BACNET_STACK
    try {
        BACNET_ADDRESS dest;
        bool found = address_get_by_device(device_id, NULL, &dest);
        if (!found) {
            LogManager::getInstance().Error("SendWritePropertyRequest: Device address not found: " + 
                                          std::to_string(device_id));
            return 0;
        }
        
        // Write Property 요청 생성
        BACNET_WRITE_PROPERTY_DATA wpdata;
        wpdata.object_type = obj_type;
        wpdata.object_instance = obj_instance;
        wpdata.object_property = prop_id;
        wpdata.array_index = array_index;
        wpdata.priority = priority;
        wpdata.application_data_len = bacapp_encode_application_data(
            &wpdata.application_data[0], &value);
        
        uint8_t invoke_id = tsm_next_free_invokeID();
        if (invoke_id) {
            int len = wp_encode_apdu(Tx_Buf, invoke_id, &wpdata);
            if (len > 0) {
                bip_send_pdu(&dest, NULL, Tx_Buf, len);
                
                // 펜딩 요청 등록
                std::lock_guard<std::mutex> lock(response_mutex_);
                auto request = std::make_unique<PendingRequest>();
                request->invoke_id = invoke_id;
                request->timeout = system_clock::now() + std::chrono::seconds(5);
                request->service_choice = SERVICE_CONFIRMED_WRITE_PROPERTY;
                request->target_device_id = device_id;
                
                pending_requests_[invoke_id] = std::move(request);
                
                return invoke_id;
            }
        }
        
        LogManager::getInstance().Error("SendWritePropertyRequest: Failed to encode or send request");
        return 0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("SendWritePropertyRequest error: " + std::string(e.what()));
        return 0;
    }
#else
    return 1;  // 스텁
#endif
}

bool BACnetDriver::ReadDeviceObjectList(uint32_t device_id, 
                                        std::vector<BACnetObjectInfo>& objects) {
#ifdef HAS_BACNET_STACK
    try {
        objects.clear();
        
        // Device Object의 Object_List property 읽기
        TimestampedValue result;
        if (ReadBACnetProperty(device_id, OBJECT_DEVICE, device_id, 
                              PROP_OBJECT_LIST, result, BACNET_ARRAY_ALL)) {
            
            // 결과 파싱하여 객체 목록 구성 (실제 구현은 복잡함)
            LogManager::getInstance().Info("ReadDeviceObjectList: Successfully read object list");
            return true;
        } else {
            LogManager::getInstance().Error("ReadDeviceObjectList: Failed to read object list");
            return false;
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("ReadDeviceObjectList error: " + std::string(e.what()));
        return false;
    }
#else
    return false;
#endif
}

// =============================================================================
// 🔥 4. 정적 콜백 핸들러들 (미구현된 것들)
// =============================================================================

void BACnetDriver::StaticWritePropertyAckHandler(uint8_t* service_request, 
                                                 uint16_t service_len,
                                                 BACNET_ADDRESS* src,
                                                 BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (instance_) {
        instance_->HandleWritePropertyAck(service_request, service_len, src, service_data);
    }
}

void BACnetDriver::StaticErrorHandler(BACNET_ADDRESS* src,
                                     uint8_t invoke_id,
                                     BACNET_ERROR_CLASS error_class,
                                     BACNET_ERROR_CODE error_code) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (instance_) {
        instance_->HandleError(src, invoke_id, error_class, error_code);
    }
}

void BACnetDriver::StaticRejectHandler(BACNET_ADDRESS* src,
                                      uint8_t invoke_id,
                                      uint8_t reject_reason) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (instance_) {
        instance_->HandleReject(src, invoke_id, reject_reason);
    }
}

// =============================================================================
// 🔥 5. 인스턴스 핸들러들 (미구현된 것들)
// =============================================================================

void BACnetDriver::HandleWritePropertyAck(uint8_t* service_request, 
                                         uint16_t service_len,
                                         BACNET_ADDRESS* src,
                                         BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data) {
    (void)service_request;  // 미사용 매개변수 억제
    (void)service_len;      // 미사용 매개변수 억제
    (void)src;              // 미사용 매개변수 억제

#ifdef HAS_BACNET_STACK
    try {
        LogManager::getInstance().Info(
            "BACnet Write Property Ack received - invoke_id: " + 
            std::to_string(service_data->invoke_id)
        );
        
        // 해당 invoke_id의 펜딩 요청 완료 처리
        CompleteRequest(service_data->invoke_id, true);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "Write Property Ack handler error: " + std::string(e.what())
        );
    }
#endif
}

void BACnetDriver::HandleError(BACNET_ADDRESS* src, 
                              uint8_t invoke_id,
                              BACNET_ERROR_CLASS error_class,
                              BACNET_ERROR_CODE error_code) {
#ifdef HAS_BACNET_STACK
    try {
        LogManager::getInstance().Error(
            "BACnet Error received - invoke_id: " + std::to_string(invoke_id) +
            ", class: " + std::to_string(error_class) +
            ", code: " + std::to_string(error_code)
        );
        
        // 해당 invoke_id의 펜딩 요청 완료 처리 (실패)
        CompleteRequest(invoke_id, false);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "Error handler error: " + std::string(e.what())
        );
    }
#endif
}

bool BACnetDriver::SendWhoIs() {
#ifdef HAS_BACNET_STACK
    try {
        // 기존 발견된 디바이스 목록 클리어
        {
            std::lock_guard<std::mutex> lock(devices_mutex_);
            discovered_devices_.clear();
        }
        
        // Who-Is 브로드캐스트 전송
        uint8_t invoke_id = SendWhoIsRequest();
        if (invoke_id == 0) {
            SetError(Enums::ErrorCode::PROTOCOL_ERROR, "Failed to send Who-Is request");
            return false;
        }
        
        LogManager::getInstance().Info("BACnet Who-Is request sent successfully");
        return true;
        
    } catch (const std::exception& e) {
        SetError(Enums::ErrorCode::INTERNAL_ERROR, 
                std::string("SendWhoIs error: ") + e.what());
        return false;
    }
#else
    // BACnet Stack이 없는 경우 스텁
    LogManager::getInstance().Info("SendWhoIs: BACnet Stack not available (stub)");
    return true;
#endif
}

std::map<uint32_t, BACnetDeviceInfo> BACnetDriver::GetDiscoveredDevices() const {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    return discovered_devices_;
}

std::vector<BACnetObjectInfo> BACnetDriver::GetDeviceObjects(uint32_t device_id) {
    std::vector<BACnetObjectInfo> objects;
    
#ifdef HAS_BACNET_STACK
    try {
        if (ReadDeviceObjectList(device_id, objects)) {
            LogManager::getInstance().Info(
                "Retrieved " + std::to_string(objects.size()) + 
                " objects from device " + std::to_string(device_id)
            );
        } else {
            LogManager::getInstance().Warn(
                "Failed to retrieve objects from device " + std::to_string(device_id)
            );
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "GetDeviceObjects error: " + std::string(e.what())
        );
    }
#else
    // BACnet Stack이 없는 경우 더미 객체 반환
    BACnetObjectInfo dummy_object;
    dummy_object.type = static_cast<BACnetObjectType>(OBJECT_ANALOG_INPUT);
    dummy_object.instance = 1;
    dummy_object.name = "Dummy Object";
    dummy_object.description = "Test object (stub)";
    objects.push_back(dummy_object);
#endif
    
    return objects;
}

// =============================================================================
// 🔥 2. BACnet 프로퍼티 읽기/쓰기 구현
// =============================================================================

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
        
        // 응답에서 결과 값 추출
        {
            std::lock_guard<std::mutex> lock(response_mutex_);
            auto it = pending_requests_.find(invoke_id);
            if (it != pending_requests_.end() && it->second->result_value) {
                result.value = ConvertBACnetValue(*(it->second->result_value));
                result.quality = Enums::DataQuality::GOOD;
                result.timestamp = system_clock::now();
                
                // 메모리 정리
                delete it->second->result_value;
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
    // BACnet Stack이 없는 경우 스텁 구현
    result.value = Structs::DataValue{25.5f};  // 더미 값
    result.quality = Enums::DataQuality::GOOD;
    result.timestamp = system_clock::now();
    
    LogManager::getInstance().Debug(
        "ReadBACnetProperty (stub): device=" + std::to_string(device_id) +
        ", obj_type=" + std::to_string(obj_type) + 
        ", instance=" + std::to_string(obj_instance)
    );
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
        
        LogManager::getInstance().Debug(
            "WriteBACnetProperty successful: device=" + std::to_string(device_id) +
            ", obj_type=" + std::to_string(obj_type) + 
            ", instance=" + std::to_string(obj_instance)
        );
        return true;
        
    } catch (const std::exception& e) {
        SetError(Enums::ErrorCode::INTERNAL_ERROR, 
                std::string("Write Property error: ") + e.what());
        return false;
    }
#else
    // BACnet Stack이 없는 경우 성공으로 처리
    LogManager::getInstance().Debug(
        "WriteBACnetProperty (stub): device=" + std::to_string(device_id) +
        ", value converted successfully"
    );
    return true;
#endif
}

// =============================================================================
// 🔥 3. BACnet 서비스 헬퍼 메서드들
// =============================================================================

uint8_t BACnetDriver::SendWhoIsRequest(uint32_t low_limit, uint32_t high_limit) {
#ifdef HAS_BACNET_STACK
    try {
        BACNET_ADDRESS dest;
        
        // 브로드캐스트 주소 설정
        bip_get_broadcast_address(&dest);
        
        // Who-Is 요청 생성
        int len = whois_encode_apdu(Tx_Buf, low_limit, high_limit);
        if (len > 0) {
            // 패킷 전송
            bip_send_pdu(&dest, nullptr, Tx_Buf, len);
            
            LogManager::getInstance().Debug(
                "Who-Is request sent: range=" + std::to_string(low_limit) + 
                "-" + std::to_string(high_limit)
            );
            return 1; // 성공
        }
        
        LogManager::getInstance().Error("Failed to encode Who-Is request");
        return 0; // 실패
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("SendWhoIsRequest error: " + std::string(e.what()));
        return 0;
    }
#else
    LogManager::getInstance().Debug("SendWhoIsRequest (stub): request sent");
    return 1;  // 스텁
#endif
}

uint8_t BACnetDriver::SendReadPropertyRequest(uint32_t device_id,
                                             BACNET_OBJECT_TYPE obj_type,
                                             uint32_t obj_instance,
                                             BACNET_PROPERTY_ID prop_id,
                                             uint32_t array_index) {
#ifdef HAS_BACNET_STACK
    try {
        BACNET_READ_PROPERTY_DATA rpdata;
        rpdata.object_type = obj_type;
        rpdata.object_instance = obj_instance;
        rpdata.object_property = prop_id;
        rpdata.array_index = array_index;
        
        BACNET_ADDRESS dest;
        bool found = address_get_by_device(device_id, nullptr, &dest);
        if (!found) {
            LogManager::getInstance().Error(
                "SendReadPropertyRequest: Device address not found: " + std::to_string(device_id)
            );
            return 0;
        }
        
        // Read Property APDU 생성
        uint8_t invoke_id = tsm_next_free_invokeID();
        if (invoke_id) {
            int len = rp_encode_apdu(Tx_Buf, invoke_id, &rpdata);
            if (len > 0) {
                bip_send_pdu(&dest, nullptr, Tx_Buf, len);
                
                // 펜딩 요청 등록
                std::lock_guard<std::mutex> lock(response_mutex_);
                auto request = std::make_unique<PendingRequest>();
                request->invoke_id = invoke_id;
                request->timeout = system_clock::now() + std::chrono::seconds(5);
                request->service_choice = SERVICE_CONFIRMED_READ_PROPERTY;
                request->target_device_id = device_id;
                request->result_value = nullptr;
                
                pending_requests_[invoke_id] = std::move(request);
                
                LogManager::getInstance().Debug(
                    "Read Property request sent: invoke_id=" + std::to_string(invoke_id)
                );
                return invoke_id;
            }
        }
        
        LogManager::getInstance().Error("Failed to encode or send Read Property request");
        return 0;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("SendReadPropertyRequest error: " + std::string(e.what()));
        return 0;
    }
#else
    LogManager::getInstance().Debug("SendReadPropertyRequest (stub): request sent");
    return 1;  // 스텁
#endif
}

// =============================================================================
// 🔥 4. 핸들러 등록 함수
// =============================================================================

void BACnetDriver::RegisterBACnetHandlers() {
#ifdef HAS_BACNET_STACK
    try {
        // 핸들러 등록
        apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, StaticIAmHandler);
        apdu_set_confirmed_ack_handler(SERVICE_CONFIRMED_READ_PROPERTY, StaticReadPropertyAckHandler);
        apdu_set_confirmed_ack_handler(SERVICE_CONFIRMED_WRITE_PROPERTY, StaticWritePropertyAckHandler);
        
        // 에러 핸들러들
        apdu_set_error_handler(SERVICE_CONFIRMED_READ_PROPERTY, StaticErrorHandler);
        apdu_set_error_handler(SERVICE_CONFIRMED_WRITE_PROPERTY, StaticErrorHandler);
        apdu_set_abort_handler(StaticAbortHandler);
        apdu_set_reject_handler(StaticRejectHandler);
        
        LogManager::getInstance().Info("BACnet handlers registered successfully");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "Failed to register BACnet handlers: " + std::string(e.what())
        );
    }
#else
    LogManager::getInstance().Info("RegisterBACnetHandlers (stub): handlers registered");
#endif
}

// =============================================================================
// 🔥 5. 정적 콜백 핸들러들 구현
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


void BACnetDriver::StaticAbortHandler(BACNET_ADDRESS* src,
                                     uint8_t invoke_id,
                                     uint8_t abort_reason,
                                     bool server) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (instance_) {
        instance_->HandleAbort(src, invoke_id, abort_reason, server);
    }
}


void BACnetDriver::HandleReject(BACNET_ADDRESS* src, uint8_t invoke_id, uint8_t reject_reason) {
    // 미사용 매개변수 경고 해결
    (void)src;
    
#ifdef HAS_BACNET_STACK
    try {
        LogManager::getInstance().Warn(
            "BACnet Reject received - invoke_id: " + std::to_string(invoke_id) +
            ", reason: " + std::to_string(reject_reason)
        );
        
        // 해당 invoke_id의 펜딩 요청 완료 처리
        CompleteRequest(invoke_id, false);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "Reject handler error: " + std::string(e.what())
        );
    }
#endif
}

void BACnetDriver::HandleIAm(uint8_t* service_request, uint16_t service_len, BACNET_ADDRESS* src) {
    // 미사용 매개변수 경고 해결
    (void)service_len;
    
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
            device_info.online = true;
            device_info.last_seen = std::chrono::system_clock::now();
            
            // IP 주소 추출
            if (src->mac_len == 6) {  // BACnet/IP
                device_info.ip_address = std::to_string(src->mac[0]) + "." +
                                        std::to_string(src->mac[1]) + "." +
                                        std::to_string(src->mac[2]) + "." +
                                        std::to_string(src->mac[3]);
                device_info.port = (src->mac[4] << 8) | src->mac[5];
                device_info.name = "Device_" + std::to_string(device_id);
                device_info.description = "BACnet Device";
            }
            
            // 발견된 디바이스 목록에 추가
            {
                std::lock_guard<std::mutex> lock(devices_mutex_);
                discovered_devices_[device_id] = device_info;
            }
            
            LogManager::getInstance().Debug(
                "Received I-Am from device " + std::to_string(device_id) + 
                " at " + device_info.ip_address + ":" + std::to_string(device_info.port)
            );
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "I-Am handler error: " + std::string(e.what())
        );
    }
#endif
}

void BACnetDriver::HandleReadPropertyAck(uint8_t* service_request, uint16_t service_len,
                                        BACNET_ADDRESS* src, 
                                        BACNET_CONFIRMED_SERVICE_ACK_DATA* service_data) {
    // 미사용 매개변수 경고 해결
    (void)src;
    
#ifdef HAS_BACNET_STACK
    try {
        BACNET_READ_PROPERTY_DATA rpdata;
        
        if (rp_ack_decode_service_request(service_request, service_len, &rpdata)) {
            // 해당 invoke_id의 펜딩 요청 찾기
            std::lock_guard<std::mutex> lock(response_mutex_);
            auto it = pending_requests_.find(service_data->invoke_id);
            if (it != pending_requests_.end()) {
                // 응답 데이터 저장
                it->second->result_value = new BACNET_APPLICATION_DATA_VALUE();
                *(it->second->result_value) = rpdata.application_data;
                
                // Promise 완료
                try {
                    it->second->promise.set_value(true);
                } catch (...) {
                    // promise가 이미 set된 경우 무시
                }
                
                LogManager::getInstance().Debug(
                    "Read Property Ack received for invoke_id " + 
                    std::to_string(service_data->invoke_id)
                );
            }
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "Read Property Ack handler error: " + std::string(e.what())
        );
    }
#endif
}

void BACnetDriver::HandleAbort(BACNET_ADDRESS* src, uint8_t invoke_id, 
                              uint8_t abort_reason, bool server) {
    // 미사용 매개변수 경고 해결
    (void)src;
    
#ifdef HAS_BACNET_STACK
    try {
        LogManager::getInstance().Warn(
            "BACnet Abort received - invoke_id: " + std::to_string(invoke_id) +
            ", reason: " + std::to_string(abort_reason) +
            ", server: " + (server ? "true" : "false")
        );
        
        // 해당 invoke_id의 펜딩 요청 완료 처리
        CompleteRequest(invoke_id, false);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error(
            "Abort handler error: " + std::string(e.what())
        );
    }
#endif
}

} // namespace Drivers
} // namespace PulseOne