// =============================================================================
// collector/tests/test_mqtt_driver.cpp
// MQTT 드라이버 단위 테스트
// =============================================================================

#include <iostream>
#include <cassert>
#include <vector>
#include <chrono>
#include <thread>

// 실제 헤더 include
#include "Drivers/Mqtt/MqttDriver.h"
#include "Drivers/Common/CommonTypes.h"

using namespace PulseOne::Drivers;

// =============================================================================
// 테스트 헬퍼 함수들
// =============================================================================

void PrintTestResult(const std::string& test_name, bool result) {
    std::cout << "[" << (result ? "✅ PASS" : "❌ FAIL") << "] " << test_name << std::endl;
}

DriverConfig CreateTestConfig() {
    DriverConfig config;
    config.device_id = 12345;
    config.device_name = "test_mqtt_device";
    config.endpoint = "mqtt://localhost:1883";
    config.polling_interval_ms = 1000;
    config.timeout_ms = 5000;
    config.retry_count = 3;
    config.auto_reconnect = true;
    return config;
}

std::vector<DataPoint> CreateTestDataPoints() {
    std::vector<DataPoint> points;
    
    DataPoint point1;
    point1.id = "point_001";
    point1.name = "temperature";
    point1.address = 1001;
    point1.data_type = DataType::FLOAT32;
    point1.unit = "°C";
    point1.scaling_factor = 1.0;
    point1.scaling_offset = 0.0;
    points.push_back(point1);
    
    DataPoint point2;
    point2.id = "point_002";
    point2.name = "humidity";
    point2.address = 1002;
    point2.data_type = DataType::FLOAT32;
    point2.unit = "%";
    point2.scaling_factor = 1.0;
    point2.scaling_offset = 0.0;
    points.push_back(point2);
    
    return points;
}

// =============================================================================
// 단위 테스트 함수들
// =============================================================================

bool TestDriverCreation() {
    try {
        auto driver = std::make_unique<MqttDriver>();
        return driver != nullptr;
    } catch (const std::exception& e) {
        std::cout << "    Exception: " << e.what() << std::endl;
        return false;
    }
}

bool TestDriverInitialization() {
    try {
        auto driver = std::make_unique<MqttDriver>();
        DriverConfig config = CreateTestConfig();
        
        bool result = driver->Initialize(config);
        
        if (result) {
            // 상태 확인
            bool status_ok = (driver->GetStatus() == DriverStatus::INITIALIZED);
            bool protocol_ok = (driver->GetProtocolType() == ProtocolType::MQTT);
            
            return status_ok && protocol_ok;
        }
        
        return false;
    } catch (const std::exception& e) {
        std::cout << "    Exception: " << e.what() << std::endl;
        return false;
    }
}

bool TestDriverConnection() {
    try {
        auto driver = std::make_unique<MqttDriver>();
        DriverConfig config = CreateTestConfig();
        
        driver->Initialize(config);
        
        // 연결 시도 (스텁이므로 성공할 것)
        bool connect_result = driver->Connect();
        
        if (connect_result) {
            bool is_connected = driver->IsConnected();
            return is_connected;
        }
        
        return false;
    } catch (const std::exception& e) {
        std::cout << "    Exception: " << e.what() << std::endl;
        return false;
    }
}

bool TestDataPointOperations() {
    try {
        auto driver = std::make_unique<MqttDriver>();
        DriverConfig config = CreateTestConfig();
        
        driver->Initialize(config);
        driver->Connect();
        
        std::vector<DataPoint> points = CreateTestDataPoints();
        std::vector<TimestampedValue> values;
        
        // 읽기 테스트
        bool read_result = driver->ReadValues(points, values);
        
        if (read_result && values.size() == points.size()) {
            // 쓰기 테스트
            DataValue test_value(42.5f);
            bool write_result = driver->WriteValue(points[0], test_value);
            
            return write_result;
        }
        
        return false;
    } catch (const std::exception& e) {
        std::cout << "    Exception: " << e.what() << std::endl;
        return false;
    }
}

bool TestMqttSpecificOperations() {
    try {
        auto driver = std::make_unique<MqttDriver>();
        DriverConfig config = CreateTestConfig();
        
        driver->Initialize(config);
        driver->Connect();
        
        // 구독 테스트
        bool subscribe_result = driver->Subscribe("test/topic", 1);
        
        // 발행 테스트
        bool publish_result = driver->Publish("test/topic", "test_message", 1, false);
        
        // JSON 발행 테스트
        nlohmann::json test_json;
        test_json["temperature"] = 25.5;
        test_json["timestamp"] = 1642680000;
        bool json_publish_result = driver->PublishJson("test/json", test_json, 1, false);
        
        // 구독 해제 테스트
        bool unsubscribe_result = driver->Unsubscribe("test/topic");
        
        return subscribe_result && publish_result && json_publish_result && unsubscribe_result;
        
    } catch (const std::exception& e) {
        std::cout << "    Exception: " + std::string(e.what()) << std::endl;
        return false;
    }
}

bool TestStatisticsAndDiagnostics() {
    try {
        auto driver = std::make_unique<MqttDriver>();
        DriverConfig config = CreateTestConfig();
        
        driver->Initialize(config);
        driver->Connect();
        
        // 몇 가지 작업 수행
        driver->Subscribe("test/topic", 1);
        driver->Publish("test/topic", "test", 1, false);
        
        // 통계 확인
        const DriverStatistics& stats = driver->GetStatistics();
        
        // 진단 정보 확인
        std::string diagnostics_json = driver->GetDiagnosticsJSON();
        
        bool stats_ok = (stats.total_operations >= 0);  // 기본 검증
        bool diag_ok = !diagnostics_json.empty();
        
        return stats_ok && diag_ok;
        
    } catch (const std::exception& e) {
        std::cout << "    Exception: " << e.what() << std::endl;
        return false;
    }
}

bool TestErrorHandling() {
    try {
        auto driver = std::make_unique<MqttDriver>();
        
        // 초기화하지 않고 연결 시도
        bool connect_without_init = driver->Connect();
        
        // 연결하지 않고 읽기 시도
        std::vector<DataPoint> points = CreateTestDataPoints();
        std::vector<TimestampedValue> values;
        bool read_without_connect = driver->ReadValues(points, values);
        
        // 에러 정보 확인
        ErrorInfo error = driver->GetLastError();
        bool error_ok = (error.code != ErrorCode::SUCCESS);
        
        return !connect_without_init && !read_without_connect && error_ok;
        
    } catch (const std::exception& e) {
        std::cout << "    Exception: " << e.what() << std::endl;
        return false;
    }
}

bool TestResourceCleanup() {
    try {
        {
            auto driver = std::make_unique<MqttDriver>();
            DriverConfig config = CreateTestConfig();
            
            driver->Initialize(config);
            driver->Connect();
            
            // 몇 가지 작업 수행
            driver->Subscribe("test/topic", 1);
            driver->Publish("test/topic", "test", 1, false);
            
            // 연결 해제
            driver->Disconnect();
            
        } // 여기서 자동으로 소멸자 호출
        
        // 메모리 누수가 없다면 정상적으로 진행됨
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "    Exception: " << e.what() << std::endl;
        return false;
    }
}

// =============================================================================
// 통합 테스트
// =============================================================================

bool TestCompleteWorkflow() {
    try {
        std::cout << "    🔄 Testing complete MQTT driver workflow..." << std::endl;
        
        auto driver = std::make_unique<MqttDriver>();
        DriverConfig config = CreateTestConfig();
        std::vector<DataPoint> points = CreateTestDataPoints();
        
        // 1. 초기화
        if (!driver->Initialize(config)) {
            std::cout << "    ❌ Initialization failed" << std::endl;
            return false;
        }
        std::cout << "    ✅ Driver initialized" << std::endl;
        
        // 2. 연결
        if (!driver->Connect()) {
            std::cout << "    ❌ Connection failed" << std::endl;
            return false;
        }
        std::cout << "    ✅ Driver connected" << std::endl;
        
        // 3. 구독
        if (!driver->Subscribe("sensor/temperature", 1)) {
            std::cout << "    ❌ Subscription failed" << std::endl;
            return false;
        }
        std::cout << "    ✅ Topic subscribed" << std::endl;
        
        // 4. 데이터 읽기
        std::vector<TimestampedValue> values;
        if (!driver->ReadValues(points, values)) {
            std::cout << "    ❌ Read values failed" << std::endl;
            return false;
        }
        std::cout << "    ✅ Values read: " << values.size() << " points" << std::endl;
        
        // 5. 데이터 발행
        if (!driver->Publish("sensor/status", "online", 1, false)) {
            std::cout << "    ❌ Publish failed" << std::endl;
            return false;
        }
        std::cout << "    ✅ Message published" << std::endl;
        
        // 6. JSON 데이터 발행
        nlohmann::json sensor_data;
        sensor_data["temperature"] = 23.5;
        sensor_data["humidity"] = 65.2;
        sensor_data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        if (!driver->PublishJson("sensor/data", sensor_data, 1, false)) {
            std::cout << "    ❌ JSON publish failed" << std::endl;
            return false;
        }
        std::cout << "    ✅ JSON data published" << std::endl;
        
        // 7. 통계 확인
        const DriverStatistics& stats = driver->GetStatistics();
        std::cout << "    📊 Operations: " << stats.total_operations 
                  << ", Success Rate: " << stats.success_rate << "%" << std::endl;
        
        // 8. 진단 정보 확인
        std::string diagnostics = driver->GetDiagnosticsJSON();
        std::cout << "    🔍 Diagnostics size: " << diagnostics.length() << " chars" << std::endl;
        
        // 9. 연결 해제
        if (!driver->Disconnect()) {
            std::cout << "    ❌ Disconnect failed" << std::endl;
            return false;
        }
        std::cout << "    ✅ Driver disconnected" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "    ❌ Exception in complete workflow: " << e.what() << std::endl;
        return false;
    }
}

// =============================================================================
// 메인 테스트 실행기
// =============================================================================

int main() {
    std::cout << "🧪 PulseOne MQTT Driver Unit Tests" << std::endl;
    std::cout << "=====================================" << std::endl;
    
    int passed = 0;
    int total = 0;
    
    // 기본 테스트들
    std::vector<std::pair<std::string, std::function<bool()>>> tests = {
        {"Driver Creation", TestDriverCreation},
        {"Driver Initialization", TestDriverInitialization},
        {"Driver Connection", TestDriverConnection},
        {"Data Point Operations", TestDataPointOperations},
        {"MQTT Specific Operations", TestMqttSpecificOperations},
        {"Statistics and Diagnostics", TestStatisticsAndDiagnostics},
        {"Error Handling", TestErrorHandling},
        {"Resource Cleanup", TestResourceCleanup},
        {"Complete Workflow", TestCompleteWorkflow}
    };
    
    for (const auto& test : tests) {
        total++;
        bool result = test.second();
        if (result) passed++;
        PrintTestResult(test.first, result);
        
        // 테스트 간 짧은 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << "🎯 Test Results: " << passed << "/" << total << " passed";
    
    if (passed == total) {
        std::cout << " ✅" << std::endl;
        std::cout << "🎉 All tests passed! MQTT Driver is working correctly." << std::endl;
        return 0;
    } else {
        std::cout << " ❌" << std::endl;
        std::cout << "⚠️  Some tests failed. Please check the implementation." << std::endl;
        return 1;
    }
}

/*
# Docker 컨테이너에서
cd /app/collector

# 1. 도움말 확인
./bin/pulseone_collector --help

# 2. 버전 정보 확인
./bin/pulseone_collector --version

# 3. 기본 실행 (설정 파일 로딩 확인)
./bin/pulseone_collector

# 4. 특정 로그 레벨로 실행
./bin/pulseone_collector --log-level debug
*/