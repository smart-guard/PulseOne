/**
 * @file ConfigPathResolver.cpp
 * @brief ConfigPathResolver 구현부
 */

#include "Utils/ConfigPathResolver.h"
#include "Platform/PlatformCompat.h"
#include <iostream>
#include <vector>

namespace PulseOne {
namespace Utils {

using namespace PulseOne::Platform;

std::string ConfigPathResolver::getExecutableDirectory() {
  return Path::GetExecutableDirectory();
}

ConfigPathResolver::SearchResult ConfigPathResolver::findConfigDirectory() {
  SearchResult result;

  const char *env_config = std::getenv("PULSEONE_CONFIG_DIR");
  if (env_config && FileSystem::DirectoryExists(env_config)) {
    result.searchLog.push_back("환경변수(PULSEONE_CONFIG_DIR): " +
                               std::string(env_config));
    result.foundPath = std::string(env_config);
    return result;
  }

  std::string exe_dir = getExecutableDirectory();
  std::vector<std::string> search_paths = {Path::Join(exe_dir, "config"),
                                           Path::Join(exe_dir, "../config"),
                                           Path::Join(exe_dir, "../../config"),
                                           "./config",
                                           "../config",
                                           "../../config"};

  for (const auto &path : search_paths) {
    std::string normalized_path = Path::Normalize(path);

    if (FileSystem::DirectoryExists(normalized_path)) {
      try {
        std::string absolute_path = Path::GetAbsolute(normalized_path);
        result.searchLog.push_back("발견: " + path + " -> " + absolute_path);
        result.foundPath = absolute_path;
        return result;
      } catch (const std::exception &) {
        result.searchLog.push_back("발견: " + normalized_path +
                                   " (절대경로 변환 실패)");
        result.foundPath = normalized_path;
        return result;
      }
    } else {
      result.searchLog.push_back("없음: " + normalized_path);
    }
  }

  result.searchLog.push_back("설정 디렉토리를 찾을 수 없음");
  result.foundPath = "";
  return result;
}

std::string
ConfigPathResolver::findDataDirectory(const std::string &dataDirFromConfig) {
  const char *pulseone_data_dir = std::getenv("PULSEONE_DATA_DIR");
  const char *data_dir_env = std::getenv("DATA_DIR");

  if (pulseone_data_dir) {
    return Path::Normalize(std::string(pulseone_data_dir));
  }
  if (data_dir_env) {
    return Path::Normalize(std::string(data_dir_env));
  }

  std::string actual_path =
      dataDirFromConfig.empty() ? "./data" : dataDirFromConfig;

  if (actual_path[0] != '/' && actual_path.find(":\\") == std::string::npos) {
    std::string exe_dir = getExecutableDirectory();
    actual_path = Path::Join(exe_dir, actual_path);
  }

  return Path::Normalize(actual_path);
}

std::string ConfigPathResolver::resolveSQLitePath(
    std::function<std::string(const std::string &)> configGetter,
    const std::string &dataDir) {
  std::string db_path = configGetter("SQLITE_PATH");

  if (db_path.empty()) {
    db_path = "data/db/pulseone.db";
  }

  bool is_absolute =
      (db_path[0] == '/' || db_path.find(":\\") != std::string::npos);
  if (!is_absolute) {
    std::string exe_dir = getExecutableDirectory();
    // prefix 제거
    if (db_path.size() >= 2 &&
        (db_path.substr(0, 2) == "./" || db_path.substr(0, 2) == ".\\")) {
      db_path = db_path.substr(2);
    }
    db_path = Path::Join(exe_dir, db_path);
  }

  db_path = Path::Normalize(db_path);

  try {
    std::string dir = Path::GetDirectory(db_path);
    ensureDirectory(dir);
  } catch (...) {
  }

  return db_path;
}

std::string ConfigPathResolver::resolveBackupDirectory(
    std::function<std::string(const std::string &)> configGetter,
    const std::string &dataDir) {
  std::string backup_dir = configGetter("BACKUP_DIR");

  if (backup_dir.empty()) {
    backup_dir = Path::Join(dataDir, "backups");
  } else if (backup_dir[0] != '/' &&
             backup_dir.find(":\\") == std::string::npos) {
    backup_dir = Path::Join(dataDir, backup_dir);
  }

  backup_dir = Path::Normalize(backup_dir);
  ensureDirectory(backup_dir);
  return backup_dir;
}

std::string
ConfigPathResolver::getSecretsDirectory(const std::string &configDir) {
  return Path::Normalize(Path::Join(configDir, "secrets"));
}

bool ConfigPathResolver::ensureDirectory(const std::string &path) {
  if (path.empty())
    return false;
  if (FileSystem::DirectoryExists(path))
    return true;

  try {
    FileSystem::CreateDirectoryRecursive(path);
    return true;
  } catch (...) {
    return false;
  }
}

std::string ConfigPathResolver::getAbsolutePath(const std::string &path) {
  try {
    return Path::GetAbsolute(path);
  } catch (...) {
    return path;
  }
}

} // namespace Utils
} // namespace PulseOne
