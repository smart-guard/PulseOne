// =============================================================================
// collector/include/VirtualPoint/VirtualPointEngine.h - 헤더 누락 수정
// =============================================================================

#ifndef VIRTUAL_POINT_ENGINE_H
#define VIRTUAL_POINT_ENGINE_H

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>  // 🔥 추가
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

// 🔥 올바른 헤더 포함 순서
#include "Common/BasicTypes.h"
#include "Common/Structs.h"
#include "Database/DatabaseManager.h"  // 🔥 Database 네임스페이스 포함
#include "Utils/LogManager.h"  // 🔥 Utils 네임스페이스 포함

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
using DatabaseManager = PulseOne::Database::DatabaseManager;  // 🔥 명시적 별칭

// 전방 선언
class ScriptLibraryManager;

// =============================================================================
// 구조체 정의들
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
    json input_points;
    std::string script_id;
    std::chrono::milliseconds update_interval{1000};
    bool is_enabled = true;
    std::chrono::system_clock::time_point last_calculation;
    DataValue last_value;
};

/**
 * @brief 계산 결과
 */
struct CalculationResult {
    bool success = false;
    DataValue value;
    std::string error_message;
    std::chrono::milliseconds execution_time{0};
    
    CalculationResult() = default;
    CalculationResult(bool s, const DataValue& v) : success(s), value(v) {}
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
    bool initialize(std::shared_ptr<DatabaseManager> db_manager);  // 🔥 타입 수정
    void shutdown();
    bool isInitialized() const { return initialized_; }

    // =======================================================================
    // 가상포인트 관리
    // =======================================================================
    bool loadVirtualPoints(int tenant_id = 0);
    std::optional<VirtualPointDef> getVirtualPoint(int vp_id) const;
    bool reloadVirtualPoint(int vp_id);
    
    // =======================================================================
    // 계산 엔진
    // =======================================================================
    std::vector<TimestampedValue> calculateForMessage(const DeviceDataMessage& msg);
    CalculationResult calculate(int vp_id, const json& input_values);
    CalculationResult calculateWithFormula(const std::string& formula, const json& input_values);
    
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
    
    // =======================================================================
    // 통계 및 상태
    // =======================================================================
    json getStatistics() const;

private:
    // =======================================================================
    // 생성자/소멸자 (private - 싱글톤)
    // =======================================================================
    VirtualPointEngine();
    ~VirtualPointEngine();

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
    // 상태 관리
    // =======================================================================
    std::atomic<bool> initialized_{false};
    
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
    mutable std::shared_mutex vp_mutex_;  // 🔥 타입 수정
    
    // =======================================================================
    // 의존성 그래프
    // =======================================================================
    std::unordered_map<int, std::unordered_set<int>> point_to_vp_map_;
    std::unordered_map<int, std::unordered_set<int>> vp_dependencies_;
    mutable std::shared_mutex dep_mutex_;  // 🔥 타입 수정
    
    // =======================================================================
    // 통계
    // =======================================================================
    std::atomic<int64_t> total_calculations_{0};
    std::atomic<int64_t> successful_calculations_{0};
    std::atomic<int64_t> failed_calculations_{0};
    
    // =======================================================================
    // 의존성들
    // =======================================================================
    std::shared_ptr<DatabaseManager> db_manager_;
    PulseOne::Utils::LogManager& logger_;  // 🔥 타입 수정
    
    // =======================================================================
    // 내부 상태
    // =======================================================================
    int current_vp_id_{0};
    int tenant_id_{0};
};

} // namespace VirtualPoint
} // namespace PulseOne

#endif // VIRTUAL_POINT_ENGINE_H