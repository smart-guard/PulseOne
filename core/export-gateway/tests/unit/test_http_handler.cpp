/**
 * @file test_http_handler.cpp
 * @brief HttpTargetHandler 완전한 단위 테스트
 * @author PulseOne Development Team
 * @date 2025-12-02
 * @version 1.0.0
 * 
 * 🎯 테스트 목표:
 * [기본 HTTP 메서드] (4개)
 * - POST/GET/PUT 요청
 * - 응답 처리
 * 
 * [인증 방식] (3개)
 * - Basic Auth
 * - Bearer Token
 * - API Key (헤더)
 * 
 * [고급 기능] (4개)
 * - 커스텀 헤더
 * - Body 템플릿
 * - 재시도 로직
 * - 타임아웃
 * 
 * [에러 처리] (3개)
 * - HTTP 4xx 에러
 * - HTTP 5xx 에러
 * - 연결 실패
 * 
 * [통계 및 기타] (2개)
 * - 통계 정확도
 * - 설정 검증
 * 
 * 📋 Mock 서버 필요:
 * python3 mock_http_server.py &
 */

#include "CSP/HttpTargetHandler.h"
#include "Utils/LogManager.h"
#include "Export/ExportTypes.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>

using namespace PulseOne::CSP;
using json = nlohmann::json;

// ═══════════════════════════════════════════════════════════════════════════
// 테스트 헬퍼
// ═══════════════════════════════════════════════════════════════════════════

static int test_count = 0;
static int passed_count = 0;
static int failed_count = 0;

#define TEST(name) \
    test_count++; \
    std::cout << "\n🧪 TEST_" << std::setfill('0') << std::setw(3) << test_count \
              << ": " << name << "... " << std::flush;

#define ASSERT(condition, message) \
    if (!(condition)) { \
        std::cout << "❌ FAIL: " << message << std::endl; \
        failed_count++; \
        return; \
    }

#define PASS() \
    std::cout << "✅" << std::flush; \
    passed_count++;

// Mock 서버 URL
const std::string MOCK_SERVER = "http://localhost:8765";

// 더미 AlarmMessage 생성
AlarmMessage createTestAlarm(int building_id = 1001, const std::string& point_name = "TEMP_01", double value = 25.5) {
    AlarmMessage alarm;
    alarm.bd = building_id;
    alarm.nm = point_name;
    alarm.vl = value;
    alarm.tm = "2025-12-02T10:30:45.123Z";
    alarm.al = 1;
    alarm.st = 0;
    alarm.des = "Test alarm message";
    return alarm;
}

// Mock 서버 헬스체크
bool checkMockServer() {
    HttpTargetHandler handler;
    json config = {
        {"url", MOCK_SERVER + "/health"},
        {"method", "GET"},
        {"timeout_sec", 2}
    };
    
    AlarmMessage dummy_alarm;
    auto result = handler.sendAlarm(dummy_alarm, config);
    return result.success;
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 1: 기본 HTTP 메서드 (4개)
// ═══════════════════════════════════════════════════════════════════════════

void test_post_success() {
    TEST("POST 요청 성공");
    
    HttpTargetHandler handler;
    
    json config = {
        {"name", "TEST_POST"},
        {"url", MOCK_SERVER + "/api/alarm"},
        {"method", "POST"},
        {"timeout_sec", 5}
    };
    
    AlarmMessage alarm = createTestAlarm(101, "TEMP_SENSOR", 28.5);
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "전송 실패: " + result.error_message);
    ASSERT(result.status_code == 200, "상태 코드 불일치: " + std::to_string(result.status_code));
    ASSERT(result.target_type == "HTTP", "타겟 타입 불일치");
    ASSERT(result.response_time.count() > 0, "응답 시간 0");
    
    std::cout << " [" << result.status_code << ", " << result.response_time.count() << "ms]";
    
    PASS();
}

void test_get_request() {
    TEST("GET 요청");
    
    HttpTargetHandler handler;
    
    json config = {
        {"name", "TEST_GET"},
        {"url", MOCK_SERVER + "/api/alarm"},
        {"method", "GET"},
        {"timeout_sec", 5}
    };
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "GET 요청 실패");
    ASSERT(result.status_code == 200, "상태 코드 불일치");
    
    std::cout << " [" << result.status_code << "]";
    
    PASS();
}

void test_put_request() {
    TEST("PUT 요청");
    
    HttpTargetHandler handler;
    
    json config = {
        {"name", "TEST_PUT"},
        {"url", MOCK_SERVER + "/api/alarm"},
        {"method", "PUT"},
        {"timeout_sec", 5}
    };
    
    AlarmMessage alarm = createTestAlarm(202, "PRESSURE", 105.3);
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "PUT 요청 실패");
    ASSERT(result.status_code == 200, "상태 코드 불일치");
    
    PASS();
}

void test_different_methods() {
    TEST("다양한 HTTP 메서드");
    
    HttpTargetHandler handler;
    
    std::vector<std::string> methods = {"POST", "GET", "PUT"};
    int success_count = 0;
    
    for (const auto& method : methods) {
        json config = {
            {"url", MOCK_SERVER + "/api/alarm"},
            {"method", method},
            {"timeout_sec", 5}
        };
        
        AlarmMessage alarm = createTestAlarm();
        auto result = handler.sendAlarm(alarm, config);
        
        if (result.success) {
            success_count++;
        }
    }
    
    ASSERT(success_count == 3, "모든 메서드 성공해야 함");
    
    std::cout << " [" << success_count << "/3]";
    
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 2: 인증 방식 (3개)
// ═══════════════════════════════════════════════════════════════════════════

void test_basic_auth() {
    TEST("Basic Authentication");
    
    HttpTargetHandler handler;
    
    // ✅ 수정: auth 객체 형식 사용
    json config = {
        {"name", "TEST_BASIC_AUTH"},
        {"url", MOCK_SERVER + "/auth/basic"},
        {"method", "POST"},
        {"auth", {                      // ✅ auth 객체로 래핑
            {"type", "basic"},          // auth.type
            {"username", "testuser"},   // auth.username
            {"password", "testpass"}    // auth.password
        }},
        {"timeout_sec", 5}
    };
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "Basic Auth 실패");
    ASSERT(result.status_code == 200, "상태 코드 200이어야 함");
    
    PASS();
}

void test_bearer_token() {
    TEST("Bearer Token Authentication");
    
    HttpTargetHandler handler;
    
    // ✅ 수정: auth 객체 형식 사용
    json config = {
        {"name", "TEST_BEARER"},
        {"url", MOCK_SERVER + "/auth/bearer"},
        {"method", "POST"},
        {"auth", {                          // ✅ auth 객체로 래핑
            {"type", "bearer"},             // auth.type
            {"token", "test-token-12345"}   // auth.token
        }},
        {"timeout_sec", 5}
    };
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "Bearer Token 실패");
    ASSERT(result.status_code == 200, "토큰 인증 실패");
    
    PASS();
}

void test_api_key() {
    TEST("API Key Authentication");
    
    HttpTargetHandler handler;
    
    json config = {
        {"name", "TEST_API_KEY"},
        {"url", MOCK_SERVER + "/auth/apikey"},
        {"method", "POST"},
        {"headers", {
            {"X-API-Key", "secret-api-key-xyz"}
        }},
        {"timeout_sec", 5}
    };
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "API Key 실패");
    ASSERT(result.status_code == 200, "API Key 인증 실패");
    
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 3: 고급 기능 (4개)
// ═══════════════════════════════════════════════════════════════════════════

void test_custom_headers() {
    TEST("커스텀 헤더");
    
    HttpTargetHandler handler;
    
    json config = {
        {"name", "TEST_HEADERS"},
        {"url", MOCK_SERVER + "/headers/echo"},
        {"method", "POST"},
        {"headers", {
            {"X-Custom-Header", "CustomValue123"},
            {"X-Request-ID", "req-test-001"}
        }},
        {"timeout_sec", 5}
    };
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "커스텀 헤더 전송 실패");
    ASSERT(result.status_code == 200, "상태 코드 불일치");
    
    PASS();
}

void test_body_template() {
    TEST("Body 템플릿");
    
    HttpTargetHandler handler;
    
    json config = {
        {"name", "TEST_TEMPLATE"},
        {"url", MOCK_SERVER + "/api/alarm"},
        {"method", "POST"},
        {"body_template", {
            {"event_type", "alarm"},
            {"severity", "high"},
            {"building_id", "{building_id}"},
            {"point_name", "{point_name}"},
            {"custom_field", "static_value"}
        }},
        {"timeout_sec", 5}
    };
    
    AlarmMessage alarm = createTestAlarm(303, "FLOW_METER", 75.2);
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(result.success, "Body 템플릿 전송 실패");
    ASSERT(result.status_code == 200, "상태 코드 불일치");
    
    PASS();
}

void test_retry_logic() {
    TEST("재시도 로직");
    
    HttpTargetHandler handler;
    
    // 500 에러는 재시도해야 함
    // max_retry: 3 = 총 4번 시도 (초기 1 + 재시도 3)
    json config = {
        {"name", "TEST_RETRY"},
        {"url", MOCK_SERVER + "/status/500"},
        {"method", "POST"},
        {"max_retry", 3},           // 총 4번 시도 (0, 1, 2, 3)
        {"retry_delay_ms", 100},    // 초기 지연 100ms
        {"timeout_sec", 10}
    };
    
    AlarmMessage alarm = createTestAlarm();
    
    // ✅ 시간 측정으로 재시도 확인
    // 재시도 백오프: 100ms * 2^0 = 100ms (1차)
    //              100ms * 2^1 = 200ms (2차)
    //              100ms * 2^2 = 400ms (3차)
    // 총 대기 시간: ~700ms + 지터(±20%) + 요청 시간
    auto start = std::chrono::steady_clock::now();
    auto result = handler.sendAlarm(alarm, config);
    auto end = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // 500 에러는 계속 실패
    ASSERT(!result.success, "500 에러는 실패해야 함");
    ASSERT(result.status_code == 500, "상태 코드 500이어야 함");
    
    // 재시도 확인: 최소 600ms 이상 (백오프 지터 감안)
    // 4번 시도했다면 3번의 대기가 있었을 것
    ASSERT(elapsed_ms >= 600, "재시도 대기 시간 부족 (4번 시도 안 함)");
    
    std::cout << " [4번 시도, " << elapsed_ms << "ms]";
    
    PASS();
}

void test_timeout() {
    TEST("타임아웃 처리");
    
    HttpTargetHandler handler;
    
    // Mock 서버의 /delay/3은 3초 지연
    // 타임아웃을 1초로 설정, max_retry: 1 (총 2번 시도)
    json config = {
        {"name", "TEST_TIMEOUT"},
        {"url", MOCK_SERVER + "/delay/3"},
        {"method", "POST"},
        {"timeout_sec", 1},
        {"connect_timeout_sec", 1},
        {"max_retry", 1}  // ✅ retry_max_attempts → max_retry (총 2번 시도)
    };
    
    AlarmMessage alarm = createTestAlarm();
    
    auto start = std::chrono::steady_clock::now();
    auto result = handler.sendAlarm(alarm, config);
    auto end = std::chrono::steady_clock::now();
    
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    ASSERT(!result.success, "타임아웃인데 성공");
    // 1초 타임아웃 × 2번 시도 + 재시도 대기 ≈ 3.5초 이내
    ASSERT(elapsed.count() < 5000, "타임아웃 시간 초과");
    
    std::cout << " [" << elapsed.count() << "ms]";
    
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 4: 에러 처리 (3개)
// ═══════════════════════════════════════════════════════════════════════════

void test_http_4xx_error() {
    TEST("HTTP 4xx 에러");
    
    HttpTargetHandler handler;
    
    json config = {
        {"name", "TEST_400"},
        {"url", MOCK_SERVER + "/status/400"},
        {"method", "POST"},
        {"timeout_sec", 5}
    };
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(!result.success, "400 에러인데 성공");
    ASSERT(result.status_code == 400, "상태 코드 불일치");
    
    std::cout << " [" << result.status_code << "]";
    
    PASS();
}

void test_http_5xx_error() {
    TEST("HTTP 5xx 에러");
    
    HttpTargetHandler handler;
    
    json config = {
        {"name", "TEST_500"},
        {"url", MOCK_SERVER + "/status/500"},
        {"method", "POST"},
        {"timeout_sec", 5}
    };
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(!result.success, "500 에러인데 성공");
    ASSERT(result.status_code == 500, "상태 코드 불일치");
    
    std::cout << " [" << result.status_code << "]";
    
    PASS();
}

void test_connection_refused() {
    TEST("연결 거부");
    
    HttpTargetHandler handler;
    
    // 존재하지 않는 포트 (유효한 범위 내)
    json config = {
        {"name", "TEST_CONNECTION_REFUSED"},
        {"url", "http://localhost:19999/nonexistent"},
        {"method", "POST"},
        {"timeout_sec", 2},
        {"connect_timeout_sec", 1},
        {"max_retry", 1}  // ✅ retry_max_attempts → max_retry (총 2번 시도)
    };
    
    AlarmMessage alarm = createTestAlarm();
    auto result = handler.sendAlarm(alarm, config);
    
    ASSERT(!result.success, "연결 거부인데 성공");
    ASSERT(!result.error_message.empty(), "에러 메시지 없음");
    
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 5: 통계 및 기타 (2개)
// ═══════════════════════════════════════════════════════════════════════════

void test_statistics() {
    TEST("통계 정확도");
    
    HttpTargetHandler handler;
    
    json config = {
        {"url", MOCK_SERVER + "/api/alarm"},
        {"method", "POST"},
        {"timeout_sec", 5}
    };
    
    // 5번 전송 (3번 성공, 2번 실패 예상)
    int success_count = 0;
    int failure_count = 0;
    
    for (int i = 0; i < 3; i++) {
        AlarmMessage alarm = createTestAlarm(1000 + i);
        auto result = handler.sendAlarm(alarm, config);
        if (result.success) success_count++;
        else failure_count++;
    }
    
    // 실패 테스트 (404)
    json fail_config = {
        {"url", MOCK_SERVER + "/status/404"},
        {"method", "POST"},
        {"timeout_sec", 5}
    };
    
    for (int i = 0; i < 2; i++) {
        AlarmMessage alarm = createTestAlarm();
        auto result = handler.sendAlarm(alarm, fail_config);
        if (!result.success) failure_count++;
    }
    
    // 통계 확인
    json status = handler.getStatus();
    ASSERT(status["type"] == "HTTP", "타입 불일치");
    ASSERT(status["request_count"].get<int>() == 5, "요청 카운트 불일치");
    ASSERT(status["success_count"].get<int>() >= 3, "성공 카운트 부족");
    ASSERT(status["failure_count"].get<int>() >= 2, "실패 카운트 부족");
    
    std::cout << " [성공:" << status["success_count"] 
              << ", 실패:" << status["failure_count"] << "]";
    
    PASS();
}

void test_config_validation() {
    TEST("설정 검증");
    
    HttpTargetHandler handler;
    std::vector<std::string> errors;
    
    // ❌ URL 없음
    json config1 = {{"method", "POST"}};
    bool valid1 = handler.validateConfig(config1, errors);
    ASSERT(!valid1, "URL 없는데 통과");
    ASSERT(!errors.empty(), "에러 메시지 없음");
    
    // ❌ 잘못된 URL
    errors.clear();
    json config2 = {
        {"url", "not-a-valid-url"},
        {"method", "POST"}
    };
    bool valid2 = handler.validateConfig(config2, errors);
    ASSERT(!valid2, "잘못된 URL인데 통과");
    
    // ✅ 정상 설정
    errors.clear();
    json config3 = {
        {"url", "http://example.com/api"},
        {"method", "POST"}
    };
    bool valid3 = handler.validateConfig(config3, errors);
    ASSERT(valid3, "정상 설정인데 실패");
    ASSERT(errors.empty(), "정상인데 에러");
    
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 메인
// ═══════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    std::cout << "  HttpTargetHandler 완전한 단위 테스트\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    
    auto& logger = LogManager::getInstance();
    logger.Info("HttpTargetHandler 테스트 시작");
    
    // Mock 서버 체크
    std::cout << "\n🔍 Mock HTTP 서버 확인 중... " << std::flush;
    if (!checkMockServer()) {
        std::cout << "❌\n";
        std::cerr << "\n💥 에러: Mock HTTP 서버가 실행되지 않았습니다!\n";
        std::cerr << "다음 명령어로 서버를 시작하세요:\n";
        std::cerr << "  python3 mock_http_server.py &\n\n";
        return 1;
    }
    std::cout << "✅\n";
    
    // 파트 1: 기본 HTTP 메서드 (4개)
    test_post_success();
    test_get_request();
    test_put_request();
    test_different_methods();
    
    // 파트 2: 인증 방식 (3개)
    test_basic_auth();
    test_bearer_token();
    test_api_key();
    
    // 파트 3: 고급 기능 (4개)
    test_custom_headers();
    test_body_template();
    test_retry_logic();
    test_timeout();
    
    // 파트 4: 에러 처리 (3개)
    test_http_4xx_error();
    test_http_5xx_error();
    test_connection_refused();
    
    // 파트 5: 통계 및 기타 (2개)
    test_statistics();
    test_config_validation();
    
    // 최종 결과
    std::cout << "\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    if (failed_count == 0) {
        std::cout << "  🎉 결과: " << passed_count << "/" << test_count << " passed - PERFECT! 🎉\n";
    } else {
        std::cout << "  ⚠️  결과: " << passed_count << "/" << test_count << " passed";
        std::cout << " (" << failed_count << " failed) ⚠️\n";
    }
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    
    logger.Info("HttpTargetHandler 테스트 완료");
    
    return (failed_count == 0) ? 0 : 1;
}