//=============================================================================
// collector/include/Common/UniversalDeviceMapper.h
// 🔥 Universal Device Mapper - 모든 프로토콜 디바이스 정보 통합 변환
//=============================================================================

#ifndef PULSEONE_COMMON_UNIVERSAL_DEVICE_MAPPER_H
#define PULSEONE_COMMON_UNIVERSAL_DEVICE_MAPPER_H

/**
 * @file UniversalDeviceMapper.h
 * @brief 모든 프로토콜의 디바이스 정보를 표준 DeviceInfo로 변환
 * @author PulseOne Development Team
 * @date 2025-08-04
 * @version 1.0.0
 * 
 * 🎯 목적:
 * - BACnetDeviceInfo → DeviceInfo 변환
 * - ModbusDeviceInfo → DeviceInfo 변환  
 * - MQTTDeviceInfo → DeviceInfo 변환
 * - DeviceInfo → Protocol 역변환
 * - 배치 변환 지원
 * - 타입 안전성 보장
 * 
 * 🔥 실제 프로젝트 구조체 활용:
 * - Structs::DeviceInfo (실제 구조체)
 * - BACnetDeviceInfo (실제 구조체)  
 * - Enums::ProtocolType (실제 enum)
 * - BasicTypes::UUID (실제 타입)
 */

// =============================================================================
// 🔥 실제 프로젝트 헤더들 포함
// =============================================================================
#include "Common/Structs.h"                      // Structs::DeviceInfo 실제 구조체
#include "Common/Enums.h"                        // Enums::ProtocolType 실제 enum
#include "Common/BasicTypes.h"                   // UUID 타입
#include "Drivers/Bacnet/BACnetCommonTypes.h"    // BACnetDeviceInfo 실제 구조체
#include "Utils/LogManager.h"                    // 실제 로깅 시스템

#include <vector>
#include <string>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <chrono>

// =============================================================================
// 🔥 타입 별칭 (실제 프로젝트 타입들)
// =============================================================================
namespace PulseOne {
namespace Common {

// 실제 구조체들
using DeviceInfo = PulseOne::Structs::DeviceInfo;
using BACnetDeviceInfo = PulseOne::Drivers::BACnetDeviceInfo;
using BACnetObjectInfo = PulseOne::Drivers::BACnetObjectInfo;

// 실제 타입들
using UUID = PulseOne::BasicTypes::UUID;
using ProtocolType = PulseOne::Enums::ProtocolType;
using Timestamp = PulseOne::BasicTypes::Timestamp;

// =============================================================================
// 🔥 각 프로토콜별 디바이스 정보 구조체들 (Workers에서 사용하는 실제 구조체들)
// =============================================================================

/**
 * @brief Modbus RTU 슬레이브 정보 (ModbusRtuWorker에서 사용)
 */
struct ModbusRtuSlaveInfo {
    int slave_id = 1;
    std::string device_name = "";
    bool is_online = false;
    uint32_t response_time_ms = 0;
    std::chrono::system_clock::time_point last_response;
    
    // 통계
    uint32_t total_requests = 0;
    uint32_t successful_requests = 0;
    uint32_t timeout_errors = 0;
    uint32_t crc_errors = 0;
    std::string last_error = "";
    
    // 네트워크 정보
    std::string endpoint = "";  // 시리얼 포트 (예: "/dev/ttyUSB0")
    int baud_rate = 9600;
    char parity = 'N';
    int data_bits = 8;
    int stop_bits = 1;
};

/**
 * @brief Modbus TCP 디바이스 정보 (ModbusTcpWorker에서 사용)
 */
struct ModbusTcpDeviceInfo {
    int slave_id = 1;
    std::string device_name = "";
    std::string ip_address = "";
    uint16_t port = 502;
    bool is_online = false;
    uint32_t response_time_ms = 0;
    std::chrono::system_clock::time_point last_response;
    
    // 통계
    uint32_t total_requests = 0;
    uint32_t successful_requests = 0;
    uint32_t timeout_errors = 0;
    std::string last_error = "";
    
    // 설정
    uint32_t timeout_ms = 3000;
    int max_retries = 3;
};

/**
 * @brief MQTT 디바이스 정보 (MQTTWorker에서 사용)
 */
struct MQTTDeviceInfo {
    std::string client_id = "";
    std::string broker_host = "";
    uint16_t broker_port = 1883;
    std::string username = "";
    std::string password = "";
    bool use_ssl = false;
    bool clean_session = true;
    int keep_alive_interval = 60;
    int default_qos = 1;
    
    // 구독 정보
    std::vector<std::string> subscribed_topics;
    
    // 연결 상태
    bool is_connected = false;
    std::chrono::system_clock::time_point last_connect_time;
    
    // 통계
    uint64_t messages_published = 0;
    uint64_t messages_received = 0;
    uint64_t connection_failures = 0;
    std::string last_error = "";
};

/**
 * @brief 매퍼 에러 정보
 */
struct MapperError {
    std::string error_code;
    std::string description;
    std::string source_field;
    std::string suggested_fix;
    
    MapperError(const std::string& code, const std::string& desc,
               const std::string& field = "", const std::string& fix = "")
        : error_code(code), description(desc), source_field(field), suggested_fix(fix) {}
};

/**
 * @brief 매퍼 결과 (에러 처리 포함)
 */
template<typename T>
struct MapperResult {
    bool success = false;
    T data;
    std::vector<MapperError> errors;
    std::vector<std::string> warnings;
    
    MapperResult() = default;
    
    MapperResult(const T& result_data) : success(true), data(result_data) {}
    
    void AddError(const MapperError& error) {
        errors.push_back(error);
        success = false;
    }
    
    void AddWarning(const std::string& warning) {
        warnings.push_back(warning);
    }
    
    bool HasErrors() const { return !errors.empty(); }
    bool HasWarnings() const { return !warnings.empty(); }
};

/**
 * @brief 배치 변환 결과
 */
template<typename T>
struct BatchMapperResult {
    std::vector<T> successful_results;
    std::vector<MapperError> failed_items;
    size_t total_processed = 0;
    
    double GetSuccessRate() const {
        return total_processed > 0 ? 
            (static_cast<double>(successful_results.size()) / total_processed) * 100.0 : 0.0;
    }
};

// =============================================================================
// 🔥 Universal Device Mapper 클래스
// =============================================================================

/**
 * @brief 범용 디바이스 정보 매퍼 (모든 프로토콜 지원)
 * @details 모든 프로토콜별 디바이스 정보를 표준 DeviceInfo로 변환하고
 *          역변환도 지원하는 통합 매퍼
 */
class UniversalDeviceMapper {
public:
    // =========================================================================
    // 🔥 BACnet 변환 (실제 구조체 사용)
    // =========================================================================
    
    /**
     * @brief BACnetDeviceInfo → DeviceInfo 변환
     * @param bacnet_info 실제 BACnetDeviceInfo 구조체
     * @return MapperResult<DeviceInfo> 변환 결과
     */
    static MapperResult<DeviceInfo> FromBACnet(const BACnetDeviceInfo& bacnet_info);
    
    /**
     * @brief DeviceInfo → BACnetDeviceInfo 역변환
     * @param device_info 표준 DeviceInfo 구조체
     * @return MapperResult<BACnetDeviceInfo> 역변환 결과
     */
    static MapperResult<BACnetDeviceInfo> ToBACnet(const DeviceInfo& device_info);
    
    /**
     * @brief BACnet 디바이스 배치 변환
     * @param bacnet_devices BACnet 디바이스 목록
     * @return BatchMapperResult<DeviceInfo> 배치 변환 결과
     */
    static BatchMapperResult<DeviceInfo> FromBACnetBatch(
        const std::vector<BACnetDeviceInfo>& bacnet_devices);
    
    // =========================================================================
    // 🔥 Modbus RTU 변환
    // =========================================================================
    
    /**
     * @brief ModbusRtuSlaveInfo → DeviceInfo 변환
     * @param modbus_info Modbus RTU 슬레이브 정보
     * @return MapperResult<DeviceInfo> 변환 결과
     */
    static MapperResult<DeviceInfo> FromModbusRtu(const ModbusRtuSlaveInfo& modbus_info);
    
    /**
     * @brief DeviceInfo → ModbusRtuSlaveInfo 역변환
     * @param device_info 표준 DeviceInfo 구조체
     * @return MapperResult<ModbusRtuSlaveInfo> 역변환 결과
     */
    static MapperResult<ModbusRtuSlaveInfo> ToModbusRtu(const DeviceInfo& device_info);
    
    // =========================================================================
    // 🔥 Modbus TCP 변환
    // =========================================================================
    
    /**
     * @brief ModbusTcpDeviceInfo → DeviceInfo 변환
     * @param modbus_info Modbus TCP 디바이스 정보
     * @return MapperResult<DeviceInfo> 변환 결과
     */
    static MapperResult<DeviceInfo> FromModbusTcp(const ModbusTcpDeviceInfo& modbus_info);
    
    /**
     * @brief DeviceInfo → ModbusTcpDeviceInfo 역변환
     * @param device_info 표준 DeviceInfo 구조체
     * @return MapperResult<ModbusTcpDeviceInfo> 역변환 결과
     */
    static MapperResult<ModbusTcpDeviceInfo> ToModbusTcp(const DeviceInfo& device_info);
    
    // =========================================================================
    // 🔥 MQTT 변환
    // =========================================================================
    
    /**
     * @brief MQTTDeviceInfo → DeviceInfo 변환
     * @param mqtt_info MQTT 디바이스 정보
     * @return MapperResult<DeviceInfo> 변환 결과
     */
    static MapperResult<DeviceInfo> FromMQTT(const MQTTDeviceInfo& mqtt_info);
    
    /**
     * @brief DeviceInfo → MQTTDeviceInfo 역변환
     * @param device_info 표준 DeviceInfo 구조체
     * @return MapperResult<MQTTDeviceInfo> 역변환 결과
     */
    static MapperResult<MQTTDeviceInfo> ToMQTT(const DeviceInfo& device_info);
    
    // =========================================================================
    // 🔥 프로토콜 감지 및 자동 변환
    // =========================================================================
    
    /**
     * @brief 프로토콜 타입 자동 감지
     * @param endpoint 엔드포인트 문자열 (예: "192.168.1.100:47808")
     * @param additional_info 추가 힌트 정보
     * @return ProtocolType 감지된 프로토콜
     */
    static ProtocolType DetectProtocolType(const std::string& endpoint,
                                          const std::unordered_map<std::string, std::string>& additional_info = {});
    
    /**
     * @brief DeviceInfo 유효성 검증
     * @param device_info 검증할 DeviceInfo
     * @return MapperResult<bool> 검증 결과 (true면 유효)
     */
    static MapperResult<bool> ValidateDeviceInfo(const DeviceInfo& device_info);
    
    /**
     * @brief 프로토콜별 필수 속성 확인
     * @param device_info 확인할 DeviceInfo
     * @return std::vector<std::string> 누락된 필수 속성 목록
     */
    static std::vector<std::string> GetMissingRequiredProperties(const DeviceInfo& device_info);
    
    // =========================================================================
    // 🔥 유틸리티 함수들
    // =========================================================================
    
    /**
     * @brief UUID 생성 (디바이스 ID 기반)
     * @param protocol_prefix 프로토콜 접두사 (예: "bacnet", "modbus")
     * @param device_identifier 디바이스 식별자
     * @return UUID 생성된 UUID
     */
    static UUID GenerateDeviceUUID(const std::string& protocol_prefix, 
                                  const std::string& device_identifier);
    
    /**
     * @brief 프로토콜별 기본 설정 적용
     * @param device_info 설정을 적용할 DeviceInfo (참조로 수정)
     * @param protocol 프로토콜 타입
     */
    static void ApplyProtocolDefaults(DeviceInfo& device_info, ProtocolType protocol);
    
    /**
     * @brief 매퍼 통계 정보
     */
    struct MapperStatistics {
        size_t total_conversions = 0;
        size_t successful_conversions = 0;
        size_t failed_conversions = 0;
        std::chrono::milliseconds total_processing_time{0};
        std::unordered_map<std::string, size_t> protocol_counts;
        
        double GetSuccessRate() const {
            return total_conversions > 0 ? 
                (static_cast<double>(successful_conversions) / total_conversions) * 100.0 : 0.0;
        }
        
        std::string GetSummary() const {
            std::ostringstream oss;
            oss << "Mapper Statistics:\n";
            oss << "  Total: " << total_conversions << "\n";
            oss << "  Success: " << successful_conversions << " (" 
                << std::fixed << std::setprecision(1) << GetSuccessRate() << "%)\n";
            oss << "  Failed: " << failed_conversions << "\n";
            oss << "  Processing Time: " << total_processing_time.count() << "ms\n";
            
            if (!protocol_counts.empty()) {
                oss << "  By Protocol:\n";
                for (const auto& [protocol, count] : protocol_counts) {
                    oss << "    " << protocol << ": " << count << "\n";
                }
            }
            return oss.str();
        }
    };
    
    /**
     * @brief 매퍼 통계 조회
     * @return MapperStatistics 현재 통계 정보
     */
    static MapperStatistics GetStatistics();
    
    /**
     * @brief 매퍼 통계 초기화
     */
    static void ResetStatistics();

private:
    // =========================================================================
    // 🔥 내부 헬퍼 함수들
    // =========================================================================
    
    /**
     * @brief 엔드포인트 문자열 파싱
     * @param endpoint "IP:PORT" 형식 문자열
     * @return std::pair<std::string, uint16_t> IP 주소와 포트
     */
    static std::pair<std::string, uint16_t> ParseEndpoint(const std::string& endpoint);
    
    /**
     * @brief Properties 맵을 안전하게 문자열로 변환
     * @param properties Properties 맵
     * @param key 찾을 키
     * @param default_value 기본값
     * @return std::string 변환된 문자열
     */
    static std::string SafeGetProperty(const std::unordered_map<std::string, std::string>& properties,
                                      const std::string& key, 
                                      const std::string& default_value = "");
    
    /**
     * @brief Properties 맵을 안전하게 숫자로 변환
     * @param properties Properties 맵
     * @param key 찾을 키
     * @param default_value 기본값
     * @return T 변환된 숫자 타입
     */
    template<typename T>
    static T SafeGetNumericProperty(const std::unordered_map<std::string, std::string>& properties,
                                   const std::string& key,
                                   T default_value = T{});
    
    /**
     * @brief 통계 업데이트 (스레드 안전)
     * @param success 성공 여부
     * @param protocol 프로토콜 이름
     * @param processing_time 처리 시간
     */
    static void UpdateStatistics(bool success, const std::string& protocol,
                               std::chrono::milliseconds processing_time);
    
    // 내부 통계 (스레드 안전을 위한 뮤텍스와 함께)
    static MapperStatistics statistics_;
    static std::mutex statistics_mutex_;
};

// =============================================================================
// 🔥 템플릿 함수 구현 (헤더에 포함되어야 함)
// =============================================================================

template<typename T>
T UniversalDeviceMapper::SafeGetNumericProperty(
    const std::unordered_map<std::string, std::string>& properties,
    const std::string& key,
    T default_value) {
    
    auto it = properties.find(key);
    if (it == properties.end() || it->second.empty()) {
        return default_value;
    }
    
    try {
        if constexpr (std::is_same_v<T, int> || std::is_same_v<T, int32_t>) {
            return static_cast<T>(std::stoi(it->second));
        } else if constexpr (std::is_same_v<T, long> || std::is_same_v<T, int64_t>) {
            return static_cast<T>(std::stol(it->second));
        } else if constexpr (std::is_same_v<T, unsigned int> || std::is_same_v<T, uint32_t>) {
            return static_cast<T>(std::stoul(it->second));
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            auto val = std::stoul(it->second);
            return static_cast<T>(val <= 65535 ? val : default_value);
        } else if constexpr (std::is_same_v<T, uint8_t>) {
            auto val = std::stoul(it->second);
            return static_cast<T>(val <= 255 ? val : default_value);
        } else if constexpr (std::is_same_v<T, double>) {
            return static_cast<T>(std::stod(it->second));
        } else if constexpr (std::is_same_v<T, float>) {
            return static_cast<T>(std::stof(it->second));
        } else {
            // 지원하지 않는 타입
            return default_value;
        }
    } catch (const std::exception&) {
        // 변환 실패 시 기본값 반환
        LogManager::getInstance().Warn(
            "Failed to convert property '" + key + "' value '" + 
            it->second + "' to numeric type, using default value");
        return default_value;
    }
}

} // namespace Common
} // namespace PulseOne

#endif // PULSEONE_COMMON_UNIVERSAL_DEVICE_MAPPER_H