#include "../include/LoggerEngine.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>

int main() {
    std::cout << "Starting LogLib Standalone Test..." << std::endl;
    
    auto& logger = LogLib::LoggerEngine::getInstance();
    
    // Test 1: Configuration via File
    if (logger.loadFromConfigFile("tests/test.env")) {
        std::cout << "Config file loaded successfully." << std::endl;
        assert(logger.getLogLevel() == LogLib::LogLevel::TRACE); // Last one wins
    } else {
        std::cerr << "FAILED to load config file!" << std::endl;
        return 1;
    }
    
    // Test 2: Logging
    logger.log("test", LogLib::LogLevel::INFO, "This is a standalone test message");
    logger.log("test", LogLib::LogLevel::DEBUG, "Debug message should be visible");
    logger.log("test", LogLib::LogLevel::TRACE, "Trace message should NOT be visible");
    
    // Test 3: Packet Logging
    logger.logPacket("Modbus", "Device01", "01 03 00 00 00 01", "Read Request");
    
    // Test 4: Statistics
    auto stats = logger.getStatistics();
    std::cout << "Total logs: " << stats.total_logs << std::endl;
    assert(stats.total_logs >= 2);
    
    // Test 5: Verify Files
    if (std::filesystem::exists("./standalone_logs/")) {
        std::cout << "Log directory created successfully." << std::endl;
    } else {
        std::cerr << "FAILED: Log directory not found!" << std::endl;
        return 1;
    }

    // Test 6: Dynamic Callback (Mocking a DB/External change)
    std::cout << "Testing Dynamic Callback..." << std::endl;
    logger.setLogLevelProvider([]() {
        // In a real app, this might query a DB or a global atomic
        static int calls = 0;
        return (calls++ % 2 == 0) ? LogLib::LogLevel::ERROR : LogLib::LogLevel::TRACE;
    });

    logger.log("dynamic", LogLib::LogLevel::INFO, "This should be HIDDEN (current=ERROR)");
    assert(logger.getLogLevel() == LogLib::LogLevel::TRACE); // Next call returns TRACE
    logger.log("dynamic", LogLib::LogLevel::INFO, "This should be VISIBLE (current=TRACE)");
    
    std::cout << "LogLib Standalone Test COMPLETED SUCCESSFULLY." << std::endl;
    return 0;
}
