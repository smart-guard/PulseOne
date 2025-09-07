#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

/**
 * @file PlatformCompat.h
 * @brief 크로스 플랫폼 호환성 레이어 - Windows/Linux 통합 (수정판)
 * @author PulseOne Development Team
 * @date 2025-09-06
 */

// 표준 헤더들 먼저 (순서 중요!)
#include <string>
#include <cstdint>
#include <ctime>

// =============================================================================
// 플랫폼 감지
// =============================================================================
#if defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) || defined(__MINGW64__)
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
    
    // UUID 타입 충돌 방지 (Windows API vs 사용자 정의)
    #ifdef UUID
        #undef UUID
    #endif
    
    // 순서 중요: winsock2.h -> ws2tcpip.h -> windows.h
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    
    // 링커 지시문
    //#pragma comment(lib, "ws2_32.lib")
    //#pragma comment(lib, "iphlpapi.lib")
    
    // Windows 전용 매크로들
    #define SAFE_LOCALTIME(t, tm) localtime_s(tm, t)
    #define SAFE_GMTIME(t, tm) gmtime_s(tm, t)
    
    // Windows SQL 지원 비활성화 (충돌 방지)
    #ifndef PULSEONE_DISABLE_MSSQL
        #define PULSEONE_DISABLE_MSSQL 1
    #endif
    
    // Windows용 poll 상수 정의
    #define POLLIN  0x0001
    #define POLLOUT 0x0004
    
#else
    // Linux/Unix 설정
    #include <unistd.h>
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <poll.h>
    #include <dlfcn.h>
    #include <sys/stat.h>
    #include <time.h>
    
    #define SAFE_LOCALTIME(t, tm) localtime_r(t, tm)
    #define SAFE_GMTIME(t, tm) gmtime_r(t, tm)
#endif

namespace PulseOne {
    // 모든 플랫폼에서 UUID는 string으로 통일
    using UUID = std::string;
}

namespace PulseOne {
namespace Platform {

// =============================================================================
// pollfd 구조체 정의 (Windows용)
// =============================================================================
#if PULSEONE_WINDOWS
struct pollfd {
    int fd;      // 파일 디스크립터
    short events; // 관심 있는 이벤트
    short revents; // 발생한 이벤트
};
#endif

// =============================================================================
// Socket 래퍼 클래스
// =============================================================================
class Socket {
public:
    static int Poll(struct pollfd* fds, size_t nfds, int timeout) {
#if PULSEONE_WINDOWS
        // Windows에서는 select 사용
        fd_set readfds, writefds, exceptfds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&exceptfds);
        
        int max_fd = 0;
        for (size_t i = 0; i < nfds; ++i) {
            if (fds[i].fd > max_fd) max_fd = fds[i].fd;
            if (fds[i].events & POLLIN) FD_SET(fds[i].fd, &readfds);
            if (fds[i].events & POLLOUT) FD_SET(fds[i].fd, &writefds);
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
// 시간 관련 유틸리티
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
// 파일 시스템 유틸리티
// =============================================================================
class FileSystem {
public:
    static bool CreateDirectory(const std::string& path) {
#if PULSEONE_WINDOWS
        return ::CreateDirectoryA(path.c_str(), NULL) != 0 ||
               ::GetLastError() == ERROR_ALREADY_EXISTS;
#else
        return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
    }
    
    static bool DirectoryExists(const std::string& path) {
#if PULSEONE_WINDOWS
        DWORD attrs = ::GetFileAttributesA(path.c_str());
        return (attrs != INVALID_FILE_ATTRIBUTES) && 
               (attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
        struct stat st;
        return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
    }
};

// =============================================================================
// 동적 라이브러리 로딩
// =============================================================================
class DynamicLibrary {
public:
    static void* LoadLibrary(const std::string& filename) {
#if PULSEONE_WINDOWS
        return ::LoadLibraryA(filename.c_str());
#else
        return dlopen(filename.c_str(), RTLD_LAZY);
#endif
    }
    
    static void* GetSymbol(void* handle, const std::string& name) {
#if PULSEONE_WINDOWS
        // FARPROC를 void*로 안전하게 캐스팅
        return reinterpret_cast<void*>(
            ::GetProcAddress(static_cast<HMODULE>(handle), name.c_str())
        );
#else
        return dlsym(handle, name.c_str());
#endif
    }
    
    static void FreeLibrary(void* handle) {
#if PULSEONE_WINDOWS
        ::FreeLibrary(static_cast<HMODULE>(handle));
#else
        dlclose(handle);
#endif
    }
};

// =============================================================================
// 프로세스 관련 유틸리티
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
        return static_cast<uint32_t>(pthread_self());
#endif
    }
};

// =============================================================================
// 문자열 변환 유틸리티
// =============================================================================
class String {
public:
    // UUID를 문자열로 변환
    static std::string UUIDToString(const PulseOne::UUID& uuid) {
        // UUID가 이미 string이므로 그대로 반환
        return uuid;
    }
    
    // 문자열을 UUID로 변환
    static PulseOne::UUID StringToUUID(const std::string& str) {
        return str;  // UUID가 string이므로 그대로 반환
    }
};

} // namespace Platform
} // namespace PulseOne

#endif // PLATFORM_COMPAT_H