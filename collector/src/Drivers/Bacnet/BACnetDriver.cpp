// =============================================================================
// src/Drivers/Bacnet/BACnetDriver.cpp
// ModbusDriver 구조를 복사한 최소한의 BACnet Driver 구현
// =============================================================================

#include "Drivers/Bacnet/BACnetDriver.h"
#include <iostream>
#include <chrono>

using namespace std::chrono;

namespace PulseOne {
namespace Drivers {

// =============================================================================
// 생성자 및 소멸자 (ModbusDriver와 유사)
// =============================================================================

BACnetDriver::BACnetDriver() {
    // 통계 초기화 (ModbusDriver와 동일)
    statistics_.total_operations = 0;
    statistics_.successful_operations = 0;
    statistics_.failed_operations = 0;
    statistics_.success_rate = 0.0;
    statistics_.avg_response_time_ms = 0.0;
    statistics_.last_connection_time = system_clock::now();
    
    // 에러 초기화
    last_error_.code = Enums::ErrorCode::SUCCESS;
    last_error_.message = "";
}

BACnetDriver::~BACnetDriver() {
    Disconnect();
    // BACnet context 정리 (나중에 실제 구현)
    if (bacnet_ctx_) {
        // bacnet_free(bacnet_ctx_);  // 실제 BACnet 함수로 교체
        bacnet_ctx_ = nullptr;
    }
}

// =============================================================================
// IProtocolDriver 인터페이스 구현 (ModbusDriver 패턴 복사)
// =============================================================================

bool BACnetDriver::Initialize(const DriverConfig& config) {
    config_ = config;
    
    // BACnet context 생성 (현재는 스텁)
    if (config.protocol == Enums::ProtocolType::BACNET_IP) {
        // 엔드포인트에서 주소와 포트 파싱 (ModbusDriver와 동일한 패턴)
        std::string host = "127.0.0.1";
        int port = 47808;  // BACnet 기본 포트
        
        if (!config.endpoint.empty()) {
            size_t colon_pos = config.endpoint.find(':');
            if (colon_pos != std::string::npos) {
                host = config.endpoint.substr(0, colon_pos);
                port = std::stoi(config.endpoint.substr(colon_pos + 1));
            }
        }
        
        // BACnet context 생성 (스텁)
        bacnet_ctx_ = (void*)0x1;  // 더미 포인터
        
    } else {
        SetError(Enums::ErrorCode::INVALID_PARAMETER, "Unsupported protocol type");
        return false;
    }
    
    if (!bacnet_ctx_) {
        SetError(Enums::ErrorCode::INTERNAL_ERROR, "Failed to create BACnet context");
        return false;
    }
    
    return true;
}

bool BACnetDriver::Connect() {
    if (is_connected_) return true;
    
    if (!bacnet_ctx_) {
        SetError(Enums::ErrorCode::CONNECTION_FAILED, "BACnet context not initialized");
        return false;
    }
    
    // BACnet 연결 시도 (현재는 스텁)
    // if (bacnet_connect(bacnet_ctx_) == 0) {  // 실제 BACnet 함수로 교체
    if (true) {  // 스텁: 항상 성공
        is_connected_ = true;
        
        UpdateStatistics(true, 0.0);
        return true;
    } else {
        SetError(Enums::ErrorCode::CONNECTION_FAILED, "BACnet connection failed");
        UpdateStatistics(false, 0.0);
        return false;
    }
}

bool BACnetDriver::Disconnect() {
    if (!is_connected_) return true;
    
    if (bacnet_ctx_) {
        // bacnet_close(bacnet_ctx_);  // 실제 BACnet 함수로 교체
    }
    is_connected_ = false;
    return true;
}

bool BACnetDriver::IsConnected() const {
    return is_connected_.load();
}

Enums::ProtocolType BACnetDriver::GetProtocolType() const {
    return config_.protocol;
}

Structs::DriverStatus BACnetDriver::GetStatus() const {
    return is_connected_ ? Structs::DriverStatus::RUNNING : Structs::DriverStatus::STOPPED;
}

bool BACnetDriver::ReadValues(const std::vector<Structs::DataPoint>& points,
                             std::vector<TimestampedValue>& values) {
    values.clear();
    
    if (!IsConnected()) {
        SetError(Enums::ErrorCode::CONNECTION_FAILED, "Not connected");
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (const auto& point : points) {
        TimestampedValue tvalue;
        tvalue.timestamp = system_clock::now();
        
        // BACnet 읽기 (현재는 스텁)
        float raw_value = 23.5f;  // 더미 값
        // int result = bacnet_read_property(bacnet_ctx_, point.address, &raw_value);  // 실제 구현
        int result = 1;  // 스텁: 항상 성공
        
        if (result == 1) {
            tvalue.value = ConvertBACnetValue(point, raw_value);
            tvalue.quality = Structs::DataQuality::GOOD;
        } else {
            tvalue.value = Structs::DataValue(0.0);
            tvalue.quality = Structs::DataQuality::BAD;
            
            // 에러 기록
            SetError(Enums::ErrorCode::DATA_FORMAT_ERROR, "Failed to read BACnet property " + std::to_string(point.address));
        }
        
        values.push_back(tvalue);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    UpdateStatistics(true, duration_ms);
    return true;
}

bool BACnetDriver::WriteValue(const Structs::DataPoint& point, 
                             const Structs::DataValue& value) {
    if (!IsConnected()) {
        SetError(Enums::ErrorCode::CONNECTION_FAILED, "Not connected");
        return false;
    }
    
    // BACnet 쓰기 (현재는 스텁)
    // float write_value = std::visit([](auto&& arg) -> float { return static_cast<float>(arg); }, value);
    // int result = bacnet_write_property(bacnet_ctx_, point.address, write_value);  // 실제 구현
    int result = 1;  // 스텁: 항상 성공
    
    if (result == 1) {
        UpdateStatistics(true, 0.0);
        return true;
    } else {
        SetError(Enums::ErrorCode::DATA_FORMAT_ERROR, "Failed to write BACnet property " + std::to_string(point.address));
        UpdateStatistics(false, 0.0);
        return false;
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
    statistics_ = DriverStatistics{};  // 기본값으로 리셋
}

// =============================================================================
// 헬퍼 메서드들 (ModbusDriver와 유사)
// =============================================================================

void BACnetDriver::SetError(Enums::ErrorCode code, const std::string& message) {
    last_error_.code = code;
    last_error_.message = message;
    last_error_.occurred_at = Utils::GetCurrentTimestamp();
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
    statistics_.success_rate = static_cast<double>(statistics_.successful_operations) / 
                              statistics_.total_operations * 100.0;
    
    // 평균 응답시간 계산
    statistics_.avg_response_time_ms = 
        (statistics_.avg_response_time_ms * (statistics_.total_operations - 1) + response_time_ms) / 
        statistics_.total_operations;
}

Structs::DataValue BACnetDriver::ConvertBACnetValue(const Structs::DataPoint& point, float raw_value) {
    // ModbusDriver와 유사한 변환 로직
    
    // 스케일링 적용 (커스텀 설정에서 가져오기)
    double scaling_factor = 1.0;
    double scaling_offset = 0.0;
    
    auto it = config_.custom_settings.find("scaling_factor");
    if (it != config_.custom_settings.end()) {
        scaling_factor = std::stod(it->second);
    }
    
    it = config_.custom_settings.find("scaling_offset");
    if (it != config_.custom_settings.end()) {
        scaling_offset = std::stod(it->second);
    }
    
    // 변환된 값 계산
    double converted_value = raw_value * scaling_factor + scaling_offset;
    
    // 데이터 타입에 따른 변환
    switch (point.data_type) {
        case Structs::DataType::BOOL:
            return Structs::DataValue(converted_value != 0.0);
        case Structs::DataType::INT16:
            return Structs::DataValue(static_cast<int16_t>(converted_value));
        case Structs::DataType::UINT16:
            return Structs::DataValue(static_cast<uint16_t>(converted_value));
        case Structs::DataType::INT32:
            return Structs::DataValue(static_cast<int32_t>(converted_value));
        case Structs::DataType::UINT32:
            return Structs::DataValue(static_cast<uint32_t>(converted_value));
        case Structs::DataType::FLOAT32:
            return Structs::DataValue(static_cast<float>(converted_value));
        case Structs::DataType::FLOAT64:
            return Structs::DataValue(converted_value);
        case Structs::DataType::STRING:
            return Structs::DataValue(std::to_string(converted_value));
        default:
            return Structs::DataValue(converted_value);
    }
}

} // namespace PulseOne::Drivers
} // namespace PulseOne