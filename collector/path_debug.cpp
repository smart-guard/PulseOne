// =============================================================================
// 경로 문제 확인 테스트 - 진짜 원인 찾기
// =============================================================================

#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdlib>

int main() {
    std::cout << "🔍 ConfigManager 경로 문제 분석" << std::endl;
    std::cout << "=============================" << std::endl;
    
    // 현재 상황 재현
    std::string test_dir = "./debug_config_test";
    
    std::cout << "📁 현재 작업 디렉토리: " << std::filesystem::current_path() << std::endl;
    
    // 1. 테스트 환경 생성
    std::cout << "\n🔧 1. 테스트 환경 생성" << std::endl;
    std::filesystem::create_directories(test_dir);
    std::cout << "✅ 테스트 디렉토리 생성: " << test_dir << std::endl;
    
    // 환경변수 설정
    setenv("PULSEONE_CONFIG_DIR", test_dir.c_str(), 1);
    std::cout << "✅ 환경변수 설정: PULSEONE_CONFIG_DIR=" << test_dir << std::endl;
    
    // .env 파일 생성
    std::ofstream env_file(test_dir + "/.env");
    env_file << "DEBUG_KEY=debug_value\n";
    env_file << "CONFIG_FILES=\n";
    env_file.close();
    std::cout << "✅ .env 파일 생성: " << test_dir << "/.env" << std::endl;
    
    // 2. 기존 ConfigManager 로직대로 경로 찾기
    std::cout << "\n🔧 2. 기존 ConfigManager 경로 탐색 재현" << std::endl;
    
    // 기존 ConfigManager의 possible_paths 재현
    std::vector<std::string> possible_paths = {
        "../config/.env",
        "./config/.env", 
        "../../config/.env",
        "/etc/pulseone/.env",
        ".env"
    };
    
    bool found = false;
    std::string found_path;
    
    for (const auto& path : possible_paths) {
        std::cout << "🔍 확인 중: " << path;
        if (std::filesystem::exists(path)) {
            std::cout << " → ✅ 발견!" << std::endl;
            found = true;
            found_path = path;
            break;
        } else {
            std::cout << " → ❌ 없음" << std::endl;
        }
    }
    
    if (found) {
        std::cout << "🎯 기존 로직으로 찾은 파일: " << found_path << std::endl;
    } else {
        std::cout << "❌ 기존 로직으로는 파일을 찾을 수 없음" << std::endl;
    }
    
    // 3. 환경변수 기반 경로 확인
    std::cout << "\n🔧 3. 환경변수 기반 경로 확인" << std::endl;
    const char* env_config = std::getenv("PULSEONE_CONFIG_DIR");
    if (env_config) {
        std::string env_path = std::string(env_config) + "/.env";
        std::cout << "🌍 환경변수 경로: " << env_path;
        if (std::filesystem::exists(env_path)) {
            std::cout << " → ✅ 발견!" << std::endl;
        } else {
            std::cout << " → ❌ 없음" << std::endl;
        }
    } else {
        std::cout << "❌ PULSEONE_CONFIG_DIR 환경변수 없음" << std::endl;
    }
    
    // 4. 새 버전 ConfigManager의 findConfigDirectory() 로직 시뮬레이션
    std::cout << "\n🔧 4. 새 버전 findConfigDirectory() 로직 시뮬레이션" << std::endl;
    
    // 환경변수 최우선 확인
    if (env_config && std::filesystem::exists(env_config)) {
        std::cout << "✅ 환경변수 디렉토리 존재: " << env_config << std::endl;
        
        std::string main_env_path = std::string(env_config) + "/.env";
        if (std::filesystem::exists(main_env_path)) {
            std::cout << "✅ 메인 .env 파일 존재: " << main_env_path << std::endl;
        } else {
            std::cout << "❌ 메인 .env 파일 없음: " << main_env_path << std::endl;
        }
    } else {
        std::cout << "❌ 환경변수 디렉토리 문제" << std::endl;
    }
    
    // 5. 실제 파일들 확인
    std::cout << "\n🔧 5. 실제 생성된 파일들 확인" << std::endl;
    
    std::vector<std::string> check_files = {
        test_dir + "/.env",
        "./config/.env",
        "../config/.env",
        ".env"
    };
    
    for (const auto& file : check_files) {
        std::cout << "📄 " << file;
        if (std::filesystem::exists(file)) {
            std::cout << " → ✅ 존재";
            
            // 파일 내용도 확인
            std::ifstream f(file);
            if (f.is_open()) {
                std::string first_line;
                std::getline(f, first_line);
                f.close();
                std::cout << " (첫 줄: " << first_line << ")";
            }
        } else {
            std::cout << " → ❌ 없음";
        }
        std::cout << std::endl;
    }
    
    // 6. 문제 진단
    std::cout << "\n🎯 문제 진단" << std::endl;
    std::cout << "============" << std::endl;
    
    if (found && env_config) {
        if (found_path != std::string(env_config) + "/.env") {
            std::cout << "🚨 경로 불일치 발견!" << std::endl;
            std::cout << "   환경변수 경로: " << env_config << "/.env" << std::endl;
            std::cout << "   실제 찾은 경로: " << found_path << std::endl;
            std::cout << "   → 다른 .env 파일을 먼저 찾고 있음!" << std::endl;
        } else {
            std::cout << "✅ 경로 일치" << std::endl;
        }
    }
    
    // 7. 해결책 제시
    std::cout << "\n💡 해결책" << std::endl;
    std::cout << "=======" << std::endl;
    std::cout << "1. 환경변수를 설정했으면 반드시 최우선으로 확인해야 함" << std::endl;
    std::cout << "2. 기존 possible_paths 로직을 우회하고 환경변수 경로를 먼저 확인" << std::endl;
    std::cout << "3. 또는 다른 .env 파일들을 제거하여 충돌 방지" << std::endl;
    
    // 8. 충돌 파일 확인
    std::cout << "\n🔧 8. 충돌 가능 파일 확인" << std::endl;
    if (std::filesystem::exists("./config/.env")) {
        std::cout << "⚠️ ./config/.env 파일이 존재 - 이 파일이 먼저 발견될 수 있음" << std::endl;
        
        // 내용 확인
        std::ifstream conflict_file("./config/.env");
        if (conflict_file.is_open()) {
            std::cout << "📄 ./config/.env 내용:" << std::endl;
            std::string line;
            int line_num = 0;
            while (std::getline(conflict_file, line) && line_num < 5) {
                std::cout << "   " << (++line_num) << ": " << line << std::endl;
            }
            conflict_file.close();
        }
    }
    
    if (std::filesystem::exists("../config/.env")) {
        std::cout << "⚠️ ../config/.env 파일이 존재 - 이 파일이 먼저 발견될 수 있음" << std::endl;
    }
    
    // 정리
    std::cout << "\n🧹 정리" << std::endl;
    std::filesystem::remove_all(test_dir);
    std::cout << "✅ 테스트 디렉토리 삭제" << std::endl;
    
    return 0;
}