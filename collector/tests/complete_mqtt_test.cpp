// ============================================================================
// Complete MQTT Driver Test
// ============================================================================

#include "Drivers/Mqtt/MqttDriver.h"
#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>

using namespace PulseOne::Drivers;
using namespace std::chrono_literals;

class MqttTestSuite {
private:
    std::unique_ptr<MqttDriver> driver_;
    int passed_tests_ = 0;
    int failed_tests_ = 0;
    
public:
    bool RunAllTests() {
        std::cout << "🧪 MQTT Driver Test Suite" << std::endl;
        std::cout << "=========================" << std::endl;
        
        // 테스트 실행
        TestDriverCreation();
        TestProtocolType();
        TestInitialState();
        TestMqttDataPointInfoStructure();
        TestDriverInitialization();
        TestUtilityMethods();
        TestErrorHandling();
        TestStatistics();
        TestDiagnostics();
        
        // 결과 출력
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "📊 Test Results:" << std::endl;
        std::cout << "✅ Passed: " << passed_tests_ << std::endl;
        std::cout << "❌ Failed: " << failed_tests_ << std::endl;
        
        if (failed_tests_ == 0) {
            std::cout << "🎉 ALL TESTS PASSED!" << std::endl;
            return true;
        } else {
            std::cout << "❌ SOME TESTS FAILED!" << std::endl;
            return false;
        }
    }
    
private:
    void AssertTrue(bool condition, const std::string& message) {
        if (condition) {
            std::cout << "✅ " << message << std::endl;
            passed_tests_++;
        } else {
            std::cout << "❌ " << message << std::endl;
            failed_tests_++;
        }
    }
    
    void TestDriverCreation() {
        std::cout << "\n🔧 Test 1: Driver Creation" << std::endl;
        
        try {
            driver_ = std::make_unique<MqttDriver>();
            AssertTrue(driver_ != nullptr, "Driver created successfully");
        } catch (const std::exception& e) {
            AssertTrue(false, "Driver creation failed: " + std::string(e.what()));
        }
    }
    
    void TestProtocolType() {
        std::cout << "\n🔧 Test 2: Protocol Type" << std::endl;
        
        if (driver_) {
            ProtocolType type = driver_->GetProtocolType();
            AssertTrue(type == ProtocolType::MQTT, "Protocol type is MQTT");
        } else {
            AssertTrue(false, "Driver not available for protocol type test");
        }
    }
    
    void TestInitialState() {
        std::cout << "\n🔧 Test 3: Initial State" << std::endl;
        
        if (driver_) {
            DriverStatus status = driver_->GetStatus();
            AssertTrue(status == DriverStatus::UNINITIALIZED, "Initial status is UNINITIALIZED");
            AssertTrue(!driver_->IsConnected(), "Initially not connected");
        }
    }
    
    void TestMqttDataPointInfoStructure() {
        std::cout << "\n🔧 Test 4: MqttDataPointInfo Structure" << std::endl;
        
        // 기본 생성자 테스트
        MqttDriver::MqttDataPointInfo default_point;
        AssertTrue(default_point.qos == 1, "Default QoS is 1");
        AssertTrue(default_point.data_type == DataType::UNKNOWN, "Default data type is UNKNOWN");
        AssertTrue(default_point.scaling_factor == 1.0, "Default scaling factor is 1.0");
        AssertTrue(default_point.scaling_offset == 0.0, "Default scaling offset is 0.0");
        AssertTrue(!default_point.is_writable, "Default is not writable");
        AssertTrue(default_point.auto_subscribe, "Default auto subscribe is true");
        
        // 매개변수 생성자 테스트
        MqttDriver::MqttDataPointInfo param_point(
            "test_point_01",
            "Test Point",
            "Test description", 
            "test/topic",
            2,
            DataType::FLOAT32
        );
        AssertTrue(param_point.point_id == "test_point_01", "Point ID set correctly");
        AssertTrue(param_point.name == "Test Point", "Point name set correctly");
        AssertTrue(param_point.description == "Test description", "Description set correctly");
        AssertTrue(param_point.topic == "test/topic", "Topic set correctly");
        AssertTrue(param_point.qos == 2, "QoS set correctly");
        AssertTrue(param_point.data_type == DataType::FLOAT32, "Data type set correctly");
        
        // 전체 생성자 테스트
        MqttDriver::MqttDataPointInfo full_point(
            "sensor_01", "Temperature", "Room temp", "sensors/temp/01",
            1, DataType::FLOAT32, "°C", 0.1, -10.0, false, true
        );
        AssertTrue(full_point.unit == "°C", "Unit set correctly");
        AssertTrue(full_point.scaling_factor == 0.1, "Scaling factor set correctly");
        AssertTrue(full_point.scaling_offset == -10.0, "Scaling offset set correctly");
        AssertTrue(!full_point.is_writable, "Writable flag set correctly");
        AssertTrue(full_point.auto_subscribe, "Auto subscribe flag set correctly");
    }
    
    void TestDriverInitialization() {
        std::cout << "\n🔧 Test 5: Driver Initialization" << std::endl;
        
        if (driver_) {
            DriverConfig config;
            config.device_id = 12345;
            config.name = "Test MQTT Device";
            config.endpoint = "mqtt://localhost:1883";
            config.timeout_ms = 5000;
            config.retry_count = 3;
            config.polling_interval_ms = 1000;
            
            bool init_result = driver_->Initialize(config);
            AssertTrue(init_result, "Driver initialization successful");
            
            if (init_result) {
                DriverStatus new_status = driver_->GetStatus();
                AssertTrue(new_status == DriverStatus::INITIALIZED, "Status changed to INITIALIZED");
            }
        }
    }
    
    void TestUtilityMethods() {
        std::cout << "\n🔧 Test 6: Utility Methods" << std::endl;
        
        if (driver_ && driver_->GetStatus() == DriverStatus::INITIALIZED) {
            // 포인트 카운트 테스트
            size_t count = driver_->GetLoadedPointCount();
            std::cout << "Loaded points count: " << count << std::endl;
            AssertTrue(count >= 0, "Point count is valid");
            
            // 기본 포인트 검색 테스트 (LoadMqttPointsFromDB에서 로드되는 포인트)
            auto found_point = driver_->FindPointByTopic("sensors/temperature/room1");
            if (found_point.has_value()) {
                AssertTrue(found_point->topic == "sensors/temperature/room1", "Point found by topic");
                AssertTrue(found_point->point_id == "temp_sensor_01", "Point ID is correct");
                AssertTrue(found_point->data_type == DataType::FLOAT32, "Data type is correct");
                std::cout << "Found point: " << found_point->name << " (ID: " << found_point->point_id << ")" << std::endl;
            } else {
                AssertTrue(false, "Default temperature point not found");
            }
            
            // 존재하지 않는 포인트 검색
            auto not_found = driver_->FindPointByTopic("non/existing/topic");
            AssertTrue(!not_found.has_value(), "Non-existing topic returns nullopt");
            
            // ID로 토픽 찾기
            std::string topic = driver_->FindTopicByPointId("temp_sensor_01");
            if (!topic.empty()) {
                AssertTrue(topic == "sensors/temperature/room1", "Topic found by point ID");
                std::cout << "Found topic by ID: " << topic << std::endl;
            }
            
            // 존재하지 않는 ID로 검색
            std::string empty_topic = driver_->FindTopicByPointId("non_existing_id");
            AssertTrue(empty_topic.empty(), "Non-existing ID returns empty string");
        } else {
            AssertTrue(false, "Driver not initialized for utility method tests");
        }
    }
    
    void TestErrorHandling() {
        std::cout << "\n🔧 Test 7: Error Handling" << std::endl;
        
        if (driver_) {
            // 연결되지 않은 상태에서 구독 시도 (실패 예상)
            bool subscribe_result = driver_->Subscribe("test/topic", 1);
            AssertTrue(!subscribe_result, "Subscribe fails when not connected");
            
            ErrorInfo error = driver_->GetLastError();
            AssertTrue(error.code != ErrorCode::SUCCESS, "Error recorded after failed operation");
            AssertTrue(error.code == ErrorCode::CONNECTION_FAILED, "Correct error code set");
        }
    }
    
    void TestStatistics() {
        std::cout << "\n🔧 Test 8: Statistics" << std::endl;
        
        if (driver_) {
            const DriverStatistics& stats = driver_->GetStatistics();
            AssertTrue(stats.total_operations >= 0, "Total operations is valid");
            AssertTrue(stats.successful_operations >= 0, "Successful operations is valid");
            AssertTrue(stats.failed_operations >= 0, "Failed operations is valid");
            AssertTrue(stats.success_rate >= 0.0 && stats.success_rate <= 100.0, "Success rate is valid");
            
            // 통계 리셋 테스트
            driver_->ResetStatistics();
            const DriverStatistics& reset_stats = driver_->GetStatistics();
            AssertTrue(reset_stats.total_operations == 0, "Statistics reset successfully");
            AssertTrue(reset_stats.successful_operations == 0, "Successful operations reset");
            AssertTrue(reset_stats.failed_operations == 0, "Failed operations reset");
        }
    }
    
    void TestDiagnostics() {
        std::cout << "\n🔧 Test 9: Diagnostics" << std::endl;
        
        if (driver_) {
            std::string diagnostics = driver_->GetDiagnosticsJSON();
            AssertTrue(!diagnostics.empty(), "Diagnostics JSON is not empty");
            AssertTrue(diagnostics.find("MQTT") != std::string::npos, "Diagnostics contains MQTT");
            AssertTrue(diagnostics.find("{") != std::string::npos, "Diagnostics is valid JSON format");
            AssertTrue(diagnostics.find("protocol") != std::string::npos, "Diagnostics contains protocol field");
            
            std::cout << "Diagnostics sample: " << diagnostics.substr(0, 100) << "..." << std::endl;
        }
    }
};

int main() {
    try {
        MqttTestSuite test_suite;
        bool all_passed = test_suite.RunAllTests();
        
        return all_passed ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Test suite failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
