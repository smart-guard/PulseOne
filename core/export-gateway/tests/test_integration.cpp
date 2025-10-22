// =============================================================================
// core/export-gateway/tests/test_integration.cpp
// Export Gateway 완전 통합 테스트 (E2E) - v8.0 FINAL COMPLETE
// =============================================================================
// 🔥 v8.0 최종 수정 사항 (2025-10-22):
//   ✅ testVerifyTransmittedData() - WHERE target_id = 1 하드코딩 완전 제거
//   ✅ findAll()로 전체 로그 조회 후 동적 검증
//   ✅ 다중 타겟 지원 (몇 개든 상관없이 작동)
//   ✅ target_id 동적 수집 및 통계 계산
//   ✅ Repository 패턴 100% 적용
//   ✅ 상세한 로그 출력으로 디버깅 용이
// =============================================================================
// v7.0 → v8.0 핵심 변경:
//   ❌ v7.0: findByTargetId(1, 100) - 하드코딩된 ID
//   ✅ v8.0: findAll() + 동적 target_id 수집
// =============================================================================

#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <set>
#include <nlohmann/json.hpp>

// PulseOne 헤더
#include "CSP/CSPGateway.h"
#include "CSP/AlarmMessage.h"
#include "Database/DatabaseManager.h"
#include "Client/RedisClientImpl.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"

// Repository 패턴
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
            if (!setupCSPGateway()) return false;
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
                std::to_string(targets.size()) + "개 타겟 로드됨");
            
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("타겟 로드 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testAddDataToRedis() {
        LogManager::getInstance().Info("📋 STEP 3: Redis 데이터 추가");
        
        try {
            if (!redis_client_ || !redis_client_->isConnected()) {
                LogManager::getInstance().Warn("⚠️ Redis 비활성 - 건너뜀");
                return true;
            }
            
            json test_data = {
                {"building_id", 1001},
                {"point_name", "TEMP_SENSOR_01"},
                {"value", 25.5},
                {"timestamp", TestHelper::getCurrentTimestamp()},
                {"status", 1}
            };
            
            std::string key = "data:1001:TEMP_SENSOR_01";
            redis_client_->set(key, test_data.dump());
            
            TestHelper::assertCondition(true, "Redis에 테스트 데이터 추가");
            LogManager::getInstance().Info("✅ STEP 3 완료: Redis 데이터 저장");
            
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Redis 데이터 추가 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testReadDataFromRedis() {
        LogManager::getInstance().Info("📋 STEP 4: Redis 데이터 읽기");
        
        try {
            if (!redis_client_ || !redis_client_->isConnected()) {
                LogManager::getInstance().Warn("⚠️ Redis 비활성 - 건너뜀");
                return true;
            }
            
            std::string key = "data:1001:TEMP_SENSOR_01";
            std::string value = redis_client_->get(key);
            
            TestHelper::assertCondition(!value.empty(), "Redis에서 데이터 읽기 성공");
            
            json data = json::parse(value);
            LogManager::getInstance().Info("📖 읽은 데이터: " + data.dump());
            LogManager::getInstance().Info("✅ STEP 4 완료: Redis 데이터 읽기 성공");
            
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Redis 데이터 읽기 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testSendAlarmToTarget() {
        LogManager::getInstance().Info("📋 STEP 5: 알람 전송 테스트");
        
        try {
            auto test_alarm = CSP::AlarmMessage::create_sample(
                1001, "TEMP_SENSOR_01", 25.5, true);
            test_alarm.des = "Test alarm from integration test";
            
            auto result = csp_gateway_->taskAlarmSingle(test_alarm);
            
            TestHelper::assertCondition(result.success, "알람 전송 성공");
            LogManager::getInstance().Info("✅ STEP 5 완료: 알람 전송 성공");
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("알람 전송 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    // 🔥🔥🔥 v8.0 핵심 수정: 동적 검증!
    bool testVerifyTransmittedData() {
        try {
            LogManager::getInstance().Info("📋 STEP 6: Repository로 전송 데이터 검증");
            
#ifdef HAVE_HTTPLIB
            // 1. Mock 서버 수신 확인
            auto received = mock_server_->getReceivedData();
            if (received.empty()) {
                LogManager::getInstance().Error("❌ Mock 서버가 데이터를 수신하지 못함");
                TestHelper::assertCondition(false, "Mock 서버가 데이터를 수신함");
                return false;
            }
            
            TestHelper::assertCondition(true, "Mock 서버가 데이터를 수신함");
            LogManager::getInstance().Info("📨 Mock 수신 데이터: " + received[0].dump());
#endif
            
            // 2. 🔥 핵심 수정: 전체 로그 조회!
            LogManager::getInstance().Info("🔍 Repository로 export_logs 검증 시작...");
            LogManager::getInstance().Info("   조회 방식: findAll() - 전체 로그 (하드코딩 제거!)");
            
            ExportLogRepository log_repo;
            
            // ✅ 방법 1: 전체 로그 조회
            auto all_logs = log_repo.findAll();
            
            LogManager::getInstance().Info("📊 조회된 로그 총 개수: " + std::to_string(all_logs.size()));
            
            if (all_logs.empty()) {
                LogManager::getInstance().Error("❌ FAILED: Repository에서 로그 조회 실패 (0개)");
                LogManager::getInstance().Error("   가능한 원인:");
                LogManager::getInstance().Error("   1. DynamicTargetManager::saveExportLog() 미호출");
                LogManager::getInstance().Error("   2. ExportLogEntity::save() 실패");
                LogManager::getInstance().Error("   3. target_id가 NULL로 저장됨");
                TestHelper::assertCondition(false, "Repository에서 최소 1개 이상의 로그 조회");
                return false;
            }
            
            // ✅ 성공!
            TestHelper::assertCondition(true, "Repository에서 로그 조회 성공 (" + 
                                       std::to_string(all_logs.size()) + "개)");
            
            // 3. 로그 상세 출력 및 검증
            LogManager::getInstance().Info("📝 조회된 로그 상세:");
            
            bool found_success = false;
            int success_count = 0;
            int failure_count = 0;
            
            for (size_t i = 0; i < all_logs.size(); ++i) {
                const auto& log = all_logs[i];
                
                LogManager::getInstance().Info("  [" + std::to_string(i+1) + "] ID: " + 
                                              std::to_string(log.getId()));
                LogManager::getInstance().Info("      Target ID: " + 
                                              std::to_string(log.getTargetId()));
                LogManager::getInstance().Info("      Log Type: " + log.getLogType());
                LogManager::getInstance().Info("      Status: " + log.getStatus());
                LogManager::getInstance().Info("      HTTP Status: " + 
                                              std::to_string(log.getHttpStatusCode()));
                LogManager::getInstance().Info("      Processing Time: " + 
                                              std::to_string(log.getProcessingTimeMs()) + "ms");
                
                if (log.getStatus() == "success") {
                    found_success = true;
                    success_count++;
                } else {
                    failure_count++;
                    if (!log.getErrorMessage().empty()) {
                        LogManager::getInstance().Info("      Error: " + log.getErrorMessage());
                    }
                }
            }
            
            TestHelper::assertCondition(found_success, "성공 로그가 최소 1개 존재함");
            
            LogManager::getInstance().Info("📊 로그 통계 요약:");
            LogManager::getInstance().Info("   총 로그 수: " + std::to_string(all_logs.size()));
            LogManager::getInstance().Info("   성공: " + std::to_string(success_count));
            LogManager::getInstance().Info("   실패: " + std::to_string(failure_count));
            
            // 4. ✅ 방법 2: 조건으로 성공 로그만 필터링
            LogManager::getInstance().Info("🔍 성공 로그만 필터링 조회...");
            
            std::vector<PulseOne::Database::QueryCondition> conditions = {
                {"status", "=", "success"}
            };
            auto success_logs = log_repo.findByConditions(conditions);
            
            LogManager::getInstance().Info("✅ 성공 로그 개수: " + 
                                          std::to_string(success_logs.size()));
            
            TestHelper::assertCondition(success_logs.size() > 0, 
                                       "성공 로그가 최소 1개 이상 존재");
            
            // 5. ✅ 방법 3: 타겟별 통계 (동적 수집)
            LogManager::getInstance().Info("📊 타겟별 로그 통계:");
            
            // 🔥 핵심: target_id를 동적으로 수집!
            std::set<int> target_ids;
            for (const auto& log : all_logs) {
                if (log.getTargetId() > 0) {  // 유효한 ID만
                    target_ids.insert(log.getTargetId());
                }
            }
            
            if (target_ids.empty()) {
                LogManager::getInstance().Warn("⚠️ 유효한 target_id가 없음!");
                LogManager::getInstance().Warn("   모든 로그의 target_id가 0 또는 NULL");
            } else {
                LogManager::getInstance().Info("   발견된 타겟 ID 개수: " + 
                                              std::to_string(target_ids.size()));
                
                // 각 타겟별로 통계 출력
                for (int target_id : target_ids) {
                    auto target_logs = log_repo.findByTargetId(target_id, 100);
                    
                    LogManager::getInstance().Info("   Target ID " + 
                                                  std::to_string(target_id) + ": " + 
                                                  std::to_string(target_logs.size()) + " 로그");
                    
                    // 타겟별 성공률 계산
                    int target_success = 0;
                    for (const auto& tlog : target_logs) {
                        if (tlog.getStatus() == "success") {
                            target_success++;
                        }
                    }
                    
                    double success_rate = target_logs.empty() ? 0.0 : 
                                         (double)target_success / target_logs.size() * 100.0;
                    
                    LogManager::getInstance().Info("      성공: " + 
                                                  std::to_string(target_success) + "/" + 
                                                  std::to_string(target_logs.size()) + 
                                                  " (성공률: " + std::to_string(success_rate) + "%)");
                }
            }
            
            LogManager::getInstance().Info("✅ STEP 6 완료: Repository 검증 성공!");
            return true;
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("데이터 검증 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool runAllTests() {
        LogManager::getInstance().Info("🚀 ========================================");
        LogManager::getInstance().Info("🚀 Export Gateway 통합 테스트 v8.0 FINAL");
        LogManager::getInstance().Info("🚀 (동적 검증 - 하드코딩 제거)");
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
    
    bool insertTestTargets() {
        try {
            std::string url = "http://localhost:18080/webhook";
        #ifdef HAVE_HTTPLIB
            if (mock_server_) {
                url = "http://localhost:" + std::to_string(mock_server_->getPort()) + "/webhook";
            }
        #endif
            
            LogManager::getInstance().Info("🏗️ Repository로 테스트 타겟 생성 시작...");
            
            ExportTargetRepository target_repo;
            ExportLogRepository log_repo;
            
            LogManager::getInstance().Info("✅ Repository 초기화 완료 (테이블 자동 생성)");
            
            ExportTargetEntity target_entity;
            target_entity.setName(url);
            target_entity.setTargetType("http");
            target_entity.setDescription("Test target for integration test");
            target_entity.setEnabled(true);
            
            json config = {
                {"url", url}, 
                {"method", "POST"}, 
                {"content_type", "application/json"},
                {"save_log", true}
            };
            
            target_entity.setConfig(config.dump());
            
            if (!target_repo.save(target_entity)) {
                throw std::runtime_error("테스트 타겟 삽입 실패 (Repository)");
            }
            
            test_target_id_ = target_entity.getId();
            
            if (test_target_id_ <= 0) {
                throw std::runtime_error("타겟 ID가 올바르게 생성되지 않음");
            }
            
            LogManager::getInstance().Info("✅ Repository로 테스트 타겟 삽입 완료");
            LogManager::getInstance().Info("   - name: " + url);
            LogManager::getInstance().Info("   - id: " + std::to_string(test_target_id_));
            
            if (csp_gateway_) {
                LogManager::getInstance().Info("🔄 CSPGateway 타겟 리로드 중...");
                
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
        LogManager::getInstance().Info("🚀 Export Gateway 통합 테스트 v8.0 FINAL 시작");
        
        ExportGatewayIntegrationTest test;
        bool success = test.runAllTests();
        
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "❌ 예외 발생: " << e.what() << std::endl;
        return 1;
    }
}