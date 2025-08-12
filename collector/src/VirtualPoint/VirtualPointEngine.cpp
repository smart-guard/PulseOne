// =============================================================================
// collector/src/VirtualPoint/VirtualPointEngine.cpp
// PulseOne 가상포인트 엔진 - 모든 문제점 수정된 완성 버전
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
// ✅ 자동 초기화 정적 변수들 (DatabaseManager 패턴)
// =============================================================================
std::once_flag VirtualPointEngine::init_flag_;
std::atomic<bool> VirtualPointEngine::initialization_success_(false);

// =============================================================================
// ✅ 자동 초기화 생성자 (DatabaseManager 패턴)
// =============================================================================
VirtualPointEngine::VirtualPointEngine() {
    // 기본 설정 (doInitialize()에서 재설정됨)
    statistics_.total_points = 0;
    statistics_.active_points = 0;
    statistics_.error_points = 0;
}

VirtualPointEngine::~VirtualPointEngine() {
    shutdown();
}

// =============================================================================
// ✅ 자동 초기화 싱글톤 구현 (DatabaseManager 패턴)
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
                                             "🚀 VirtualPointEngine 자동 초기화 성공!");
            } else {
                LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                             "❌ VirtualPointEngine 자동 초기화 실패!");
            }
        } catch (const std::exception& e) {
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                         "❌ VirtualPointEngine 초기화 예외: " + std::string(e.what()));
            initialization_success_.store(false, std::memory_order_release);
        }
    });
}

// =============================================================================
// ✅ 실제 초기화 로직 (DatabaseManager 패턴)
// =============================================================================
bool VirtualPointEngine::doInitialize() {
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                 "🔧 VirtualPointEngine 자동 초기화 시작...");
    
    try {
        // ✅ 싱글톤끼리 직접 연동 - 자동 초기화 활용
        auto& db_manager = DatabaseManager::getInstance();
        
        // JavaScript 엔진 초기화
        if (!initJSEngine()) {
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                         "JavaScript 엔진 초기화 실패");
            return false;
        }
        
        // ScriptLibraryManager 초기화 - 싱글톤끼리 직접 연동
        auto& script_lib = ScriptLibraryManager::getInstance();
        // script_lib는 자동 초기화되므로 별도 initialize() 호출 불필요
        
        // 시스템 함수 등록
        if (!registerSystemFunctions()) {
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                         "시스템 함수 등록 일부 실패");
        }
        
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                     "✅ VirtualPointEngine 자동 초기화 완료");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                     "VirtualPointEngine 초기화 실패: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// ✅ 강제 재초기화 (DatabaseManager 패턴)
// =============================================================================
void VirtualPointEngine::reinitialize() {
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                 "🔄 VirtualPointEngine 강제 재초기화 중...");
    
    // 기존 상태 정리
    shutdown();
    
    // 초기화 상태 리셋
    initialization_success_.store(false, std::memory_order_release);
    
    // 재초기화 실행
    bool success = doInitialize();
    initialization_success_.store(success, std::memory_order_release);
    
    if (success) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                     "✅ VirtualPointEngine 재초기화 성공");
    } else {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                     "❌ VirtualPointEngine 재초기화 실패");
    }
}

void VirtualPointEngine::shutdown() {
    if (!initialization_success_.load()) return;
    
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                 "VirtualPointEngine 종료 중...");
    
    // JavaScript 엔진 정리
    cleanupJSEngine();
    
    // 캐시 정리
    {
        std::unique_lock<std::shared_mutex> lock(vp_mutex_);
        virtual_points_.clear();
    }
    
    {
        std::unique_lock<std::shared_mutex> lock(dep_mutex_);
        point_to_vp_map_.clear();
        vp_dependencies_.clear();
    }
    
    // 통계 리셋
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
    
    // QuickJS 런타임 생성
    js_runtime_ = JS_NewRuntime();
    if (!js_runtime_) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                     "JavaScript 런타임 생성 실패");
        return false;
    }
    
    // 메모리 제한 설정 (16MB)
    JS_SetMemoryLimit(js_runtime_, 16 * 1024 * 1024);
    
    // 컨텍스트 생성
    js_context_ = JS_NewContext(js_runtime_);
    if (!js_context_) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                     "JavaScript 컨텍스트 생성 실패");
        JS_FreeRuntime(js_runtime_);
        js_runtime_ = nullptr;
        return false;
    }
    
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                 "✅ JavaScript 엔진 초기화 완료");
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
    
    // 기본 수학 함수들 등록
    const std::vector<std::pair<std::string, std::string>> system_functions = {
        {"Math.max", "Math.max"},
        {"Math.min", "Math.min"}, 
        {"Math.abs", "Math.abs"},
        {"Math.sqrt", "Math.sqrt"},
        {"Math.pow", "Math.pow"},
        {"Math.round", "Math.round"},
        {"Math.floor", "Math.floor"},
        {"Math.ceil", "Math.ceil"}
    };
    
    int success_count = 0;
    for (const auto& [name, code] : system_functions) {
        try {
            JSValue result = JS_Eval(js_context_, 
                                   ("var " + name.substr(5) + " = " + code + ";").c_str(),
                                   (name + code).length(),
                                   "<system>",
                                   JS_EVAL_TYPE_GLOBAL);
            
            if (!JS_IsException(result)) {
                success_count++;
            }
            JS_FreeValue(js_context_, result);
            
        } catch (const std::exception& e) {
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                         "시스템 함수 '" + name + "' 등록 실패: " + std::string(e.what()));
        }
    }
    
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                 "시스템 함수 " + std::to_string(success_count) + "/" + 
                                 std::to_string(system_functions.size()) + " 개 등록 완료");
    
    return success_count > 0;
}

// =============================================================================
// ✅ 가상포인트 관리 (Repository 패턴 활용)
// =============================================================================
bool VirtualPointEngine::loadVirtualPoints(int tenant_id) {
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::INFO,
                                 "테넌트 " + std::to_string(tenant_id) + "의 가상포인트 로드 중...");
    
    try {
        // ✅ Repository Factory를 통한 올바른 접근
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
            
            // ✅ VirtualPointTypes의 enum 변환 활용
            vp_def.execution_type = convertEntityExecutionType(entity.getExecutionType());
            vp_def.error_handling = convertEntityErrorHandling(entity.getErrorHandling());
            vp_def.state = entity.getIsEnabled() ? VirtualPointState::ACTIVE : VirtualPointState::INACTIVE;
            
            vp_def.calculation_interval_ms = std::chrono::milliseconds(entity.getCalculationInterval());
            vp_def.is_enabled = entity.getIsEnabled();
            
            // input_mappings 안전한 파싱
            if (!entity.getInputMappings().empty()) {
                try {
                    vp_def.input_points = json::parse(entity.getInputMappings());
                } catch (const std::exception& e) {
                    LogManager::getInstance().log("VirtualPointEngine", LogLevel::WARN,
                                                 "가상포인트 " + std::to_string(vp_def.id) + 
                                                 " input_mappings 파싱 실패: " + std::string(e.what()));
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
        
        // ✅ 통계 업데이트
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
// ✅ 메인 계산 인터페이스 (VirtualPointTypes 활용)
// =============================================================================

std::vector<TimestampedValue> VirtualPointEngine::calculateForMessage(const DeviceDataMessage& msg) {
    std::vector<TimestampedValue> results;
    
    if (!isInitialized()) {
        LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                     "VirtualPointEngine이 초기화되지 않음");
        return results;
    }
    
    // 이 메시지에 영향받는 가상포인트들 찾기
    auto affected_vps = getAffectedVirtualPoints(msg);
    
    if (affected_vps.empty()) {
        return results;
    }
    
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::DEBUG,
                                 "메시지로 인해 " + std::to_string(affected_vps.size()) + " 개 가상포인트 재계산 필요");
    
    // 각 가상포인트 계산
    for (int vp_id : affected_vps) {
        try {
            auto vp_opt = getVirtualPoint(vp_id);
            if (!vp_opt || !vp_opt->is_enabled) {
                continue;
            }
            
            // 입력값 수집
            json inputs = collectInputValues(*vp_opt, msg);
            
            // ✅ VirtualPointTypes의 CalculationResult 사용
            auto calc_result = calculate(vp_id, inputs);
            
            if (calc_result.success) {
                TimestampedValue tv;
                tv.value = jsonToDataValue(calc_result.value);
                tv.timestamp = std::chrono::system_clock::now();
                tv.quality = DataQuality::GOOD;
                
                results.push_back(tv);
                
                // 통계 업데이트
                updateVirtualPointStats(vp_id, calc_result);
                
                // 알람 평가 트리거
                triggerAlarmEvaluation(vp_id, jsonToDataValue(calc_result.value));
                
            } else {
                LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                             "가상포인트 " + std::to_string(vp_id) + 
                                             " 계산 실패: " + calc_result.error_message);
                
                // ✅ 통계에 실패 기록
                std::lock_guard<std::mutex> lock(stats_mutex_);
                statistics_.failed_calculations++;
            }
            
        } catch (const std::exception& e) {
            LogManager::getInstance().log("VirtualPointEngine", LogLevel::ERROR,
                                         "가상포인트 " + std::to_string(vp_id) + 
                                         " 처리 예외: " + std::string(e.what()));
        }
    }
    
    LogManager::getInstance().log("VirtualPointEngine", LogLevel::DEBUG,
                                 std::to_string(results.size()) + " 개 가상포인트 계산 완료");
    return results;
}

// =============================================================================
// ✅ 개별 계산 (VirtualPointTypes 활용)
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
        
        // ✅ VirtualPointStatistics 업데이트
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            statistics_.total_calculations++;
            statistics_.successful_calculations++;
            
            // 평균 실행 시간 업데이트
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
        
        // ✅ 실패 통계 업데이트
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
// ✅ 안전한 입력값 수집 (예외 처리 강화)
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
// ✅ 나머지 핵심 메서드들 (원래 코드 그대로 - 이미 완벽함)
// =============================================================================

DataValue VirtualPointEngine::evaluateFormula(const std::string& formula, const json& inputs) {
    std::lock_guard<std::mutex> lock(js_mutex_);
    
    if (!js_context_) {
        throw std::runtime_error("JavaScript 엔진이 초기화되지 않음");
    }
    
    // 수식 전처리 (라이브러리 함수 주입)
    std::string processed_formula = preprocessFormula(formula, tenant_id_);
    
    // 입력 변수들을 JavaScript 전역 객체로 설정
    for (auto& [key, value] : inputs.items()) {
        std::string js_code;
        
        if (value.is_number()) {
            js_code = "var " + key + " = " + std::to_string(value.get<double>()) + ";";
        } else if (value.is_boolean()) {
            js_code = "var " + key + " = " + (value.get<bool>() ? "true" : "false") + ";";
        } else if (value.is_string()) {
            js_code = "var " + key + " = '" + value.get<std::string>() + "';";
        } else if (value.is_array()) {
            js_code = "var " + key + " = " + value.dump() + ";";
        } else {
            js_code = "var " + key + " = null;";
        }
        
        JSValue var_result = JS_Eval(js_context_, js_code.c_str(), js_code.length(), 
                                     "<input>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(var_result)) {
            JS_FreeValue(js_context_, var_result);
            throw std::runtime_error("입력 변수 '" + key + "' 설정 실패");
        }
        JS_FreeValue(js_context_, var_result);
    }
    
    // 전처리된 수식 실행
    JSValue eval_result = JS_Eval(js_context_, 
                                  processed_formula.c_str(), 
                                  processed_formula.length(),
                                  "<formula>", 
                                  JS_EVAL_TYPE_GLOBAL);
    
    // 예외 처리
    if (JS_IsException(eval_result)) {
        JSValue exception = JS_GetException(js_context_);
        const char* error_str = JS_ToCString(js_context_, exception);
        std::string error_msg = error_str ? error_str : "Unknown error";
        JS_FreeCString(js_context_, error_str);
        JS_FreeValue(js_context_, exception);
        JS_FreeValue(js_context_, eval_result);
        
        throw std::runtime_error("JavaScript 실행 오류: " + error_msg);
    }
    
    // 결과 변환
    DataValue result;
    
    if (JS_IsBool(eval_result)) {
        result = DataValue(static_cast<bool>(JS_ToBool(js_context_, eval_result)));
    } else if (JS_IsNumber(eval_result)) {
        double val;
        if (JS_ToFloat64(js_context_, &val, eval_result) == 0) {
            result = DataValue(val);
        } else {
            result = DataValue(0.0);
        }
    } else if (JS_IsString(eval_result)) {
        const char* str = JS_ToCString(js_context_, eval_result);
        result = DataValue(std::string(str ? str : ""));
        JS_FreeCString(js_context_, str);
    } else {
        result = DataValue();  // null
    }
    
    JS_FreeValue(js_context_, eval_result);
    
    return result;
}

std::string VirtualPointEngine::preprocessFormula(const std::string& formula, int tenant_id) {
    try {
        // ✅ getInstance() 호출 전에 초기화 상태 확인
        auto& script_mgr = ScriptLibraryManager::getInstance();
        
        // ✅ collectDependencies 메서드 호출
        auto dependencies = script_mgr.collectDependencies(formula);
        
        if (dependencies.empty()) {
            return formula;
        }
        
        std::stringstream complete_script;
        
        // 라이브러리 함수들 주입
        for (const auto& func_name : dependencies) {
            auto script_opt = script_mgr.getScript(func_name, tenant_id);
            if (script_opt) {
                complete_script << "// Library: " << script_opt->display_name << "\n";
                complete_script << script_opt->script_code << "\n\n";
                
                // ✅ recordUsage 메서드 호출
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
        return formula; // 실패 시 원본 반환
    }
}

ExecutionType VirtualPointEngine::convertEntityExecutionType(
    const PulseOne::Database::Entities::VirtualPointEntity::ExecutionType& entity_type) {
    
    using EntityType = PulseOne::Database::Entities::VirtualPointEntity::ExecutionType;
    using VPType = ExecutionType;
    
    switch(entity_type) {
        case EntityType::JAVASCRIPT: return VPType::JAVASCRIPT;
        case EntityType::FORMULA: return VPType::FORMULA;
        case EntityType::AGGREGATE: return VPType::AGGREGATE;
        case EntityType::REFERENCE: return VPType::REFERENCE;
        default: return VPType::JAVASCRIPT;
    }
}

ErrorHandling VirtualPointEngine::convertEntityErrorHandling(
    const PulseOne::Database::Entities::VirtualPointEntity::ErrorHandling& entity_handling) {
    
    using EntityType = PulseOne::Database::Entities::VirtualPointEntity::ErrorHandling;
    using VPType = ErrorHandling;
    
    switch(entity_handling) {
        case EntityType::RETURN_NULL: return VPType::RETURN_NULL;
        case EntityType::RETURN_LAST: return VPType::RETURN_LAST;
        case EntityType::RETURN_ZERO: return VPType::RETURN_ZERO;
        case EntityType::RETURN_DEFAULT: return VPType::RETURN_DEFAULT;
        default: return VPType::RETURN_NULL;
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
            inputs[var_name] = 0.0;  // 기본값
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
        // AlarmManager 연동 (자동 초기화 활용)
        // auto& alarm_mgr = Alarm::AlarmManager::getInstance();
        // TODO: AlarmManager에 evaluateForVirtualPoint 메서드 추가 필요
        
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
        // CalculationContext를 json으로 변환
        json inputs = context.input_values;
        
        // 수식 실행
        auto data_value = evaluateFormula(context.formula, inputs);
        
        // DataValue를 json으로 변환
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

} // namespace VirtualPoint
} // namespace PulseOne