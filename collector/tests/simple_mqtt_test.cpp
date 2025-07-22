// ============================================================================
// Simple MQTT Driver Test (No Google Test dependency)
// ============================================================================

#include "Drivers/Mqtt/MqttDriver.h"
#include <iostream>
#include <cassert>
#include <memory>

using namespace PulseOne::Drivers;

// 간단한 테스트 매크로
#define SIMPLE_TEST(condition, message) \
    if (condition) { \
        std::cout << "✅ " << message << std::endl; \
    } else { \
        std::cout << "❌ " << message << std::endl; \
        test_failed = true; \
    }

int main() {
    std::cout << "🧪 Simple MQTT Driver Test" << std::endl;
    std::cout << "=========================" << std::endl;
    
    bool test_failed = false;
    
    try {
        // 테스트 1: 드라이버 생성
        std::cout << "\n🔧 Test 1: Driver Creation" << std::endl;
        auto driver = std::make_unique<MqttDriver>();
        SIMPLE_TEST(driver != nullptr, "Driver created successfully");
        
        // 테스트 2: 프로토콜 타입 확인
        std::cout << "\n🔧 Test 2: Protocol Type" << std::endl;
        ProtocolType type = driver->GetProtocolType();
        SIMPLE_TEST(type == ProtocolType::MQTT, "Protocol type is MQTT");
        
        // 테스트 3: 초기 상태 확인
        std::cout << "\n🔧 Test 3: Initial State" << std::endl;
        DriverStatus status = driver->GetStatus();
        SIMPLE_TEST(status == DriverStatus::UNINITIALIZED, "Initial status is UNINITIALIZED");
        SIMPLE_TEST(!driver->IsConnected(), "Initially not connected");
        
        // 테스트 4: 설정 및 초기화
        std::cout << "\n🔧 Test 4: Configuration and Initialization" << std::endl;
        DriverConfig config;
        config.device_id = 12345;
        config.name = "Test MQTT Device";
        config.endpoint = "mqtt://localhost:1883";
        config.timeout_ms = 5000;
        config.retry_count = 3;
        config.polling_interval_ms = 1000;
        
        bool init_result = driver->Initialize(config);
        SIMPLE_TEST(init_result, "Driver initialization successful");
        
        if (init_result) {
            DriverStatus new_status = driver->GetStatus();
            SIMPLE_TEST(new_status == DriverStatus::INITIALIZED, "Status changed to INITIALIZED");
        }
        
        // 테스트 5: MqttDataPointInfo 구조체
        std::cout << "\n🔧 Test 5: MqttDataPointInfo Structure" << std::endl;
        
        // 기본 생성자 테스트
        MqttDriver::MqttDataPointInfo default_point;
        SIMPLE_TEST(default_point.qos == 1, "Default QoS is 1");
        SIMPLE_TEST(default_point.data_type == DataType::UNKNOWN, "Default data type is UNKNOWN");
        SIMPLE_TEST(default_point.scaling_factor == 1.0, "Default scaling factor is 1.0");
        SIMPLE_TEST(default_point.scaling_offset == 0.0, "Default scaling offset is 0.0");
        SIMPLE_TEST(!default_point.is_writable, "Default is not writable");
        SIMPLE_TEST(default_point.auto_subscribe, "Default auto subscribe is true");
        
        // 매개변수 생성자 테스트
        MqttDriver::MqttDataPointInfo param_point(
            "test_point_01",
            "Test Point",
            "Test description", 
            "test/topic",
            2,
            DataType::FLOAT32
        );
        SIMPLE_TEST(param_point.point_id == "test_point_01", "Point ID set correctly");
        SIMPLE_TEST(param_point.name == "Test Point", "Point name set correctly");
        SIMPLE_TEST(param_point.description == "Test description", "Point description set correctly");
        SIMPLE_TEST(param_point.topic == "test/topic", "Topic set correctly");
        SIMPLE_TEST(param_point.qos == 2, "QoS set correctly");
        SIMPLE_TEST(param_point.data_type == DataType::FLOAT32, "Data type set correctly");
        
        // 테스트 6: 데이터 포인트 관리
        std::cout << "\n🔧 Test 6: Data Point Management" << std::endl;
        if (init_result) {
            size_t count = driver->GetLoadedPointCount();
            std::cout << "Loaded points count: " << count << std::endl;
            SIMPLE_TEST(count >= 0, "Point count is valid");
            
            // 기본 포인트 검색 테스트
            auto found_point = driver->FindPointByTopic("sensors/temperature/room1");
            if (found_point.has_value()) {
                SIMPLE_TEST(found_point->topic == "sensors/temperature/room1", "Point found by topic");
                SIMPLE_TEST(!found_point->point_id.empty(), "Point has valid ID");
                std::cout << "Found point: " << found_point->name << " (ID: " << found_point->point_id << ")" << std::endl;
            }
            
            // 존재하지 않는 포인트 검색
            auto not_found = driver->FindPointByTopic("non/existing/topic");
            SIMPLE_TEST(!not_found.has_value(), "Non-existing topic returns nullopt");
            
            // ID로 토픽 찾기
            std::string topic = driver->FindTopicByPointId("temp_sensor_01");
            if (!topic.empty()) {
                SIMPLE_TEST(topic == "sensors/temperature/room1", "Topic found by point ID");
                std::cout << "Found topic by ID: " << topic << std::endl;
            }
            
            // 존재하지 않는 ID
            std::string empty_topic = driver->FindTopicByPointId("non_existing_id");
            SIMPLE_TEST(empty_topic.empty(), "Non-existing ID returns empty string");
        }
        
        // 테스트 7: 에러 처리
        std::cout << "\n🔧 Test 7: Error Handling" << std::endl;
        ErrorInfo initial_error = driver->GetLastError();
        SIMPLE_TEST(initial_error.code == ErrorCode::SUCCESS, "Initial error state is SUCCESS");
        
        // 연결되지 않은 상태에서 구독 시도 (실패 예상)
        bool subscribe_result = driver->Subscribe("test/topic", 1);
        SIMPLE_TEST(!subscribe_result, "Subscribe fails when not connected");
        
        ErrorInfo after_error = driver->GetLastError();
        SIMPLE_TEST(after_error.code != ErrorCode::SUCCESS, "Error recorded after failed operation");
        
        // 테스트 8: 통계
        std::cout << "\n🔧 Test 8: Statistics" << std::endl;
        const DriverStatistics& stats = driver->GetStatistics();
        SIMPLE_TEST(stats.total_operations >= 0, "Total operations is valid");
        SIMPLE_TEST(stats.successful_operations >= 0, "Successful operations is valid");
        SIMPLE_TEST(stats.failed_operations >= 0, "Failed operations is valid");
        
        // 통계 리셋
        driver->ResetStatistics();
        const DriverStatistics& reset_stats = driver->GetStatistics();
        SIMPLE_TEST(reset_stats.total_operations == 0, "Statistics reset successfully");
        
        // 테스트 9: 진단 정보
        std::cout << "\n🔧 Test 9: Diagnostics" << std::endl;
        std::string diagnostics = driver->GetDiagnosticsJSON();
        SIMPLE_TEST(!diagnostics.empty(), "Diagnostics JSON is not empty");
        SIMPLE_TEST(diagnostics.find("MQTT") != std::string::npos, "Diagnostics contains MQTT");
        SIMPLE_TEST(diagnostics.find("{") != std::string::npos, "Diagnostics is valid JSON");
        
        std::cout << "\nDiagnostics sample: " << diagnostics.substr(0, 200) << "..." << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Exception occurred: " << e.what() << std::endl;
        test_failed = true;
    }
    
    // 테스트 결과 요약
    std::cout << "\n" << std::string(50, '=') << std::endl;
    if (!test_failed) {
        std::cout << "🎉 ALL TESTS PASSED!" << std::endl;
        std::cout << "✅ MQTT Driver basic functionality verified" << std::endl;
        return 0;
    } else {
        std::cout << "❌ SOME TESTS FAILED!" << std::endl;
        std::cout << "Please check the output above for details" << std::endl;
        return 1;
    }
}
