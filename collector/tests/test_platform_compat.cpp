// collector/tests/test_platform_compat.cpp 생성 필요
#include <gtest/gtest.h>
#include "Platform/PlatformCompat.h"

using namespace PulseOne::Platform;

TEST(PlatformCompatTest, TimeUtils) {
    std::string time_str = TimeUtils::GetCurrentTimeISO8601();
    EXPECT_FALSE(time_str.empty());
    EXPECT_TRUE(time_str.find("T") != std::string::npos);
}

TEST(PlatformCompatTest, FileUtils) {
    std::string path = FileUtils::JoinPath("home", "config");
    EXPECT_FALSE(path.empty());
}

TEST(PlatformCompatTest, ProcessUtils) {
    int pid = ProcessUtils::GetCurrentProcessId();
    EXPECT_GT(pid, 0);
}