/**
 * @file test_persistence_stage.cpp
 * @brief Unit test for PersistenceStage using GMock
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Pipeline/Stages/PersistenceStage.h"
#include "Pipeline/PipelineContext.h"
#include "Storage/RedisDataWriter.h"
#include "Client/RedisClient.h"
#include "Pipeline/IPersistenceQueue.h"
#include "Logging/LogManager.h"

using namespace PulseOne::Pipeline;
using namespace PulseOne::Pipeline::Stages;
using namespace PulseOne::Storage;
using namespace testing;

// Mock RedisClient
class MockRedisClient : public RedisClient {
public:
    MOCK_METHOD(bool, connect, (const std::string&, int, const std::string&), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(bool, isConnected, (), (const, override));
    
    MOCK_METHOD(bool, set, (const std::string&, const std::string&), (override));
    MOCK_METHOD(bool, setex, (const std::string&, const std::string&, int), (override));
    MOCK_METHOD(std::string, get, (const std::string&), (override));
    MOCK_METHOD(int, del, (const std::string&), (override));
    MOCK_METHOD(bool, exists, (const std::string&), (override));
    MOCK_METHOD(bool, expire, (const std::string&, int), (override));
    MOCK_METHOD(int, ttl, (const std::string&), (override));
    MOCK_METHOD(StringList, keys, (const std::string&), (override));
    MOCK_METHOD(int, incr, (const std::string&, int), (override));
    
    // Hash
    MOCK_METHOD(bool, hset, (const std::string&, const std::string&, const std::string&), (override));
    MOCK_METHOD(std::string, hget, (const std::string&, const std::string&), (override));
    MOCK_METHOD(StringMap, hgetall, (const std::string&), (override));
    MOCK_METHOD(int, hdel, (const std::string&, const std::string&), (override));
    MOCK_METHOD(bool, hexists, (const std::string&, const std::string&), (override));
    MOCK_METHOD(int, hlen, (const std::string&), (override));
    
    // List - Adding missing overrides
    MOCK_METHOD(int, lpush, (const std::string&, const std::string&), (override));
    MOCK_METHOD(int, rpush, (const std::string&, const std::string&), (override));
    MOCK_METHOD(std::string, lpop, (const std::string&), (override));
    MOCK_METHOD(std::string, rpop, (const std::string&), (override));
    MOCK_METHOD(StringList, lrange, (const std::string&, int, int), (override));
    MOCK_METHOD(int, llen, (const std::string&), (override));

    // Set
    MOCK_METHOD(int, sadd, (const std::string&, const std::string&), (override));
    MOCK_METHOD(int, srem, (const std::string&, const std::string&), (override));
    MOCK_METHOD(bool, sismember, (const std::string&, const std::string&), (override));
    MOCK_METHOD(StringList, smembers, (const std::string&), (override));
    MOCK_METHOD(int, scard, (const std::string&), (override));

    // Sorted Set
    MOCK_METHOD(int, zadd, (const std::string&, double, const std::string&), (override));
    MOCK_METHOD(int, zrem, (const std::string&, const std::string&), (override));
    MOCK_METHOD(StringList, zrange, (const std::string&, int, int), (override));
    MOCK_METHOD(int, zcard, (const std::string&), (override));

    // Pub/Sub
    MOCK_METHOD(bool, subscribe, (const std::string&), (override));
    MOCK_METHOD(bool, unsubscribe, (const std::string&), (override));
    MOCK_METHOD(int, publish, (const std::string&, const std::string&), (override));
    MOCK_METHOD(bool, psubscribe, (const std::string&), (override));
    MOCK_METHOD(bool, punsubscribe, (const std::string&), (override));
    MOCK_METHOD(void, setMessageCallback, (MessageCallback), (override));
    MOCK_METHOD(bool, waitForMessage, (int), (override));

    // Batch
    MOCK_METHOD(bool, mset, (const StringMap&), (override));
    MOCK_METHOD(StringList, mget, (const StringList&), (override));

    // Transaction
    MOCK_METHOD(bool, multi, (), (override));
    MOCK_METHOD(bool, exec, (), (override));
    MOCK_METHOD(bool, discard, (), (override));

    // Info
    MOCK_METHOD(StringMap, info, (), (override));
    MOCK_METHOD(bool, ping, (), (override));
    MOCK_METHOD(bool, select, (int), (override));
    MOCK_METHOD(int, dbsize, (), (override));
};

// Mock PersistenceQueue
class MockPersistenceQueue : public IPersistenceQueue {
public:
    MOCK_METHOD(void, QueueRDBTask, (const PulseOne::Structs::DeviceDataMessage&, const std::vector<PulseOne::Structs::TimestampedValue>&), (override));
    MOCK_METHOD(void, QueueInfluxTask, (const PulseOne::Structs::DeviceDataMessage&, const std::vector<PulseOne::Structs::TimestampedValue>&), (override));
    MOCK_METHOD(void, QueueCommStatsTask, (const PulseOne::Structs::DeviceDataMessage&), (override));
};

class PersistenceStageTest : public ::testing::Test {
protected:
    std::shared_ptr<MockRedisClient> mock_redis;
    std::shared_ptr<RedisDataWriter> redis_writer;
    std::shared_ptr<MockPersistenceQueue> mock_queue;

    void SetUp() override {
        LogManager::getInstance().setLogLevel(LogLevel::DEBUG);
        mock_redis = std::make_shared<MockRedisClient>();
        // Inject mock redis into writer
        redis_writer = std::make_shared<RedisDataWriter>(mock_redis);
        
        mock_queue = std::make_shared<MockPersistenceQueue>();
    }
};

TEST_F(PersistenceStageTest, SkipsWhenPersistenceDisabled) {
    PersistenceStage stage(redis_writer, mock_queue);
    PulseOne::Structs::DeviceDataMessage msg;
    PipelineContext context(msg);
    context.should_persist = false;

    bool result = stage.Process(context);
    EXPECT_TRUE(result);
    // Should verify no calls to redis or queue
}

TEST_F(PersistenceStageTest, PersistsToRedisAndQueue) {
    PersistenceStage stage(redis_writer, mock_queue);
    
    PulseOne::Structs::DeviceDataMessage msg;
    msg.device_id = "device_test";
    msg.tenant_id = 0;
    
    PulseOne::Structs::TimestampedValue val;
    val.point_id = 999;
    val.source = "test_point";
    val.value = 100.0;
    msg.points.push_back(val);
    
    PipelineContext context(msg);
    context.should_persist = true;

    // Default expectation for checks
    EXPECT_CALL(*mock_redis, isConnected()).WillRepeatedly(Return(true));

    // Expect calls
    // We expect at least some Redis calls. 
    EXPECT_CALL(*mock_redis, multi()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_redis, exec()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_redis, hset(_, _, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_redis, set(_, _)).WillRepeatedly(Return(true));
    // It might call other things. 
    
    // Check Queue calls
    EXPECT_CALL(*mock_queue, QueueRDBTask(_, _)).Times(1);
    EXPECT_CALL(*mock_queue, QueueInfluxTask(_, _)).Times(1);
    EXPECT_CALL(*mock_queue, QueueCommStatsTask(_)).Times(1);

    bool result = stage.Process(context);
    EXPECT_TRUE(result);
    EXPECT_TRUE(context.stats.persisted_to_redis);
    EXPECT_TRUE(context.stats.persisted_to_rdb);
}

TEST_F(PersistenceStageTest, PersistsAlarmsToRedis) {
    PersistenceStage stage(redis_writer, mock_queue);
    
    PulseOne::Structs::DeviceDataMessage msg;
    PipelineContext context(msg);
    context.should_persist = true;
    
    // Add an alarm
    PulseOne::Alarm::AlarmEvent alarm;
    alarm.occurrence_id = 12345;
    alarm.rule_id = 10;
    alarm.message = "Test Alarm";
    context.alarm_events.push_back(alarm);

    // Default expectation for checks
    EXPECT_CALL(*mock_redis, isConnected()).WillRepeatedly(Return(true));

    // Expect Redis Publish
    // RedisDataWriter::PublishAlarmEvent calls redis_client->publish
    EXPECT_CALL(*mock_redis, publish(_, _)).Times(AtLeast(1));
    
    // Also Queue calls (even if message empty)
    EXPECT_CALL(*mock_queue, QueueRDBTask(_, _)).Times(1);
    EXPECT_CALL(*mock_queue, QueueInfluxTask(_, _)).Times(1);
    EXPECT_CALL(*mock_queue, QueueCommStatsTask(_)).Times(1);

    bool result = stage.Process(context);
    EXPECT_TRUE(result);
}
