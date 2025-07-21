#!/bin/bash
# collector/restart.sh
# PulseOne Collector 간단한 재시작 자동화 스크립트

set -e  # 에러 발생 시 중단

# 색상 코드
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${CYAN}"
echo "🔥 ============================================= 🔥"
echo "   PulseOne Collector 간단한 재시작"
echo "   복잡한 DeviceManager 제거하고 처음부터!"
echo "🔥 ============================================= 🔥"
echo -e "${NC}"

# 현재 위치 확인
if [[ ! -f "Makefile" ]]; then
    echo -e "${RED}❌ collector 디렉토리에서 실행하세요!${NC}"
    echo "현재 위치: $(pwd)"
    exit 1
fi

echo -e "${BLUE}📍 현재 위치: $(pwd)${NC}"

# 1단계: 기존 복잡한 코드 백업
echo -e "${YELLOW}🗂️ 1단계: 기존 코드 백업 중...${NC}"

# 백업 디렉토리 생성
mkdir -p backup/$(date +%Y%m%d_%H%M%S)
BACKUP_DIR="backup/$(date +%Y%m%d_%H%M%S)"

# DeviceManager 백업 (있으면)
if [[ -f "include/Engine/DeviceManager.h" ]]; then
    echo -e "${PURPLE}  📦 DeviceManager.h 백업...${NC}"
    cp include/Engine/DeviceManager.h "$BACKUP_DIR/"
fi

if [[ -f "src/Engine/DeviceManager.cpp" ]]; then
    echo -e "${PURPLE}  📦 DeviceManager.cpp 백업...${NC}"
    cp src/Engine/DeviceManager.cpp "$BACKUP_DIR/"
fi

# Engine 디렉토리 전체 백업 (있으면)
if [[ -d "include/Engine" ]]; then
    echo -e "${PURPLE}  📦 Engine 디렉토리 백업...${NC}"
    cp -r include/Engine "$BACKUP_DIR/include_Engine_backup" 2>/dev/null || true
fi

if [[ -d "src/Engine" ]]; then
    echo -e "${PURPLE}  📦 Engine 소스 백업...${NC}"
    cp -r src/Engine "$BACKUP_DIR/src_Engine_backup" 2>/dev/null || true
fi

echo -e "${GREEN}✅ 백업 완료: $BACKUP_DIR${NC}"

# 2단계: 복잡한 파일들 제거
echo -e "${YELLOW}🗑️ 2단계: 복잡한 파일들 제거...${NC}"

# DeviceManager 관련 파일들 제거
rm -f include/Engine/DeviceManager.h
rm -f src/Engine/DeviceManager.cpp

# 복잡한 Engine 헤더들 제거 (하지만 기본 것들은 유지)
rm -f include/Engine/DeviceWorker*.h
rm -f include/Engine/VirtualPoint*.h  
rm -f include/Engine/Alarm*.h
rm -f include/Engine/DeviceControl*.h

# 복잡한 Engine 소스들 제거
rm -f src/Engine/DeviceWorker*.cpp
rm -f src/Engine/VirtualPoint*.cpp
rm -f src/Engine/Alarm*.cpp  
rm -f src/Engine/DeviceControl*.cpp

echo -e "${GREEN}✅ 복잡한 파일들 제거 완료${NC}"

# 3단계: 새로운 간단한 구조 생성
echo -e "${YELLOW}🏗️ 3단계: 새로운 간단한 구조 생성...${NC}"

# 필요한 디렉토리 생성
mkdir -p include/Core
mkdir -p src/Core

# CollectorApplication.h 생성
echo -e "${BLUE}  📝 CollectorApplication.h 생성...${NC}"
cat > include/Core/CollectorApplication.h << 'EOF'
// collector/include/Core/CollectorApplication.h
// 간단하고 실용적인 메인 애플리케이션 클래스
#pragma once

#include <atomic>
#include <memory>
#include "Utils/LogManager.h"
#include "Config/ConfigManager.h"

namespace PulseOne {
namespace Core {

/**
 * @brief 메인 컬렉터 애플리케이션
 * @details 복잡한 의존성 없이 기본 기능만 제공하는 간단한 구조
 */
class CollectorApplication {
private:
    // 기본 구성요소들
    LogManager& logger_;
    std::unique_ptr<ConfigManager> config_;
    std::atomic<bool> running_{false};
    
    // 상태 정보
    std::string version_ = "2.0.0-simple";
    std::chrono::system_clock::time_point start_time_;

public:
    CollectorApplication();
    ~CollectorApplication();
    
    // 복사/이동 생성자 비활성화
    CollectorApplication(const CollectorApplication&) = delete;
    CollectorApplication& operator=(const CollectorApplication&) = delete;
    
    /**
     * @brief 애플리케이션 메인 실행
     */
    void Run();
    
    /**
     * @brief 애플리케이션 중지 요청
     */
    void Stop();
    
    /**
     * @brief 현재 실행 상태 확인
     */
    bool IsRunning() const { return running_.load(); }
    
    /**
     * @brief 버전 정보 반환
     */
    std::string GetVersion() const { return version_; }

private:
    bool Initialize();
    void MainLoop();
    void Cleanup();
    void PrintStatus();
};

} // namespace Core
} // namespace PulseOne
EOF

# CollectorApplication.cpp 생성  
echo -e "${BLUE}  📝 CollectorApplication.cpp 생성...${NC}"
cat > src/Core/CollectorApplication.cpp << 'EOF'
// collector/src/Core/CollectorApplication.cpp
#include "Core/CollectorApplication.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace PulseOne {
namespace Core {

CollectorApplication::CollectorApplication() 
    : logger_(LogManager::GetInstance())
    , start_time_(std::chrono::system_clock::now()) {
    
    logger_.Info("🚀 CollectorApplication v" + version_ + " created");
}

CollectorApplication::~CollectorApplication() {
    if (running_.load()) {
        Stop();
    }
    logger_.Info("💀 CollectorApplication destroyed");
}

void CollectorApplication::Run() {
    logger_.Info("🏁 Starting CollectorApplication...");
    
    try {
        if (!Initialize()) {
            logger_.Error("❌ Initialization failed");
            return;
        }
        
        logger_.Info("✅ Initialization successful - entering main loop");
        MainLoop();
        Cleanup();
        
    } catch (const std::exception& e) {
        logger_.Error("💥 Fatal exception: " + std::string(e.what()));
    }
}

bool CollectorApplication::Initialize() {
    try {
        config_ = std::make_unique<ConfigManager>();
        
        // 설정 파일 로드 시도
        std::vector<std::string> config_paths = {
            "../config/.env",
            "/etc/pulseone/.env", 
            ".env"
        };
        
        bool config_loaded = false;
        for (const auto& path : config_paths) {
            if (config_->LoadFromFile(path)) {
                logger_.Info("✅ Configuration loaded from: " + path);
                config_loaded = true;
                break;
            }
        }
        
        if (!config_loaded) {
            logger_.Warn("⚠️ No config file found - using defaults");
        }
        
        start_time_ = std::chrono::system_clock::now();
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("💥 Init exception: " + std::string(e.what()));
        return false;
    }
}

void CollectorApplication::MainLoop() {
    running_.store(true);
    int heartbeat_counter = 0;
    
    while (running_.load()) {
        try {
            heartbeat_counter++;
            
            if (heartbeat_counter % 12 == 0) {
                PrintStatus();
                heartbeat_counter = 0;
            } else {
                logger_.Debug("💗 Heartbeat #" + std::to_string(heartbeat_counter));
            }
            
            // TODO: 여기서 실제 데이터 수집 작업 추가
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
        } catch (const std::exception& e) {
            logger_.Error("💥 MainLoop exception: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    logger_.Info("🛑 Main loop stopped");
}

void CollectorApplication::Stop() {
    logger_.Info("🛑 Stop requested");
    running_.store(false);
}

void CollectorApplication::Cleanup() {
    logger_.Info("🧹 Cleanup completed");
}

void CollectorApplication::PrintStatus() {
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    
    auto hours = std::chrono::duration_cast<std::chrono::hours>(uptime);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(uptime % std::chrono::hours(1));
    auto seconds = uptime % std::chrono::minutes(1);
    
    std::stringstream status;
    status << "📊 STATUS: v" << version_ 
           << " | Uptime: " << hours.count() << "h " 
           << minutes.count() << "m " << seconds.count() << "s";
    
    logger_.Info(status.str());
}

} // namespace Core
} // namespace PulseOne
EOF

# main.cpp 수정
echo -e "${BLUE}  📝 main.cpp 수정...${NC}"
cat > src/main.cpp << 'EOF'
#include <iostream>
#include <signal.h>
#include <memory>
#include "Core/CollectorApplication.h"

using namespace PulseOne::Core;

std::unique_ptr<CollectorApplication> g_app;

void SignalHandler(int signal) {
    std::cout << "\n🛑 종료 신호 받음" << std::endl;
    if (g_app) {
        g_app->Stop();
    }
}

int main() {
    try {
        signal(SIGINT, SignalHandler);
        
        std::cout << R"(
🚀 PulseOne Collector v2.0 (Simple Edition)
Enterprise Industrial Data Collection System
)" << std::endl;
        
        g_app = std::make_unique<CollectorApplication>();
        g_app->Run();
        
        std::cout << "✅ 정상 종료" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "💥 오류: " << e.what() << std::endl;
        return -1;
    }
}
EOF

echo -e "${GREEN}✅ 새로운 구조 생성 완료${NC}"

# 4단계: 간단한 Makefile 생성
echo -e "${YELLOW}📝 4단계: 간단한 Makefile 생성...${NC}"

# 기존 Makefile 백업
if [[ -f "Makefile" ]]; then
    cp Makefile "$BACKUP_DIR/Makefile.backup"
fi

# 새로운 간단한 Makefile 생성
cat > Makefile << 'EOF'
# PulseOne Collector 간단한 Makefile

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude
TARGET = pulseone_collector

# 핵심 소스만
SOURCES = src/main.cpp \
          src/Core/CollectorApplication.cpp \
          src/Config/ConfigManager.cpp \
          src/Utils/LogManager.cpp

OBJECTS = $(SOURCES:.cpp=.o)
LIBS = -lpthread

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "🔗 Linking $(TARGET)..."
	$(CXX) -o $@ $^ $(LIBS)
	@echo "✅ Build successful!"

%.o: %.cpp
	@echo "🔨 Compiling: $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
EOF

echo -e "${GREEN}✅ 간단한 Makefile 생성 완료${NC}"

# 5단계: 빌드 테스트
echo -e "${YELLOW}🔨 5단계: 빌드 테스트...${NC}"

# 필요한 파일들 존재 확인
echo -e "${BLUE}  📋 필요 파일 확인:${NC}"
REQUIRED_FILES=(
    "include/Utils/LogManager.h"
    "include/Config/ConfigManager.h"
    "src/Utils/LogManager.cpp"
    "src/Config/ConfigManager.cpp"
)

ALL_EXIST=true
for file in "${REQUIRED_FILES[@]}"; do
    if [[ -f "$file" ]]; then
        echo -e "${GREEN}    ✅ $file${NC}"
    else
        echo -e "${RED}    ❌ $file (missing)${NC}"
        ALL_EXIST=false
    fi
done

if [[ "$ALL_EXIST" = true ]]; then
    echo -e "${GREEN}✅ 모든 필요 파일 존재${NC}"
    
    # 클린 빌드 시도
    echo -e "${BLUE}  🧹 클린 빌드 시도...${NC}"
    make clean
    
    echo -e "${BLUE}  🔨 컴파일 시도...${NC}"
    if make; then
        echo -e "${GREEN}🎉 컴파일 성공!${NC}"
        echo -e "${CYAN}🚀 실행 방법: ./$(TARGET)${NC}"
        echo -e "${CYAN}🧪 테스트: make run${NC}"
    else
        echo -e "${RED}❌ 컴파일 실패${NC}"
        echo -e "${YELLOW}💡 에러를 확인하고 필요한 파일을 수정하세요${NC}"
    fi
else
    echo -e "${RED}❌ 필요한 파일이 없어서 빌드를 건너뜁니다${NC}"
    echo -e "${YELLOW}💡 없는 파일들을 먼저 생성하세요${NC}"
fi

# 완료 메시지
echo -e "${CYAN}"
echo "🎉 ============================================= 🎉"
echo "   PulseOne Collector 간단한 재시작 완료!"
echo ""
echo "   📁 백업 위치: $BACKUP_DIR"
echo "   🎯 새로운 구조: 최소 의존성, 최대 단순성"
echo "   🚀 다음 단계: 빌드 성공 후 점진적 기능 추가"
echo "🎉 ============================================= 🎉"
echo -e "${NC}"
EOF

# 스크립트에 실행 권한 부여
chmod +x restart.sh

echo -e "${GREEN}✅ restart.sh 스크립트가 생성되었습니다!${NC}"
echo -e "${CYAN}🚀 실행 방법: ./restart.sh${NC}"

