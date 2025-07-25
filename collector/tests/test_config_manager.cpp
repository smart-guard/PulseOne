// =============================================================================
// collector/tests/test_config_manager.cpp - 포괄적 테스트
// ConfigManager 모든 기능 검증
// =============================================================================

#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <functional>

// 색상 출력 매크로
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define PURPLE  "\033[35m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

// 테스트 결과 추적
struct TestResult {
    std::string name;
    bool passed;
    std::string message;
    double duration_ms;
};

class ConfigManagerTester {
private:
    std::vector<TestResult> results_;
    std::string test_config_dir_;
    
public:
    ConfigManagerTester() : test_config_dir_("./test_config") {}
    
    // ==========================================================================
    // 테스트 실행 및 결과 출력
    // ==========================================================================
    
    void runAllTests() {
        printHeader();
        
        // 환경 준비
        setupTestEnvironment();
        
        // 기본 기능 테스트
        testBasicFunctionality();
        testConfigFileLoading();
        testVariableExpansion();
        testTypeConversions();
        
        // 확장 기능 테스트
        testModuleManagement();
        testPasswordFileLoading();
        testTemplateGeneration();
        testDirectoryManagement();
        
        // 멀티스레드 테스트
        testThreadSafety();
        
        // 에러 처리 테스트
        testErrorHandling();
        
        // 결과 출력
        printResults();
        
        // 정리
        cleanupTestEnvironment();
    }

private:
    void printHeader() {
        std::cout << CYAN << BOLD << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                    ConfigManager 테스트                     ║\n";
        std::cout << "║                     통합 검증 시스템                        ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
        std::cout << RESET << "\n";
    }
    
    void runTest(const std::string& test_name, std::function<void()> test_func) {
        std::cout << YELLOW << "🧪 " << test_name << "..." << RESET;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            test_func();
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(end - start);
            
            results_.push_back({test_name, true, "성공", duration.count()});
            std::cout << GREEN << " ✅ 통과" << RESET << std::endl;
            
        } catch (const std::exception& e) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(end - start);
            
            results_.push_back({test_name, false, e.what(), duration.count()});
            std::cout << RED << " ❌ 실패: " << e.what() << RESET << std::endl;
        }
    }
    
    // ==========================================================================
    // 환경 설정
    // ==========================================================================
    
    void setupTestEnvironment() {
        std::cout << BLUE << "\n📁 테스트 환경 준비 중...\n" << RESET;
        
        // 테스트 디렉토리 생성
        if (std::filesystem::exists(test_config_dir_)) {
            std::filesystem::remove_all(test_config_dir_);
        }
        std::filesystem::create_directories(test_config_dir_);
        
        // 환경변수 설정
        #ifdef _WIN32
        _putenv_s("PULSEONE_CONFIG_DIR", test_config_dir_.c_str());
        #else
        setenv("PULSEONE_CONFIG_DIR", test_config_dir_.c_str(), 1);
        #endif
        
        std::cout << GREEN << "✅ 테스트 환경 준비 완료: " << test_config_dir_ << "\n" << RESET;
    }
    
    void cleanupTestEnvironment() {
        std::cout << BLUE << "\n🧹 테스트 환경 정리 중...\n" << RESET;
        
        try {
            if (std::filesystem::exists(test_config_dir_)) {
                std::filesystem::remove_all(test_config_dir_);
                std::cout << GREEN << "✅ 테스트 디렉토리 정리 완료\n" << RESET;
            }
        } catch (const std::exception& e) {
            std::cout << YELLOW << "⚠️ 정리 중 오류: " << e.what() << "\n" << RESET;
        }
    }
    
    // ==========================================================================
    // 기본 기능 테스트
    // ==========================================================================
    
    void testBasicFunctionality() {
        std::cout << PURPLE << "\n🔧 기본 기능 테스트\n" << RESET;
        
        runTest("싱글톤 패턴", [&]() {
            auto& config1 = ConfigManager::getInstance();
            auto& config2 = ConfigManager::getInstance();
            
            if (&config1 != &config2) {
                throw std::runtime_error("싱글톤 패턴 위반");
            }
        });
        
        runTest("초기화", [&]() {
            auto& config = ConfigManager::getInstance();
            config.initialize();
            
            // 설정 디렉토리가 올바르게 설정되었는지 확인
            std::string config_dir = config.getConfigDirectory();
            if (config_dir.empty()) {
                throw std::runtime_error("설정 디렉토리가 설정되지 않음");
            }
        });
        
        runTest("기본 설정 get/set", [&]() {
            auto& config = ConfigManager::getInstance();
            
            // 설정값 저장
            config.set("TEST_KEY", "test_value");
            
            // 설정값 조회
            std::string value = config.get("TEST_KEY");
            if (value != "test_value") {
                throw std::runtime_error("설정값 저장/조회 실패");
            }
            
            // 키 존재 확인
            if (!config.hasKey("TEST_KEY")) {
                throw std::runtime_error("키 존재 확인 실패");
            }
        });
        
        runTest("기본값 처리", [&]() {
            auto& config = ConfigManager::getInstance();
            
            // 존재하지 않는 키
            std::string value = config.getOrDefault("NON_EXISTENT_KEY", "default_value");
            if (value != "default_value") {
                throw std::runtime_error("기본값 처리 실패");
            }
        });
    }
    
    void testConfigFileLoading() {
        std::cout << PURPLE << "\n📄 설정 파일 로딩 테스트\n" << RESET;
        
        runTest("메인 .env 파일 생성 및 로드", [&]() {
            // 테스트용 .env 파일 생성
            std::string env_content = R"(# 테스트 설정 파일
TEST_STRING=hello_world
TEST_NUMBER=12345
TEST_BOOL=true
TEST_WITH_SPACES= spaced value 
TEST_QUOTED="quoted value"
)";
            
            std::ofstream env_file(test_config_dir_ + "/.env");
            env_file << env_content;
            env_file.close();
            
            // ConfigManager 재초기화
            auto& config = ConfigManager::getInstance();
            config.reload();
            
            // 값 확인
            if (config.get("TEST_STRING") != "hello_world") {
                throw std::runtime_error("문자열 값 로드 실패");
            }
            
            if (config.get("TEST_NUMBER") != "12345") {
                throw std::runtime_error("숫자 값 로드 실패");
            }
            
            if (config.get("TEST_QUOTED") != "quoted value") {
                throw std::runtime_error("따옴표 제거 실패");
            }
        });
        
        runTest("추가 설정 파일 로드", [&]() {
            // 메인 .env에 CONFIG_FILES 추가
            std::string env_content = R"(CONFIG_FILES=test1.env,test2.env
MAIN_VALUE=from_main
)";
            std::ofstream env_file(test_config_dir_ + "/.env");
            env_file << env_content;
            env_file.close();
            
            // 추가 설정 파일들 생성
            std::string test1_content = "TEST1_VALUE=from_test1\n";
            std::ofstream test1_file(test_config_dir_ + "/test1.env");
            test1_file << test1_content;
            test1_file.close();
            
            std::string test2_content = "TEST2_VALUE=from_test2\n";
            std::ofstream test2_file(test_config_dir_ + "/test2.env");
            test2_file << test2_content;
            test2_file.close();
            
            // 재로드
            auto& config = ConfigManager::getInstance();
            config.reload();
            
            // 모든 파일의 값이 로드되었는지 확인
            if (config.get("MAIN_VALUE") != "from_main") {
                throw std::runtime_error("메인 파일 값 로드 실패");
            }
            
            if (config.get("TEST1_VALUE") != "from_test1") {
                throw std::runtime_error("test1.env 값 로드 실패");
            }
            
            if (config.get("TEST2_VALUE") != "from_test2") {
                throw std::runtime_error("test2.env 값 로드 실패");
            }
        });
    }
    
    void testVariableExpansion() {
        std::cout << PURPLE << "\n🔄 변수 확장 테스트\n" << RESET;
        
        runTest("기본 변수 확장", [&]() {
            auto& config = ConfigManager::getInstance();
            
            // 기본 변수들 설정
            config.set("BASE_DIR", "/opt/pulseone");
            config.set("APP_NAME", "collector");
            config.set("FULL_PATH", "${BASE_DIR}/${APP_NAME}");
            
            // 수동으로 변수 확장 트리거
            config.triggerVariableExpansion();
            
            // 변수 확장 실행
            std::string expanded = config.get("FULL_PATH");
            
            if (expanded != "/opt/pulseone/collector") {
                throw std::runtime_error("변수 확장 실패: " + expanded);
            }
        });
        
        runTest("중첩 변수 확장", [&]() {
            auto& config = ConfigManager::getInstance();
            
            config.set("ENV", "dev");
            config.set("ENV_DIR", "/config/${ENV}");
            config.set("CONFIG_FILE", "${ENV_DIR}/app.conf");
            
            // 수동으로 변수 확장 트리거
            config.triggerVariableExpansion();
            
            std::string result = config.get("CONFIG_FILE");
            
            if (result != "/config/dev/app.conf") {
                throw std::runtime_error("중첩 변수 확장 실패: " + result);
            }
        });
    }
    
    void testTypeConversions() {
        std::cout << PURPLE << "\n🔢 타입 변환 테스트\n" << RESET;
        
        runTest("정수형 변환", [&]() {
            auto& config = ConfigManager::getInstance();
            
            config.set("INT_VALUE", "42");
            config.set("INVALID_INT", "not_a_number");
            
            if (config.getInt("INT_VALUE") != 42) {
                throw std::runtime_error("정수 변환 실패");
            }
            
            if (config.getInt("INVALID_INT", 100) != 100) {
                throw std::runtime_error("잘못된 정수에 대한 기본값 처리 실패");
            }
            
            if (config.getInt("NON_EXISTENT", 200) != 200) {
                throw std::runtime_error("존재하지 않는 키에 대한 기본값 처리 실패");
            }
        });
        
        runTest("불린형 변환", [&]() {
            auto& config = ConfigManager::getInstance();
            
            config.set("BOOL_TRUE1", "true");
            config.set("BOOL_TRUE2", "yes");
            config.set("BOOL_TRUE3", "1");
            config.set("BOOL_TRUE4", "on");
            config.set("BOOL_FALSE", "false");
            config.set("BOOL_INVALID", "maybe");
            
            if (!config.getBool("BOOL_TRUE1")) throw std::runtime_error("true 변환 실패");
            if (!config.getBool("BOOL_TRUE2")) throw std::runtime_error("yes 변환 실패");
            if (!config.getBool("BOOL_TRUE3")) throw std::runtime_error("1 변환 실패");
            if (!config.getBool("BOOL_TRUE4")) throw std::runtime_error("on 변환 실패");
            if (config.getBool("BOOL_FALSE")) throw std::runtime_error("false 변환 실패");
            if (config.getBool("BOOL_INVALID", true) != true) throw std::runtime_error("잘못된 불린에 대한 기본값 처리 실패");
        });
        
        runTest("실수형 변환", [&]() {
            auto& config = ConfigManager::getInstance();
            
            config.set("DOUBLE_VALUE", "3.14159");
            config.set("INVALID_DOUBLE", "not_a_double");
            
            double value = config.getDouble("DOUBLE_VALUE");
            if (std::abs(value - 3.14159) > 0.0001) {
                throw std::runtime_error("실수 변환 실패");
            }
            
            if (config.getDouble("INVALID_DOUBLE", 2.71) != 2.71) {
                throw std::runtime_error("잘못된 실수에 대한 기본값 처리 실패");
            }
        });
    }
    
    // ==========================================================================
    // 확장 기능 테스트
    // ==========================================================================
    
    void testModuleManagement() {
        std::cout << PURPLE << "\n🧩 모듈 관리 테스트\n" << RESET;
        
        runTest("데이터베이스 타입 확인", [&]() {
            auto& config = ConfigManager::getInstance();
            
            // 기본값 테스트
            config.set("DATABASE_TYPE", "POSTGRESQL");
            std::string db_type = config.getActiveDatabaseType();
            
            if (db_type != "POSTGRESQL") {
                throw std::runtime_error("데이터베이스 타입 확인 실패: " + db_type);
            }
            
            // 기본값이 없을 때
            config.set("DATABASE_TYPE", "");
            db_type = config.getActiveDatabaseType();
            if (db_type != "SQLITE") {
                throw std::runtime_error("기본 데이터베이스 타입 실패: " + db_type);
            }
        });
        
        runTest("모듈 활성화 상태 확인", [&]() {
            auto& config = ConfigManager::getInstance();
            
            // database 모듈은 항상 활성화
            if (!config.isModuleEnabled("database")) {
                throw std::runtime_error("database 모듈이 비활성화됨");
            }
            
            // timeseries 모듈 테스트
            config.set("INFLUX_ENABLED", "true");
            if (!config.isModuleEnabled("timeseries")) {
                throw std::runtime_error("timeseries 모듈 활성화 실패");
            }
            
            config.set("INFLUX_ENABLED", "false");
            if (config.isModuleEnabled("timeseries")) {
                throw std::runtime_error("timeseries 모듈 비활성화 실패");
            }
            
            // redis 모듈 테스트
            config.set("REDIS_PRIMARY_ENABLED", "yes");
            if (!config.isModuleEnabled("redis")) {
                throw std::runtime_error("redis 모듈 활성화 실패");
            }
            
            // messaging 모듈 테스트
            config.set("RABBITMQ_ENABLED", "1");
            config.set("MQTT_ENABLED", "false");
            if (!config.isModuleEnabled("messaging")) {
                throw std::runtime_error("messaging 모듈 활성화 실패 (RABBITMQ)");
            }
            
            config.set("RABBITMQ_ENABLED", "false");
            config.set("MQTT_ENABLED", "true");
            if (!config.isModuleEnabled("messaging")) {
                throw std::runtime_error("messaging 모듈 활성화 실패 (MQTT)");
            }
        });
    }
    
    void testPasswordFileLoading() {
        std::cout << PURPLE << "\n🔐 비밀번호 파일 로딩 테스트\n" << RESET;
        
        runTest("비밀번호 파일 생성 및 로드", [&]() {
            auto& config = ConfigManager::getInstance();
            
            // secrets 디렉토리 생성
            std::string secrets_dir = test_config_dir_ + "/secrets";
            std::filesystem::create_directories(secrets_dir);
            
            // 테스트용 비밀번호 파일 생성
            std::string password_file = secrets_dir + "/test_password.key";
            std::ofstream pw_file(password_file);
            pw_file << "super_secret_password\n";
            pw_file.close();
            
            // 설정에 비밀번호 파일 경로 등록
            config.set("TEST_PASSWORD_FILE", password_file);
            
            // 비밀번호 로드
            std::string password = config.loadPasswordFromFile("TEST_PASSWORD_FILE");
            
            if (password != "super_secret_password") {
                throw std::runtime_error("비밀번호 로드 실패: " + password);
            }
        });
        
        runTest("존재하지 않는 비밀번호 파일 처리", [&]() {
            auto& config = ConfigManager::getInstance();
            
            config.set("MISSING_PASSWORD_FILE", "/non/existent/file.key");
            std::string password = config.loadPasswordFromFile("MISSING_PASSWORD_FILE");
            
            if (!password.empty()) {
                throw std::runtime_error("존재하지 않는 파일에서 비밀번호가 로드됨");
            }
        });
    }
    
    void testTemplateGeneration() {
        std::cout << PURPLE << "\n📝 템플릿 생성 테스트\n" << RESET;
        
        runTest("설정 파일 템플릿 자동 생성", [&]() {
            // 빈 디렉토리에서 시작
            if (std::filesystem::exists(test_config_dir_)) {
                std::filesystem::remove_all(test_config_dir_);
            }
            std::filesystem::create_directories(test_config_dir_);
            
            // 초기화 (템플릿 자동 생성)
            auto& config = ConfigManager::getInstance();
            config.reload();
            
            // 생성된 파일들 확인
            std::vector<std::string> expected_files = {
                ".env", "database.env", "timeseries.env", 
                "redis.env", "messaging.env", "security.env"
            };
            
            for (const auto& filename : expected_files) {
                std::string filepath = test_config_dir_ + "/" + filename;
                if (!std::filesystem::exists(filepath)) {
                    throw std::runtime_error("템플릿 파일 생성 실패: " + filename);
                }
            }
            
            // secrets 디렉토리 확인
            std::string secrets_dir = test_config_dir_ + "/secrets";
            if (!std::filesystem::exists(secrets_dir)) {
                throw std::runtime_error("secrets 디렉토리 생성 실패");
            }
        });
        
        runTest("설정 파일 상태 확인", [&]() {
            auto& config = ConfigManager::getInstance();
            
            auto status = config.checkAllConfigFiles();
            
            // 모든 설정 파일이 존재해야 함
            std::vector<std::string> required_files = {
                ".env", "database.env", "timeseries.env", 
                "redis.env", "messaging.env", "security.env"
            };
            
            for (const auto& filename : required_files) {
                if (status.find(filename) == status.end() || !status[filename]) {
                    throw std::runtime_error("설정 파일 상태 확인 실패: " + filename);
                }
            }
        });
    }
    
    void testDirectoryManagement() {
        std::cout << PURPLE << "\n📁 디렉토리 관리 테스트\n" << RESET;
        
        runTest("데이터 디렉토리 관리", [&]() {
            auto& config = ConfigManager::getInstance();
            
            std::string data_dir = config.getDataDirectory();
            if (data_dir.empty()) {
                throw std::runtime_error("데이터 디렉토리가 설정되지 않음");
            }
            
            std::string sqlite_path = config.getSQLiteDbPath();
            if (sqlite_path.find("pulseone.db") == std::string::npos) {
                throw std::runtime_error("SQLite 경로 설정 실패: " + sqlite_path);
            }
            
            std::string backup_dir = config.getBackupDirectory();
            if (backup_dir.find("backup") == std::string::npos) {
                throw std::runtime_error("백업 디렉토리 설정 실패: " + backup_dir);
            }
        });
        
        runTest("secrets 디렉토리 관리", [&]() {
            auto& config = ConfigManager::getInstance();
            
            std::string secrets_dir = config.getSecretsDirectory();
            if (secrets_dir.empty()) {
                throw std::runtime_error("secrets 디렉토리 경로가 설정되지 않음");
            }
            
            if (!std::filesystem::exists(secrets_dir)) {
                throw std::runtime_error("secrets 디렉토리가 존재하지 않음");
            }
        });
    }
    
    // ==========================================================================
    // 멀티스레드 테스트
    // ==========================================================================
    
    void testThreadSafety() {
        std::cout << PURPLE << "\n🧵 멀티스레드 안전성 테스트\n" << RESET;
        
        runTest("동시 읽기/쓰기", [&]() {
            auto& config = ConfigManager::getInstance();
            
            const int num_threads = 10;
            const int operations_per_thread = 100;
            std::vector<std::thread> threads;
            std::vector<bool> thread_results(num_threads, true);
            
            // 여러 스레드에서 동시에 읽기/쓰기
            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back([&, i]() {
                    try {
                        for (int j = 0; j < operations_per_thread; ++j) {
                            std::string key = "THREAD_" + std::to_string(i) + "_KEY_" + std::to_string(j);
                            std::string value = "value_" + std::to_string(i) + "_" + std::to_string(j);
                            
                            // 쓰기
                            config.set(key, value);
                            
                            // 읽기
                            std::string read_value = config.get(key);
                            if (read_value != value) {
                                thread_results[i] = false;
                                break;
                            }
                            
                            // 키 존재 확인
                            if (!config.hasKey(key)) {
                                thread_results[i] = false;
                                break;
                            }
                        }
                    } catch (...) {
                        thread_results[i] = false;
                    }
                });
            }
            
            // 모든 스레드 완료 대기
            for (auto& thread : threads) {
                thread.join();
            }
            
            // 결과 확인
            for (int i = 0; i < num_threads; ++i) {
                if (!thread_results[i]) {
                    throw std::runtime_error("스레드 " + std::to_string(i) + "에서 실패");
                }
            }
        });
    }
    
    // ==========================================================================
    // 에러 처리 테스트
    // ==========================================================================
    
    void testErrorHandling() {
        std::cout << PURPLE << "\n🚨 에러 처리 테스트\n" << RESET;
        
        runTest("잘못된 설정 디렉토리", [&]() {
            // 환경변수를 존재하지 않는 경로로 설정
            #ifdef _WIN32
            _putenv_s("PULSEONE_CONFIG_DIR", "/this/path/does/not/exist");
            #else
            setenv("PULSEONE_CONFIG_DIR", "/this/path/does/not/exist", 1);
            #endif
            
            // 새로운 인스턴스 생성을 시뮬레이션하기 위해 reload 사용
            auto& config = ConfigManager::getInstance();
            
            // 이 경우에도 크래시하지 않고 적절히 처리되어야 함
            // (실제 구현에서는 fallback 경로를 찾아야 함)
            try {
                config.reload();
                // 에러가 발생하지 않으면 성공
            } catch (const std::exception& e) {
                // 예외가 발생해도 적절한 에러 메시지와 함께 처리되어야 함
                std::string error_msg = e.what();
                if (error_msg.empty()) {
                    throw std::runtime_error("에러 메시지가 비어있음");
                }
            }
            
            // 환경변수 복원
            #ifdef _WIN32
            _putenv_s("PULSEONE_CONFIG_DIR", test_config_dir_.c_str());
            #else
            setenv("PULSEONE_CONFIG_DIR", test_config_dir_.c_str(), 1);
            #endif
        });
        
        runTest("잘못된 설정 파일 형식", [&]() {
            // 테스트 전용 깨끗한 환경으로 시작
            std::string clean_test_dir = "./clean_test_config";
            if (std::filesystem::exists(clean_test_dir)) {
                std::filesystem::remove_all(clean_test_dir);
            }
            std::filesystem::create_directories(clean_test_dir);
            
            // 환경변수 임시 변경
            #ifdef _WIN32
            _putenv_s("PULSEONE_CONFIG_DIR", clean_test_dir.c_str());
            #else
            setenv("PULSEONE_CONFIG_DIR", clean_test_dir.c_str(), 1);
            #endif
            
            // 간단한 메인 설정 파일 생성
            std::string main_content = "CONFIG_FILES=bad_format.env\n";
            std::ofstream main_file(clean_test_dir + "/.env");
            main_file << main_content;
            main_file.close();
            
            // 잘못된 형식의 설정 파일 생성 (한글 제거)
            std::string bad_content = R"(# 잘못된 설정 파일
KEY_WITHOUT_VALUE
=VALUE_WITHOUT_KEY
KEY=value
invalid_line_without_equals
# 정상적인 주석
NORMAL_KEY=normal_value
ANOTHER_NORMAL_KEY=another_value
)";
            
            std::ofstream bad_file(clean_test_dir + "/bad_format.env");
            bad_file << bad_content;
            bad_file.close();
            
            // 새로운 ConfigManager 인스턴스로 로드
            auto& config = ConfigManager::getInstance();
            config.reload();
            
            // 정상적인 키들이 로드되었는지 확인
            std::string normal_value = config.get("NORMAL_KEY");
            std::string another_value = config.get("ANOTHER_NORMAL_KEY");
            
            if (normal_value != "normal_value") {
                // 디버깅 정보 출력
                std::cout << "\n🔍 상세 디버깅 정보:\n";
                std::cout << "NORMAL_KEY 예상: 'normal_value', 실제: '" << normal_value << "'\n";
                std::cout << "ANOTHER_NORMAL_KEY: '" << another_value << "'\n";
                
                auto loaded_files = config.getLoadedFiles();
                std::cout << "로드된 파일들:\n";
                for (const auto& file : loaded_files) {
                    std::cout << "  " << file << "\n";
                }
                
                // 파일 내용 확인
                std::ifstream check_file(clean_test_dir + "/bad_format.env");
                std::cout << "bad_format.env 내용:\n";
                std::string line;
                while (std::getline(check_file, line)) {
                    std::cout << "  '" << line << "'\n";
                }
                check_file.close();
                
                throw std::runtime_error("정상적인 키 로드 실패: '" + normal_value + "'");
            }
            
            // 환경변수 복원
            #ifdef _WIN32
            _putenv_s("PULSEONE_CONFIG_DIR", test_config_dir_.c_str());
            #else
            setenv("PULSEONE_CONFIG_DIR", test_config_dir_.c_str(), 1);
            #endif
            
            // 정리
            std::filesystem::remove_all(clean_test_dir);
        });
    }
    
    // ==========================================================================
    // 결과 출력
    // ==========================================================================
    
    void printResults() {
        std::cout << CYAN << BOLD << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                        테스트 결과                          ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
        std::cout << RESET << "\n";
        
        int passed = 0;
        int failed = 0;
        double total_time = 0;
        
        for (const auto& result : results_) {
            std::string status_icon = result.passed ? GREEN "✅" : RED "❌";
            std::string duration = std::to_string((int)result.duration_ms) + "ms";
            
            std::cout << status_icon << " " << result.name 
                      << RESET << " (" << duration << ")";
            
            if (!result.passed) {
                std::cout << RED << " - " << result.message << RESET;
            }
            
            std::cout << std::endl;
            
            if (result.passed) passed++;
            else failed++;
            total_time += result.duration_ms;
        }
        
        std::cout << "\n" << BOLD;
        std::cout << "═══════════════════════════════════════════════════════════════\n";
        std::cout << "총 테스트: " << results_.size() << " | ";
        std::cout << GREEN << "통과: " << passed << RESET << BOLD << " | ";
        std::cout << RED << "실패: " << failed << RESET << BOLD << " | ";
        std::cout << "총 시간: " << (int)total_time << "ms\n";
        std::cout << RESET;
        
        if (failed == 0) {
            std::cout << GREEN << BOLD << "🎉 모든 테스트 통과!\n" << RESET;
        } else {
            std::cout << RED << BOLD << "❌ " << failed << "개 테스트 실패\n" << RESET;
        }
    }
};

// =============================================================================
// 메인 함수
// =============================================================================

int main() {
    try {
        // ConfigManager만 테스트 (LogManager initialize 제거)
        std::cout << "ConfigManager 통합 테스트를 시작합니다...\n";
        
        ConfigManagerTester tester;
        tester.runAllTests();
        
        std::cout << "\n테스트 완료!\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << RED << "❌ 테스트 실행 중 심각한 오류: " << e.what() << RESET << std::endl;
        return 1;
    } catch (...) {
        std::cout << RED << "❌ 알 수 없는 오류 발생" << RESET << std::endl;
        return 1;
    }
}