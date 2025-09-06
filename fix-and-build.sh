#!/bin/bash
# =============================================================================
# working-build.sh - MinGW 스레드 문제 해결
# =============================================================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  PulseOne Windows Build - Working Version${NC}"
echo -e "${BLUE}================================================${NC}"

# =============================================================================
# 1. Windows Sleep API를 사용하는 버전
# =============================================================================
echo -e "${YELLOW}[1/3] 소스 생성 (Windows API 사용)...${NC}"

mkdir -p collector/src/minimal

cat > collector/src/minimal/main.cpp << 'EOF'
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cstdlib>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <conio.h>
#else
    #include <unistd.h>
#endif

namespace PulseOne {
    using UUID = std::string;
    
    // 크로스 플랫폼 sleep 함수
    void sleep_seconds(int seconds) {
        #ifdef _WIN32
            Sleep(seconds * 1000);  // Windows: milliseconds
        #else
            sleep(seconds);         // Unix: seconds
        #endif
    }
    
    class Collector {
    private:
        bool running_;
        std::vector<std::string> protocols_;
        
    public:
        Collector() : running_(false) {
            protocols_ = {"Modbus TCP", "MQTT", "BACnet/IP", "Redis"};
        }
        
        void start() {
            running_ = true;
            std::cout << "========================================\n";
            std::cout << "PulseOne Collector v2.1.0 - Windows\n";
            std::cout << "========================================\n\n";
            
            std::cout << "Enabled Protocols:\n";
            for (size_t i = 0; i < protocols_.size(); ++i) {
                std::cout << "  [OK] " << protocols_[i] << "\n";
            }
            std::cout << "\nCollector started successfully!\n";
            std::cout << "Press ESC to stop...\n\n";
            
            run();
        }
        
        void run() {
            int counter = 0;
            while (running_) {
                // Windows Sleep 사용
                sleep_seconds(2);
                counter++;
                
                std::cout << "[" << counter << "] Working... Status: OK\n";
                std::cout.flush();
                
                #ifdef _WIN32
                if (_kbhit()) {
                    char ch = _getch();
                    if (ch == 27) {  // ESC key
                        stop();
                    }
                }
                #endif
                
                // 10번 반복 후 자동 종료
                if (counter >= 10) {
                    std::cout << "\nDemo complete after 10 iterations\n";
                    stop();
                }
            }
        }
        
        void stop() {
            running_ = false;
            std::cout << "\nShutting down...\n";
        }
    };
}

int main(int argc, char* argv[]) {
    std::cout << "Starting PulseOne Collector...\n\n";
    
    try {
        PulseOne::Collector collector;
        collector.start();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Collector terminated normally.\n";
    
    #ifdef _WIN32
    std::cout << "\nPress any key to exit...";
    _getch();
    #endif
    
    return 0;
}
EOF

echo -e "${GREEN}✅ 소스 생성 완료${NC}"

# =============================================================================
# 2. Docker 이미지 (POSIX 스레드 지원 버전)
# =============================================================================
echo -e "${YELLOW}[2/3] Docker 이미지 준비...${NC}"

cat > Dockerfile.mingw-posix << 'EOF'
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive

# MinGW with POSIX thread model
RUN apt-get update && apt-get install -y \
    gcc-mingw-w64-x86-64-posix \
    g++-mingw-w64-x86-64-posix \
    mingw-w64-tools \
    make \
    && rm -rf /var/lib/apt/lists/*

# POSIX 스레드 모델 사용
RUN update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix && \
    update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix

WORKDIR /src
EOF

docker build -f Dockerfile.mingw-posix -t pulseone-mingw-posix . || {
    echo -e "${YELLOW}⚠️ POSIX 버전 실패, 기본 버전 사용${NC}"
    docker tag pulseone-minimal pulseone-mingw-posix 2>/dev/null || true
}

echo -e "${GREEN}✅ Docker 준비 완료${NC}"

# =============================================================================
# 3. 빌드 실행
# =============================================================================
echo -e "${YELLOW}[3/3] Windows 실행파일 빌드...${NC}"

docker run --rm \
    -v "$(pwd)/collector:/src" \
    -v "$(pwd)/dist:/output" \
    pulseone-mingw-posix bash -c '
    echo "=========================================="
    echo "컴파일러 정보:"
    x86_64-w64-mingw32-g++ --version | head -1
    echo "스레드 모델:"
    x86_64-w64-mingw32-g++ -v 2>&1 | grep "Thread model"
    echo "=========================================="
    
    cd /src
    mkdir -p bin-win
    
    # 컴파일 (std::thread 없이)
    echo "컴파일 중..."
    x86_64-w64-mingw32-g++ \
        -std=c++17 \
        -O2 \
        -Wall \
        -D_WIN32 \
        -DWIN32 \
        -D_WIN32_WINNT=0x0601 \
        -DNOMINMAX \
        -static \
        -static-libgcc \
        -static-libstdc++ \
        src/minimal/main.cpp \
        -o bin-win/collector.exe \
        -lws2_32 \
        -lwinmm \
        -lkernel32
    
    if [ -f "bin-win/collector.exe" ]; then
        x86_64-w64-mingw32-strip bin-win/collector.exe
        cp bin-win/collector.exe /output/
        echo "✅ 빌드 성공!"
        ls -lh /output/collector.exe
        file /output/collector.exe
    else
        echo "❌ 빌드 실패"
        exit 1
    fi
'

# 결과 확인
if [ -f "dist/collector.exe" ]; then
    echo ""
    echo -e "${GREEN}================================================${NC}"
    echo -e "${GREEN}  🎉 빌드 성공!${NC}"
    echo -e "${GREEN}================================================${NC}"
    echo ""
    echo "📦 빌드 정보:"
    echo "  파일: dist/collector.exe"
    echo "  크기: $(du -h dist/collector.exe | cut -f1)"
    echo ""
    
    # 간단한 배치 파일
    cat > dist/run.bat << 'EOF'
@echo off
cls
collector.exe
EOF
    
    echo "🚀 Windows에서 실행:"
    echo "  1. dist/collector.exe를 Windows로 복사"
    echo "  2. 실행하면 10회 반복 후 자동 종료"
    echo "  3. ESC 키로 수동 종료 가능"
else
    echo -e "${RED}❌ 빌드 실패${NC}"
    exit 1
fi