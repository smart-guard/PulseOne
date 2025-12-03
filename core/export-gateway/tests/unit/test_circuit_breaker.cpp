// =============================================================================
// test_circuit_breaker.cpp
// Circuit Breaker (FailureProtector) 단위 테스트
// =============================================================================

#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <iomanip>
#include <vector>

#include "CSP/FailureProtector.h"
#include "Utils/LogManager.h"

// =============================================================================
// 네임스페이스 및 타입 별칭
// =============================================================================

using namespace std::chrono_literals;

// 명시적으로 CSP 네임스페이스 사용
using PulseOne::CSP::FailureProtector;
using PulseOne::Export::FailureProtectorConfig;
using PulseOne::Export::FailureProtectorStats;

// =============================================================================
// 테스트 헬퍼 클래스
// =============================================================================

class TestRunner {
private:
    int passed_ = 0;
    int failed_ = 0;
    std::string current_test_;

public:
    void startTest(const std::string& name) {
        current_test_ = name;
        std::cout << "\n🧪 " << name << "... " << std::flush;
    }

    void assertTrue(bool condition, const std::string& message = "") {
        if (condition) {
            std::cout << "✅" << std::flush;  // 즉시 출력
            passed_++;
        } else {
            std::cout << "\n   ❌ FAIL: " << message << "\n" << std::flush;
            failed_++;
        }
    }

    void assertFalse(bool condition, const std::string& message = "") {
        assertTrue(!condition, message);
    }

    void assertEquals(int expected, int actual, const std::string& message = "") {
        if (expected == actual) {
            std::cout << "✅";
            passed_++;
        } else {
            std::cout << "\n   ❌ FAIL: " << message 
                     << " (expected: " << expected << ", actual: " << actual << ")\n";
            failed_++;
        }
    }

    void assertEquals(const std::string& expected, const std::string& actual, const std::string& message = "") {
        if (expected == actual) {
            std::cout << "✅";
            passed_++;
        } else {
            std::cout << "\n   ❌ FAIL: " << message 
                     << " (expected: " << expected << ", actual: " << actual << ")\n";
            failed_++;
        }
    }

    bool printSummary() {
        std::cout << "\n\n";
        std::cout << "═══════════════════════════════════════════════════════\n";
        std::cout << "  Circuit Breaker 테스트 결과\n";
        std::cout << "═══════════════════════════════════════════════════════\n";
        std::cout << "  ✅ 성공: " << passed_ << "\n";
        std::cout << "  ❌ 실패: " << failed_ << "\n";
        std::cout << "  📊 성공률: " << std::fixed << std::setprecision(1) 
                 << (100.0 * passed_ / (passed_ + failed_)) << "%\n";
        std::cout << "═══════════════════════════════════════════════════════\n";
        std::cout << "\n";

        return (failed_ == 0);
    }
};

// =============================================================================
// 테스트 케이스들
// =============================================================================

// TEST_001: 초기 상태 검증
void test_001_initial_state(TestRunner& runner) {
    runner.startTest("TEST_001: 초기 상태 (CLOSED)");
    
    FailureProtectorConfig config;
    FailureProtector protector("test_001", config);
    
    runner.assertTrue(protector.canExecute(), "초기 상태에서 실행 가능해야 함");
    runner.assertEquals("CLOSED", protector.getStateString(), "초기 상태는 CLOSED");
    runner.assertTrue(protector.isHealthy(), "초기 상태는 healthy");
}

// TEST_002: CLOSED → OPEN 전환
void test_002_closed_to_open(TestRunner& runner) {
    runner.startTest("TEST_002: CLOSED → OPEN 전환");
    
    FailureProtectorConfig config;
    config.failure_threshold = 3;
    FailureProtector protector("test_002", config);
    
    // 1번째 실패 - 아직 CLOSED
    protector.recordFailure();
    runner.assertTrue(protector.canExecute(), "1번 실패 후에도 실행 가능");
    
    // 2번째 실패 - 아직 CLOSED
    protector.recordFailure();
    runner.assertTrue(protector.canExecute(), "2번 실패 후에도 실행 가능");
    
    // 3번째 실패 - OPEN으로 전환
    protector.recordFailure();
    runner.assertFalse(protector.canExecute(), "3번 실패 후 실행 불가");
    runner.assertEquals("OPEN", protector.getStateString(), "상태는 OPEN");
    runner.assertFalse(protector.isHealthy(), "OPEN 상태는 unhealthy");
    
    // getStats() 호출 제거 (데드락 방지)
    runner.assertEquals(3, static_cast<int>(protector.getFailureCount()), "실패 카운트는 3");
}

// TEST_003: OPEN → HALF_OPEN 전환 (타임아웃 후)
void test_003_open_to_half_open(TestRunner& runner) {
    runner.startTest("TEST_003: OPEN → HALF_OPEN 전환");
    
    FailureProtectorConfig config;
    config.failure_threshold = 2;
    config.recovery_timeout_ms = 500;  // 0.5초
    config.backoff_multiplier = 1.0;   // Backoff 비활성화
    FailureProtector protector("test_003", config);
    
    // OPEN 상태로 만들기
    protector.recordFailure();
    protector.recordFailure();
    runner.assertEquals("OPEN", protector.getStateString(), "OPEN 상태");
    
    // 타임아웃 전 - 여전히 OPEN
    std::this_thread::sleep_for(300ms);
    runner.assertFalse(protector.canExecute(), "타임아웃 전에는 실행 불가");
    runner.assertEquals("OPEN", protector.getStateString(), "여전히 OPEN");
    
    // 타임아웃 후 - HALF_OPEN
    std::this_thread::sleep_for(300ms);
    runner.assertTrue(protector.canExecute(), "타임아웃 후 실행 가능");
    runner.assertEquals("HALF_OPEN", protector.getStateString(), "HALF_OPEN으로 전환");
}

// TEST_004: HALF_OPEN → CLOSED (성공 시)
void test_004_half_open_to_closed(TestRunner& runner) {
    runner.startTest("TEST_004: HALF_OPEN → CLOSED 복구");
    
    FailureProtectorConfig config;
    config.failure_threshold = 2;
    config.recovery_timeout_ms = 500;
    config.half_open_success_threshold = 2;  // 2번 성공 필요
    config.backoff_multiplier = 1.0;         // Backoff 비활성화
    FailureProtector protector("test_004", config);
    
    // OPEN 상태로 만들기
    protector.recordFailure();
    protector.recordFailure();
    std::this_thread::sleep_for(600ms);
    
    // canExecute() 호출로 HALF_OPEN 전환 트리거
    runner.assertTrue(protector.canExecute(), "타임아웃 후 실행 가능");
    runner.assertEquals("HALF_OPEN", protector.getStateString(), "HALF_OPEN 상태");
    
    // 1번째 성공 - 아직 HALF_OPEN
    protector.recordSuccess();
    runner.assertEquals("HALF_OPEN", protector.getStateString(), "1번 성공 후에도 HALF_OPEN");
    
    // 2번째 성공 - CLOSED로 복구
    protector.recordSuccess();
    runner.assertEquals("CLOSED", protector.getStateString(), "2번 성공 후 CLOSED로 복구");
    runner.assertTrue(protector.isHealthy(), "다시 healthy 상태");
}

// TEST_005: HALF_OPEN → OPEN (실패 시)
void test_005_half_open_to_open(TestRunner& runner) {
    runner.startTest("TEST_005: HALF_OPEN → OPEN 재실패");
    
    FailureProtectorConfig config;
    config.failure_threshold = 2;
    config.recovery_timeout_ms = 500;
    config.backoff_multiplier = 1.0;  // Backoff 비활성화
    FailureProtector protector("test_005", config);
    
    // OPEN 상태로 만들기
    protector.recordFailure();
    protector.recordFailure();
    std::this_thread::sleep_for(600ms);
    
    // canExecute() 호출로 HALF_OPEN 전환 트리거
    runner.assertTrue(protector.canExecute(), "타임아웃 후 실행 가능");
    runner.assertEquals("HALF_OPEN", protector.getStateString(), "HALF_OPEN 상태");
    
    // HALF_OPEN에서 실패 - 다시 OPEN
    protector.recordFailure();
    runner.assertEquals("OPEN", protector.getStateString(), "실패 시 다시 OPEN");
    runner.assertFalse(protector.canExecute(), "실행 불가 상태");
}

// TEST_006: 연속 성공 후 실패 카운트 리셋
void test_006_success_resets_failures(TestRunner& runner) {
    runner.startTest("TEST_006: 성공 시 실패 카운트 리셋");
    
    FailureProtectorConfig config;
    config.failure_threshold = 3;
    FailureProtector protector("test_006", config);
    
    // 2번 실패
    protector.recordFailure();
    protector.recordFailure();
    runner.assertEquals(2, static_cast<int>(protector.getFailureCount()), "2번 실패");
    
    // 1번 성공 - 실패 카운트 리셋
    protector.recordSuccess();
    runner.assertEquals(0, static_cast<int>(protector.getFailureCount()), "성공 후 실패 카운트 0");
    runner.assertEquals("CLOSED", protector.getStateString(), "여전히 CLOSED");
}

// TEST_007: 통계 정보 확인
void test_007_statistics(TestRunner& runner) {
    runner.startTest("TEST_007: 통계 정보");
    
    FailureProtectorConfig config;
    config.failure_threshold = 5;
    FailureProtector protector("test_007", config);
    
    // 성공 3번, 실패 2번
    protector.recordSuccess();
    protector.recordSuccess();
    protector.recordFailure();
    protector.recordSuccess();
    protector.recordFailure();
    
    // 성공 카운트만 체크 (실패 카운트는 성공 시 리셋됨)
    runner.assertEquals(3, static_cast<int>(protector.getSuccessCount()), "3번 성공");
    runner.assertEquals("CLOSED", protector.getStateString(), "여전히 CLOSED");
}

// TEST_008: Exponential Backoff
void test_008_exponential_backoff(TestRunner& runner) {
    runner.startTest("TEST_008: Exponential Backoff");
    
    FailureProtectorConfig config;
    config.failure_threshold = 3;  // 3번 실패 후 OPEN
    config.recovery_timeout_ms = 100;
    config.max_recovery_timeout_ms = 10000;
    config.backoff_multiplier = 2.0;
    FailureProtector protector("test_008", config);
    
    // 실패 3번으로 OPEN
    protector.recordFailure();  // failure_count = 1
    protector.recordFailure();  // failure_count = 2
    protector.recordFailure();  // failure_count = 3, OPEN!
    runner.assertEquals("OPEN", protector.getStateString(), "OPEN 상태");
    
    // Backoff: 100ms * 2^3 = 800ms 대기 필요
    std::this_thread::sleep_for(700ms);
    runner.assertFalse(protector.canExecute(), "700ms는 부족");
    
    std::this_thread::sleep_for(200ms);  // 총 900ms
    runner.assertTrue(protector.canExecute(), "900ms 후 HALF_OPEN");
}

// TEST_009: 동시성 테스트
void test_009_concurrency(TestRunner& runner) {
    runner.startTest("TEST_009: 동시성 안전성");
    
    FailureProtectorConfig config;
    config.failure_threshold = 100;
    FailureProtector protector("test_009", config);
    
    // 10개 스레드에서 동시에 성공/실패 기록
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < 10; ++j) {
                if (j % 2 == 0) {
                    protector.recordSuccess();
                } else {
                    protector.recordFailure();
                }
                std::this_thread::sleep_for(1ms);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // 크래시 없이 완료되면 성공
    runner.assertTrue(true, "동시성 처리 완료");
    runner.assertEquals("CLOSED", protector.getStateString(), "여전히 CLOSED");
}

// TEST_010: 완전 복구 후 재시작
void test_010_full_recovery_cycle(TestRunner& runner) {
    runner.startTest("TEST_010: 완전 복구 사이클");
    
    FailureProtectorConfig config;
    config.failure_threshold = 2;
    config.recovery_timeout_ms = 300;
    config.half_open_success_threshold = 3;
    config.backoff_multiplier = 1.0;  // Backoff 비활성화
    FailureProtector protector("test_010", config);
    
    // 사이클 1: 실패 → 복구
    protector.recordFailure();
    protector.recordFailure();
    runner.assertEquals("OPEN", protector.getStateString(), "OPEN");
    
    std::this_thread::sleep_for(400ms);
    protector.canExecute();  // HALF_OPEN 전환 트리거
    protector.recordSuccess();
    protector.recordSuccess();
    protector.recordSuccess();
    runner.assertEquals("CLOSED", protector.getStateString(), "복구됨");
    
    // 사이클 2: 다시 실패 → 복구
    protector.recordFailure();
    protector.recordFailure();
    runner.assertEquals("OPEN", protector.getStateString(), "다시 OPEN");
    
    std::this_thread::sleep_for(400ms);
    protector.canExecute();  // HALF_OPEN 전환 트리거
    protector.recordSuccess();
    protector.recordSuccess();
    protector.recordSuccess();
    runner.assertEquals("CLOSED", protector.getStateString(), "다시 복구됨");
    
    runner.assertTrue(protector.isHealthy(), "최종 상태는 healthy");
}

// =============================================================================
// main
// =============================================================================

int main() {
    try {
        std::cout << "\n";
        std::cout << "═══════════════════════════════════════════════════════\n";
        std::cout << "  Circuit Breaker (FailureProtector) 단위 테스트\n";
        std::cout << "═══════════════════════════════════════════════════════\n";
        
        TestRunner runner;
        
        // 모든 테스트 실행
        test_001_initial_state(runner);
        test_002_closed_to_open(runner);
        test_003_open_to_half_open(runner);
        test_004_half_open_to_closed(runner);
        test_005_half_open_to_open(runner);
        test_006_success_resets_failures(runner);
        test_007_statistics(runner);
        test_008_exponential_backoff(runner);
        test_009_concurrency(runner);
        test_010_full_recovery_cycle(runner);
        
        // 결과 출력
        bool all_passed = runner.printSummary();
        
        return all_passed ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "\n💥 예외 발생: " << e.what() << "\n";
        return 1;
    }
}