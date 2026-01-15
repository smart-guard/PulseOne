#ifndef SCRIPT_EXECUTOR_H
#define SCRIPT_EXECUTOR_H

#include <string>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <nlohmann/json.hpp>

// QuickJS forward declarations or headers
#if HAS_QUICKJS
extern "C" {
    #include <quickjs.h>
}
#endif

#include "Common/Structs.h"

namespace PulseOne {
namespace Scripting {

/**
 * @brief 스크립트 실행 결과 구조체
 */
struct ScriptExecutionResult {
    bool success = false;
    PulseOne::Structs::DataValue value;
    std::string error_message;
    std::chrono::microseconds execution_time{0};
};

/**
 * @brief 스크립트 실행 컨텍스트
 */
struct ScriptContext {
    int id = 0;              // VP ID 또는 Rule ID
    int tenant_id = 0;
    std::string script;      // 실행할 스크립트 또는 수식
    const nlohmann::json* inputs = nullptr; // 주입할 변수들
    
    // QuickJS 관련 바이트코드 (캐싱용)
    void* bytecode = nullptr;
    size_t bytecode_len = 0;
};

class ScriptExecutor {
public:
    ScriptExecutor();
    ~ScriptExecutor();

    bool initialize();
    void shutdown();
    bool reset();

    // Core Execution
    PulseOne::Structs::DataValue evaluate(const ScriptContext& ctx);
    PulseOne::Structs::DataValue evaluateRaw(const std::string& script, const nlohmann::json& inputs);
    ScriptExecutionResult executeSafe(const ScriptContext& context);

    // Bytecode Caching
    bool cacheBytecode(const std::string& script_key, void* bytecode, size_t len);
    std::pair<void*, size_t> getCachedBytecode(const std::string& script_key) const;

private:
    bool initJSEngine();
    void cleanupJSEngine();
    bool registerSystemFunctions();
    std::string preprocessFormula(const std::string& script, int tenant_id);

#if HAS_QUICKJS
    JSRuntime* js_runtime_{nullptr};
    JSContext* js_context_{nullptr};
#else
    void* js_runtime_{nullptr};
    void* js_context_{nullptr};
#endif
    mutable std::recursive_mutex js_mutex_;

    // Bytecode Cache
    std::unordered_map<std::string, std::pair<std::vector<uint8_t>, size_t>> bytecode_cache_;
    mutable std::shared_mutex cache_mutex_;
};

} // namespace Scripting
} // namespace PulseOne

#endif // SCRIPT_EXECUTOR_H
