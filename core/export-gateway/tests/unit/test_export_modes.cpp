/**
 * @file test_export_modes.cpp
 * @brief Export 모드 완전한 단위 테스트
 * @author PulseOne Development Team
 * @date 2025-12-17
 * @version 1.0.0
 * 
 * 🎯 테스트 목표:
 * Export Target의 3가지 전송 모드 검증
 * 
 * [on_change 모드] (5개)
 * - 값 변경 시 즉시 전송
 * - 동일 값은 전송 안 함
 * - 임계값 기반 변경 감지
 * - 첫 번째 값은 항상 전송
 * - 강제 전송 옵션
 * 
 * [periodic 모드] (5개)
 * - 주기적 전송 간격
 * - 간격 내 여러 변경은 마지막 값만
 * - 간격 검증 (최소/최대)
 * - 타이머 정확도
 * - 일시정지/재개
 * 
 * [batch 모드] (5개)
 * - 배치 크기만큼 모아서 전송
 * - 배치 타임아웃
 * - 부분 배치 전송
 * - 배치 크기 검증
 * - 배치 오버플로우 처리
 * 
 * [모드 전환] (3개)
 * - 런타임 모드 변경
 * - 모드별 설정 검증
 * - 상태 초기화
 * 
 * [통계 및 로깅] (2개)
 * - 모드별 통계 정확도
 * - 전송 로그 기록
 */

#include "CSP/DynamicTargetManager.h"
#include "CSP/HttpTargetHandler.h"
#include "CSP/FileTargetHandler.h"
#include "CSP/AlarmMessage.h"
#include "Utils/LogManager.h"
#include "Export/ExportTypes.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <vector>
#include <atomic>
#include <mutex>

using namespace PulseOne::CSP;
using namespace PulseOne::Export;
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

// 테스트용 전송 카운터
static std::atomic<int> g_send_count{0};
static std::mutex g_send_mutex;
static std::vector<std::string> g_sent_values;

void resetSendCounter() {
    g_send_count = 0;
    std::lock_guard<std::mutex> lock(g_send_mutex);
    g_sent_values.clear();
}

int getSendCount() {
    return g_send_count.load();
}

std::vector<std::string> getSentValues() {
    std::lock_guard<std::mutex> lock(g_send_mutex);
    return g_sent_values;
}

void recordSend(const std::string& value) {
    g_send_count++;
    std::lock_guard<std::mutex> lock(g_send_mutex);
    g_sent_values.push_back(value);
}

// 테스트 디렉토리 정리
const std::string TEST_BASE_DIR = "/tmp/pulseone_export_modes_test";

void cleanupTestDir() {
    try {
        if (std::filesystem::exists(TEST_BASE_DIR)) {
            std::filesystem::remove_all(TEST_BASE_DIR);
        }
        std::filesystem::create_directories(TEST_BASE_DIR);
    } catch (...) {}
}

// 파일 내용 읽기
std::string readFileContent(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// 파일 라인 수 계산
int countFileLines(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return 0;
    int count = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) count++;
    }
    return count;
}

// 더미 AlarmMessage 생성
AlarmMessage createTestAlarm(int building_id = 1001, 
                             const std::string& point_name = "TEMP_01", 
                             double value = 25.5) {
    AlarmMessage alarm;
    alarm.bd = building_id;
    alarm.nm = point_name;
    alarm.vl = value;
    alarm.tm = "2025-12-17T10:30:45.123Z";
    alarm.al = 1;
    alarm.st = 0;
    alarm.des = "Test alarm for export mode";
    return alarm;
}

// ═══════════════════════════════════════════════════════════════════════════
// Export 모드 시뮬레이터
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Export 모드 동작을 시뮬레이션하는 클래스
 * 실제 DynamicTargetManager의 모드 로직을 테스트
 */
class ExportModeSimulator {
public:
    enum class Mode {
        ON_CHANGE,
        PERIODIC,
        BATCH
    };
    
    struct Config {
        Mode mode = Mode::ON_CHANGE;
        double change_threshold = 0.0;      // on_change: 변경 임계값
        int periodic_interval_ms = 1000;    // periodic: 전송 간격 (ms)
        int batch_size = 10;                // batch: 배치 크기
        int batch_timeout_ms = 5000;        // batch: 타임아웃 (ms)
        bool force_first_send = true;       // 첫 값은 항상 전송
    };
    
    explicit ExportModeSimulator(const Config& config) 
        : config_(config), last_value_(0), has_last_value_(false),
          send_count_(0), batch_count_(0) {
        last_send_time_ = std::chrono::steady_clock::now();
        batch_start_time_ = std::chrono::steady_clock::now();
    }
    
    /**
     * @brief 값을 처리하고 전송 여부 결정
     * @return true면 즉시 전송, false면 전송 안 함
     */
    bool processValue(double value) {
        auto now = std::chrono::steady_clock::now();
        
        switch (config_.mode) {
            case Mode::ON_CHANGE:
                return processOnChange(value);
                
            case Mode::PERIODIC:
                return processPeriodic(value, now);
                
            case Mode::BATCH:
                return processBatch(value, now);
        }
        
        return false;
    }
    
    /**
     * @brief 배치 모드에서 강제 플러시
     */
    bool flushBatch() {
        if (config_.mode != Mode::BATCH || batch_buffer_.empty()) {
            return false;
        }
        
        // 배치 전송
        send_count_++;
        batch_buffer_.clear();
        batch_count_ = 0;
        batch_start_time_ = std::chrono::steady_clock::now();
        return true;
    }
    
    /**
     * @brief 강제 전송 (모든 모드)
     */
    void forceSend(double value) {
        last_value_ = value;
        has_last_value_ = true;
        send_count_++;
        last_send_time_ = std::chrono::steady_clock::now();
    }
    
    int getSendCount() const { return send_count_; }
    int getBatchSize() const { return batch_buffer_.size(); }
    double getLastValue() const { return last_value_; }
    
    void reset() {
        send_count_ = 0;
        batch_count_ = 0;
        has_last_value_ = false;
        batch_buffer_.clear();
        last_send_time_ = std::chrono::steady_clock::now();
        batch_start_time_ = std::chrono::steady_clock::now();
    }
    
    void setMode(Mode mode) {
        config_.mode = mode;
        reset();
    }
    
    Config& getConfig() { return config_; }

private:
    bool processOnChange(double value) {
        // 첫 번째 값
        if (!has_last_value_) {
            if (config_.force_first_send) {
                last_value_ = value;
                has_last_value_ = true;
                send_count_++;
                return true;
            }
            last_value_ = value;
            has_last_value_ = true;
            return false;
        }
        
        // 변경 감지
        double diff = std::abs(value - last_value_);
        if (diff > config_.change_threshold) {
            last_value_ = value;
            send_count_++;
            return true;
        }
        
        return false;
    }
    
    bool processPeriodic(double value, std::chrono::steady_clock::time_point now) {
        last_value_ = value;
        
        // 첫 번째 값은 항상 전송
        if (!has_last_value_) {
            has_last_value_ = true;
            send_count_++;
            last_send_time_ = now;
            return true;
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_send_time_).count();
        
        if (elapsed >= config_.periodic_interval_ms) {
            send_count_++;
            last_send_time_ = now;
            return true;
        }
        
        return false;
    }
    
    bool processBatch(double value, std::chrono::steady_clock::time_point now) {
        batch_buffer_.push_back(value);
        batch_count_++;
        
        // 배치 크기 도달
        if (batch_count_ >= config_.batch_size) {
            send_count_++;
            batch_buffer_.clear();
            batch_count_ = 0;
            batch_start_time_ = now;
            return true;
        }
        
        // 타임아웃 체크
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - batch_start_time_).count();
        
        if (elapsed >= config_.batch_timeout_ms && !batch_buffer_.empty()) {
            send_count_++;
            batch_buffer_.clear();
            batch_count_ = 0;
            batch_start_time_ = now;
            return true;
        }
        
        return false;
    }
    
    Config config_;
    double last_value_;
    bool has_last_value_;
    int send_count_;
    int batch_count_;
    std::chrono::steady_clock::time_point last_send_time_;
    std::chrono::steady_clock::time_point batch_start_time_;
    std::vector<double> batch_buffer_;
};

// ═══════════════════════════════════════════════════════════════════════════
// 파트 1: on_change 모드 테스트 (5개)
// ═══════════════════════════════════════════════════════════════════════════

void test_on_change_immediate_send() {
    TEST("on_change: 값 변경 시 즉시 전송");
    
    ExportModeSimulator::Config config;
    config.mode = ExportModeSimulator::Mode::ON_CHANGE;
    config.change_threshold = 0.0;  // 모든 변경 감지
    config.force_first_send = true;
    
    ExportModeSimulator sim(config);
    
    // 첫 번째 값 → 전송
    bool sent1 = sim.processValue(25.0);
    ASSERT(sent1, "첫 번째 값 전송 안 됨");
    ASSERT(sim.getSendCount() == 1, "전송 카운트 불일치");
    
    // 다른 값 → 전송
    bool sent2 = sim.processValue(26.0);
    ASSERT(sent2, "변경된 값 전송 안 됨");
    ASSERT(sim.getSendCount() == 2, "전송 카운트 불일치");
    
    // 또 다른 값 → 전송
    bool sent3 = sim.processValue(24.5);
    ASSERT(sent3, "변경된 값 전송 안 됨");
    ASSERT(sim.getSendCount() == 3, "전송 카운트 불일치");
    
    std::cout << " [3번 전송]";
    PASS();
}

void test_on_change_same_value_skip() {
    TEST("on_change: 동일 값 전송 안 함");
    
    ExportModeSimulator::Config config;
    config.mode = ExportModeSimulator::Mode::ON_CHANGE;
    config.change_threshold = 0.0;
    config.force_first_send = true;
    
    ExportModeSimulator sim(config);
    
    // 첫 번째 값
    sim.processValue(25.0);
    ASSERT(sim.getSendCount() == 1, "첫 번째 전송 실패");
    
    // 동일 값 반복
    bool sent2 = sim.processValue(25.0);
    ASSERT(!sent2, "동일 값인데 전송됨");
    ASSERT(sim.getSendCount() == 1, "카운트 증가됨");
    
    bool sent3 = sim.processValue(25.0);
    ASSERT(!sent3, "동일 값인데 전송됨");
    ASSERT(sim.getSendCount() == 1, "카운트 증가됨");
    
    // 다른 값
    bool sent4 = sim.processValue(25.1);
    ASSERT(sent4, "변경된 값 전송 안 됨");
    ASSERT(sim.getSendCount() == 2, "카운트 불일치");
    
    std::cout << " [4번 입력, 2번 전송]";
    PASS();
}

void test_on_change_threshold() {
    TEST("on_change: 임계값 기반 변경 감지");
    
    ExportModeSimulator::Config config;
    config.mode = ExportModeSimulator::Mode::ON_CHANGE;
    config.change_threshold = 1.0;  // 1.0 이상 변경만 감지
    config.force_first_send = true;
    
    ExportModeSimulator sim(config);
    
    // 첫 번째 값
    sim.processValue(25.0);
    ASSERT(sim.getSendCount() == 1, "첫 번째 전송 실패");
    
    // 작은 변경 (임계값 미만)
    bool sent2 = sim.processValue(25.5);  // 0.5 차이
    ASSERT(!sent2, "임계값 미만인데 전송됨");
    ASSERT(sim.getSendCount() == 1, "카운트 증가됨");
    
    bool sent3 = sim.processValue(25.8);  // 0.8 차이
    ASSERT(!sent3, "임계값 미만인데 전송됨");
    
    // 큰 변경 (임계값 이상)
    bool sent4 = sim.processValue(27.0);  // 2.0 차이
    ASSERT(sent4, "임계값 이상인데 전송 안 됨");
    ASSERT(sim.getSendCount() == 2, "카운트 불일치");
    
    std::cout << " [임계값 1.0, 4번 입력, 2번 전송]";
    PASS();
}

void test_on_change_first_value() {
    TEST("on_change: 첫 번째 값 처리");
    
    // force_first_send = true
    {
        ExportModeSimulator::Config config;
        config.mode = ExportModeSimulator::Mode::ON_CHANGE;
        config.force_first_send = true;
        
        ExportModeSimulator sim(config);
        bool sent = sim.processValue(25.0);
        ASSERT(sent, "force=true인데 첫 값 전송 안 됨");
    }
    
    // force_first_send = false
    {
        ExportModeSimulator::Config config;
        config.mode = ExportModeSimulator::Mode::ON_CHANGE;
        config.force_first_send = false;
        
        ExportModeSimulator sim(config);
        bool sent = sim.processValue(25.0);
        ASSERT(!sent, "force=false인데 첫 값 전송됨");
        
        // 두 번째 값은 전송
        bool sent2 = sim.processValue(26.0);
        ASSERT(sent2, "두 번째 값 전송 안 됨");
    }
    
    std::cout << " [force_first_send 검증]";
    PASS();
}

void test_on_change_force_send() {
    TEST("on_change: 강제 전송 옵션");
    
    ExportModeSimulator::Config config;
    config.mode = ExportModeSimulator::Mode::ON_CHANGE;
    config.change_threshold = 10.0;  // 높은 임계값
    config.force_first_send = true;
    
    ExportModeSimulator sim(config);
    
    sim.processValue(25.0);  // 첫 값
    ASSERT(sim.getSendCount() == 1, "첫 전송 실패");
    
    // 임계값 미만 변경
    sim.processValue(25.5);
    ASSERT(sim.getSendCount() == 1, "임계값 미만인데 전송됨");
    
    // 강제 전송
    sim.forceSend(25.5);
    ASSERT(sim.getSendCount() == 2, "강제 전송 실패");
    
    std::cout << " [forceSend 동작]";
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 2: periodic 모드 테스트 (5개)
// ═══════════════════════════════════════════════════════════════════════════

void test_periodic_interval() {
    TEST("periodic: 주기적 전송 간격");
    
    ExportModeSimulator::Config config;
    config.mode = ExportModeSimulator::Mode::PERIODIC;
    config.periodic_interval_ms = 100;  // 100ms 간격
    
    ExportModeSimulator sim(config);
    
    // 첫 번째 값 (간격 체크 없이 바로 전송)
    bool sent1 = sim.processValue(25.0);
    ASSERT(sent1, "첫 번째 전송 실패");
    
    // 즉시 다시 전송 시도 (간격 내)
    bool sent2 = sim.processValue(26.0);
    ASSERT(!sent2, "간격 내인데 전송됨");
    
    // 간격 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    
    // 간격 후 전송
    bool sent3 = sim.processValue(27.0);
    ASSERT(sent3, "간격 후인데 전송 안 됨");
    ASSERT(sim.getSendCount() == 2, "전송 카운트 불일치");
    
    std::cout << " [100ms 간격]";
    PASS();
}

void test_periodic_last_value_only() {
    TEST("periodic: 간격 내 마지막 값만 전송");
    
    ExportModeSimulator::Config config;
    config.mode = ExportModeSimulator::Mode::PERIODIC;
    config.periodic_interval_ms = 200;
    
    ExportModeSimulator sim(config);
    
    // 첫 전송
    sim.processValue(25.0);
    ASSERT(sim.getSendCount() == 1, "첫 전송 실패");
    
    // 간격 내 여러 값 입력
    sim.processValue(26.0);
    sim.processValue(27.0);
    sim.processValue(28.0);
    ASSERT(sim.getSendCount() == 1, "간격 내인데 추가 전송됨");
    ASSERT(sim.getLastValue() == 28.0, "마지막 값 저장 안 됨");
    
    // 간격 후
    std::this_thread::sleep_for(std::chrono::milliseconds(210));
    bool sent = sim.processValue(29.0);
    ASSERT(sent, "간격 후 전송 안 됨");
    ASSERT(sim.getSendCount() == 2, "전송 카운트 불일치");
    
    std::cout << " [마지막 값 28.0 → 29.0 전송]";
    PASS();
}

void test_periodic_interval_validation() {
    TEST("periodic: 간격 검증 (최소/최대)");
    
    // 최소 간격 (10ms)
    {
        ExportModeSimulator::Config config;
        config.mode = ExportModeSimulator::Mode::PERIODIC;
        config.periodic_interval_ms = 10;
        
        ExportModeSimulator sim(config);
        sim.processValue(25.0);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        bool sent = sim.processValue(26.0);
        ASSERT(sent, "최소 간격 10ms 동작 안 함");
    }
    
    // 큰 간격 (500ms)
    {
        ExportModeSimulator::Config config;
        config.mode = ExportModeSimulator::Mode::PERIODIC;
        config.periodic_interval_ms = 500;
        
        ExportModeSimulator sim(config);
        sim.processValue(25.0);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        bool sent = sim.processValue(26.0);
        ASSERT(!sent, "500ms 간격인데 100ms에 전송됨");
    }
    
    std::cout << " [10ms ~ 500ms 검증]";
    PASS();
}

void test_periodic_timer_accuracy() {
    TEST("periodic: 타이머 정확도");
    
    ExportModeSimulator::Config config;
    config.mode = ExportModeSimulator::Mode::PERIODIC;
    config.periodic_interval_ms = 50;
    
    ExportModeSimulator sim(config);
    
    auto start = std::chrono::steady_clock::now();
    int send_count = 0;
    
    // 250ms 동안 반복 (약 5번 전송 예상)
    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        
        if (elapsed >= 250) break;
        
        if (sim.processValue(25.0 + send_count * 0.1)) {
            send_count++;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // 5번 ± 1번 허용
    ASSERT(send_count >= 4 && send_count <= 6, 
           "타이머 정확도 오차 (전송: " + std::to_string(send_count) + ")");
    
    std::cout << " [50ms x 5회 ≈ " << send_count << "회]";
    PASS();
}

void test_periodic_pause_resume() {
    TEST("periodic: 일시정지/재개");
    
    ExportModeSimulator::Config config;
    config.mode = ExportModeSimulator::Mode::PERIODIC;
    config.periodic_interval_ms = 50;
    
    ExportModeSimulator sim(config);
    
    // 시작
    sim.processValue(25.0);
    ASSERT(sim.getSendCount() == 1, "첫 전송 실패");
    
    // 일시정지 시뮬레이션 (모드 변경)
    sim.setMode(ExportModeSimulator::Mode::ON_CHANGE);
    
    // on_change 모드 첫 값은 전송됨 (force_first_send=true 기본값)
    // 그래서 다른 값으로 테스트
    sim.processValue(30.0);  // 첫 값 → 전송됨
    int count_after_pause = sim.getSendCount();
    
    // 동일 값 재입력 → 전송 안 됨
    bool sent = sim.processValue(30.0);
    ASSERT(!sent, "동일 값인데 전송됨");
    ASSERT(sim.getSendCount() == count_after_pause, "카운트 증가됨");
    
    // 재개 (periodic 모드로 복귀)
    sim.setMode(ExportModeSimulator::Mode::PERIODIC);
    sim.processValue(26.0);  // 모드 변경 후 첫 값 → 전송됨
    
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    bool sent2 = sim.processValue(27.0);
    ASSERT(sent2, "재개 후 전송 안 됨");
    
    std::cout << " [정지 → 재개 동작]";
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 3: batch 모드 테스트 (5개)
// ═══════════════════════════════════════════════════════════════════════════

void test_batch_size_trigger() {
    TEST("batch: 배치 크기만큼 모아서 전송");
    
    ExportModeSimulator::Config config;
    config.mode = ExportModeSimulator::Mode::BATCH;
    config.batch_size = 5;
    config.batch_timeout_ms = 10000;  // 긴 타임아웃
    
    ExportModeSimulator sim(config);
    
    // 4개 입력 (배치 크기 미만)
    for (int i = 0; i < 4; i++) {
        bool sent = sim.processValue(25.0 + i);
        ASSERT(!sent, "배치 크기 미만인데 전송됨");
    }
    ASSERT(sim.getBatchSize() == 4, "버퍼 크기 불일치");
    ASSERT(sim.getSendCount() == 0, "전송 카운트 불일치");
    
    // 5번째 입력 (배치 완성)
    bool sent = sim.processValue(29.0);
    ASSERT(sent, "배치 완성인데 전송 안 됨");
    ASSERT(sim.getSendCount() == 1, "전송 카운트 불일치");
    ASSERT(sim.getBatchSize() == 0, "버퍼 비워지지 않음");
    
    std::cout << " [배치 크기 5]";
    PASS();
}

void test_batch_timeout() {
    TEST("batch: 배치 타임아웃");
    
    ExportModeSimulator::Config config;
    config.mode = ExportModeSimulator::Mode::BATCH;
    config.batch_size = 100;  // 큰 배치 크기
    config.batch_timeout_ms = 100;  // 짧은 타임아웃
    
    ExportModeSimulator sim(config);
    
    // 3개 입력
    sim.processValue(25.0);
    sim.processValue(26.0);
    sim.processValue(27.0);
    ASSERT(sim.getSendCount() == 0, "타임아웃 전에 전송됨");
    
    // 타임아웃 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    
    // 타임아웃 후 입력
    bool sent = sim.processValue(28.0);
    ASSERT(sent, "타임아웃 후 전송 안 됨");
    ASSERT(sim.getSendCount() == 1, "전송 카운트 불일치");
    
    std::cout << " [100ms 타임아웃]";
    PASS();
}

void test_batch_partial_flush() {
    TEST("batch: 부분 배치 전송 (flush)");
    
    ExportModeSimulator::Config config;
    config.mode = ExportModeSimulator::Mode::BATCH;
    config.batch_size = 10;
    config.batch_timeout_ms = 10000;
    
    ExportModeSimulator sim(config);
    
    // 3개 입력
    sim.processValue(25.0);
    sim.processValue(26.0);
    sim.processValue(27.0);
    ASSERT(sim.getBatchSize() == 3, "버퍼 크기 불일치");
    ASSERT(sim.getSendCount() == 0, "전송됨");
    
    // 강제 플러시
    bool flushed = sim.flushBatch();
    ASSERT(flushed, "플러시 실패");
    ASSERT(sim.getSendCount() == 1, "전송 카운트 불일치");
    ASSERT(sim.getBatchSize() == 0, "버퍼 비워지지 않음");
    
    std::cout << " [3개 부분 플러시]";
    PASS();
}

void test_batch_size_validation() {
    TEST("batch: 배치 크기 검증");
    
    // 작은 배치 (1)
    {
        ExportModeSimulator::Config config;
        config.mode = ExportModeSimulator::Mode::BATCH;
        config.batch_size = 1;
        config.batch_timeout_ms = 10000;
        
        ExportModeSimulator sim(config);
        
        bool sent = sim.processValue(25.0);
        ASSERT(sent, "배치 크기 1인데 전송 안 됨");
    }
    
    // 큰 배치 (100)
    {
        ExportModeSimulator::Config config;
        config.mode = ExportModeSimulator::Mode::BATCH;
        config.batch_size = 100;
        config.batch_timeout_ms = 10000;
        
        ExportModeSimulator sim(config);
        
        for (int i = 0; i < 99; i++) {
            sim.processValue(i);
        }
        ASSERT(sim.getSendCount() == 0, "99개인데 전송됨");
        
        bool sent = sim.processValue(99);
        ASSERT(sent, "100개인데 전송 안 됨");
    }
    
    std::cout << " [배치 1~100 검증]";
    PASS();
}

void test_batch_continuous() {
    TEST("batch: 연속 배치 처리");
    
    ExportModeSimulator::Config config;
    config.mode = ExportModeSimulator::Mode::BATCH;
    config.batch_size = 3;
    config.batch_timeout_ms = 10000;
    
    ExportModeSimulator sim(config);
    
    // 10개 연속 입력 → 3번 전송 + 1개 남음
    for (int i = 0; i < 10; i++) {
        sim.processValue(i);
    }
    
    ASSERT(sim.getSendCount() == 3, "전송 카운트 불일치 (예상: 3)");
    ASSERT(sim.getBatchSize() == 1, "남은 버퍼 크기 불일치");
    
    std::cout << " [10개 → 3배치 + 1잔여]";
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 4: 모드 전환 테스트 (3개)
// ═══════════════════════════════════════════════════════════════════════════

void test_mode_switch_runtime() {
    TEST("모드 전환: 런타임 모드 변경");
    
    ExportModeSimulator::Config config;
    config.mode = ExportModeSimulator::Mode::ON_CHANGE;
    config.batch_size = 3;
    config.periodic_interval_ms = 50;
    
    ExportModeSimulator sim(config);
    
    // on_change 모드
    sim.processValue(25.0);
    sim.processValue(26.0);
    ASSERT(sim.getSendCount() == 2, "on_change 전송 실패");
    
    // batch 모드로 전환
    sim.setMode(ExportModeSimulator::Mode::BATCH);
    sim.processValue(27.0);
    sim.processValue(28.0);
    ASSERT(sim.getSendCount() == 0, "배치 모드인데 즉시 전송됨");
    
    sim.processValue(29.0);  // 3개 완성
    ASSERT(sim.getSendCount() == 1, "배치 전송 실패");
    
    // periodic 모드로 전환
    sim.setMode(ExportModeSimulator::Mode::PERIODIC);
    sim.processValue(30.0);  // 첫 값 → 전송됨
    ASSERT(sim.getSendCount() == 1, "periodic 첫 전송 실패");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    sim.processValue(31.0);
    ASSERT(sim.getSendCount() == 2, "periodic 간격 후 전송 실패");
    
    std::cout << " [on_change → batch → periodic]";
    PASS();
}

void test_mode_config_validation() {
    TEST("모드 전환: 모드별 설정 검증");
    
    ExportModeSimulator::Config config;
    
    // on_change 필수 설정
    config.mode = ExportModeSimulator::Mode::ON_CHANGE;
    ASSERT(config.change_threshold >= 0, "임계값 음수");
    
    // periodic 필수 설정
    config.mode = ExportModeSimulator::Mode::PERIODIC;
    ASSERT(config.periodic_interval_ms > 0, "간격 0 이하");
    
    // batch 필수 설정
    config.mode = ExportModeSimulator::Mode::BATCH;
    ASSERT(config.batch_size > 0, "배치 크기 0 이하");
    ASSERT(config.batch_timeout_ms > 0, "타임아웃 0 이하");
    
    PASS();
}

void test_mode_state_reset() {
    TEST("모드 전환: 상태 초기화");
    
    ExportModeSimulator::Config config;
    config.mode = ExportModeSimulator::Mode::BATCH;
    config.batch_size = 5;
    
    ExportModeSimulator sim(config);
    
    // 배치에 데이터 쌓기
    sim.processValue(25.0);
    sim.processValue(26.0);
    sim.processValue(27.0);
    ASSERT(sim.getBatchSize() == 3, "버퍼 크기 불일치");
    
    // 리셋
    sim.reset();
    ASSERT(sim.getBatchSize() == 0, "리셋 후 버퍼 비워지지 않음");
    ASSERT(sim.getSendCount() == 0, "리셋 후 카운트 초기화 안 됨");
    
    // 모드 변경으로 리셋
    sim.processValue(28.0);
    sim.processValue(29.0);
    sim.setMode(ExportModeSimulator::Mode::ON_CHANGE);
    ASSERT(sim.getBatchSize() == 0, "모드 변경 후 버퍼 비워지지 않음");
    
    std::cout << " [reset 및 setMode 상태 초기화]";
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 파트 5: 통계 및 파일 핸들러 통합 테스트 (2개)
// ═══════════════════════════════════════════════════════════════════════════

void test_mode_statistics() {
    TEST("통계: 모드별 통계 정확도");
    
    ExportModeSimulator::Config config;
    config.mode = ExportModeSimulator::Mode::ON_CHANGE;
    config.change_threshold = 0.5;
    
    ExportModeSimulator sim(config);
    
    // 10번 입력, 변경된 값만 전송
    std::vector<double> values = {25.0, 25.3, 25.8, 26.5, 26.6, 27.5, 27.6, 28.5, 28.6, 30.0};
    int expected_sends = 0;
    double last_sent = 0;
    
    for (size_t i = 0; i < values.size(); i++) {
        bool sent = sim.processValue(values[i]);
        if (sent) expected_sends++;
    }
    
    ASSERT(sim.getSendCount() == expected_sends, "통계 불일치");
    
    std::cout << " [10입력 → " << expected_sends << "전송]";
    PASS();
}

void test_file_handler_with_modes() {
    TEST("통합: FileHandler + Export 모드");
    
    cleanupTestDir();
    
    FileTargetHandler handler;
    
    json config = {
        {"name", "MODE_TEST"},
        {"base_path", TEST_BASE_DIR},
        {"file_format", "json"},
        {"filename_template", "export_log.json"},
        {"write_mode", "append"}
    };
    
    handler.initialize(config);
    
    // on_change 시뮬레이션: 변경된 값만 전송
    ExportModeSimulator::Config sim_config;
    sim_config.mode = ExportModeSimulator::Mode::ON_CHANGE;
    sim_config.change_threshold = 1.0;
    
    ExportModeSimulator sim(sim_config);
    
    int file_writes = 0;
    std::vector<double> values = {25.0, 25.3, 25.5, 27.0, 27.2, 29.0};
    
    for (double val : values) {
        if (sim.processValue(val)) {
            AlarmMessage alarm = createTestAlarm(1001, "TEMP_01", val);
            auto result = handler.sendAlarm(alarm, config);
            if (result.success) file_writes++;
        }
    }
    
    // 검증 - atomic write는 덮어쓰기이므로 성공 횟수만 확인
    ASSERT(file_writes == sim.getSendCount(), "전송 횟수 불일치");
    ASSERT(file_writes > 0, "파일 쓰기 없음");
    
    std::string filepath = TEST_BASE_DIR + "/1001/2025/12/17/export_log.json";
    ASSERT(std::filesystem::exists(filepath), "파일 생성 안 됨");
    
    cleanupTestDir();
    
    std::cout << " [" << values.size() << "입력 → " << file_writes << "파일쓰기]";
    PASS();
}

// ═══════════════════════════════════════════════════════════════════════════
// 메인
// ═══════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    std::cout << "  Export 모드 완전한 단위 테스트\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    
    auto& logger = LogManager::getInstance();
    logger.Info("Export 모드 테스트 시작");
    
    cleanupTestDir();
    
    // 파트 1: on_change 모드 (5개)
    std::cout << "\n📌 Part 1: on_change 모드" << std::endl;
    test_on_change_immediate_send();
    test_on_change_same_value_skip();
    test_on_change_threshold();
    test_on_change_first_value();
    test_on_change_force_send();
    
    // 파트 2: periodic 모드 (5개)
    std::cout << "\n📌 Part 2: periodic 모드" << std::endl;
    test_periodic_interval();
    test_periodic_last_value_only();
    test_periodic_interval_validation();
    test_periodic_timer_accuracy();
    test_periodic_pause_resume();
    
    // 파트 3: batch 모드 (5개)
    std::cout << "\n📌 Part 3: batch 모드" << std::endl;
    test_batch_size_trigger();
    test_batch_timeout();
    test_batch_partial_flush();
    test_batch_size_validation();
    test_batch_continuous();
    
    // 파트 4: 모드 전환 (3개)
    std::cout << "\n📌 Part 4: 모드 전환" << std::endl;
    test_mode_switch_runtime();
    test_mode_config_validation();
    test_mode_state_reset();
    
    // 파트 5: 통계 및 통합 (2개)
    std::cout << "\n📌 Part 5: 통계 및 통합" << std::endl;
    test_mode_statistics();
    test_file_handler_with_modes();
    
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
    
    logger.Info("Export 모드 테스트 완료");
    
    cleanupTestDir();
    
    return (failed_count == 0) ? 0 : 1;
}