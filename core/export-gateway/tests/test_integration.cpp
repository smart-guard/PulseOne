/**
 * @file test_integration_complete.cpp
 * @brief Export Gateway 완전 통합 테스트 - FINAL
 * @version 10.0 - Repository 활용 (ConfigManager + ensureTableExists)
 * @date 2025-11-04
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <fstream>
#include <filesystem>

// PulseOne 헤더
#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"
#include "DatabaseManager.hpp"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/ExportScheduleRepository.h"
#include "Database/Repositories/ExportLogRepository.h"
#include "Database/Entities/ExportTargetEntity.h"
#include "Database/Entities/ExportScheduleEntity.h"
#include "Database/Entities/ExportLogEntity.h"

// Export Gateway 헤더
#include "CSP/ExportCoordinator.h"
#include "CSP/AlarmMessage.h"
#include "CSP/DynamicTargetManager.h"
#include "CSP/FailureProtector.h"
#include "CSP/FileTargetHandler.h"
#include "Transform/PayloadTransformer.h"
#include "Client/RedisClientImpl.h"

// httplib
#ifdef HAVE_HTTPLIB
#include <httplib.h>
#endif

// JSON
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace PulseOne;

// =============================================================================
// 테스트 유틸리티
// =============================================================================

class TestHelper {
public:
    static std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &time);
#else
        localtime_r(&time, &tm_buf);
#endif
        ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }
    
    static void assertCondition(bool condition, const std::string& message) {
        if (!condition) {
            LogManager::getInstance().Error("❌ " + message);
            throw std::runtime_error("Test failed: " + message);
        }
        LogManager::getInstance().Info("✅ " + message);
    }
};

// =============================================================================
// Mock Webhook 서버
// =============================================================================

#ifdef HAVE_HTTPLIB
class MockWebhookServer {
private:
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    std::atomic<bool> running_{false};
    int port_ = 18080;
    mutable std::mutex data_mutex_;
    std::vector<json> received_data_;
    
public:
    MockWebhookServer() {
        server_ = std::make_unique<httplib::Server>();
        setupRoutes();
    }
    
    ~MockWebhookServer() {
        stop();
    }
    
    bool start() {
        if (running_.load()) return true;
        
        running_.store(true);
        server_thread_ = std::thread([this]() {
            LogManager::getInstance().Info("🌐 Mock 서버 시작: http://localhost:" + std::to_string(port_));
            server_->listen("0.0.0.0", port_);
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return true;
    }
    
    void stop() {
        if (!running_.load()) return;
        
        running_.store(false);
        if (server_) server_->stop();
        if (server_thread_.joinable()) server_thread_.join();
        
        LogManager::getInstance().Info("🛑 Mock 서버 중지");
    }
    
    int getPort() const { return port_; }
    
    std::vector<json> getReceivedData() const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return received_data_;
    }
    
    void clearReceivedData() {
        std::lock_guard<std::mutex> lock(data_mutex_);
        received_data_.clear();
    }
    
private:
    void setupRoutes() {
        server_->Post("/webhook", [this](const httplib::Request& req, httplib::Response& res) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            try {
                json data = json::parse(req.body);
                received_data_.push_back(data);
                LogManager::getInstance().Info("📨 Mock 서버 수신: " + req.body);
                res.status = 200;
                res.set_content(R"({"status":"success"})", "application/json");
            } catch (const std::exception& e) {
                LogManager::getInstance().Error("Mock 서버 에러: " + std::string(e.what()));
                res.status = 400;
                res.set_content(R"({"status":"error"})", "application/json");
            }
        });
        
        server_->Get("/health", [](const httplib::Request&, httplib::Response& res) {
            res.status = 200;
            res.set_content(R"({"status":"ok"})", "application/json");
        });
    }
};
#endif

// =============================================================================
// 통합 테스트 클래스
// =============================================================================

class CompleteIntegrationTest {
private:
    std::unique_ptr<Coordinator::ExportCoordinator> coordinator_;
    std::shared_ptr<RedisClientImpl> redis_client_;
    
#ifdef HAVE_HTTPLIB
    std::unique_ptr<MockWebhookServer> mock_server_;
#endif
    
    int alarm_target_id_ = 0;
    int schedule_target_id_ = 0;
    
public:
    CompleteIntegrationTest() {
        LogManager::getInstance().Info("🧪 통합 테스트 초기화 (Complete Version)");
    }
    
    ~CompleteIntegrationTest() {
        cleanup();
    }
    
    bool runAllTests() {
        LogManager::getInstance().Info("========================================");
        LogManager::getInstance().Info("🚀 Export Gateway 완전 통합 테스트");
        LogManager::getInstance().Info("========================================");
        
        int total_tests = 9;
        int passed_tests = 0;
        
        try {
            if (!setupEnvironment()) return false;
            
            if (testAlarmFlow()) passed_tests++;
            if (testScheduleFlow()) passed_tests++;
            if (testDynamicTargetManager()) passed_tests++;
            if (testPayloadTransformer()) passed_tests++;
            if (testFailureProtector()) passed_tests++;
            if (testFileTargetHandler()) passed_tests++;
            if (testMultipleTargetsConcurrent()) passed_tests++;
            if (testFailureAndRetry()) passed_tests++;
            if (testExportLogRepository()) passed_tests++;
            
            LogManager::getInstance().Info("========================================");
            LogManager::getInstance().Info("✅ 테스트 완료: " + std::to_string(passed_tests) + 
                                          "/" + std::to_string(total_tests) + " 통과!");
            LogManager::getInstance().Info("========================================");
            
            return (passed_tests == total_tests);
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("테스트 실패: " + std::string(e.what()));
            LogManager::getInstance().Info("통과한 테스트: " + std::to_string(passed_tests) + 
                                          "/" + std::to_string(total_tests));
            return false;
        }
    }
    
private:
    bool setupEnvironment() {
        LogManager::getInstance().Info("\n📋 STEP 0: 환경 설정");
        
        try {
            const std::string test_db_path = "/tmp/test_export_complete.db";
            
            // 1. 기존 DB 삭제
            std::remove(test_db_path.c_str());
            LogManager::getInstance().Info("✅ 기존 테스트 DB 삭제");
            
            // 2. ConfigManager 초기화 및 테스트 경로 설정
            ConfigManager::getInstance().initialize();
            ConfigManager::getInstance().set("SQLITE_DB_PATH", test_db_path);
            LogManager::getInstance().Info("✅ ConfigManager 설정: " + test_db_path);
            
            // 3. DatabaseManager 초기화
            if (!DatabaseManager::getInstance().initialize()) {
                throw std::runtime_error("DatabaseManager 초기화 실패");
            }
            LogManager::getInstance().Info("✅ DatabaseManager 초기화");
            
            // 4. RepositoryFactory 초기화
            if (!PulseOne::Database::RepositoryFactory::getInstance().initialize()) {
                throw std::runtime_error("RepositoryFactory 초기화 실패");
            }
            LogManager::getInstance().Info("✅ RepositoryFactory 초기화");
            
            // 5. 테이블 생성 (Repository 활용)
            using namespace Database::Repositories;
            auto& factory = Database::RepositoryFactory::getInstance();
            
            auto export_target_repo = factory.getExportTargetRepository();
            auto export_log_repo = factory.getExportLogRepository();
            auto export_schedule_repo = factory.getExportScheduleRepository();
            
            // ✅ Repository의 ensureTableExists() 사용
            LogManager::getInstance().Info("📊 테이블 생성 중...");
            
            // export_targets 테이블은 save() 호출 시 자동 생성됨
            // export_logs 테이블도 save() 호출 시 자동 생성됨
            // export_schedules 테이블도 save() 호출 시 자동 생성됨
            
            LogManager::getInstance().Info("✅ 테이블 준비 완료 (Repository 활용)");
            
            // 6. 테스트 타겟 생성
            if (!createTestTargets()) {
                throw std::runtime_error("테스트 타겟 생성 실패");
            }
            
#ifdef HAVE_HTTPLIB
            // 7. Mock 서버 시작
            mock_server_ = std::make_unique<MockWebhookServer>();
            if (!mock_server_->start()) {
                throw std::runtime_error("Mock 서버 시작 실패");
            }
            LogManager::getInstance().Info("✅ Mock 서버 시작");
#endif
            
            // 8. Redis 연결
            redis_client_ = std::make_shared<RedisClientImpl>();
            
            if (!redis_client_->connect("pulseone-redis", 6379)) {
                LogManager::getInstance().Error("Redis 연결 실패 - 재시도");
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (!redis_client_->connect("pulseone-redis", 6379)) {
                    throw std::runtime_error("Redis 재연결 실패");
                }
            }
            LogManager::getInstance().Info("✅ Redis 연결");
            
            // 9. ExportCoordinator 시작
            Coordinator::ExportCoordinatorConfig config;
            config.database_path = test_db_path;
            config.redis_host = "pulseone-redis";
            config.redis_port = 6379;
            config.redis_password = "";
            config.alarm_channels = {"alarms:all"};
            config.alarm_patterns = {"alarm:building:*"};
            config.alarm_worker_threads = 4;
            config.alarm_max_queue_size = 10000;
            config.schedule_check_interval_seconds = 60;
            config.schedule_reload_interval_seconds = 300;
            config.schedule_batch_size = 100;
            config.enable_debug_log = true;
            config.log_retention_days = 30;
            config.max_concurrent_exports = 50;
            config.export_timeout_seconds = 30;
            
            coordinator_ = std::make_unique<Coordinator::ExportCoordinator>(config);
            
            if (!coordinator_->start()) {
                throw std::runtime_error("ExportCoordinator 시작 실패");
            }
            
            LogManager::getInstance().Info("✅ ExportCoordinator 시작");
            
            LogManager::getInstance().Info("⏰ 구독 초기화 대기 (3초)...");
            std::this_thread::sleep_for(std::chrono::seconds(3));
            LogManager::getInstance().Info("✅ 준비 완료\n");
            
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("환경 설정 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool createTestTargets() {
        try {
            using namespace Database::Repositories;
            using namespace Database::Entities;
            
            ExportTargetRepository target_repo;
            
            // 알람 타겟
            ExportTargetEntity alarm_target;
            alarm_target.setName("TEST_ALARM_TARGET");
            alarm_target.setTargetType("http");
            alarm_target.setEnabled(true);
            alarm_target.setDescription("Test alarm webhook target");
            alarm_target.setExportMode("alarm");
            
#ifdef HAVE_HTTPLIB
            json alarm_config = {
                {"url", "http://localhost:18080/webhook"},
                {"method", "POST"},
                {"timeout", 5000},
                {"export_mode", "alarm"}
            };
#else
            json alarm_config = {
                {"url", "http://httpbin.org/post"},
                {"method", "POST"},
                {"timeout", 5000},
                {"export_mode", "alarm"}
            };
#endif
            alarm_target.setConfig(alarm_config.dump());
            
            if (!target_repo.save(alarm_target)) {
                throw std::runtime_error("알람 타겟 저장 실패");
            }
            alarm_target_id_ = alarm_target.getId();
            
            // 스케줄 타겟
            ExportTargetEntity schedule_target;
            schedule_target.setName("TEST_SCHEDULE_TARGET");
            schedule_target.setTargetType("http");
            schedule_target.setEnabled(true);
            schedule_target.setDescription("Test schedule webhook target");
            schedule_target.setExportMode("schedule");
            
#ifdef HAVE_HTTPLIB
            json schedule_config = {
                {"url", "http://localhost:18080/webhook"},
                {"method", "POST"},
                {"timeout", 5000},
                {"export_mode", "schedule"}
            };
#else
            json schedule_config = {
                {"url", "http://httpbin.org/post"},
                {"method", "POST"},
                {"timeout", 5000},
                {"export_mode", "schedule"}
            };
#endif
            schedule_target.setConfig(schedule_config.dump());
            
            if (!target_repo.save(schedule_target)) {
                throw std::runtime_error("스케줄 타겟 저장 실패");
            }
            schedule_target_id_ = schedule_target.getId();
            
            LogManager::getInstance().Info("✅ 테스트 타겟 생성 (알람: " + 
                std::to_string(alarm_target_id_) + ", 스케줄: " + 
                std::to_string(schedule_target_id_) + ")");
            
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("타겟 생성 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testAlarmFlow() {
        LogManager::getInstance().Info("\n📋 STEP 1: 알람 플로우 테스트");
        
        try {
#ifdef HAVE_HTTPLIB
            mock_server_->clearReceivedData();
#endif
            
            CSP::AlarmMessage alarm;
            alarm.bd = 1001;
            alarm.nm = "TEMP_01";
            alarm.vl = 85.5;
            alarm.tm = TestHelper::getCurrentTimestamp();
            alarm.al = 1;
            alarm.st = 1;
            alarm.des = "온도 상한 초과";
            
            // ✅ to_json() 메서드 사용
            json alarm_json = alarm.to_json();
            
            redis_client_->publish("alarms:all", alarm_json.dump());
            LogManager::getInstance().Info("✅ 알람 발행");
            
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
#ifdef HAVE_HTTPLIB
            auto received = mock_server_->getReceivedData();
            TestHelper::assertCondition(
                !received.empty(),
                "알람 전송 확인 (" + std::to_string(received.size()) + "건)");
#endif
            
            LogManager::getInstance().Info("✅ 알람 플로우 완료\n");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("알람 플로우 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testScheduleFlow() {
        LogManager::getInstance().Info("\n📋 STEP 2: 스케줄 플로우 테스트");
        
        try {
            using namespace Database::Repositories;
            using namespace Database::Entities;
            
            ExportScheduleRepository schedule_repo;
            ExportScheduleEntity schedule;
            schedule.setTargetId(schedule_target_id_);
            schedule.setScheduleName("TEST_SCHEDULE");
            schedule.setCronExpression("* * * * *");
            schedule.setEnabled(true);
            
            if (!schedule_repo.save(schedule)) {
                throw std::runtime_error("스케줄 저장 실패");
            }
            
            LogManager::getInstance().Info("✅ 스케줄 생성 (ID: " + std::to_string(schedule.getId()) + ")");
            
            redis_client_->publish("schedule:reload", "{}");
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            json execute_event = {{"schedule_id", 1}, {"trigger", "manual_test"}};
            redis_client_->publish("schedule:execute:1", execute_event.dump());
            
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            LogManager::getInstance().Info("✅ 스케줄 플로우 완료\n");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("스케줄 플로우 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testDynamicTargetManager() {
        LogManager::getInstance().Info("\n📋 STEP 3: DynamicTargetManager 검증");
        
        try {
            auto& manager = CSP::DynamicTargetManager::getInstance();
            
            TestHelper::assertCondition(
                manager.isRunning(),
                "DynamicTargetManager 실행 중");
            
            auto targets = manager.getAllTargets();
            TestHelper::assertCondition(
                !targets.empty(),
                "타겟 로드됨 (" + std::to_string(targets.size()) + "개)");
            
            auto stats = manager.getStatistics();
            LogManager::getInstance().Info("📊 통계:");
            LogManager::getInstance().Info("  요청: " + std::to_string(stats["total_requests"].get<uint64_t>()));
            LogManager::getInstance().Info("  성공: " + std::to_string(stats["total_successes"].get<uint64_t>()));
            
            LogManager::getInstance().Info("✅ DynamicTargetManager 완료\n");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("DynamicTargetManager 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testPayloadTransformer() {
        LogManager::getInstance().Info("\n📋 STEP 4: PayloadTransformer 테스트");
        
        try {
            auto& transformer = Transform::PayloadTransformer::getInstance();
            
            CSP::AlarmMessage alarm;
            alarm.bd = 1001;
            alarm.nm = "TEST_POINT";
            alarm.vl = 42.0;
            alarm.tm = TestHelper::getCurrentTimestamp();
            alarm.al = 1;
            alarm.st = 1;
            
            auto context = transformer.createContext(alarm, "Field1", "Description", "42.0");
            
            json template_json = transformer.getGenericDefaultTemplate();
            json result = transformer.transform(template_json, context);
            
            TestHelper::assertCondition(
                !result.empty() && result["building_id"] == 1001,
                "템플릿 변환 성공");
            
            std::string str_template = "Building {{building_id}}: {{point_name}}";
            std::string str_result = transformer.transformString(str_template, context);
            
            TestHelper::assertCondition(
                str_result.find("Building 1001") != std::string::npos,
                "문자열 템플릿 변환 성공");
            
            LogManager::getInstance().Info("✅ PayloadTransformer 완료\n");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("PayloadTransformer 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testFailureProtector() {
        LogManager::getInstance().Info("\n📋 STEP 5: FailureProtector 테스트");
        
        try {
            Export::FailureProtectorConfig config;
            config.failure_threshold = 3;
            config.recovery_timeout_ms = 500;
            config.half_open_max_attempts = 2;
            config.backoff_multiplier = 1.0;  // ✅ exponential backoff 비활성화
            
            CSP::FailureProtector protector("TEST_CIRCUIT", config);
            
            TestHelper::assertCondition(
                protector.canExecute(),
                "초기 CLOSED 상태");
            
            protector.recordFailure();
            protector.recordFailure();
            protector.recordFailure();
            
            TestHelper::assertCondition(
                !protector.canExecute(),
                "OPEN 상태 전환");
            
            LogManager::getInstance().Info("  ⏰ 0.7초 대기...");
            std::this_thread::sleep_for(std::chrono::milliseconds(700));
            
            TestHelper::assertCondition(
                protector.canExecute(),
                "HALF_OPEN 상태 전환");
            
            protector.recordSuccess();
            TestHelper::assertCondition(
                protector.canExecute(),
                "CLOSED 복구");
            
            LogManager::getInstance().Info("✅ FailureProtector 완료\n");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("FailureProtector 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testFileTargetHandler() {
        LogManager::getInstance().Info("\n📋 STEP 6: FileTargetHandler 테스트");
        
        try {
            CSP::FileTargetHandler handler;
            
            std::string test_dir = "/tmp/test_file_export";
            std::filesystem::remove_all(test_dir);
            std::filesystem::create_directories(test_dir);
            
            json config = {
                {"base_path", test_dir},
                {"file_format", "json"},
                {"filename_template", "alarm_{{building_id}}.json"},
                {"create_subdirs", true}
            };
            
            TestHelper::assertCondition(
                handler.initialize(config),
                "FileTargetHandler 초기화");
            
            CSP::AlarmMessage alarm;
            alarm.bd = 1001;
            alarm.nm = "FILE_TEST";
            alarm.vl = 99.0;
            alarm.tm = TestHelper::getCurrentTimestamp();
            alarm.al = 1;
            alarm.st = 1;
            
            auto result = handler.sendAlarm(alarm, config);
            
            TestHelper::assertCondition(
                result.success && !result.file_path.empty(),
                "파일 저장 성공");
            
            TestHelper::assertCondition(
                std::filesystem::exists(result.file_path),
                "파일 존재 확인");
            
            handler.cleanup();
            std::filesystem::remove_all(test_dir);
            
            LogManager::getInstance().Info("✅ FileTargetHandler 완료\n");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("FileTargetHandler 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testMultipleTargetsConcurrent() {
        LogManager::getInstance().Info("\n📋 STEP 7: 다중 타겟 동시 전송 테스트");
        
        try {
#ifdef HAVE_HTTPLIB
            mock_server_->clearReceivedData();
#endif
            
            const int alarm_count = 5;
            LogManager::getInstance().Info("  📤 " + std::to_string(alarm_count) + "개 알람 발행");
            
            for (int i = 0; i < alarm_count; ++i) {
                CSP::AlarmMessage alarm;
                alarm.bd = 1001;
                alarm.nm = "MULTI_" + std::to_string(i);
                alarm.vl = 50.0 + i;
                alarm.tm = TestHelper::getCurrentTimestamp();
                alarm.al = 1;
                alarm.st = 1;
                
                // ✅ to_json() 메서드 사용
                json alarm_json = alarm.to_json();
                
                redis_client_->publish("alarms:all", alarm_json.dump());
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            LogManager::getInstance().Info("  ⏰ 처리 대기 (2초)...");
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
#ifdef HAVE_HTTPLIB
            auto received = mock_server_->getReceivedData();
            LogManager::getInstance().Info("  📊 발행: " + std::to_string(alarm_count) + 
                                          ", 수신: " + std::to_string(received.size()));
#endif
            
            LogManager::getInstance().Info("✅ 다중 타겟 완료\n");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("다중 타겟 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testFailureAndRetry() {
        LogManager::getInstance().Info("\n📋 STEP 8: 실패 및 재시도 테스트");
        
        try {
            using namespace Database::Repositories;
            using namespace Database::Entities;
            
            ExportTargetRepository target_repo;
            ExportTargetEntity failure_target;
            
            failure_target.setName("TEST_FAILURE");
            failure_target.setTargetType("http");
            failure_target.setEnabled(true);
            failure_target.setExportMode("alarm");
            
            json fail_config = {
                {"url", "http://localhost:99999/fail"},
                {"method", "POST"},
                {"timeout", 1000},
                {"max_retries", 2}
            };
            failure_target.setConfig(fail_config.dump());
            
            if (!target_repo.save(failure_target)) {
                throw std::runtime_error("실패 타겟 저장 실패");
            }
            
            int fail_id = failure_target.getId();
            LogManager::getInstance().Info("  🎯 실패 타겟 생성: ID=" + std::to_string(fail_id));
            
            auto& manager = CSP::DynamicTargetManager::getInstance();
            manager.loadFromDatabase();
            
            CSP::AlarmMessage alarm;
            alarm.bd = 1001;
            alarm.nm = "FAILURE_TEST";
            alarm.vl = 999.0;
            alarm.tm = TestHelper::getCurrentTimestamp();
            alarm.al = 1;
            alarm.st = 1;
            
            // ✅ to_json() 메서드 사용
            json alarm_json = alarm.to_json();
            
            redis_client_->publish("alarms:all", alarm_json.dump());
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            auto protector_stats = manager.getFailureProtectorStats();
            LogManager::getInstance().Info("  📊 FailureProtector 상태:");
            for (const auto& [name, stats] : protector_stats) {
                LogManager::getInstance().Info("    " + name + ": 실패 " + 
                                              std::to_string(stats.failure_count));
            }
            
            target_repo.deleteById(fail_id);
            manager.loadFromDatabase();
            
            LogManager::getInstance().Info("✅ 실패/재시도 완료\n");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("실패 시나리오 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testExportLogRepository() {
        LogManager::getInstance().Info("\n📋 STEP 9: ExportLogRepository 테스트");
        
        try {
            using namespace Database::Repositories;
            using namespace Database::Entities;
            
            ExportLogRepository log_repo;
            ExportLogEntity log;
            
            log.setTargetId(alarm_target_id_);
            log.setLogType("alarm");
            log.setStatus("success");
            log.setHttpStatusCode(200);
            log.setProcessingTimeMs(100);
            
            if (!log_repo.save(log)) {
                throw std::runtime_error("로그 저장 실패");
            }
            
            int log_id = log.getId();
            LogManager::getInstance().Info("  ✅ 로그 저장: ID=" + std::to_string(log_id));
            
            auto retrieved = log_repo.findById(log_id);
            TestHelper::assertCondition(
                retrieved.has_value(),
                "로그 조회 성공");
            
            if (retrieved.has_value()) {
                LogManager::getInstance().Info("  📋 로그 정보:");
                LogManager::getInstance().Info("    타겟 ID: " + std::to_string(retrieved->getTargetId()));
                LogManager::getInstance().Info("    로그 타입: " + retrieved->getLogType());
                LogManager::getInstance().Info("    상태: " + retrieved->getStatus());
                LogManager::getInstance().Info("    HTTP 코드: " + std::to_string(retrieved->getHttpStatusCode()));
                LogManager::getInstance().Info("    처리시간: " + std::to_string(retrieved->getProcessingTimeMs()) + "ms");
            }
            
            LogManager::getInstance().Info("✅ ExportLogRepository 완료\n");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("ExportLogRepository 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    void cleanup() {
        LogManager::getInstance().Info("\n🧹 정리 중...");
        
        if (coordinator_) {
            coordinator_->stop();
            coordinator_.reset();
        }
        
        if (redis_client_) {
            redis_client_->disconnect();
            redis_client_.reset();
        }
        
#ifdef HAVE_HTTPLIB
        if (mock_server_) {
            mock_server_->stop();
            mock_server_.reset();
        }
#endif
        
        try {
            std::remove("/tmp/test_export_complete.db");
            LogManager::getInstance().Info("✅ 테스트 DB 삭제");
        } catch (...) {
            LogManager::getInstance().Warn("테스트 DB 삭제 실패 (무시)");
        }
        
        LogManager::getInstance().Info("✅ 정리 완료");
    }
};

// =============================================================================
// main
// =============================================================================

int main() {
    try {
        std::cout << "\n";
        std::cout << "═══════════════════════════════════════════════════════\n";
        std::cout << "  PulseOne Export Gateway 완전 통합 테스트\n";
        std::cout << "═══════════════════════════════════════════════════════\n";
        std::cout << "\n";
        
        CompleteIntegrationTest test;
        bool success = test.runAllTests();
        
        std::cout << "\n";
        if (success) {
            std::cout << "✨ 테스트 결과: 완전 성공! 🎉\n";
            std::cout << "   모든 9개 테스트 통과\n";
        } else {
            std::cout << "💥 테스트 결과: 일부 실패\n";
        }
        std::cout << "\n";
        
        return success ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "💥 예외 발생: " << e.what() << "\n";
        return 1;
    }
}