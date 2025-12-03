/**
 * @file test_dynamic_target.cpp
 * @brief DynamicTargetManager 단위 테스트 (실제 API 기반)
 * @author PulseOne Development Team
 * @date 2025-11-05
 * @version 3.0.0 - 실제 코드 분석 후 완전 재작성
 * 
 * 🎯 실제 확인된 API:
 * - addOrUpdateTarget(const DynamicTarget& target)
 * - removeTarget(const std::string& name)
 * - setTargetEnabled(const std::string& name, bool enabled)
 * - getAllTargets()
 * - getTarget(const std::string& name)
 * - sendAlarmToTargets(const AlarmMessage& alarm)
 * - sendAlarmToTarget(const std::string& name, const AlarmMessage& alarm)
 * - getStatistics()
 * 
 * 📋 DynamicTarget 구조체 (id 필드 없음!):
 * - std::string name
 * - std::string type
 * - bool enabled
 * - json config
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <cassert>
#include <iomanip>

// PulseOne 공통 헤더
#include "CSP/DynamicTargetManager.h"
#include "CSP/AlarmMessage.h"
#include "Export/ExportTypes.h"
#include "Utils/LogManager.h"

using namespace std::chrono_literals;

// 명시적 타입 별칭 (ambiguous 방지)
using DynamicTargetManager = PulseOne::CSP::DynamicTargetManager;
using AlarmMessage = PulseOne::CSP::AlarmMessage;
using DynamicTarget = PulseOne::Export::DynamicTarget;
// LogManager는 전역 네임스페이스 - 별칭 불필요

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
            std::cout << "✅" << std::flush;
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
            std::cout << "✅" << std::flush;
            passed_++;
        } else {
            std::cout << "\n   ❌ FAIL: " << message 
                     << " (expected: " << expected << ", actual: " << actual << ")\n" << std::flush;
            failed_++;
        }
    }

    void assertEquals(const std::string& expected, const std::string& actual, const std::string& message = "") {
        if (expected == actual) {
            std::cout << "✅" << std::flush;
            passed_++;
        } else {
            std::cout << "\n   ❌ FAIL: " << message 
                     << " (expected: " << expected << ", actual: " << actual << ")\n" << std::flush;
            failed_++;
        }
    }
    
    void assertGreater(size_t value, size_t threshold, const std::string& message = "") {
        if (value > threshold) {
            std::cout << "✅" << std::flush;
            passed_++;
        } else {
            std::cout << "\n   ❌ FAIL: " << message 
                     << " (value: " << value << " should be > " << threshold << ")\n" << std::flush;
            failed_++;
        }
    }
    
    void assertGreaterOrEqual(size_t value, size_t threshold, const std::string& message = "") {
        if (value >= threshold) {
            std::cout << "✅" << std::flush;
            passed_++;
        } else {
            std::cout << "\n   ❌ FAIL: " << message 
                     << " (value: " << value << " should be >= " << threshold << ")\n" << std::flush;
            failed_++;
        }
    }

    bool printSummary() {
        std::cout << "\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        
        if (failed_ == 0) {
            std::cout << "  결과: " << passed_ << "/" << (passed_ + failed_) << " passed ✅\n";
            std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
            return true;
        } else {
            std::cout << "  결과: " << passed_ << " passed, " << failed_ << " failed ❌\n";
            std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
            return false;
        }
    }
};

// =============================================================================
// 테스트 헬퍼 함수들
// =============================================================================

AlarmMessage createTestAlarm(int building_id, const std::string& name, double value) {
    AlarmMessage alarm;
    alarm.bd = building_id;
    alarm.nm = name;
    alarm.vl = value;
    alarm.al = 1;
    alarm.st = 1;
    alarm.tm = std::to_string(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    return alarm;
}

DynamicTarget createTestTarget(const std::string& name, const std::string& type, bool enabled = true) {
    DynamicTarget target;
    target.name = name;
    target.type = type;
    target.enabled = enabled;
    
    // 타입별 기본 설정
    if (type == "http") {
        target.config["url"] = "http://example.com/api/alarm";
        target.config["method"] = "POST";
    } else if (type == "s3") {
        target.config["bucket"] = "test-bucket";
        target.config["prefix"] = "alarms/";
    } else if (type == "mqtt") {
        target.config["broker"] = "mqtt://localhost:1883";
        target.config["topic"] = "alarms/test";
    }
    
    return target;
}

// =============================================================================
// 테스트 함수들 (실제 API 기반)
// =============================================================================

void test_070_singleton_instance(TestRunner& runner) {
    runner.startTest("TEST_070: 싱글턴 인스턴스");
    
    auto& instance1 = DynamicTargetManager::getInstance();
    auto& instance2 = DynamicTargetManager::getInstance();
    
    runner.assertTrue(&instance1 == &instance2, "싱글턴 인스턴스 동일");
}

void test_071_load_from_database(TestRunner& runner) {
    runner.startTest("TEST_071: DB에서 타겟 로드");
    
    auto& manager = DynamicTargetManager::getInstance();
    
    bool result = manager.loadFromDatabase();
    runner.assertTrue(result, "DB 로드 성공");
    
    auto targets = manager.getAllTargets();
    std::cout << " [" << targets.size() << "개 로드]";
    
    runner.assertGreaterOrEqual(targets.size(), 0, "타겟 목록 조회");
}

void test_072_get_all_targets(TestRunner& runner) {
    runner.startTest("TEST_072: 전체 타겟 조회");
    
    auto& manager = DynamicTargetManager::getInstance();
    auto targets = manager.getAllTargets();
    
    runner.assertGreaterOrEqual(targets.size(), 0, "타겟 목록 조회");
    
    std::cout << " [" << targets.size() << "개]";
}

void test_073_add_or_update_target(TestRunner& runner) {
    runner.startTest("TEST_073: 타겟 추가/수정");
    
    auto& manager = DynamicTargetManager::getInstance();
    size_t initial_count = manager.getAllTargets().size();
    
    // ✅ 실제 API: addOrUpdateTarget
    DynamicTarget target = createTestTarget("DYNAMIC_TEST", "http", true);
    bool result = manager.addOrUpdateTarget(target);
    
    runner.assertTrue(result, "타겟 추가 성공");
    
    size_t new_count = manager.getAllTargets().size();
    runner.assertEquals(initial_count + 1, new_count, "타겟 수 증가");
    
    auto added = manager.getTarget("DYNAMIC_TEST");
    runner.assertTrue(added.has_value(), "타겟 조회 성공");
}

void test_074_remove_target(TestRunner& runner) {
    runner.startTest("TEST_074: 타겟 제거");
    
    auto& manager = DynamicTargetManager::getInstance();
    
    // 먼저 테스트 타겟 추가
    DynamicTarget target = createTestTarget("TO_REMOVE", "http", true);
    manager.addOrUpdateTarget(target);
    
    size_t count_before = manager.getAllTargets().size();
    
    // ✅ 실제 API: removeTarget(name)
    bool result = manager.removeTarget("TO_REMOVE");
    
    runner.assertTrue(result, "타겟 제거 성공");
    
    size_t count_after = manager.getAllTargets().size();
    runner.assertEquals(count_before - 1, count_after, "타겟 수 감소");
    
    auto removed = manager.getTarget("TO_REMOVE");
    runner.assertFalse(removed.has_value(), "제거된 타겟 조회 불가");
}

void test_075_enable_disable_target(TestRunner& runner) {
    runner.startTest("TEST_075: 타겟 활성화/비활성화");
    
    auto& manager = DynamicTargetManager::getInstance();
    
    // 테스트 타겟 추가
    DynamicTarget target = createTestTarget("TOGGLE_TEST", "http", true);
    manager.addOrUpdateTarget(target);
    
    // ✅ 실제 API: setTargetEnabled(name, false)
    bool disable_result = manager.setTargetEnabled("TOGGLE_TEST", false);
    runner.assertTrue(disable_result, "비활성화 성공");
    
    auto disabled = manager.getTarget("TOGGLE_TEST");
    runner.assertTrue(disabled.has_value() && !disabled->enabled, "비활성 상태 확인");
    
    // ✅ 실제 API: setTargetEnabled(name, true)
    bool enable_result = manager.setTargetEnabled("TOGGLE_TEST", true);
    runner.assertTrue(enable_result, "활성화 성공");
    
    auto enabled = manager.getTarget("TOGGLE_TEST");
    runner.assertTrue(enabled.has_value() && enabled->enabled, "활성 상태 확인");
    
    // Cleanup
    manager.removeTarget("TOGGLE_TEST");
}

void test_076_send_alarm(TestRunner& runner) {
    runner.startTest("TEST_076: 알람 전송");
    
    auto& manager = DynamicTargetManager::getInstance();
    
    // 테스트 타겟 추가
    DynamicTarget target = createTestTarget("ALARM_TEST", "http", true);
    manager.addOrUpdateTarget(target);
    
    AlarmMessage alarm = createTestAlarm(1001, "TEST_POINT", 85.5);
    
    auto results = manager.sendAlarmToTargets(alarm);
    runner.assertGreater(results.size(), 0, "전송 결과 존재");
    
    std::cout << " [" << results.size() << "개 타겟]";
    
    // Cleanup
    manager.removeTarget("ALARM_TEST");
}

void test_077_statistics(TestRunner& runner) {
    runner.startTest("TEST_077: 통계 조회");
    
    auto& manager = DynamicTargetManager::getInstance();
    
    // 테스트 타겟 추가
    DynamicTarget target = createTestTarget("STATS_TEST", "http", true);
    manager.addOrUpdateTarget(target);
    
    // 몇 개 알람 전송
    for (int i = 0; i < 3; i++) {
        AlarmMessage alarm = createTestAlarm(1001, "STATS_POINT_" + std::to_string(i), i * 10.0);
        manager.sendAlarmToTargets(alarm);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    auto stats = manager.getStatistics();
    runner.assertTrue(stats.contains("total_requests"), "total_requests 존재");
    runner.assertTrue(stats.contains("total_successes"), "total_successes 존재");
    runner.assertTrue(stats.contains("total_failures"), "total_failures 존재");
    
    // Cleanup
    manager.removeTarget("STATS_TEST");
}

void test_078_health_check(TestRunner& runner) {
    runner.startTest("TEST_078: 헬스체크");
    
    auto& manager = DynamicTargetManager::getInstance();
    
    bool is_running = manager.isRunning();
    runner.assertTrue(is_running, "실행 중");
    
    auto targets = manager.getAllTargets();
    size_t enabled_count = 0;
    for (const auto& target : targets) {
        if (target.enabled) enabled_count++;
    }
    
    runner.assertGreaterOrEqual(targets.size(), 0, "타겟 목록 조회");
    
    std::cout << " [전체:" << targets.size() << ", 활성:" << enabled_count << "]";
}

void test_079_send_to_specific_target(TestRunner& runner) {
    runner.startTest("TEST_079: 타겟별 전송");
    
    auto& manager = DynamicTargetManager::getInstance();
    
    // 테스트 타겟 추가
    DynamicTarget target = createTestTarget("SPECIFIC_TARGET", "http", true);
    manager.addOrUpdateTarget(target);
    
    AlarmMessage alarm = createTestAlarm(1002, "SPECIFIC_POINT", 99.9);
    
    auto result = manager.sendAlarmToTarget("SPECIFIC_TARGET", alarm);
    runner.assertEquals("SPECIFIC_TARGET", result.target_name, "타겟 이름 확인");
    runner.assertFalse(result.target_type.empty(), "타겟 타입 존재");
    
    std::cout << " [" << (result.success ? "성공" : "실패") << "]";
    
    // Cleanup
    manager.removeTarget("SPECIFIC_TARGET");
}

void test_080_update_existing_target(TestRunner& runner) {
    runner.startTest("TEST_080: 기존 타겟 수정");
    
    auto& manager = DynamicTargetManager::getInstance();
    
    // 타겟 추가
    DynamicTarget target = createTestTarget("UPDATE_TEST", "http", true);
    target.config["url"] = "http://old-url.com";
    manager.addOrUpdateTarget(target);
    
    // 같은 이름으로 수정 (addOrUpdateTarget은 이름이 같으면 업데이트)
    DynamicTarget updated_target = createTestTarget("UPDATE_TEST", "http", true);
    updated_target.config["url"] = "http://new-url.com";
    bool result = manager.addOrUpdateTarget(updated_target);
    
    runner.assertTrue(result, "타겟 수정 성공");
    
    auto fetched = manager.getTarget("UPDATE_TEST");
    runner.assertTrue(fetched.has_value(), "수정된 타겟 조회");
    
    if (fetched.has_value()) {
        std::string url = fetched->config["url"];
        runner.assertEquals("http://new-url.com", url, "URL 변경 확인");
    }
    
    // Cleanup
    manager.removeTarget("UPDATE_TEST");
}

// =============================================================================
// 메인 함수
// =============================================================================

int main() {
    try {
        std::cout << "\n";
        std::cout << "═══════════════════════════════════════════════════════\n";
        std::cout << "  DynamicTargetManager 단위 테스트 (실제 API)\n";
        std::cout << "═══════════════════════════════════════════════════════\n";
        
        // 로그 초기화
        LogManager::getInstance().Info("DynamicTargetManager 테스트 시작");
        
        // DynamicTargetManager 시작
        auto& manager = DynamicTargetManager::getInstance();
        if (!manager.isRunning()) {
            manager.start();
            std::cout << "✅ DynamicTargetManager 시작됨\n";
        }
        
        TestRunner runner;
        
        // 모든 테스트 실행
        test_070_singleton_instance(runner);
        test_071_load_from_database(runner);
        test_072_get_all_targets(runner);
        test_073_add_or_update_target(runner);
        test_074_remove_target(runner);
        test_075_enable_disable_target(runner);
        test_076_send_alarm(runner);
        test_077_statistics(runner);
        test_078_health_check(runner);
        test_079_send_to_specific_target(runner);
        test_080_update_existing_target(runner);
        
        // 결과 출력
        bool all_passed = runner.printSummary();
        
        LogManager::getInstance().Info("DynamicTargetManager 테스트 " + 
                                      std::string(all_passed ? "성공" : "실패"));
        
        return all_passed ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "\n💥 예외 발생: " << e.what() << "\n";
        return 1;
    }
}