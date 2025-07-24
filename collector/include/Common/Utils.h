#ifndef PULSEONE_COMMON_UTILS_H
#define PULSEONE_COMMON_UTILS_H

/**
 * @file Utils.h
 * @brief PulseOne 유틸리티 함수들 (점검 기능 포함)
 * @author PulseOne Development Team  
 * @date 2025-07-24
 */

#include "BasicTypes.h"
#include "Enums.h"
#include "Constants.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>

namespace PulseOne::Utils {
    
    // 필요한 타입들을 네임스페이스에서 가져오기
    using namespace PulseOne::BasicTypes;
    using namespace PulseOne::Enums;
    
    // ========================================
    // 문자열 변환 함수들
    // ========================================
    
    /**
     * @brief 프로토콜 타입을 문자열로 변환
     */
    inline std::string ProtocolTypeToString(ProtocolType type) {
        switch (type) {
            case ProtocolType::MODBUS_TCP: return "MODBUS_TCP";
            case ProtocolType::MODBUS_RTU: return "MODBUS_RTU";
            case ProtocolType::MQTT: return "MQTT";
            case ProtocolType::BACNET_IP: return "BACNET_IP";
            case ProtocolType::BACNET_MSTP: return "BACNET_MSTP";
            case ProtocolType::OPC_UA: return "OPC_UA";
            case ProtocolType::ETHERNET_IP: return "ETHERNET_IP";
            case ProtocolType::CUSTOM: return "CUSTOM";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * @brief 문자열을 프로토콜 타입으로 변환
     */
    inline ProtocolType StringToProtocolType(const std::string& str) {
        std::string upper = str;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        
        if (upper == "MODBUS_TCP") return ProtocolType::MODBUS_TCP;
        if (upper == "MODBUS_RTU") return ProtocolType::MODBUS_RTU;
        if (upper == "MQTT") return ProtocolType::MQTT;
        if (upper == "BACNET_IP") return ProtocolType::BACNET_IP;
        if (upper == "BACNET_MSTP") return ProtocolType::BACNET_MSTP;
        if (upper == "OPC_UA") return ProtocolType::OPC_UA;
        if (upper == "ETHERNET_IP") return ProtocolType::ETHERNET_IP;
        if (upper == "CUSTOM") return ProtocolType::CUSTOM;
        return ProtocolType::UNKNOWN;
    }
    
    /**
     * @brief 드라이버 로그 카테고리를 문자열로 변환 (🆕)
     */
    inline std::string DriverLogCategoryToString(DriverLogCategory category) {
        switch (category) {
            case DriverLogCategory::GENERAL: return "GENERAL";
            case DriverLogCategory::CONNECTION: return "CONNECTION";
            case DriverLogCategory::COMMUNICATION: return "COMMUNICATION";
            case DriverLogCategory::DATA_PROCESSING: return "DATA_PROCESSING";
            case DriverLogCategory::ERROR_HANDLING: return "ERROR_HANDLING";
            case DriverLogCategory::PERFORMANCE: return "PERFORMANCE";
            case DriverLogCategory::SECURITY: return "SECURITY";
            case DriverLogCategory::PROTOCOL_SPECIFIC: return "PROTOCOL_SPECIFIC";
            case DriverLogCategory::DIAGNOSTICS: return "DIAGNOSTICS";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * @brief 로그 레벨을 문자열로 변환 (기존 LogLevels.h 호환)
     */
    inline std::string LogLevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::TRACE: return "TRACE";
            case LogLevel::DEBUG_LEVEL: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARN: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::FATAL: return "FATAL";
            case LogLevel::MAINTENANCE: return "MAINTENANCE";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * @brief 문자열을 로그 레벨로 변환 (기존 LogLevels.h 호환)
     */
    inline LogLevel StringToLogLevel(const std::string& level) {
        std::string upper = level;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        
        if (upper == "TRACE") return LogLevel::TRACE;
        if (upper == "DEBUG") return LogLevel::DEBUG_LEVEL;
        if (upper == "INFO") return LogLevel::INFO;
        if (upper == "WARN") return LogLevel::WARN;
        if (upper == "ERROR") return LogLevel::ERROR;
        if (upper == "FATAL") return LogLevel::FATAL;
        if (upper == "MAINTENANCE") return LogLevel::MAINTENANCE;
        return LogLevel::INFO; // 기본값
    }
    inline std::string DataQualityToString(DataQuality quality) {
        switch (quality) {
            case DataQuality::GOOD: return "GOOD";
            case DataQuality::BAD: return "BAD";
            case DataQuality::UNCERTAIN: return "UNCERTAIN";
            case DataQuality::NOT_CONNECTED: return "NOT_CONNECTED";
            case DataQuality::SCAN_DELAYED: return "SCAN_DELAYED";
            case DataQuality::UNDER_MAINTENANCE: return "UNDER_MAINTENANCE";
            case DataQuality::STALE_DATA: return "STALE_DATA";
            case DataQuality::VERY_STALE_DATA: return "VERY_STALE_DATA";
            case DataQuality::MAINTENANCE_BLOCKED: return "MAINTENANCE_BLOCKED";
            case DataQuality::ENGINEER_OVERRIDE: return "ENGINEER_OVERRIDE";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * @brief 연결 상태를 문자열로 변환 (🆕)
     */
    inline std::string ConnectionStatusToString(ConnectionStatus status) {
        switch (status) {
            case ConnectionStatus::DISCONNECTED: return "DISCONNECTED";
            case ConnectionStatus::CONNECTING: return "CONNECTING";
            case ConnectionStatus::CONNECTED: return "CONNECTED";
            case ConnectionStatus::RECONNECTING: return "RECONNECTING";
            case ConnectionStatus::ERROR: return "ERROR";
            case ConnectionStatus::MAINTENANCE_MODE: return "MAINTENANCE_MODE";
            default: return "UNKNOWN";
        }
    }
    inline std::string MaintenanceStatusToString(MaintenanceStatus status) {
        switch (status) {
            case MaintenanceStatus::NORMAL: return "NORMAL";
            case MaintenanceStatus::MAINTENANCE_REQUESTED: return "MAINTENANCE_REQUESTED";
            case MaintenanceStatus::UNDER_MAINTENANCE: return "UNDER_MAINTENANCE";
            case MaintenanceStatus::MAINTENANCE_COMPLETED: return "MAINTENANCE_COMPLETED";
            case MaintenanceStatus::EMERGENCY_STOP: return "EMERGENCY_STOP";
            case MaintenanceStatus::REMOTE_BLOCKED: return "REMOTE_BLOCKED";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * @brief 에러 코드를 문자열로 변환
     */
    inline std::string ErrorCodeToString(ErrorCode code) {
        switch (code) {
            case ErrorCode::SUCCESS: return "SUCCESS";
            case ErrorCode::CONNECTION_FAILED: return "CONNECTION_FAILED";
            case ErrorCode::CONNECTION_TIMEOUT: return "CONNECTION_TIMEOUT";
            case ErrorCode::CONNECTION_LOST: return "CONNECTION_LOST";
            case ErrorCode::PROTOCOL_ERROR: return "PROTOCOL_ERROR";
            case ErrorCode::DATA_FORMAT_ERROR: return "DATA_FORMAT_ERROR";
            case ErrorCode::MAINTENANCE_ACTIVE: return "MAINTENANCE_ACTIVE";
            case ErrorCode::REMOTE_CONTROL_BLOCKED: return "REMOTE_CONTROL_BLOCKED";
            case ErrorCode::INSUFFICIENT_PERMISSION: return "INSUFFICIENT_PERMISSION";
            case ErrorCode::RESOURCE_EXHAUSTED: return "RESOURCE_EXHAUSTED";
            case ErrorCode::INTERNAL_ERROR: return "INTERNAL_ERROR";
            default: return "UNKNOWN_ERROR";
        }
    }
    
    // ========================================
    // UUID 생성 함수들
    // ========================================
    
    /**
     * @brief UUID 생성 (간단한 형태)
     */
    inline UUID GenerateUUID() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        static std::uniform_int_distribution<> dis2(8, 11);
        
        std::stringstream ss;
        ss << std::hex;
        for (int i = 0; i < 8; i++) ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 4; i++) ss << dis(gen);
        ss << "-4";
        for (int i = 0; i < 3; i++) ss << dis(gen);
        ss << "-";
        ss << dis2(gen);
        for (int i = 0; i < 3; i++) ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 12; i++) ss << dis(gen);
        
        return ss.str();
    }
    
    /**
     * @brief 디바이스 ID 생성 (프로토콜별)
     */
    inline UUID GenerateDeviceID(ProtocolType protocol, const std::string& endpoint) {
        std::string prefix = ProtocolTypeToString(protocol);
        std::hash<std::string> hasher;
        size_t hash = hasher(endpoint);
        
        std::stringstream ss;
        ss << prefix << "-" << std::hex << hash << "-" << std::hex << std::time(nullptr);
        return ss.str();
    }
    
    // ========================================
    // 시간 관련 함수들
    // ========================================
    
    /**
     * @brief 타임스탬프를 ISO 8601 문자열로 변환
     */
    inline std::string TimestampToISOString(const Timestamp& ts) {
        auto time_t = std::chrono::system_clock::to_time_t(ts);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            ts.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
        return ss.str();
    }
    
    /**
     * @brief 현재 시간 가져오기
     */
    inline Timestamp GetCurrentTimestamp() {
        return std::chrono::system_clock::now();
    }
    
    /**
     * @brief 두 시간 사이의 간격 계산
     */
    inline Duration GetDurationSince(const Timestamp& start) {
        return std::chrono::duration_cast<Duration>(
            GetCurrentTimestamp() - start);
    }
    
    /**
     * @brief 데이터 나이 계산 (🆕 점검 기능)
     */
    inline Duration CalculateDataAge(const Timestamp& data_timestamp) {
        return GetDurationSince(data_timestamp);
    }

    /**
     * @brief Duration을 밀리초로 변환 (기존 timeout_ms 대체)
     */
    inline uint32_t DurationToMs(const Duration& duration) {
        return static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
        );
    }
     
    /**
     * @brief 밀리초를 Duration으로 변환
     */
    inline Duration MsToDuration(uint32_t ms) {
        return std::chrono::milliseconds(ms);
    }
    
    /**
     * @brief endpoint에서 IP 추출
     */
    inline std::string ExtractIPFromEndpoint(const std::string& endpoint) {
        size_t colon_pos = endpoint.find(':');
        return (colon_pos != std::string::npos) ? endpoint.substr(0, colon_pos) : endpoint;
    }
    
    /**
     * @brief endpoint에서 포트 추출
     */
    inline uint16_t ExtractPortFromEndpoint(const std::string& endpoint, uint16_t default_port = 502) {
        size_t colon_pos = endpoint.find(':');
        if (colon_pos != std::string::npos) {
            std::string port_str = endpoint.substr(colon_pos + 1);
            size_t slash_pos = port_str.find('/');
            if (slash_pos != std::string::npos) {
                port_str = port_str.substr(0, slash_pos);
            }
            return static_cast<uint16_t>(std::stoi(port_str));
        }
        return default_port;
    }   
    // ========================================
    // 점검 관련 유틸리티 함수들 (🆕)
    // ========================================
    
    /**
     * @brief 점검 권한 확인
     */
    inline bool IsAuthorizedEngineer(const std::vector<EngineerID>& authorized_list, 
                                   const EngineerID& engineer_id) {
        if (authorized_list.empty()) return true;  // 제한 없음
        return std::find(authorized_list.begin(), authorized_list.end(), 
                        engineer_id) != authorized_list.end();
    }
    
    /**
     * @brief 점검 시간이 만료되었는지 확인
     */
    inline bool IsMaintenanceExpired(const Timestamp& start_time, 
                                   const Duration& max_duration) {
        return GetDurationSince(start_time) > max_duration;
    }
    
    /**
     * @brief 안전한 점검 해제 가능 여부 확인
     */
    inline bool CanSafelyReleaseMaintenance(const Timestamp& start_time,
                                          const Duration& min_duration = std::chrono::minutes(5)) {
        return GetDurationSince(start_time) >= min_duration;
    }
    
    // ========================================
    // 데이터 변환 함수들
    // ========================================
    
    /**
     * @brief DataVariant를 문자열로 변환
     */
    inline std::string DataVariantToString(const DataVariant& value) {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, bool>) {
                return arg ? "true" : "false";
            } else if constexpr (std::is_same_v<T, std::string>) {
                return arg;
            } else {
                return std::to_string(arg);
            }
        }, value);
    }
    
    /**
     * @brief 문자열을 DataVariant로 변환 (타입 추론)
     */
    inline DataVariant StringToDataVariant(const std::string& str, const std::string& type_hint = "") {
        if (type_hint == "bool" || str == "true" || str == "false") {
            return str == "true";
        }
        
        // 정수 시도
        try {
            if (str.find('.') == std::string::npos) {
                return std::stoi(str);
            }
        } catch (...) {}
        
        // 실수 시도
        try {
            return std::stod(str);
        } catch (...) {}
        
        // 문자열로 처리
        return str;
    }
    
    // ========================================
    // 검증 함수들
    // ========================================
    
    /**
     * @brief 엔드포인트 형식 검증
     */
    inline bool IsValidEndpoint(const std::string& endpoint, ProtocolType protocol) {
        if (endpoint.empty()) return false;
        
        switch (protocol) {
            case ProtocolType::MODBUS_TCP:
            case ProtocolType::MQTT:
                // IP:PORT 형식 확인
                return endpoint.find(':') != std::string::npos;
            case ProtocolType::MODBUS_RTU:
                // 시리얼 포트 형식 확인 (예: COM1, /dev/ttyUSB0)
                return endpoint.find("COM") != std::string::npos || 
                       endpoint.find("/dev/") != std::string::npos;
            default:
                return true;  // 기타 프로토콜은 관대하게 처리
        }
    }
    
    /**
     * @brief 엔지니어 ID 형식 검증
     */
    inline bool IsValidEngineerID(const EngineerID& engineer_id) {
        return !engineer_id.empty() && 
               engineer_id.length() <= 64 &&
               engineer_id != "UNKNOWN" &&
               engineer_id != "SYSTEM";
    }
    
} // namespace PulseOne::Utils

#endif // PULSEONE_COMMON_UTILS_H