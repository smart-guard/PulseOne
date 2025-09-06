#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

// Windows에서는 반드시 winsock2.h를 windows.h보다 먼저!
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    
    // 순서 중요!
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <poll.h>
#endif

#include <string>
#include <cstdint>

namespace PulseOne {
namespace Platform {

class Socket {
public:
    static int Poll(struct pollfd* fds, size_t nfds, int timeout) {
#ifdef _WIN32
        // WSAPoll 대신 select 사용 (크로스 컴파일 호환성)
        fd_set readfds, writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        
        struct timeval tv;
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        
        return select(0, &readfds, &writefds, NULL, &tv);
#else
        return poll(fds, nfds, timeout);
#endif
    }
};

class Time {
public:
    static uint64_t GetTickCount() {
#ifdef _WIN32
        // GetTickCount64 대신 GetTickCount 사용
        return ::GetTickCount();
#else
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;
#endif
    }
};

class FileSystem {
public:
    static bool CreateDirectory(const std::string& path) {
#ifdef _WIN32
        // :: 전역 스코프 명시
        return ::CreateDirectoryA(path.c_str(), NULL) || 
               ::GetLastError() == ERROR_ALREADY_EXISTS;
#else
        return mkdir(path.c_str(), 0755) == 0;
#endif
    }
};

class DynamicLibrary {
public:
    static void* GetSymbol(void* handle, const std::string& name) {
#ifdef _WIN32
        // FARPROC를 void*로 캐스팅
        return reinterpret_cast<void*>(::GetProcAddress((HMODULE)handle, name.c_str()));
#else
        return dlsym(handle, name.c_str());
#endif
    }
};

} // namespace Platform
} // namespace PulseOne

#endif
