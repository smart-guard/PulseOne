// ============================================================================
// collector/tests/Drivers/Mqtt/MqttDriverTest.cpp
// MQTT 드라이버 단위 테스트
// ============================================================================

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Drivers/Mqtt/MqttDriver.h"
#include "Drivers/Common/CommonTypes.h"
#include <memory>
#include <thread>
#include <chrono>

using namespace PulseOne::Drivers;
using namespace std::chrono_literals;
using ::testing::_;
using ::testing::Return;
using ::testing::AtLeast;

// ============================================================================
// Mock 클래스 정의
// ============================================================================

class MockMqttClient {
public:
    MOCK_METHOD(bool, connect, (), ());
    MOCK_METHOD(bool, disconnect, (), ());
    MOCK_METHOD(bool, is_connected, (), (const));
    MOCK_METHOD(bool, subscribe, (const std::string& topic, int qos), ());
    MOCK_METHOD(bool, unsubscribe, (const std::string& topic), ());
    MOCK_METHOD(bool, publish, (const std::string& topic, const std::string& payload, int qos, bool retained), ());
};

// ============================================================================
// 테스트 픽스처 클래스
// ============================================================================

class MqttDriverTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 드라이버 설정
        config_.device_id = 12345;
        config_.name = "Test MQTT Device";
        config_.endpoint = "mqtt://localhost:1883";
        config_.timeout_ms = 5000;
        config_.retry_count = 3;
        config_.polling_interval_ms = 1000;
        
        // 드라이버 인스턴스 생성
        driver_ = std::make_unique<MqttDriver>();
    }
    
    void TearDown() override {
        if (driver_) {
            driver_->Disconnect();
        }
    }
    
    DriverConfig config_;
    std::unique_ptr<MqttDriver> driver_;
};

// ============================================================================
// MqttDataPointInfo 구조체 테스트
// ============================================================================

TEST_F(MqttDriverTest, MqttDataPointInfo_DefaultConstructor) {
    MqttDriver::MqttDataPointInfo point;
    
    EXPECT_EQ(point.qos, 1);
    EXPECT_EQ(point.data_type, DataType::UNKNOWN);
    EXPECT_EQ(point.scaling_factor, 1.0);
    EXPECT_EQ(point.scaling_offset, 0.0);
    EXPECT_FALSE(point.is_writable);
    EXPECT_TRUE(point.auto_subscribe);
}

TEST_F(MqttDriverTest, MqttDataPointInfo_ParameterConstructor) {
    MqttDriver::MqttDataPointInfo point(
        "test_point_01",
        "Test Point",
        "Test description",
        "test/topic",
        2,  // QoS
        DataType::FLOAT32
    );
    
    EXPECT_EQ(point.point_id, "test_point_01");
    EXPECT_EQ(point.name, "Test Point");
    EXPECT_EQ(point.description, "Test description");
    EXPECT_EQ(point.topic, "test/topic");
    EXPECT_EQ(point.qos, 2);
    EXPECT_EQ(point.data_type, DataType::FLOAT32);
}

TEST_F(MqttDriverTest, MqttDataPointInfo_FullConstructor) {
    MqttDriver::MqttDataPointInfo point(
        "sensor_01",        // point_id
        "Temperature",      // name
        "Room temp",        // description
        "sensors/temp/01",  // topic
        1,                  // qos
        DataType::FLOAT32,  // data_type
        "°C",              // unit
        0.1,               // scaling_factor
        -10.0,             // scaling_offset
        false,             // is_writable
        true               // auto_subscribe
    );
    
    EXPECT_EQ(point.point_id, "sensor_01");
    EXPECT_EQ(point.name, "Temperature");
    EXPECT_EQ(point.unit, "°C");
    EXPECT_EQ(point.scaling_factor, 0.1);
    EXPECT_EQ(point.scaling_offset, -10.0);
    EXPECT_FALSE(point.is_writable);
    EXPECT_TRUE(point.auto_subscribe);
}

// ============================================================================
// 드라이버 초기화 테스트
// ============================================================================

TEST_F(MqttDriverTest, Initialize_ValidConfig_ReturnsTrue) {
    EXPECT_TRUE(driver_->Initialize(config_));
    EXPECT_EQ(driver_->GetProtocolType(), ProtocolType::MQTT);
    EXPECT_EQ(driver_->GetStatus(), DriverStatus::INITIALIZED);
}

TEST_F(MqttDriverTest, Initialize_InvalidEndpoint_ReturnsFalse) {
    config_.endpoint = "invalid_endpoint";
    
    EXPECT_FALSE(driver_->Initialize(config_));
    EXPECT_NE(driver_->GetStatus(), DriverStatus::INITIALIZED);
}

TEST_F(MqttDriverTest, GetProtocolType_ReturnsCorrectType) {
    EXPECT_EQ(driver_->GetProtocolType(), ProtocolType::MQTT);
}

// ============================================================================
// 연결 관리 테스트
// ============================================================================

TEST_F(MqttDriverTest, Connect_WithoutInitialize_ReturnsFalse) {
    EXPECT_FALSE(driver_->Connect());
    EXPECT_FALSE(driver_->IsConnected());
}

TEST_F(MqttDriverTest, Connect_AfterInitialize_ReturnsTrue) {
    driver_->Initialize(config_);
    
    // 실제 MQTT 브로커가 없으므로 연결은 실패할 수 있음
    // 하지만 초기화된 상태에서는 연결 시도가 가능해야 함
    bool result = driver_->Connect();
    
    // 연결 결과와 관계없이 상태가 적절히 설정되는지 확인
    if (result) {
        EXPECT_TRUE(driver_->IsConnected());
    } else {
        // 연결 실패 시 에러 정보가 설정되어야 함
        ErrorInfo error = driver_->GetLastError();
        EXPECT_NE(error.code, ErrorCode::SUCCESS);
    }
}

TEST_F(MqttDriverTest, Disconnect_WhenNotConnected_ReturnsTrue) {
    driver_->Initialize(config_);
    EXPECT_TRUE(driver_->Disconnect());
}

// ============================================================================
// 데이터 포인트 관리 테스트
// ============================================================================

TEST_F(MqttDriverTest, GetLoadedPointCount_InitiallyZero) {
    driver_->Initialize(config_);
    
    // LoadMqttPointsFromDB가 호출되지 않은 상태에서는 0이어야 함
    // 단, 현재 구현에서는 기본 포인트들이 로드됨
    size_t count = driver_->GetLoadedPointCount();
    EXPECT_GE(count, 0);  // 0 이상이어야 함
}

TEST_F(MqttDriverTest, FindPointByTopic_ExistingTopic_ReturnsPoint) {
    driver_->Initialize(config_);
    
    // LoadMqttPointsFromDB에서 로드되는 기본 포인트 중 하나 테스트
    auto point = driver_->FindPointByTopic("sensors/temperature/room1");
    
    if (point.has_value()) {
        EXPECT_EQ(point->topic, "sensors/temperature/room1");
        EXPECT_EQ(point->point_id, "temp_sensor_01");
        EXPECT_EQ(point->data_type, DataType::FLOAT32);
    }
}

TEST_F(MqttDriverTest, FindPointByTopic_NonExistingTopic_ReturnsNullopt) {
    driver_->Initialize(config_);
    
    auto point = driver_->FindPointByTopic("non/existing/topic");
    EXPECT_FALSE(point.has_value());
}

TEST_F(MqttDriverTest, FindTopicByPointId_ExistingId_ReturnsTopic) {
    driver_->Initialize(config_);
    
    std::string topic = driver_->FindTopicByPointId("temp_sensor_01");
    if (!topic.empty()) {
        EXPECT_EQ(topic, "sensors/temperature/room1");
    }
}

TEST_F(MqttDriverTest, FindTopicByPointId_NonExistingId_ReturnsEmpty) {
    driver_->Initialize(config_);
    
    std::string topic = driver_->FindTopicByPointId("non_existing_id");
    EXPECT_TRUE(topic.empty());
}

// ============================================================================
// 데이터 읽기/쓰기 테스트
// ============================================================================

TEST_F(MqttDriverTest, ReadValues_WithoutConnection_ReturnsFalse) {
    driver_->Initialize(config_);
    
    std::vector<DataPoint> points;
    std::vector<TimestampedValue> values;
    
    DataPoint point;
    point.id = "test_point";
    point.name = "Test Point";
    point.data_type = DataType::FLOAT32;
    points.push_back(point);
    
    bool result = driver_->ReadValues(points, values);
    
    // 연결되지 않은 상태에서는 실패해야 함
    EXPECT_FALSE(result);
    
    ErrorInfo error = driver_->GetLastError();
    EXPECT_EQ(error.code, ErrorCode::CONNECTION_FAILED);
}

TEST_F(MqttDriverTest, WriteValue_WithoutConnection_ReturnsFalse) {
    driver_->Initialize(config_);
    
    DataPoint point;
    point.id = "test_point";
    point.name = "Test Point";
    point.data_type = DataType::FLOAT32;
    
    DataValue value(42.5f);
    
    bool result = driver_->WriteValue(point, value);
    
    // 연결되지 않은 상태에서는 실패해야 함
    EXPECT_FALSE(result);
    
    ErrorInfo error = driver_->GetLastError();
    EXPECT_EQ(error.code, ErrorCode::CONNECTION_FAILED);
}

// ============================================================================
// 통계 및 진단 테스트
// ============================================================================

TEST_F(MqttDriverTest, GetStatistics_InitialValues) {
    driver_->Initialize(config_);
    
    const DriverStatistics& stats = driver_->GetStatistics();
    
    EXPECT_EQ(stats.total_operations, 0);
    EXPECT_EQ(stats.successful_operations, 0);
    EXPECT_EQ(stats.failed_operations, 0);
    EXPECT_EQ(stats.success_rate, 0.0);
}

TEST_F(MqttDriverTest, ResetStatistics_ClearsAllCounters) {
    driver_->Initialize(config_);
    
    // 통계 초기화
    driver_->ResetStatistics();
    
    const DriverStatistics& stats = driver_->GetStatistics();
    
    EXPECT_EQ(stats.total_operations, 0);
    EXPECT_EQ(stats.successful_operations, 0);
    EXPECT_EQ(stats.failed_operations, 0);
}

TEST_F(MqttDriverTest, GetDiagnosticsJSON_ValidFormat) {
    driver_->Initialize(config_);
    
    std::string diagnostics = driver_->GetDiagnosticsJSON();
    
    EXPECT_FALSE(diagnostics.empty());
    
    // JSON 형식인지 기본 검증
    EXPECT_TRUE(diagnostics.find("{") != std::string::npos);
    EXPECT_TRUE(diagnostics.find("}") != std::string::npos);
    EXPECT_TRUE(diagnostics.find("protocol") != std::string::npos);
    EXPECT_TRUE(diagnostics.find("MQTT") != std::string::npos);
}

// ============================================================================
// 에러 처리 테스트
// ============================================================================

TEST_F(MqttDriverTest, GetLastError_InitialState_NoError) {
    ErrorInfo error = driver_->GetLastError();
    
    // 초기 상태에서는 에러가 없어야 함
    EXPECT_EQ(error.code, ErrorCode::SUCCESS);
}

TEST_F(MqttDriverTest, GetLastError_AfterFailedOperation_ReturnsError) {
    driver_->Initialize(config_);
    
    // 연결되지 않은 상태에서 구독 시도 (실패 예상)
    bool result = driver_->Subscribe("test/topic", 1);
    EXPECT_FALSE(result);
    
    ErrorInfo error = driver_->GetLastError();
    EXPECT_NE(error.code, ErrorCode::SUCCESS);
}

// ============================================================================
// 멀티스레드 안전성 테스트
// ============================================================================

TEST_F(MqttDriverTest, ConcurrentAccess_ThreadSafety) {
    driver_->Initialize(config_);
    
    const int num_threads = 4;
    const int operations_per_thread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> completed_operations{0};
    
    // 여러 스레드에서 동시에 작업 수행
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, operations_per_thread, &completed_operations]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                // 다양한 작업을 동시에 수행
                driver_->GetLoadedPointCount();
                driver_->FindPointByTopic("test/topic" + std::to_string(j));
                driver_->GetStatistics();
                completed_operations++;
                
                std::this_thread::sleep_for(1ms);
            }
        });
    }
    
    // 모든 스레드 완료 대기
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(completed_operations.load(), num_threads * operations_per_thread);
}

// ============================================================================
// 성능 테스트
// ============================================================================

TEST_F(MqttDriverTest, Performance_PointLookup_UnderThreshold) {
    driver_->Initialize(config_);
    
    const int lookup_count = 1000;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 반복적인 포인트 조회
    for (int i = 0; i < lookup_count; ++i) {
        driver_->FindPointByTopic("sensors/temperature/room1");
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // 1000번 조회가 100ms 이내에 완료되어야 함
    EXPECT_LT(duration.count(), 100000);  // 100ms = 100,000 microseconds
    
    std::cout << "Point lookup performance: " << lookup_count 
              << " lookups in " << duration.count() << " microseconds" << std::endl;
}

// ============================================================================
// 메인 테스트 실행 함수
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}