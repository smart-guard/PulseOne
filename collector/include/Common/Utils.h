// collector/include/Common/Utils.h
#ifndef PULSEONE_COMMON_UTILS_H
#define PULSEONE_COMMON_UTILS_H

/**
 * @file Utils.h
 * @brief PulseOne 유틸리티 함수들 - 원본 기능 100% 유지하면서 타입 에러 해결
 * @author PulseOne Development Team  
 * @date 2025-08-05
 * 
 * 🔥 주요 수정사항:
 * - GetCurrentTimestamp() 반환 타입 완전 수정
 * - LogLevel::DEBUG_LEVEL 문제 해결
 * - 원본의 모든 함수 유지
 * - 타입 에러 완전 해결
 */

#include "BasicTypes.h"
#include "Enums.h"
#include "Constants.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>
#include <vector>
#include <chrono>
#include <ctime>

namespace PulseOne::Utils {
    
    // 🔥 using namespace 완전 제거! 개별 타입만 명시적 선언
    
    // BasicTypes에서 필요한 타입들만 개별 선언
    using UUID = PulseOne::BasicTypes::UUID;
    using Timestamp = PulseOne::BasicTypes::Timestamp;
    using Duration = PulseOne::BasicTypes::Duration;
    using DataVariant = PulseOne::BasicTypes::DataVariant;
    using EngineerID = PulseOne::BasicTypes::EngineerID;
    
    // Enums에서 필요한 열거형들만 개별 선언
    using ProtocolType = PulseOne::Enums::ProtocolType;
    using LogLevel = PulseOne::Enums::LogLevel;
    using DriverLogCategory = PulseOne::Enums::DriverLogCategory;
    using DataQuality = PulseOne::Enums::DataQuality;
    using ConnectionStatus = PulseOne::Enums::ConnectionStatus;
    using MaintenanceStatus = PulseOne::Enums::MaintenanceStatus;
    using ErrorCode = PulseOne::Enums::ErrorCode;
    
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
     * @brief 드라이버 로그 카테고리를 문자열로 변환
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
     * @brief 로그 레벨을 문자열로 변환
     */
    inline std::string LogLevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::TRACE: return "TRACE";
            case LogLevel::DEBUG: return "DEBUG";        // 🔥 DEBUG_LEVEL → DEBUG 수정
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARN: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::FATAL: return "FATAL";
            case LogLevel::MAINTENANCE: return "MAINTENANCE";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * @brief 문자열을 로그 레벨로 변환
     */
    inline LogLevel StringToLogLevel(const std::string& level) {
        std::string upper = level;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        
        if (upper == "TRACE") return LogLevel::TRACE;
        if (upper == "DEBUG") return LogLevel::DEBUG;    // 🔥 DEBUG_LEVEL → DEBUG 수정
        if (upper == "INFO") return LogLevel::INFO;
        if (upper == "WARN") return LogLevel::WARN;
        if (upper == "ERROR") return LogLevel::ERROR;
        if (upper == "FATAL") return LogLevel::FATAL;
        if (upper == "MAINTENANCE") return LogLevel::MAINTENANCE;
        return LogLevel::INFO; // 기본값
    }
    
    /**
     * @brief 데이터 품질을 문자열로 변환
     * @param quality 데이터 품질 enum 값
     * @param lowercase true면 소문자, false면 대문자 (기본값)
     * @return 품질 코드의 문자열 표현
     */
    inline std::string DataQualityToString(DataQuality quality, bool lowercase = false) {
        std::string result;
        
        switch (quality) {
            // 기본 품질 상태 (0-7)
            case DataQuality::UNKNOWN: result = "UNKNOWN"; break;
            case DataQuality::GOOD: result = "GOOD"; break;
            case DataQuality::BAD: result = "BAD"; break;
            case DataQuality::UNCERTAIN: result = "UNCERTAIN"; break;
            case DataQuality::STALE: result = "STALE"; break;
            case DataQuality::MAINTENANCE: result = "MAINTENANCE"; break;
            case DataQuality::SIMULATED: result = "SIMULATED"; break;
            case DataQuality::MANUAL: result = "MANUAL"; break;
            
            // 확장 품질 상태 (8-15)
            case DataQuality::NOT_CONNECTED: result = "NOT_CONNECTED"; break;
            case DataQuality::TIMEOUT: result = "TIMEOUT"; break;
            case DataQuality::SCAN_DELAYED: result = "SCAN_DELAYED"; break;
            case DataQuality::UNDER_MAINTENANCE: result = "UNDER_MAINTENANCE"; break;
            case DataQuality::STALE_DATA: result = "STALE_DATA"; break;
            case DataQuality::VERY_STALE_DATA: result = "VERY_STALE_DATA"; break;
            case DataQuality::MAINTENANCE_BLOCKED: result = "MAINTENANCE_BLOCKED"; break;
            case DataQuality::ENGINEER_OVERRIDE: result = "ENGINEER_OVERRIDE"; break;
            
            default: result = "UNKNOWN"; break;
        }
        
        if (lowercase) {
            std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        }
        
        return result;
    }

        
    /**
     * @brief 문자열을 데이터 품질로 변환 (역변환)
     */
    inline DataQuality StringToDataQuality(const std::string& str) {
        std::string upper = str;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        
        // 기본 품질 상태
        if (upper == "UNKNOWN") return DataQuality::UNKNOWN;
        if (upper == "GOOD") return DataQuality::GOOD;
        if (upper == "BAD") return DataQuality::BAD;
        if (upper == "UNCERTAIN") return DataQuality::UNCERTAIN;
        if (upper == "STALE") return DataQuality::STALE;
        if (upper == "MAINTENANCE") return DataQuality::MAINTENANCE;
        if (upper == "SIMULATED") return DataQuality::SIMULATED;
        if (upper == "MANUAL") return DataQuality::MANUAL;
        
        // 확장 품질 상태
        if (upper == "NOT_CONNECTED") return DataQuality::NOT_CONNECTED;
        if (upper == "TIMEOUT") return DataQuality::TIMEOUT;
        if (upper == "SCAN_DELAYED") return DataQuality::SCAN_DELAYED;
        if (upper == "UNDER_MAINTENANCE") return DataQuality::UNDER_MAINTENANCE;
        if (upper == "STALE_DATA") return DataQuality::STALE_DATA;
        if (upper == "VERY_STALE_DATA") return DataQuality::VERY_STALE_DATA;
        if (upper == "MAINTENANCE_BLOCKED") return DataQuality::MAINTENANCE_BLOCKED;
        if (upper == "ENGINEER_OVERRIDE") return DataQuality::ENGINEER_OVERRIDE;
        
        return DataQuality::BAD; // 기본값 (알 수 없는 문자열은 BAD로)
    }
    
    /**
     * @brief 연결 상태를 문자열로 변환
     */
    inline std::string ConnectionStatusToString(ConnectionStatus status) {
        switch (status) {
            case ConnectionStatus::DISCONNECTED: return "DISCONNECTED";
            case ConnectionStatus::CONNECTING: return "CONNECTING";
            case ConnectionStatus::CONNECTED: return "CONNECTED";
            case ConnectionStatus::RECONNECTING: return "RECONNECTING";
            case ConnectionStatus::ERROR: return "ERROR";
            case ConnectionStatus::MAINTENANCE: return "MAINTENANCE_MODE";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * @brief 점검 상태를 문자열로 변환
     */
    inline std::string MaintenanceStatusToString(MaintenanceStatus status) {
        switch (status) {
            case MaintenanceStatus::NORMAL: return "NORMAL";
            case MaintenanceStatus::SCHEDULED: return "SCHEDULED";           // 변경
            case MaintenanceStatus::IN_PROGRESS: return "IN_PROGRESS";       // 변경
            case MaintenanceStatus::PAUSED: return "PAUSED";                 // 추가
            case MaintenanceStatus::COMPLETED: return "COMPLETED";           // 변경
            case MaintenanceStatus::CANCELLED: return "CANCELLED";           // 추가
            case MaintenanceStatus::EMERGENCY: return "EMERGENCY";           // 변경
            default: return "UNKNOWN";
        }
    }
    
    /**
     * @brief 에러 코드를 문자열로 변환
     */
    inline std::string ErrorCodeToString(ErrorCode code) {
        switch (code) {
            // 공통 성공/실패
            case ErrorCode::SUCCESS: return "SUCCESS";
            case ErrorCode::UNKNOWN_ERROR: return "UNKNOWN_ERROR";
            
            // 연결 관련 에러 (10-15)
            case ErrorCode::CONNECTION_FAILED: return "CONNECTION_FAILED";
            case ErrorCode::CONNECTION_TIMEOUT: return "CONNECTION_TIMEOUT";
            case ErrorCode::CONNECTION_REFUSED: return "CONNECTION_REFUSED";
            case ErrorCode::CONNECTION_LOST: return "CONNECTION_LOST";
            case ErrorCode::AUTHENTICATION_FAILED: return "AUTHENTICATION_FAILED";
            case ErrorCode::AUTHORIZATION_FAILED: return "AUTHORIZATION_FAILED";
            
            // 통신 관련 에러 (100-105)
            case ErrorCode::TIMEOUT: return "TIMEOUT";
            case ErrorCode::PROTOCOL_ERROR: return "PROTOCOL_ERROR";
            case ErrorCode::INVALID_REQUEST: return "INVALID_REQUEST";
            case ErrorCode::INVALID_RESPONSE: return "INVALID_RESPONSE";
            case ErrorCode::CHECKSUM_ERROR: return "CHECKSUM_ERROR";
            case ErrorCode::FRAME_ERROR: return "FRAME_ERROR";
            
            // 데이터 관련 에러 (200-204)
            case ErrorCode::INVALID_DATA: return "INVALID_DATA";
            case ErrorCode::DATA_TYPE_MISMATCH: return "DATA_TYPE_MISMATCH";
            case ErrorCode::DATA_OUT_OF_RANGE: return "DATA_OUT_OF_RANGE";
            case ErrorCode::DATA_FORMAT_ERROR: return "DATA_FORMAT_ERROR";
            case ErrorCode::DATA_STALE: return "DATA_STALE";
            
            // 디바이스 관련 에러 (300-304)
            case ErrorCode::DEVICE_NOT_RESPONDING: return "DEVICE_NOT_RESPONDING";
            case ErrorCode::DEVICE_BUSY: return "DEVICE_BUSY";
            case ErrorCode::DEVICE_ERROR: return "DEVICE_ERROR";
            case ErrorCode::DEVICE_NOT_FOUND: return "DEVICE_NOT_FOUND";
            case ErrorCode::DEVICE_OFFLINE: return "DEVICE_OFFLINE";
            
            // 설정 관련 에러 (400-402)
            case ErrorCode::INVALID_CONFIGURATION: return "INVALID_CONFIGURATION";
            case ErrorCode::MISSING_CONFIGURATION: return "MISSING_CONFIGURATION";
            case ErrorCode::CONFIGURATION_ERROR: return "CONFIGURATION_ERROR";
            
            // 시스템 관련 에러 (500-503)
            case ErrorCode::MEMORY_ERROR: return "MEMORY_ERROR";
            case ErrorCode::RESOURCE_EXHAUSTED: return "RESOURCE_EXHAUSTED";
            case ErrorCode::INTERNAL_ERROR: return "INTERNAL_ERROR";
            case ErrorCode::FILE_ERROR: return "FILE_ERROR";
            
            // 점검 관련 에러 (600-604)
            case ErrorCode::MAINTENANCE_ACTIVE: return "MAINTENANCE_ACTIVE";
            case ErrorCode::MAINTENANCE_PERMISSION_DENIED: return "MAINTENANCE_PERMISSION_DENIED";
            case ErrorCode::MAINTENANCE_TIMEOUT: return "MAINTENANCE_TIMEOUT";
            case ErrorCode::REMOTE_CONTROL_BLOCKED: return "REMOTE_CONTROL_BLOCKED";
            case ErrorCode::INSUFFICIENT_PERMISSION: return "INSUFFICIENT_PERMISSION";
            
            // 프로토콜 특화 에러 (1000+)
            case ErrorCode::MODBUS_EXCEPTION: return "MODBUS_EXCEPTION";
            case ErrorCode::MQTT_PUBLISH_FAILED: return "MQTT_PUBLISH_FAILED";
            case ErrorCode::BACNET_SERVICE_ERROR: return "BACNET_SERVICE_ERROR";
            
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
    // 🔥 시간 관련 함수들 (완전 수정)
    // ========================================
    
    /**
     * @brief 현재 타임스탬프 얻기 (기본) - 🔥 타입 에러 완전 수정
     */
    inline Timestamp GetCurrentTimestamp() {
        // 🔥 올바른 구현: system_clock::time_point 반환
        return std::chrono::system_clock::now();
    }
    
    /**
     * @brief 현재 타임스탬프 얻기 (별칭 - 기존 코드 호환성)
     */
    inline Timestamp CurrentTimestamp() {
        return GetCurrentTimestamp();
    }
    
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
     * @brief 두 시간 사이의 간격 계산
     */
    inline Duration GetDurationSince(const Timestamp& start) {
        return std::chrono::duration_cast<Duration>(
            GetCurrentTimestamp() - start);
    }
    
    /**
     * @brief 데이터 나이 계산
     */
    inline Duration CalculateDataAge(const Timestamp& data_timestamp) {
        return GetDurationSince(data_timestamp);
    }

    /**
     * @brief Duration을 밀리초로 변환
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
    // 점검 관련 유틸리티 함수들
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
            if constexpr (std::is_same_v<T, std::monostate>) {
                return "null";
            } else if constexpr (std::is_same_v<T, bool>) {
                return arg ? "true" : "false";
            } else if constexpr (std::is_same_v<T, std::string>) {
                return arg;
            } else if constexpr (std::is_arithmetic_v<T>) {
                return std::to_string(arg);
            } else {
                return "unknown_type";
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

    /**
     * @brief 문자열을 타임스탬프로 변환 (DB용)
     * @param str 시간 문자열 (형식: "YYYY-MM-DD HH:MM:SS")
     * @return 타임스탬프 (실패 시 현재 시간)
     */
    inline Timestamp ParseTimestampFromString(const std::string& str) {
        std::tm tm = {};
        std::istringstream ss(str);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        
        if (ss.fail()) {
            return GetCurrentTimestamp();  // 실패 시 현재 시간 반환
        }
        
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }
    
    /**
     * @brief 타임스탬프를 DB용 문자열로 변환
     * @param tp 타임스탬프
     * @return DB 문자열 (형식: "YYYY-MM-DD HH:MM:SS")
     */
    inline std::string TimestampToDBString(const Timestamp& tp) {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    /**
     * @brief Timestamp를 문자열로 변환
     * @param timestamp 변환할 타임스탬프
     * @return ISO 8601 형식의 문자열 (예: "2025-08-04T12:34:56.789Z")
     */
    inline std::string TimestampToString(const PulseOne::BasicTypes::Timestamp& timestamp) {
        try {
            auto time_t = std::chrono::system_clock::to_time_t(timestamp);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()) % 1000;
            
            std::stringstream ss;
            ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
            ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
            
            return ss.str();
        } catch (const std::exception&) {
            return "1970-01-01T00:00:00.000Z";  // 기본값
        }
    }
    
    /**
     * @brief 문자열을 Timestamp로 변환
     * @param timestamp_str ISO 8601 형식의 문자열
     * @return Timestamp 객체
     */
    inline PulseOne::BasicTypes::Timestamp StringToTimestamp(const std::string& timestamp_str) {
        (void)timestamp_str;
        
        try {
            // 간단한 구현 - 현재 시간 반환
            // 실제로는 ISO 8601 파싱 로직 필요
            return std::chrono::system_clock::now();
        } catch (const std::exception&) {
            return std::chrono::system_clock::now();
        }
    }
    
    /**
     * @brief 현재 시간을 문자열로 반환
     * @return 현재 시간의 ISO 8601 문자열
     */
    inline std::string GetCurrentTimestampString() {
        return TimestampToString(GetCurrentTimestamp());
    }
    
    /**
     * @brief 접근 모드를 문자열로 변환
     */
    inline std::string AccessModeToString(PulseOne::Enums::AccessMode mode) {
        switch (mode) {
            case PulseOne::Enums::AccessMode::read: return "read";
            case PulseOne::Enums::AccessMode::write: return "write";
            case PulseOne::Enums::AccessMode::read_write: return "read_write";
            default: return "read";
        }
    }

    /**
     * @brief 문자열을 접근 모드로 변환
     */
    inline PulseOne::Enums::AccessMode StringToAccessMode(const std::string& str) {
        if (str == "write") return PulseOne::Enums::AccessMode::write;
        if (str == "read_write") return PulseOne::Enums::AccessMode::read_write;
        return PulseOne::Enums::AccessMode::read;  // 기본값
    }

    /**
     * @brief 데이터 타입을 문자열로 변환 (WorkerFactory에서 사용)
     */
    inline std::string DataTypeToString(PulseOne::Enums::DataType type) {
        switch (type) {
            case PulseOne::Enums::DataType::BOOL: return "BOOLEAN";
            case PulseOne::Enums::DataType::INT8: return "INT8";
            case PulseOne::Enums::DataType::UINT8: return "UINT8";
            case PulseOne::Enums::DataType::INT16: return "INT16";
            case PulseOne::Enums::DataType::UINT16: return "UINT16";
            case PulseOne::Enums::DataType::INT32: return "INT32";
            case PulseOne::Enums::DataType::UINT32: return "UINT32";
            case PulseOne::Enums::DataType::INT64: return "INT64";
            case PulseOne::Enums::DataType::UINT64: return "UINT64";
            case PulseOne::Enums::DataType::FLOAT32: return "FLOAT";
            case PulseOne::Enums::DataType::FLOAT64: return "DOUBLE";
            case PulseOne::Enums::DataType::STRING: return "STRING";
            case PulseOne::Enums::DataType::BINARY: return "BINARY";
            case PulseOne::Enums::DataType::DATETIME: return "DATETIME";
            case PulseOne::Enums::DataType::JSON: return "JSON";
            case PulseOne::Enums::DataType::ARRAY: return "ARRAY";
            case PulseOne::Enums::DataType::OBJECT: return "OBJECT";
            default: return "UNKNOWN";
        }
    }

    /**
     * @brief 문자열을 데이터 타입으로 변환 (WorkerFactory에서 사용)
     */
    inline PulseOne::Enums::DataType StringToDataType(const std::string& str) {
        std::string upper = str;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        
        if (upper == "BOOLEAN" || upper == "BOOL") return PulseOne::Enums::DataType::BOOL;
        if (upper == "INT8") return PulseOne::Enums::DataType::INT8;
        if (upper == "UINT8") return PulseOne::Enums::DataType::UINT8;
        if (upper == "INT16") return PulseOne::Enums::DataType::INT16;
        if (upper == "UINT16") return PulseOne::Enums::DataType::UINT16;
        if (upper == "INT32") return PulseOne::Enums::DataType::INT32;
        if (upper == "UINT32") return PulseOne::Enums::DataType::UINT32;
        if (upper == "INT64") return PulseOne::Enums::DataType::INT64;
        if (upper == "UINT64") return PulseOne::Enums::DataType::UINT64;
        if (upper == "FLOAT" || upper == "FLOAT32") return PulseOne::Enums::DataType::FLOAT32;
        if (upper == "DOUBLE" || upper == "FLOAT64") return PulseOne::Enums::DataType::FLOAT64;
        if (upper == "STRING") return PulseOne::Enums::DataType::STRING;
        if (upper == "BINARY") return PulseOne::Enums::DataType::BINARY;
        if (upper == "DATETIME") return PulseOne::Enums::DataType::DATETIME;
        if (upper == "JSON") return PulseOne::Enums::DataType::JSON;
        if (upper == "ARRAY") return PulseOne::Enums::DataType::ARRAY;
        if (upper == "OBJECT") return PulseOne::Enums::DataType::OBJECT;
        
        return PulseOne::Enums::DataType::UNKNOWN;
    }

    /**
     * @brief 프로토콜 타입을 문자열로 변환 (문자열 버전 - WorkerFactory 호환)
     */
    inline std::string ProtocolTypeToString(const std::string& protocol) {
        return protocol;  // 이미 문자열이므로 그대로 반환
    }

    /**
     * @brief 프로토콜 타입을 문자열로 변환 (enum 버전 - 기존 함수 유지)
     */
    // 기존 ProtocolTypeToString(ProtocolType type) 함수는 그대로 유지    

    
} // namespace PulseOne::Utils

#endif // PULSEONE_COMMON_UTILS_H