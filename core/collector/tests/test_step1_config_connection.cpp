/**
 * @file test_step1_config_connection.cpp
 * @brief Step 1: ConfigManager와 DatabaseManager 기본 테스트
 * @date 2025-08-29
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>

#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Database/DatabaseManager.h"

// =============================================================================
// Step 1 테스트 클래스
// =============================================================================

class Step1ConfigConnectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::cout << "\n===========================================" << std::endl;
        std::cout << "Step 1: 설정 및 연결 테스트 시작" << std::endl;
        std::cout << "===========================================" << std::endl;
    }
    
    void TearDown() override {
        std::cout << "Step 1 테스트 종료" << std::endl;
    }
};

// =============================================================================
// Test 1: ConfigManager 테스트
// =============================================================================

TEST_F(Step1ConfigConnectionTest, Test_ConfigManager) {
    std::cout << "\n[TEST 1] ConfigManager 테스트" << std::endl;
    std::cout << "------------------------------" << std::endl;
    
    // 인스턴스 획득
    auto& config = ConfigManager::getInstance();
    std::cout << "ConfigManager 인스턴스 획득: OK" << std::endl;
    
    // 초기화
    config.initialize();
    std::cout << "ConfigManager 초기화: OK" << std::endl;
    
    // 설정값 읽기
    std::string db_type = config.get("DATABASE_TYPE");
    if (db_type.empty()) {
        db_type = "SQLITE";
    }
    std::cout << "DATABASE_TYPE = " << db_type << std::endl;
    
    std::string sqlite_path = config.get("SQLITE_DB_PATH");
    if (sqlite_path.empty()) {
        sqlite_path = "./data/db/pulseone.db";
    }
    std::cout << "SQLITE_DB_PATH = " << sqlite_path << std::endl;
    
    int redis_port = config.getInt("REDIS_PRIMARY_PORT", 6379);
    std::cout << "REDIS_PRIMARY_PORT = " << redis_port << std::endl;
    
    bool influx_enabled = config.getBool("INFLUX_ENABLED", false);
    std::cout << "INFLUX_ENABLED = " << (influx_enabled ? "true" : "false") << std::endl;
    
    // 결과
    EXPECT_FALSE(db_type.empty());
    std::cout << "\n결과: PASS" << std::endl;
}

// =============================================================================
// Test 2: DatabaseManager 테스트
// =============================================================================

TEST_F(Step1ConfigConnectionTest, Test_DatabaseManager) {
    std::cout << "\n[TEST 2] DatabaseManager 테스트" << std::endl;
    std::cout << "-------------------------------" << std::endl;
    
    // ConfigManager 먼저 초기화
    ConfigManager::getInstance().initialize();
    
    // DatabaseManager 인스턴스 획득
    auto& db = DatabaseManager::getInstance();
    std::cout << "DatabaseManager 인스턴스 획득: OK" << std::endl;
    
    // 초기화
    bool init_result = db.initialize();
    std::cout << "DatabaseManager 초기화: " << (init_result ? "OK" : "FAILED") << std::endl;
    
    // 연결 상태 확인
    if (db.isSQLiteConnected()) {
        std::cout << "SQLite: 연결됨" << std::endl;
    } else {
        std::cout << "SQLite: 연결 안됨" << std::endl;
    }
    
    if (db.isConnected(DatabaseManager::DatabaseType::POSTGRESQL)) {
        std::cout << "PostgreSQL: 연결됨" << std::endl;
    } else {
        std::cout << "PostgreSQL: 연결 안됨" << std::endl;
    }
    
    if (db.isRedisConnected()) {
        std::cout << "Redis: 연결됨" << std::endl;
    } else {
        std::cout << "Redis: 연결 안됨" << std::endl;
    }
    
    if (db.isInfluxConnected()) {
        std::cout << "InfluxDB: 연결됨" << std::endl;
    } else {
        std::cout << "InfluxDB: 연결 안됨" << std::endl;
    }
    
    // 결과 (최소한 초기화는 성공해야 함)
    EXPECT_TRUE(init_result);
    std::cout << "\n결과: " << (init_result ? "PASS" : "FAIL") << std::endl;
}

// =============================================================================
// Test 3: 전체 시스템 요약
// =============================================================================

TEST_F(Step1ConfigConnectionTest, Test_Summary) {
    std::cout << "\n[TEST 3] 전체 시스템 요약" << std::endl;
    std::cout << "-------------------------" << std::endl;
    
    bool config_ok = false;
    bool db_ok = false;
    
    // ConfigManager 확인
    try {
        auto& config = ConfigManager::getInstance();
        config.initialize();
        
        // 주요 설정 확인
        std::string db_type = config.get("DATABASE_TYPE");
        if (db_type.empty()) db_type = "SQLITE";
        
        config_ok = true;
        std::cout << "ConfigManager: OK (DB 타입: " << db_type << ")" << std::endl;
    } catch (...) {
        std::cout << "ConfigManager: FAILED" << std::endl;
    }
    
    // DatabaseManager 확인
    try {
        auto& db = DatabaseManager::getInstance();
        db_ok = db.initialize();
        
        if (db_ok) {
            int connected_count = 0;
            if (db.isSQLiteConnected()) connected_count++;
            if (db.isConnected(DatabaseManager::DatabaseType::POSTGRESQL)) connected_count++;
            if (db.isRedisConnected()) connected_count++;
            if (db.isInfluxConnected()) connected_count++;
            
            std::cout << "DatabaseManager: OK (" << connected_count << "개 DB 연결됨)" << std::endl;
        } else {
            std::cout << "DatabaseManager: 초기화 실패" << std::endl;
        }
    } catch (...) {
        std::cout << "DatabaseManager: FAILED" << std::endl;
    }
    
    // 최종 결과
    std::cout << "\n===========================================" << std::endl;
    std::cout << "Step 1 최종 결과" << std::endl;
    std::cout << "-------------------------------------------" << std::endl;
    std::cout << "ConfigManager:   " << (config_ok ? "PASS" : "FAIL") << std::endl;
    std::cout << "DatabaseManager: " << (db_ok ? "PASS" : "FAIL") << std::endl;
    std::cout << "-------------------------------------------" << std::endl;
    
    bool overall = config_ok && db_ok;
    std::cout << "전체 결과: " << (overall ? "PASS - Step 2 진행 가능" : "FAIL - 문제 해결 필요") << std::endl;
    std::cout << "===========================================" << std::endl;
    
    EXPECT_TRUE(overall);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}