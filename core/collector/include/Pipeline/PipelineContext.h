#pragma once

#ifndef PIPELINE_CONTEXT_H
#define PIPELINE_CONTEXT_H

#include "Common/Structs.h"
#include "Alarm/AlarmTypes.h"
#include <vector>
#include <memory>
#include <atomic>

namespace PulseOne::Pipeline {

/**
 * @brief Holds the state and data flowing through the processing pipeline.
 */
struct PipelineContext {
    // Input Data
    PulseOne::Structs::DeviceDataMessage message;

    // Enriched Data (Result of EnrichmentStage)
    PulseOne::Structs::DeviceDataMessage enriched_message;

    // Processing Flags
    bool should_persist = true;
    bool should_evaluate_alarms = true;
    
    // Alarms (Result of AlarmStage)
    std::vector<PulseOne::Alarm::AlarmEvent> alarm_events;
    
    // Statistics for this specific message
    struct ContextStats {
        int virtual_points_added = 0;
        int alarms_triggered = 0;
        bool persisted_to_redis = false;
        bool persisted_to_rdb = false;
        bool persisted_to_influx = false;
    } stats;

    // Detailed error info if processing fails
    std::string error_message;

    explicit PipelineContext(const PulseOne::Structs::DeviceDataMessage& msg) 
        : message(msg), enriched_message(msg) {}
};

} // namespace PulseOne::Pipeline

#endif // PIPELINE_CONTEXT_H
