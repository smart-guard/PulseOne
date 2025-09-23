/**
 * @file main.cpp - 완전한 최종 버전
 * @brief PulseOne Export Gateway 메인 진입점 (외부 설정 + 멀티빌딩 + 시간필터 + 배치관리)
 * @author PulseOne Development Team
 * @date 2025-09-23
 * 저장 위치: core/export-gateway/src/main.cpp
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

/**
 * @brief 배너 출력
 */
void print_banner() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║                    PulseOne Export Gateway                   ║
║                        Version 1.0.0                        ║
║               CSP Gateway + Multi-Building Support          ║
╚══════════════════════════════════════════════════════════════╝
)" << std::endl;
}

/**
 * @brief 도움말 출력
 */
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
    std::cout << "예제:\n";
    std::cout << "  " << program_name << " --test-multi\n";
    std::cout << "  " << program_name << " --daemon\n";
    std::cout << "  " << program_name << " --config=/etc/csp-gateway.conf\n";
}

/**
 * @brief 빌딩 ID 문자열 파싱 ("1001,1002,1003" → vector<int>)
 */
std::vector<int> parseBuildingIds(const std::string& building_ids_str) {
    std::vector<int> building_ids;
    std::stringstream ss(building_ids_str);
    std::string item;
    
    while (std::getline(ss, item, ',')) {
        // 공백 제거
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
 * @brief 설정 검증
 */
void validateConfig(const CSPGatewayConfig& config) {
    std::vector<std::string> warnings;
    
    // 필수 설정 검사
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
    
    // 경고 출력
    if (!warnings.empty()) {
        std::cout << "\n설정 검증 경고:\n";
        for (const auto& warning : warnings) {
            std::cout << "  ⚠️ " << warning << "\n";
        }
        std::cout << "\n";
    }
}

/**
 * @brief 로드된 설정 로그 출력
 */
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

/**
 * @brief 외부 설정 파일에서 CSP Gateway 설정 로드
 */
CSPGatewayConfig loadConfigFromFiles() {
    CSPGatewayConfig config;
    
#ifdef HAS_SHARED_LIBS
    ConfigManager& config_mgr = ConfigManager::getInstance();
    
    try {
        // =================================================================
        // 1. 기본 설정 (평문)
        // =================================================================
        config.building_id = config_mgr.getOrDefault("CSP_BUILDING_ID", "1001");
        config.api_endpoint = config_mgr.getOrDefault("CSP_API_ENDPOINT", "");
        config.api_timeout_sec = config_mgr.getInt("CSP_API_TIMEOUT_SEC", 30);
        config.debug_mode = config_mgr.getBool("CSP_DEBUG_MODE", false);
        config.max_retry_attempts = config_mgr.getInt("CSP_MAX_RETRY_ATTEMPTS", 3);
        config.initial_delay_ms = config_mgr.getInt("CSP_INITIAL_DELAY_MS", 1000);
        config.max_queue_size = config_mgr.getInt("CSP_MAX_QUEUE_SIZE", 10000);
        
        // =================================================================
        // 2. 암호화된 설정 (secrets/)
        // =================================================================
        try {
            // API 키 (암호화된 파일에서 로드)
            config.api_key = config_mgr.loadPasswordFromFile("CSP_API_KEY_FILE");
            
            // S3 키들 (암호화된 파일에서 로드)
            config.s3_access_key = config_mgr.loadPasswordFromFile("CSP_S3_ACCESS_KEY_FILE");
            config.s3_secret_key = config_mgr.loadPasswordFromFile("CSP_S3_SECRET_KEY_FILE");
            
        } catch (const std::exception& e) {
            std::cout << "경고: 암호화된 설정 로드 실패, 기본값 사용: " << e.what() << "\n";
            // 기본 테스트 값들 설정
            config.api_key = "test_api_key_from_config";
            config.s3_access_key = "";
            config.s3_secret_key = "";
        }
        
        // =================================================================
        // 3. S3 설정
        // =================================================================
        config.use_s3 = config_mgr.getBool("CSP_USE_S3", false);
        config.s3_endpoint = config_mgr.getOrDefault("CSP_S3_ENDPOINT", "https://s3.amazonaws.com");
        config.s3_bucket_name = config_mgr.getOrDefault("CSP_S3_BUCKET_NAME", "");
        config.s3_region = config_mgr.getOrDefault("CSP_S3_REGION", "us-east-1");
        config.use_api = config_mgr.getBool("CSP_USE_API", true);
        
        // =================================================================
        // 4. 멀티빌딩 설정
        // =================================================================
        config.multi_building_enabled = config_mgr.getBool("CSP_MULTI_BUILDING_ENABLED", true);
        
        // 지원 빌딩 ID 목록 파싱 (콤마 구분: "1001,1002,1003")
        std::string building_ids_str = config_mgr.getOrDefault("CSP_SUPPORTED_BUILDING_IDS", "1001,1002,1003");
        config.supported_building_ids = parseBuildingIds(building_ids_str);
        
        // =================================================================
        // 5. 알람 무시 시간 설정
        // =================================================================
        config.alarm_ignore_minutes = config_mgr.getInt("CSP_ALARM_IGNORE_MINUTES", 5);
        config.use_local_time = config_mgr.getBool("CSP_USE_LOCAL_TIME", true);
        
        // =================================================================
        // 6. 배치 파일 관리 설정
        // =================================================================
        config.alarm_dir_path = config_mgr.getOrDefault("CSP_ALARM_DIR_PATH", "./alarm_files");
        config.failed_file_path = config_mgr.getOrDefault("CSP_FAILED_FILE_PATH", "./failed_alarms");
        config.auto_cleanup_success_files = config_mgr.getBool("CSP_AUTO_CLEANUP_SUCCESS_FILES", true);
        config.keep_failed_files_days = config_mgr.getInt("CSP_KEEP_FAILED_FILES_DAYS", 7);
        config.max_batch_size = config_mgr.getInt("CSP_MAX_BATCH_SIZE", 1000);
        config.batch_timeout_ms = config_mgr.getInt("CSP_BATCH_TIMEOUT_MS", 5000);
        
        std::cout << "✅ ConfigManager에서 설정 로드 완료\n";
        
    } catch (const std::exception& e) {
        std::cout << "⚠️ ConfigManager 설정 로드 실패, 기본값 사용: " << e.what() << "\n";
        // 기본값으로 fallback
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
    // Shared 라이브러리가 없을 때 기본값
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

/**
 * @brief 테스트 알람 메시지들 생성
 */
std::vector<AlarmMessage> createTestAlarms() {
    std::vector<AlarmMessage> alarms;
    
    // 빌딩 1001 알람들 (최근)
    auto alarm1 = AlarmMessage::create_sample(1001, "Tank001.Level", 85.5, true);
    alarm1.des = "Tank Level High - 85.5%";
    alarms.push_back(alarm1);
    
    auto alarm2 = AlarmMessage::create_sample(1001, "Pump001.Status", 0.0, false);
    alarm2.des = "Pump Status Normal";
    alarms.push_back(alarm2);
    
    // 빌딩 1002 알람들 (최근)
    auto alarm3 = AlarmMessage::create_sample(1002, "Reactor.Temperature", 120.0, true);
    alarm3.des = "CRITICAL Temperature High - 120°C";
    alarms.push_back(alarm3);
    
    // 빌딩 1003 알람들 (최근)
    auto alarm4 = AlarmMessage::create_sample(1003, "HVAC.Airflow", 45.2, true);
    alarm4.des = "HVAC Airflow Low - 45.2 m³/h";
    alarms.push_back(alarm4);
    
    // 오래된 알람 (무시될 예정 - 시간 필터링 테스트)
    auto old_alarm = AlarmMessage::create_sample(1001, "OldSensor.Value", 30.0, true);
    old_alarm.des = "Old alarm - should be ignored";
    // 현재 시간에서 10분 전으로 설정
    auto old_time = std::chrono::system_clock::now() - std::chrono::minutes(10);
    old_alarm.tm = AlarmMessage::time_to_csharp_format(old_time, true);
    alarms.push_back(old_alarm);
    
    return alarms;
}

/**
 * @brief 단일 알람 테스트
 */
void testSingleAlarm(CSPGateway& gateway) {
    std::cout << "=== 단일 알람 테스트 ===\n";
    
    auto result = gateway.sendTestAlarm();
    
    std::cout << "테스트 알람 전송 결과:\n";
    std::cout << "  성공: " << (result.success ? "예" : "아니오") << "\n";
    std::cout << "  상태 코드: " << result.status_code << "\n";
    
    if (!result.success) {
        std::cout << "  오류 메시지: " << result.error_message << "\n";
    }
    
    if (result.s3_success) {
        std::cout << "  S3 업로드: 성공\n";
        std::cout << "  S3 파일: " << result.s3_file_path << "\n";
    }
    
    std::cout << "\n";
}

/**
 * @brief 멀티빌딩 알람 테스트
 */
void testMultiBuildingAlarms(CSPGateway& gateway) {
    std::cout << "=== 멀티빌딩 알람 테스트 ===\n";
    
    // 테스트 알람들 생성
    auto test_alarms = createTestAlarms();
    std::cout << "생성된 테스트 알람 수: " << test_alarms.size() << "\n";
    
    // 빌딩별로 그룹화
    auto grouped_alarms = gateway.groupAlarmsByBuilding(test_alarms);
    std::cout << "그룹화된 빌딩 수: " << grouped_alarms.size() << "\n";
    
    for (const auto& [building_id, alarms] : grouped_alarms) {
        std::cout << "  빌딩 " << building_id << ": " << alarms.size() << "개 알람\n";
    }
    
    // 멀티빌딩 처리 실행
    auto results = gateway.processMultiBuildingAlarms(grouped_alarms);
    
    std::cout << "\n처리 결과:\n";
    for (const auto& [building_id, result] : results) {
        std::cout << "빌딩 " << building_id << ":\n";
        std::cout << "  전체 알람: " << result.total_alarms << "\n";
        std::cout << "  API 성공: " << result.successful_api_calls << "\n";
        std::cout << "  API 실패: " << result.failed_api_calls << "\n";
        std::cout << "  S3 업로드: " << (result.s3_success ? "성공" : "실패") << "\n";
        std::cout << "  배치 파일: " << result.batch_file_path << "\n";
        std::cout << "  완전 성공: " << (result.isCompleteSuccess() ? "예" : "아니오") << "\n";
        std::cout << "\n";
    }
}

/**
 * @brief 배치 처리 및 시간 필터링 테스트
 */
void testBatchAndTimeFiltering(CSPGateway& gateway) {
    std::cout << "=== 배치 처리 및 시간 필터링 테스트 ===\n";
    
    // 알람 무시 시간을 3분으로 설정
    gateway.setAlarmIgnoreMinutes(3);
    std::cout << "알람 무시 시간: 3분으로 설정\n";
    
    // 테스트 알람들 생성 (다양한 시간)
    auto test_alarms = createTestAlarms();
    
    // 시간 필터링 테스트
    auto filtered_alarms = gateway.filterIgnoredAlarms(test_alarms);
    
    std::cout << "원본 알람 수: " << test_alarms.size() << "\n";
    std::cout << "필터링 후 알람 수: " << filtered_alarms.size() << "\n";
    std::cout << "무시된 알람 수: " << (test_alarms.size() - filtered_alarms.size()) << "\n\n";
    
    // 필터링된 알람들로 멀티빌딩 처리
    if (!filtered_alarms.empty()) {
        auto grouped = gateway.groupAlarmsByBuilding(filtered_alarms);
        auto results = gateway.processMultiBuildingAlarms(grouped);
        
        std::cout << "필터링된 알람 처리 완료\n";
    }
}

/**
 * @brief 연결 테스트
 */
void testConnection(CSPGateway& gateway) {
    std::cout << "=== 연결 테스트 ===\n";
    
    bool connection_ok = gateway.testConnection();
    std::cout << "전체 연결 상태: " << (connection_ok ? "OK" : "FAILED") << "\n";
    
    bool s3_ok = gateway.testS3Connection();
    std::cout << "S3 연결 상태: " << (s3_ok ? "OK" : "FAILED") << "\n";
    
    std::cout << "\n";
}

/**
 * @brief 통계 출력
 */
void printStatistics(CSPGateway& gateway) {
    std::cout << "=== 통계 정보 ===\n";
    
    auto stats = gateway.getStats();
    
    std::cout << "전체 알람 수: " << stats.total_alarms.load() << "\n";
    std::cout << "API 호출 성공: " << stats.successful_api_calls.load() << "\n";
    std::cout << "API 호출 실패: " << stats.failed_api_calls.load() << "\n";
    std::cout << "API 성공률: " << std::fixed << std::setprecision(1) << stats.getAPISuccessRate() << "%\n";
    std::cout << "S3 업로드 성공: " << stats.successful_s3_uploads.load() << "\n";
    std::cout << "S3 업로드 실패: " << stats.failed_s3_uploads.load() << "\n";
    std::cout << "S3 성공률: " << std::fixed << std::setprecision(1) << stats.getS3SuccessRate() << "%\n";
    std::cout << "배치 처리 수: " << stats.total_batches.load() << "\n";
    std::cout << "배치 성공 수: " << stats.successful_batches.load() << "\n";
    std::cout << "무시된 알람 수: " << stats.ignored_alarms.load() << "\n";
    std::cout << "재시도 횟수: " << stats.retry_attempts.load() << "\n";
    std::cout << "평균 응답시간: " << std::fixed << std::setprecision(2) << stats.avg_response_time_ms << "ms\n";
    
    std::cout << "\n";
}

/**
 * @brief 파일 정리 테스트
 */
void testCleanup(CSPGateway& gateway) {
    std::cout << "=== 파일 정리 테스트 ===\n";
    
    size_t deleted_count = gateway.cleanupOldFailedFiles(1); // 1일 이상 된 파일 삭제
    std::cout << "정리된 파일 수: " << deleted_count << "\n\n";
}

/**
 * @brief 데몬 모드 실행
 */
void runDaemonMode(CSPGateway& gateway) {
    std::cout << "=== 데몬 모드 시작 ===\n";
    std::cout << "Ctrl+C로 종료하세요.\n\n";
    
    if (!gateway.start()) {
        std::cerr << "CSPGateway 시작 실패\n";
        return;
    }
    
    std::cout << "CSP Gateway 서비스가 성공적으로 시작되었습니다!\n";
    std::cout << "서비스 상태를 30초마다 출력합니다...\n\n";
    
    // 주기적으로 통계 출력
    int cycle = 0;
    while (!g_shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        if (g_shutdown_requested.load()) break;
        
        cycle++;
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::cout << "\n--- 사이클 " << cycle << " (" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << ") ---\n";
        printStatistics(gateway);
        
        // 5분마다 정리 작업 (10 사이클)
        if (cycle % 10 == 0) {
            std::cout << "정기 정리 작업 수행...\n";
            size_t cleaned = gateway.cleanupOldFailedFiles();
            if (cleaned > 0) {
                std::cout << "정리된 파일 수: " << cleaned << "\n";
            }
        }
    }
    
    std::cout << "\n데몬 모드 종료 중...\n";
    gateway.stop();
}

/**
 * @brief 대화형 모드 실행
 */
void runInteractiveMode(CSPGateway& gateway) {
    std::cout << "=== 대화형 모드 ===\n";
    std::cout << "명령어를 입력하세요 (help 입력 시 도움말)\n\n";
    
    std::string command;
    while (true) {
        std::cout << "CSP> ";
        std::getline(std::cin, command);
        
        if (command == "quit" || command == "exit") {
            break;
        }
        else if (command == "help") {
            std::cout << "사용 가능한 명령어:\n";
            std::cout << "  test-alarm    - 단일 알람 테스트\n";
            std::cout << "  test-multi    - 멀티빌딩 테스트\n";
            std::cout << "  test-batch    - 배치/시간필터 테스트\n";
            std::cout << "  connection    - 연결 테스트\n";
            std::cout << "  stats         - 통계 출력\n";
            std::cout << "  cleanup       - 파일 정리\n";
            std::cout << "  start         - 서비스 시작\n";
            std::cout << "  stop          - 서비스 중지\n";
            std::cout << "  status        - 서비스 상태\n";
            std::cout << "  quit/exit     - 종료\n";
        }
        else if (command == "test-alarm") {
            testSingleAlarm(gateway);
        }
        else if (command == "test-multi") {
            testMultiBuildingAlarms(gateway);
        }
        else if (command == "test-batch") {
            testBatchAndTimeFiltering(gateway);
        }
        else if (command == "connection") {
            testConnection(gateway);
        }
        else if (command == "stats") {
            printStatistics(gateway);
        }
        else if (command == "cleanup") {
            testCleanup(gateway);
        }
        else if (command == "start") {
            if (gateway.start()) {
                std::cout << "서비스 시작됨\n";
            } else {
                std::cout << "서비스 시작 실패\n";
            }
        }
        else if (command == "stop") {
            gateway.stop();
            std::cout << "서비스 중지됨\n";
        }
        else if (command == "status") {
            std::cout << "서비스 상태: " << (gateway.isRunning() ? "실행 중" : "중지됨") << "\n";
        }
        else if (!command.empty()) {
            std::cout << "알 수 없는 명령어: " << command << " (help 입력 시 도움말)\n";
        }
    }
}

/**
 * @brief 메인 함수
 */
int main(int argc, char* argv[]) {
    // 시그널 핸들러 등록
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    print_banner();
    
    try {
        // 명령행 인수 처리
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
                std::cout << "Features: Multi-Building + Time Filtering + Batch Management\n";
                return 0;
            }
            else if (arg.find("--config=") == 0) {
                config_file = arg.substr(9);
            }
            else if (arg == "--interactive") {
                interactive_mode = true;
            }
            else if (arg.find("--") == 0) {
                command = arg.substr(2); // --test-alarm → test-alarm
            }
        }
        
#ifdef HAS_SHARED_LIBS
        // LogManager 및 ConfigManager 초기화
        LogManager::getInstance().initialize();
        ConfigManager::getInstance().initialize();
        LogManager::getInstance().Info("Export Gateway starting...");
#endif
        
        // 외부 설정 파일에서 로드
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
            // 기본 모드: 데몬 모드
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