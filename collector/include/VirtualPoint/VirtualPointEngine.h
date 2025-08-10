// =============================================================================
// collector/include/VirtualPoint/VirtualPointEngine.h
// PulseOne 가상포인트 엔진 헤더 - 완성본
// =============================================================================

#ifndef VIRTUAL_POINT_ENGINE_H
#define VIRTUAL_POINT_ENGINE_H

#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <optional>
#include <chrono>
#include <string>
#include <quickjs.h>
#include <nlohmann/json.hpp>

#include "Common/Structs.h"
#include "Database/DatabaseManager.h"
#include "Utils/LogManager.h"

namespace PulseOne {
namespace VirtualPoint {

using json = nlohmann::json;
using TimestampedValue = Structs::TimestampedValue;
using DeviceDataMessage = Structs::DeviceDataMessage;
using DataValue = Structs::DataValue;

// =============================================================================
// 가상포인트 정의
// =============================================================================
struct VirtualPointDef {
    int id = 0;
    int tenant_id = 0;
    std::string name;
    std::string description;
    std::string formula;  // JavaScript 수식
    std::string data_type = "float";
    std::string unit;
    int calculation_interval_ms = 1000;
    std::string execution_type = "javascript";
    std::string error_handling = "return_null";
    std::vector<int> dependencies;  // 의존 포인트 ID들
    json input_mappings;  // 입력 변수 매핑
    bool is_enabled = true;
    
    // 런타임 상태
    double last_value = 0.0;
    std::string last_error;
    int execution_count = 0;
    double avg_execution_time_ms = 0.0;
};

// =============================================================================
// 계산 결과
// =============================================================================
struct CalculationResult {
    bool success = false;
    DataValue value;
    std::string error_message;
    std::chrono::milliseconds execution_time{0};
    json input_snapshot;  // 계산에 사용된 입력값들
};

// =============================================================================
// VirtualPointEngine 클래스
// =============================================================================
class VirtualPointEngine {
public:
    // 싱글톤 패턴
    static VirtualPointEngine& getInstance();
    
    // 초기화/종료
    bool initialize(std::shared_ptr<Database::DatabaseManager> db_manager);
    void shutdown();
    bool isInitialized() const { return initialized_; }
    
    // 가상포인트 관리
    bool loadVirtualPoints(int tenant_id);
    bool reloadVirtualPoint(int vp_id);
    std::optional<VirtualPointDef> getVirtualPoint(int vp_id) const;
    
    // Pipeline에서 호출 - 메인 인터페이스
    std::vector<TimestampedValue> calculateForMessage(const DeviceDataMessage& msg);
    
    // 개별 계산
    CalculationResult calculate(int vp_id, const json& inputs = {});
    CalculationResult calculateWithFormula(const std::string& formula, const json& inputs);
    
    // JavaScript 함수 관리
    bool registerSystemFunctions();
    bool registerCustomFunction(const std::string& name, const std::string& code);
    bool unregisterCustomFunction(const std::string& name);
    
    // 의존성 관리
    std::vector<int> getAffectedVirtualPoints(const DeviceDataMessage& msg) const;
    std::vector<int> getDependentVirtualPoints(int point_id) const;
    bool hasDependency(int vp_id, int point_id) const;
    
    // 통계
    json getStatistics() const;
    
private:
    VirtualPointEngine();
    ~VirtualPointEngine();
    VirtualPointEngine(const VirtualPointEngine&) = delete;
    VirtualPointEngine& operator=(const VirtualPointEngine&) = delete;
    
    // JavaScript 엔진 관리
    bool initJSEngine();
    void cleanupJSEngine();
    JSValue evaluateJS(const std::string& code);
    
    // 수식 평가
    DataValue evaluateFormula(const std::string& formula, const json& inputs);
    
    // 입력값 수집
    json collectInputValues(const VirtualPointDef& vp, const DeviceDataMessage& msg);
    std::optional<double> getPointValue(const std::string& point_id, const DeviceDataMessage& msg);
    
    // 데이터베이스 작업
    bool loadVirtualPointFromDB(int vp_id);
    bool loadDependenciesFromDB(int vp_id);
    bool saveExecutionHistory(int vp_id, const CalculationResult& result);
    
    // 에러 처리
    DataValue handleError(const VirtualPointDef& vp, const std::string& error);
    
private:
    // JavaScript 엔진
    JSRuntime* js_runtime_ = nullptr;
    JSContext* js_context_ = nullptr;
    mutable std::mutex js_mutex_;
    
    // 가상포인트 저장소
    std::map<int, VirtualPointDef> virtual_points_;
    mutable std::shared_mutex vp_mutex_;
    
    // 의존성 맵
    std::map<int, std::vector<int>> point_to_vp_map_;  // data_point_id -> [vp_ids]
    std::map<int, std::vector<int>> vp_dependencies_;  // vp_id -> [depends_on]
    mutable std::shared_mutex dep_mutex_;
    
    // 커스텀 함수
    std::map<std::string, std::string> custom_functions_;
    
    // 통계
    std::atomic<uint64_t> total_calculations_{0};
    std::atomic<uint64_t> failed_calculations_{0};
    
    // 연결
    std::shared_ptr<Database::DatabaseManager> db_manager_;
    Utils::LogManager& logger_;
    
    // 상태
    std::atomic<bool> initialized_{false};
};

} // namespace VirtualPoint
} // namespace PulseOne

#endif // VIRTUAL_POINT_ENGINE_H