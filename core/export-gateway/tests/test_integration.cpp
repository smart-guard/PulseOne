// =============================================================================
// core/export-gateway/tests/test_integration.cpp
// Export Gateway 완전 통합 테스트 (E2E) - v5.0 수정 완료판
// =============================================================================
// 🔥 v5.0 수정 사항 (2025-10-22):
//   ✅ JSON 필드 매핑 수정: bd→building_id, nm→point_name, vl→value, al→alarm_flag
//   ✅ 타겟 이름 일치: URL 전체를 name으로 사용
//   ✅ 안전한 JSON 접근: contains() 체크 추가
//   ✅ 에러 핸들링 강화: 상세한 에러 메시지
// =============================================================================
// 이전 버전 문제:
//   ❌ v4.0: received["bd"] 접근 시 키 없음으로 assertion 실패
//   ❌ v4.0: 타겟 이름 불일치 (test-http vs http://localhost:18080/webhook)
// =============================================================================
// 테스트 시나리오:
//   STEP 1: 테스트 환경 준비 (DB + Redis + Mock 서버 + 타겟 삽입)
//   STEP 2: DB에서 Export Target 로드 검증
//   STEP 3: Redis에 테스트 데이터 쓰기 ✅
//   STEP 4: Redis에서 데이터 읽기 및 검증 ✅
//   STEP 5: 알람 전송 (포맷 변환 포함) ✅
//   STEP 6: 전송된 데이터 검증 (원본 vs 전송 일치) ✅ <- 수정됨
// =============================================================================

#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <cmath>
#include <nlohmann/json.hpp>

// PulseOne 헤더
#include "CSP/CSPGateway.h"
#include "CSP/AlarmMessage.h"
#include "Database/DatabaseManager.h"
#include "Client/RedisClientImpl.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"

// httplib for mock server
#ifdef HAVE_HTTPLIB
#include <httplib.h>
#endif

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
    
    // ✅ 새로 추가: 안전한 JSON 값 추출
    template<typename T>
    static T safeGetJson(const json& j, const std::string& key, const T& default_value) {
        if (!j.contains(key)) {
            LogManager::getInstance().Warn("⚠️ JSON 키 없음: " + key + " (기본값 사용)");
            return default_value;
        }
        
        try {
            return j[key].get<T>();
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("❌ JSON 변환 실패 [" + key + "]: " + std::string(e.what()));
            return default_value;
        }
    }
    
    // ✅ 새로 추가: JSON 필드 존재 여부 확인
    static bool verifyJsonFields(const json& j, const std::vector<std::string>& required_fields) {
        for (const auto& field : required_fields) {
            if (!j.contains(field)) {
                LogManager::getInstance().Error("❌ 필수 필드 누락: " + field);
                return false;
            }
        }
        return true;
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
    json original_redis_data_;  // ✅ 원본 데이터 저장 (검증용)
    
public:
    ExportGatewayIntegrationTest() 
        : db_manager_(DatabaseManager::getInstance())
        , test_db_path_("/tmp/test_export_gateway.db") {
        LogManager::getInstance().Info("🧪 통합 테스트 초기화 시작");
    }
    
    ~ExportGatewayIntegrationTest() {
        cleanup();
    }
    
    // =========================================================================
    // STEP 1: 테스트 환경 준비
    // =========================================================================
    bool setupTestEnvironment() {
        LogManager::getInstance().Info("📋 STEP 1: 테스트 환경 준비");
        try {
            if (!setupTestDatabase()) return false;
            if (!setupRedisConnection()) return false;
#ifdef HAVE_HTTPLIB
            if (!setupMockServer()) return false;
#endif
            // 🔥 중요: CSPGateway 초기화 전에 타겟 미리 삽입!
            insertTestTargets();
            if (!setupCSPGateway()) return false;
            LogManager::getInstance().Info("✅ 테스트 환경 준비 완료");
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("테스트 환경 준비 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    // =========================================================================
    // STEP 2: DB에서 타겟 로드 테스트
    // =========================================================================
    bool testLoadTargetsFromDatabase() {
        LogManager::getInstance().Info("📋 STEP 2: DB에서 타겟 로드 테스트");
        try {
            // 타겟은 이미 STEP 1에서 삽입되었음 (CSPGateway 초기화 전)
            // 단지 검증만 수행
            auto target_types = csp_gateway_->getSupportedTargetTypes();
            TestHelper::assertCondition(!target_types.empty(), "타겟 타입이 로드되어야 함");
            
            LogManager::getInstance().Info("✅ 타겟 로드 성공 (" + 
                std::to_string(target_types.size()) + "개 타입)");
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("타겟 로드 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    // =========================================================================
    // STEP 3: Redis에 테스트 데이터 추가 ✅
    // =========================================================================
    bool testAddDataToRedis() {
        LogManager::getInstance().Info("📋 STEP 3: Redis에 테스트 데이터 추가");
        try {
            json alarm_data = {
                {"bd", 1001},
                {"nm", "TEST_TEMP_SENSOR"},
                {"vl", 25.5},
                {"tm", TestHelper::getCurrentTimestamp()},
                {"al", 1},
                {"st", 2},
                {"des", "온도 센서 고온 알람"}
            };
            
            // 원본 데이터 저장 (STEP 6 검증용)
            original_redis_data_ = alarm_data;
            
            std::string redis_key = "alarm:test:1001";
            bool success = redis_client_->set(redis_key, alarm_data.dump());
            
            TestHelper::assertCondition(success, "Redis 데이터 저장 성공");
            LogManager::getInstance().Info("✅ Redis에 테스트 데이터 저장 완료");
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Redis 데이터 추가 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    // =========================================================================
    // STEP 4: Redis에서 데이터 읽기 ✅
    // =========================================================================
    bool testReadDataFromRedis() {
        LogManager::getInstance().Info("📋 STEP 4: Redis에서 데이터 읽기");
        try {
            std::string redis_key = "alarm:test:1001";
            std::string value = redis_client_->get(redis_key);
            
            TestHelper::assertCondition(!value.empty(), "Redis 데이터 읽기 성공");
            
            json read_data = json::parse(value);
            TestHelper::assertCondition(
                read_data["bd"] == original_redis_data_["bd"], 
                "읽은 데이터 일치 확인");
            
            LogManager::getInstance().Info("✅ Redis 데이터 읽기 및 검증 완료");
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Redis 데이터 읽기 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    // =========================================================================
    // STEP 5: 알람 전송 ✅
    // =========================================================================
    bool testSendAlarmToTarget() {
        LogManager::getInstance().Info("📋 STEP 5: 알람 전송 테스트");
        try {
#ifdef HAVE_HTTPLIB
            // Mock 서버 데이터 초기화
            mock_server_->clearReceivedData();
#endif
            
            // AlarmMessage 생성
            CSP::AlarmMessage alarm;
            alarm.bd = original_redis_data_["bd"].get<int>();
            alarm.nm = original_redis_data_["nm"].get<std::string>();
            alarm.vl = original_redis_data_["vl"].get<double>();
            alarm.tm = original_redis_data_["tm"].get<std::string>();
            alarm.al = original_redis_data_["al"].get<int>();
            alarm.st = original_redis_data_["st"].get<int>();
            alarm.des = original_redis_data_["des"].get<std::string>();
            
            auto result = csp_gateway_->taskAlarmSingle(alarm);
            TestHelper::assertCondition(result.success, "알람 전송 성공: " + result.error_message);
            LogManager::getInstance().Info("✅ 알람 전송 완료");
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("알람 전송 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    // =========================================================================
    // STEP 6: 전송 데이터 검증 ✅ (v5.0 수정됨)
    // =========================================================================
    bool testVerifyTransmittedData() {
        LogManager::getInstance().Info("📋 STEP 6: 전송 데이터 검증 (v5.0 수정본)");
#ifdef HAVE_HTTPLIB
        try {
            auto received_list = mock_server_->getReceivedData();
            TestHelper::assertCondition(!received_list.empty(), "데이터 수신 확인");
            
            const json& received = received_list[0];
            
            // 📊 로그 출력
            LogManager::getInstance().Info("📊 원본 데이터: " + original_redis_data_.dump());
            LogManager::getInstance().Info("📊 전송 데이터: " + received.dump());
            
            // ✅ 필수 필드 존재 여부 확인
            std::vector<std::string> required_fields = {
                "building_id", "point_name", "value", "timestamp", 
                "alarm_flag", "status", "description"
            };
            
            if (!TestHelper::verifyJsonFields(received, required_fields)) {
                throw std::runtime_error("필수 필드 누락!");
            }
            
            // ✅ v5.0 수정: 변환된 필드명으로 검증
            // bd → building_id
            TestHelper::assertCondition(
                TestHelper::safeGetJson<int>(received, "building_id", 0) == 
                original_redis_data_["bd"].get<int>(), 
                "BD → building_id 일치");
            
            // nm → point_name
            TestHelper::assertCondition(
                TestHelper::safeGetJson<std::string>(received, "point_name", "") == 
                original_redis_data_["nm"].get<std::string>(), 
                "NM → point_name 일치");
            
            // vl → value
            double received_value = TestHelper::safeGetJson<double>(received, "value", 0.0);
            double original_value = original_redis_data_["vl"].get<double>();
            TestHelper::assertCondition(
                std::abs(received_value - original_value) < 0.01,
                "VL → value 일치 (차이: " + std::to_string(std::abs(received_value - original_value)) + ")");
            
            // al → alarm_flag
            TestHelper::assertCondition(
                TestHelper::safeGetJson<int>(received, "alarm_flag", 0) == 
                original_redis_data_["al"].get<int>(), 
                "AL → alarm_flag 일치");
            
            // st → status
            TestHelper::assertCondition(
                TestHelper::safeGetJson<int>(received, "status", 0) == 
                original_redis_data_["st"].get<int>(), 
                "ST → status 일치");
            
            // des → description
            TestHelper::assertCondition(
                TestHelper::safeGetJson<std::string>(received, "description", "") == 
                original_redis_data_["des"].get<std::string>(), 
                "DES → description 일치");
            
            // ✅ 추가 필드 검증
            TestHelper::assertCondition(
                received.contains("source"), 
                "source 필드 존재");
            
            TestHelper::assertCondition(
                received.contains("version"), 
                "version 필드 존재");
            
            TestHelper::assertCondition(
                received.contains("alarm_status"), 
                "alarm_status 필드 존재");
            
            LogManager::getInstance().Info("✅ 데이터 검증 완료 - 모든 필드 일치!");
            LogManager::getInstance().Info("   - 원본 필드: bd, nm, vl, al, st, des");
            LogManager::getInstance().Info("   - 변환 필드: building_id, point_name, value, alarm_flag, status, description");
            LogManager::getInstance().Info("   - 추가 필드: source, version, alarm_status");
            
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("데이터 검증 실패: " + std::string(e.what()));
            LogManager::getInstance().Error("💡 힌트: HttpTargetHandler::buildJsonRequestBody()에서 필드 변환 확인");
            return false;
        }
#else
        LogManager::getInstance().Warn("⚠️ httplib 없음 - 검증 건너뜀");
        return true;
#endif
    }
    
    // =========================================================================
    // 전체 테스트 실행
    // =========================================================================
    bool runAllTests() {
        LogManager::getInstance().Info("🚀 ========================================");
        LogManager::getInstance().Info("🚀 Export Gateway 통합 테스트 v5.0");
        LogManager::getInstance().Info("🚀 ========================================");
        
        bool all_passed = true;
        
        if (!setupTestEnvironment()) return false;
        if (!testLoadTargetsFromDatabase()) all_passed = false;
        if (!testAddDataToRedis()) all_passed = false;          // ✅ Redis 쓰기
        if (!testReadDataFromRedis()) all_passed = false;       // ✅ Redis 읽기
        if (!testSendAlarmToTarget()) all_passed = false;       // ✅ 알람 전송
        if (!testVerifyTransmittedData()) all_passed = false;   // ✅ 데이터 검증 (수정됨)
        
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
    // =========================================================================
    // 초기화 헬퍼 함수들
    // =========================================================================
    
    bool setupTestDatabase() {
        try {
            // 기존 테스트 DB 삭제
            if (std::filesystem::exists(test_db_path_)) {
                std::filesystem::remove(test_db_path_);
            }
            
            // ConfigManager 설정
            auto& config = ConfigManager::getInstance();
            config.set("SQLITE_DB_PATH", test_db_path_);
            config.set("CSP_DATABASE_PATH", test_db_path_);
            config.set("DATABASE_PATH", test_db_path_);
            
            // DatabaseManager 재초기화
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
            // ✅ CSPGatewayConfig 생성 (기존 패턴 준수)
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
            
            // ✅ CSPGateway 생성 (생성자에서 자동 초기화)
            csp_gateway_ = std::make_unique<CSP::CSPGateway>(config);
            
            LogManager::getInstance().Info("✅ 1-4. CSPGateway 초기화 완료");
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("CSPGateway 초기화 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    // ✅ v5.0 수정: 타겟 이름을 URL 전체로 변경
    void insertTestTargets() {
        std::string url = "http://localhost:18080/webhook";
#ifdef HAVE_HTTPLIB
        if (mock_server_) {
            url = "http://localhost:" + std::to_string(mock_server_->getPort()) + "/webhook";
        }
#endif
        
        // 🔥 테이블 생성 먼저 수행 (CSPGateway 초기화 전이므로)
        std::string create_table_sql = R"(
            CREATE TABLE IF NOT EXISTS export_targets (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                profile_id INTEGER,
                name VARCHAR(100) NOT NULL UNIQUE,
                target_type VARCHAR(20) NOT NULL,
                description TEXT,
                is_enabled BOOLEAN DEFAULT 1,
                config TEXT NOT NULL,
                export_mode VARCHAR(20) DEFAULT 'on_change',
                export_interval INTEGER DEFAULT 0,
                batch_size INTEGER DEFAULT 100,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        )";
        
        if (!db_manager_.executeNonQuery(create_table_sql)) {
            throw std::runtime_error("테이블 생성 실패: export_targets");
        }
        
        LogManager::getInstance().Info("✅ export_targets 테이블 준비 완료");
        
        json config = {
            {"url", url}, 
            {"method", "POST"}, 
            {"content_type", "application/json"}
        };
        
        // ✅ v5.0 수정: name을 URL 전체로 변경 (타겟 조회 시 일치하도록)
        std::string sql = 
            "INSERT INTO export_targets (name, target_type, config, is_enabled) VALUES "
            "('" + url + "', 'http', '" + config.dump() + "', 1);";
            //  ^^^ URL 전체를 name으로 사용 (기존: 'test-http')
        
        if (!db_manager_.executeNonQuery(sql)) {
            throw std::runtime_error("테스트 타겟 삽입 실패");
        }
        LogManager::getInstance().Info("✅ 테스트 타겟 삽입 완료 (name: " + url + ")");
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
        LogManager::getInstance().Info("🚀 Export Gateway 통합 테스트 v5.0 시작");
        
        ExportGatewayIntegrationTest test;
        bool success = test.runAllTests();
        
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "❌ 예외 발생: " << e.what() << std::endl;
        return 1;
    }
}