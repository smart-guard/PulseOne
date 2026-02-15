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
 *
 * 🔥 의존성 해결:
 * - ProtocolType 타입 별칭 명시적 선언
 * - 순환 참조 방지
 */

#include "BasicTypes.h"
#include <memory>

// 🔥 순환 참조 방지: 전방 선언 후 필요한 타입만 별칭
namespace PulseOne {
namespace Structs {

// Protocols are now identified by string. Using the central alias.
using ProtocolType = PulseOne::BasicTypes::ProtocolType;

/**
 * @brief 프로토콜 설정 인터페이스 (추상 클래스)
 */
class IProtocolConfig {
public:
  virtual ~IProtocolConfig() = default;

  /**
   * @brief 프로토콜 이름 반환
   */
  virtual std::string GetProtocol() const = 0;
  virtual ProtocolType GetProtocolType() const { return GetProtocol(); }

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
  virtual bool FromJson(const std::string &json) = 0;
};
} // namespace Structs
} // namespace PulseOne

#endif // PULSEONE_IPROTOCOL_CONFIG_H