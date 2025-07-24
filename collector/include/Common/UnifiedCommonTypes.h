#ifndef PULSEONE_UNIFIED_COMMON_TYPES_H
#define PULSEONE_UNIFIED_COMMON_TYPES_H

/**
 * @file UnifiedCommonTypes.h
 * @brief PulseOne 통합 타입 시스템 - 단일 진입점
 * @author PulseOne Development Team
 * @date 2025-07-24
 * 
 * 모든 PulseOne 타입들을 한 곳에서 include할 수 있는 단일 진입점
 * 기존 중복 구조체들을 통합하고 현장 점검 기능을 추가
 */

// 🔥 DEBUG 매크로 충돌 방지 - 가장 먼저 처리
#ifdef DEBUG
#pragma push_macro("DEBUG")
#undef DEBUG
#define PULSEONE_DEBUG_MACRO_WAS_DEFINED
#endif

#include "Common/BasicTypes.h"    // 기본 타입 정의
#include "Common/Constants.h"     // 전역 상수들
#include "Common/Enums.h"         // 열거형들
#include "Common/Structs.h"       // 구조체들 (점검 기능 포함)
#include "Common/Utils.h"         // 유틸리티 함수들

namespace PulseOne {
    // 모든 하위 네임스페이스를 루트로 끌어올림
    using namespace BasicTypes;
    using namespace Constants;
    using namespace Enums;
    using namespace Structs;
    using namespace Utils;
    
    // 기존 코드 호환성을 위한 별칭들
    namespace Compatibility {
        // Database::DeviceInfo -> DeviceInfo
        using DatabaseDeviceInfo = DeviceInfo;
        // Drivers::DataPoint -> DataPoint  
        using DriversDataPoint = DataPoint;
        // 기존 LogLevels.h 호환
        using LegacyLogLevel = LogLevel;
        // DriverLogger.h 타입들 호환
        using LegacyDriverLogContext = DriverLogContext;
        using LegacyDriverLogCategory = DriverLogCategory;
        
        // 🔥 DEBUG 매크로 충돌 완전 회피 - 매크로 해제 후 재정의
        #ifdef DEBUG
        #undef DEBUG
        #endif
        constexpr LogLevel DEBUG_LOG_LEVEL = LogLevel::DEBUG_LEVEL;
        // DEBUG 상수는 별도 네임스페이스로 격리
    }
    
    // 🔥 기존 Drivers 네임스페이스 호환성을 위한 별칭
    namespace Drivers {
        using DeviceInfo = PulseOne::DeviceInfo;
        using DataPoint = PulseOne::DataPoint;
        using TimestampedValue = PulseOne::TimestampedValue;
        using ProtocolType = PulseOne::ProtocolType;
        using DriverLogContext = PulseOne::DriverLogContext;
        
        // 유틸리티 함수들은 Utils 네임스페이스 것을 사용
        inline std::string ProtocolTypeToString(ProtocolType type) {
            return PulseOne::Utils::ProtocolTypeToString(type);
        }
    }
}

// 🔥 DEBUG 매크로 복원 (파일 끝에서)
#ifdef PULSEONE_DEBUG_MACRO_WAS_DEFINED
#pragma pop_macro("DEBUG")
#undef PULSEONE_DEBUG_MACRO_WAS_DEFINED
#endif

#endif // PULSEONE_UNIFIED_COMMON_TYPES_H