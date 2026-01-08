#pragma once

#ifndef I_PERSISTENCE_QUEUE_H
#define I_PERSISTENCE_QUEUE_H

#include "Common/Structs.h"
#include <vector>

namespace PulseOne::Pipeline {

struct PersistenceTask; // Forward declare the task struct

/**
 * @brief Interface for enqueueing persistence tasks.
 */
class IPersistenceQueue {
public:
    virtual ~IPersistenceQueue() = default;
    
    // We need to define PersistenceTask here or include it.
    // Ideally it should be moved to a common header if shared.
    // For now, let's keep it abstract or use the types directly?
    // Let's assume PersistenceTask is now public or we use concrete method params.
    
    // Simplified interface for what PersistenceStage needs
    virtual void QueueRDBTask(const Structs::DeviceDataMessage& message, 
                             const std::vector<Structs::TimestampedValue>& points) = 0;
                             
    virtual void QueueInfluxTask(const Structs::DeviceDataMessage& message, 
                                const std::vector<Structs::TimestampedValue>& points) = 0;
                                
    virtual void QueueCommStatsTask(const Structs::DeviceDataMessage& message) = 0;
};

} // namespace PulseOne::Pipeline

#endif // I_PERSISTENCE_QUEUE_H
