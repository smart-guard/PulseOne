#!/bin/bash
# =============================================================================
# setup_tests.sh
# Export Gateway 테스트 환경 원클릭 설치
# =============================================================================
# 사용법:
#   chmod +x setup_tests.sh
#   ./setup_tests.sh
# =============================================================================

set -e

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# =============================================================================
# 유틸리티 함수
# =============================================================================

log_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

log_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

log_warn() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

log_error() {
    echo -e "${RED}❌ $1${NC}"
}

log_step() {
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}📋 $1${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

# =============================================================================
# 사전 요구사항 체크
# =============================================================================

check_prerequisites() {
    log_step "STEP 1: 사전 요구사항 체크"
    
    # 현재 디렉토리 확인
    if [ ! -f "Makefile" ]; then
        log_error "Makefile이 없습니다. export-gateway 디렉토리에서 실행하세요!"
        exit 1
    fi
    log_success "올바른 디렉토리 확인됨"
    
    # Redis 확인
    if command -v redis-cli &> /dev/null; then
        log_success "Redis CLI 설치됨"
        if redis-cli ping &> /dev/null; then
            log_success "Redis 서버 실행 중"
        else
            log_warn "Redis 서버가 실행 중이지 않습니다"
            log_info "테스트 실행 전에 'sudo service redis-server start' 실행 필요"
        fi
    else
        log_warn "Redis CLI 미설치 (테스트 실행에 필요)"
    fi
    
    # 컴파일러 확인
    if command -v g++ &> /dev/null; then
        log_success "g++ 컴파일러 확인됨"
    else
        log_error "g++ 컴파일러가 없습니다!"
        exit 1
    fi
    
    echo ""
}

# =============================================================================
# 디렉토리 생성
# =============================================================================

create_directories() {
    log_step "STEP 2: 디렉토리 구조 생성"
    
    mkdir -p tests
    log_success "tests/ 디렉토리 생성"
    
    mkdir -p scripts
    log_success "scripts/ 디렉토리 생성"
    
    mkdir -p docs
    log_success "docs/ 디렉토리 생성"
    
    echo ""
}

# =============================================================================
# 통합 테스트 코드 생성
# =============================================================================

create_integration_test() {
    log_step "STEP 3: 통합 테스트 코드 생성"
    
    cat > tests/test_integration.cpp << 'TESTCODE_EOF'
// =============================================================================
// core/export-gateway/tests/test_integration.cpp
// Export Gateway 완전 통합 테스트 (E2E)
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

// httplib for mock server
#ifdef HAVE_HTTPLIB
#include <httplib.h>
#endif

using json = nlohmann::json;
using namespace PulseOne::CSP;

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
    
    void start() {
        if (running_.load()) return;
        running_.store(true);
        server_thread_ = std::thread([this]() {
            LogManager::getInstance().Info("🚀 Mock 서버 시작: http://localhost:" + 
                                          std::to_string(port_));
            server_->listen("0.0.0.0", port_);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    void stop() {
        if (!running_.load()) return;
        running_.store(false);
        server_->stop();
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
    std::unique_ptr<CSPGateway> csp_gateway_;
#ifdef HAVE_HTTPLIB
    std::unique_ptr<MockTargetServer> mock_server_;
#endif
    std::string test_db_path_;
    json original_redis_data_;
    
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
            LogManager::getInstance().Info("✅ 테스트 환경 준비 완료");
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("테스트 환경 준비 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testLoadTargetsFromDatabase() {
        LogManager::getInstance().Info("📋 STEP 2: DB에서 타겟 로드 테스트");
        try {
            insertTestTargets();
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
            original_redis_data_ = alarm_data;
            std::string redis_key = "alarm:test:1001";
            bool redis_ok = redis_client_->set(redis_key, alarm_data.dump());
            TestHelper::assertCondition(redis_ok, "Redis에 데이터 저장 성공");
            LogManager::getInstance().Info("✅ Redis 데이터 추가 완료");
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Redis 데이터 추가 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testReadDataFromRedis() {
        LogManager::getInstance().Info("📋 STEP 4: Redis에서 데이터 읽기");
        try {
            std::string redis_key = "alarm:test:1001";
            
            // ✅ 수정: get()은 std::string을 반환
            std::string redis_value = redis_client_->get(redis_key);
            
            TestHelper::assertCondition(!redis_value.empty(), "Redis 데이터 읽기 성공");
            
            json read_data = json::parse(redis_value);
            TestHelper::assertCondition(read_data["bd"] == 1001, "Building ID 일치");
            TestHelper::assertCondition(read_data["nm"] == "TEST_TEMP_SENSOR", "Point Name 일치");
            TestHelper::assertCondition(read_data["vl"] == 25.5, "Value 일치");
            LogManager::getInstance().Info("✅ Redis 데이터 읽기 검증 완료");
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Redis 읽기 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testSendAlarmToTarget() {
        LogManager::getInstance().Info("📋 STEP 5: 알람 전송 테스트");
        try {
            AlarmMessage alarm;
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
    
    bool testVerifyTransmittedData() {
        LogManager::getInstance().Info("📋 STEP 6: 전송 데이터 검증");
#ifdef HAVE_HTTPLIB
        try {
            auto received_list = mock_server_->getReceivedData();
            TestHelper::assertCondition(!received_list.empty(), "데이터 수신 확인");
            const json& received = received_list[0];
            
            LogManager::getInstance().Info("📊 원본: " + original_redis_data_.dump());
            LogManager::getInstance().Info("📊 전송: " + received.dump());
            
            TestHelper::assertCondition(received["bd"] == original_redis_data_["bd"], "BD 일치");
            TestHelper::assertCondition(received["nm"] == original_redis_data_["nm"], "NM 일치");
            TestHelper::assertCondition(
                std::abs(received["vl"].get<double>() - original_redis_data_["vl"].get<double>()) < 0.01,
                "VL 일치");
            TestHelper::assertCondition(received["al"] == original_redis_data_["al"], "AL 일치");
            TestHelper::assertCondition(received["st"] == original_redis_data_["st"], "ST 일치");
            
            LogManager::getInstance().Info("✅ 데이터 검증 완료 - 완벽히 일치!");
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
        LogManager::getInstance().Info("🚀 Export Gateway 통합 테스트 시작");
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
            if (!db_manager_.initialize()) return false;
            
            std::string create_sql = R"(
                CREATE TABLE IF NOT EXISTS export_targets (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    name TEXT UNIQUE NOT NULL,
                    type TEXT NOT NULL,
                    config TEXT NOT NULL,
                    is_enabled INTEGER DEFAULT 1,
                    priority INTEGER DEFAULT 100,
                    description TEXT,
                    success_count INTEGER DEFAULT 0,
                    failure_count INTEGER DEFAULT 0,
                    avg_response_time_ms REAL DEFAULT 0.0,
                    last_success DATETIME,
                    last_failure DATETIME,
                    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                    is_deleted INTEGER DEFAULT 0
                )
            )";
            return db_manager_.executeNonQuery(create_sql);
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("DB 초기화 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool setupRedisConnection() {
        try {
            redis_client_ = std::make_unique<RedisClientImpl>();
            if (!redis_client_->connect("127.0.0.1", 6379, "")) {
                LogManager::getInstance().Error("Redis 연결 실패");
                return false;
            }
            LogManager::getInstance().Info("✅ Redis 연결 성공");
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
            mock_server_->start();
            LogManager::getInstance().Info("✅ Mock 서버 시작");
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Mock 서버 실패: " + std::string(e.what()));
            return false;
        }
    }
#endif
    
    bool setupCSPGateway() {
        try {
            CSPGatewayConfig config;
            config.building_id = "1001";
            config.use_dynamic_targets = true;
#ifdef HAVE_HTTPLIB
            config.api_endpoint = "http://localhost:" + std::to_string(mock_server_->getPort());
#else
            config.api_endpoint = "http://localhost:18080";
#endif
            csp_gateway_ = std::make_unique<CSPGateway>(config);
            LogManager::getInstance().Info("✅ CSPGateway 초기화");
            return true;
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("CSPGateway 초기화 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    void insertTestTargets() {
        std::string url = "http://localhost:18080/webhook";
#ifdef HAVE_HTTPLIB
        if (mock_server_) {
            url = "http://localhost:" + std::to_string(mock_server_->getPort()) + "/webhook";
        }
#endif
        json config = {{"url", url}, {"method", "POST"}, {"content_type", "application/json"}};
        std::string sql = "INSERT INTO export_targets (name, type, config, is_enabled) VALUES "
                         "('test-http', 'http', '" + config.dump() + "', 1);";
        db_manager_.executeNonQuery(sql);
    }
    
    void cleanup() {
        LogManager::getInstance().Info("🧹 정리 중...");
#ifdef HAVE_HTTPLIB
        if (mock_server_) mock_server_->stop();
#endif
        if (redis_client_) redis_client_->disconnect();
        try {
            if (std::filesystem::exists(test_db_path_)) {
                std::filesystem::remove(test_db_path_);
            }
        } catch (...) {}
    }
};

// =============================================================================
// main
// =============================================================================

int main(int /* argc */, char** /* argv */) {
    try {
        LogManager::getInstance().Info("Export Gateway 통합 테스트 시작");
        ExportGatewayIntegrationTest test;
        bool success = test.runAllTests();
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "예외: " << e.what() << std::endl;
        return 1;
    }
}
TESTCODE_EOF

    log_success "tests/test_integration.cpp 생성 완료"
    echo ""
}

# =============================================================================
# 테스트 실행 스크립트 생성
# =============================================================================

create_test_runner() {
    log_step "STEP 4: 테스트 실행 스크립트 생성"
    
    cat > scripts/run_integration_tests.sh << 'RUNNER_EOF'
#!/bin/bash
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}ℹ️  $1${NC}"; }
log_success() { echo -e "${GREEN}✅ $1${NC}"; }
log_error() { echo -e "${RED}❌ $1${NC}"; }

check_prerequisites() {
    log_info "사전 요구사항 체크..."
    if ! redis-cli ping > /dev/null 2>&1; then
        log_error "Redis가 실행되지 않음!"
        log_info "Redis 시작: sudo service redis-server start"
        exit 1
    fi
    log_success "Redis 실행 중"
    
    if [ ! -f "bin/tests/test_integration" ]; then
        log_error "테스트 바이너리 없음!"
        log_info "먼저 빌드: make tests"
        exit 1
    fi
    log_success "테스트 바이너리 존재"
}

setup_test_environment() {
    log_info "테스트 환경 초기화..."
    redis-cli DEL "alarm:test:1001" > /dev/null 2>&1 || true
    rm -f /tmp/test_export_gateway.db 2>/dev/null || true
    log_success "환경 초기화 완료"
}

run_integration_tests() {
    log_info "=========================================="
    log_info "통합 테스트 실행"
    log_info "=========================================="
    
    if ./bin/tests/test_integration; then
        log_success "=========================================="
        log_success "모든 테스트 통과! 🎉"
        log_success "=========================================="
        return 0
    else
        log_error "=========================================="
        log_error "테스트 실패!"
        log_error "=========================================="
        return 1
    fi
}

cleanup_test_environment() {
    log_info "정리 중..."
    redis-cli DEL "alarm:test:1001" > /dev/null 2>&1 || true
    rm -f /tmp/test_export_gateway.db 2>/dev/null || true
    log_success "정리 완료"
}

main() {
    echo ""
    log_info "🚀 Export Gateway 통합 테스트 러너"
    echo ""
    
    check_prerequisites
    setup_test_environment
    
    if run_integration_tests; then
        TEST_RESULT=0
    else
        TEST_RESULT=1
    fi
    
    cleanup_test_environment
    
    echo ""
    if [ $TEST_RESULT -eq 0 ]; then
        log_success "✨ 테스트 완료 - 모든 테스트 통과!"
        exit 0
    else
        log_error "💥 테스트 실패"
        exit 1
    fi
}

main
RUNNER_EOF

    chmod +x scripts/run_integration_tests.sh
    log_success "scripts/run_integration_tests.sh 생성 및 실행 권한 부여"
    echo ""
}

# =============================================================================
# Makefile 업데이트
# =============================================================================

update_makefile() {
    log_step "STEP 5: Makefile 업데이트"
    
    # Makefile 백업
    if [ -f "Makefile" ]; then
        cp Makefile Makefile.backup
        log_success "Makefile 백업 생성 (Makefile.backup)"
    fi
    
    # 테스트 타겟이 이미 있는지 확인
    if grep -q "TEST_DIR := tests" Makefile 2>/dev/null; then
        log_warn "Makefile에 이미 테스트 타겟이 있습니다 (건너뜀)"
    else
        cat >> Makefile << 'MAKEFILE_EOF'

# =============================================================================
# 테스트 설정 (setup_tests.sh로 자동 추가됨)
# =============================================================================

TEST_DIR := tests
TEST_BUILD_DIR := build/tests
TEST_BIN_DIR := bin/tests

TEST_SOURCES := $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJECTS := $(TEST_SOURCES:$(TEST_DIR)/%.cpp=$(TEST_BUILD_DIR)/%.o)
TEST_TARGETS := $(TEST_SOURCES:$(TEST_DIR)/%.cpp=$(TEST_BIN_DIR)/%)

# CSP 관련 소스 파일들 (테스트에 필요)
CSP_SOURCES := $(wildcard src/CSP/*.cpp)
CSP_OBJECTS := $(CSP_SOURCES:src/%.cpp=build/%.o)

# Export 관련 소스 파일들
EXPORT_SOURCES := $(wildcard src/Export/*.cpp)
EXPORT_OBJECTS := $(EXPORT_SOURCES:src/%.cpp=build/%.o)

.PHONY: tests
tests: $(TEST_BIN_DIR) $(TEST_TARGETS)
	@echo -e "✅ All tests built successfully"

$(TEST_BIN_DIR) $(TEST_BUILD_DIR):
	@mkdir -p $@

# 통합 테스트 빌드 - 모든 필요한 오브젝트 파일 포함
$(TEST_BIN_DIR)/test_integration: $(TEST_BUILD_DIR)/test_integration.o $(CSP_OBJECTS) $(EXPORT_OBJECTS)
	@echo -e "🔗 Linking integration test..."
	@echo -e "  Objects: $(words $(CSP_OBJECTS)) CSP + $(words $(EXPORT_OBJECTS)) Export"
	@$(CXX) $^ -o $@ $(LDFLAGS) $(SHARED_LIBS) -pthread
	@echo -e "✅ Integration test built: $@"

$(TEST_BUILD_DIR)/%.o: $(TEST_DIR)/%.cpp | $(TEST_BUILD_DIR)
	@echo -e "⚙️  Compiling test $<..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

.PHONY: test-integration
test-integration: $(TEST_BIN_DIR)/test_integration
	@echo -e "🧪 Running integration tests..."
	@$(TEST_BIN_DIR)/test_integration

.PHONY: test
test: tests
	@echo -e "🧪 Running all tests..."
	@for test in $(TEST_TARGETS); do \
		echo -e "Running $test..."; \
		$test || exit 1; \
	done
	@echo -e "✅ All tests passed!"

.PHONY: clean-tests
clean-tests:
	@rm -rf $(TEST_BUILD_DIR) $(TEST_BIN_DIR)


MAKEFILE_EOF
        log_success "Makefile에 테스트 타겟 추가 완료"
    fi
    echo ""
}

# =============================================================================
# README 문서 생성
# =============================================================================

create_readme() {
    log_step "STEP 6: 테스트 가이드 문서 생성"
    
    cat > docs/TEST_GUIDE.md << 'README_EOF'
# 🧪 Export Gateway 통합 테스트 가이드

## 빠른 시작

```bash
# 1. Redis 시작
sudo service redis-server start

# 2. 빌드
make clean && make all && make tests

# 3. 테스트 실행
./scripts/run_integration_tests.sh
```

## 테스트 시나리오

| # | 테스트 | 검증 내용 |
|---|--------|----------|
| 1 | DB 타겟 로드 | export_targets 테이블에서 설정 읽기 |
| 2 | Redis 쓰기 | 테스트 데이터 저장 |
| 3 | Redis 읽기 | 저장된 데이터 읽기 |
| 4 | 포맷 변환 | Redis → 타겟 형식 변환 |
| 5 | HTTP 전송 | Mock 서버에 전송 |
| 6 | 데이터 검증 | 원본 vs 전송 데이터 일치 확인 |

## 문제 해결

### Redis 연결 실패
```bash
sudo service redis-server start
redis-cli ping
```

### 컴파일 에러
```bash
cd /app/core/shared
make clean && make all

cd /app/core/export-gateway
make clean && make all && make tests
```

## Make 타겟

- `make tests` - 테스트 빌드
- `make test` - 모든 테스트 실행
- `make test-integration` - 통합 테스트만 실행
- `make clean-tests` - 테스트 파일 정리
README_EOF

    log_success "docs/TEST_GUIDE.md 생성 완료"
    echo ""
}

# =============================================================================
# 요약 및 다음 단계 안내
# =============================================================================

print_summary() {
    log_step "설치 완료!"
    
    echo ""
    log_success "✨ 테스트 환경이 성공적으로 구축되었습니다!"
    echo ""
    
    log_info "생성된 파일:"
    echo "  ✅ tests/test_integration.cpp"
    echo "  ✅ scripts/run_integration_tests.sh"
    echo "  ✅ docs/TEST_GUIDE.md"
    echo "  ✅ Makefile (테스트 타겟 추가)"
    echo ""
    
    log_info "다음 단계:"
    echo ""
    echo "  1️⃣  Redis 시작:"
    echo "      ${GREEN}sudo service redis-server start${NC}"
    echo ""
    echo "  2️⃣  테스트 빌드:"
    echo "      ${GREEN}make clean && make all && make tests${NC}"
    echo ""
    echo "  3️⃣  테스트 실행:"
    echo "      ${GREEN}./scripts/run_integration_tests.sh${NC}"
    echo ""
    echo "  또는 한 번에:"
    echo "      ${GREEN}make clean && make all && make tests && ./scripts/run_integration_tests.sh${NC}"
    echo ""
    
    log_info "도움말:"
    echo "  📖 자세한 가이드: cat docs/TEST_GUIDE.md"
    echo "  🔧 Make 타겟: make help"
    echo ""
}

# =============================================================================
# 메인 실행
# =============================================================================

main() {
    clear
    echo ""
    log_info "╔════════════════════════════════════════════════╗"
    log_info "║   Export Gateway 테스트 환경 자동 설치         ║"
    log_info "╚════════════════════════════════════════════════╝"
    echo ""
    
    check_prerequisites
    create_directories
    create_integration_test
    create_test_runner
    update_makefile
    create_readme
    print_summary
    
    log_success "🎉 모든 작업이 완료되었습니다!"
    echo ""
}

# 실행
main "$@"