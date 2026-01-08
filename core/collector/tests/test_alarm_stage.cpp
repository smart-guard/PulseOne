/**
 * @file test_alarm_stage.cpp
 * @brief Unit test for AlarmStage
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Pipeline/Stages/AlarmStage.h"
#include "Pipeline/PipelineContext.h"
#include "Alarm/AlarmManager.h"
#include "Logging/LogManager.h"

using namespace PulseOne::Pipeline;
using namespace PulseOne::Pipeline::Stages;

class AlarmStageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize LogManager
        LogManager::getInstance().setLogLevel(LogLevel::DEBUG);
    }

    void TearDown() override {
    }
};

TEST_F(AlarmStageTest, SkipsWhenEvaluationDisabled) {
    AlarmStage stage;
    PulseOne::Structs::DeviceDataMessage msg;
    PipelineContext context(msg);
    context.should_evaluate_alarms = false;

    bool result = stage.Process(context);
    EXPECT_TRUE(result);
    EXPECT_EQ(context.stats.alarms_triggered, 0);
}

TEST_F(AlarmStageTest, ReturnsTrueWhenManagerNotInitialized) {
    AlarmStage stage;
    PulseOne::Structs::DeviceDataMessage msg;
    PipelineContext context(msg);
    context.should_evaluate_alarms = true;

    // AlarmManager singleton might be uninitialized here
    bool result = stage.Process(context);
    EXPECT_TRUE(result);
}
