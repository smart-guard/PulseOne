// =============================================================================
// collector/include/VirtualPoint/VirtualPointEngine.h
// PulseOne 가상포인트 엔진 - 리팩토링 버전 (Facade 패턴)
// =============================================================================

#ifndef VIRTUAL_POINT_ENGINE_H
#define VIRTUAL_POINT_ENGINE_H

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

#include "Common/BasicTypes.h"
#include "Common/Structs.h"
#include "Scripting/ScriptExecutor.h"
#include "VirtualPoint/VirtualPointRegistry.h"
#include "VirtualPoint/VirtualPointTypes.h"

namespace PulseOne {
namespace VirtualPoint {

/**
 * @brief VirtualPointEngine 클래스 (싱글톤 - Facade 패턴)
 * 가상 포인트의 조회, 계산, 의존성 관리를 총괄하는 최상위 인터페이스입니다.
 */
class VirtualPointEngine {
public:
  static VirtualPointEngine &getInstance();

  // 복사/이동 방지
  VirtualPointEngine(const VirtualPointEngine &) = delete;
  VirtualPointEngine &operator=(const VirtualPointEngine &) = delete;

  // =======================================================================
  // 생명주기 관리
  // =======================================================================
  bool isInitialized() const {
    return initialization_success_.load(std::memory_order_acquire);
  }

  bool initialize();
  void shutdown();
  void reinitialize();

  // =======================================================================
  // 가상포인트 관리 (Delegated to VirtualPointRegistry)
  // =======================================================================
  bool loadVirtualPoints(int tenant_id = 0) {
    return registry_.loadVirtualPoints(tenant_id);
  }

  std::optional<VirtualPointDef> getVirtualPoint(int vp_id) const {
    return registry_.getVirtualPoint(vp_id);
  }

  bool reloadVirtualPoint(int vp_id) {
    return registry_.reloadVirtualPoint(vp_id);
  }

  // =======================================================================
  // 계산 엔진 (Orchestration)
  // =======================================================================
  std::vector<PulseOne::Structs::TimestampedValue>
  calculateForMessage(const PulseOne::Structs::DeviceDataMessage &msg);

  CalculationResult calculate(int vp_id, const nlohmann::json &input_values);
  CalculationResult calculateWithFormula(const std::string &formula,
                                         const nlohmann::json &input_values);

  // =======================================================================
  // 스크립트 실행 (Delegated to ScriptExecutor)
  // =======================================================================
  ExecutionResult executeScript(const CalculationContext &context) {
    PulseOne::Scripting::ScriptContext s_ctx;
    s_ctx.id = context.virtual_point_id;
    s_ctx.tenant_id = context.tenant_id;
    s_ctx.script = context.formula;
    s_ctx.inputs = &context.input_values;

    auto res = executor_.executeSafe(s_ctx);
    ExecutionResult e_res;
    e_res.status =
        res.success ? ExecutionStatus::SUCCESS : ExecutionStatus::RUNTIME_ERROR;
    e_res.result = dataValueToJson(res.value);
    return e_res;
  }

  // =======================================================================
  // 의존성 관리 (Delegated to VirtualPointRegistry)
  // =======================================================================
  std::vector<int> getAffectedVirtualPoints(
      const PulseOne::Structs::DeviceDataMessage &msg) const {
    return registry_.getAffectedVirtualPoints(msg);
  }

  std::vector<int> getDependentVirtualPoints(int point_id) const {
    return registry_.getDependentVirtualPoints(point_id);
  }

  bool hasDependency(int vp_id, int point_id) const {
    return registry_.hasDependency(vp_id, point_id);
  }

  // =======================================================================
  // 통계 및 상태
  // =======================================================================
  VirtualPointStatistics getStatistics() const;
  nlohmann::json getStatisticsJson() const;

private:
  VirtualPointEngine();
  ~VirtualPointEngine();

  void ensureInitialized();
  bool doInitialize();

  static std::once_flag init_flag_;
  static std::atomic<bool> initialization_success_;

  // Helper Functions
  PulseOne::Structs::DataValue jsonToDataValue(const nlohmann::json &j);
  nlohmann::json dataValueToJson(const PulseOne::Structs::DataValue &dv) {
    return std::visit(
        [](const auto &v) -> nlohmann::json { return nlohmann::json(v); }, dv);
  }

  // Orchestration Helpers
  void updateVirtualPointStats(int vp_id, const CalculationResult &result);
  void triggerAlarmEvaluation(int vp_id,
                              const PulseOne::Structs::DataValue &value,
                              int tenant_id);
  nlohmann::json
  collectInputValues(const VirtualPointDef &vp,
                     const PulseOne::Structs::DeviceDataMessage &msg);
  std::optional<double>
  getPointValue(const std::string &point_id,
                const PulseOne::Structs::DeviceDataMessage &msg);

  // Components
  VirtualPointRegistry registry_;
  PulseOne::Scripting::ScriptExecutor executor_;

  // Statistics
  VirtualPointStatistics statistics_;
  mutable std::mutex stats_mutex_;
};

} // namespace VirtualPoint
} // namespace PulseOne

#endif // VIRTUAL_POINT_ENGINE_H