#include "Pipeline/Stages/EnrichmentStage.h"
#include "VirtualPoint/VirtualPointEngine.h"
#include "Logging/LogManager.h"
#include "Pipeline/PipelineContext.h"
#include "VirtualPoint/VirtualPointBatchWriter.h"

namespace PulseOne::Pipeline::Stages {

EnrichmentStage::EnrichmentStage() {
    // Constructor
}

bool EnrichmentStage::Process(PipelineContext& context) {
    try {
        auto& vp_engine = VirtualPoint::VirtualPointEngine::getInstance();
        
        if (!vp_engine.isInitialized()) {
             // Try initialize if not ready? Or just skip.
             // Original service had skip logic.
             return true; 
        }

        auto vp_results = vp_engine.calculateForMessage(context.message);
        
        if (vp_results.empty()) {
            return true;
        }

        // Enrich the message
        for (const auto& vp : vp_results) {
            context.enriched_message.points.push_back(vp);
        }
        
        context.stats.virtual_points_added = vp_results.size();
        
        // Asynchronous Queueing for Virtual Point Batch Writer (persistence/logging side effect)
        // Original code did this in CalculateVirtualPoints.
        // Ideally this side effect belongs in PersistenceStage, but we need to queue raw VPs specifically?
        // Let's keep it here if VPBatchWriter is minimal, OR move to Persistence.
        // Implementation Plan said PersistenceStage handles persistence.
        // But VPBatchWriter is specifically for *Virtual Points* optimization. 
        // We will delegate actual persistent storage to Persist Stage, 
        // but VPBatchWriter also does internal Redis caching of VPs?
        // Let's stick to simple calculation here.
        
        LogManager::getInstance().log("EnrichmentStage", Enums::LogLevel::DEBUG_LEVEL, 
            "Enriched message with " + std::to_string(vp_results.size()) + " virtual points.");

    } catch (const std::exception& e) {
        context.error_message = "Enrichment Error: " + std::string(e.what());
        LogManager::getInstance().Error(context.error_message);
        // Continue processing even if enrichment fails? Yes, original code did.
    }
    return true;
}

} // namespace PulseOne::Pipeline::Stages
