#ifndef PULSEONE_DRIVERS_PLUGIN_LOADER_H
#define PULSEONE_DRIVERS_PLUGIN_LOADER_H

#include "Drivers/Common/DriverFactory.h"
#include "Logging/LogManager.h"
#include "Platform/PlatformCompat.h"
#include <memory>
#include <string>
#include <vector>

namespace PulseOne {
namespace Drivers {

/**
 * @brief 동적 플러그인 로더 클래스
 *
 * 특정 디렉토리에서 공유 라이브러리(.dll, .so)를 검색하여
 * 드라이버 플러그인을 동적으로 로드합니다.
 */
class PluginLoader {
public:
  static PluginLoader &GetInstance() {
    static PluginLoader instance;
    return instance;
  }

  /**
   * @brief 지정된 디렉토리에서 플러그인을 로드합니다.
   * @param directory_path 플러그인 디렉토리 경로
   * @return 로드된 플러그인 수
   */
  size_t LoadPlugins(const std::string &directory_path) {
    if (!Platform::FileSystem::DirectoryExists(directory_path)) {
      LogManager::getInstance().Warn("Plugin directory does not exist: " +
                                     directory_path);
      return 0;
    }

    LogManager::getInstance().Info("Scanning for plugins in: " +
                                   directory_path);
    size_t loaded_count = 0;

    // 플랫폼별 확장자 설정
#if PULSEONE_WINDOWS
    std::string extension = ".dll";
#else
    std::string extension = ".so";
#endif

    std::vector<std::string> files =
        Platform::FileSystem::ListFiles(directory_path);
    for (const auto &file : files) {
      if (file.size() >= extension.size() &&
          file.compare(file.size() - extension.size(), extension.size(),
                       extension) == 0) {
        if (LoadPlugin(file)) {
          loaded_count++;
        }
      }
    }

    return loaded_count;
  }

  /**
   * @brief 단일 플러그인 파일을 로드합니다.
   * @param plugin_path 플러그인 파일 경로
   * @return 로드 성공 여부
   */
  bool LoadPlugin(const std::string &plugin_path) {
    LogManager::getInstance().Info("Loading plugin: " + plugin_path);

    void *handle = Platform::DynamicLibrary::LoadLibrary(plugin_path);
    if (!handle) {
      std::string error_msg = "Unknown error";
#ifdef _WIN32
      // Windows specific error handling
      DWORD error_code = GetLastError();
      error_msg = "WinError: " + std::to_string(error_code);
#else
      const char *dl_err = dlerror();
      if (dl_err)
        error_msg = dl_err;
#endif
      LogManager::getInstance().Error("Failed to load library: " + plugin_path +
                                      " Reason: " + error_msg);
      return false;
    }

    // 플러그인 등록 함수 찾기 (C linkage)
    using RegisterFunc = void (*)();
    RegisterFunc reg_func = reinterpret_cast<RegisterFunc>(
        Platform::DynamicLibrary::GetSymbol(handle, "RegisterPlugin"));

    if (!reg_func) {
      LogManager::getInstance().Error("RegisterPlugin symbol not found in: " +
                                      plugin_path);
      Platform::DynamicLibrary::FreeLibrary(handle);
      return false;
    }

    try {
      // 플러그인 스스로 DriverFactory에 등록하도록 유도
      reg_func();
      handles_.push_back(handle);
      LogManager::getInstance().Info(
          "Plugin loaded and registered successfully: " + plugin_path);
      return true;
    } catch (const std::exception &e) {
      LogManager::getInstance().Error("Error during plugin registration: " +
                                      std::string(e.what()));
      Platform::DynamicLibrary::FreeLibrary(handle);
      return false;
    }
  }

  /**
   * @brief 모든 플러그인을 언로드합니다.
   */
  void UnloadAll() {
    for (void *handle : handles_) {
      Platform::DynamicLibrary::FreeLibrary(handle);
    }
    handles_.clear();
  }

private:
  PluginLoader() = default;
  ~PluginLoader() { UnloadAll(); }

  std::vector<void *> handles_;
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_PLUGIN_LOADER_H
