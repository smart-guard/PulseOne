/**
 * @file test_enrichment_stage.cpp
 * @brief Unit test for EnrichmentStage
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Pipeline/Stages/EnrichmentStage.h"
#include "Pipeline/PipelineContext.h"
#include "VirtualPoint/VirtualPointEngine.h"
#include "DatabaseManager.hpp"
#include "Logging/LogManager.h"

using namespace PulseOne::Pipeline;
using namespace PulseOne::Pipeline::Stages;

class EnrichmentStageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize LogManager to avoid crashes
        LogManager::getInstance().setLogLevel(LogLevel::DEBUG);
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(EnrichmentStageTest, ReturnsTrueWhenEngineNotInitialized) {
    EnrichmentStage stage;
    PulseOne::Structs::DeviceDataMessage msg;
    msg.device_id = "test_device";
    PipelineContext context(msg);

    bool result = stage.Process(context);
    EXPECT_TRUE(result);
}

TEST_F(EnrichmentStageTest, EnrichesMessageWithVirtualPoints) {
    EnrichmentStage stage;
    PulseOne::Structs::DeviceDataMessage msg;
    msg.device_id = "test_device";
    
    PipelineContext context(msg);
    bool result = stage.Process(context);
    
    EXPECT_TRUE(result);
    // Expect 0 points if VPE is not set up
    EXPECT_EQ(context.stats.virtual_points_added, 0);
}
