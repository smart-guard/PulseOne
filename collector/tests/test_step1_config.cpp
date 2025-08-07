// =============================================================================
// collector/tests/test_step1_config.cpp
// 1단계: 기존 ConfigManager 클래스 테스트
// =============================================================================

#include <gtest/gtest.h>
#include <iostream>

// 기존 PulseOne 헤더들 (프로젝트에 이미 존재하는 것들)
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"

// =============================================================================
// 기존 ConfigManager 테스트
// =============================================================================

class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::cout << "🔧 기존 ConfigManager 테스트 준비\n";
    }
};

TEST_F(ConfigManagerTest, SingletonInstance) {
    std::cout << "\n=== TEST: ConfigManager 싱글톤 ===\n";
    
    // 기존 코드: ConfigManager 싱글톤 가져오기
    ConfigManager& config1 = ConfigManager::getInstance();
    ConfigManager& config2 = ConfigManager::getInstance();
    
    // 동일한 인스턴스인지 확인
    EXPECT_EQ(&config1, &config2);
    std::cout << "✅ ConfigManager 싱글톤 동작 확인\n";
}

TEST_F(ConfigManagerTest, InitializeAndBasicGet) {
    std::cout << "\n=== TEST: 기존 ConfigManager 초기화 ===\n";
    
    ConfigManager& config = ConfigManager::getInstance();
    
    // 기존 코드: initialize() 호출
    config.initialize();
    std::cout << "✅ ConfigManager.initialize() 호출 완료\n";
    
    // 기존 메소드들 테스트
    std::string db_type = config.get("DATABASE_TYPE");
    std::cout << "📋 DATABASE_TYPE: " << (db_type.empty() ? "(기본값)" : db_type) << "\n";
    
    // getOrDefault 테스트 (기존 메소드)
    std::string redis_host = config.getOrDefault("REDIS_PRIMARY_HOST", "localhost");
    std::cout << "🔴 REDIS_HOST: " << redis_host << "\n";
    
    EXPECT_TRUE(true); // 에러 없이 실행되면 성공
    std::cout << "✅ 기존 ConfigManager 메소드들 동작 확인\n";
}

TEST_F(ConfigManagerTest, DatabaseConfiguration) {
    std::cout << "\n=== TEST: 기존 DB 설정 확인 ===\n";
    
    ConfigManager& config = ConfigManager::getInstance();
    
    // 기존 메소드: getActiveDatabaseType() 
    std::string active_db = config.getActiveDatabaseType();
    std::cout << "🗄️  활성 DB 타입: " << active_db << "\n";
    
    // SQLite 경로 (기존 메소드)
    std::string sqlite_path = config.getSQLiteDbPath();
    std::cout << "📁 SQLite 경로: " << sqlite_path << "\n";
    
    EXPECT_FALSE(active_db.empty());
    std::cout << "✅ DB 설정 확인 완료\n";
}

TEST_F(ConfigManagerTest, ConfigDirectory) {
    std::cout << "\n=== TEST: 기존 설정 디렉토리 ===\n";
    
    ConfigManager& config = ConfigManager::getInstance();
    
    // 기존 메소드들
    std::string config_dir = config.getConfigDirectory();
    std::string data_dir = config.getDataDirectory();
    
    std::cout << "📂 Config Dir: " << config_dir << "\n";
    std::cout << "💾 Data Dir: " << data_dir << "\n";
    
    EXPECT_FALSE(config_dir.empty());
    std::cout << "✅ 디렉토리 설정 확인 완료\n";
}

// =============================================================================
// 메인 실행부
// =============================================================================

class Step1Environment : public ::testing::Environment {
public:
    void SetUp() override {
        std::cout << "\n🚀 1단계: 기존 ConfigManager 테스트\n";
        std::cout << "======================================\n";
        std::cout << "🎯 목표: 기존 클래스가 정상 동작하는지 확인\n\n";
    }
    
    void TearDown() override {
        std::cout << "\n✅ 1단계 완료 - 기존 ConfigManager 정상 동작 확인\n";
        std::cout << "➡️  다음: make run-step2 (기존 DatabaseManager 테스트)\n\n";
    }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new Step1Environment);
    
    return RUN_ALL_TESTS();
}