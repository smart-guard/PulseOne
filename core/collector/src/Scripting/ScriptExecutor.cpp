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
    JS_SetMaxStackSize(js_runtime_, 1024 * 1024); // 1MB stack
    
    js_context_ = JS_NewContext(js_runtime_);
    if (!js_context_) {
        JS_FreeRuntime(js_runtime_);
        js_runtime_ = nullptr;
        return false;
    }

    // Verify context works (in current thread)
    JS_UpdateStackTop(js_runtime_);
    JSValue val = JS_Eval(js_context_, "1+1", 3, "<init>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(val)) {
        JS_FreeValue(js_context_, val);
        cleanupJSEngine();
        return false;
    }
    JS_FreeValue(js_context_, val);
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
    JS_UpdateStackTop(js_runtime_);

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
    if (!js_runtime_) throw std::runtime_error("JS runtime not initialized");
    JS_UpdateStackTop(js_runtime_);

    // Create a fresh context for each evaluation to avoid persistent thread/stack issues
    JSContext* temp_ctx = JS_NewContext(js_runtime_);
    if (!temp_ctx) throw std::runtime_error("Failed to create temporary JS context");

    try {
        std::string processed_script = preprocessFormula(ctx.script, ctx.tenant_id);

        // Register system functions in this context
        const char* systemFuncs = R"(
function getPointValue(pointId) {
    var id = parseInt(pointId);
    if (isNaN(id)) return null;
    if (typeof point_values !== 'undefined') return point_values[id] !== undefined ? point_values[id] : point_values[pointId];
    return null;
}
function getCurrentValue(pointId) { return getPointValue(pointId); }
)";
        JSValue sys_res = JS_Eval(temp_ctx, systemFuncs, std::strlen(systemFuncs), "<system>", JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(temp_ctx, sys_res);

        // Context setup
        JSValue global_obj = JS_GetGlobalObject(temp_ctx);
        JSValue point_values = JS_NewObject(temp_ctx);
        
        std::stringstream debug_inputs;
        if (ctx.inputs) {
            for (auto& [key, value] : ctx.inputs->items()) {
                JSValue js_val;
                if (value.is_number()) {
                    js_val = JS_NewFloat64(temp_ctx, value.get<double>());
                    debug_inputs << key << "=" << value.get<double>() << " ";
                }
                else if (value.is_boolean()) {
                    js_val = JS_NewBool(temp_ctx, value.get<bool>());
                    debug_inputs << key << "=" << (value.get<bool>() ? "true" : "false") << " ";
                }
                else if (value.is_string()) {
                    js_val = JS_NewString(temp_ctx, value.get<std::string>().c_str());
                    debug_inputs << key << "=\"" << value.get<std::string>() << "\" ";
                }
                else continue;
                
                JS_SetPropertyStr(temp_ctx, global_obj, key.c_str(), JS_DupValue(temp_ctx, js_val));
                JS_SetPropertyStr(temp_ctx, point_values, key.c_str(), JS_DupValue(temp_ctx, js_val));
                JS_FreeValue(temp_ctx, js_val);
            }
        }
        JS_SetPropertyStr(temp_ctx, global_obj, "point_values", point_values);
        JS_FreeValue(temp_ctx, global_obj);

        // Execution
        JSValue eval_result = JS_Eval(temp_ctx, processed_script.c_str(), processed_script.length(), 
                                    "<script>", JS_EVAL_TYPE_GLOBAL);

        if (JS_IsException(eval_result)) {
            JSValue exception = JS_GetException(temp_ctx);
            std::string err = "Unknown error";
            
            // Try get message property first
            JSValue msg_val = JS_GetPropertyStr(temp_ctx, exception, "message");
            if (!JS_IsUndefined(msg_val) && !JS_IsNull(msg_val)) {
                const char* msg_ptr = JS_ToCString(temp_ctx, msg_val);
                if (msg_ptr) {
                    err = msg_ptr;
                    JS_FreeCString(temp_ctx, msg_ptr);
                }
                JS_FreeValue(temp_ctx, msg_val);
            } else {
                const char* msg_ptr = JS_ToCString(temp_ctx, exception);
                if (msg_ptr) {
                    err = msg_ptr;
                    JS_FreeCString(temp_ctx, msg_ptr);
                }
            }

            // Try get stack property
            JSValue stack_val = JS_GetPropertyStr(temp_ctx, exception, "stack");
            if (!JS_IsUndefined(stack_val) && !JS_IsNull(stack_val)) {
                const char* stack_ptr = JS_ToCString(temp_ctx, stack_val);
                if (stack_ptr) {
                    err += "\nStack: " + std::string(stack_ptr);
                    JS_FreeCString(temp_ctx, stack_ptr);
                }
                JS_FreeValue(temp_ctx, stack_val);
            }

            // Log details on failure
            LogManager::getInstance().log("ScriptExecutor", Enums::LogLevel::LOG_ERROR, 
                "VP " + std::to_string(ctx.id) + " Execution FAILED: " + err + 
                " | Script: " + processed_script + " | Inputs: " + debug_inputs.str());

            JS_FreeValue(temp_ctx, exception);
            JS_FreeValue(temp_ctx, eval_result);
            JS_FreeContext(temp_ctx);
            throw std::runtime_error("JS Execution error: " + err);
        }

        PulseOne::Structs::DataValue result = 0.0;
        if (JS_IsBool(eval_result)) {
            result = static_cast<bool>(JS_ToBool(temp_ctx, eval_result));
        } else if (JS_IsNumber(eval_result)) {
            double val;
            JS_ToFloat64(temp_ctx, &val, eval_result);
            result = val;
        } else if (JS_IsString(eval_result)) {
            const char* str = JS_ToCString(temp_ctx, eval_result);
            if (str) {
                result = std::string(str);
                JS_FreeCString(temp_ctx, str);
            }
        }

        JS_FreeValue(temp_ctx, eval_result);
        JS_FreeContext(temp_ctx);
        return result;

    } catch (...) {
        JS_FreeContext(temp_ctx);
        throw;
    }
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
