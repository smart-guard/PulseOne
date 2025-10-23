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
#include "Coordinator/ExportCoordinator.h"
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
ExportCoordinatorConfig loadCoordinatorConfig() {
    ExportCoordinatorConfig config;
    
    try {
        ConfigManager& config_mgr = ConfigManager::getInstance();
        
        // Redis 설정
        config.redis_host = config_mgr.getOrDefault("REDIS_HOST", "localhost");
        config.redis_port = config_mgr.getInt("REDIS_PORT", 6379);
        config.redis_password = config_mgr.getOrDefault("REDIS_PASSWORD", "");
        
        // 스케줄 설정
        config.schedule_check_interval_seconds = 
            config_mgr.getInt("SCHEDULE_CHECK_INTERVAL_SEC", 10);
        config.schedule_reload_interval_seconds = 
            config_mgr.getInt("SCHEDULE_RELOAD_INTERVAL_SEC", 60);
        
        // 알람 설정
        config.alarm_worker_thread_count = 
            config_mgr.getInt("ALARM_WORKER_THREADS", 2);
        config.alarm_max_queue_size = 
            config_mgr.getInt("ALARM_MAX_QUEUE_SIZE", 10000);
        
        // 알람 구독 채널
        std::string channels_str = config_mgr.getOrDefault(
            "ALARM_SUBSCRIBE_CHANNELS", "alarms:all");
        std::stringstream ss(channels_str);
        std::string channel;
        while (std::getline(ss, channel, ',')) {
            channel.erase(0, channel.find_first_not_of(" \t"));
            channel.erase(channel.find_last_not_of(" \t") + 1);
            if (!channel.empty()) {
                config.alarm_subscribe_channels.push_back(channel);
            }
        }
        
        // 알람 옵션
        config.alarm_use_parallel_send = 
            config_mgr.getBool("ALARM_USE_PARALLEL_SEND", false);
        config.alarm_max_priority_filter = 
            config_mgr.getInt("ALARM_MAX_PRIORITY_FILTER", 1000);
        
        // 디버그
        config.enable_debug_log = config_mgr.getBool("DEBUG_MODE", false);
        
        LogManager::getInstance().Info("✅ Coordinator 설정 로드 완료");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Warn(
            "ConfigManager 설정 로드 실패, 기본값 사용: " + std::string(e.what()));
        
        // 기본값
        config.redis_host = "localhost";
        config.redis_port = 6379;
        config.schedule_check_interval_seconds = 10;
        config.schedule_reload_interval_seconds = 60;
        config.alarm_subscribe_channels = {"alarms:all"};
        config.alarm_worker_thread_count = 2;
        config.alarm_max_queue_size = 10000;
        config.alarm_use_parallel_send = false;
        config.enable_debug_log = false;
    }
    
    return config;
}

void logLoadedConfig(const ExportCoordinatorConfig& config) {
    std::cout << "\n";
    std::cout << "Export Coordinator 설정:\n";
    std::cout << "================================\n";
    
    std::cout << "\n[Redis 설정]\n";
    std::cout << "호스트: " << config.redis_host << "\n";
    std::cout << "포트: " << config.redis_port << "\n";
    std::cout << "비밀번호: " << (config.redis_password.empty() ? "(없음)" : "***설정됨***") << "\n";
    
    std::cout << "\n[스케줄 설정]\n";
    std::cout << "체크 간격: " << config.schedule_check_interval_seconds << "초\n";
    std::cout << "리로드 간격: " << config.schedule_reload_interval_seconds << "초\n";
    
    std::cout << "\n[알람 설정]\n";
    std::cout << "구독 채널 (" << config.alarm_subscribe_channels.size() << "개):\n";
    for (const auto& channel : config.alarm_subscribe_channels) {
        std::cout << "  - " << channel << "\n";
    }
    std::cout << "워커 스레드: " << config.alarm_worker_thread_count << "개\n";
    std::cout << "최대 큐 크기: " << config.alarm_max_queue_size << "\n";
    std::cout << "병렬 전송: " << (config.alarm_use_parallel_send ? "활성화" : "비활성화") << "\n";
    std::cout << "우선순위 필터: " << config.alarm_max_priority_filter << "\n";
    
    std::cout << "\n[고급 설정]\n";
    std::cout << "디버그 모드: " << (config.enable_debug_log ? "활성화" : "비활성화") << "\n";
    std::cout << "================================\n\n";
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
void testSchedule(ExportCoordinator& coordinator) {
    std::cout << "\n=== 스케줄 테스트 ===\n";
    
    try {
        // 모든 활성 스케줄 실행
        int executed = coordinator.executeAllSchedules();
        
        std::cout << "실행된 스케줄: " << executed << "개\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "스케줄 테스트 실패: " << e.what() << "\n";
    }
}

/**
 * @brief 통계 출력
 */
void printStatistics(ExportCoordinator& coordinator) {
    std::cout << "\n=== 시스템 통계 ===\n";
    
    try {
        auto stats = coordinator.getStatistics();
        
        std::cout << "\n[DynamicTargetManager]\n";
        if (stats.contains("dynamic_target_manager")) {
            auto dm_stats = stats["dynamic_target_manager"];
            std::cout << "  총 요청: " << dm_stats.value("total_requests", 0) << "\n";
            std::cout << "  성공: " << dm_stats.value("successful_requests", 0) << "\n";
            std::cout << "  실패: " << dm_stats.value("failed_requests", 0) << "\n";
        }
        
        std::cout << "\n[AlarmSubscriber]\n";
        if (stats.contains("alarm_subscriber")) {
            auto alarm_stats = stats["alarm_subscriber"];
            std::cout << "  수신: " << alarm_stats.value("total_received", 0) << "\n";
            std::cout << "  처리: " << alarm_stats.value("total_processed", 0) << "\n";
            std::cout << "  실패: " << alarm_stats.value("total_failed", 0) << "\n";
        }
        
        std::cout << "\n[ScheduledExporter]\n";
        if (stats.contains("scheduled_exporter")) {
            auto sched_stats = stats["scheduled_exporter"];
            std::cout << "  총 실행: " << sched_stats.value("total_executions", 0) << "\n";
            std::cout << "  성공: " << sched_stats.value("successful_executions", 0) << "\n";
            std::cout << "  실패: " << sched_stats.value("failed_executions", 0) << "\n";
        }
        
        std::cout << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "통계 조회 실패: " << e.what() << "\n";
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
    // 시그널 핸들러 등록
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    print_banner();
    
    try {
        // ============================================================
        // 1단계: ConfigManager 초기화
        // ============================================================
        std::cout << "1/3 ConfigManager 초기화 중...\n";
        ConfigManager::getInstance();
        std::cout << "✅ ConfigManager 초기화 완료\n";
        
        // ============================================================
        // 2단계: LogManager 초기화
        // ============================================================
        std::cout << "2/3 LogManager 초기화 중...\n";
        LogManager::getInstance();
        std::cout << "✅ LogManager 초기화 완료\n";
        
        // ============================================================
        // 3단계: 로그 시작
        // ============================================================
        std::cout << "3/3 Export Gateway v2.0 시작...\n\n";
        LogManager::getInstance().Info("=== Export Gateway v2.0 Starting ===");
        
        // ============================================================
        // 명령행 인자 파싱
        // ============================================================
        std::string config_file = "";
        std::string command = "";
        bool interactive_mode = false;
        
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            
            if (arg == "--help" || arg == "-h") {
                print_usage(argv[0]);
                return 0;
            }
            else if (arg == "--version" || arg == "-v") {
                std::cout << "PulseOne Export Gateway v2.0.0\n";
                return 0;
            }
            else if (arg.find("--config=") == 0) {
                config_file = arg.substr(9);
            }
            else if (arg == "--interactive") {
                interactive_mode = true;
            }
            else if (arg.find("--") == 0) {
                command = arg.substr(2);
            }
        }
        
        // ============================================================
        // 설정 로드
        // ============================================================
        auto config = loadCoordinatorConfig();
        logLoadedConfig(config);
        
        // ============================================================
        // ExportCoordinator 생성 및 설정
        // ============================================================
        LogManager::getInstance().Info("ExportCoordinator 초기화 중...");
        
        ExportCoordinator coordinator;
        coordinator.configure(config);
        
        // ============================================================
        // ExportCoordinator 시작
        // ============================================================
        if (!coordinator.start()) {
            std::cerr << "❌ ExportCoordinator 시작 실패\n";
            LogManager::getInstance().Error("Failed to start ExportCoordinator");
            return 1;
        }
        
        std::cout << "✅ Export Gateway v2.0 시작 완료!\n\n";
        LogManager::getInstance().Info("Export Gateway v2.0 started successfully");
        
        // ============================================================
        // 명령 처리
        // ============================================================
        if (command == "test-alarm") {
            testSingleAlarm();
        }
        else if (command == "test-targets") {
            testTargets();
        }
        else if (command == "test-schedule") {
            testSchedule(coordinator);
        }
        else if (command == "test-connection") {
            testConnection();
        }
        else if (command == "test-all") {
            std::cout << "모든 기능 테스트를 수행합니다...\n";
            testConnection();
            testTargets();
            testSingleAlarm();
            testSchedule(coordinator);
            printStatistics(coordinator);
            std::cout << "모든 테스트 완료!\n";
        }
        else if (interactive_mode) {
            runInteractiveMode(coordinator);
        }
        else {
            // 기본값: 데몬 모드
            runDaemonMode(coordinator);
        }
        
        // ============================================================
        // 정리 및 종료
        // ============================================================
        std::cout << "\n정리 중...\n";
        LogManager::getInstance().Info("Shutting down Export Gateway...");
        
        coordinator.stop();
        
        std::cout << "✅ Export Gateway v2.0 정상 종료\n";
        LogManager::getInstance().Info("=== Export Gateway v2.0 Stopped ===");
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ 치명적 오류: " << e.what() << "\n";
        LogManager::getInstance().Error("Fatal error: " + std::string(e.what()));
        return 1;
    }
}