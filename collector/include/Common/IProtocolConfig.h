// collector/include/Common/IProtocolConfig.h
#ifndef PULSEONE_IPROTOCOL_CONFIG_H
#define PULSEONE_IPROTOCOL_CONFIG_H

/**
 * @file IProtocolConfig.h
 * @brief 프로토콜 설정 인터페이스 클래스
 * @author PulseOne Development Team
 * @date 2025-08-05
 * 
 * 🎯 목적: 스마트 포인터 기반 확장성 제공
 * - Union 방식의 한계 극복
 * - 무제한 프로토콜 추가 지원
 * - 타입 안전성 확보
 */

#include "BasicTypes.h"
#include "Enums.h"
#include <memory>
#include <string>

namespace PulseOne::Structs {
    
    /**
     * @brief 프로토콜별 설정 기본 인터페이스
     * @details 모든 프로토콜 설정 클래스의 기본 클래스
     */
    class IProtocolConfig {
    public:
        virtual ~IProtocolConfig() = default;
        
        /**
         * @brief 프로토콜 타입 반환
         */
        virtual ProtocolType GetProtocol() const = 0;
        
        /**
         * @brief 설정 복제 (깊은 복사)
         */
        virtual std::unique_ptr<IProtocolConfig> Clone() const = 0;
        
        /**
         * @brief 설정 검증
         */
        virtual bool IsValid() const = 0;
        
        /**
         * @brief JSON 직렬화
         */
        virtual std::string ToJson() const = 0;
        
        /**
         * @brief JSON 역직렬화
         */
        virtual bool FromJson(const std::string& json) = 0;
    };
}

#endif // PULSEONE_IPROTOCOL_CONFIG_H