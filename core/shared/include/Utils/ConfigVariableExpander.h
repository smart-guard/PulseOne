/**
 * @file ConfigVariableExpander.h
 * @brief 설정값 내의 변수(${VAR})를 확장하는 기능을 제공하는 모듈
 * @author PulseOne Development Team
 * @date 2026-02-18
 */

#pragma once

#include <functional>
#include <string>

namespace PulseOne {
namespace Utils {

/**
 * @class ConfigVariableExpander
 * @brief 문자열 내의 ${VARIABLE} 패턴을 찾아 실제 값으로 치환하는 유틸리티
 */
class ConfigVariableExpander {
public:
  /// 변수 이름을 입력받아 치환될 값을 반환하는 콜백 타입
  using ValueProvider = std::function<std::string(const std::string &)>;

  /**
   * @brief 입력 문자열에서 ${VAR} 형태의 패턴을 찾아 치환합니다.
   * @param input 원본 문자열
   * @param provider 변수 값을 제공하는 콜백 함수
   * @return 치환된 문자열
   */
  static std::string expand(const std::string &input, ValueProvider provider);

  /**
   * @brief PulseOne 특화 변수($DATA_DIR 등)를 포함하여 확장합니다.
   * @param input 원본 문자열
   * @param provider 일반 변수 제공 콜백
   * @param configDir 설정 디렉토리
   * @param dataDir 데이터 디렉토리
   * @return 치환된 문자열
   */
  static std::string expandPulseOneVariables(const std::string &input,
                                             ValueProvider provider,
                                             const std::string &configDir,
                                             const std::string &dataDir);
};

} // namespace Utils
} // namespace PulseOne
