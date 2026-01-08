#include "Scripting/ScriptExecutor.h"
#include "Scripting/ScriptLibraryManager.h"
#include "Logging/LogManager.h"
#include <sstream>
#include <cstring>

namespace PulseOne {
namespace Scripting {

ScriptExecutor::ScriptExecutor() {
}

ScriptExecutor::~ScriptExecutor() {
    shutdown();
}

bool ScriptExecutor::initialize() {
    return initJSEngine() && registerSystemFunctions();
}

void ScriptExecutor::shutdown() {
    cleanupJSEngine();
}

bool ScriptExecutor::reset() {
    cleanupJSEngine();
    return initialize();
}

bool ScriptExecutor::initJSEngine() {
#if HAS_QUICKJS
    std::lock_guard<std::recursive_mutex> lock(js_mutex_);
    
    js_runtime_ = JS_NewRuntime();
    if (!js_runtime_) return false;
    
    JS_SetMemoryLimit(js_runtime_, 16 * 1024 * 1024);
    
    js_context_ = JS_NewContext(js_runtime_);
    if (!js_context_) {
        JS_FreeRuntime(js_runtime_);
        js_runtime_ = nullptr;
        return false;
    }
    return true;
#else
    return false;
#endif
}

void ScriptExecutor::cleanupJSEngine() {
#if HAS_QUICKJS
    std::lock_guard<std::recursive_mutex> lock(js_mutex_);
    if (js_context_) {
        JS_FreeContext(js_context_);
        js_context_ = nullptr;
    }
    if (js_runtime_) {
        JS_FreeRuntime(js_runtime_);
        js_runtime_ = nullptr;
    }
#endif
}

bool ScriptExecutor::registerSystemFunctions() {
#if HAS_QUICKJS
    std::lock_guard<std::recursive_mutex> lock(js_mutex_);
    if (!js_context_) return false;

    // Common system functions for both VP and Alarms
    const char* systemFuncs = R"(
function getPointValue(pointId) {
    var id = parseInt(pointId);
    if (isNaN(id)) return null;
    if (typeof point_values !== 'undefined') return point_values[id] !== undefined ? point_values[id] : point_values[pointId];
    return null;
}
function getCurrentValue(pointId) { return getPointValue(pointId); }
)";
    JSValue res = JS_Eval(js_context_, systemFuncs, std::strlen(systemFuncs), "<system>", JS_EVAL_TYPE_GLOBAL);
    JS_FreeValue(js_context_, res);
    return true;
#else
    return false;
#endif
}

PulseOne::Structs::DataValue ScriptExecutor::evaluate(const ScriptContext& ctx) {
#if HAS_QUICKJS
    std::lock_guard<std::recursive_mutex> lock(js_mutex_);
    if (!js_context_) throw std::runtime_error("JS context not initialized");

    std::string processed_script = preprocessFormula(ctx.script, ctx.tenant_id);

    // Context setup
    JSValue global_obj = JS_GetGlobalObject(js_context_);
    JSValue point_values = JS_NewObject(js_context_);
    if (ctx.inputs) {
        for (auto& [key, value] : ctx.inputs->items()) {
            JSValue js_val;
            if (value.is_number()) js_val = JS_NewFloat64(js_context_, value.get<double>());
            else if (value.is_boolean()) js_val = JS_NewBool(js_context_, value.get<bool>());
            else if (value.is_string()) js_val = JS_NewString(js_context_, value.get<std::string>().c_str());
            else continue;
            
            JS_SetPropertyStr(js_context_, global_obj, key.c_str(), JS_DupValue(js_context_, js_val));
            JS_SetPropertyStr(js_context_, point_values, key.c_str(), JS_DupValue(js_context_, js_val));
            JS_FreeValue(js_context_, js_val);
        }
    }
    JS_SetPropertyStr(js_context_, global_obj, "point_values", point_values);
    JS_FreeValue(js_context_, global_obj);

    // Bytecode check
    JSValue script_func;
    auto cached = getCachedBytecode(processed_script);
    if (cached.first != nullptr) {
        script_func = JS_ReadObject(js_context_, static_cast<const uint8_t*>(cached.first), cached.second, JS_READ_OBJ_BYTECODE);
    } else {
        script_func = JS_Eval(js_context_, processed_script.c_str(), processed_script.length(), 
                              "<script>", JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
        if (!JS_IsException(script_func)) {
            size_t bc_len;
            uint8_t* bc = JS_WriteObject(js_context_, &bc_len, script_func, JS_WRITE_OBJ_BYTECODE);
            if (bc) {
                cacheBytecode(processed_script, bc, bc_len);
                js_free(js_context_, bc);
            }
        }
    }

    if (JS_IsException(script_func)) {
        JSValue exception = JS_GetException(js_context_);
        const char* msg = JS_ToCString(js_context_, exception);
        std::string err = msg ? msg : "Unknown compile error";
        if (msg) JS_FreeCString(js_context_, msg);
        JS_FreeValue(js_context_, exception);
        JS_FreeValue(js_context_, script_func);
        throw std::runtime_error("JS Compile error: " + err);
    }

    JSValue eval_result = JS_EvalFunction(js_context_, script_func);
    if (JS_IsException(eval_result)) {
        JSValue exception = JS_GetException(js_context_);
        const char* msg = JS_ToCString(js_context_, exception);
        std::string err = msg ? msg : "Unknown execution error";
        if (msg) JS_FreeCString(js_context_, msg);
        JS_FreeValue(js_context_, exception);
        JS_FreeValue(js_context_, eval_result);
        throw std::runtime_error("JS Execution error: " + err);
    }

    PulseOne::Structs::DataValue result = 0.0;
    if (JS_IsBool(eval_result)) result = static_cast<bool>(JS_ToBool(js_context_, eval_result));
    else if (JS_IsNumber(eval_result)) {
        double val;
        JS_ToFloat64(js_context_, &val, eval_result);
        result = val;
    } else if (JS_IsString(eval_result)) {
        const char* str = JS_ToCString(js_context_, eval_result);
        result = std::string(str);
        JS_FreeCString(js_context_, str);
    }

    JS_FreeValue(js_context_, eval_result);
    return result;
#else
    return 0.0;
#endif
}

PulseOne::Structs::DataValue ScriptExecutor::evaluateRaw(const std::string& script, const nlohmann::json& inputs) {
    ScriptContext ctx;
    ctx.script = script;
    ctx.inputs = &inputs;
    return evaluate(ctx);
}

ScriptExecutionResult ScriptExecutor::executeSafe(const ScriptContext& context) {
    ScriptExecutionResult res;
    auto start = std::chrono::steady_clock::now();
    try {
        res.value = evaluate(context);
        res.success = true;
    } catch (const std::exception& e) {
        res.success = false;
        res.error_message = e.what();
    }
    auto end = std::chrono::steady_clock::now();
    res.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return res;
}

bool ScriptExecutor::cacheBytecode(const std::string& script_key, void* bytecode, size_t len) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    std::vector<uint8_t> data(static_cast<uint8_t*>(bytecode), static_cast<uint8_t*>(bytecode) + len);
    bytecode_cache_[script_key] = {std::move(data), len};
    return true;
}

std::pair<void*, size_t> ScriptExecutor::getCachedBytecode(const std::string& script_key) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    auto it = bytecode_cache_.find(script_key);
    if (it != bytecode_cache_.end()) {
        return {static_cast<void*>(const_cast<uint8_t*>(it->second.first.data())), it->second.second};
    }
    return {nullptr, 0};
}

std::string ScriptExecutor::preprocessFormula(const std::string& script, int tenant_id) {
    try {
        auto& script_mgr = ScriptLibraryManager::getInstance();
        auto dependencies = script_mgr.collectDependencies(script);
        if (dependencies.empty()) return script;
        
        std::stringstream ss;
        for (const auto& func : dependencies) {
            auto s_opt = script_mgr.getScript(func, tenant_id);
            if (s_opt) ss << s_opt->script_code << "\n\n";
        }
        ss << script;
        return ss.str();
    } catch (...) {
        return script;
    }
}

} // namespace Scripting
} // namespace PulseOne
