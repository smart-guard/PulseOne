/**
 * @file main.cpp - 순환 초기화 문제 해결 버전
 * @brief 초기화 순서: ConfigManager → LogManager → CSPGateway
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

#include "CSP/CSPGateway.h"
#include "CSP/AlarmMessage.h"

#ifdef HAS_SHARED_LIBS
    #include "Utils/LogManager.h"
    #include "Utils/ConfigManager.h"
#endif

using namespace PulseOne::CSP;

// 전역 종료 플래그
std::atomic<bool> g_shutdown_requested{false};

// 시그널 핸들러
void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ". Shutting down gracefully..." << std::endl;
    g_shutdown_requested.store(true);
}

void print_banner() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║                    PulseOne Export Gateway                   ║
║                        Version 1.0.0                        ║
║               CSP Gateway + Multi-Building Support          ║
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
    std::cout << "  --test-multi        멀티빌딩 테스트 알람 전송\n";
    std::cout << "  --test-batch        배치 알람 테스트 (시간 필터링 포함)\n";
    std::cout << "  --test-connection   연결 테스트\n";
    std::cout << "  --test-all          모든 기능 테스트\n";
    std::cout << "  --cleanup           오래된 파일 정리\n\n";
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

void validateConfig(const CSPGatewayConfig& config) {
    std::vector<std::string> warnings;
    
    if (config.building_id.empty()) {
        warnings.push_back("CSP_BUILDING_ID가 설정되지 않음");
    }
    
    if (config.use_api && config.api_endpoint.empty()) {
        warnings.push_back("API 사용이 활성화되었지만 CSP_API_ENDPOINT가 설정되지 않음");
    }
    
    if (config.use_api && config.api_key.empty()) {
        warnings.push_back("API 사용이 활성화되었지만 API 키가 설정되지 않음");
    }
    
    if (config.use_s3 && config.s3_bucket_name.empty()) {
        warnings.push_back("S3 사용이 활성화되었지만 버킷명이 설정되지 않음");
    }
    
    if (config.use_s3 && (config.s3_access_key.empty() || config.s3_secret_key.empty())) {
        warnings.push_back("S3 사용이 활성화되었지만 액세스 키가 설정되지 않음");
    }
    
    if (config.alarm_ignore_minutes < 0) {
        warnings.push_back("알람 무시 시간이 음수입니다");
    }
    
    if (!warnings.empty()) {
        std::cout << "\n설정 검증 경고:\n";
        for (const auto& warning : warnings) {
            std::cout << "  ⚠️ " << warning << "\n";
        }
        std::cout << "\n";
    }
}

void logLoadedConfig(const CSPGatewayConfig& config) {
    std::cout << "CSP Gateway 설정 로드 완료:\n";
    std::cout << "================================\n";
    std::cout << "Building ID: " << config.building_id << "\n";
    std::cout << "API Endpoint: " << (config.api_endpoint.empty() ? "(설정 안됨)" : config.api_endpoint) << "\n";
    std::cout << "API 사용: " << (config.use_api ? "예" : "아니오") << "\n";
    std::cout << "API 키: " << (config.api_key.empty() ? "(설정 안됨)" : "***설정됨***") << "\n";
    std::cout << "S3 사용: " << (config.use_s3 ? "예" : "아니오") << "\n";
    std::cout << "S3 버킷: " << (config.s3_bucket_name.empty() ? "(설정 안됨)" : config.s3_bucket_name) << "\n";
    
    std::cout << "\n멀티빌딩 설정:\n";
    std::cout << "멀티빌딩 모드: " << (config.multi_building_enabled ? "활성화" : "비활성화") << "\n";
    std::cout << "지원 빌딩 수: " << config.supported_building_ids.size() << "\n";
    std::cout << "지원 빌딩 ID: ";
    for (size_t i = 0; i < config.supported_building_ids.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << config.supported_building_ids[i];
    }
    std::cout << "\n";
    
    std::cout << "\n시간 필터링 설정:\n";
    std::cout << "알람 무시 시간: " << config.alarm_ignore_minutes << "분\n";
    std::cout << "로컬 시간 사용: " << (config.use_local_time ? "예" : "아니오") << "\n";
    
    std::cout << "\n배치 파일 설정:\n";
    std::cout << "알람 디렉토리: " << config.alarm_dir_path << "\n";
    std::cout << "실패 파일 디렉토리: " << config.failed_file_path << "\n";
    std::cout << "자동 정리: " << (config.auto_cleanup_success_files ? "활성화" : "비활성화") << "\n";
    std::cout << "실패 파일 보관: " << config.keep_failed_files_days << "일\n";
    std::cout << "최대 배치 크기: " << config.max_batch_size << "\n";
    
    std::cout << "\n고급 설정:\n";
    std::cout << "디버그 모드: " << (config.debug_mode ? "활성화" : "비활성화") << "\n";
    std::cout << "최대 재시도: " << config.max_retry_attempts << "회\n";
    std::cout << "초기 지연: " << config.initial_delay_ms << "ms\n";
    std::cout << "API 타임아웃: " << config.api_timeout_sec << "초\n";
    std::cout << "================================\n\n";
}

CSPGatewayConfig loadConfigFromFiles() {
    CSPGatewayConfig config;
    
#ifdef HAS_SHARED_LIBS
    ConfigManager& config_mgr = ConfigManager::getInstance();
    
    try {
        config.building_id = config_mgr.getOrDefault("CSP_BUILDING_ID", "1001");
        config.api_endpoint = config_mgr.getOrDefault("CSP_API_ENDPOINT", "");
        config.api_timeout_sec = config_mgr.getInt("CSP_API_TIMEOUT_SEC", 30);
        config.debug_mode = config_mgr.getBool("CSP_DEBUG_MODE", false);
        config.max_retry_attempts = config_mgr.getInt("CSP_MAX_RETRY_ATTEMPTS", 3);
        config.initial_delay_ms = config_mgr.getInt("CSP_INITIAL_DELAY_MS", 1000);
        config.max_queue_size = config_mgr.getInt("CSP_MAX_QUEUE_SIZE", 10000);
        
        try {
            config.api_key = config_mgr.loadPasswordFromFile("CSP_API_KEY_FILE");
            config.s3_access_key = config_mgr.loadPasswordFromFile("CSP_S3_ACCESS_KEY_FILE");
            config.s3_secret_key = config_mgr.loadPasswordFromFile("CSP_S3_SECRET_KEY_FILE");
        } catch (const std::exception& e) {
            std::cout << "경고: 암호화된 설정 로드 실패, 기본값 사용: " << e.what() << "\n";
            config.api_key = "test_api_key_from_config";
            config.s3_access_key = "";
            config.s3_secret_key = "";
        }
        
        config.use_s3 = config_mgr.getBool("CSP_USE_S3", false);
        config.s3_endpoint = config_mgr.getOrDefault("CSP_S3_ENDPOINT", "https://s3.amazonaws.com");
        config.s3_bucket_name = config_mgr.getOrDefault("CSP_S3_BUCKET_NAME", "");
        config.s3_region = config_mgr.getOrDefault("CSP_S3_REGION", "us-east-1");
        config.use_api = config_mgr.getBool("CSP_USE_API", true);
        
        config.use_dynamic_targets = config_mgr.getBool("CSP_USE_DYNAMIC_TARGETS", false);
        config.dynamic_targets_config_path = config_mgr.getOrDefault("CSP_DYNAMIC_TARGETS_CONFIG", "");
        
        config.multi_building_enabled = config_mgr.getBool("CSP_MULTI_BUILDING_ENABLED", true);
        std::string building_ids_str = config_mgr.getOrDefault("CSP_SUPPORTED_BUILDING_IDS", "1001,1002,1003");
        config.supported_building_ids = parseBuildingIds(building_ids_str);
        
        config.alarm_ignore_minutes = config_mgr.getInt("CSP_ALARM_IGNORE_MINUTES", 5);
        config.use_local_time = config_mgr.getBool("CSP_USE_LOCAL_TIME", true);
        
        config.alarm_dir_path = config_mgr.getOrDefault("CSP_ALARM_DIR_PATH", "./alarm_files");
        config.failed_file_path = config_mgr.getOrDefault("CSP_FAILED_FILE_PATH", "./failed_alarms");
        config.auto_cleanup_success_files = config_mgr.getBool("CSP_AUTO_CLEANUP_SUCCESS_FILES", true);
        config.keep_failed_files_days = config_mgr.getInt("CSP_KEEP_FAILED_FILES_DAYS", 7);
        config.max_batch_size = config_mgr.getInt("CSP_MAX_BATCH_SIZE", 1000);
        config.batch_timeout_ms = config_mgr.getInt("CSP_BATCH_TIMEOUT_MS", 5000);
        
        std::cout << "✅ ConfigManager에서 설정 로드 완료\n";
        
    } catch (const std::exception& e) {
        std::cout << "⚠️ ConfigManager 설정 로드 실패, 기본값 사용: " << e.what() << "\n";
        config.building_id = "1001";
        config.api_endpoint = "https://api.example.com/alarms";
        config.api_key = "test_api_key_default";
        config.multi_building_enabled = true;
        config.supported_building_ids = {1001, 1002, 1003};
        config.alarm_ignore_minutes = 5;
        config.use_local_time = true;
        config.alarm_dir_path = "./test_alarm_files";
        config.auto_cleanup_success_files = true;
        config.keep_failed_files_days = 7;
        config.max_batch_size = 100;
        config.use_s3 = false;
    }
    
#else
    std::cout << "⚠️ ConfigManager를 사용할 수 없어 하드코딩된 기본 설정 사용\n";
    config.building_id = "1001";
    config.api_endpoint = "https://api.example.com/alarms";
    config.api_key = "test_api_key_hardcoded";
    config.debug_mode = true;
    config.multi_building_enabled = true;
    config.supported_building_ids = {1001, 1002, 1003};
    config.alarm_ignore_minutes = 5;
    config.use_local_time = true;
    config.alarm_dir_path = "./test_alarm_files";
    config.auto_cleanup_success_files = true;
    config.keep_failed_files_days = 7;
    config.max_batch_size = 100;
    config.use_s3 = false;
#endif
    
    return config;
}

// 테스트 함수들은 동일하게 유지 (생략)
std::vector<AlarmMessage> createTestAlarms() {
    std::vector<AlarmMessage> alarms;
    
    auto alarm1 = AlarmMessage::create_sample(1001, "Tank001.Level", 85.5, true);
    alarm1.des = "Tank Level High - 85.5%";
    alarms.push_back(alarm1);
    
    auto alarm2 = AlarmMessage::create_sample(1001, "Pump001.Status", 0.0, false);
    alarm2.des = "Pump Status Normal";
    alarms.push_back(alarm2);
    
    auto alarm3 = AlarmMessage::create_sample(1002, "Reactor.Temperature", 120.0, true);
    alarm3.des = "CRITICAL Temperature High - 120°C";
    alarms.push_back(alarm3);
    
    return alarms;
}


void testSingleAlarm(CSPGateway& gateway) {
    std::cout << "\n=== 단일 알람 테스트 ===\n";
    
    auto result = gateway.sendTestAlarm();
    
    std::cout << "테스트 알람 전송 결과:\n";
    std::cout << "  성공: " << (result.success ? "예" : "아니오") << "\n";
    std::cout << "  상태 코드: " << result.status_code << "\n";
    
    if (!result.success) {
        std::cout << "  오류 메시지: " << result.error_message << "\n";
    }
    
    std::cout << "\n";
}


void testMultiBuildingAlarms(CSPGateway& gateway) {
    std::cout << "\n=== 멀티빌딩 테스트 ===\n";
    auto alarms = createTestAlarms();
    auto grouped = gateway.groupAlarmsByBuilding(alarms);
    auto results = gateway.processMultiBuildingAlarms(grouped);
    std::cout << "처리 완료: " << results.size() << "개 빌딩\n\n";
}


void testBatchAndTimeFiltering(CSPGateway& gateway) {
    std::cout << "\n=== 배치/시간필터 테스트 ===\n";
    auto alarms = createTestAlarms();
    auto filtered = gateway.filterIgnoredAlarms(alarms);
    std::cout << "필터링 완료: " << filtered.size() << "개 알람\n\n";
}


void testConnection(CSPGateway& gateway) {
    std::cout << "\n=== 연결 테스트 ===\n";
    bool ok = gateway.testConnection();
    std::cout << "연결 상태: " << (ok ? "OK" : "FAILED") << "\n\n";
}


void printStatistics(CSPGateway& gateway) {
    std::cout << "\n=== 통계 ===\n";
    auto stats = gateway.getStats();
    std::cout << "전체 알람: " << stats.total_alarms << "\n\n";
}

void testCleanup(CSPGateway& gateway) {
    std::cout << "\n=== 정리 테스트 ===\n";
    size_t deleted = gateway.cleanupOldFailedFiles(1);
    std::cout << "삭제된 파일: " << deleted << "\n\n";
}

void runDaemonMode(CSPGateway& gateway) {
    std::cout << "데몬 모드는 구현 예정\n";
}

void runInteractiveMode(CSPGateway& gateway) {
    std::cout << "대화형 모드는 구현 예정\n";
}

/**
 * @brief 메인 함수 - 초기화 순서 강제
 */
int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    print_banner();
    
    try {
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
                std::cout << "PulseOne Export Gateway v1.0.0\n";
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
        // 핵심 수정: 명시적 초기화 순서 보장
        // ============================================================
#ifdef HAS_SHARED_LIBS
        // 1단계: ConfigManager 먼저 초기화 (LogManager 호출 안함)
        std::cout << "1/2 ConfigManager 초기화 중...\n";
        ConfigManager::getInstance();  // 명시적 초기화
        std::cout << "✅ ConfigManager 초기화 완료\n";
        
        // 2단계: 이제 LogManager 안전하게 초기화 가능
        std::cout << "2/2 LogManager 초기화 중...\n";
        LogManager::getInstance();  // 명시적 초기화
        std::cout << "✅ LogManager 초기화 완료\n\n";
        
        // 3단계: 이제 LogManager 안전하게 사용 가능
        LogManager::getInstance().Info("Export Gateway starting...");
#endif
        
        // 설정 로딩
        auto config = loadConfigFromFiles();
        validateConfig(config);
        logLoadedConfig(config);
        
        // CSPGateway 생성
        CSPGateway gateway(config);
        
        // 명령 처리
        if (command == "test-alarm") {
            testSingleAlarm(gateway);
        }
        else if (command == "test-multi") {
            testMultiBuildingAlarms(gateway);
        }
        else if (command == "test-batch") {
            testBatchAndTimeFiltering(gateway);
        }
        else if (command == "test-connection") {
            testConnection(gateway);
        }
        else if (command == "test-all") {
            std::cout << "모든 기능 테스트를 수행합니다...\n\n";
            testConnection(gateway);
            testSingleAlarm(gateway);
            testMultiBuildingAlarms(gateway);
            testBatchAndTimeFiltering(gateway);
            testCleanup(gateway);
            printStatistics(gateway);
            std::cout << "모든 테스트 완료!\n";
        }
        else if (command == "cleanup") {
            testCleanup(gateway);
        }
        else if (interactive_mode) {
            runInteractiveMode(gateway);
        }
        else {
            runDaemonMode(gateway);
        }
        
#ifdef HAS_SHARED_LIBS
        LogManager::getInstance().Info("Export Gateway finished");
#endif
        
        std::cout << "\nPulseOne Export Gateway 종료\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "치명적 오류: " << e.what() << "\n";
#ifdef HAS_SHARED_LIBS
        LogManager::getInstance().Error("Fatal error: " + std::string(e.what()));
#endif
        return 1;
    }
}