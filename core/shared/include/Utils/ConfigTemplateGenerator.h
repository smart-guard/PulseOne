/**
 * @file ConfigTemplateGenerator.h
 * @brief 기본 설정 파일 템플릿 생성 기능을 제공하는 모듈
 * @author PulseOne Development Team
 * @date 2026-02-18
 */

#pragma once

#include <string>

namespace PulseOne {
namespace Utils {

/**
 * @class ConfigTemplateGenerator
 * @brief 설정 파일이 없을 경우 기본 파일들을 생성하는 유틸리티
 */
class ConfigTemplateGenerator {
public:
  /**
   * @brief 모든 기본 설정 파일(.env, database.env 등)을 생성합니다.
   * @param configDir 설정 파일이 생성될 디렉토리
   */
  static void generateDefaultConfigs(const std::string &configDir);

  /**
   * @brief 시크릿 보관 디렉토리 및 .gitignore를 생성합니다.
   * @param configDir 설정 파일 디렉토리 (하위에 secrets가 생성됨)
   */
  static void createSecretsDirectory(const std::string &configDir);

private:
  static void createMainEnvFile(const std::string &configDir);
  static void createDatabaseEnvFile(const std::string &configDir);
  static void createRedisEnvFile(const std::string &configDir);
  static void createTimeseriesEnvFile(const std::string &configDir);
  static void createMessagingEnvFile(const std::string &configDir);
  static void createSecurityEnvFile(const std::string &configDir);

  static bool createFile(const std::string &filepath,
                         const std::string &content);
};

} // namespace Utils
} // namespace PulseOne
