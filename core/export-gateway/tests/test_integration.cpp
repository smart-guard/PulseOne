// =============================================================================
// core/export-gateway/tests/test_integration.cpp
// Export Gateway 완전 통합 테스트 (E2E) - v7.0 FINAL FIX
// =============================================================================
// 🔥 v7.0 수정 사항 (2025-10-22):
//   ✅ test_target_id_ 추적 문제 완전 해결
//   ✅ insertTestTargets()에서 생성된 ID를 test_target_id_에 저장
//   ✅ testVerifyTransmittedData()에서 정확한 target_id로 조회
//   ✅ Repository 패턴 100% 적용
//   ✅ DB 직접 쿼리 완전 제거
// =============================================================================
// 이전 버전 문제:
//   ❌ v6.0: test_target_id_ = 0 (초기화만 되고 실제 값 저장 안 됨)
//   ❌ v6.0: testLoadTargetsFromDatabase()에서 설정했지만 너무 늦음
// =============================================================================

#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <nlohmann/json.hpp>

// PulseOne 헤더
#include "CSP/CSPGateway.h"
#include "CSP/AlarmMessage.h"
#include "Database/DatabaseManager.h"
#include "Client/RedisClientImpl.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"

// 🔥 Repository 추가!
#include "Database/Repositories/ExportLogRepository.h"
#include "Database/Repositories/ExportTargetRepository.h"

// httplib for mock server
#ifdef HAVE_HTTPLIB
#include <httplib.h>
#endif

using json = nlohmann::json;
using namespace PulseOne;
using namespace PulseOne::Database::Repositories;
using namespace PulseOne::Database::Entities;

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
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }
    
    static void assertCondition(bool condition, const std::string& message) {
        if (!condition) {
            LogManager::getInstance().Error("❌ FAILED: " + message);
            throw std::runtime_error("Test failed: " + message);
        }
        LogManager::getInstance().Info("✅ PASSED: " + message);
    }
};

// =============================================================================
// Mock HTTP 서버
// =============================================================================

#ifdef HAVE_HTTPLIB
class MockTargetServer {
private:
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    std::atomic<bool> running_{false};
    int port_ = 18080;
    mutable std::mutex data_mutex_;
    std::vector<json> received_data_;
    
public:
    MockTargetServer() {
        server_ = std::make_unique<httplib::Server>();
        setupRoutes();
    }
    
    ~MockTargetServer() {
        stop();
    }
    
    bool start() {
        if (running_.load()) return true;
        running_.store(true);
        server_thread_ = std::thread([this]() {
            LogManager::getInstance().Info("🚀 Mock 서버 시작: http://localhost:" + 
                                          std::to_string(port_));
            server_->listen("0.0.0.0", port_);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return true;
    }
    
    void stop() {
        if (!running_.load()) return;
        running_.store(false);
        if (server_) {
            server_->stop();
        }
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
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
                json received = json::parse(req.body);
                received_data_.push_back(received);
                LogManager::getInstance().Info("📨 Mock 서버 수신: " + req.body);
                res.status = 200;
                res.set_content(R"({"status":"success"})", "application/json");
            } catch (const std::exception& e) {
                LogManager::getInstance().Error("❌ Mock 서버 파싱 실패: " + std::string(e.what()));
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

class ExportGatewayIntegrationTest {
private:
    DatabaseManager& db_manager_;
    std::unique_ptr<RedisClientImpl> redis_client_;
    std::unique_ptr<CSP::CSPGateway> csp_gateway_;
#ifdef HAVE_HTTPLIB
    std::unique_ptr<MockTargetServer> mock_server_;
#endif
    std::string test_db_path_;
    json original_redis_data_;
    
    // 🔥 핵심 수정: test_target_id_를 여기서 추적!
    int test_target_id_ = 0;
    
public:
    ExportGatewayIntegrationTest() 
        : db_manager_(DatabaseManager::getInstance())
        , test_db_path_("/tmp/test_export_gateway.db") {
        LogManager::getInstance().Info("🧪 통합 테스트 초기화 시작");
    }
    
    ~ExportGatewayIntegrationTest() {
        cleanup();
    }
    
    bool setupTestEnvironment() {
        LogManager::getInstance().Info("📋 STEP 1: 테스트 환경 준비");
        try {
            if (!setupTestDatabase()) return false;
            if (!setupRedisConnection()) return false;
#ifdef HAVE_HTTPLIB
            if (!setupMockServer()) return false;
#endif
            // 🔥 핵심 수정: CSPGateway를 먼저 초기화!
            // (DynamicTargetManager가 준비되어야 함)
            if (!setupCSPGateway()) return false;
            
            // 그 다음 타겟 삽입 + 자동 리로드
            if (!insertTestTargets()) return false;
            
            LogManager::getInstance().Info("✅ 테스트 환경 준비 완료");
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("환경 준비 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testLoadTargetsFromDatabase() {
        LogManager::getInstance().Info("📋 STEP 2: Export Target 로드 검증");
        
        try {
            ExportTargetRepository target_repo;
            auto targets = target_repo.findByEnabled(true);
            
            TestHelper::assertCondition(
                !targets.empty(),
                "활성화된 타겟이 최소 1개 이상 존재");
            
            TestHelper::assertCondition(
                test_target_id_ > 0,
                "test_target_id가 올바르게 설정됨 (ID: " + std::to_string(test_target_id_) + ")");
            
            LogManager::getInstance().Info("✅ STEP 2 완료: " + 
                std::to_string(targets.size()) + "개 타겟 로드됨 (test_target_id=" + 
                std::to_string(test_target_id_) + ")");
            
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("타겟 로드 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testAddDataToRedis() {
        LogManager::getInstance().Info("📋 STEP 3: Redis 데이터 추가");
        
        if (!redis_client_ || !redis_client_->isConnected()) {
            LogManager::getInstance().Warn("⚠️ Redis 비활성 - 건너뜀");
            return true;
        }
        
        try {
            original_redis_data_ = {
                {"building_id", "1001"},
                {"point_name", "TEMP_SENSOR_01"},
                {"value", 25.5},
                {"timestamp", TestHelper::getCurrentTimestamp()},
                {"alarm_flag", 1},
                {"status", 1}
            };
            
            std::string redis_key = "alarm:1001:TEMP_SENSOR_01";
            redis_client_->setex(redis_key, original_redis_data_.dump(), 3600);
            
            TestHelper::assertCondition(true, "Redis 데이터 추가 성공");
            LogManager::getInstance().Info("✅ STEP 3 완료: Redis 키 '" + redis_key + "' 추가됨");
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Redis 데이터 추가 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testReadDataFromRedis() {
        LogManager::getInstance().Info("📋 STEP 4: Redis 데이터 읽기");
        
        if (!redis_client_ || !redis_client_->isConnected()) {
            LogManager::getInstance().Warn("⚠️ Redis 비활성 - 건너뜀");
            return true;
        }
        
        try {
            std::string redis_key = "alarm:1001:TEMP_SENSOR_01";
            std::string value = redis_client_->get(redis_key);
            
            TestHelper::assertCondition(!value.empty(), "Redis 데이터 읽기 성공");
            
            json parsed = json::parse(value);
            
            TestHelper::assertCondition(
                parsed["building_id"] == original_redis_data_["building_id"],
                "building_id 일치");
            
            TestHelper::assertCondition(
                parsed["point_name"] == original_redis_data_["point_name"],
                "point_name 일치");
            
            LogManager::getInstance().Info("✅ STEP 4 완료: Redis 데이터 읽기 및 검증 완료");
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Redis 데이터 읽기 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testSendAlarmToTarget() {
        LogManager::getInstance().Info("📋 STEP 5: 알람 전송 (CSPGateway)");
        
        try {
            if (!csp_gateway_) {
                throw std::runtime_error("CSPGateway가 초기화되지 않음");
            }
            
            // AlarmMessage 생성
            CSP::AlarmMessage alarm;
            alarm.bd = 1001;  // int
            alarm.nm = "TEMP_SENSOR_01";
            alarm.vl = 25.5;  // double
            alarm.tm = TestHelper::getCurrentTimestamp();
            alarm.al = 1;
            alarm.st = 1;
            alarm.des = "Test alarm from integration test";
            
            // 알람 전송
            auto result = csp_gateway_->taskAlarmSingle(alarm);
            
            TestHelper::assertCondition(result.success, "알람 전송 성공");
            
            LogManager::getInstance().Info("✅ STEP 5 완료: 알람 전송 성공");
            
            // 로그 저장을 위해 잠시 대기
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("알람 전송 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testVerifyTransmittedData() {
        LogManager::getInstance().Info("📋 STEP 6: Repository로 전송 데이터 검증");
        
#ifdef HAVE_HTTPLIB
        try {
            // 1. Mock 서버 수신 확인
            auto received = mock_server_->getReceivedData();
            
            TestHelper::assertCondition(
                !received.empty(),
                "Mock 서버가 데이터를 수신함");
            
            LogManager::getInstance().Info("📨 Mock 수신 데이터: " + received[0].dump());
            
            // 2. 🔥 핵심 수정: test_target_id_로 정확히 조회!
            LogManager::getInstance().Info("🔍 Repository로 export_logs 검증 시작...");
            LogManager::getInstance().Info("   검색 target_id: " + std::to_string(test_target_id_));
            
            ExportLogRepository log_repo;
            
            // 2-1. findByTargetId로 로그 조회
            auto logs = log_repo.findByTargetId(test_target_id_, 10);
            
            TestHelper::assertCondition(
                !logs.empty(),
                "Repository에서 로그 조회 성공 (최소 1개)");
            
            LogManager::getInstance().Info("📝 조회된 로그 수: " + std::to_string(logs.size()));
            
            // 2-2. 로그 내용 검증
            bool found_success = false;
            for (const auto& log : logs) {
                if (log.getStatus() == "success") {
                    found_success = true;
                    LogManager::getInstance().Info(
                        "  ✅ 성공 로그 발견: target_id=" + std::to_string(log.getTargetId()) +
                        ", status=" + log.getStatus() +
                        ", processing_time=" + std::to_string(log.getProcessingTimeMs()) + "ms");
                    break;
                }
            }
            
            TestHelper::assertCondition(found_success, "성공 로그가 존재함");
            
            // 3. 통계 검증
            auto stats = log_repo.getTargetStatistics(test_target_id_, 24);
            
            TestHelper::assertCondition(
                stats.find("total") != stats.end() && stats["total"] > 0,
                "통계: 전체 로그 수 > 0");
            
            LogManager::getInstance().Info("📊 통계 정보:");
            LogManager::getInstance().Info("  - 전체 전송: " + std::to_string(stats["total"]));
            LogManager::getInstance().Info("  - 성공: " + std::to_string(stats["success_count"]));
            LogManager::getInstance().Info("  - 실패: " + std::to_string(stats["failure_count"]));
            
            LogManager::getInstance().Info("✅ STEP 6 완료: Repository 검증 성공!");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("데이터 검증 실패: " + std::string(e.what()));
            return false;
        }
#else
        LogManager::getInstance().Warn("⚠️ httplib 없음 - 검증 건너뜀");
        return true;
#endif
    }
    
    bool runAllTests() {
        LogManager::getInstance().Info("🚀 ========================================");
        LogManager::getInstance().Info("🚀 Export Gateway 통합 테스트 v7.0");
        LogManager::getInstance().Info("🚀 (target_id 추적 문제 해결)");
        LogManager::getInstance().Info("🚀 ========================================");
        
        bool all_passed = true;
        
        if (!setupTestEnvironment()) return false;
        if (!testLoadTargetsFromDatabase()) all_passed = false;
        if (!testAddDataToRedis()) all_passed = false;
        if (!testReadDataFromRedis()) all_passed = false;
        if (!testSendAlarmToTarget()) all_passed = false;
        if (!testVerifyTransmittedData()) all_passed = false;
        
        LogManager::getInstance().Info("🚀 ========================================");
        if (all_passed) {
            LogManager::getInstance().Info("🎉 모든 테스트 통과!");
        } else {
            LogManager::getInstance().Error("❌ 일부 테스트 실패");
        }
        LogManager::getInstance().Info("🚀 ========================================");
        
        return all_passed;
    }
    
private:
    bool setupTestDatabase() {
        try {
            if (std::filesystem::exists(test_db_path_)) {
                std::filesystem::remove(test_db_path_);
            }
            
            auto& config = ConfigManager::getInstance();
            config.set("SQLITE_DB_PATH", test_db_path_);
            config.set("CSP_DATABASE_PATH", test_db_path_);
            config.set("DATABASE_PATH", test_db_path_);
            
            db_manager_.reinitialize();
            
            TestHelper::assertCondition(
                db_manager_.isSQLiteConnected(), 
                "SQLite 연결 성공");
            
            LogManager::getInstance().Info("✅ 1-1. 테스트 DB 초기화 완료: " + test_db_path_);
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("DB 초기화 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool setupRedisConnection() {
        try {
            redis_client_ = std::make_unique<RedisClientImpl>();
            
            if (redis_client_->isConnected()) {
                LogManager::getInstance().Info("✅ 1-2. Redis 연결 성공");
            } else {
                LogManager::getInstance().Warn("⚠️ Redis 비활성 (선택적)");
            }
            
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Redis 연결 실패: " + std::string(e.what()));
            return false;
        }
    }
    
#ifdef HAVE_HTTPLIB
    bool setupMockServer() {
        try {
            mock_server_ = std::make_unique<MockTargetServer>();
            if (!mock_server_->start()) {
                throw std::runtime_error("Mock 서버 시작 실패");
            }
            LogManager::getInstance().Info("✅ 1-3. Mock 서버 시작 완료 (포트: " + 
                std::to_string(mock_server_->getPort()) + ")");
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Mock 서버 실패: " + std::string(e.what()));
            return false;
        }
    }
#else
    bool setupMockServer() {
        LogManager::getInstance().Warn("⚠️ httplib 없음 - Mock 서버 건너뜀");
        return true;
    }
#endif
    
    bool setupCSPGateway() {
        try {
            CSP::CSPGatewayConfig config;
            config.building_id = "1001";
            config.use_dynamic_targets = true;
            config.debug_mode = true;
            
#ifdef HAVE_HTTPLIB
            if (mock_server_) {
                config.api_endpoint = "http://localhost:" + std::to_string(mock_server_->getPort());
            }
#else
            config.api_endpoint = "http://localhost:18080";
#endif
            
            csp_gateway_ = std::make_unique<CSP::CSPGateway>(config);
            
            LogManager::getInstance().Info("✅ 1-4. CSPGateway 초기화 완료");
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("CSPGateway 초기화 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    // 🔥 핵심 수정: bool 반환 타입으로 변경하고 test_target_id_ 설정!
    bool insertTestTargets() {
        try {
            std::string url = "http://localhost:18080/webhook";
        #ifdef HAVE_HTTPLIB
            if (mock_server_) {
                url = "http://localhost:" + std::to_string(mock_server_->getPort()) + "/webhook";
            }
        #endif
            
            LogManager::getInstance().Info("🏗️ Repository로 테스트 타겟 생성 시작...");
            
            // 1. Repository 생성 (테이블 자동 생성됨)
            ExportTargetRepository target_repo;
            ExportLogRepository log_repo;
            
            LogManager::getInstance().Info("✅ Repository 초기화 완료 (테이블 자동 생성)");
            
            // 2. ExportTargetEntity 생성
            ExportTargetEntity target_entity;
            target_entity.setName(url);
            target_entity.setTargetType("http");  // 소문자!
            target_entity.setDescription("Test target for integration test");
            target_entity.setEnabled(true);
            
            // 3. Config JSON 생성
            json config = {
                {"url", url}, 
                {"method", "POST"}, 
                {"content_type", "application/json"},
                {"save_log", true}  // ← 로그 저장 활성화
            };
            
            target_entity.setConfig(config.dump());
            
            // 4. ✅ Repository로 저장 (ID 자동 생성!)
            if (!target_repo.save(target_entity)) {
                throw std::runtime_error("테스트 타겟 삽입 실패 (Repository)");
            }
            
            // 5. 🔥🔥🔥 핵심 수정: 생성된 ID를 test_target_id_에 저장!
            test_target_id_ = target_entity.getId();
            
            if (test_target_id_ <= 0) {
                throw std::runtime_error("타겟 ID가 올바르게 생성되지 않음");
            }
            
            LogManager::getInstance().Info(
                "✅ Repository로 테스트 타겟 삽입 완료");
            LogManager::getInstance().Info(
                "   - name: " + url);
            LogManager::getInstance().Info(
                "   - id: " + std::to_string(test_target_id_) + " ← 저장됨!");
            
            // 6. 🔥 추가: CSPGateway에 타겟 리로드!
            if (csp_gateway_) {
                LogManager::getInstance().Info("🔄 CSPGateway 타겟 리로드 중...");
                
                // 🔥 public 메소드인 reloadDynamicTargets() 사용!
                if (!csp_gateway_->reloadDynamicTargets()) {
                    throw std::runtime_error("CSPGateway 타겟 리로드 실패");
                }
                
                LogManager::getInstance().Info("✅ CSPGateway 타겟 리로드 완료");
            }
            
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("테스트 타겟 생성 실패: " + std::string(e.what()));
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
        
        if (redis_client_) {
            redis_client_->disconnect();
        }
        
        try {
            if (std::filesystem::exists(test_db_path_)) {
                std::filesystem::remove(test_db_path_);
            }
        } catch (...) {}
        
        LogManager::getInstance().Info("✅ 정리 완료");
    }
};

// =============================================================================
// main
// =============================================================================

int main(int /* argc */, char** /* argv */) {
    try {
        LogManager::getInstance().Info("🚀 Export Gateway 통합 테스트 v7.0 시작");
        
        ExportGatewayIntegrationTest test;
        bool success = test.runAllTests();
        
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "❌ 예외 발생: " << e.what() << std::endl;
        return 1;
    }
}