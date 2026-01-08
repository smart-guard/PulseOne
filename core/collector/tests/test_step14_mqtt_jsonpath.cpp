/**
 * @file test_step14_mqtt_jsonpath.cpp
 * @brief Step 14: MQTT JSONPath 매핑 기능 검증 테스트
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Workers/Protocol/MQTTWorker.h"
#include "Pipeline/PipelineManager.h"
#include "Logging/LogManager.h"
#include <thread>
#include <chrono>

using namespace PulseOne::Workers;
using namespace PulseOne::Structs;
using namespace testing;

class TestableMQTTWorker : public MQTTWorker {
public:
    explicit TestableMQTTWorker(const DeviceInfo& info) : MQTTWorker(info) {}
    
    // 접근자 노출
    using BaseDeviceWorker::AddDataPoint;
    using BaseDeviceWorker::previous_values_;
    using BaseDeviceWorker::data_points_;
    using MQTTWorker::SendJsonValuesToPipeline;
    
    // 이 테스트에서는 PipelineManager 의존성 없이 previous_values_에 저장된 값만 확인합니다.
};

class MQTTJsonPathTest : public Test {
protected:
    void SetUp() override {
        LogManager::getInstance().setLogLevel(PulseOne::Enums::LogLevel::DEBUG_LEVEL);
    }
};

TEST_F(MQTTJsonPathTest, ExtractNestedValue) {
    DeviceInfo info;
    info.id = "mqtt-test-01";
    info.name = "Test MQTT Device";
    info.protocol_type = "MQTT";
    
    TestableMQTTWorker worker(info);
    
    // 1. DataPoint 설정 (JSONPath 매핑) - numeric ID ("1") 사용
    DataPoint point;
    printf("[DEBUG TEST] sizeof(DataPoint) in test = %zu\n", sizeof(DataPoint));
    point.id = "1";
    point.address_string = "test/sensor";
    point.mapping_key = "sensors[0].temp";
    worker.AddDataPoint(point);
    
    // Internal state verification
    ASSERT_EQ(worker.data_points_.size(), 1);
    EXPECT_EQ(worker.data_points_[0].mapping_key, "sensors[0].temp");
    printf("[DEBUG TEST] Internal check: mapping_key in data_points_[0] = '%s'\n", 
           worker.data_points_[0].mapping_key.c_str());
    
    // 2. JSON 메시지 생성
    nlohmann::json payload = {
        {"sensors", nlohmann::json::array({
            {{"temp", 25.5}, {"humi", 60.0}}
        })}
    };
    
    // 3. 실행 (Pipeline Manager가 없어도 previous_values_는 채워져야 함)
    worker.SendJsonValuesToPipeline(payload, "test/sensor", 0);
    
    // 4. 추출된 값 검증 (previous_values_ 확인)
    ASSERT_TRUE(worker.previous_values_.count(1));
    EXPECT_DOUBLE_EQ(std::get<double>(worker.previous_values_[1]), 25.5);
}

TEST_F(MQTTJsonPathTest, ExtractSimpleDotNotation) {
    DeviceInfo info;
    info.id = "mqtt-test-02";
    
    TestableMQTTWorker worker(info);
    
    DataPoint point;
    point.id = "2";
    point.address_string = "test/simple";
    point.mapping_key = "data.value";
    worker.AddDataPoint(point);
    
    nlohmann::json payload = {
        {"data", {
            {"value", 100}
        }}
    };
    
    worker.SendJsonValuesToPipeline(payload, "test/simple", 0);
    
    ASSERT_TRUE(worker.previous_values_.count(2));
    EXPECT_EQ(std::get<int64_t>(worker.previous_values_[2]), 100);
}

TEST_F(MQTTJsonPathTest, FallbackToAutoDiscoveryIfNoMapping) {
    DeviceInfo info;
    info.id = "mqtt-test-03";
    
    TestableMQTTWorker worker(info);
    
    nlohmann::json payload = {{"temp", 20}};
    
    worker.SendJsonValuesToPipeline(payload, "test/auto", 0);
    
    // 자동 탐색에서는 "test/auto.temp" 해시값이 point_id로 사용됨
    EXPECT_FALSE(worker.previous_values_.empty());
}

// 메인 실행
int main(int argc, char **argv) {
    InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
