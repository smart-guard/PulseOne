/**
 * @file ConfigPathResolver.h
 * @brief 설정 및 데이터 디렉토리 경로 해결 기능을 제공하는 전용 모듈
 * @author PulseOne Development Team
 * @date 2026-02-18
 */

#pragma once

#include <functional>
#include <string>
#include <vector>

namespace PulseOne {
namespace Utils {

/**
 * @class ConfigPathResolver
 * @brief 실행 경로, 설정 디렉토리, 데이터 디렉토리 등을 찾고 관리하는 유틸리티
 */
class ConfigPathResolver {
public:
  /// 경로 탐색 결과 구조체
  struct SearchResult {
    std::string foundPath;
    std::vector<std::string> searchLog;
  };

  /**
   * @brief 현재 프로세스의 실행 위치(디렉토리)를 반환합니다.
   */
  static std::string getExecutableDirectory();

  /**
   * @brief 설정 디렉토리(.env 파일들이 있는 곳)를 탐색합니다.
   * @return 탐색 결과 (경로 및 로그)
   */
  static SearchResult findConfigDirectory();

  /**
   * @brief 데이터 디렉토리(로그, DB 등 저장소)를 결정합니다.
   * @param dataDirFromConfig 설정 파일에 정의된 경로 (없으면 기본값 사용)
   * @return 결정된 절대 경로
   */
  static std::string findDataDirectory(const std::string &dataDirFromConfig);

  /**
   * @brief SQLite 데이터베이스 파일 경로를 결정합니다.
   * @param configGetter 설정값을 가져오는 콜백 (key -> value)
   * @param dataDir 데이터 디렉토리
   * @return 결정된 절대 경로
   */
  static std::string resolveSQLitePath(
      std::function<std::string(const std::string &)> configGetter,
      const std::string &dataDir);

  /**
   * @brief 백업 디렉토리를 결정합니다.
   * @param configGetter 설정값을 가져오는 콜백 (key -> value)
   * @param dataDir 데이터 디렉토리
   * @return 결정된 절대 경로
   */
  static std::string resolveBackupDirectory(
      std::function<std::string(const std::string &)> configGetter,
      const std::string &dataDir);

  /**
   * @brief 시크릿 보관 디렉토리를 반환합니다.
   */
  static std::string getSecretsDirectory(const std::string &configDir);

  /**
   * @brief 디렉토리가 존재하는지 확인하고, 없으면 생성합니다 (Recursive).
   */
  static bool ensureDirectory(const std::string &path);

  /**
   * @brief 상대 경로를 절대 경로로 변환합니다.
   */
  static std::string getAbsolutePath(const std::string &path);
};

} // namespace Utils
} // namespace PulseOne
