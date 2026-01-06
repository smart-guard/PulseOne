/**
 * @file test_performance.cpp
 * @brief Export Gateway 성능 테스트 (Throughput)
 * @author PulseOne Development Team
 * @date 2025-12-19
 * @version 1.0.2
 * 
 * 🎯 테스트 목표:
 * - Throughput: 100+ alarms/sec 달성 여부 검증
 * - Latency: 평균 처리 시간 측정
 * - Stability: 대량 데이터 처리 시 메모리/리소스 안정성
 * 
 * v1.0.2 변경사항:
 * - 컴파일 에러 수정 (ITargetHandler 추상 메소드 구현)
 * - DynamicTarget 필드명 수정 (target_id 제거, enabled 수정)
 */

#include "CSP/ExportCoordinator.h"
#include "CSP/DynamicTargetManager.h"
#include "CSP/AlarmMessage.h"
#include "Utils/LogManager.h"
#include "Export/ExportTypes.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <iomanip>
#include <numeric>

using namespace PulseOne::Coordinator;
using namespace PulseOne::CSP;
using json = nlohmann::json;

// =============================================================================
// 테스트 설정
// =============================================================================

const int TARGET_THROUGHPUT = 100;     // 목표: 100 alarms/sec
const int TEST_DURATION_SEC = 5;       // 테스트 지속 시간
const int WARMUP_COUNT = 100;          // 워밍업 알람 수

// =============================================================================
// Mock Components
// =============================================================================

// 성능 측정용 Mock Target Handler
class MockPerfTargetHandler : public ITargetHandler {
public:
    std::atomic<int> success_count{0};
    
    // 필수 구현 메소드: 핸들러 타입 반환
    std::string getHandlerType() const override {
        return "MOCK_PERF";
    }
    
    // 필수 구현 메소드: 설정 검증
    bool validateConfig(const json& /*config*/, std::vector<std::string>& /*errors*/) override {
        return true;
    }

    bool initialize(const json& /*config*/) override { return true; }
    
    TargetSendResult sendAlarm(const AlarmMessage& /*alarm*/, const json& /*config*/) override {
        // 처리 시간 시뮬레이션 (약 0.1ms - 고성능 시나리오)
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        
        TargetSendResult result;
        result.success = true;
        result.target_type = "MOCK_PERF";
        result.target_name = "PerfTest";
        
        success_count++;
        return result;
    }
    
    bool testConnection(const json& /*config*/) override { return true; }
    json getStatus() const override { return json{}; }
    void cleanup() override {}
};

// =============================================================================
// 테스트 헬퍼
// =============================================================================

AlarmMessage createPerfAlarm(int i) {
    AlarmMessage alarm;
    alarm.bd = 1001;
    alarm.nm = "PERF_PT_" + std::to_string(i % 100);
    alarm.vl = 25.0 + (i % 10);
    alarm.tm = "2025-12-19T12:00:00.000Z";
    alarm.al = 1;
    alarm.st = 0;
    alarm.des = "Performance Test Alarm";
    return alarm;
}

ExportCoordinatorConfig createPerfConfig() {
    ExportCoordinatorConfig config;
    config.database_path = ":memory:";  // 인메모리 DB로 디스크 I/O 배제
    config.enable_debug_log = false;    // 로깅 오버헤드 최소화
    config.max_concurrent_exports = 50; // 병렬 처리
    return config;
}

// =============================================================================
// 메인 테스트
// =============================================================================

int main() {
    std::cout << "\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    std::cout << "  🚀 Export Gateway 성능 테스트 (Throughput)\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    
    // 1. 로그 레벨 조정 (성능 영향 최소화)
    LogManager::getInstance().setLogLevel(LogLevel::WARN);
    
    // 2. 코디네이터 초기화
    std::cout << "📌 초기화 중..." << std::endl;
    ExportCoordinatorConfig config = createPerfConfig();
    ExportCoordinator coordinator(config);
    
    // 중요: 코디네이터 시작 (내부 컴포넌트 초기화)
    if (!coordinator.start()) {
        std::cerr << "❌ ExportCoordinator 시작 실패" << std::endl;
        return 1;
    }
    
    // 3. Mock 핸들러 및 타겟 등록
    // DynamicTargetManager 설정
    auto target_manager = ExportCoordinator::getTargetManager();
    if (!target_manager) {
        std::cerr << "❌ TargetManager 획득 실패" << std::endl;
        return 1;
    }
    
    // Mock 핸들러 등록
    target_manager->registerHandler("MOCK_PERF", std::make_unique<MockPerfTargetHandler>());
    
    // Mock 타겟 추가
    DynamicTarget target;
    // target.target_id = 9999; // 필드 없음
    target.name = "PerfTestTarget";
    target.type = "MOCK_PERF"; 
    target.config = {{"test", "config"}};
    target.enabled = true; // is_enabled -> enabled
    // target.retry_policy.max_retries = 0; // 필드 없음
    
    if (!target_manager->addOrUpdateTarget(target)) {
        std::cerr << "❌ 타겟 등록 실패" << std::endl;
        return 1;
    }
    
    std::cout << "📌 Mock 타겟 등록 완료" << std::endl;
    
    // 4. 워밍업
    std::cout << "📌 워밍업 (" << WARMUP_COUNT << "개)..." << std::flush;
    for (int i = 0; i < WARMUP_COUNT; i++) {
        coordinator.handleAlarmEvent(createPerfAlarm(i));
    }
    // 워밍업 처리 대기 (간단히 시간 지연)
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << " 완료" << std::endl;
    
    // 5. 본 테스트 시작
    std::cout << "📌 성능 측정 시작 (목표: " << TARGET_THROUGHPUT << "+ alarms/sec)" << std::endl;
    
    int total_processed = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    // 5초간 부하 주입
    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_sec = std::chrono::duration<double>(now - start_time).count();
        
        if (elapsed_sec >= TEST_DURATION_SEC) break;
        
        // 알람 주입
        coordinator.handleAlarmEvent(createPerfAlarm(total_processed));
        total_processed++;
        
        // 큐 풀 방지 (10000개 마다 살짝 대기)
        if (total_processed % 10000 == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    // 처리 완료 대기 (큐 비우기)
    std::cout << "📌 잔여 큐 처리 대기..." << std::endl;
    int wait_count = 0;
    while (wait_count < 100) { // 최대 10초 대기
        auto stats = coordinator.getStats();
        
        // 주입된 것(alarm_events)과 처리된 것(alarm_exports)가 비슷해질 때까지
        // 실제로는 알람 이벤트가 큐에 들어가는 시간이 있으므로,
        // alarm_events가 total_processed에 도달하고,
        // 그게 다 처리될 때(total_exports)까지 기다려야 하지만,
        // 여기서는 간단히 stats.total_exports가 증가하지 않을 때까지 대기
        
        static size_t last_exports = 0;
        if (wait_count > 5 && stats.total_exports == last_exports && stats.total_exports > 0) {
            break; 
        }
        last_exports = stats.total_exports;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }
    
    auto end_time = std::chrono::steady_clock::now();
    double duration = std::chrono::duration<double>(end_time - start_time).count();
    
    // 6. 결과 계산
    auto stats = coordinator.getStats();
    size_t actual_processed = stats.total_exports; 
    
    double throughput = actual_processed / duration;
    
    std::cout << "\n═══════════════════════════════════════════════════════\n";
    std::cout << "  📊 테스트 결과\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    std::cout << "  ⏱️  총 소요 시간: " << std::fixed << std::setprecision(2) << duration << "초\n";
    std::cout << "  📥 주입 요청: " << total_processed << "개\n";
    std::cout << "  📦 실제 처리: " << actual_processed << "개\n";
    std::cout << "  🚀 Throughput: " << std::setprecision(1) << throughput << " alarms/sec\n";
    std::cout << "  🎯 목표 달성: " << (throughput >= TARGET_THROUGHPUT ? "YES ✅" : "NO ❌") << "\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    
    coordinator.stop();
    
    if (throughput >= TARGET_THROUGHPUT) {
        std::cout << "\n✅ 성능 테스트 통과!\n";
        return 0;
    } else {
        std::cout << "\n❌ 성능 목표 미달\n";
        return 1;
    }
}
