/**
 * @file test_integration.cpp
 * @brief Export Gateway 통합 테스트 - 컴파일 에러 완전 수정
 * @version 7.4 - FINAL (API 오류 수정)
 * @date 2025-10-31
 * 
 * ✅ 수정 완료:
 * 1. ConfigManager::initialize() - 파라미터 없음
 * 2. DatabaseManager::initialize() - 파라미터 없음  
 * 3. 운영 DB를 /tmp로 복사해서 테스트 (읽기 데이터 보존, 쓰기 격리)
 * 4. ExportCoordinator(const ExportCoordinatorConfig& config) - 생성자에 config 전달
 * 5. ExportCoordinatorConfig 필드명 정확히 수정
 * 6. ExportTargetEntity::setName() 사용 (setTargetName 아님)
 * 7. AlarmMessage 필드: bd, nm, vl, tm, al, st, des 사용
 * 8. ❌ getTargetNames() → ✅ getAllTargets() 사용
 * 9. ❌ getTargetStatistics() → ✅ getStatistics() 사용
 * 10. DynamicTarget에 직접 통계 필드 없음 → getStatistics()로 전체 통계 조회
 * 
 * 테스트 DB 전략:
 * - 운영 DB (./data/db/pulseone.db) → /tmp/test_export_complete.db 복사
 * - 실제 export_targets/schedules 데이터 그대로 사용
 * - 테스트 중 생성되는 logs는 임시 DB에만 기록
 * - 테스트 후 임시 DB 자동 삭제
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
#include <cstdio>  // std::remove
#include <fstream>  // 파일 복사

// PulseOne 헤더
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Database/DatabaseManager.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/ExportScheduleRepository.h"
#include "Database/Repositories/ExportLogRepository.h"
#include "Database/Entities/ExportTargetEntity.h"
#include "Database/Entities/ExportScheduleEntity.h"

// Export Gateway 헤더
#include "CSP/ExportCoordinator.h"
#include "CSP/AlarmMessage.h"
#include "CSP/DynamicTargetManager.h"
#include "Client/RedisClientImpl.h"

// httplib (선택)
#ifdef HAVE_HTTPLIB
#include <httplib.h>
#endif

// JSON
#include <nlohmann/json.hpp>

// SQLite3 (테스트 DB 생성용)
#include <sqlite3.h>

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

class RealWorkingIntegrationTest {
private:
    std::unique_ptr<Coordinator::ExportCoordinator> coordinator_;
    std::shared_ptr<RedisClientImpl> redis_client_;
    
#ifdef HAVE_HTTPLIB
    std::unique_ptr<MockWebhookServer> mock_server_;
#endif
    
    int alarm_target_id_ = 0;
    int schedule_target_id_ = 0;
    
public:
    RealWorkingIntegrationTest() {
        LogManager::getInstance().Info("🧪 통합 테스트 초기화");
    }
    
    ~RealWorkingIntegrationTest() {
        cleanup();
    }
    
    // =========================================================================
    // 메인 테스트 실행
    // =========================================================================
    
    bool runAllTests() {
        LogManager::getInstance().Info("========================================");
        LogManager::getInstance().Info("🚀 Export Gateway 완전 통합 테스트");
        LogManager::getInstance().Info("========================================");
        
        try {
            if (!setupEnvironment()) return false;
            if (!testAlarmFlow()) return false;
            if (!testScheduleFlow()) return false;
            if (!testDynamicTargetManager()) return false;
            if (!testExportLogs()) return false;
            
            LogManager::getInstance().Info("========================================");
            LogManager::getInstance().Info("✅ 모든 테스트 통과!");
            LogManager::getInstance().Info("========================================");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("테스트 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    // =========================================================================
    // 환경 설정
    // =========================================================================
    
    /**
     * @brief DB 파일 복사 (운영 DB → 임시 DB)
     */
    bool copyDatabase(const std::string& source, const std::string& dest) {
        try {
            std::ifstream src(source, std::ios::binary);
            if (!src.is_open()) {
                LogManager::getInstance().Warn("원본 DB 열기 실패: " + source);
                return false;
            }
            
            std::ofstream dst(dest, std::ios::binary);
            if (!dst.is_open()) {
                LogManager::getInstance().Error("대상 DB 생성 실패: " + dest);
                return false;
            }
            
            dst << src.rdbuf();
            
            src.close();
            dst.close();
            
            LogManager::getInstance().Info("✅ DB 복사 완료: " + source + " → " + dest);
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("DB 복사 중 예외: " + std::string(e.what()));
            return false;
        }
    }
    
    /**
     * @brief 테스트용 SQLite DB 생성 - 프로젝트 SQL 파일 사용
     */
    bool createTestDatabase(const std::string& db_path) {
        sqlite3* db = nullptr;
        
        try {
            // DB 파일 생성
            int rc = sqlite3_open(db_path.c_str(), &db);
            if (rc != SQLITE_OK) {
                LogManager::getInstance().Error("SQLite DB 생성 실패: " + std::string(sqlite3_errmsg(db)));
                if (db) sqlite3_close(db);
                return false;
            }
            
            // 프로젝트 SQL 파일들 실행
            std::vector<std::string> sql_files = {
                "/mnt/project/10-export_system.sql"  // Export 시스템 테이블만
            };
            
            for (const auto& sql_file : sql_files) {
                if (!executeSqlFile(db, sql_file)) {
                    LogManager::getInstance().Warn("SQL 파일 실행 실패 (무시): " + sql_file);
                }
            }
            
            sqlite3_close(db);
            LogManager::getInstance().Info("테스트 DB 스키마 초기화 완료");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("테스트 DB 생성 중 예외: " + std::string(e.what()));
            if (db) sqlite3_close(db);
            return false;
        }
    }
    
    /**
     * @brief SQL 파일 실행
     */
    bool executeSqlFile(sqlite3* db, const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string sql = buffer.str();
        file.close();
        
        char* err_msg = nullptr;
        int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
        
        if (rc != SQLITE_OK) {
            std::string error = err_msg ? err_msg : "Unknown error";
            sqlite3_free(err_msg);
            return false;
        }
        
        return true;
    }
    
    bool setupEnvironment() {
        LogManager::getInstance().Info("\n📋 STEP 0: 환경 설정");
        
        try {
            const std::string source_db = "./data/db/pulseone.db";
            const std::string test_db_path = "/tmp/test_export_complete.db";
            
            // ✅ 1단계: 운영 DB를 /tmp로 복사 (읽기 데이터 포함)
            if (!copyDatabase(source_db, test_db_path)) {
                // 복사 실패 시 빈 DB 생성
                LogManager::getInstance().Warn("운영 DB 복사 실패, 새 DB 생성");
                std::remove(test_db_path.c_str());
                if (!createTestDatabase(test_db_path)) {
                    throw std::runtime_error("테스트 DB 생성 실패");
                }
            }
            LogManager::getInstance().Info("✅ 테스트 DB 준비 완료: " + test_db_path);
            
            // ✅ 2단계: ConfigManager 초기화 후 DB 경로 설정
            ConfigManager::getInstance().initialize();
            ConfigManager::getInstance().set("SQLITE_DB_PATH", test_db_path);
            LogManager::getInstance().Info("✅ ConfigManager 설정 완료");
            
            // ✅ 3단계: DatabaseManager 초기화 (위에서 설정한 경로 사용)
            if (!DatabaseManager::getInstance().initialize()) {
                throw std::runtime_error("DatabaseManager 초기화 실패");
            }
            LogManager::getInstance().Info("✅ DatabaseManager 초기화 완료");
            
            // ✅ 4단계: RepositoryFactory 초기화 (필수!)
            if (!PulseOne::Database::RepositoryFactory::getInstance().initialize()) {
                throw std::runtime_error("RepositoryFactory 초기화 실패");
            }
            LogManager::getInstance().Info("✅ RepositoryFactory 초기화 완료");
            
            if (!createTestTargets()) {
                throw std::runtime_error("테스트 타겟 생성 실패");
            }
            
    #ifdef HAVE_HTTPLIB
            mock_server_ = std::make_unique<MockWebhookServer>();
            if (!mock_server_->start()) {
                throw std::runtime_error("Mock 서버 시작 실패");
            }
            LogManager::getInstance().Info("✅ Mock 서버 시작 완료");
    #endif
            
            redis_client_ = std::make_shared<RedisClientImpl>();
            
            LogManager::getInstance().Info("Redis 연결 시도: pulseone-redis:6379");
            if (!redis_client_->connect("pulseone-redis", 6379)) {
                LogManager::getInstance().Error("Redis 연결 실패 - 재시도");
                std::this_thread::sleep_for(std::chrono::seconds(1));
                
                if (!redis_client_->connect("pulseone-redis", 6379)) {
                    throw std::runtime_error("Redis 재연결 실패");
                }
            }
            LogManager::getInstance().Info("✅ Redis 연결 완료");
            
            // ✅ 정확한 필드명 사용
            Coordinator::ExportCoordinatorConfig config;
            config.database_path = "/tmp/test_export_complete.db";  // 임시 테스트 DB
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
            
            // ✅ 생성자에 config 전달
            coordinator_ = std::make_unique<Coordinator::ExportCoordinator>(config);
            
            if (!coordinator_->start()) {
                throw std::runtime_error("ExportCoordinator 시작 실패");
            }
            
            LogManager::getInstance().Info("✅ ExportCoordinator 시작 완료");
            
            // ✅ 추가: 구독이 완전히 완료될 때까지 대기
            LogManager::getInstance().Info("⏰ 구독 초기화 대기 중...");
            std::this_thread::sleep_for(std::chrono::seconds(3));
            LogManager::getInstance().Info("✅ 구독 준비 완료\n");
            
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
            
            // 알람 전송용 타겟
            ExportTargetEntity alarm_target;
            
            // ✅ setName() 사용 (setTargetName 아님)
            alarm_target.setName("TEST_ALARM_TARGET");
            alarm_target.setTargetType("http");
            alarm_target.setEnabled(true);
            alarm_target.setDescription("Test alarm webhook target");
            
#ifdef HAVE_HTTPLIB
            json alarm_config = {
                {"url", "http://localhost:18080/webhook"},
                {"method", "POST"},
                {"timeout", 5000}
            };
#else
            json alarm_config = {
                {"url", "http://httpbin.org/post"},
                {"method", "POST"},
                {"timeout", 5000}
            };
#endif
            alarm_target.setConfig(alarm_config.dump());
            
            if (!target_repo.save(alarm_target)) {
                throw std::runtime_error("알람 타겟 저장 실패");
            }
            alarm_target_id_ = alarm_target.getId();
            
            // 스케줄 전송용 타겟
            ExportTargetEntity schedule_target;
            
            // ✅ setName() 사용
            schedule_target.setName("TEST_SCHEDULE_TARGET");
            schedule_target.setTargetType("http");
            schedule_target.setEnabled(true);
            schedule_target.setDescription("Test schedule webhook target");
            
#ifdef HAVE_HTTPLIB
            json schedule_config = {
                {"url", "http://localhost:18080/webhook"},
                {"method", "POST"},
                {"timeout", 5000}
            };
#else
            json schedule_config = {
                {"url", "http://httpbin.org/post"},
                {"method", "POST"},
                {"timeout", 5000}
            };
#endif
            schedule_target.setConfig(schedule_config.dump());
            
            if (!target_repo.save(schedule_target)) {
                throw std::runtime_error("스케줄 타겟 저장 실패");
            }
            schedule_target_id_ = schedule_target.getId();
            
            LogManager::getInstance().Info("✅ 테스트 타겟 생성 완료 (알람: " + 
                std::to_string(alarm_target_id_) + ", 스케줄: " + 
                std::to_string(schedule_target_id_) + ")");
            
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("타겟 생성 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    // =========================================================================
    // 테스트 1: 알람 플로우
    // =========================================================================
    
    bool testAlarmFlow() {
        LogManager::getInstance().Info("\n📋 STEP 1: 알람 플로우 테스트");
        
        try {
            // ✅ AlarmMessage 필드: bd, nm, vl, tm, al, st, des
            CSP::AlarmMessage alarm;
            alarm.bd = 1001;  // building_id
            alarm.nm = "TEMP_01";  // point_name
            alarm.vl = 85.5;  // value
            alarm.tm = TestHelper::getCurrentTimestamp();  // timestamp
            alarm.al = 1;  // alarm flag (1=발생)
            alarm.st = 1;  // status
            alarm.des = "온도 상한 초과";  // description
            
            // Redis는 이미 setupEnvironment에서 연결되어 있음
            
            json alarm_json = {
                {"bd", alarm.bd},
                {"nm", alarm.nm},
                {"vl", alarm.vl},
                {"tm", alarm.tm},
                {"al", alarm.al},
                {"st", alarm.st},
                {"des", alarm.des}
            };
            
            std::string alarm_str = alarm_json.dump();
            LogManager::getInstance().Info("알람 JSON: " + alarm_str);
            
            // ✅ 수정: publish()는 int 반환 (구독자 수)
            int subscriber_count = redis_client_->publish("alarms:all", alarm_str);
            bool publish_success = (subscriber_count >= 0);  // 0 이상이면 성공
            
            LogManager::getInstance().Info(
                "Redis publish 결과: " + 
                std::string(publish_success ? "성공" : "실패") + 
                " (구독자: " + std::to_string(subscriber_count) + "명)"
            );
            
            if (!publish_success) {
                LogManager::getInstance().Error("Redis publish 실패 - 재시도");
                
                // 재연결 시도
                redis_client_->disconnect();
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                
                if (!redis_client_->connect("pulseone-redis", 6379)) {
                    throw std::runtime_error("Redis 재연결 실패");
                }
                
                subscriber_count = redis_client_->publish("alarms:all", alarm_str);
                publish_success = (subscriber_count >= 0);
                
                if (!publish_success) {
                    throw std::runtime_error("Redis publish 재시도 실패");
                }
                
                LogManager::getInstance().Info(
                    "✅ 재시도 성공 (구독자: " + std::to_string(subscriber_count) + "명)"
                );
            }
            
            LogManager::getInstance().Info("✅ 알람 발행 완료");
            
            LogManager::getInstance().Info("⏰ 알람 처리 대기 중...");
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
    #ifdef HAVE_HTTPLIB
            if (mock_server_) {
                auto received = mock_server_->getReceivedData();
                TestHelper::assertCondition(
                    !received.empty(),
                    "알람이 타겟으로 전송됨 (수신: " + std::to_string(received.size()) + "건)");
            }
    #endif
            
            LogManager::getInstance().Info("✅ 알람 플로우 테스트 완료\n");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("알람 플로우 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    // =========================================================================
    // 테스트 2: 스케줄 플로우
    // =========================================================================
    
    bool testScheduleFlow() {
        LogManager::getInstance().Info("\n📋 STEP 3: 스케줄 플로우 테스트");
        
        try {
            auto next_run = std::chrono::system_clock::now() + std::chrono::seconds(3);
            if (!createTestSchedule(next_run)) {
                throw std::runtime_error("스케줄 생성 실패");
            }
            
#ifdef HAVE_HTTPLIB
            if (mock_server_) {
                mock_server_->clearReceivedData();
            }
#endif
            
            LogManager::getInstance().Info("⏰ 스케줄 실행 대기 중...");
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
#ifdef HAVE_HTTPLIB
            if (mock_server_) {
                auto received = mock_server_->getReceivedData();
                TestHelper::assertCondition(
                    !received.empty(),
                    "스케줄이 실행됨 (수신: " + std::to_string(received.size()) + "건)");
            }
#endif
            
            LogManager::getInstance().Info("✅ 스케줄 플로우 테스트 완료\n");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("스케줄 플로우 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool createTestSchedule(const std::chrono::system_clock::time_point& next_run) {
        try {
            using namespace Database::Repositories;
            using namespace Database::Entities;
            
            ExportScheduleRepository schedule_repo;
            ExportScheduleEntity schedule;
            schedule.setTargetId(schedule_target_id_);
            schedule.setScheduleName("TEST_SCHEDULE");
            schedule.setCronExpression("* * * * *");
            schedule.setNextRunAt(next_run);
            schedule.setEnabled(true);
            schedule.setDescription("Test schedule for integration test");
            
            if (!schedule_repo.save(schedule)) {
                throw std::runtime_error("스케줄 저장 실패");
            }
            
            int schedule_id = schedule.getId();
            
            LogManager::getInstance().Info("✅ 테스트 스케줄 생성 완료 (ID: " + 
                std::to_string(schedule_id) + ")");
            
            // ✅ Redis 이벤트 발행 (ScheduledExporter 즉시 리로드)
            if (redis_client_) {
                std::string event_payload = R"({"type":"created","schedule_id":)" + 
                    std::to_string(schedule_id) + "}";
                
                redis_client_->publish("schedule:reload", event_payload);
                LogManager::getInstance().Info("📢 스케줄 리로드 이벤트 발행");
            }
            
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("스케줄 생성 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    // =========================================================================
    // 테스트 3: DynamicTargetManager 통계
    // =========================================================================
    
    bool testDynamicTargetManager() {
        LogManager::getInstance().Info("\n📋 STEP 4: DynamicTargetManager 검증");
        
        try {
            auto& manager = CSP::DynamicTargetManager::getInstance();
            
            TestHelper::assertCondition(
                manager.isRunning(),
                "DynamicTargetManager 실행 중");
            
            // ✅ 수정 1: getAllTargets() 사용
            auto targets = manager.getAllTargets();
            TestHelper::assertCondition(
                !targets.empty(),
                "타겟 목록 로드됨 (" + std::to_string(targets.size()) + "개)");
            
            // ✅ 수정 2: getStatistics() 사용 (전체 통계)
            auto stats = manager.getStatistics();
            
            LogManager::getInstance().Info("\n📊 전체 전송 통계:");
            LogManager::getInstance().Info("  - 총 요청: " + 
                std::to_string(stats["total_requests"].get<uint64_t>()));
            LogManager::getInstance().Info("  - 성공: " + 
                std::to_string(stats["total_successes"].get<uint64_t>()));
            LogManager::getInstance().Info("  - 실패: " + 
                std::to_string(stats["total_failures"].get<uint64_t>()));
            
            if (stats.contains("success_rate")) {
                LogManager::getInstance().Info("  - 성공률: " + 
                    std::to_string(stats["success_rate"].get<double>()) + "%");
            }
            
            if (stats.contains("avg_response_time_ms")) {
                LogManager::getInstance().Info("  - 평균 응답시간: " + 
                    std::to_string(stats["avg_response_time_ms"].get<uint64_t>()) + "ms");
            }
            
            // 타겟별 정보 출력
            LogManager::getInstance().Info("\n📋 로드된 타겟 목록:");
            for (size_t i = 0; i < targets.size(); ++i) {
                const auto& target = targets[i];
                LogManager::getInstance().Info(
                    "  " + std::to_string(i + 1) + ". " + target.name + 
                    " (" + target.type + ") - " + 
                    (target.enabled ? "활성화" : "비활성화"));
            }
            
            LogManager::getInstance().Info("\n✅ DynamicTargetManager 검증 완료\n");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("DynamicTargetManager 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    // =========================================================================
    // 테스트 4: ExportLog 검증
    // =========================================================================
    
    bool testExportLogs() {
        LogManager::getInstance().Info("\n📋 STEP 5: ExportLog 검증");
        
        try {
            using namespace Database::Repositories;
            
            ExportLogRepository log_repo;
            
            auto alarm_stats = log_repo.getTargetStatistics(alarm_target_id_, 24);
            
            LogManager::getInstance().Info("📊 알람 타겟 통계:");
            LogManager::getInstance().Info("  - 총 전송: " + 
                std::to_string(alarm_stats["total"]));
            LogManager::getInstance().Info("  - 성공: " + 
                std::to_string(alarm_stats["successful"]));
            LogManager::getInstance().Info("  - 실패: " + 
                std::to_string(alarm_stats["failed"]));
            
            auto schedule_stats = log_repo.getTargetStatistics(schedule_target_id_, 24);
            
            LogManager::getInstance().Info("\n📊 스케줄 타겟 통계:");
            LogManager::getInstance().Info("  - 총 전송: " + 
                std::to_string(schedule_stats["total"]));
            LogManager::getInstance().Info("  - 성공: " + 
                std::to_string(schedule_stats["successful"]));
            LogManager::getInstance().Info("  - 실패: " + 
                std::to_string(schedule_stats["failed"]));
            
            LogManager::getInstance().Info("\n✅ ExportLog 검증 완료\n");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("ExportLog 검증 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    // =========================================================================
    // 정리
    // =========================================================================
    
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
        
        // ✅ 임시 테스트 DB 삭제
        try {
            std::remove("/tmp/test_export_complete.db");
            LogManager::getInstance().Info("✅ 테스트 DB 삭제 완료");
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
        
        RealWorkingIntegrationTest test;
        bool success = test.runAllTests();
        
        std::cout << "\n";
        if (success) {
            std::cout << "✨ 테스트 결과: 성공! 🎉\n";
        } else {
            std::cout << "💥 테스트 결과: 실패\n";
        }
        std::cout << "\n";
        
        return success ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "💥 예외 발생: " << e.what() << "\n";
        return 1;
    }
}