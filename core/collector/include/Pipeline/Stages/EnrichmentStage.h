#pragma once

#ifndef ENRICHMENT_STAGE_H
#define ENRICHMENT_STAGE_H

#include "Pipeline/IPipelineStage.h"
#include <vector>

namespace PulseOne::Pipeline::Stages {

class EnrichmentStage : public IPipelineStage {
public:
    explicit EnrichmentStage();
    virtual ~EnrichmentStage() = default;

    bool Process(PipelineContext& context) override;
    std::string GetName() const override { return "EnrichmentStage"; }

private:
   // Internal counters or helpers if needed
};

} // namespace PulseOne::Pipeline::Stages

#endif // ENRICHMENT_STAGE_H
