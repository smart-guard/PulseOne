// =============================================================================
// collector/src/VirtualPoint/VirtualPointEngine.cpp
// PulseOne 가상포인트 엔진 - 컴파일 에러 완전 수정 버전
// =============================================================================

#include "VirtualPoint/VirtualPointEngine.h"
#include "VirtualPoint/ScriptLibraryManager.h"
#include "Database/Repositories/VirtualPointRepository.h"
#include "Database/RepositoryFactory.h"
#include "Alarm/AlarmManager.h"
#include "Utils/LogManager.h"
#include "Database/DatabaseManager.h"
#include <sstream>
#include <chrono>
#include <algorithm>
#include <regex>

namespace PulseOne {
namespace VirtualPoint {

// =============================================================================
// 자동 초기화 정적 변수들
// =============================================================================
std::once_flag VirtualPointEngine::init_flag_;
std::atomic<bool> VirtualPointEngine::initialization_success_(false);

// =============================================================================
// 자동 초기화 생성자
// =============================================================================
VirtualPointEngine::VirtualPointEngine() {
    statistics_.total_points = 0;
    statistics_.active_points = 0;
    statistics_.error_points = 0;
}

VirtualPointEngine::~VirtualPointEngine() {
    shutdown();
}

// =============================================================================
// 싱글톤 구현
// =============================================================================
VirtualPointEngine& VirtualPointEngine::getInstance() {
    static VirtualPointEngine instance;
    instance.ensureInitialized();
    return instance;
}

void VirtualPointEngine::ensureInitialized() {
    if (initialization_success_.load(std::memory_order_acquire)) {
        return;
    }
    
    std::call_once(init_flag_, [this] {
        try {
            bool success = doInitialize();
            initialization_success_.store(success, std::memory_order_release);
            
            if (success) {
                LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO, 
                                             "VirtualPointEngine 자동 초기화 성공");
            } else {
                LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                             "VirtualPointEngine 자동 초기화 실패");
            }
        } catch (const std::exception& e) {
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                         "VirtualPointEngine 초기화 예외: " + std::string(e.what()));
            initialization_success_.store(false, std::memory_order_release);
        }
    });
}

// =============================================================================
// 실제 초기화 로직
// =============================================================================
bool VirtualPointEngine::doInitialize() {
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                 "VirtualPointEngine 자동 초기화 시작...");
    
    try {
        auto& db_manager = DatabaseManager::getInstance();
        
        // JavaScript 엔진 초기화
        if (!initJSEngine()) {
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                         "JavaScript 엔진 초기화 실패");
            return false;
        }
        
        // ScriptLibraryManager 초기화
        auto& script_lib = ScriptLibraryManager::getInstance();
        
        // 시스템 함수 등록
        if (!registerSystemFunctions()) {
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                         "시스템 함수 등록 일부 실패");
        }

        if (!loadVirtualPoints(0)) {
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                     "가상포인트 로드 실패 - 빈 엔진으로 시작");
        }
        
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                     "VirtualPointEngine 자동 초기화 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                     "VirtualPointEngine 초기화 실패: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 강제 재초기화
// =============================================================================
void VirtualPointEngine::reinitialize() {
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                 "VirtualPointEngine 강제 재초기화 중...");
    
    shutdown();
    initialization_success_.store(false, std::memory_order_release);
    
    bool success = doInitialize();
    initialization_success_.store(success, std::memory_order_release);
    
    if (success) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                     "VirtualPointEngine 재초기화 성공");
    } else {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                     "VirtualPointEngine 재초기화 실패");
    }
}

void VirtualPointEngine::shutdown() {
    if (!initialization_success_.load()) return;
    
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                 "VirtualPointEngine 종료 중...");
    
    cleanupJSEngine();
    
    {
        std::unique_lock<std::shared_mutex> lock(vp_mutex_);
        virtual_points_.clear();
    }
    
    {
        std::unique_lock<std::shared_mutex> lock(dep_mutex_);
        point_to_vp_map_.clear();
        vp_dependencies_.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_ = VirtualPointStatistics{};
    }
    
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                 "VirtualPointEngine 종료 완료");
}

// =============================================================================
// JavaScript 엔진 관리
// =============================================================================

bool VirtualPointEngine::initJSEngine() {
    std::lock_guard<std::mutex> lock(js_mutex_);
    
    js_runtime_ = JS_NewRuntime();
    if (!js_runtime_) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                     "JavaScript 런타임 생성 실패");
        return false;
    }
    
    JS_SetMemoryLimit(js_runtime_, 16 * 1024 * 1024);
    
    js_context_ = JS_NewContext(js_runtime_);
    if (!js_context_) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                     "JavaScript 컨텍스트 생성 실패");
        JS_FreeRuntime(js_runtime_);
        js_runtime_ = nullptr;
        return false;
    }
    
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                 "JavaScript 엔진 초기화 완료");
    return true;
}

void VirtualPointEngine::cleanupJSEngine() {
    std::lock_guard<std::mutex> lock(js_mutex_);
    
    if (js_context_) {
        JS_FreeContext(js_context_);
        js_context_ = nullptr;
    }
    
    if (js_runtime_) {
        JS_FreeRuntime(js_runtime_);
        js_runtime_ = nullptr;
    }
    
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                 "JavaScript 엔진 정리 완료");
}

bool VirtualPointEngine::registerSystemFunctions() {
    if (!js_context_) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                     "JavaScript 엔진이 초기화되지 않음");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(js_mutex_);
    
    try {
        int success_count = 0;
        
        // getPointValue() 함수 등록
        std::string getPointValueFunc = R"(
function getPointValue(pointId) {
    var id = parseInt(pointId);
    if (isNaN(id)) {
        console.log('[getPointValue] Invalid pointId: ' + pointId);
        return null;
    }
    
    if (typeof point_values !== 'undefined' && point_values[id] !== undefined) {
        return point_values[id];
    }
    
    if (typeof point_values !== 'undefined' && point_values[pointId] !== undefined) {
        return point_values[pointId];
    }
    
    var varName = 'point_' + id;
    try {
        if (typeof eval(varName) !== 'undefined') {
            return eval(varName);
        }
    } catch (e) {
        // eval 실패는 무시
    }
    
    if (typeof globalThis !== 'undefined' && globalThis[varName] !== undefined) {
        return globalThis[varName];
    }
    
    console.log('[getPointValue] Point ' + pointId + ' not found in any scope');
    return null;
}
)";
        
        JSValue func_result = JS_Eval(js_context_, 
                                     getPointValueFunc.c_str(), 
                                     getPointValueFunc.length(), 
                                     "<getPointValue>", 
                                     JS_EVAL_TYPE_GLOBAL);
        
        if (!JS_IsException(func_result)) {
            success_count++;
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                         "getPointValue() 함수 등록 성공");
        } else {
            JSValue exception = JS_GetException(js_context_);
            const char* error_str = JS_ToCString(js_context_, exception);
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                         "getPointValue() 등록 실패: " + 
                                         std::string(error_str ? error_str : "Unknown error"));
            JS_FreeCString(js_context_, error_str);
            JS_FreeValue(js_context_, exception);
        }
        JS_FreeValue(js_context_, func_result);
        
        // getCurrentValue() 함수 등록
        std::string getCurrentValueFunc = R"(
function getCurrentValue(pointId) {
    return getPointValue(pointId);
}
)";
        
        JSValue getCurrentValue_result = JS_Eval(js_context_, 
                                                getCurrentValueFunc.c_str(), 
                                                getCurrentValueFunc.length(), 
                                                "<getCurrentValue>", 
                                                JS_EVAL_TYPE_GLOBAL);
        
        if (!JS_IsException(getCurrentValue_result)) {
            success_count++;
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                         "getCurrentValue() 함수 등록 성공");
        }
        JS_FreeValue(js_context_, getCurrentValue_result);
        
        // 디버깅 함수들 등록
        std::string debugFuncs = R"(
function debugPointValues() {
    console.log('[DEBUG] Available point_values:', typeof point_values !== 'undefined' ? Object.keys(point_values) : 'undefined');
    if (typeof point_values !== 'undefined') {
        for (var key in point_values) {
            console.log('[DEBUG] point_values[' + key + '] = ' + point_values[key]);
        }
    }
}

function logMessage(msg) {
    console.log('[VP] ' + msg);
}

function isPointAvailable(pointId) {
    return getPointValue(pointId) !== null;
}
)";
        
        JSValue debug_result = JS_Eval(js_context_, 
                                      debugFuncs.c_str(), 
                                      debugFuncs.length(), 
                                      "<debug_functions>", 
                                      JS_EVAL_TYPE_GLOBAL);
        
        if (!JS_IsException(debug_result)) {
            success_count++;
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                         "디버깅 함수들 등록 성공");
        }
        JS_FreeValue(js_context_, debug_result);
        
        // Math 함수들 등록
        const std::vector<std::pair<std::string, std::string>> math_functions = {
            {"Math.max", "Math.max"},
            {"Math.min", "Math.min"}, 
            {"Math.abs", "Math.abs"},
            {"Math.sqrt", "Math.sqrt"},
            {"Math.pow", "Math.pow"},
            {"Math.round", "Math.round"},
            {"Math.floor", "Math.floor"},
            {"Math.ceil", "Math.ceil"},
            {"Math.sin", "Math.sin"},
            {"Math.cos", "Math.cos"},
            {"Math.tan", "Math.tan"},
            {"Math.log", "Math.log"},
            {"Math.exp", "Math.exp"}
        };
        
        for (const auto& [name, code] : math_functions) {
            try {
                std::string func_copy = "var " + name.substr(5) + " = " + code + ";";
                
                JSValue result = JS_Eval(js_context_, 
                                       func_copy.c_str(),
                                       func_copy.length(),
                                       "<math_function>",
                                       JS_EVAL_TYPE_GLOBAL);
                
                if (!JS_IsException(result)) {
                    success_count++;
                } else {
                    LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                                 "Math 함수 '" + name + "' 등록 실패");
                }
                JS_FreeValue(js_context_, result);
                
            } catch (const std::exception& e) {
                LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                             "Math 함수 '" + name + "' 등록 예외: " + std::string(e.what()));
            }
        }
        
        // 유틸리티 함수들 등록
        std::string utilityFuncs = R"(
function toNumber(value) {
    if (typeof value === 'number') return value;
    if (typeof value === 'string') {
        var num = parseFloat(value);
        return isNaN(num) ? 0 : num;
    }
    if (typeof value === 'boolean') return value ? 1 : 0;
    return 0;
}

function toString(value) {
    if (value === null || value === undefined) return '';
    return String(value);
}

function toBool(value) {
    if (typeof value === 'boolean') return value;
    if (typeof value === 'number') return value !== 0;
    if (typeof value === 'string') return value !== '' && value !== '0' && value.toLowerCase() !== 'false';
    return false;
}

function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
}

function lerp(a, b, t) {
    return a + (b - a) * t;
}
)";
        
        JSValue utility_result = JS_Eval(js_context_, 
                                        utilityFuncs.c_str(), 
                                        utilityFuncs.length(), 
                                        "<utility_functions>", 
                                        JS_EVAL_TYPE_GLOBAL);
        
        if (!JS_IsException(utility_result)) {
            success_count++;
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                         "유틸리티 함수들 등록 성공");
        }
        JS_FreeValue(js_context_, utility_result);
        
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                     "시스템 함수 등록 완료: " + std::to_string(success_count) + "개 성공");
        
        return success_count >= 1;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                     "시스템 함수 등록 중 예외: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// ✅ 수정된 Entity 타입 변환 메서드들 (string 기반)
// =============================================================================

ExecutionType VirtualPointEngine::convertEntityExecutionType(const std::string& entity_type_str) {
    return stringToExecutionType(entity_type_str);
}

ErrorHandling VirtualPointEngine::convertEntityErrorHandling(const std::string& entity_handling_str) {
    return stringToErrorHandling(entity_handling_str);
}

// =============================================================================
// 가상포인트 관리
// =============================================================================

bool VirtualPointEngine::loadVirtualPoints(int tenant_id) {
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                 "테넌트 " + std::to_string(tenant_id) + "의 가상포인트 로드 중...");
    
    try {
        auto& factory = Database::RepositoryFactory::getInstance();
        auto repo = factory.getVirtualPointRepository();
        auto entities = repo->findByTenant(tenant_id);
        
        std::unique_lock<std::shared_mutex> vp_lock(vp_mutex_);
        std::unique_lock<std::shared_mutex> dep_lock(dep_mutex_);
        
        // 기존 테넌트 데이터 정리
        auto it = virtual_points_.begin();
        while (it != virtual_points_.end()) {
            if (it->second.tenant_id == tenant_id) {
                it = virtual_points_.erase(it);
            } else {
                ++it;
            }
        }
        
        // 새 데이터 로드
        for (const auto& entity : entities) {
            VirtualPointDef vp_def;
            vp_def.id = entity.getId();
            vp_def.tenant_id = entity.getTenantId();
            vp_def.name = entity.getName();
            vp_def.description = entity.getDescription();
            vp_def.formula = entity.getFormula();
            
            // ✅ string → enum 변환 사용
            vp_def.execution_type = convertEntityExecutionType(entity.getExecutionType());
            vp_def.error_handling = convertEntityErrorHandling(entity.getErrorHandling());
            vp_def.state = entity.getIsEnabled() ? VirtualPointState::ACTIVE : VirtualPointState::INACTIVE;
            
            vp_def.calculation_interval_ms = std::chrono::milliseconds(entity.getCalculationInterval());
            vp_def.is_enabled = entity.getIsEnabled();
            
            // ✅ VirtualPointEntity에서 실제로 존재하는 필드들만 사용
            if (!entity.getDependencies().empty()) {
                try {
                    vp_def.input_points = json::parse(entity.getDependencies());
                } catch (const std::exception& e) {
                    LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                                 "가상포인트 " + std::to_string(vp_def.id) + 
                                                 " dependencies 파싱 실패: " + std::string(e.what()));
                }
            }
            
            virtual_points_[vp_def.id] = vp_def;
            
            // 의존성 맵 구축
            if (vp_def.input_points.contains("inputs") &&
                vp_def.input_points["inputs"].is_array()) {
                
                for (const auto& input : vp_def.input_points["inputs"]) {
                    if (input.contains("point_id") && input["point_id"].is_number()) {
                        int point_id = input["point_id"].get<int>();
                        point_to_vp_map_[point_id].insert(vp_def.id);
                        vp_dependencies_[vp_def.id].insert(point_id);
                    }
                }
            }
        }
        
        // 통계 업데이트
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            statistics_.total_points = entities.size();
            statistics_.active_points = std::count_if(entities.begin(), entities.end(),
                [](const auto& e) { return e.getIsEnabled(); });
        }
        
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                     "가상포인트 " + std::to_string(entities.size()) + " 개 로드 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                     "가상포인트 로드 실패: " + std::string(e.what()));
        return false;
    }
}

std::optional<VirtualPointDef> VirtualPointEngine::getVirtualPoint(int vp_id) const {
    std::shared_lock<std::shared_mutex> lock(vp_mutex_);
    
    auto it = virtual_points_.find(vp_id);
    if (it != virtual_points_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

// =============================================================================
// 메인 계산 인터페이스
// =============================================================================

std::vector<TimestampedValue> VirtualPointEngine::calculateForMessage(const DeviceDataMessage& msg) {
    std::vector<TimestampedValue> results;
    
    if (!isInitialized()) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                     "VirtualPointEngine이 초기화되지 않음");
        return results;
    }
    
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
        "메시지 수신: device_id=" + msg.device_id + 
        ", points=" + std::to_string(msg.points.size()) + "개");
    
    for (const auto& point : msg.points) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
            "point_id=" + std::to_string(point.point_id) + 
            ", is_virtual=" + (point.is_virtual_point ? "true" : "false"));
    }    
    
    auto affected_vps = getAffectedVirtualPoints(msg);
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
        "영향받는 가상포인트: " + std::to_string(affected_vps.size()) + "개");

    if (affected_vps.empty()) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
            "이 메시지와 관련된 가상포인트가 없음");
        
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
            "전체 가상포인트 개수: " + std::to_string(virtual_points_.size()));
        
        if (!virtual_points_.empty()) {
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                "가상포인트가 존재하지만 해당 디바이스와 연결되지 않음");
            
            auto first_vp = virtual_points_.begin();
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                "첫 번째 가상포인트: ID=" + std::to_string(first_vp->first) + 
                ", name=" + first_vp->second.name);
        }
        
        return results;
    }
    
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                 "메시지로 인해 " + std::to_string(affected_vps.size()) + " 개 가상포인트 재계산 필요");
    
    // 각 가상포인트 계산
    for (int vp_id : affected_vps) {
        try {
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                "가상포인트 ID " + std::to_string(vp_id) + " 계산 시작");
            
            auto vp_opt = getVirtualPoint(vp_id);
            if (!vp_opt || !vp_opt->is_enabled) {
                LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                    "가상포인트 ID " + std::to_string(vp_id) + " 비활성화되어 건너뜀");
                continue;
            }
            
            // 입력값 수집
            json inputs = collectInputValues(*vp_opt, msg);
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                "입력값 수집 완료: " + inputs.dump());
            
            auto calc_result = calculate(vp_id, inputs);
            
            if (calc_result.success) {
                TimestampedValue tv;
                tv.value = jsonToDataValue(calc_result.value);
                tv.timestamp = std::chrono::system_clock::now();
                tv.quality = DataQuality::GOOD;
                
                // 핵심 수정: is_virtual_point = true 설정
                tv.is_virtual_point = true;
                
                // 가상포인트 관련 필드들 설정
                tv.point_id = vp_id;
                tv.source = "VirtualPointEngine";
                tv.sequence_number = static_cast<uint32_t>(vp_id);
                
                tv.value_changed = true;
                tv.force_rdb_store = true;
                tv.trigger_alarm_check = true;
                
                results.push_back(tv);
                
                LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                    "가상포인트 " + vp_opt->name + " (ID:" + std::to_string(vp_id) + 
                    ") 계산 완료 - 값: " + std::to_string(tv.GetDoubleValue()));
                
                updateVirtualPointStats(vp_id, calc_result);
                triggerAlarmEvaluation(vp_id, jsonToDataValue(calc_result.value));
                
            } else {
                LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                             "가상포인트 " + std::to_string(vp_id) + 
                                             " 계산 실패: " + calc_result.error_message);
                
                std::lock_guard<std::mutex> lock(stats_mutex_);
                statistics_.failed_calculations++;
            }
            
        } catch (const std::exception& e) {
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                         "가상포인트 " + std::to_string(vp_id) + 
                                         " 처리 예외: " + std::string(e.what()));
        }
    }
    
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                 "가상포인트 계산 완료: " + std::to_string(results.size()) + " 개 결과 생성");
    
    return results;
}

// =============================================================================
// 개별 계산
// =============================================================================

CalculationResult VirtualPointEngine::calculate(int vp_id, const json& inputs) {
    CalculationResult result;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    current_vp_id_ = vp_id;
    
    try {
        auto vp_opt = getVirtualPoint(vp_id);
        if (!vp_opt) {
            result.error_message = "가상포인트 ID " + std::to_string(vp_id) + " 찾을 수 없음";
            return result;
        }
        
        const auto& vp = *vp_opt;
        
        if (!vp.is_enabled) {
            result.error_message = "가상포인트 " + vp.name + "이 비활성화됨";
            return result;
        }
        
        tenant_id_ = vp.tenant_id;
        
        // 수식 실행
        result.value = dataValueToJson(evaluateFormula(vp.formula, inputs));
        result.success = true;
        
        // 실행 시간 계산
        auto end_time = std::chrono::high_resolution_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        // 통계 업데이트
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            statistics_.total_calculations++;
            statistics_.successful_calculations++;
            
            auto total_calc = statistics_.total_calculations;
            statistics_.avg_execution_time_ms = 
                (statistics_.avg_execution_time_ms * (total_calc - 1) + result.execution_time.count()) / total_calc;
            
            statistics_.last_calculation = std::chrono::system_clock::now();
        }
        
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::DEBUG,
                                     "가상포인트 " + vp.name + " 계산 완료 (" + 
                                     std::to_string(result.execution_time.count()) + "ms)");
        
    } catch (const std::exception& e) {
        result.error_message = e.what();
        result.success = false;
        
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            statistics_.total_calculations++;
            statistics_.failed_calculations++;
        }
        
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                     "가상포인트 " + std::to_string(vp_id) + 
                                     " 계산 실패: " + result.error_message);
    }
    
    current_vp_id_ = 0;
    tenant_id_ = 0;
    
    return result;
}

// =============================================================================
// 안전한 입력값 수집
// =============================================================================

std::optional<double> VirtualPointEngine::getPointValue(const std::string& point_id, const DeviceDataMessage& msg) {
    try {
        int id = std::stoi(point_id);
        
        for (const auto& data_point : msg.points) {
            if (data_point.point_id == id) {
                if (std::holds_alternative<double>(data_point.value)) {
                    return std::get<double>(data_point.value);
                } else if (std::holds_alternative<int>(data_point.value)) {
                    return static_cast<double>(std::get<int>(data_point.value));
                } else if (std::holds_alternative<float>(data_point.value)) {
                    return static_cast<double>(std::get<float>(data_point.value));
                }
            }
        }
        
    } catch (const std::invalid_argument& e) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                     "Invalid point_id format: " + point_id);
    } catch (const std::out_of_range& e) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                     "Point_id out of range: " + point_id);
    }
    
    return std::nullopt;
}

// =============================================================================
// 핵심 계산 메서드
// =============================================================================

DataValue VirtualPointEngine::evaluateFormula(const std::string& formula, const json& inputs) {
    std::lock_guard<std::mutex> lock(js_mutex_);
    
    if (!js_context_) {
        throw std::runtime_error("JavaScript 엔진이 초기화되지 않음");
    }
    
    std::string processed_formula = preprocessFormula(formula, tenant_id_);
    
    try {
        // point_values 객체 생성
        std::string point_values_setup = "var point_values = {};";
        JSValue setup_result = JS_Eval(js_context_, 
                                      point_values_setup.c_str(), 
                                      point_values_setup.length(), 
                                      "<point_values_setup>", 
                                      JS_EVAL_TYPE_GLOBAL);
        
        if (JS_IsException(setup_result)) {
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                         "point_values 객체 생성 실패");
        }
        JS_FreeValue(js_context_, setup_result);
        
        // 입력 변수들을 JavaScript 전역 객체와 point_values에 동시 설정
        for (auto& [key, value] : inputs.items()) {
            std::string js_code;
            std::string point_values_code;
            
            if (value.is_number()) {
                double num_val = value.get<double>();
                js_code = "var " + key + " = " + std::to_string(num_val) + ";";
                
                if (key.find("point_") == 0) {
                    std::string point_id = key.substr(6);
                    point_values_code = "point_values[" + point_id + "] = " + std::to_string(num_val) + "; " +
                                       "point_values['" + point_id + "'] = " + std::to_string(num_val) + ";";
                } else {
                    point_values_code = "point_values['" + key + "'] = " + std::to_string(num_val) + ";";
                }
                
            } else if (value.is_boolean()) {
                bool bool_val = value.get<bool>();
                js_code = "var " + key + " = " + (bool_val ? "true" : "false") + ";";
                
                if (key.find("point_") == 0) {
                    std::string point_id = key.substr(6);
                    point_values_code = "point_values[" + point_id + "] = " + (bool_val ? "true" : "false") + "; " +
                                       "point_values['" + point_id + "'] = " + (bool_val ? "true" : "false") + ";";
                } else {
                    point_values_code = "point_values['" + key + "'] = " + (bool_val ? "true" : "false") + ";";
                }
                
            } else if (value.is_string()) {
                std::string str_val = value.get<std::string>();
                std::string escaped_str = str_val;
                size_t pos = 0;
                while ((pos = escaped_str.find("'", pos)) != std::string::npos) {
                    escaped_str.replace(pos, 1, "\\'");
                    pos += 2;
                }
                
                js_code = "var " + key + " = '" + escaped_str + "';";
                
                if (key.find("point_") == 0) {
                    std::string point_id = key.substr(6);
                    point_values_code = "point_values[" + point_id + "] = '" + escaped_str + "'; " +
                                       "point_values['" + point_id + "'] = '" + escaped_str + "';";
                } else {
                    point_values_code = "point_values['" + key + "'] = '" + escaped_str + "';";
                }
                
            } else if (value.is_array()) {
                std::string array_str = value.dump();
                js_code = "var " + key + " = " + array_str + ";";
                point_values_code = "point_values['" + key + "'] = " + array_str + ";";
                
            } else {
                js_code = "var " + key + " = null;";
                point_values_code = "point_values['" + key + "'] = null;";
            }
            
            // 전역 변수 설정
            JSValue var_result = JS_Eval(js_context_, js_code.c_str(), js_code.length(), 
                                        "<input_var>", JS_EVAL_TYPE_GLOBAL);
            if (JS_IsException(var_result)) {
                LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                             "입력 변수 '" + key + "' 설정 실패");
            }
            JS_FreeValue(js_context_, var_result);
            
            // point_values 객체에 설정
            if (!point_values_code.empty()) {
                JSValue pv_result = JS_Eval(js_context_, point_values_code.c_str(), point_values_code.length(), 
                                           "<point_values>", JS_EVAL_TYPE_GLOBAL);
                if (JS_IsException(pv_result)) {
                    LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                                 "point_values['" + key + "'] 설정 실패");
                }
                JS_FreeValue(js_context_, pv_result);
            }
        }
        
        // 디버깅 정보 출력
        std::string debug_code = "debugPointValues();";
        JSValue debug_result = JS_Eval(js_context_, debug_code.c_str(), debug_code.length(), 
                                      "<debug>", JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(js_context_, debug_result);
        
        // 전처리된 수식 실행
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::DEBUG,
                                     "수식 실행: " + processed_formula.substr(0, 100) + 
                                     (processed_formula.length() > 100 ? "..." : ""));
        
        JSValue eval_result = JS_Eval(js_context_, 
                                      processed_formula.c_str(), 
                                      processed_formula.length(),
                                      "<formula>", 
                                      JS_EVAL_TYPE_GLOBAL);
        
        // 예외 처리 및 결과 변환
        if (JS_IsException(eval_result)) {
            JSValue exception = JS_GetException(js_context_);
            const char* error_str = JS_ToCString(js_context_, exception);
            std::string error_msg = error_str ? error_str : "Unknown error";
            
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                         "JavaScript 실행 오류: " + error_msg);
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                         "실행한 수식: " + processed_formula);
            
            JS_FreeCString(js_context_, error_str);
            JS_FreeValue(js_context_, exception);
            JS_FreeValue(js_context_, eval_result);
            
            throw std::runtime_error("JavaScript 실행 오류: " + error_msg);
        }
        
        // 결과 변환
        DataValue result;
        
        if (JS_IsBool(eval_result)) {
            bool bool_val = static_cast<bool>(JS_ToBool(js_context_, eval_result));
            result = DataValue(bool_val);
            std::string bool_str = bool_val ? "true" : "false";
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::DEBUG,
                                         "수식 결과 (bool): " + bool_str);
        } else if (JS_IsNumber(eval_result)) {
            double val;
            if (JS_ToFloat64(js_context_, &val, eval_result) == 0) {
                result = DataValue(val);
                LogManager::getInstance().log("VirtualPointEngine", LogLevel::DEBUG,
                                             "수식 결과 (number): " + std::to_string(val));
            } else {
                result = DataValue(0.0);
                LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                             "수치 변환 실패, 0.0으로 설정");
            }
        } else if (JS_IsString(eval_result)) {
            const char* str = JS_ToCString(js_context_, eval_result);
            std::string str_result = str ? str : "";
            result = DataValue(str_result);
            JS_FreeCString(js_context_, str);
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::DEBUG,
                                         "수식 결과 (string): " + str_result);
        } else {
            result = DataValue(0.0);
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                         "알 수 없는 결과 타입, 0.0으로 설정");
        }
        
        JS_FreeValue(js_context_, eval_result);
        
        return result;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                     "evaluateFormula 예외: " + std::string(e.what()));
        throw;
    }
}

std::string VirtualPointEngine::preprocessFormula(const std::string& formula, int tenant_id) {
    try {
        auto& script_mgr = ScriptLibraryManager::getInstance();
        
        auto dependencies = script_mgr.collectDependencies(formula);
        
        if (dependencies.empty()) {
            return formula;
        }
        
        std::stringstream complete_script;
        
        for (const auto& func_name : dependencies) {
            auto script_opt = script_mgr.getScript(func_name, tenant_id);
            if (script_opt) {
                complete_script << "// Library: " << script_opt->display_name << "\n";
                complete_script << script_opt->script_code << "\n\n";
                
                if (current_vp_id_ > 0) {
                    script_mgr.recordUsage(script_opt->id, current_vp_id_, "virtual_point");
                }
            }
        }
        
        complete_script << "// User Formula\n";
        complete_script << formula;
        
        return complete_script.str();
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                     "Formula preprocessing failed: " + std::string(e.what()));
        return formula;
    }
}

std::vector<int> VirtualPointEngine::getAffectedVirtualPoints(const DeviceDataMessage& msg) const {
    std::vector<int> affected_vps;
    
    std::shared_lock<std::shared_mutex> lock(dep_mutex_);
    
    for (const auto& data_point : msg.points) {
        auto it = point_to_vp_map_.find(data_point.point_id); 
        if (it != point_to_vp_map_.end()) {
            for (int vp_id : it->second) {
                if (std::find(affected_vps.begin(), affected_vps.end(), vp_id) == affected_vps.end()) {
                    affected_vps.push_back(vp_id);
                }
            }
        }
    }
    
    return affected_vps;
}

std::vector<int> VirtualPointEngine::getDependentVirtualPoints(int point_id) const {
    std::shared_lock<std::shared_mutex> lock(dep_mutex_);
    
    auto it = point_to_vp_map_.find(point_id);
    if (it != point_to_vp_map_.end()) {
        return std::vector<int>(it->second.begin(), it->second.end());
    }
    
    return {};
}

bool VirtualPointEngine::hasDependency(int vp_id, int point_id) const {
    std::shared_lock<std::shared_mutex> lock(dep_mutex_);
    
    auto it = vp_dependencies_.find(vp_id);
    if (it != vp_dependencies_.end()) {
        const auto& deps = it->second;
        return deps.find(point_id) != deps.end();
    }
    
    return false;
}

json VirtualPointEngine::collectInputValues(const VirtualPointDef& vp, const DeviceDataMessage& msg) {
    json inputs;
    
    if (!vp.input_points.contains("inputs") || !vp.input_points["inputs"].is_array()) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                     "가상포인트 " + vp.name + " 에 입력 매핑이 없음");
        return inputs;
    }
    
    for (const auto& input_def : vp.input_points["inputs"]) {
        if (!input_def.contains("variable") || !input_def.contains("point_id")) {
            continue;
        }
        
        std::string var_name = input_def["variable"].get<std::string>();
        int point_id = input_def["point_id"].get<int>();
        
        auto value_opt = getPointValue(std::to_string(point_id), msg);
        if (value_opt) {
            inputs[var_name] = *value_opt;
        } else {
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                         "가상포인트 " + vp.name + " 의 입력 " + var_name + 
                                         " (point_id=" + std::to_string(point_id) + ") 값을 찾을 수 없음");
            inputs[var_name] = 0.0;
        }
    }
    
    return inputs;
}

void VirtualPointEngine::updateVirtualPointStats(int vp_id, const CalculationResult& result) {
    std::unique_lock<std::shared_mutex> lock(vp_mutex_);
    
    auto it = virtual_points_.find(vp_id);
    if (it != virtual_points_.end()) {
        auto& vp = it->second;
        if (result.value.is_number()) {
            vp.last_value = jsonToDataValue(result.value);
        }
        vp.last_calculation = std::chrono::system_clock::now();
        
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::DEBUG,
                                     "가상포인트 " + vp.name + " 통계 업데이트 완료");
    }
}

void VirtualPointEngine::triggerAlarmEvaluation(int vp_id, const DataValue& value) {
    try {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::DEBUG,
                                     "가상포인트 " + std::to_string(vp_id) + " 알람 평가 트리거");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                     "가상포인트 " + std::to_string(vp_id) + " 알람 평가 실패: " + std::string(e.what()));
    }
}

ExecutionResult VirtualPointEngine::executeScript(const CalculationContext& context) {
    ExecutionResult result;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        json inputs = context.input_values;
        
        auto data_value = evaluateFormula(context.formula, inputs);
        
        if (std::holds_alternative<double>(data_value)) {
            result.result = std::get<double>(data_value);
        } else if (std::holds_alternative<std::string>(data_value)) {
            result.result = std::get<std::string>(data_value);
        } else if (std::holds_alternative<bool>(data_value)) {
            result.result = std::get<bool>(data_value);
        } else {
            result.result = nullptr;
        }
        
        result.status = ExecutionStatus::SUCCESS;
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);
        
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::DEBUG,
                                     "스크립트 실행 성공 (" + std::to_string(result.execution_time.count()) + "μs)");
        
    } catch (const std::exception& e) {
        result.status = ExecutionStatus::RUNTIME_ERROR;
        result.error_message = e.what();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);
        
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                     "스크립트 실행 실패: " + result.error_message);
    }
    
    return result;
}

bool VirtualPointEngine::reloadVirtualPoint(int vp_id) {
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                 "reloadVirtualPoint 미구현 - loadVirtualPoints 사용 권장");
    return false;
}

bool VirtualPointEngine::registerCustomFunction(const std::string& name, const std::string& code) {
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                 "registerCustomFunction은 ScriptLibraryManager 사용 권장");
    return false;
}

bool VirtualPointEngine::unregisterCustomFunction(const std::string& name) {
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                 "unregisterCustomFunction은 ScriptLibraryManager 사용 권장");
    return false;
}

std::vector<ScriptMetadata> VirtualPointEngine::getAvailableScripts(int tenant_id) const {
    auto& script_lib = ScriptLibraryManager::getInstance();
    
    // ScriptDefinition을 ScriptMetadata로 변환
    auto definitions = script_lib.getAllScripts(tenant_id);
    std::vector<ScriptMetadata> metadata_list;
    
    for (const auto& def : definitions) {
        ScriptMetadata meta;
        meta.id = def.id;
        meta.name = def.name;
        meta.display_name = def.display_name;
        meta.description = def.description;
        meta.category = stringToScriptCategory(def.category);
        meta.return_type = stringToScriptReturnType(def.return_type);
        meta.version = def.version;
        meta.author = def.author;
        meta.is_system = def.is_system;
        meta.usage_count = def.usage_count;
        meta.rating = def.rating;
        
        metadata_list.push_back(meta);
    }
    
    return metadata_list;
}

CalculationResult VirtualPointEngine::calculateWithFormula(const std::string& formula, const json& inputs) {
    CalculationResult result;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        DataValue formula_result = evaluateFormula(formula, inputs);
        result.value = dataValueToJson(formula_result);
        result.success = true;
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        // 통계 업데이트
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            statistics_.total_calculations++;
            statistics_.successful_calculations++;
        }
        
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::DEBUG,
                                     "수식 계산 성공 (" + std::to_string(result.execution_time.count()) + "ms)");
        
    } catch (const std::exception& e) {
        result.error_message = e.what();
        result.success = false;
        
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            statistics_.total_calculations++;
            statistics_.failed_calculations++;
        }
        
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                     "수식 계산 실패: " + result.error_message);
    }
    
    return result;
}

VirtualPointStatistics VirtualPointEngine::getStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

json VirtualPointEngine::getStatisticsJson() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    json stats_json;
    stats_json["total_points"] = statistics_.total_points;
    stats_json["active_points"] = statistics_.active_points;
    stats_json["error_points"] = statistics_.error_points;
    stats_json["total_calculations"] = statistics_.total_calculations;
    stats_json["successful_calculations"] = statistics_.successful_calculations;
    stats_json["failed_calculations"] = statistics_.failed_calculations;
    stats_json["avg_execution_time_ms"] = statistics_.avg_execution_time_ms;
    
    // 시간 필드들을 문자열로 변환
    auto time_t = std::chrono::system_clock::to_time_t(statistics_.last_calculation);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    stats_json["last_calculation"] = oss.str();
    
    return stats_json;
}

} // namespace VirtualPoint
} // namespace PulseOne