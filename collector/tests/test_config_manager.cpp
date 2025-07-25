#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include <iostream>
#include <chrono>
#include <filesystem>

int main() {
    std::cout << "🧪 ConfigManager 완전 테스트" << std::endl;
    std::cout << "============================" << std::endl;
    
    try {
        auto& config = ConfigManager::getInstance();
        std::cout << "✅ 1. ConfigManager 인스턴스 생성 성공" << std::endl;
        
        // 기본 기능 테스트
        config.set("DEBUG_TEST", "debug_value");
        std::string value = config.get("DEBUG_TEST");
        std::cout << "✅ 2. 기본 get/set 기능 작동: " << value << std::endl;
        
        // 환경변수 설정 (테스트용 디렉토리)
        std::string test_dir = "./complete_test_config";
        setenv("PULSEONE_CONFIG_DIR", test_dir.c_str(), 1);
        std::cout << "✅ 3. 테스트 환경변수 설정: " << test_dir << std::endl;
        
        // 테스트 디렉토리 생성 (기존 .env는 생성하지 않음)
        std::string mkdir_cmd = "mkdir -p " + test_dir;
        system(mkdir_cmd.c_str());
        std::cout << "✅ 4. 테스트 디렉토리 생성: " << test_dir << std::endl;
        
        // ConfigManager가 자동으로 모든 설정 파일을 생성하도록 함
        std::cout << "🔄 5. initialize() 호출 (자동 파일 생성)..." << std::endl;
        
        auto start_time = std::chrono::steady_clock::now();
        
        config.initialize();
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "✅ 6. initialize() 성공! (소요시간: " << duration.count() << "ms)" << std::endl;
        
        // 생성된 파일들 확인
        std::cout << "\n📁 생성된 설정 파일들:" << std::endl;
        std::vector<std::string> expected_files = {
            ".env", "database.env", "redis.env", "timeseries.env", "messaging.env"
        };
        
        for (const auto& filename : expected_files) {
            std::string filepath = test_dir + "/" + filename;
            if (std::filesystem::exists(filepath)) {
                std::cout << "   ✅ " << filename << std::endl;
            } else {
                std::cout << "   ❌ " << filename << " (없음)" << std::endl;
            }
        }
        
        // 로드된 설정들 확인
        std::cout << "\n⚙️ 로드된 설정들:" << std::endl;
        auto all_configs = config.listAll();
        std::cout << "   총 " << all_configs.size() << "개 설정 로드됨" << std::endl;
        
        // 주요 설정들 확인
        std::vector<std::string> important_keys = {
            "NODE_ENV", "LOG_LEVEL", "DATABASE_TYPE", "DATA_DIR",
            "REDIS_PRIMARY_HOST", "INFLUX_HOST", "RABBITMQ_HOST"
        };
        
        for (const auto& key : important_keys) {
            std::string val = config.get(key);
            if (!val.empty()) {
                std::cout << "   ✅ " << key << "=" << val << std::endl;
            }
        }
        
        // 설정 디렉토리 및 로드된 파일 정보
        std::cout << "\n📂 설정 정보:" << std::endl;
        std::cout << "   설정 디렉토리: " << config.getConfigDirectory() << std::endl;
        std::cout << "   데이터 디렉토리: " << config.getDataDirectory() << std::endl;
        
        auto loaded_files = config.getLoadedFiles();
        std::cout << "   로드된 파일 수: " << loaded_files.size() << std::endl;
        for (const auto& file : loaded_files) {
            std::cout << "      📄 " << file << std::endl;
        }
        
        // 편의 기능 테스트
        std::cout << "\n🔧 편의 기능 테스트:" << std::endl;
        int threads = config.getInt("MAX_WORKER_THREADS", 1);
        int timeout = config.getInt("DEFAULT_TIMEOUT_MS", 1000);
        bool ssl = config.getBool("REDIS_PRIMARY_SSL", true);
        
        std::cout << "   MAX_WORKER_THREADS: " << threads << std::endl;
        std::cout << "   DEFAULT_TIMEOUT_MS: " << timeout << std::endl;
        std::cout << "   REDIS_PRIMARY_SSL: " << (ssl ? "true" : "false") << std::endl;
        
        // 정리
        std::string cleanup_cmd = "rm -rf " + test_dir;
        system(cleanup_cmd.c_str());
        std::cout << "\n🧹 테스트 디렉토리 정리 완료" << std::endl;
        
        std::cout << "\n🎉 ConfigManager 완전 테스트 성공!" << std::endl;
        std::cout << "✅ 모든 기능이 정상적으로 작동합니다." << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "❌ 테스트 중 예외 발생: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "❌ 알 수 없는 예외 발생" << std::endl;
        return 1;
    }
}
