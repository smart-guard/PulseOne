// =============================================================================
// collector/include/VirtualPoint/VirtualPointEngine.h
// PulseOne 가상포인트 엔진 - VirtualPointTypes 적용 및 싱글톤 패턴 수정
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

// ✅ PulseOne 공통 헤더들
#include "Common/BasicTypes.h"
#include "Common/Structs.h"
#include "VirtualPoint/VirtualPointTypes.h"  // 🔥 VirtualPointTypes 추가

// QuickJS 헤더
extern "C" {
    #include <quickjs/quickjs.h>
}

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
// ✅ VirtualPointTypes.h의 구조체 사용 (중복 제거)
// =============================================================================

/**
 * @brief 가상포인트 정의 (기존 VirtualPointDef 확장)
 */
struct VirtualPointDef {
    int id = 0;
    int tenant_id = 0;
    std::string name;
    std::string description;
    std::string formula;
    
    // ✅ VirtualPointTypes의 enum 활용
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
    
    // ✅ VirtualPointTypes의 변환 함수 활용
    std::string getStateString() const {
        return virtualPointStateToString(state);
    }
    
    std::string getExecutionTypeString() const {
        return executionTypeToString(execution_type);
    }
};

/**
 * @brief VirtualPointEngine 클래스 (싱글톤) - 자동 초기화 적용
 */
class VirtualPointEngine {
public:
    // =======================================================================
    // ✅ 자동 초기화 싱글톤 패턴 (DatabaseManager와 동일)
    // =======================================================================
    static VirtualPointEngine& getInstance();
    
    // 복사/이동 방지
    VirtualPointEngine(const VirtualPointEngine&) = delete;
    VirtualPointEngine& operator=(const VirtualPointEngine&) = delete;
    VirtualPointEngine(VirtualPointEngine&&) = delete;
    VirtualPointEngine& operator=(VirtualPointEngine&&) = delete;

    // =======================================================================
    // 생명주기 관리 (자동 초기화)
    // =======================================================================
    bool isInitialized() const { 
        return initialization_success_.load(std::memory_order_acquire); 
    }
    
    // ✅ 기존 호환성을 위한 initialize (자동 초기화로 대체됨)
    bool initialize() { 
        ensureInitialized(); 
        return isInitialized(); 
    }
    
    void shutdown();
    
    // ✅ 강제 재초기화 (DatabaseManager와 동일)
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
    
    // ✅ VirtualPointTypes의 CalculationResult 사용
    CalculationResult calculate(int vp_id, const json& input_values);
    CalculationResult calculateWithFormula(const std::string& formula, const json& input_values);
    
    // ✅ VirtualPointTypes의 ExecutionResult 사용
    ExecutionResult executeScript(const CalculationContext& context);
    
    // =======================================================================
    // 의존성 관리
    // =======================================================================
    std::vector<int> getAffectedVirtualPoints(const DeviceDataMessage& msg) const;
    std::vector<int> getDependentVirtualPoints(int point_id) const;
    bool hasDependency(int vp_id, int point_id) const;
    
    // =======================================================================
    // 스크립트 관리 (ScriptLibraryManager 연동)
    // =======================================================================
    bool registerCustomFunction(const std::string& name, const std::string& script);
    bool unregisterCustomFunction(const std::string& name);
    
    // ✅ VirtualPointTypes의 ScriptMetadata 활용
    std::vector<ScriptMetadata> getAvailableScripts(int tenant_id = 0) const;
    
    // =======================================================================
    // 통계 및 상태 (VirtualPointTypes 활용)
    // =======================================================================
    VirtualPointStatistics getStatistics() const;
    json getStatisticsJson() const;

private:
    // =======================================================================
    // ✅ 자동 초기화 싱글톤 (DatabaseManager 패턴)
    // =======================================================================
    VirtualPointEngine();
    ~VirtualPointEngine();
    
    // 자동 초기화 메서드
    void ensureInitialized();
    bool doInitialize();  // 실제 초기화 로직
    
    // 자동 초기화 정적 변수들
    static std::once_flag init_flag_;
    static std::atomic<bool> initialization_success_;

    // =======================================================================
    // JavaScript 엔진 관리
    // =======================================================================
    bool initJSEngine();
    void cleanupJSEngine();
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

private:
    // =======================================================================
    // JavaScript 엔진
    // =======================================================================
    JSRuntime* js_runtime_{nullptr};
    JSContext* js_context_{nullptr};
    mutable std::mutex js_mutex_;
    
    // =======================================================================
    // 가상포인트 캐시
    // =======================================================================
    std::unordered_map<int, VirtualPointDef> virtual_points_;
    mutable std::shared_mutex vp_mutex_;
    
    // =======================================================================
    // 의존성 그래프
    // =======================================================================
    std::unordered_map<int, std::unordered_set<int>> point_to_vp_map_;
    std::unordered_map<int, std::unordered_set<int>> vp_dependencies_;
    mutable std::shared_mutex dep_mutex_;
    
    // =======================================================================
    // ✅ VirtualPointTypes의 통계 구조체 활용
    // =======================================================================
    VirtualPointStatistics statistics_;
    mutable std::mutex stats_mutex_;
    
    // =======================================================================
    // 런타임 상태
    // =======================================================================
    int current_vp_id_{0};
    int tenant_id_{0};
};

} // namespace VirtualPoint
} // namespace PulseOne

#endif // VIRTUAL_POINT_ENGINE_H