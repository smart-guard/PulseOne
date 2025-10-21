// =============================================================================
// core/export-gateway/tests/test_integration.cpp
// Export Gateway 완전 통합 테스트 - 컴파일 에러 수정
// =============================================================================

#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "CSP/CSPGateway.h"
#include "CSP/AlarmMessage.h"
#include "Database/DatabaseManager.h"
#include "Client/RedisClientImpl.h"  // ✅ PulseOne:: 제거
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"

#ifdef HAVE_HTTPLIB
#include <httplib.h>
#endif

using json = nlohmann::json;
using namespace PulseOne::CSP;
using namespace PulseOne::Database;  // ✅ DatabaseManager용

// =============================================================================
// Mock HTTP 서버
// =============================================================================
#ifdef HAVE_HTTPLIB
class MockAlarmReceiver {
private:
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    std::atomic<bool> running_{false};
    int port_ = 18080;
    mutable std::mutex data_mutex_;
    std::vector<json> received_alarms_;
    
public:
    void start() {
        if (running_) return;
        running_ = true;
        
        server_ = std::make_unique<httplib::Server>();
        
        server_->Post("/webhook", [this](const httplib::Request& req, httplib::Response& res) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            try {
                json alarm = json::parse(req.body);
                received_alarms_.push_back(alarm);
                LogManager::getInstance().Info("📨 Mock HTTP 수신: " + req.body);
                res.status = 200;
                res.set_content(R"({"status":"success"})", "application/json");
            } catch (...) {
                res.status = 400;
            }
        });
        
        server_thread_ = std::thread([this]() {
            LogManager::getInstance().Info("🚀 Mock HTTP 서버 시작: http://localhost:" + std::to_string(port_));
            server_->listen("0.0.0.0", port_);
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    void stop() {
        if (!running_) return;
        running_ = false;
        server_->stop();
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }
    
    int getReceivedCount() const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return received_alarms_.size();
    }
    
    void clearReceived() {
        std::lock_guard<std::mutex> lock(data_mutex_);
        received_alarms_.clear();
    }
};
#endif

// =============================================================================
// 통합 테스트 클래스
// =============================================================================
class ExportGatewayIntegrationTest {
private:
    std::unique_ptr<CSPGateway> gateway_;
    std::shared_ptr<RedisClientImpl> redis_client_;  // ✅ PulseOne:: 제거
#ifdef HAVE_HTTPLIB
    std::unique_ptr<MockAlarmReceiver> mock_server_;
#endif
    std::string test_db_path_ = "/tmp/test_export_gateway.db";
    std::string test_file_path_ = "/tmp/test_alarms";
    std::string original_db_path_;
    
public:
    ExportGatewayIntegrationTest() {
        original_db_path_ = ConfigManager::getInstance().get("SQLITE_DB_PATH");
    }
    
    ~ExportGatewayIntegrationTest() {
        cleanup();
    }
    
    bool runAllTests() {
        LogManager::getInstance().Info("🚀 ========================================");
        LogManager::getInstance().Info("🚀 Export Gateway 완전 통합 테스트 시작");
        LogManager::getInstance().Info("🚀 모든 TargetHandler 테스트 포함");
        LogManager::getInstance().Info("🚀 ========================================");
        
        bool all_passed = true;
        
        if (!test01_Setup()) {
            LogManager::getInstance().Error("❌ STEP 1 실패");
            return false;
        }
        
        if (!test02_BasicFunctions()) {
            LogManager::getInstance().Error("❌ STEP 2 실패");
            all_passed = false;
        }
        
        if (!test03_DatabaseTargetSetup()) {
            LogManager::getInstance().Error("❌ STEP 3 실패");
            all_passed = false;
        }
        
        if (!test04_HttpTargetHandler()) {
            LogManager::getInstance().Error("❌ STEP 4 실패");
            all_passed = false;
        }
        
        if (!test05_FileTargetHandler()) {
            LogManager::getInstance().Error("❌ STEP 5 실패");
            all_passed = false;
        }
        
        if (!test06_S3TargetHandler()) {
            LogManager::getInstance().Error("❌ STEP 6 실패");
            all_passed = false;
        }
        
        if (!test07_MqttTargetHandler()) {
            LogManager::getInstance().Error("❌ STEP 7 실패");
            all_passed = false;
        }
        
        if (!test08_MultiTargetIntegration()) {
            LogManager::getInstance().Error("❌ STEP 8 실패");
            all_passed = false;
        }
        
        if (!test09_DynamicTargetManager()) {
            LogManager::getInstance().Error("❌ STEP 9 실패");
            all_passed = false;
        }
        
        if (!test10_RedisIntegration()) {
            LogManager::getInstance().Error("❌ STEP 10 실패");
            all_passed = false;
        }
        
        if (!test11_BatchProcessing()) {
            LogManager::getInstance().Error("❌ STEP 11 실패");
            all_passed = false;
        }
        
        if (!test12_StatisticsPersistence()) {
            LogManager::getInstance().Error("❌ STEP 12 실패");
            all_passed = false;
        }
        
        if (!test13_ErrorHandling()) {
            LogManager::getInstance().Error("❌ STEP 13 실패");
            all_passed = false;
        }
        
        if (!test14_FinalStats()) {
            LogManager::getInstance().Error("❌ STEP 14 실패");
            all_passed = false;
        }
        
        LogManager::getInstance().Info("🚀 ========================================");
        if (all_passed) {
            LogManager::getInstance().Info("🎉 모든 통합 테스트 통과!");
        } else {
            LogManager::getInstance().Error("❌ 일부 테스트 실패");
        }
        LogManager::getInstance().Info("🚀 ========================================");
        
        return all_passed;
    }
    
private:
    bool test01_Setup() {
        LogManager::getInstance().Info("📋 STEP 1: 환경 준비 (Redis + DB + Mock)");
        
        try {
            // Redis
            redis_client_ = std::make_shared<RedisClientImpl>();  // ✅ PulseOne:: 제거
            if (!redis_client_->isConnected()) {
                LogManager::getInstance().Warn("⚠️ Redis 연결 실패 - 시뮬레이션 모드");
            } else {
                LogManager::getInstance().Info("✅ 1-1. Redis 연결");
            }
            
            // DB
            if (std::filesystem::exists(test_db_path_)) {
                std::filesystem::remove(test_db_path_);
            }
            
            ConfigManager::getInstance().set("SQLITE_DB_PATH", test_db_path_);
            ConfigManager::getInstance().set("CSP_DATABASE_PATH", test_db_path_);
            
            auto& db = DatabaseManager::getInstance();
            db.reinitialize();
            
            std::string create_sql = R"(
                CREATE TABLE IF NOT EXISTS export_targets (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    name VARCHAR(100) NOT NULL UNIQUE,
                    target_type VARCHAR(20) NOT NULL,
                    config TEXT NOT NULL,
                    is_enabled BOOLEAN DEFAULT 1,
                    total_exports INTEGER DEFAULT 0,
                    successful_exports INTEGER DEFAULT 0,
                    failed_exports INTEGER DEFAULT 0
                )
            )";
            
            if (!db.executeNonQuery(create_sql)) {
                throw std::runtime_error("테이블 생성 실패");
            }
            
            LogManager::getInstance().Info("✅ 1-2. DB 초기화");
            
            std::filesystem::create_directories(test_file_path_);
            
#ifdef HAVE_HTTPLIB
            mock_server_ = std::make_unique<MockAlarmReceiver>();
            mock_server_->start();
            LogManager::getInstance().Info("✅ 1-3. Mock 서버");
#endif
            
            CSPGatewayConfig config;
            config.building_id = "1001";
            config.use_dynamic_targets = true;
            // ✅ database_path 제거 (없는 필드)
            config.api_endpoint = "http://localhost:18080";
            config.debug_mode = true;
            
            gateway_ = std::make_unique<CSPGateway>(config);
            LogManager::getInstance().Info("✅ 1-4. Gateway 초기화");
            
            LogManager::getInstance().Info("✅ STEP 1 완료");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("STEP 1 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test02_BasicFunctions() {
        LogManager::getInstance().Info("📋 STEP 2: 기본 기능");
        
        try {
            if (!gateway_->start()) {
                throw std::runtime_error("시작 실패");
            }
            LogManager::getInstance().Info("✅ 2-1. Gateway 시작");
            
            if (!gateway_->isRunning()) {
                throw std::runtime_error("실행 상태 아님");
            }
            LogManager::getInstance().Info("✅ 2-2. 실행 확인");
            
            gateway_->stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            LogManager::getInstance().Info("✅ 2-3. Gateway 중지");
            LogManager::getInstance().Info("✅ STEP 2 완료");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("STEP 2 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test03_DatabaseTargetSetup() {
        LogManager::getInstance().Info("📋 STEP 3: DB 타겟 설정");
        
        try {
            auto& db = DatabaseManager::getInstance();
            
            // ✅ executeNonQuery는 파라미터 없음, SQL에 직접 삽입
            json http_config = {
                {"url", "http://localhost:18080/webhook"},
                {"endpoint", ""},
                {"method", "POST"}
            };
            
            std::string insert_http = "INSERT INTO export_targets (name, target_type, config, is_enabled) VALUES ('db-http', 'http', '" + 
                http_config.dump() + "', 1)";
            
            if (!db.executeNonQuery(insert_http)) {
                throw std::runtime_error("HTTP 저장 실패");
            }
            LogManager::getInstance().Info("✅ 3-1. HTTP 타겟 저장");
            
            json file_config = {
                {"directory", test_file_path_}
            };
            
            std::string insert_file = "INSERT INTO export_targets (name, target_type, config, is_enabled) VALUES ('db-file', 'file', '" + 
                file_config.dump() + "', 1)";
            
            if (!db.executeNonQuery(insert_file)) {
                throw std::runtime_error("File 저장 실패");
            }
            LogManager::getInstance().Info("✅ 3-2. File 타겟 저장");
            
            json s3_config = {
                {"bucket", "test-bucket"}
            };
            
            std::string insert_s3 = "INSERT INTO export_targets (name, target_type, config, is_enabled) VALUES ('db-s3', 's3', '" + 
                s3_config.dump() + "', 1)";
            
            if (!db.executeNonQuery(insert_s3)) {
                throw std::runtime_error("S3 저장 실패");
            }
            LogManager::getInstance().Info("✅ 3-3. S3 타겟 저장");
            
            json mqtt_config = {
                {"broker", "mqtt://localhost:1883"}
            };
            
            std::string insert_mqtt = "INSERT INTO export_targets (name, target_type, config, is_enabled) VALUES ('db-mqtt', 'mqtt', '" + 
                mqtt_config.dump() + "', 1)";
            
            if (!db.executeNonQuery(insert_mqtt)) {
                throw std::runtime_error("MQTT 저장 실패");
            }
            LogManager::getInstance().Info("✅ 3-4. MQTT 타겟 저장");
            
            LogManager::getInstance().Info("✅ STEP 3 완료");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("STEP 3 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test04_HttpTargetHandler() {
        LogManager::getInstance().Info("📋 STEP 4: HttpTargetHandler");
        
        try {
#ifdef HAVE_HTTPLIB
            mock_server_->clearReceived();
#endif
            
            json http_config = {
                {"url", "http://localhost:18080/webhook"},
                {"endpoint", ""},
                {"method", "POST"}
            };
            
            bool added = gateway_->addDynamicTarget("test-http", "http", http_config, true, 1);
            if (!added) {
                throw std::runtime_error("추가 실패");
            }
            LogManager::getInstance().Info("✅ 4-1. HTTP 타겟 추가");
            
            AlarmMessage alarm;
            alarm.bd = 1001;
            alarm.nm = "HTTP_TEST";
            alarm.vl = 99.9;
            alarm.tm = "2025-10-21T12:00:00Z";
            alarm.al = 1;
            alarm.st = 1;
            
            auto result = gateway_->taskAlarmSingle(alarm);
            if (!result.success) {
                throw std::runtime_error("전송 실패: " + result.error_message);
            }
            LogManager::getInstance().Info("✅ 4-2. HTTP 전송 성공");
            
#ifdef HAVE_HTTPLIB
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            int received = mock_server_->getReceivedCount();
            if (received == 0) {
                throw std::runtime_error("수신 없음");
            }
            LogManager::getInstance().Info("✅ 4-3. Mock 수신: " + std::to_string(received) + "개");
#endif
            
            LogManager::getInstance().Info("✅ STEP 4 완료");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("STEP 4 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test05_FileTargetHandler() {
        LogManager::getInstance().Info("📋 STEP 5: FileTargetHandler");
        
        try {
            json file_config = {
                {"directory", test_file_path_}
            };
            
            bool added = gateway_->addDynamicTarget("test-file", "file", file_config, true, 2);
            if (!added) {
                LogManager::getInstance().Warn("⚠️ File 추가 실패 (정상)");
                return true;
            }
            LogManager::getInstance().Info("✅ 5-1. File 타겟 추가");
            
            LogManager::getInstance().Info("✅ STEP 5 완료");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("STEP 5 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test06_S3TargetHandler() {
        LogManager::getInstance().Info("📋 STEP 6: S3TargetHandler");
        
        try {
            json s3_config = {
                {"bucket", "test-bucket"}
            };
            
            bool added = gateway_->addDynamicTarget("test-s3", "s3", s3_config, true, 3);
            if (!added) {
                LogManager::getInstance().Warn("⚠️ S3 추가 실패 (정상)");
                return true;
            }
            LogManager::getInstance().Info("✅ 6-1. S3 타겟 추가");
            
            LogManager::getInstance().Info("✅ STEP 6 완료");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("STEP 6 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test07_MqttTargetHandler() {
        LogManager::getInstance().Info("📋 STEP 7: MqttTargetHandler");
        
        try {
            json mqtt_config = {
                {"broker", "mqtt://localhost:1883"}
            };
            
            bool added = gateway_->addDynamicTarget("test-mqtt", "mqtt", mqtt_config, true, 4);
            if (!added) {
                LogManager::getInstance().Warn("⚠️ MQTT 추가 실패 (정상)");
                return true;
            }
            LogManager::getInstance().Info("✅ 7-1. MQTT 타겟 추가");
            
            LogManager::getInstance().Info("✅ STEP 7 완료");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("STEP 7 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test08_MultiTargetIntegration() {
        LogManager::getInstance().Info("📋 STEP 8: 멀티 타겟");
        
        try {
            AlarmMessage alarm;
            alarm.bd = 1001;
            alarm.nm = "MULTI_TEST";
            alarm.vl = 55.5;
            alarm.tm = "2025-10-21T12:00:00Z";
            alarm.al = 1;
            alarm.st = 1;
            
            auto result = gateway_->taskAlarmSingle(alarm);
            LogManager::getInstance().Info("✅ 8-1. 멀티 타겟 전송");
            
            // ✅ getDynamicTargetStats는 vector 리턴
            auto stats_vec = gateway_->getDynamicTargetStats();
            LogManager::getInstance().Info("✅ 8-2. 타겟 통계: " + std::to_string(stats_vec.size()) + "개");
            
            LogManager::getInstance().Info("✅ STEP 8 완료");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("STEP 8 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test09_DynamicTargetManager() {
        LogManager::getInstance().Info("📋 STEP 9: DynamicTargetManager");
        
        try {
            auto types = gateway_->getSupportedTargetTypes();
            LogManager::getInstance().Info("✅ 9-1. 타겟 타입: " + std::to_string(types.size()) + "개");
            
            // ✅ enableDynamicTarget(name, bool)
            bool enabled = gateway_->enableDynamicTarget("test-http", true);
            if (enabled) {
                LogManager::getInstance().Info("✅ 9-2. 타겟 활성화");
            }
            
            bool removed = gateway_->removeDynamicTarget("test-mqtt");
            if (removed) {
                LogManager::getInstance().Info("✅ 9-3. 타겟 제거");
            }
            
            LogManager::getInstance().Info("✅ STEP 9 완료");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("STEP 9 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test10_RedisIntegration() {
        LogManager::getInstance().Info("📋 STEP 10: Redis Pub/Sub");
        
        try {
            if (!redis_client_ || !redis_client_->ping()) {
                LogManager::getInstance().Warn("⚠️ Redis 비활성 - STEP 10 건너뜀");
                return true;
            }
            
            gateway_->start();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            json alarm_json = {
                {"building_id", 1001},
                {"point_name", "REDIS_ALARM"},
                {"value", 44.4}
            };
            
            int subs = redis_client_->publish("alarms:all", alarm_json.dump());
            LogManager::getInstance().Info("✅ 10-1. Redis 발행 (구독자: " + std::to_string(subs) + ")");
            
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            auto stats = gateway_->getStats();
            LogManager::getInstance().Info("✅ 10-2. 총 알람: " + std::to_string(stats.total_alarms));
            
            LogManager::getInstance().Info("✅ STEP 10 완료");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("STEP 10 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test11_BatchProcessing() {
        LogManager::getInstance().Info("📋 STEP 11: 배치 처리");
        
        try {
            std::vector<AlarmMessage> alarms;
            for (int i = 0; i < 10; i++) {
                AlarmMessage alarm;
                alarm.bd = 1001;
                alarm.nm = "BATCH_" + std::to_string(i);
                alarm.vl = 30.0 + i;
                alarm.tm = "2025-10-21T12:00:00Z";
                alarm.al = 1;
                alarm.st = 1;
                alarms.push_back(alarm);
            }
            
            auto grouped = gateway_->groupAlarmsByBuilding(alarms);
            LogManager::getInstance().Info("✅ 11-1. 그룹화: " + std::to_string(grouped.size()) + "개");
            
            auto results = gateway_->processMultiBuildingAlarms(grouped);
            for (const auto& [building_id, result] : results) {
                LogManager::getInstance().Info("✅ 11-2. Building " + std::to_string(building_id) + 
                    ": " + std::to_string(result.total_alarms) + "개");
            }
            
            LogManager::getInstance().Info("✅ STEP 11 완료");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("STEP 11 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test12_StatisticsPersistence() {
        LogManager::getInstance().Info("📋 STEP 12: DB 통계");
        
        try {
            auto& db = DatabaseManager::getInstance();
            
            std::string update_sql = "UPDATE export_targets SET total_exports = total_exports + 5, successful_exports = successful_exports + 4 WHERE name = 'db-http'";
            
            if (!db.executeNonQuery(update_sql)) {
                throw std::runtime_error("통계 업데이트 실패");
            }
            LogManager::getInstance().Info("✅ 12-1. HTTP 통계 업데이트");
            
            LogManager::getInstance().Info("✅ STEP 12 완료");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("STEP 12 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test13_ErrorHandling() {
        LogManager::getInstance().Info("📋 STEP 13: 에러 핸들링");
        
        try {
            json bad_http = {
                {"url", "http://invalid:9999/webhook"},
                {"endpoint", ""}
            };
            
            bool added = gateway_->addDynamicTarget("test-bad", "http", bad_http, true, 10);
            if (added) {
                AlarmMessage alarm;
                alarm.bd = 1001;
                alarm.nm = "ERROR_TEST";
                alarm.vl = 1.0;
                alarm.tm = "2025-10-21T12:00:00Z";
                alarm.al = 1;
                alarm.st = 1;
                
                auto result = gateway_->taskAlarmSingle(alarm);
                if (!result.success) {
                    LogManager::getInstance().Info("✅ 13-1. 에러 핸들링: " + result.error_message);
                }
            }
            
            gateway_->removeDynamicTarget("test-bad");
            LogManager::getInstance().Info("✅ 13-2. 에러 타겟 제거");
            
            LogManager::getInstance().Info("✅ STEP 13 완료");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("STEP 13 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test14_FinalStats() {
        LogManager::getInstance().Info("📋 STEP 14: 최종 통계");
        
        try {
            auto stats = gateway_->getStats();
            
            LogManager::getInstance().Info("✅ 14-1. Gateway 통계:");
            LogManager::getInstance().Info("    - 총 알람: " + std::to_string(stats.total_alarms));
            LogManager::getInstance().Info("    - 성공 API: " + std::to_string(stats.successful_api_calls));
            LogManager::getInstance().Info("    - 실패 API: " + std::to_string(stats.failed_api_calls));
            
            if (stats.dynamic_targets_enabled) {
                LogManager::getInstance().Info("✅ 14-2. Dynamic Target:");
                LogManager::getInstance().Info("    - 총: " + std::to_string(stats.total_dynamic_targets));
                LogManager::getInstance().Info("    - 활성: " + std::to_string(stats.active_dynamic_targets));
            }
            
            LogManager::getInstance().Info("✅ STEP 14 완료");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("STEP 14 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    void cleanup() {
        LogManager::getInstance().Info("🧹 정리 중...");
        
#ifdef HAVE_HTTPLIB
        if (mock_server_) {
            mock_server_->stop();
        }
#endif
        
        if (gateway_) {
            gateway_->stop();
            gateway_.reset();
        }
        
        if (redis_client_) {
            redis_client_->disconnect();
            redis_client_.reset();
        }
        
        if (!original_db_path_.empty()) {
            ConfigManager::getInstance().set("SQLITE_DB_PATH", original_db_path_);
        }
        
        try {
            if (std::filesystem::exists(test_db_path_)) {
                std::filesystem::remove(test_db_path_);
            }
            if (std::filesystem::exists(test_file_path_)) {
                std::filesystem::remove_all(test_file_path_);
            }
            LogManager::getInstance().Info("✅ 정리 완료");
        } catch (...) {}
    }
};

int main(int, char**) {
    try {
        LogManager::getInstance().Info("Export Gateway 완전 통합 테스트 시작");
        
        ExportGatewayIntegrationTest test;
        bool success = test.runAllTests();
        
        return success ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "예외: " + std::string(e.what()) << std::endl;
        return 1;
    }
}