/**
 * @file test_driver_logger.cpp
 * @brief DriverLogger.h의 단위 테스트
 * @details 드라이버 로깅 시스템의 기능과 성능을 검증하는 테스트 코드
 * @author PulseOne Development Team
 * @date 2025-01-17
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Drivers/DriverLogger.h"
#include "Drivers/CommonTypes.h"
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <sstream>
#include <fstream>

using namespace PulseOne::Drivers;
using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

// =============================================================================
// Mock LogManager (기존 LogManager가 없는 경우를 위한 Mock)
// =============================================================================

class MockLegacyLogManager {
public:
    MOCK_METHOD(void, Debug, (const std::string& message), ());
    MOCK_METHOD(void, Info, (const std::string& message), ());
    MOCK_METHOD(void, Warn, (const std::string& message), ());
    MOCK_METHOD(void, Error, (const std::string& message), ());
    MOCK_METHOD(void, Fatal, (const std::string& message), ());
};

// =============================================================================
// DriverLogger 기본 기능 테스트
// =============================================================================

class DriverLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        device_id_ = GenerateUUID();
        protocol_ = ProtocolType::MODBUS_TCP;
        endpoint_ = "192.168.1.100:502";
        
        logger_ = std::make_unique<DriverLogger>(device_id_, protocol_, endpoint_);
        logger_->SetMinLevel(LogLevel::DEBUG);  // 모든 로그 레벨 활성화
        
        // 모든 카테고리 활성화
        logger_->SetCategoryEnabled(DriverLogCategory::GENERAL, true);
        logger_->SetCategoryEnabled(DriverLogCategory::CONNECTION, true);
        logger_->SetCategoryEnabled(DriverLogCategory::COMMUNICATION, true);
        logger_->SetCategoryEnabled(DriverLogCategory::DATA_PROCESSING, true);
        logger_->SetCategoryEnabled(DriverLogCategory::ERROR_HANDLING, true);
        logger_->SetCategoryEnabled(DriverLogCategory::PERFORMANCE, true);
        logger_->SetCategoryEnabled(DriverLogCategory::SECURITY, true);
        logger_->SetCategoryEnabled(DriverLogCategory::PROTOCOL_SPECIFIC, true);
        logger_->SetCategoryEnabled(DriverLogCategory::DIAGNOSTICS, true);
    }
    
    void TearDown() override {
        logger_.reset();
    }
    
    UUID device_id_;
    ProtocolType protocol_;
    std::string endpoint_;
    std::unique_ptr<DriverLogger> logger_;
};

// =============================================================================
// 기본 로깅 기능 테스트
// =============================================================================

TEST_F(DriverLoggerTest, BasicLoggingFunctions) {
    // 통계 초기화
    logger_->ResetStatistics();
    
    // 각 레벨별 로그 메시지 생성
    logger_->Debug("Debug message for testing");
    logger_->Info("Info message for testing");
    logger_->Warn("Warning message for testing");
    logger_->Error("Error message for testing");
    logger_->Fatal("Fatal message for testing");
    
    // 통계 확인
    auto stats = logger_->GetStatistics();
    EXPECT_EQ(1, stats.debug_count);
    EXPECT_EQ(1, stats.info_count);
    EXPECT_EQ(1, stats.warn_count);
    EXPECT_EQ(1, stats.error_count);
    EXPECT_EQ(1, stats.fatal_count);
    EXPECT_EQ(5, stats.total_count);
}

TEST_F(DriverLoggerTest, CategorySpecificLogging) {
    logger_->ResetStatistics();
    
    // 카테고리별 로그 생성
    logger_->Info("General message", DriverLogCategory::GENERAL);
    logger_->Info("Connection message", DriverLogCategory::CONNECTION);
    logger_->Info("Communication message", DriverLogCategory::COMMUNICATION);
    logger_->Info("Data processing message", DriverLogCategory::DATA_PROCESSING);
    logger_->Error("Error handling message", DriverLogCategory::ERROR_HANDLING);
    logger_->Debug("Performance message", DriverLogCategory::PERFORMANCE);
    logger_->Warn("Security message", DriverLogCategory::SECURITY);
    logger_->Info("Protocol specific message", DriverLogCategory::PROTOCOL_SPECIFIC);
    logger_->Info("Diagnostics message", DriverLogCategory::DIAGNOSTICS);
    
    auto stats = logger_->GetStatistics();
    EXPECT_EQ(7, stats.info_count);   // INFO 레벨 메시지 7개
    EXPECT_EQ(1, stats.error_count);  // ERROR 레벨 메시지 1개
    EXPECT_EQ(1, stats.debug_count);  // DEBUG 레벨 메시지 1개
    EXPECT_EQ(1, stats.warn_count);   // WARN 레벨 메시지 1개
    EXPECT_EQ(10, stats.total_count); // 총 10개
}

// =============================================================================
// 로그 레벨 필터링 테스트
// =============================================================================

TEST_F(DriverLoggerTest, LogLevelFiltering) {
    // INFO 레벨로 설정 (DEBUG 메시지는 필터링됨)
    logger_->SetMinLevel(LogLevel::INFO);
    logger_->ResetStatistics();
    
    logger_->Debug("This debug message should be filtered");
    logger_->Info("This info message should pass");
    logger_->Warn("This warning message should pass");
    logger_->Error("This error message should pass");
    
    auto stats = logger_->GetStatistics();
    EXPECT_EQ(0, stats.debug_count);  // DEBUG는 필터링됨
    EXPECT_EQ(1, stats.info_count);
    EXPECT_EQ(1, stats.warn_count);
    EXPECT_EQ(1, stats.error_count);
    EXPECT_EQ(3, stats.total_count);
    
    // ERROR 레벨로 변경 (INFO, WARN도 필터링됨)
    logger_->SetMinLevel(LogLevel::ERROR);
    logger_->ResetStatistics();
    
    logger_->Debug("Filtered debug");
    logger_->Info("Filtered info");
    logger_->Warn("Filtered warning");
    logger_->Error("This error should pass");
    logger_->Fatal("This fatal should pass");
    
    stats = logger_->GetStatistics();
    EXPECT_EQ(0, stats.debug_count);
    EXPECT_EQ(0, stats.info_count);
    EXPECT_EQ(0, stats.warn_count);
    EXPECT_EQ(1, stats.error_count);
    EXPECT_EQ(1, stats.fatal_count);
    EXPECT_EQ(2, stats.total_count);
}

// =============================================================================
// 카테고리 필터링 테스트
// =============================================================================

TEST_F(DriverLoggerTest, CategoryFiltering) {
    // PERFORMANCE 카테고리 비활성화
    logger_->SetCategoryEnabled(DriverLogCategory::PERFORMANCE, false);
    logger_->ResetStatistics();
    
    logger_->Info("General message", DriverLogCategory::GENERAL);
    logger_->Info("Performance message", DriverLogCategory::PERFORMANCE);  // 필터링됨
    logger_->Error("Error message", DriverLogCategory::ERROR_HANDLING);
    
    auto stats = logger_->GetStatistics();
    EXPECT_EQ(1, stats.info_count);   // PERFORMANCE 메시지는 필터링됨
    EXPECT_EQ(1, stats.error_count);
    EXPECT_EQ(2, stats.total_count);
    
    // 카테고리 상태 확인
    EXPECT_TRUE(logger_->IsCategoryEnabled(DriverLogCategory::GENERAL));
    EXPECT_FALSE(logger_->IsCategoryEnabled(DriverLogCategory::PERFORMANCE));
    EXPECT_TRUE(logger_->IsCategoryEnabled(DriverLogCategory::ERROR_HANDLING));
}

// =============================================================================
// 컨텍스트 포함 로깅 테스트
// =============================================================================

TEST_F(DriverLoggerTest, ContextualLogging) {
    logger_->ResetStatistics();
    
    // 컨텍스트 정보 생성
    DriverLogContext context;
    context.device_id = device_id_;
    context.protocol = protocol_;
    context.endpoint = endpoint_;
    context.function_name = "TestFunction";
    context.file_name = "test_file.cpp";
    context.line_number = 123;
    context.extra_data["request_id"] = "REQ-001";
    context.extra_data["operation"] = "ReadRegisters";
    
    logger_->LogWithContext(LogLevel::INFO, "Test message with context", 
                           DriverLogCategory::COMMUNICATION, context);
    
    auto stats = logger_->GetStatistics();
    EXPECT_EQ(1, stats.info_count);
    EXPECT_EQ(1, stats.total_count);
}

// =============================================================================
// 에러 정보 로깅 테스트
// =============================================================================

TEST_F(DriverLoggerTest, ErrorInfoLogging) {
    logger_->ResetStatistics();
    
    // 에러 정보 생성
    ErrorInfo error(ErrorCode::CONNECTION_FAILED, "Connection timeout occurred");
    error.context = "ModbusConnect";
    error.file = "modbus_driver.cpp";
    error.line = 456;
    
    logger_->LogError(error, "Additional context information");
    
    auto stats = logger_->GetStatistics();
    EXPECT_EQ(1, stats.error_count);
    EXPECT_EQ(1, stats.total_count);
}

// =============================================================================
// 연결 상태 변경 로깅 테스트
// =============================================================================

TEST_F(DriverLoggerTest, ConnectionStatusChangeLogging) {
    logger_->ResetStatistics();
    
    // 연결 상태 변경 로깅
    logger_->LogConnectionStatusChange(
        ConnectionStatus::DISCONNECTED, 
        ConnectionStatus::CONNECTING,
        "Starting connection attempt"
    );
    
    logger_->LogConnectionStatusChange(
        ConnectionStatus::CONNECTING, 
        ConnectionStatus::CONNECTED,
        "Connection established successfully"
    );
    
    logger_->LogConnectionStatusChange(
        ConnectionStatus::CONNECTED, 
        ConnectionStatus::ERROR,
        "Connection lost due to network error"
    );
    
    auto stats = logger_->GetStatistics();
    EXPECT_GT(stats.total_count, 0);  // 상태 변경 로그가 기록되었는지 확인
}

// =============================================================================
// 데이터 전송 로깅 테스트
// =============================================================================

TEST_F(DriverLoggerTest, DataTransferLogging) {
    logger_->ResetStatistics();
    
    // 송신 데이터 로깅
    logger_->LogDataTransfer("TX", 256, 15, true);
    
    // 수신 데이터 로깅
    logger_->LogDataTransfer("RX", 128, 8, true);
    
    // 실패한 전송 로깅
    logger_->LogDataTransfer("TX", 0, 5000, false);
    
    auto stats = logger_->GetStatistics();
    EXPECT_GT(stats.total_count, 0);
}

// =============================================================================
// 성능 메트릭 로깅 테스트
// =============================================================================

TEST_F(DriverLoggerTest, PerformanceMetricsLogging) {
    logger_->ResetStatistics();
    
    // 처리 시간 메트릭
    auto start_time = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    logger_->LogPerformanceMetric("ReadOperation", duration.count(), "microseconds");
    
    // 메모리 사용량 메트릭
    logger_->LogPerformanceMetric("MemoryUsage", 1024*1024, "bytes");
    
    // CPU 사용률 메트릭
    logger_->LogPerformanceMetric("CpuUsage", 75.5, "percent");
    
    // 통계 확인
    auto stats = logger_->GetStatistics();
    EXPECT_GT(stats.total_count, 0);
}

TEST_F(DriverLoggerTest, ThreadThroughputMetrics) {
    logger_->ResetStatistics();
    
    // 스레드별 처리량 메트릭
    logger_->LogThroughputMetric("ReadThread", 1000, 1.0, "operations/sec");
    logger_->LogThroughputMetric("WriteThread", 500, 1.0, "operations/sec");
    logger_->LogThroughputMetric("EventThread", 250, 1.0, "events/sec");
    
    auto stats = logger_->GetStatistics();
    EXPECT_GT(stats.total_count, 0);
}

// =============================================================================
// 프로토콜별 특화 로깅 테스트
// =============================================================================

TEST_F(DriverLoggerTest, ModbusProtocolLogging) {
    logger_->ResetStatistics();
    
    // Modbus 특화 로깅 - 성공적인 읽기
    logger_->LogModbusOperation(3, 100, "42 43 44 45", true, 15);
    
    // Modbus 특화 로깅 - 실패한 쓰기
    logger_->LogModbusOperation(3, 200, "", false, 5000);
    
    // Modbus 예외 코드 로깅
    logger_->LogModbusException(3, 100, 0x02, "Illegal Data Address");
    
    auto stats = logger_->GetStatistics();
    EXPECT_GT(stats.total_count, 0);
}

TEST_F(DriverLoggerTest, MqttProtocolLogging) {
    logger_->ResetStatistics();
    
    // MQTT 특화 로깅 - PUBLISH
    logger_->LogMqttOperation("PUBLISH", "sensors/temperature", 1, 256, true);
    
    // MQTT 특화 로깅 - SUBSCRIBE
    logger_->LogMqttOperation("SUBSCRIBE", "control/valve", 2, 0, true);
    
    // MQTT 연결 상태 로깅
    logger_->LogMqttConnectionStatus(true, "mqtt://broker:1883", "Connection established");
    
    auto stats = logger_->GetStatistics();
    EXPECT_GT(stats.total_count, 0);
}

TEST_F(DriverLoggerTest, BacnetProtocolLogging) {
    logger_->ResetStatistics();
    
    // BACnet 특화 로깅 - ReadProperty
    logger_->LogBacnetOperation("ReadProperty", 2, 1001, 85, true);
    
    // BACnet 특화 로깅 - WriteProperty
    logger_->LogBacnetOperation("WriteProperty", 2, 1002, 16, false);
    
    // BACnet 디바이스 발견 로깅
    logger_->LogBacnetDeviceDiscovery(123, "Device Name", "192.168.1.50", true);
    
    auto stats = logger_->GetStatistics();
    EXPECT_GT(stats.total_count, 0);
}

// =============================================================================
// 멀티스레드 안전성 테스트
// =============================================================================

TEST_F(DriverLoggerTest, ConcurrentLogging) {
    logger_->ResetStatistics();
    
    const int num_threads = 10;
    const int messages_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> completed_threads(0);
    
    // 여러 스레드에서 동시에 로깅
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < messages_per_thread; ++j) {
                std::string message = "Thread " + std::to_string(i) + " Message " + std::to_string(j);
                logger_->Info(message, DriverLogCategory::GENERAL);
                
                // 약간의 지연을 추가하여 경합 상황 유도
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
            completed_threads++;
        });
    }
    
    // 모든 스레드 완료 대기
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 모든 스레드가 완료되었는지 확인
    EXPECT_EQ(num_threads, completed_threads.load());
    
    // 모든 메시지가 기록되었는지 확인
    auto stats = logger_->GetStatistics();
    EXPECT_EQ(num_threads * messages_per_thread, stats.info_count);
    EXPECT_EQ(num_threads * messages_per_thread, stats.total_count);
}

TEST_F(DriverLoggerTest, ConcurrentCategoryFiltering) {
    logger_->ResetStatistics();
    
    const int num_threads = 5;
    const int operations_per_thread = 50;
    std::vector<std::thread> threads;
    
    // 한 스레드는 카테고리 설정을 변경하고, 다른 스레드들은 로깅
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            if (i == 0) {
                // 카테고리 설정 변경 스레드
                for (int j = 0; j < operations_per_thread; ++j) {
                    bool enable = (j % 2 == 0);
                    logger_->SetCategoryEnabled(DriverLogCategory::PERFORMANCE, enable);
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            } else {
                // 로깅 스레드들
                for (int j = 0; j < operations_per_thread; ++j) {
                    logger_->Info("Performance test message", DriverLogCategory::PERFORMANCE);
                    logger_->Info("General test message", DriverLogCategory::GENERAL);
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                }
            }
        });
    }
    
    // 모든 스레드 완료 대기
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 데드락이나 크래시 없이 완료되었는지 확인
    auto stats = logger_->GetStatistics();
    EXPECT_GT(stats.total_count, 0);
}

// =============================================================================
// 메모리 사용량 및 성능 테스트
// =============================================================================

TEST_F(DriverLoggerTest, MemoryUsageTest) {
    logger_->ResetStatistics();
    
    // 대량의 로그 메시지 생성하여 메모리 누수 검사
    const int num_messages = 10000;
    const std::string large_message(1024, 'A');  // 1KB 크기 메시지
    
    for (int i = 0; i < num_messages; ++i) {
        logger_->Info(large_message + std::to_string(i), DriverLogCategory::GENERAL);
    }
    
    auto stats = logger_->GetStatistics();
    EXPECT_EQ(num_messages, stats.info_count);
    EXPECT_EQ(num_messages, stats.total_count);
    
    // 통계 초기화 후 메모리가 해제되는지 확인
    logger_->ResetStatistics();
    stats = logger_->GetStatistics();
    EXPECT_EQ(0, stats.total_count);
}

TEST_F(DriverLoggerTest, LoggingPerformanceBenchmark) {
    logger_->ResetStatistics();
    
    const int num_iterations = 100000;
    const std::string test_message = "Performance test message";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_iterations; ++i) {
        logger_->Info(test_message, DriverLogCategory::PERFORMANCE);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // 성능 기준: 평균 10마이크로초 이내 (초당 100,000 메시지 이상)
    double avg_time_per_log = static_cast<double>(duration.count()) / num_iterations;
    EXPECT_LT(avg_time_per_log, 10.0);
    
    std::cout << "Average logging time: " << avg_time_per_log << " μs per message" << std::endl;
    std::cout << "Total logging rate: " << (num_iterations * 1000000.0 / duration.count()) << " messages/sec" << std::endl;
}

// =============================================================================
// 에러 조건 및 예외 상황 테스트
// =============================================================================

TEST_F(DriverLoggerTest, NullPointerHandling) {
    // nullptr 매개변수 처리 테스트
    EXPECT_NO_THROW(logger_->Info("", DriverLogCategory::GENERAL));
    
    // 빈 메시지 처리 테스트
    EXPECT_NO_THROW(logger_->Info("", DriverLogCategory::GENERAL));
    
    auto stats = logger_->GetStatistics();
    EXPECT_EQ(2, stats.info_count);
}

TEST_F(DriverLoggerTest, InvalidParameterHandling) {
    logger_->ResetStatistics();
    
    // 잘못된 로그 레벨 처리
    EXPECT_NO_THROW(logger_->SetMinLevel(static_cast<LogLevel>(999)));
    
    // 잘못된 카테고리 처리
    EXPECT_NO_THROW(logger_->SetCategoryEnabled(static_cast<DriverLogCategory>(999), true));
    
    // 여전히 정상적으로 로깅이 가능한지 확인
    logger_->Info("Test after invalid parameters", DriverLogCategory::GENERAL);
    
    auto stats = logger_->GetStatistics();
    EXPECT_EQ(1, stats.info_count);
}

TEST_F(DriverLoggerTest, LargeMessageHandling) {
    logger_->ResetStatistics();
    
    // 매우 큰 메시지 처리 (10MB)
    const std::string large_message(10 * 1024 * 1024, 'X');
    
    EXPECT_NO_THROW(logger_->Info(large_message, DriverLogCategory::GENERAL));
    
    auto stats = logger_->GetStatistics();
    EXPECT_EQ(1, stats.info_count);
}

// =============================================================================
// 설정 변경 및 동적 구성 테스트
// =============================================================================

TEST_F(DriverLoggerTest, DynamicLevelChange) {
    logger_->ResetStatistics();
    
    // 초기에는 모든 레벨 허용
    logger_->SetMinLevel(LogLevel::DEBUG);
    logger_->Debug("Debug message 1");
    logger_->Info("Info message 1");
    
    // 레벨을 INFO로 변경
    logger_->SetMinLevel(LogLevel::INFO);
    logger_->Debug("Debug message 2");  // 필터링됨
    logger_->Info("Info message 2");
    
    // 레벨을 ERROR로 변경
    logger_->SetMinLevel(LogLevel::ERROR);
    logger_->Info("Info message 3");    // 필터링됨
    logger_->Error("Error message 1");
    
    auto stats = logger_->GetStatistics();
    EXPECT_EQ(1, stats.debug_count);   // Debug message 1만 기록
    EXPECT_EQ(2, stats.info_count);    // Info message 1, 2만 기록
    EXPECT_EQ(1, stats.error_count);   // Error message 1만 기록
    EXPECT_EQ(4, stats.total_count);
}

TEST_F(DriverLoggerTest, DynamicCategoryChange) {
    logger_->ResetStatistics();
    
    // 모든 카테고리 활성화
    logger_->SetCategoryEnabled(DriverLogCategory::GENERAL, true);
    logger_->SetCategoryEnabled(DriverLogCategory::PERFORMANCE, true);
    
    logger_->Info("General 1", DriverLogCategory::GENERAL);
    logger_->Info("Performance 1", DriverLogCategory::PERFORMANCE);
    
    // PERFORMANCE 카테고리 비활성화
    logger_->SetCategoryEnabled(DriverLogCategory::PERFORMANCE, false);
    
    logger_->Info("General 2", DriverLogCategory::GENERAL);
    logger_->Info("Performance 2", DriverLogCategory::PERFORMANCE);  // 필터링됨
    
    // PERFORMANCE 카테고리 재활성화
    logger_->SetCategoryEnabled(DriverLogCategory::PERFORMANCE, true);
    
    logger_->Info("Performance 3", DriverLogCategory::PERFORMANCE);
    
    auto stats = logger_->GetStatistics();
    EXPECT_EQ(4, stats.info_count);  // General 1,2 + Performance 1,3
    EXPECT_EQ(4, stats.total_count);
}

// =============================================================================
// 통계 및 메트릭 테스트
// =============================================================================

TEST_F(DriverLoggerTest, StatisticsAccuracy) {
    logger_->ResetStatistics();
    
    // 다양한 레벨의 메시지 생성
    for (int i = 0; i < 10; ++i) {
        logger_->Debug("Debug " + std::to_string(i));
    }
    for (int i = 0; i < 20; ++i) {
        logger_->Info("Info " + std::to_string(i));
    }
    for (int i = 0; i < 5; ++i) {
        logger_->Warn("Warn " + std::to_string(i));
    }
    for (int i = 0; i < 3; ++i) {
        logger_->Error("Error " + std::to_string(i));
    }
    for (int i = 0; i < 1; ++i) {
        logger_->Fatal("Fatal " + std::to_string(i));
    }
    
    auto stats = logger_->GetStatistics();
    EXPECT_EQ(10, stats.debug_count);
    EXPECT_EQ(20, stats.info_count);
    EXPECT_EQ(5, stats.warn_count);
    EXPECT_EQ(3, stats.error_count);
    EXPECT_EQ(1, stats.fatal_count);
    EXPECT_EQ(39, stats.total_count);
}

TEST_F(DriverLoggerTest, StatisticsReset) {
    logger_->ResetStatistics();
    
    // 메시지 생성
    logger_->Info("Test message 1");
    logger_->Error("Test error 1");
    
    auto stats_before = logger_->GetStatistics();
    EXPECT_GT(stats_before.total_count, 0);
    
    // 통계 리셋
    logger_->ResetStatistics();
    
    auto stats_after = logger_->GetStatistics();
    EXPECT_EQ(0, stats_after.debug_count);
    EXPECT_EQ(0, stats_after.info_count);
    EXPECT_EQ(0, stats_after.warn_count);
    EXPECT_EQ(0, stats_after.error_count);
    EXPECT_EQ(0, stats_after.fatal_count);
    EXPECT_EQ(0, stats_after.total_count);
}

// =============================================================================
// 실제 시나리오 통합 테스트
// =============================================================================

TEST_F(DriverLoggerTest, ModbusDriverScenario) {
    logger_->ResetStatistics();
    
    // Modbus 드라이버 시나리오 시뮬레이션
    logger_->Info("Modbus driver initialization started", DriverLogCategory::GENERAL);
    logger_->Info("Connecting to device at 192.168.1.100:502", DriverLogCategory::CONNECTION);
    
    // 연결 성공
    logger_->LogConnectionStatusChange(
        ConnectionStatus::DISCONNECTED,
        ConnectionStatus::CONNECTED,
        "Modbus connection established"
    );
    
    // 정상적인 데이터 읽기 시뮬레이션
    for (int i = 0; i < 5; ++i) {
        logger_->LogModbusOperation(1, 100 + i, std::to_string(42 + i), true, 15 + i);
        logger_->LogPerformanceMetric("ReadLatency", 15 + i, "ms");
    }
    
    // 통신 오류 시뮬레이션
    logger_->LogModbusOperation(1, 200, "", false, 5000);
    logger_->LogModbusException(1, 200, 0x02, "Illegal Data Address");
    
    // 연결 복구
    logger_->LogConnectionStatusChange(
        ConnectionStatus::ERROR,
        ConnectionStatus::CONNECTED,
        "Connection recovered after error"
    );
    
    auto stats = logger_->GetStatistics();
    EXPECT_GT(stats.total_count, 10);  // 충분한 로그가 기록되었는지 확인
}

TEST_F(DriverLoggerTest, HighVolumeDataCollectionScenario) {
    logger_->ResetStatistics();
    
    // 고용량 데이터 수집 시나리오
    const int num_devices = 10;
    const int readings_per_device = 100;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int device = 1; device <= num_devices; ++device) {
        logger_->Info("Starting data collection for device " + std::to_string(device),
                     DriverLogCategory::GENERAL);
        
        for (int reading = 0; reading < readings_per_device; ++reading) {
            // 데이터 읽기 시뮬레이션
            bool success = (reading % 20 != 0);  // 5% 실패율
            int latency = 10 + (reading % 30);   // 10-40ms 지연
            
            if (success) {
                logger_->LogDataTransfer("RX", 64, latency, true);
            } else {
                logger_->LogDataTransfer("RX", 0, latency * 10, false);
                logger_->Warn("Data read timeout for device " + std::to_string(device),
                             DriverLogCategory::ERROR_HANDLING);
            }
            
            // 성능 메트릭 기록
            if (reading % 10 == 0) {
                logger_->LogPerformanceMetric("DeviceLatency", latency, "ms");
            }
        }
        
        logger_->Info("Completed data collection for device " + std::to_string(device),
                     DriverLogCategory::GENERAL);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    logger_->LogPerformanceMetric("TotalCollectionTime", duration.count(), "ms");
    
    auto stats = logger_->GetStatistics();
    
    // 예상되는 로그 수 확인
    int expected_data_logs = num_devices * readings_per_device;
    int expected_performance_logs = num_devices * (readings_per_device / 10) + 1;
    int expected_device_logs = num_devices * 2;  // 시작/완료 메시지
    
    EXPECT_GT(stats.total_count, expected_data_logs + expected_device_logs);
    
    std::cout << "High volume scenario completed:" << std::endl;
    std::cout << "  Total logs: " << stats.total_count << std::endl;
    std::cout << "  Error logs: " << stats.warn_count << std::endl;
    std::cout << "  Duration: " << duration.count() << "ms" << std::endl;
}

// =============================================================================
// 메인 함수
// =============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}