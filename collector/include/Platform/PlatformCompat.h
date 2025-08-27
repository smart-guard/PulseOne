// =============================================================================
// collector/include/Platform/PlatformCompat.h
// 🛡️ PulseOne 플랫폼 호환성 레이어 - Windows/Linux 완전 호환
// 지금 바로 생성하여 향후 확장성 확보
// =============================================================================

#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

// =============================================================================
// 🔧 플랫폼 감지 매크로
// =============================================================================
#if defined(_WIN32) || defined(_WIN64)
    #define PULSEONE_WINDOWS 1
    #define PULSEONE_LINUX 0
    #define PULSEONE_MACOS 0
#elif defined(__linux__)
    #define PULSEONE_WINDOWS 0
    #define PULSEONE_LINUX 1
    #define PULSEONE_MACOS 0
#elif defined(__APPLE__)
    #define PULSEONE_WINDOWS 0
    #define PULSEONE_LINUX 0
    #define PULSEONE_MACOS 1
#else
    #error "Unsupported platform"
#endif

// =============================================================================
// 🔧 Windows 전용 헤더 및 설정
// =============================================================================
#if PULSEONE_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX  // min/max 매크로 충돌 방지
    #endif
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0A00  // Windows 10
    #endif
    
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    #include <process.h>
    #include <direct.h>
    #include <io.h>

// =============================================================================
// 🔧 Linux/Unix 전용 헤더
// =============================================================================
#elif PULSEONE_LINUX || PULSEONE_MACOS
    #include <unistd.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <sys/time.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <signal.h>
    #include <errno.h>
    #include <fcntl.h>
    #include <pthread.h>
    #include <semaphore.h>
    #include <dirent.h>
#endif

// =============================================================================
// 📂 공통 STL 헤더 (플랫폼 독립적)
// =============================================================================
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <functional>

namespace PulseOne {
namespace Platform {

// =============================================================================
// 🕐 시간 처리 래퍼 클래스 (핵심!)
// =============================================================================
class TimeUtils {
public:
    // ✅ 안전한 크로스 플랫폼 시간 변환 (LogManager에서 사용)
    static std::string FormatTimeISO8601(const std::chrono::system_clock::time_point& tp) {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::tm tm_buf{};
        
#if PULSEONE_WINDOWS
        if (gmtime_s(&tm_buf, &time_t) != 0) {
            return "1970-01-01T00:00:00Z";
        }
#else
        if (gmtime_r(&time_t, &tm_buf) == nullptr) {
            return "1970-01-01T00:00:00Z";
        }
#endif
        
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
        return std::string(buffer);
    }
    
    // ✅ localtime 안전 버전 (LogManager 수정용)
    static bool SafeLocaltime(const std::time_t* time_t_ptr, std::tm* tm_buf) {
#if PULSEONE_WINDOWS
        return localtime_s(tm_buf, time_t_ptr) == 0;
#else
        return localtime_r(time_t_ptr, tm_buf) != nullptr;
#endif
    }
    
    // ✅ 현재 시간
    static std::string GetCurrentTimeISO8601() {
        return FormatTimeISO8601(std::chrono::system_clock::now());
    }
};

// =============================================================================
// 📂 파일시스템 래퍼 클래스 (향후 사용)
// =============================================================================
class FileUtils {
public:
    static const char PATH_SEPARATOR = 
#if PULSEONE_WINDOWS
        '\\';
#else
        '/';
#endif
    
    static std::string JoinPath(const std::string& base, const std::string& sub) {
        if (base.empty()) return sub;
        if (sub.empty()) return base;
        
        std::string result = base;
        if (result.back() != PATH_SEPARATOR && result.back() != '/' && result.back() != '\\') {
            result += PATH_SEPARATOR;
        }
        
        std::string clean_sub = sub;
        while (!clean_sub.empty() && (clean_sub.front() == '/' || clean_sub.front() == '\\')) {
            clean_sub = clean_sub.substr(1);
        }
        
        return result + clean_sub;
    }
    
    static bool CreateDirectoryRecursive(const std::string& path) {
        try {
            return std::filesystem::create_directories(path);
        } catch (const std::exception&) {
            return false;
        }
    }
    
    static bool FileExists(const std::string& path) {
        try {
            return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
        } catch (const std::exception&) {
            return false;
        }
    }
};

// =============================================================================
// 🔧 프로세스 관리 래퍼 클래스 (향후 사용)
// =============================================================================
class ProcessUtils {
public:
    static int GetCurrentProcessId() {
#if PULSEONE_WINDOWS
        return static_cast<int>(GetCurrentProcessId());
#else
        return static_cast<int>(getpid());
#endif
    }
    
    static bool TerminateProcess(int pid) {
#if PULSEONE_WINDOWS
        HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
        if (process == NULL) return false;
        
        BOOL result = ::TerminateProcess(process, 1);
        CloseHandle(process);
        return result != FALSE;
#else
        return kill(pid, SIGTERM) == 0;
#endif
    }
};

// =============================================================================
// 🌐 네트워크 래퍼 클래스 (향후 사용)
// =============================================================================
class NetworkUtils {
public:
    static bool InitializeNetwork() {
#if PULSEONE_WINDOWS
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        return result == 0;
#else
        return true;
#endif
    }
    
    static void CleanupNetwork() {
#if PULSEONE_WINDOWS
        WSACleanup();
#endif
    }
    
    static void CloseSocket(int socket_fd) {
#if PULSEONE_WINDOWS
        closesocket(socket_fd);
#else
        close(socket_fd);
#endif
    }
};

// =============================================================================
// 🛡️ 예외 안전 래퍼 클래스 (향후 사용)
// =============================================================================
class SafeWrapper {
public:
    template<typename Func>
    static auto SafeExecute(Func&& func, const char* error_message = "Operation failed") 
        -> decltype(func()) {
        try {
            return func();
        } catch (const std::exception& e) {
            std::cerr << "SafeWrapper Error: " << error_message 
                      << " - " << e.what() << std::endl;
            if constexpr (std::is_same_v<decltype(func()), bool>) {
                return false;
            } else if constexpr (std::is_same_v<decltype(func()), int>) {
                return -1;
            } else if constexpr (std::is_same_v<decltype(func()), std::string>) {
                return std::string("Error: ") + e.what();
            }
            return decltype(func()){};
        }
    }
};

} // namespace Platform
} // namespace PulseOne

// =============================================================================
// 🔧 편의 매크로들 (향후 사용)
// =============================================================================
#define SAFE_EXECUTE(func, error_msg) \
    PulseOne::Platform::SafeWrapper::SafeExecute([&]() { return func; }, error_msg)

#define PLATFORM_SPECIFIC_CODE(windows_code, linux_code) \
    do { \
        if constexpr (PULSEONE_WINDOWS) { \
            windows_code \
        } else { \
            linux_code \
        } \
    } while(0)

// =============================================================================
// 🚀 즉시 사용 가능한 유틸리티들
// =============================================================================

// LogManager.cpp에서 바로 사용 가능
#define SAFE_LOCALTIME(time_ptr, tm_buf) \
    PulseOne::Platform::TimeUtils::SafeLocaltime(time_ptr, tm_buf)

// 새로운 코드에서 바로 사용 가능  
#define GET_CURRENT_TIME_ISO() \
    PulseOne::Platform::TimeUtils::GetCurrentTimeISO8601()

#define SAFE_CREATE_DIR(path) \
    PulseOne::Platform::FileUtils::CreateDirectoryRecursive(path)

#endif // PLATFORM_COMPAT_H