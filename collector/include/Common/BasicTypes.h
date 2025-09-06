#ifndef COMMON_BASIC_TYPES_H
#define COMMON_BASIC_TYPES_H

#include <string>
#include <chrono>
#include <variant>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

// Windows에서 UUID 충돌 방지
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <rpc.h>
    #pragma comment(lib, "rpcrt4.lib")
    
    // Windows의 UUID를 저장하고 재정의
    namespace WinAPI {
        using WinUUID = ::UUID;
    }
    
    // UUID 매크로 제거
    #ifdef UUID
        #undef UUID
    #endif
#else
    // Linux/Unix 시스템
    #ifdef __has_include
        #if __has_include(<uuid/uuid.h>)
            #include <uuid/uuid.h>
            #define HAS_LIBUUID 1
        #endif
    #endif
#endif

namespace PulseOne {
namespace BasicTypes {
    
    // ===================================================================
    // 기본 타입 정의
    // ===================================================================
    
    // PulseOne의 UUID는 항상 string
    using UUID = std::string;
    using DeviceID = std::string;
    using DataPointID = uint32_t;
    using AlarmID = uint32_t;
    using TenantID = uint32_t;
    using SiteID = uint32_t;
    using UserID = uint32_t;
    using Value = double;
    
    // 시간 관련 타입
    using Timestamp = std::chrono::system_clock::time_point;
    using Duration = std::chrono::milliseconds;
    using Seconds = std::chrono::seconds;
    using Minutes = std::chrono::minutes;
    
    // 엔지니어 ID (문자열)
    using EngineerID = std::string;
    
    // 데이터 변형 타입
    using DataVariant = std::variant<
        bool,
        int16_t,
        uint16_t,
        int32_t,
        uint32_t,
        int64_t,
        uint64_t,
        float,
        double,
        std::string
    >;
    
    // 프로토콜 타입
    enum class ProtocolType : uint8_t {
        UNKNOWN = 0,
        MODBUS_TCP = 1,
        MODBUS_RTU = 2,
        MQTT = 3,
        BACNET = 4,
        OPC_UA = 5,
        CUSTOM = 99
    };
    
    // 데이터 품질
    enum class DataQuality : uint8_t {
        GOOD = 0,
        UNCERTAIN = 1,
        BAD = 2,
        NOT_CONNECTED = 3
    };
    
    // ===================================================================
    // UUID 생성 함수
    // ===================================================================
    
    inline UUID GenerateUUID() {
#ifdef _WIN32
        // Windows: RPC 라이브러리 사용
        ::UUID uuid;
        UuidCreate(&uuid);
        
        unsigned char* str;
        UuidToStringA(&uuid, &str);
        
        std::string result(reinterpret_cast<char*>(str));
        RpcStringFreeA(&str);
        
        // 소문자로 변환
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
        
#elif defined(HAS_LIBUUID)
        // Linux: libuuid 사용
        uuid_t uuid;
        uuid_generate(uuid);
        
        char uuid_str[37];
        uuid_unparse_lower(uuid, uuid_str);
        return std::string(uuid_str);
        
#else
        // Fallback: 간단한 랜덤 UUID 생성 (UUID v4)
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        static std::uniform_int_distribution<> dis2(8, 11);
        
        std::stringstream ss;
        ss << std::hex;
        
        // 8-4-4-4-12 형식
        for (int i = 0; i < 8; i++) ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 4; i++) ss << dis(gen);
        ss << "-4"; // Version 4 UUID
        for (int i = 0; i < 3; i++) ss << dis(gen);
        ss << "-";
        ss << dis2(gen); // Variant bits
        for (int i = 0; i < 3; i++) ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 12; i++) ss << dis(gen);
        
        return ss.str();
#endif
    }
    
    // ===================================================================
    // 유틸리티 함수
    // ===================================================================
    
    // UUID 유효성 검사
    inline bool IsValidUUID(const UUID& uuid) {
        if (uuid.length() != 36) return false;
        
        // 8-4-4-4-12 형식 확인
        if (uuid[8] != '-' || uuid[13] != '-' || 
            uuid[18] != '-' || uuid[23] != '-') {
            return false;
        }
        
        // 16진수 문자 확인
        for (size_t i = 0; i < uuid.length(); i++) {
            if (i == 8 || i == 13 || i == 18 || i == 23) continue;
            char c = uuid[i];
            if (!((c >= '0' && c <= '9') || 
                  (c >= 'a' && c <= 'f') || 
                  (c >= 'A' && c <= 'F'))) {
                return false;
            }
        }
        
        return true;
    }
    
    // 현재 시간 가져오기
    inline Timestamp GetCurrentTime() {
        return std::chrono::system_clock::now();
    }
    
    // 타임스탬프를 문자열로 변환
    inline std::string TimestampToString(const Timestamp& ts) {
        auto time_t = std::chrono::system_clock::to_time_t(ts);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    
    // 문자열을 타임스탬프로 변환
    inline Timestamp StringToTimestamp(const std::string& str) {
        std::tm tm = {};
        std::istringstream ss(str);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }
    
    // Duration을 밀리초로 변환
    inline uint64_t DurationToMs(const Duration& duration) {
        return duration.count();
    }
    
    // 밀리초를 Duration으로 변환
    inline Duration MsToDuration(uint64_t ms) {
        return Duration(ms);
    }
    
    // 프로토콜 타입을 문자열로 변환
    inline std::string ProtocolTypeToString(ProtocolType type) {
        switch (type) {
            case ProtocolType::MODBUS_TCP: return "MODBUS_TCP";
            case ProtocolType::MODBUS_RTU: return "MODBUS_RTU";
            case ProtocolType::MQTT: return "MQTT";
            case ProtocolType::BACNET: return "BACNET";
            case ProtocolType::OPC_UA: return "OPC_UA";
            case ProtocolType::CUSTOM: return "CUSTOM";
            default: return "UNKNOWN";
        }
    }
    
    // 문자열을 프로토콜 타입으로 변환
    inline ProtocolType StringToProtocolType(const std::string& str) {
        if (str == "MODBUS_TCP") return ProtocolType::MODBUS_TCP;
        if (str == "MODBUS_RTU") return ProtocolType::MODBUS_RTU;
        if (str == "MQTT") return ProtocolType::MQTT;
        if (str == "BACNET") return ProtocolType::BACNET;
        if (str == "OPC_UA") return ProtocolType::OPC_UA;
        if (str == "CUSTOM") return ProtocolType::CUSTOM;
        return ProtocolType::UNKNOWN;
    }
    
    // 데이터 품질을 문자열로 변환
    inline std::string DataQualityToString(DataQuality quality) {
        switch (quality) {
            case DataQuality::GOOD: return "GOOD";
            case DataQuality::UNCERTAIN: return "UNCERTAIN";
            case DataQuality::BAD: return "BAD";
            case DataQuality::NOT_CONNECTED: return "NOT_CONNECTED";
            default: return "UNKNOWN";
        }
    }
    
} // namespace BasicTypes
} // namespace PulseOne

// ===================================================================
// 전역 별칭 (편의를 위해)
// ===================================================================
using PulseOneUUID = PulseOne::BasicTypes::UUID;
using PulseOneTimestamp = PulseOne::BasicTypes::Timestamp;
using PulseOneDuration = PulseOne::BasicTypes::Duration;

#endif // COMMON_BASIC_TYPES_H