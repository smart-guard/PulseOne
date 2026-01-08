#pragma once

#ifndef I_PIPELINE_STAGE_H
#define I_PIPELINE_STAGE_H

#include "Pipeline/PipelineContext.h"
#include <string>

namespace PulseOne::Pipeline {

/**
 * @brief Interface for a single stage in the data processing pipeline.
 */
class IPipelineStage {
public:
    virtual ~IPipelineStage() = default;

    /**
     * @brief Process the context.
     * @return true if processing should continue to the next stage, false to stop.
     */
    virtual bool Process(PipelineContext& context) = 0;

    /**
     * @brief Get the name of the stage for logging/debugging.
     */
    virtual std::string GetName() const = 0;
};

} // namespace PulseOne::Pipeline

#endif // I_PIPELINE_STAGE_H
