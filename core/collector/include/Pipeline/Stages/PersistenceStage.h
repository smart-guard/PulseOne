#pragma once

#ifndef PERSISTENCE_STAGE_H
#define PERSISTENCE_STAGE_H

#include "Pipeline/IPipelineStage.h"
#include "Pipeline/IPersistenceQueue.h"
#include <memory>

// Forward declarations
namespace PulseOne::Storage {
    class RedisDataWriter;
}
namespace PulseOne::Client {
    class InfluxClient;
}

namespace PulseOne::Pipeline::Stages {

class PersistenceStage : public IPipelineStage {
public:
    PersistenceStage(std::shared_ptr<Storage::RedisDataWriter> redis_writer,
                    std::shared_ptr<IPersistenceQueue> persistence_queue);
    virtual ~PersistenceStage() = default;

    bool Process(PipelineContext& context) override;
    std::string GetName() const override { return "PersistenceStage"; }

private:
   std::shared_ptr<Storage::RedisDataWriter> redis_writer_;
   std::shared_ptr<IPersistenceQueue> persistence_queue_;
   
   void SaveToRedis(PipelineContext& context);
   void QueueForRDB(PipelineContext& context);
   void BufferForInflux(PipelineContext& context);
};

} // namespace PulseOne::Pipeline::Stages

#endif // PERSISTENCE_STAGE_H
