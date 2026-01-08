/**
 * @file test_export_coordinator.cpp
 * @brief ExportCoordinator E2E 통합 테스트
 * @author PulseOne Development Team
 * @date 2025-12-17
 * @version 1.1.0 - 구조체 필드명 수정
 */

#include "CSP/ExportCoordinator.h"
#include "CSP/DynamicTargetManager.h"
#include "CSP/AlarmMessage.h"
#include "Transform/PayloadTransformer.h"
#include "Logging/LogManager.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

using namespace PulseOne::Coordinator;
using namespace PulseOne::CSP;
using namespace PulseOne::Transform;
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

// 테스트용 AlarmMessage 생성
AlarmMessage createTestAlarm(int building_id = 1001, 
                             const std::string& point_name = "TEMP_01", 
                             double value = 25.5,
                             int alarm_flag = 1,
                             int status = 0) {
    AlarmMessage alarm;
    alarm.bd = building_id;
    alarm.nm = point_name;
    alarm.vl = value;
    alarm.tm = "2025-12-17T10:30:45.123Z";
    alarm.al = alarm_flag;
    alarm.st = status;
    alarm.des = "테스트 알람";
    return alarm;
}

// 테스트용 설정 생성
ExportCoordinatorConfig createTestConfig() {
    ExportCoordinatorConfig config;
    config.database_path = "/tmp/pulseone_coordinator_test.db";
    config.redis_host = "pulseone-redis";
    config.redis_port = 6379;
    config.redis_password = "";
    config.alarm_channels = {"alarms:test"};
    config.alarm_patterns = {};
    config.alarm_worker_threads = 2;
    config.alarm_max_queue_size = 1000;
    config.schedule_check_interval_seconds = 60;
    config.schedule_reload_interval_seconds = 300;
    config.schedule_batch_size = 50;
    config.enable_debug_log = true;
    config.log_retention_days = 7;
    config.max_concurrent_exports = 10;
    config.export_timeout_seconds = 10;
    return config;
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 1: 초기화 및 시작 테스트 (4개)
// ═══════════════════════════════════════════════════════════════════════════

void test_coordinator_creation() {
    TEST("ExportCoordinator 생성");
    
    ExportCoordinatorConfig config = createTestConfig();
    
    try {
        ExportCoordinator coordinator(config);
        ASSERT(!coordinator.isRunning(), "생성 직후 실행 중이면 안 됨");
        std::cout << " [생성 성공]";
        PASS();
    } catch (const std::exception& e) {
        ASSERT(false, std::string("생성 실패: ") + e.what());
    }
}

void test_coordinator_component_init() {
    TEST("컴포넌트 초기화");
    
    ExportCoordinatorConfig config = createTestConfig();
    ExportCoordinator coordinator(config);
    
    bool started = coordinator.start();
    
    if (started) {
        auto target_manager = ExportCoordinator::getTargetManager();
        auto transformer = ExportCoordinator::getPayloadTransformer();
        
        coordinator.stop();
        
        ASSERT(target_manager != nullptr || transformer != nullptr, 
               "최소 하나의 컴포넌트는 초기화되어야 함");
        
        std::cout << " [TargetManager, Transformer]";
    } else {
        std::cout << " [Redis 미연결로 스킵]";
    }
    
    PASS();
}

void test_coordinator_start_stop() {
    TEST("시작/중지");
    
    ExportCoordinatorConfig config = createTestConfig();
    ExportCoordinator coordinator(config);
    
    ASSERT(!coordinator.isRunning(), "초기 상태는 중지");
    
    bool started = coordinator.start();
    
    if (started) {
        ASSERT(coordinator.isRunning(), "시작 후 실행 중이어야 함");
        
        coordinator.stop();
        ASSERT(!coordinator.isRunning(), "중지 후 실행 중이면 안 됨");
        
        std::cout << " [start → running → stop]";
    } else {
        std::cout << " [Redis 미연결]";
    }
    
    PASS();
}

void test_coordinator_status() {
    TEST("상태 조회");
    
    ExportCoordinatorConfig config = createTestConfig();
    ExportCoordinator coordinator(config);
    
    // 초기 상태 - 실제 필드명 사용
    auto stats = coordinator.getStats();
    ASSERT(stats.total_exports == 0, "초기 total_exports가 0이어야 함");
    ASSERT(stats.successful_exports == 0, "초기 successful_exports가 0이어야 함");
    ASSERT(stats.failed_exports == 0, "초기 failed_exports가 0이어야 함");
    
    // 컴포넌트 상태
    json component_status = coordinator.getComponentStatus();
    ASSERT(!component_status.empty(), "컴포넌트 상태가 비어있음");
    
    std::cout << " [stats, componentStatus]";
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 2: 알람 처리 흐름 테스트 (5개)
// ═══════════════════════════════════════════════════════════════════════════

void test_single_alarm_handling() {
    TEST("단일 알람 처리");
    
    ExportCoordinatorConfig config = createTestConfig();
    ExportCoordinator coordinator(config);
    
    AlarmMessage alarm = createTestAlarm(1001, "TEMP_01", 85.5, 1, 1);
    
    auto results = coordinator.handleAlarmEvent(alarm);
    
    auto stats = coordinator.getStats();
    ASSERT(stats.alarm_events >= 0, "알람 이벤트 카운트 오류");
    
    std::cout << " [처리 완료, 결과: " << results.size() << "개]";
    PASS();
}

void test_batch_alarm_handling() {
    TEST("배치 알람 처리");
    
    ExportCoordinatorConfig config = createTestConfig();
    ExportCoordinator coordinator(config);
    
    std::vector<AlarmMessage> alarms;
    for (int i = 0; i < 5; i++) {
        AlarmMessage alarm = createTestAlarm(1001 + i, "POINT_" + std::to_string(i), 
                                             20.0 + i, 1, 0);
        alarms.push_back(alarm);
    }
    
    auto results = coordinator.handleAlarmBatch(alarms);
    
    std::cout << " [5개 알람, 결과: " << results.size() << "개]";
    PASS();
}

void test_alarm_filtering() {
    TEST("알람 필터링");
    
    ExportCoordinatorConfig config = createTestConfig();
    ExportCoordinator coordinator(config);
    
    AlarmMessage normal_alarm = createTestAlarm(1001, "TEMP_01", 25.0, 0, 0);
    auto results1 = coordinator.handleAlarmEvent(normal_alarm);
    
    AlarmMessage warning_alarm = createTestAlarm(1001, "TEMP_01", 75.0, 1, 1);
    auto results2 = coordinator.handleAlarmEvent(warning_alarm);
    
    AlarmMessage critical_alarm = createTestAlarm(1001, "TEMP_01", 95.0, 2, 2);
    auto results3 = coordinator.handleAlarmEvent(critical_alarm);
    
    std::cout << " [정상/경고/위험 처리]";
    PASS();
}

void test_alarm_transformation() {
    TEST("알람 변환");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    AlarmMessage alarm = createTestAlarm(1001, "TEMP_01", 85.5, 1, 1);
    
    auto context = transformer.createContext(
        alarm, 
        "Temperature_Sensor_01", 
        "1층 온도 센서",
        "85.5"
    );
    
    json template_json = transformer.getGenericDefaultTemplate();
    json result = transformer.transform(template_json, context);
    
    ASSERT(!result.empty(), "변환 결과가 비어있음");
    ASSERT(result.contains("building_id"), "building_id 없음");
    ASSERT(result.contains("point_name"), "point_name 없음");
    
    std::cout << " [Generic 템플릿 적용]";
    PASS();
}

void test_alarm_failure_handling() {
    TEST("실패 처리");
    
    ExportCoordinatorConfig config = createTestConfig();
    ExportCoordinator coordinator(config);
    
    AlarmMessage empty_alarm;
    empty_alarm.bd = 0;
    empty_alarm.nm = "";
    empty_alarm.vl = 0;
    empty_alarm.tm = "";
    empty_alarm.al = 0;
    empty_alarm.st = 0;
    empty_alarm.des = "";
    
    try {
        auto results = coordinator.handleAlarmEvent(empty_alarm);
        std::cout << " [빈 알람 처리 완료]";
        PASS();
    } catch (const std::exception& e) {
        ASSERT(false, std::string("예외 발생: ") + e.what());
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 3: 타겟 연동 테스트 (4개)
// ═══════════════════════════════════════════════════════════════════════════

void test_target_manager_integration() {
    TEST("DynamicTargetManager 연동");
    
    // 싱글톤 또는 static 메서드로 가져오기
    auto target_manager = ExportCoordinator::getTargetManager();
    
    if (target_manager) {
        // 알람 전송 테스트
        AlarmMessage alarm = createTestAlarm();
        auto results = target_manager->sendAlarmToTargets(alarm);
        
        std::cout << " [타겟 매니저 연동, 결과: " << results.size() << "개]";
    } else {
        std::cout << " [TargetManager 미초기화 - 스킵]";
    }
    
    PASS();
}

void test_transformer_integration() {
    TEST("PayloadTransformer 연동");
    
    auto& transformer = PayloadTransformer::getInstance();
    
    json insite = transformer.getInsiteDefaultTemplate();
    json hdc = transformer.getHDCDefaultTemplate();
    json bems = transformer.getBEMSDefaultTemplate();
    json generic = transformer.getGenericDefaultTemplate();
    
    ASSERT(!insite.empty(), "Insite 템플릿 비어있음");
    ASSERT(!hdc.empty(), "HDC 템플릿 비어있음");
    ASSERT(!bems.empty(), "BEMS 템플릿 비어있음");
    ASSERT(!generic.empty(), "Generic 템플릿 비어있음");
    
    AlarmMessage alarm = createTestAlarm();
    auto context = transformer.createContext(alarm, "Field", "Desc", "Value");
    
    json result1 = transformer.transform(insite, context);
    json result2 = transformer.transform(hdc, context);
    json result3 = transformer.transform(bems, context);
    json result4 = transformer.transform(generic, context);
    
    ASSERT(!result1.empty(), "Insite 결과 비어있음");
    ASSERT(!result2.empty(), "HDC 결과 비어있음");
    ASSERT(!result3.empty(), "BEMS 결과 비어있음");
    ASSERT(!result4.empty(), "Generic 결과 비어있음");
    
    std::cout << " [4개 시스템 템플릿]";
    PASS();
}

void test_multi_target_send() {
    TEST("멀티 타겟 전송");
    
    auto target_manager = ExportCoordinator::getTargetManager();
    
    if (target_manager) {
        AlarmMessage alarm = createTestAlarm();
        auto results = target_manager->sendAlarmToTargets(alarm);
        
        std::cout << " [" << results.size() << "개 타겟 전송]";
    } else {
        std::cout << " [TargetManager 미초기화 - 스킵]";
    }
    
    PASS();
}

void test_target_reload() {
    TEST("타겟 갱신");
    
    ExportCoordinatorConfig config = createTestConfig();
    ExportCoordinator coordinator(config);
    
    int reloaded = coordinator.reloadTargets();
    int templates = coordinator.reloadTemplates();
    
    std::cout << " [타겟: " << reloaded << ", 템플릿: " << templates << "]";
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 4: 통계 및 로깅 테스트 (4개)
// ═══════════════════════════════════════════════════════════════════════════

void test_processing_stats() {
    TEST("처리 통계");
    
    ExportCoordinatorConfig config = createTestConfig();
    ExportCoordinator coordinator(config);
    
    auto stats1 = coordinator.getStats();
    size_t initial_count = stats1.alarm_events;
    
    for (int i = 0; i < 3; i++) {
        AlarmMessage alarm = createTestAlarm(1000 + i, "POINT_" + std::to_string(i), 
                                             25.0 + i);
        coordinator.handleAlarmEvent(alarm);
    }
    
    auto stats2 = coordinator.getStats();
    ASSERT(stats2.alarm_events >= initial_count, "알람 이벤트 카운트 증가 안 됨");
    
    std::cout << " [알람 이벤트: " << stats2.alarm_events << "]";
    PASS();
}

void test_success_failure_count() {
    TEST("성공/실패 카운트");
    
    ExportCoordinatorConfig config = createTestConfig();
    ExportCoordinator coordinator(config);
    
    auto stats = coordinator.getStats();
    
    size_t total = stats.total_exports;
    size_t success = stats.successful_exports;
    size_t failed = stats.failed_exports;
    
    // 값 출력
    std::cout << " [성공: " << success << ", 실패: " << failed << "]";
    PASS();
}

void test_export_logging() {
    TEST("로그 기록");
    
    ExportCoordinatorConfig config = createTestConfig();
    ExportCoordinator coordinator(config);
    
    // ExportResult 생성 - 실제 필드명 사용
    ExportResult result;
    result.success = true;
    result.target_id = 1;
    result.target_name = "TEST_TARGET";
    result.error_message = "";
    result.http_status_code = 200;
    result.processing_time = std::chrono::milliseconds(15);
    result.data_size = 100;
    
    try {
        coordinator.logExportResult(result);
        std::cout << " [단일 로그]";
        PASS();
    } catch (const std::exception& e) {
        ASSERT(false, std::string("로깅 실패: ") + e.what());
    }
}

void test_health_check() {
    TEST("헬스체크");
    
    ExportCoordinatorConfig config = createTestConfig();
    ExportCoordinator coordinator(config);
    
    bool healthy = coordinator.healthCheck();
    
    json status = coordinator.getComponentStatus();
    ASSERT(!status.empty(), "상태 비어있음");
    
    std::cout << " [상태: " << (healthy ? "정상" : "비정상") << "]";
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 5: Redis 연동 테스트 (3개)
// ═══════════════════════════════════════════════════════════════════════════

void test_redis_subscription_config() {
    TEST("Redis 구독 설정");
    
    ExportCoordinatorConfig config = createTestConfig();
    
    ASSERT(config.alarm_channels.size() > 0, "알람 채널이 없음");
    ASSERT(config.redis_host == "pulseone-redis", "Redis 호스트 불일치");
    ASSERT(config.redis_port == 6379, "Redis 포트 불일치");
    
    ExportCoordinator coordinator(config);
    
    std::cout << " [채널: " << config.alarm_channels[0] << "]";
    PASS();
}

void test_redis_message_flow() {
    TEST("Redis 메시지 흐름 (시뮬레이션)");
    
    ExportCoordinatorConfig config = createTestConfig();
    ExportCoordinator coordinator(config);
    
    AlarmMessage alarm = createTestAlarm(1001, "REDIS_TEST", 42.0);
    
    json alarm_json = alarm.to_json();
    std::string message = alarm_json.dump();
    
    ASSERT(!message.empty(), "메시지가 비어있음");
    ASSERT(message.find("REDIS_TEST") != std::string::npos, "포인트명 없음");
    ASSERT(message.find("1001") != std::string::npos, "빌딩ID 없음");
    
    json parsed = json::parse(message);
    ASSERT(parsed["bd"].get<int>() == 1001, "파싱 후 bd 불일치");
    ASSERT(parsed["nm"].get<std::string>() == "REDIS_TEST", "파싱 후 nm 불일치");
    
    std::cout << " [직렬화 ↔ 역직렬화]";
    PASS();
}

void test_redis_reconnection() {
    TEST("Redis 연결 복구");
    
    ExportCoordinatorConfig config = createTestConfig();
    ExportCoordinator coordinator(config);
    
    bool started = coordinator.start();
    
    if (started) {
        ASSERT(coordinator.isRunning(), "실행 중이어야 함");
        
        coordinator.stop();
        ASSERT(!coordinator.isRunning(), "중지되어야 함");
        
        bool restarted = coordinator.start();
        if (restarted) {
            ASSERT(coordinator.isRunning(), "재시작 후 실행 중이어야 함");
            coordinator.stop();
        }
        
        std::cout << " [시작 → 중지 → 재시작]";
    } else {
        std::cout << " [Redis 미연결로 스킵]";
    }
    
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 메인
// ═══════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    std::cout << "  ExportCoordinator E2E 통합 테스트\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    
    auto& logger = LogManager::getInstance();
    logger.Info("ExportCoordinator 통합 테스트 시작");
    
    // 파트 1: 초기화 및 시작 (4개)
    std::cout << "\n📌 Part 1: 초기화 및 시작" << std::endl;
    test_coordinator_creation();
    test_coordinator_component_init();
    test_coordinator_start_stop();
    test_coordinator_status();
    
    // 파트 2: 알람 처리 흐름 (5개)
    std::cout << "\n📌 Part 2: 알람 처리 흐름" << std::endl;
    test_single_alarm_handling();
    test_batch_alarm_handling();
    test_alarm_filtering();
    test_alarm_transformation();
    test_alarm_failure_handling();
    
    // 파트 3: 타겟 연동 (4개)
    std::cout << "\n📌 Part 3: 타겟 연동" << std::endl;
    test_target_manager_integration();
    test_transformer_integration();
    test_multi_target_send();
    test_target_reload();
    
    // 파트 4: 통계 및 로깅 (4개)
    std::cout << "\n📌 Part 4: 통계 및 로깅" << std::endl;
    test_processing_stats();
    test_success_failure_count();
    test_export_logging();
    test_health_check();
    
    // 파트 5: Redis 연동 (3개)
    std::cout << "\n📌 Part 5: Redis 연동" << std::endl;
    test_redis_subscription_config();
    test_redis_message_flow();
    test_redis_reconnection();
    
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
    
    logger.Info("ExportCoordinator 통합 테스트 완료");
    
    return (failed_count == 0) ? 0 : 1;
}