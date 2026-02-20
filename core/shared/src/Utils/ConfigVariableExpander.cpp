/**
 * @file ConfigVariableExpander.cpp
 * @brief ConfigVariableExpander 구현부 - <regex> 헤더를 이 곳으로 고립시킴
 */

#include "Utils/ConfigVariableExpander.h"
#include <regex>

namespace PulseOne {
namespace Utils {

std::string ConfigVariableExpander::expand(const std::string &input,
                                           ValueProvider provider) {
  if (input.empty() || !provider) {
    return input;
  }

  std::string result = input;

  // ${VARIABLE} 패턴 처리 (예: ${DATA_DIR}, ${SECRETS_DIR}/key.key)
  // 중괄호 안의 유효한 문자들: 알파벳, 숫자, 언더바
  std::regex var_pattern(R"(\$\{([A-Z0-9_]+)\})");
  std::smatch match;

  // 무한 루프 방지를 위한 최대 치환 횟수 제한 (중첩 변수 지원)
  int max_iterations = 10;
  bool changed = true;

  while (changed && max_iterations-- > 0) {
    changed = false;
    std::string current_result = result;
    auto search_start = current_result.cbegin();

    while (std::regex_search(search_start, current_result.cend(), match,
                             var_pattern)) {
      std::string var_name = match[1].str();
      std::string replacement = provider(var_name);

      if (!replacement.empty()) {
        size_t pos =
            match.position() + (search_start - current_result.cbegin());
        result.replace(pos, match.length(), replacement);
        changed = true;
        // 치환 후 다음 탐색은 치환된 부분 이후부터 (다만 다시 처음부터 도는 게
        // 중첩에 안전함)
        break;
      }
      search_start = match.suffix().first;
    }
  }

  return result;
}

std::string ConfigVariableExpander::expandPulseOneVariables(
    const std::string &input, ValueProvider provider,
    const std::string &configDir, const std::string &dataDir) {
  return expand(input, [&](const std::string &var_name) -> std::string {
    // 1. 특수 변수 우선 처리
    if (var_name == "CONFIG_DIR")
      return configDir;
    if (var_name == "DATA_DIR")
      return dataDir;
    if (var_name == "SECRETS_DIR")
      return std::string(configDir + "/secrets");

    // 2. 일반 변수 제공자 호출
    if (provider) {
      std::string val = provider(var_name);
      if (!val.empty())
        return val;
    }

    return "";
  });
}

} // namespace Utils
} // namespace PulseOne
