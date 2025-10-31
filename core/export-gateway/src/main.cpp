/**
 * @file main.cpp - Export Gateway v2.0
 * @brief ExportCoordinator 기반 통합 아키텍처
 * @author PulseOne Development Team
 * @date 2025-10-23
 * @version 2.0.0
 * 
 * 주요 변경사항:
 * - ❌ CSPGateway 제거
 * - ✅ ExportCoordinator 사용
 * - ✅ DynamicTargetManager 싱글턴
 * - ✅ AlarmSubscriber + ScheduledExporter 통합
 * - ✅ 템플릿 기반 데이터 변환 지원
 */

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <memory>
#include <sstream>
#include <iomanip>

// ✅ v2.0 헤더
#include "CSP/ExportCoordinator.h"
#include "CSP/DynamicTargetManager.h"
#include "CSP/AlarmMessage.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"

using namespace PulseOne;
using namespace PulseOne::Coordinator;
using namespace PulseOne::CSP;

// 전역 종료 플래그
std::atomic<bool> g_shutdown_requested{false};

// 시그널 핸들러
void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ". Shutting down gracefully..." << std::endl;
    LogManager::getInstance().Info("Shutdown signal received: " + std::to_string(signal));
    g_shutdown_requested.store(true);
}

void print_banner() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║                    PulseOne Export Gateway                   ║
║                        Version 2.0.0                        ║
║          Coordinator + DynamicTargetManager + Templates     ║
╚══════════════════════════════════════════════════════════════╝
)" << std::endl;
}

void print_usage(const char* program_name) {
    std::cout << "사용법: " << program_name << " [옵션]\n\n";
    std::cout << "옵션:\n";
    std::cout << "  --help              도움말 출력\n";
    std::cout << "  --version           버전 정보 출력\n";
    std::cout << "  --config=PATH       설정 파일 경로 지정\n\n";
    std::cout << "테스트 옵션:\n";
    std::cout << "  --test-alarm        단일 테스트 알람 전송\n";
    std::cout << "  --test-targets      타겟 목록 출력\n";
    std::cout << "  --test-schedule     스케줄 실행 테스트\n";
    std::cout << "  --test-connection   연결 테스트\n";
    std::cout << "  --test-all          모든 기능 테스트\n\n";
    std::cout << "서비스 옵션:\n";
    std::cout << "  --daemon            데몬 모드로 실행 (기본값)\n";
    std::cout << "  --interactive       대화형 모드로 실행\n\n";
}

std::vector<int> parseBuildingIds(const std::string& building_ids_str) {
    std::vector<int> building_ids;
    std::stringstream ss(building_ids_str);
    std::string item;
    
    while (std::getline(ss, item, ',')) {
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        
        try {
            int building_id = std::stoi(item);
            if (building_id > 0) {
                building_ids.push_back(building_id);
            }
        } catch (const std::exception& e) {
            std::cout << "경고: 잘못된 빌딩 ID 무시: " << item << "\n";
        }
    }
    
    if (building_ids.empty()) {
        std::cout << "경고: 유효한 빌딩 ID가 없어 기본값(1001) 사용\n";
        building_ids.push_back(1001);
    }
    
    return building_ids;
}

/**
 * @brief ConfigManager에서 ExportCoordinatorConfig 로드
 */
PulseOne::Coordinator::ExportCoordinatorConfig loadCoordinatorConfig() {
    PulseOne::Coordinator::ExportCoordinatorConfig config;
    
    try {
        auto& cfg_mgr = ConfigManager::getInstance();
        
        // 데이터베이스 설정
        config.database_path = cfg_mgr.getOrDefault("DATABASE_PATH", "/app/data/db/pulseone.db");
        
        // Redis 설정
        config.redis_host = cfg_mgr.getOrDefault("REDIS_HOST", "localhost");
        config.redis_port = std::stoi(cfg_mgr.getOrDefault("REDIS_PORT", "6379"));
        config.redis_password = cfg_mgr.getOrDefault("REDIS_PASSWORD", "");
        
        // AlarmSubscriber 설정
        config.alarm_worker_threads = 
            std::stoi(cfg_mgr.getOrDefault("ALARM_WORKER_THREADS", "4"));
        
        config.alarm_max_queue_size = 
            std::stoi(cfg_mgr.getOrDefault("ALARM_MAX_QUEUE_SIZE", "10000"));
        
        // 구독 채널 설정
        std::string channels = cfg_mgr.getOrDefault("ALARM_CHANNELS", "");
        if (!channels.empty()) {
            config.alarm_channels.clear();
            std::stringstream ss(channels);
            std::string channel;
            while (std::getline(ss, channel, ',')) {
                config.alarm_channels.push_back(channel);
            }
        }
        
        // ScheduledExporter 설정
        config.schedule_check_interval_seconds = 
            std::stoi(cfg_mgr.getOrDefault("SCHEDULE_CHECK_INTERVAL", "10"));
        
        config.schedule_reload_interval_seconds = 
            std::stoi(cfg_mgr.getOrDefault("SCHEDULE_RELOAD_INTERVAL", "60"));
        
        config.schedule_batch_size = 
            std::stoi(cfg_mgr.getOrDefault("SCHEDULE_BATCH_SIZE", "100"));
        
        // 공통 설정
        config.enable_debug_log = 
            (cfg_mgr.getOrDefault("ENABLE_DEBUG_LOG", "false") == "true");
        
        config.log_retention_days = 
            std::stoi(cfg_mgr.getOrDefault("LOG_RETENTION_DAYS", "30"));
        
        // 성능 설정
        config.max_concurrent_exports = 
            std::stoi(cfg_mgr.getOrDefault("MAX_CONCURRENT_EXPORTS", "50"));
        
        config.export_timeout_seconds = 
            std::stoi(cfg_mgr.getOrDefault("EXPORT_TIMEOUT_SECONDS", "30"));
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("설정 로드 실패, 기본값 사용: " + std::string(e.what()));
        
        // 기본값 설정
        config.alarm_channels = {"alarms:all"};
        config.alarm_worker_threads = 2;
        config.alarm_max_queue_size = 5000;
    }
    
    return config;
}

void logLoadedConfig(const PulseOne::Coordinator::ExportCoordinatorConfig& config) {
    std::cout << "\n========================================\n";
    std::cout << "Export Coordinator 설정:\n";
    std::cout << "========================================\n";
    std::cout << "데이터베이스: " << config.database_path << "\n";
    std::cout << "Redis: " << config.redis_host << ":" << config.redis_port << "\n";
    std::cout << "\n[AlarmSubscriber 설정]\n";
    std::cout << "구독 채널 (" << config.alarm_channels.size() << "개):\n";
    for (const auto& channel : config.alarm_channels) {
        std::cout << "  - " << channel << "\n";
    }
    std::cout << "워커 스레드: " << config.alarm_worker_threads << "개\n";
    std::cout << "최대 큐 크기: " << config.alarm_max_queue_size << "\n";
    
    std::cout << "\n[ScheduledExporter 설정]\n";
    std::cout << "체크 간격: " << config.schedule_check_interval_seconds << "초\n";
    std::cout << "리로드 간격: " << config.schedule_reload_interval_seconds << "초\n";
    std::cout << "배치 크기: " << config.schedule_batch_size << "\n";
    
    std::cout << "\n[공통 설정]\n";
    std::cout << "디버그 로그: " << (config.enable_debug_log ? "활성화" : "비활성화") << "\n";
    std::cout << "로그 보관 기간: " << config.log_retention_days << "일\n";
    std::cout << "최대 동시 Export: " << config.max_concurrent_exports << "\n";
    std::cout << "Export 타임아웃: " << config.export_timeout_seconds << "초\n";
    std::cout << "========================================\n\n";
}
/**
 * @brief 테스트: 단일 알람 전송
 */
void testSingleAlarm() {
    std::cout << "\n=== 단일 알람 테스트 ===\n";
    
    try {
        // DynamicTargetManager 싱글턴 사용
        auto& manager = DynamicTargetManager::getInstance();
        
        // 테스트 알람 생성
        AlarmMessage alarm;
        alarm.bd = 1001;
        alarm.nm = "TEST_POINT_001";
        alarm.vl = 85.5;
        alarm.al = 1;
        alarm.st = 1;
        alarm.tm = std::to_string(
            std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        
        LogManager::getInstance().Info("테스트 알람 전송: " + alarm.nm);
        
        // 모든 타겟으로 전송
        auto results = manager.sendAlarmToAllTargets(alarm);
        
        std::cout << "전송 결과:\n";
        std::cout << "  총 타겟 수: " << results.size() << "\n";
        
        int success_count = 0;
        int failure_count = 0;
        
        for (const auto& result : results) {
            if (result.success) {
                success_count++;
                std::cout << "  ✅ " << result.target_name << " - 성공\n";
            } else {
                failure_count++;
                std::cout << "  ❌ " << result.target_name << " - 실패: " 
                         << result.error_message << "\n";
            }
        }
        
        std::cout << "\n성공: " << success_count << " / 실패: " << failure_count << "\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "테스트 실패: " << e.what() << "\n";
        LogManager::getInstance().Error("Test alarm failed: " + std::string(e.what()));
    }
}

/**
 * @brief 테스트: 타겟 목록 출력
 */
void testTargets() {
    std::cout << "\n=== 타겟 목록 ===\n";
    
    try {
        auto& manager = DynamicTargetManager::getInstance();
        
        auto target_names = manager.getTargetNames();
        
        std::cout << "총 타겟 수: " << target_names.size() << "\n\n";
        
        for (size_t i = 0; i < target_names.size(); ++i) {
            const auto& name = target_names[i];
            
            std::cout << (i + 1) << ". " << name;
            
            // 타겟 정보 조회
            auto target_opt = manager.getTarget(name);
            if (target_opt.has_value()) {
                const auto& target = target_opt.value();
                std::cout << " (" << target.type << ")";
                std::cout << " - " << (target.enabled ? "활성화" : "비활성화");
            }
            
            std::cout << "\n";
        }
        
        std::cout << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "타겟 목록 조회 실패: " << e.what() << "\n";
    }
}

/**
 * @brief 테스트: 연결 테스트
 */
void testConnection() {
    std::cout << "\n=== 연결 테스트 ===\n";
    
    try {
        auto& manager = DynamicTargetManager::getInstance();
        
        auto results = manager.testAllConnections();
        
        std::cout << "총 타겟 수: " << results.size() << "\n\n";
        
        int success_count = 0;
        for (const auto& [name, ok] : results) {
            std::cout << (ok ? "✅" : "❌") << " " << name << "\n";
            if (ok) success_count++;
        }
        
        std::cout << "\n성공: " << success_count << " / " << results.size() << "\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "연결 테스트 실패: " << e.what() << "\n";
    }
}

/**
 * @brief 테스트: 스케줄 실행
 */
void testSchedule(PulseOne::Coordinator::ExportCoordinator& coordinator) {
    std::cout << "\n========================================\n";
    std::cout << "스케줄 Export 테스트\n";
    std::cout << "========================================\n";
    
    try {
        // getComponentStatus() 사용
        auto status = coordinator.getComponentStatus();
        
        std::cout << "ScheduledExporter 상태: " 
                  << (status["scheduled_exporter"].get<bool>() ? "실행 중" : "중지됨") 
                  << "\n";
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        std::cout << "스케줄 실행 대기 중...\n";
        
    } catch (const std::exception& e) {
        std::cout << "스케줄 테스트 실패: " << e.what() << "\n";
    }
}

/**
 * @brief 통계 출력
 */
void printStatistics(PulseOne::Coordinator::ExportCoordinator& coordinator) {
    std::cout << "\n========================================\n";
    std::cout << "Export 통계\n";
    std::cout << "========================================\n";
    
    try {
        auto stats = coordinator.getStats();  // ✅ getStatistics() → getStats()
        
        std::cout << "전체 Export: " << stats.total_exports << "\n";
        std::cout << "성공: " << stats.successful_exports << "\n";
        std::cout << "실패: " << stats.failed_exports << "\n";
        
        if (stats.total_exports > 0) {
            double success_rate = (double)stats.successful_exports / stats.total_exports * 100.0;
            std::cout << "성공률: " << std::fixed << std::setprecision(2) 
                      << success_rate << "%\n";
        }
        
        std::cout << "\n알람 이벤트: " << stats.alarm_events << "\n";
        std::cout << "알람 Export: " << stats.alarm_exports << "\n";
        std::cout << "스케줄 실행: " << stats.schedule_executions << "\n";
        std::cout << "스케줄 Export: " << stats.schedule_exports << "\n";
        
        std::cout << "\n평균 처리 시간: " << std::fixed << std::setprecision(2) 
                  << stats.avg_processing_time_ms << "ms\n";
        
        std::cout << "========================================\n";
        
    } catch (const std::exception& e) {
        std::cout << "통계 조회 실패: " << e.what() << "\n";
    }
}

/**
 * @brief 데몬 모드 실행
 */
void runDaemonMode(ExportCoordinator& coordinator) {
    LogManager::getInstance().Info("데몬 모드 시작");
    std::cout << "데몬 모드로 실행 중...\n";
    std::cout << "종료하려면 Ctrl+C를 누르세요.\n\n";
    
    // 통계 출력 간격 (60초)
    int stats_counter = 0;
    const int stats_interval = 60;
    
    while (!g_shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // 통계 출력
        stats_counter++;
        if (stats_counter >= stats_interval) {
            printStatistics(coordinator);
            stats_counter = 0;
        }
    }
    
    LogManager::getInstance().Info("데몬 모드 종료");
}

/**
 * @brief 대화형 모드 실행
 */
void runInteractiveMode(ExportCoordinator& coordinator) {
    LogManager::getInstance().Info("대화형 모드 시작");
    std::cout << "대화형 모드로 실행 중...\n";
    std::cout << "명령어: status, test, targets, schedule, quit\n\n";
    
    std::string command;
    while (!g_shutdown_requested.load()) {
        std::cout << "> ";
        std::getline(std::cin, command);
        
        if (command == "quit" || command == "exit") {
            break;
        }
        else if (command == "status") {
            printStatistics(coordinator);
        }
        else if (command == "test") {
            testSingleAlarm();
        }
        else if (command == "targets") {
            testTargets();
        }
        else if (command == "schedule") {
            testSchedule(coordinator);
        }
        else if (command == "connection") {
            testConnection();
        }
        else if (command == "help") {
            std::cout << "명령어:\n";
            std::cout << "  status      - 통계 출력\n";
            std::cout << "  test        - 테스트 알람 전송\n";
            std::cout << "  targets     - 타겟 목록\n";
            std::cout << "  schedule    - 스케줄 실행\n";
            std::cout << "  connection  - 연결 테스트\n";
            std::cout << "  quit/exit   - 종료\n";
        }
        else {
            std::cout << "알 수 없는 명령어. 'help' 입력\n";
        }
    }
    
    LogManager::getInstance().Info("대화형 모드 종료");
}

/**
 * @brief 메인 함수 - v2.0 아키텍처
 */
int main(int argc, char* argv[]) {
    try {
        std::cout << "===========================================\n";
        std::cout << "PulseOne Export Gateway v1.0.0\n";
        std::cout << "===========================================\n\n";
        
        // 1. 설정 로드
        auto config = loadCoordinatorConfig();
        logLoadedConfig(config);
        
        // 2. ExportCoordinator 생성 (config 필수)
        PulseOne::Coordinator::ExportCoordinator coordinator(config);
        
        // 3. ExportCoordinator 시작
        if (!coordinator.start()) {
            std::cerr << "ExportCoordinator 시작 실패\n";
            return 1;
        }
        
        std::cout << "ExportCoordinator 시작 완료 ✅\n\n";
        
        // 4. 상태 확인 - getComponentStatus() 사용
        auto status = coordinator.getComponentStatus();
        std::cout << "시스템 상태:\n";
        std::cout << "  - Running: " << coordinator.isRunning() << "\n";
        std::cout << "  - AlarmSubscriber: " << status["alarm_subscriber"].get<bool>() << "\n";
        std::cout << "  - ScheduledExporter: " << status["scheduled_exporter"].get<bool>() << "\n";
        std::cout << "  - TargetManager: " << status["target_manager"].get<bool>() << "\n\n";
        
        // 5. 테스트 실행
        std::cout << "테스트 시작...\n\n";
        
        // 스케줄 테스트
        testSchedule(coordinator);
        
        // 통계 출력
        std::this_thread::sleep_for(std::chrono::seconds(5));
        printStatistics(coordinator);
        
        // 6. 종료 대기
        std::cout << "\nExport Gateway 실행 중... (Ctrl+C로 종료)\n";
        
        // 신호 처리 대기
        std::mutex mtx;
        std::condition_variable cv;
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock);
        
        // 7. 정리
        coordinator.stop();
        std::cout << "\nExport Gateway 종료 완료\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "심각한 에러: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}