#ifndef PULSEONE_UNIFIED_COMMON_TYPES_H
#define PULSEONE_UNIFIED_COMMON_TYPES_H

/**
 * @file UnifiedCommonTypes.h
 * @brief PulseOne 통합 타입 시스템 - 순서 수정
 * @date 2025-07-29
 * 
 * 🔥 중요: include 순서가 중요함!
 * 1. BasicTypes, Enums, Constants 먼저
 * 2. 타입 선언
 * 3. Structs 나중에
 */

// 🔥 1단계: 기본 타입들 먼저 include
#include "Common/BasicTypes.h"
#include "Common/Enums.h"
#include "Common/Constants.h"

namespace PulseOne {
    // 🔥 2단계: 기본 타입들 선언
    using UUID = BasicTypes::UUID;
    using Timestamp = BasicTypes::Timestamp;
    using Duration = BasicTypes::Duration;
    using DataVariant = BasicTypes::DataVariant;
    using EngineerID = BasicTypes::EngineerID;
    
    // Enums에서 가져오기
    using ProtocolType = Enums::ProtocolType;
    using LogLevel = Enums::LogLevel;
    using DriverLogCategory = Enums::DriverLogCategory;
    using DataQuality = Enums::DataQuality;
    using ConnectionStatus = Enums::ConnectionStatus;
    using MaintenanceStatus = Enums::MaintenanceStatus;
    using MaintenanceType = Enums::MaintenanceType;
    using ErrorCode = Enums::ErrorCode;
    
    // Constants에서 가져오기
    using Constants::LOG_MODULE_SYSTEM;
}

// 🔥 3단계: 타입 선언 후에 Structs include
#include "Common/Structs.h"

namespace PulseOne {
    // 🔥 4단계: Structs에서 가져오기 (이제 타입들이 정의된 후)
    using DeviceInfo = Structs::DeviceInfo;
    using DataPoint = Structs::DataPoint;
    using TimestampedValue = Structs::TimestampedValue;
    using DriverLogContext = Structs::DriverLogContext;
    using LogStatistics = Structs::LogStatistics;
    using MaintenanceState = Structs::MaintenanceState;
    using DriverConfig = Structs::DriverConfig;
    using DriverStatistics = Structs::DriverStatistics;
    using ErrorInfo = Structs::ErrorInfo;
    
    // Drivers 네임스페이스 호환성
    namespace Drivers {
        using DeviceInfo = PulseOne::DeviceInfo;
        using DataPoint = PulseOne::DataPoint;
        using TimestampedValue = PulseOne::TimestampedValue;
        using ProtocolType = PulseOne::ProtocolType;
        using DriverLogContext = PulseOne::DriverLogContext;
    }
}

#endif // PULSEONE_UNIFIED_COMMON_TYPES_H