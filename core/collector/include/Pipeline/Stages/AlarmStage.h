#pragma once

#ifndef ALARM_STAGE_H
#define ALARM_STAGE_H

#include "Pipeline/IPipelineStage.h"
#include "Alarm/AlarmTypes.h"
#include <vector>

namespace PulseOne::Pipeline::Stages {

class AlarmStage : public IPipelineStage {
public:
    explicit AlarmStage();
    virtual ~AlarmStage() = default;

    bool Process(PipelineContext& context) override;
    std::string GetName() const override { return "AlarmStage"; }
    
    // Define public getter for events so PersistenceStage can access them?
    // Start with storing them in context if needed downstream.
    // The original code called 'ProcessAlarmEvents' inside the loop.
    // We should probably store generated events in Context for PersistenceStage to save them.

private:
   // Internal helpers
};

} // namespace PulseOne::Pipeline::Stages

#endif // ALARM_STAGE_H
