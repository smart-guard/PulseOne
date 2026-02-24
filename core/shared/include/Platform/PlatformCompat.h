#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

/**
 * @file PlatformCompat.h
 * @brief 크로스 플랫폼 호환성 레이어 - Windows/Linux 통합 + 경로 처리 강화
 * @author PulseOne Development Team
 * @date 2025-09-09
 */

// 표준 헤더들 먼저 (순서 중요!)
#include <cerrno>
#include <cstdint>
#include <ctime>
#include <errno.h>
#include <string>
#include <vector>

// C++17 filesystem 지원 확인
// (단, Windows/MinGW 환경에서는 GetModuleFileNameA가 반환하는 CP949(ANSI)
// 경로와 std::filesystem의 UTF-8 인코딩 가정이 충돌하므로 사용하지 않고 Windows
// API 폴백 사용)
#if __cplusplus >= 201703L && __has_include(<filesystem>) && !defined(_WIN32)
#include <algorithm>
#include <filesystem>
#define HAS_FILESYSTEM 1
#else
#define HAS_FILESYSTEM 0
#endif

// =============================================================================
// 플랫폼 감지
// =============================================================================
#if defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) ||              \
    defined(__MINGW64__)
#define PULSEONE_WINDOWS 1
#define PULSEONE_LINUX 0
#else
#define PULSEONE_WINDOWS 0
#define PULSEONE_LINUX 1
#endif

// =============================================================================
// Windows 전용 설정 (MinGW 크로스 컴파일 포함)
// =============================================================================
#if PULSEONE_WINDOWS
// 반드시 winsock2.h를 windows.h보다 먼저!
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

// UniqueId 타입 충돌 방지 (Windows API vs 사용자 정의)
#ifdef UniqueId
#undef UniqueId
#endif

// 순서 중요: winsock2.h -> ws2tcpip.h -> windows.h
#include <algorithm>
#include <shlwapi.h> // 경로 처리용
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

// Windows API 매크로 제거 (PULSEONE Enum과 충돌방지)
#ifdef ERROR
#undef ERROR
#endif
// #pragma comment(lib, "ws2_32.lib")
// #pragma comment(lib, "iphlpapi.lib")
// #pragma comment(lib, "shlwapi.lib")

// Windows 전용 매크로들
#define SAFE_LOCALTIME(t, tm) localtime_s(tm, t)
#define SAFE_GMTIME(t, tm) gmtime_s(tm, t)

// Windows SQL 지원 비활성화 (충돌 방지)
#ifndef PULSEONE_DISABLE_MSSQL
#define PULSEONE_DISABLE_MSSQL 1
#endif

// Windows용 poll 상수 정의 (중복 정의 방지)
#ifndef POLLIN
#define POLLIN 0x0001
#endif
#ifndef POLLOUT
#define POLLOUT 0x0004
#endif

#else
// Linux/Unix 설정
#include <arpa/inet.h>
#include <dlfcn.h>
#include <libgen.h> // dirname, basename용
#include <limits.h> // PATH_MAX용
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define SAFE_LOCALTIME(t, tm) localtime_r(t, tm)
#define SAFE_GMTIME(t, tm) gmtime_r(t, tm)
#endif

namespace PulseOne {
// 모든 플랫폼에서 UniqueId는 string으로 통일
using UniqueId = std::string;
} // namespace PulseOne

namespace PulseOne {
namespace Platform {

// =============================================================================
// pollfd 구조체 정의 (Windows용)
// =============================================================================
#if PULSEONE_WINDOWS
struct pollfd {
  int fd;        // 파일 디스크립터
  short events;  // 관심 있는 이벤트
  short revents; // 발생한 이벤트
};
#endif

// =============================================================================
// 크로스 플랫폼 경로 처리 클래스 (새로 추가)
// =============================================================================
class Path {
public:
  /**
   * @brief 경로 구분자 반환 (Windows: \, Linux: /)
   */
  static char GetSeparator() {
#if PULSEONE_WINDOWS
    return '\\';
#else
    return '/';
#endif
  }

  /**
   * @brief 경로 구분자 문자열 반환
   */
  static std::string GetSeparatorString() {
#if PULSEONE_WINDOWS
    return "\\";
#else
    return "/";
#endif
  }

  /**
   * @brief 두 경로를 안전하게 결합
   */
  static std::string Join(const std::string &base,
                          const std::string &relative) {
#if HAS_FILESYSTEM
    try {
      std::filesystem::path result = std::filesystem::path(base) / relative;
      return result.string();
    } catch (const std::exception &) {
      // 폴백: 수동 결합
    }
#endif
    // 수동 경로 결합
    std::string result = base;
    if (!result.empty() && result.back() != '/' && result.back() != '\\') {
      result += GetSeparator();
    }
    result += relative;
    return result;
  }

  /**
   * @brief 여러 경로 요소를 결합
   */
  static std::string Join(const std::vector<std::string> &parts) {
    if (parts.empty())
      return "";

    std::string result = parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
      result = Join(result, parts[i]);
    }
    return result;
  }

  /**
   * @brief 경로를 정규화 (. 및 .. 처리, 중복 구분자 제거)
   */
  static std::string Normalize(const std::string &path) {
#if HAS_FILESYSTEM
    try {
      std::filesystem::path fs_path(path);
      return fs_path.lexically_normal().string();
    } catch (const std::exception &) {
      // 폴백: 기본 정규화
    }
#endif
    // 기본 정규화 (중복 구분자 제거)
    std::string result = path;

    // 중복 구분자 제거
    std::string double_sep = std::string(1, GetSeparator()) + GetSeparator();
    size_t pos = 0;
    while ((pos = result.find(double_sep, pos)) != std::string::npos) {
      result.erase(pos, 1);
    }

    return result;
  }

  /**
   * @brief ANSI (CP949) 문자열을 UTF-8(로그 출력용)으로 변환
   */
  static std::string ToUtf8(const char *ansiStr) {
#if PULSEONE_WINDOWS
    if (!ansiStr || !ansiStr[0])
      return "";
    // 1. ANSI -> UTF-16
    int wLen = MultiByteToWideChar(CP_ACP, 0, ansiStr, -1, nullptr, 0);
    if (wLen <= 0)
      return ansiStr;
    std::wstring wStr(wLen, 0);
    MultiByteToWideChar(CP_ACP, 0, ansiStr, -1, &wStr[0], wLen);

    // 2. UTF-16 -> UTF-8
    int uLen = WideCharToMultiByte(CP_UTF8, 0, wStr.c_str(), -1, nullptr, 0,
                                   nullptr, nullptr);
    if (uLen <= 0)
      return ansiStr;
    std::string uStr(uLen - 1, 0); // null terminator 제거
    WideCharToMultiByte(CP_UTF8, 0, wStr.c_str(), -1, &uStr[0], uLen, nullptr,
                        nullptr);
    return uStr;
#else
    return ansiStr ? ansiStr : "";
#endif
  }

#if PULSEONE_WINDOWS
  /**
   * @brief UTF-16 문자열을 UTF-8로 변환 (GetModuleFileNameW 등 W API 결과
   * 처리용)
   */
  static std::string ToUtf8(const wchar_t *wStr) {
    if (!wStr || !wStr[0])
      return "";
    int uLen =
        WideCharToMultiByte(CP_UTF8, 0, wStr, -1, nullptr, 0, nullptr, nullptr);
    if (uLen <= 0)
      return "";
    std::string uStr(uLen - 1, 0); // Remove null terminator
    WideCharToMultiByte(CP_UTF8, 0, wStr, -1, &uStr[0], uLen, nullptr, nullptr);
    return uStr;
  }
  static std::string ToUtf8(const std::wstring &wStr) {
    return ToUtf8(wStr.c_str());
  }
#endif

  /**
   * @brief 절대 경로로 변환
   */
  static std::string GetAbsolute(const std::string &path) {
#if HAS_FILESYSTEM
    try {
      return std::filesystem::absolute(path).string();
    } catch (const std::exception &) {
      // 폴백: 플랫폼별 구현
    }
#endif

#if PULSEONE_WINDOWS
    char abs_path[MAX_PATH];
    if (GetFullPathNameA(path.c_str(), MAX_PATH, abs_path, nullptr)) {
      return ToUtf8(abs_path);
    }
#else
    char abs_path[PATH_MAX];
    if (realpath(path.c_str(), abs_path)) {
      return std::string(abs_path);
    }
#endif
    return path; // 실패 시 원본 반환
  }

  /**
   * @brief 디렉토리 부분 추출
   */
  static std::string GetDirectory(const std::string &path) {
#if HAS_FILESYSTEM
    try {
      return std::filesystem::path(path).parent_path().string();
    } catch (const std::exception &) {
      // 폴백
    }
#endif

    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
      return path.substr(0, pos);
    }
    return "."; // 현재 디렉토리
  }

  /**
   * @brief 파일명 부분 추출
   */
  static std::string GetFileName(const std::string &path) {
#if HAS_FILESYSTEM
    try {
      return std::filesystem::path(path).filename().string();
    } catch (const std::exception &) {
      // 폴백
    }
#endif

    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
      return path.substr(pos + 1);
    }
    return path;
  }

  /**
   * @brief 확장자 추출
   */
  static std::string GetExtension(const std::string &path) {
#if HAS_FILESYSTEM
    try {
      return std::filesystem::path(path).extension().string();
    } catch (const std::exception &) {
      // 폴백
    }
#endif

    size_t pos = path.find_last_of('.');
    size_t sep_pos = path.find_last_of("/\\");

    // 확장자가 경로 구분자 이후에 있는지 확인
    if (pos != std::string::npos &&
        (sep_pos == std::string::npos || pos > sep_pos)) {
      return path.substr(pos);
    }
    return "";
  }

  /**
   * @brief 현재 실행 파일의 디렉토리 반환
   */
  static std::string GetExecutableDirectory() {
#if PULSEONE_WINDOWS
    wchar_t buffer[MAX_PATH];
    DWORD result = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (result > 0 && result < MAX_PATH) {
      return GetDirectory(ToUtf8(buffer));
    }
#else
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
      buffer[len] = '\0';
      return GetDirectory(std::string(buffer));
    }
#endif
    return "."; // 폴백
  }

  /**
   * @brief 경로 유형 변환 (Windows <-> Unix 스타일)
   */
  static std::string ToNativeStyle(const std::string &path) {
    std::string result = path;

#if PULSEONE_WINDOWS
    // Unix 스타일을 Windows 스타일로 변환
    std::replace(result.begin(), result.end(), '/', '\\');
#else
    // Windows 스타일을 Unix 스타일로 변환
    std::replace(result.begin(), result.end(), '\\', '/');
#endif

    return result;
  }
};

// =============================================================================
// 파일 시스템 유틸리티 (개선된 버전)
// =============================================================================
class FileSystem {
public:
#if PULSEONE_WINDOWS
  /**
   * @brief UTF-8 문자열을 UTF-16 (Windows API용)으로 변환
   */
  static std::wstring ToUtf16(const std::string &utf8Str) {
    if (utf8Str.empty())
      return L"";
    int wLen = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, nullptr, 0);
    if (wLen <= 0)
      return L"";
    std::wstring wStr(wLen - 1, 0); // null terminator 제거
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &wStr[0], wLen);
    return wStr;
  }
#endif

  static bool CreateDirectory(const std::string &path) {
#if HAS_FILESYSTEM
    try {
      return std::filesystem::create_directories(path);
    } catch (const std::exception &) {
      // 폴백: 플랫폼별 구현
    }
#endif

#if PULSEONE_WINDOWS
    std::wstring wPath = ToUtf16(path);
    return ::CreateDirectoryW(wPath.c_str(), nullptr) != 0 ||
           ::GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
  }

  static bool DirectoryExists(const std::string &path) {
#if HAS_FILESYSTEM
    try {
      return std::filesystem::exists(path) &&
             std::filesystem::is_directory(path);
    } catch (const std::exception &) {
      // 폴백
    }
#endif

#if PULSEONE_WINDOWS
    std::wstring wPath = ToUtf16(path);
    DWORD attrs = ::GetFileAttributesW(wPath.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES) &&
           (attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
  }

  static bool FileExists(const std::string &path) {
#if HAS_FILESYSTEM
    try {
      return std::filesystem::exists(path) &&
             std::filesystem::is_regular_file(path);
    } catch (const std::exception &) {
      // 폴백
    }
#endif

#if PULSEONE_WINDOWS
    std::wstring wPath = ToUtf16(path);
    DWORD attrs = ::GetFileAttributesW(wPath.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES) &&
           !(attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
#endif
  }

  static bool Exists(const std::string &path) {
#if HAS_FILESYSTEM
    try {
      return std::filesystem::exists(path);
    } catch (const std::exception &) {
      // 폴백
    }
#endif

#if PULSEONE_WINDOWS
    std::wstring wPath = ToUtf16(path);
    return ::GetFileAttributesW(wPath.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0;
#endif
  }

  /**
   * @brief 디렉토리 내 파일 목록 반환
   */
  static std::vector<std::string> ListFiles(const std::string &directory_path) {
    std::vector<std::string> files;
#if HAS_FILESYSTEM
    try {
      if (DirectoryExists(directory_path)) {
        for (const auto &entry :
             std::filesystem::directory_iterator(directory_path)) {
          if (entry.is_regular_file()) {
            files.push_back(entry.path().string());
          }
        }
      }
    } catch (const std::exception &) {
      // 폴백
    }
#endif

    if (files.empty()) {
#if PULSEONE_WINDOWS
      WIN32_FIND_DATAW find_data;
      std::string search_path = directory_path + "\\*";
      std::wstring wSearchPath = ToUtf16(search_path);
      HANDLE hFind = FindFirstFileW(wSearchPath.c_str(), &find_data);
      if (hFind != INVALID_HANDLE_VALUE) {
        do {
          if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            // find_data.cFileName is a wide string (UTF-16)
            // Convert it back to UTF-8 for our internal string representations
            std::string utf8_filename = Path::ToUtf8(find_data.cFileName);
            files.push_back(Path::Join(directory_path, utf8_filename));
          }
        } while (FindNextFileW(hFind, &find_data));
        FindClose(hFind);
      }
#else
      // Linux fallback could use opendir/readdir if needed
#endif
    }
    return files;
  }

  /**
   * @brief 재귀적으로 디렉토리 생성
   */
  static bool CreateDirectoryRecursive(const std::string &path) {
    if (DirectoryExists(path)) {
      return true;
    }

    // 부모 디렉토리 먼저 생성
    std::string parent = Path::GetDirectory(path);
    if (!parent.empty() && parent != path && parent != ".") {
      if (!CreateDirectoryRecursive(parent)) {
        return false;
      }
    }

    return CreateDirectory(path);
  }
};

// =============================================================================
// Socket 래퍼 클래스 (기존 유지)
// =============================================================================
class Socket {
public:
  static int Poll(struct pollfd *fds, size_t nfds, int timeout) {
#if PULSEONE_WINDOWS
    // Windows에서는 select 사용
    fd_set readfds, writefds, exceptfds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    int max_fd = 0;
    for (size_t i = 0; i < nfds; ++i) {
      if (fds[i].fd > max_fd)
        max_fd = fds[i].fd;
      if (fds[i].events & POLLIN)
        FD_SET(fds[i].fd, &readfds);
      if (fds[i].events & POLLOUT)
        FD_SET(fds[i].fd, &writefds);
      FD_SET(fds[i].fd, &exceptfds);
    }

    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    return select(max_fd + 1, &readfds, &writefds, &exceptfds,
                  timeout >= 0 ? &tv : nullptr);
#else
    return poll(fds, nfds, timeout);
#endif
  }

  static int CloseSocket(int sockfd) {
#if PULSEONE_WINDOWS
    return closesocket(sockfd);
#else
    return close(sockfd);
#endif
  }
};

// =============================================================================
// 시간 관련 유틸리티 (기존 유지)
// =============================================================================
class Time {
public:
  static uint64_t GetTickCount() {
#if PULSEONE_WINDOWS
    // GetTickCount64 대신 GetTickCount 사용 (MinGW 호환)
    return static_cast<uint64_t>(::GetTickCount());
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000ULL +
           static_cast<uint64_t>(ts.tv_nsec) / 1000000ULL;
#endif
  }

  static void Sleep(uint32_t milliseconds) {
#if PULSEONE_WINDOWS
    ::Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
  }
};

// =============================================================================
// 동적 라이브러리 로딩 (기존 유지)
// =============================================================================
class DynamicLibrary {
public:
  static void *LoadLibrary(const std::string &filename) {
#if PULSEONE_WINDOWS
    std::wstring wFilename = FileSystem::ToUtf16(filename);
    return ::LoadLibraryW(wFilename.c_str());
#else
    return dlopen(filename.c_str(), RTLD_LAZY);
#endif
  }

  static void *GetSymbol(void *handle, const std::string &name) {
#if PULSEONE_WINDOWS
    // FARPROC를 void*로 안전하게 캐스팅
    return reinterpret_cast<void *>(
        ::GetProcAddress(static_cast<HMODULE>(handle), name.c_str()));
#else
    return dlsym(handle, name.c_str());
#endif
  }

  static void FreeLibrary(void *handle) {
#if PULSEONE_WINDOWS
    ::FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
  }
};

// =============================================================================
// 프로세스 관련 유틸리티 (기존 유지)
// =============================================================================
class Process {
public:
  static uint32_t GetCurrentProcessId() {
#if PULSEONE_WINDOWS
    return ::GetCurrentProcessId();
#else
    return static_cast<uint32_t>(getpid());
#endif
  }

  static uint32_t GetCurrentThreadId() {
#if PULSEONE_WINDOWS
    return ::GetCurrentThreadId();
#else
    return (uint32_t)(uintptr_t)pthread_self();
#endif
  }
};

// =============================================================================
// 문자열 변환 유틸리티 (기존 유지)
// =============================================================================
class String {
public:
  // UniqueId를 문자열로 변환
  static std::string UniqueIdToString(const PulseOne::UniqueId &UniqueId) {
    // UniqueId가 이미 string이므로 그대로 반환
    return UniqueId;
  }

  // 문자열을 UniqueId로 변환
  static PulseOne::UniqueId StringToUniqueId(const std::string &str) {
    return str; // UniqueId가 string이므로 그대로 반환
  }
};

} // namespace Platform
} // namespace PulseOne

#endif // PLATFORM_COMPAT_H