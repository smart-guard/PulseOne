/**
 * @file main.cpp
 * @brief PulseOne Export Gateway 메인 진입점
 * @author PulseOne Development Team
 * @date 2025-09-22
 */

#include <iostream>
#include <csignal>
#include <atomic>
#include <memory>
#include <thread>
#include <chrono>

#include "CSP/CSPGateway.h"
#include "Utils/LogManager.h"

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
╚══════════════════════════════════════════════════════════════╝
)" << std::endl;
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n"
              << "\nOptions:\n"
              << "  --config <file>     Configuration file path\n"
              << "  --test             Run test mode\n"
              << "  --version          Show version information\n"
              << "  --help             Show this help message\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    // 시그널 핸들러 등록
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    print_banner();
    
    // 명령줄 인자 처리
    std::string config_file = "config/export-gateway.json";
    bool test_mode = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        }
        else if (arg == "--version" || arg == "-v") {
            std::cout << "PulseOne Export Gateway v1.0.0" << std::endl;
            return 0;
        }
        else if (arg == "--test") {
            test_mode = true;
        }
        else if (arg == "--config" && i + 1 < argc) {
            config_file = argv[++i];
        }
        else {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    
    LogManager::getInstance().Info("Starting PulseOne Export Gateway...");
    
    try {
        // CSP Gateway 설정
        PulseOne::CSP::CSPGatewayConfig config;
        config.building_id = "1001";
        config.api_endpoint = "https://api.example.com/alarm";
        config.api_key = "your-api-key-here";
        config.s3_bucket_name = "pulseone-alarms";
        config.s3_region = "us-east-1";
        config.debug_mode = test_mode;
        
        // CSP Gateway 생성
        auto gateway = std::make_unique<PulseOne::CSP::CSPGateway>(config);
        
        if (test_mode) {
            std::cout << "Running in test mode..." << std::endl;
            
            // 연결 테스트
            std::cout << "Testing connections..." << std::endl;
            bool connection_ok = gateway->testConnection();
            std::cout << "Connection test: " << (connection_ok ? "PASSED" : "FAILED") << std::endl;
            
            // 테스트 알람 전송
            std::cout << "Sending test alarm..." << std::endl;
            auto test_result = gateway->sendTestAlarm();
            std::cout << "Test alarm result: " << (test_result.success ? "SUCCESS" : "FAILED") << std::endl;
            if (!test_result.success) {
                std::cout << "Error: " << test_result.error_message << std::endl;
            }
            
            // 통계 출력
            auto stats = gateway->getStats();
            std::cout << "\nStatistics:" << std::endl;
            std::cout << "  Total alarms: " << stats.total_alarms.load() << std::endl;
            std::cout << "  API calls: " << stats.successful_api_calls.load() 
                      << " success, " << stats.failed_api_calls.load() << " failed" << std::endl;
            std::cout << "  S3 uploads: " << stats.successful_s3_uploads.load() 
                      << " success, " << stats.failed_s3_uploads.load() << " failed" << std::endl;
            
            return test_result.success ? 0 : 1;
        }
        
        // 서비스 시작
        std::cout << "Starting CSP Gateway service..." << std::endl;
        if (!gateway->start()) {
            std::cerr << "Failed to start CSP Gateway service" << std::endl;
            return 1;
        }
        
        std::cout << "CSP Gateway service started successfully!" << std::endl;
        std::cout << "Press Ctrl+C to stop..." << std::endl;
        
        // 메인 루프
        while (!g_shutdown_requested.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // 주기적 상태 출력 (매 10초)
            static auto last_status_time = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_status_time).count() >= 10) {
                auto stats = gateway->getStats();
                std::cout << "[STATUS] Alarms: " << stats.total_alarms.load() 
                          << ", API: " << stats.successful_api_calls.load() << "/" 
                          << stats.failed_api_calls.load() << std::endl;
                last_status_time = now;
            }
        }
        
        // 서비스 정지
        std::cout << "Stopping CSP Gateway service..." << std::endl;
        gateway->stop();
        
        LogManager::getInstance().Info("PulseOne Export Gateway stopped");
        
        std::cout << "Export Gateway stopped successfully." << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        LogManager::getInstance().Error("Fatal error: " + std::string(e.what()));
        return 1;
    }
}
