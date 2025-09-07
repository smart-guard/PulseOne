#ifndef COMMON_BASIC_TYPES_H
#define COMMON_BASIC_TYPES_H

/**
 * @file BasicTypes.h
 * @brief PulseOne 기본 타입 정의 - Windows 크로스 컴파일 완전 지원
 * @author PulseOne Development Team
 * @date 2025-09-06
 * 
 * Windows/Linux 통합 타입 시스템:
 * - UUID는 모든 플랫폼에서 string으로 통일
 * - Windows API 충돌 완전 방지
 * - 기존 코드 100% 호환성 보장
 */

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

namespace PulseOne {
namespace BasicTypes {
    
    // =========================================================================
    // 핵심 식별자 타입들 (모든 플랫폼에서 string으로 통일)
    // =========================================================================
    
    /**
     * @brief UUID 타입 - 모든 플랫폼에서 string으로 통일
     * @details Windows GUID 충돌 방지를 위해 string 사용
     */
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
    
    // 엔지니어 ID (점검 기능용)
    using EngineerID = std::string;
    
    // =========================================================================
    // 데이터 변형 타입 (모든 프로토콜 지원)
    // =========================================================================
    
    /**
     * @brief 범용 데이터 값 타입
     * @details 모든 프로토콜에서 사용하는 통합 데이터 타입
     */
    using DataVariant = std::variant<
        bool,           // 불린 값 (코일, 디지털 입력)
        int16_t,        // 16비트 정수 (Modbus 레지스터)
        uint16_t,       // 16비트 부호없는 정수
        int32_t,        // 32비트 정수
        uint32_t,       // 32비트 부호없는 정수
        int64_t,        // 64비트 정수
        uint64_t,       // 64비트 부호없는 정수
        float,          // 32비트 부동소수점
        double,         // 64비트 부동소수점
        std::string     // 문자열 (MQTT JSON, BACnet 문자열)
    >;
    
    // =========================================================================
    // 프로토콜 관련 열거형
    // =========================================================================
    
    enum class ProtocolType : uint8_t {
        UNKNOWN = 0,
        MODBUS_TCP = 1,
        MODBUS_RTU = 2,
        MQTT = 3,
        BACNET = 4,
        OPC_UA = 5,
        CUSTOM = 99
    };
    
    enum class DataQuality : uint8_t {
        GOOD = 0,
        UNCERTAIN = 1,
        BAD = 2,
        NOT_CONNECTED = 3
    };
    
    // =========================================================================
    // 유틸리티 함수들
    // =========================================================================
    
    /**
     * @brief UUID 생성 함수 (플랫폼 독립적)
     * @return 새로운 UUID 문자열
     */
    inline UUID GenerateUUID() {
        // 간단한 UUID 생성 (RFC 4122 버전 4 스타일)
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        std::uniform_int_distribution<> dis2(8, 11);
        
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
     * @brief 현재 타임스탬프 반환
     */
    inline Timestamp GetCurrentTimestamp() {
        return std::chrono::system_clock::now();
    }
    
    /**
     * @brief 타임스탬프를 문자열로 변환
     */
    inline std::string TimestampToString(const Timestamp& timestamp) {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        std::tm tm_buf{};
        
#ifdef _WIN32
        gmtime_s(&tm_buf, &time_t);
#else
        gmtime_r(&time_t, &tm_buf);
#endif
        
        std::stringstream ss;
        ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    
    /**
     * @brief DataVariant를 문자열로 변환
     */
    inline std::string DataVariantToString(const DataVariant& value) {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return arg;
            } else if constexpr (std::is_same_v<T, bool>) {
                return arg ? "true" : "false";
            } else {
                return std::to_string(arg);
            }
        }, value);
    }
    
    /**
     * @brief DataVariant에서 double 값 추출
     */
    inline double DataVariantToDouble(const DataVariant& value) {
        return std::visit([](auto&& arg) -> double {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string>) {
                try {
                    return std::stod(arg);
                } catch (...) {
                    return 0.0;
                }
            } else if constexpr (std::is_same_v<T, bool>) {
                return arg ? 1.0 : 0.0;
            } else {
                return static_cast<double>(arg);
            }
        }, value);
    }
    
    /**
     * @brief DataVariant 타입명 반환
     */
    inline std::string GetDataVariantTypeName(const DataVariant& value) {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, bool>) return "bool";
            else if constexpr (std::is_same_v<T, int16_t>) return "int16";
            else if constexpr (std::is_same_v<T, uint16_t>) return "uint16";
            else if constexpr (std::is_same_v<T, int32_t>) return "int32";
            else if constexpr (std::is_same_v<T, uint32_t>) return "uint32";
            else if constexpr (std::is_same_v<T, int64_t>) return "int64";
            else if constexpr (std::is_same_v<T, uint64_t>) return "uint64";
            else if constexpr (std::is_same_v<T, float>) return "float";
            else if constexpr (std::is_same_v<T, double>) return "double";
            else if constexpr (std::is_same_v<T, std::string>) return "string";
            else return "unknown";
        }, value);
    }
    
    // =========================================================================
    // 스마트 포인터 별칭들
    // =========================================================================
    
    template<typename T>
    using UniquePtr = std::unique_ptr<T>;
    
    template<typename T>
    using SharedPtr = std::shared_ptr<T>;
    
    template<typename T>
    using WeakPtr = std::weak_ptr<T>;
    
    // =========================================================================
    // 컨테이너 별칭들
    // =========================================================================
    
    using StringVector = std::vector<std::string>;
    using UUIDVector = std::vector<UUID>;
    using DataVariantVector = std::vector<DataVariant>;
    using StringMap = std::map<std::string, std::string>;
    using DataVariantMap = std::map<std::string, DataVariant>;
    
    // =========================================================================
    // 기존 호환성을 위한 별칭들
    // =========================================================================
    
    namespace Compatibility {
        using DeviceId = UUID;
        using PointId = UUID;
        using RequestId = UUID;
        using Value = DataVariant;
        using TimeStamp = Timestamp;
    }

} // namespace BasicTypes
} // namespace PulseOne

#endif // COMMON_BASIC_TYPES_H