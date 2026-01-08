#include <gtest/gtest.h>
#include <iostream>

// Windows 헤더
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

// 싱글톤 접근용 헤더들
#include "Utils/ConfigManager.h"
#include "Logging/LogManager.h"
#include "DatabaseManager.hpp"

using namespace PulseOne;

class WindowsStep1Test : public ::testing::Test {
protected:
    void SetUp() override {
        std::cout << "\nWindows Step 1 테스트 시작\n";
    }
    
    void TearDown() override {
        std::cout << "Windows Step 1 테스트 완료\n";
    }
};

TEST_F(WindowsStep1Test, WindowsEnvironmentValidation) {
    std::cout << "Windows 환경 검증 테스트\n";
    
    // 1. 플랫폼 매크로 확인
    #ifdef _WIN32
        std::cout << "  WIN32 매크로 정의됨\n";
        EXPECT_TRUE(true);
    #else
        FAIL() << "Windows 매크로가 정의되지 않음";
    #endif
    
    // 2. Windows API 테스트 (GetTickCount64 대신 GetTickCount 사용)
    #ifdef _WIN32
        DWORD processId = GetCurrentProcessId();
        std::cout << "  현재 프로세스 ID: " << processId << "\n";
        EXPECT_GT(processId, 0);
        
        DWORD tickCount = GetTickCount();  // GetTickCount64 대신
        std::cout << "  시스템 실행 시간: " << tickCount << "ms\n";
        EXPECT_GT(tickCount, 0);
    #endif
    
    // 3. 파일 시스템 테스트
    char currentDir[1024];
    if (getcwd(currentDir, sizeof(currentDir)) != nullptr) {
        std::cout << "  현재 디렉토리: " << currentDir << "\n";
        EXPECT_TRUE(true);
    } else {
        FAIL() << "현재 디렉토리 접근 불가";
    }
}

TEST_F(WindowsStep1Test, SingletonManagerAccess) {
    std::cout << "싱글톤 매니저 접근 테스트\n";
    
    try {
        // ConfigManager 싱글톤 접근
        auto& configManager = ConfigManager::getInstance();
        std::cout << "  ConfigManager 싱글톤 접근 성공\n";
        EXPECT_TRUE(true);
        
        // LogManager 싱글톤 접근  
        auto& logManager = LogManager::getInstance();
        std::cout << "  LogManager 싱글톤 접근 성공\n";
        EXPECT_TRUE(true);
        
        // DbLib::DatabaseManager 싱글톤 접근
        auto& dbManager = DbLib::DatabaseManager::getInstance();
        std::cout << "  DbLib::DatabaseManager 싱글톤 접근 성공\n";
        EXPECT_TRUE(true);
        
    } catch (const std::exception& e) {
        std::cout << "  싱글톤 접근 실패: " << e.what() << "\n";
        FAIL() << "싱글톤 매니저 접근 실패";
    }
}

TEST_F(WindowsStep1Test, FileSystemTest) {
    std::cout << "파일 시스템 테스트\n";
    
    // 파일 생성/삭제 테스트
    std::string testFile = "test_windows.tmp";
    FILE* file = fopen(testFile.c_str(), "w");
    ASSERT_NE(file, nullptr) << "파일 생성 실패";
    
    fprintf(file, "Windows test\n");
    fclose(file);
    
    // 파일 읽기
    file = fopen(testFile.c_str(), "r");
    ASSERT_NE(file, nullptr) << "파일 읽기 실패";
    
    char buffer[100];
    fgets(buffer, sizeof(buffer), file);
    fclose(file);
    
    EXPECT_STRNE(buffer, "") << "파일 내용이 비어있음";
    
    // 파일 삭제
    EXPECT_EQ(remove(testFile.c_str()), 0) << "파일 삭제 실패";
    
    std::cout << "  파일 I/O 테스트 완료\n";
}

int main(int argc, char** argv) {
    std::cout << "PulseOne Windows Step 1 테스트 시작\n";
    std::cout << "플랫폼: Windows (크로스 컴파일)\n";
    
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
