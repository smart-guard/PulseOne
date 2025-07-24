#ifndef PULSEONE_COMMON_BASIC_TYPES_H
#define PULSEONE_COMMON_BASIC_TYPES_H

/**
 * @file BasicTypes.h
 * @brief PulseOne 기본 타입 정의
 * @author PulseOne Development Team
 * @date 2025-07-24
 */

#include <string>
#include <chrono>
#include <variant>
#include <vector>
#include <cstdint>

namespace PulseOne::BasicTypes {
    
    /**
     * @brief 범용 고유 식별자 (UUID/GUID)
     */
    using UUID = std::string;
    
    /**
     * @brief 타임스탬프 타입 (UTC 기준)
     */
    using Timestamp = std::chrono::system_clock::time_point;
    
    /**
     * @brief 시간 간격 타입
     */
    using Duration = std::chrono::milliseconds;
    
    /**
     * @brief 데이터 값을 담는 범용 variant 타입
     * 산업용 데이터 타입들을 모두 포함
     */
    using DataVariant = std::variant<
        bool,           // Digital/Boolean 값
        int16_t,        // 16비트 정수 (Modbus 표준)
        int32_t,        // 32비트 정수
        int64_t,        // 64비트 정수
        float,          // 32비트 실수
        double,         // 64비트 실수
        std::string     // 문자열 값
    >;
    
    /**
     * @brief 바이트 배열 타입 (Raw 데이터용)
     */
    using ByteArray = std::vector<uint8_t>;
    
    /**
     * @brief 엔지니어 식별 정보
     */
    using EngineerID = std::string;
    
    /**
     * @brief 디바이스 엔드포인트 (IP:PORT, Serial 등)
     */
    using Endpoint = std::string;
    
} // namespace PulseOne::BasicTypes

#endif // PULSEONE_COMMON_BASIC_TYPES_H