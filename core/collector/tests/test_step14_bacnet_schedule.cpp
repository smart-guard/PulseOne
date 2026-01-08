/**
 * @file test_step14_bacnet_schedule.cpp
 * @brief Step 14: BACnet Schedule 객체 (Weekly_Schedule) 지원 검증 테스트
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Drivers/Bacnet/BACnetDriver.h"
#include "Common/Structs.h"
#include <nlohmann/json.hpp>

using namespace PulseOne::Drivers;
using namespace PulseOne::Structs;
using namespace testing;

class BACnetScheduleTest : public Test {
protected:
    std::shared_ptr<BACnetDriver> driver;

    void SetUp() override {
        driver = std::make_shared<BACnetDriver>();
        DriverConfig config;
        driver->Initialize(config);
        driver->SetConnectedForTesting(true); // 시뮬레이션 연결 상태로 설정
    }
    
    void TearDown() override {
        if (driver) {
            driver->Disconnect();
        }
    }
};

TEST_F(BACnetScheduleTest, ReadWeeklySchedule) {
    // 1. DataPoint 설정 (Weekly_Schedule 프로퍼티)
    DataPoint point;
    point.id = "sch-001";
    point.address = 1; 
    point.address_string = "1";
    point.protocol_params["property_id"] = "123"; // PROP_WEEKLY_SCHEDULE (123)
    
    // 2. 읽기 수행
    std::vector<TimestampedValue> values;
    bool success = driver->ReadValues({point}, values);
    
    EXPECT_TRUE(success);
    ASSERT_EQ(values.size(), 1);
    
    // 3. 값 검증 (JSON 문자열)
    std::string json_str;
    if (std::holds_alternative<std::string>(values[0].value)) {
        json_str = std::get<std::string>(values[0].value);
    } else {
        FAIL() << "Value is not a string";
    }
    
    EXPECT_THAT(json_str, HasSubstr("monday"));
    EXPECT_THAT(json_str, HasSubstr("events"));
    
    // JSON 파싱 확인
    try {
        auto j = nlohmann::json::parse(json_str);
        EXPECT_EQ(j["day"], "monday");
    } catch (...) {
        FAIL() << "Returned value is not valid JSON";
    }
}

TEST_F(BACnetScheduleTest, WriteWeeklySchedule) {
    // 1. DataPoint 설정
    DataPoint point;
    point.id = "sch-001";
    point.address = 1; 
    point.protocol_params["property_id"] = "123";
    
    // 2. 쓸 값 생성 (JSON)
    nlohmann::json new_schedule = {
        {"day", "tuesday"},
        {"events", nlohmann::json::array()}
    };
    DataValue val = new_schedule.dump();
    
    // 3. 쓰기 수행
    bool success = driver->WriteValue(point, val);
    EXPECT_TRUE(success);
}

TEST_F(BACnetScheduleTest, ReadNormalProperty) {
    // 기본 프로퍼티 (Temperature) 읽기 확인
    DataPoint point;
    point.id = "temp-001";
    point.address = 2;
    // property_id 없음 -> 기본값 Present_Value(85)
    
    std::vector<TimestampedValue> values;
    bool success = driver->ReadValues({point}, values);
    
    EXPECT_TRUE(success);
    ASSERT_EQ(values.size(), 1);
    
    // 시뮬레이션 값 23.5 확인
    if (std::holds_alternative<double>(values[0].value)) {
        EXPECT_DOUBLE_EQ(std::get<double>(values[0].value), 23.5);
    } else {
        FAIL() << "Value is not a double";
    }
}
