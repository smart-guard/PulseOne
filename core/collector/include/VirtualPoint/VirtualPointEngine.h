// =============================================================================
// collector/include/VirtualPoint/VirtualPointEngine.h
// PulseOne 가상포인트 엔진 - 컴파일 에러 완전 수정 버전
// =============================================================================

#ifndef VIRTUAL_POINT_ENGINE_H
#define VIRTUAL_POINT_ENGINE_H

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

// PulseOne 공통 헤더들
#include "Common/BasicTypes.h"
#include "Common/Structs.h"
#include "VirtualPoint/VirtualPointTypes.h"
#include "Database/Entities/VirtualPointEntity.h"

// QuickJS 헤더
#if HAS_QUICKJS
#ifdef _WIN32
    extern "C" {
        #include <quickjs.h>
    }
#else
    extern "C" {
        #include <quickjs/quickjs.h>
    }
#endif
#endif

// JSON 라이브러리
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace VirtualPoint {

// =============================================================================
// 타입 별칭 및 전방 선언
// =============================================================================
using json = nlohmann::json;
using DataValue = PulseOne::Structs::DataValue;
using DeviceDataMessage = PulseOne::Structs::DeviceDataMessage;
using TimestampedValue = PulseOne::Structs::TimestampedValue;
using DataQuality = PulseOne::Enums::DataQuality;

// 전방 선언
class ScriptLibraryManager;

// =============================================================================
// 가상포인트 정의
// =============================================================================

/**
 * @brief 가상포인트 정의
 */
struct VirtualPointDef {
    int id = 0;
    int tenant_id = 0;
    std::string name;
    std::string description;
    std::string formula;
    
    // VirtualPointTypes의 enum 사용
    VirtualPointState state = VirtualPointState::INACTIVE;
    ExecutionType execution_type = ExecutionType::JAVASCRIPT;
    ErrorHandling error_handling = ErrorHandling::RETURN_NULL;
    CalculationTrigger trigger = CalculationTrigger::ON_CHANGE;
    
    // 기본 필드들
    std::string data_type = "float";
    std::string unit;
    std::chrono::milliseconds calculation_interval_ms{1000};
    json input_points;
    json input_mappings;
    
    std::string script_id;
    std::chrono::milliseconds update_interval{1000};
    bool is_enabled = true;
    std::chrono::system_clock::time_point last_calculation;
    DataValue last_value;
    
    // VirtualPointTypes의 변환 함수 활용
    std::string getStateString() const {
        return virtualPointStateToString(state);
    }
    
    std::string getExecutionTypeString() const {
        return executionTypeToString(execution_type);
    }
    
    std::string getErrorHandlingString() const {
        return errorHandlingToString(error_handling);
    }
};

/**
 * @brief VirtualPointEngine 클래스 (싱글톤)
 */
class VirtualPointEngine {
public:
    // =======================================================================
    // 싱글톤 패턴
    // =======================================================================
    static VirtualPointEngine& getInstance();
    
    // 복사/이동 방지
    VirtualPointEngine(const VirtualPointEngine&) = delete;
    VirtualPointEngine& operator=(const VirtualPointEngine&) = delete;
    VirtualPointEngine(VirtualPointEngine&&) = delete;
    VirtualPointEngine& operator=(VirtualPointEngine&&) = delete;

    // =======================================================================
    // 생명주기 관리
    // =======================================================================
    bool isInitialized() const { 
        return initialization_success_.load(std::memory_order_acquire); 
    }
    
    bool initialize() { 
        ensureInitialized(); 
        return isInitialized(); 
    }
    
    void shutdown();
    void reinitialize();

    // =======================================================================
    // 가상포인트 관리
    // =======================================================================
    bool loadVirtualPoints(int tenant_id = 0);
    std::optional<VirtualPointDef> getVirtualPoint(int vp_id) const;
    bool reloadVirtualPoint(int vp_id);
    
    // =======================================================================
    // 계산 엔진 (메인 인터페이스)
    // =======================================================================
    std::vector<TimestampedValue> calculateForMessage(const DeviceDataMessage& msg);
    
    // VirtualPointTypes의 CalculationResult 사용
    CalculationResult calculate(int vp_id, const json& input_values);
    CalculationResult calculateWithFormula(const std::string& formula, const json& input_values);
    
    // VirtualPointTypes의 ExecutionResult 사용
    ExecutionResult executeScript(const CalculationContext& context);
    
    // =======================================================================
    // 의존성 관리
    // =======================================================================
    std::vector<int> getAffectedVirtualPoints(const DeviceDataMessage& msg) const;
    std::vector<int> getDependentVirtualPoints(int point_id) const;
    bool hasDependency(int vp_id, int point_id) const;
    
    // =======================================================================
    // 스크립트 관리
    // =======================================================================
    bool registerCustomFunction(const std::string& name, const std::string& script);
    bool unregisterCustomFunction(const std::string& name);
    
    // VirtualPointTypes의 ScriptMetadata 활용
    std::vector<ScriptMetadata> getAvailableScripts(int tenant_id = 0) const;
    
    // =======================================================================
    // 통계 및 상태
    // =======================================================================
    VirtualPointStatistics getStatistics() const;
    json getStatisticsJson() const;

private:
    // =======================================================================
    // 싱글톤 생성자/소멸자
    // =======================================================================
    VirtualPointEngine();
    ~VirtualPointEngine();
    
    // 초기화 메서드
    void ensureInitialized();
    bool doInitialize();
    
    // 정적 변수들
    static std::once_flag init_flag_;
    static std::atomic<bool> initialization_success_;

    // =======================================================================
    // ✅ 수정된 Entity → VirtualPoint 타입 변환 헬퍼들
    // =======================================================================
    ExecutionType convertEntityExecutionType(const std::string& entity_type_str);
    ErrorHandling convertEntityErrorHandling(const std::string& entity_handling_str);

    // =======================================================================
    // JSON ↔ DataValue 변환 헬퍼들
    // =======================================================================
    DataValue jsonToDataValue(const nlohmann::json& j) {
        try {
            if (j.is_null()) return std::string("null");
            if (j.is_boolean()) return j.get<bool>();
            if (j.is_number_integer()) return j.get<int>();
            if (j.is_number_unsigned()) return j.get<uint32_t>();
            if (j.is_number_float()) return j.get<double>();
            if (j.is_string()) return j.get<std::string>();
            return j.dump();  // 복잡한 객체는 JSON 문자열로
        } catch (...) {
            return std::string("conversion_error");
        }
    }
    
    nlohmann::json dataValueToJson(const DataValue& dv) {
        return std::visit([](const auto& v) -> nlohmann::json {
            return nlohmann::json(v);
        }, dv);
    }

    // =======================================================================
    // JavaScript 엔진 관리
    // =======================================================================
    bool initJSEngine();
    void cleanupJSEngine();
    bool resetJSEngine();
    bool registerSystemFunctions();
    
    // =======================================================================
    // 계산 헬퍼
    // =======================================================================
    DataValue evaluateFormula(const std::string& formula, const json& input_values);
    json collectInputValues(const VirtualPointDef& vp, const DeviceDataMessage& msg);
    std::optional<double> getPointValue(const std::string& point_name, const DeviceDataMessage& msg);
    
    // =======================================================================
    // 전처리 및 후처리
    // =======================================================================
    std::string preprocessFormula(const std::string& formula, int tenant_id);
    void updateVirtualPointStats(int vp_id, const CalculationResult& result);
    void triggerAlarmEvaluation(int vp_id, const DataValue& value);

    // =======================================================================
    // 멤버 변수들
    // =======================================================================
    
    // JavaScript 엔진
#if HAS_QUICKJS
    JSRuntime* js_runtime_{nullptr};
    JSContext* js_context_{nullptr};
#else
    void* js_runtime_{nullptr};
    void* js_context_{nullptr};
#endif
    mutable std::recursive_mutex js_mutex_;
    
    // 가상포인트 캐시
    std::unordered_map<int, VirtualPointDef> virtual_points_;
    mutable std::shared_mutex vp_mutex_;
    
    // 의존성 그래프
    std::unordered_map<int, std::unordered_set<int>> point_to_vp_map_;
    std::unordered_map<int, std::unordered_set<int>> vp_dependencies_;
    mutable std::shared_mutex dep_mutex_;

    // VirtualPointTypes의 통계 구조체 활용
    VirtualPointStatistics statistics_;
    mutable std::mutex stats_mutex_;
    
    // 런타임 상태
    int tenant_id_{0};

    mutable std::mutex current_vp_mutex_;
    int current_vp_id_ = 0;
    
    // 안전한 접근 메서드들:
    void setCurrentVpId(int vp_id) {
        std::lock_guard<std::mutex> lock(current_vp_mutex_);
        current_vp_id_ = vp_id;
    }
    
    int getCurrentVpId() const {
        std::lock_guard<std::mutex> lock(current_vp_mutex_);
        return current_vp_id_;
    }
};

} // namespace VirtualPoint
} // namespace PulseOne

#endif // VIRTUAL_POINT_ENGINE_H