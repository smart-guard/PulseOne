// =============================================================================
// collector/src/VirtualPoint/VirtualPointEngine.cpp
// PulseOne 가상포인트 엔진 - 리팩토링 버전 (Facade 패턴)
// =============================================================================

#include "VirtualPoint/VirtualPointEngine.h"
#include "Logging/LogManager.h"
#include <sstream>

namespace PulseOne {
namespace VirtualPoint {

std::once_flag VirtualPointEngine::init_flag_;
std::atomic<bool> VirtualPointEngine::initialization_success_(false);

VirtualPointEngine::VirtualPointEngine() {
    statistics_.total_points = 0;
    statistics_.active_points = 0;
    statistics_.error_points = 0;
}

VirtualPointEngine::~VirtualPointEngine() {
    shutdown();
}

VirtualPointEngine& VirtualPointEngine::getInstance() {
    static VirtualPointEngine instance;
    instance.ensureInitialized();
    return instance;
}

void VirtualPointEngine::ensureInitialized() {
    if (initialization_success_.load(std::memory_order_acquire)) return;
    std::call_once(init_flag_, [this] {
        initialization_success_.store(doInitialize(), std::memory_order_release);
    });
}

bool VirtualPointEngine::initialize() {
    ensureInitialized();
    return isInitialized();
}

bool VirtualPointEngine::doInitialize() {
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO, "VirtualPointEngine 초기화 시작...");
    if (!executor_.initialize()) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::LOG_ERROR, "ScriptExecutor 초기화 실패");
        return false;
    }
    if (!registry_.loadVirtualPoints(1)) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN, "가상포인트 로드 실패 - 빈 상태로 시작");
    }
    return true;
}

void VirtualPointEngine::shutdown() {
    if (!initialization_success_.load()) return;
    executor_.shutdown();
    registry_.clear();
    initialization_success_.store(false, std::memory_order_release);
}

void VirtualPointEngine::reinitialize() {
    shutdown();
    ensureInitialized();
}

std::vector<PulseOne::Structs::TimestampedValue> VirtualPointEngine::calculateForMessage(const PulseOne::Structs::DeviceDataMessage& msg) {
    std::vector<PulseOne::Structs::TimestampedValue> results;
    if (!isInitialized()) return results;

    auto affected_vps = registry_.getAffectedVirtualPoints(msg);
    for (int vp_id : affected_vps) {
        auto vp_opt = registry_.getVirtualPoint(vp_id);
        if (!vp_opt || !vp_opt->is_enabled) continue;

        nlohmann::json inputs = collectInputValues(*vp_opt, msg);
        auto calc_result = calculate(vp_id, inputs);

        if (calc_result.success) {
            PulseOne::Structs::TimestampedValue tv;
            tv.value = jsonToDataValue(calc_result.value);
            tv.timestamp = std::chrono::system_clock::now();
            tv.quality = PulseOne::Enums::DataQuality::GOOD;
            tv.is_virtual_point = true;
            tv.force_rdb_store = true;
            tv.point_id = vp_id;
            tv.source = "VirtualPointEngine";
            results.push_back(tv);
            
            triggerAlarmEvaluation(vp_id, tv.value);
        }
    }
    return results;
}

CalculationResult VirtualPointEngine::calculate(int vp_id, const nlohmann::json& inputs) {
    CalculationResult result;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    auto vp_opt = registry_.getVirtualPoint(vp_id);
    if (!vp_opt) {
        result.error_message = "VP " + std::to_string(vp_id) + " not found";
        return result;
    }

    try {
        PulseOne::Scripting::ScriptContext ctx;
        ctx.id = vp_id;
        ctx.tenant_id = vp_opt->tenant_id;
        ctx.script = vp_opt->formula;
        ctx.inputs = &inputs;

        result.value = dataValueToJson(executor_.evaluate(ctx));
        result.success = true;
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        updateVirtualPointStats(vp_id, result);
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::LOG_ERROR, 
            "VP " + std::to_string(vp_id) + " Calculation Error: " + std::string(e.what()));
        result.success = false;
        result.error_message = e.what();
    }
    return result;
}

CalculationResult VirtualPointEngine::calculateWithFormula(const std::string& formula, const nlohmann::json& input_values) {
    CalculationResult result;
    try {
        result.value = dataValueToJson(executor_.evaluateRaw(formula, input_values));
        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
    }
    return result;
}

VirtualPointStatistics VirtualPointEngine::getStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

nlohmann::json VirtualPointEngine::getStatisticsJson() const {
    auto stats = getStatistics();
    nlohmann::json j;
    j["total_calculations"] = stats.total_calculations;
    j["successful_calculations"] = stats.successful_calculations;
    j["failed_calculations"] = stats.failed_calculations;
    return j;
}

PulseOne::Structs::DataValue VirtualPointEngine::jsonToDataValue(const nlohmann::json& j) {
    if (j.is_null()) return std::string("null");
    if (j.is_boolean()) return j.get<bool>();
    if (j.is_number_integer()) return j.get<int>();
    if (j.is_number_float()) return j.get<double>();
    if (j.is_string()) return j.get<std::string>();
    return j.dump();
}

nlohmann::json VirtualPointEngine::collectInputValues(const VirtualPointDef& vp, const PulseOne::Structs::DeviceDataMessage& msg) {
    nlohmann::json inputs;
    if (!vp.input_points.contains("inputs") || !vp.input_points["inputs"].is_array()) return inputs;
    
    for (const auto& input_def : vp.input_points["inputs"]) {
        std::string var_name = input_def["variable"].get<std::string>();
        int point_id = input_def["point_id"].get<int>();
        auto value_opt = getPointValue(std::to_string(point_id), msg);
        inputs[var_name] = value_opt ? *value_opt : 0.0;
    }
    return inputs;
}

std::optional<double> VirtualPointEngine::getPointValue(const std::string& point_id, const PulseOne::Structs::DeviceDataMessage& msg) {
    int id = std::stoi(point_id);
    for (const auto& dp : msg.points) {
        if (dp.point_id == id) {
            if (std::holds_alternative<double>(dp.value)) return std::get<double>(dp.value);
            if (std::holds_alternative<int>(dp.value)) return static_cast<double>(std::get<int>(dp.value));
        }
    }
    return std::nullopt;
}

void VirtualPointEngine::updateVirtualPointStats(int vp_id, const CalculationResult& result) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    statistics_.total_calculations++;
    if (result.success) statistics_.successful_calculations++;
    else statistics_.failed_calculations++;
}

void VirtualPointEngine::triggerAlarmEvaluation(int vp_id, const PulseOne::Structs::DataValue& value) {
    // Basic trigger log
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::DEBUG, "Alarm trigger for VP " + std::to_string(vp_id));
}

} // namespace VirtualPoint
} // namespace PulseOne