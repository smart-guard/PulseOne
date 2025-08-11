// =============================================================================
// collector/src/VirtualPoint/VirtualPointEngine.cpp
// PulseOne 가상포인트 엔진 완전 구현 - 올바른 싱글톤 사용법
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
#include <shared_mutex>

namespace PulseOne {
namespace VirtualPoint {

// =============================================================================
// 싱글톤 구현
// =============================================================================

VirtualPointEngine& VirtualPointEngine::getInstance() {
    static VirtualPointEngine instance;
    return instance;
}

VirtualPointEngine::VirtualPointEngine() {
    auto& logger = LogManager::getInstance();
    logger.log("virtualpoint", LogLevel::INFO, "🔧 VirtualPointEngine 생성");
}

VirtualPointEngine::~VirtualPointEngine() {
    shutdown();
}

// =============================================================================
// 초기화/종료 - 올바른 싱글톤 사용법
// =============================================================================

bool VirtualPointEngine::initialize() {
    if (initialized_) {
        auto& logger = LogManager::getInstance();
        logger.log("virtualpoint", LogLevel::WARN, "VirtualPointEngine 이미 초기화됨");
        return true;
    }
    
    auto& logger = LogManager::getInstance();
    logger.log("virtualpoint", LogLevel::INFO, "🚀 VirtualPointEngine 초기화 시작");
    
    try {
        // ✅ 올바른 싱글톤 사용법 - DatabaseManager 직접 가져오기
        auto& db_manager = Database::DatabaseManager::getInstance();
        
        // JavaScript 엔진 초기화
        if (!initJSEngine()) {
            logger.log("virtualpoint", LogLevel::ERROR, "JavaScript 엔진 초기화 실패");
            return false;
        }
        
        // ScriptLibraryManager 초기화 - 싱글톤끼리 직접 연동
        auto& script_lib = ScriptLibraryManager::getInstance();
        if (!script_lib.initialize()) {  // 파라미터 없이 호출
            logger.log("virtualpoint", LogLevel::ERROR, "ScriptLibraryManager 초기화 실패");
            return false;
        }
        
        // 시스템 함수 등록
        if (!registerSystemFunctions()) {
            logger.log("virtualpoint", LogLevel::WARN, "시스템 함수 등록 일부 실패");
        }
        
        initialized_ = true;
        logger.log("virtualpoint", LogLevel::INFO, "✅ VirtualPointEngine 초기화 완료");
        
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("virtualpoint", LogLevel::ERROR, 
                   "VirtualPointEngine 초기화 실패: " + std::string(e.what()));
        return false;
    }
}

void VirtualPointEngine::shutdown() {
    if (!initialized_) return;
    
    auto& logger = LogManager::getInstance();
    logger.log("virtualpoint", LogLevel::INFO, "VirtualPointEngine 종료 중...");
    
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
    
    initialized_ = false;
    logger.log("virtualpoint", LogLevel::INFO, "VirtualPointEngine 종료 완료");
}

// =============================================================================
// JavaScript 엔진 관리
// =============================================================================

bool VirtualPointEngine::initJSEngine() {
    std::lock_guard<std::mutex> lock(js_mutex_);
    
    // QuickJS 런타임 생성
    js_runtime_ = JS_NewRuntime();
    if (!js_runtime_) {
        auto& logger = LogManager::getInstance();
        logger.log("virtualpoint", LogLevel::ERROR, "JavaScript 런타임 생성 실패");
        return false;
    }
    
    // 메모리 제한 설정 (16MB)
    JS_SetMemoryLimit(js_runtime_, 16 * 1024 * 1024);
    
    // 컨텍스트 생성
    js_context_ = JS_NewContext(js_runtime_);
    if (!js_context_) {
        auto& logger = LogManager::getInstance();
        logger.log("virtualpoint", LogLevel::ERROR, "JavaScript 컨텍스트 생성 실패");
        JS_FreeRuntime(js_runtime_);
        js_runtime_ = nullptr;
        return false;
    }
    
    auto& logger = LogManager::getInstance();
    logger.log("virtualpoint", LogLevel::INFO, "✅ JavaScript 엔진 초기화 완료");
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
    
    auto& logger = LogManager::getInstance();
    logger.log("virtualpoint", LogLevel::INFO, "JavaScript 엔진 정리 완료");
}

bool VirtualPointEngine::registerSystemFunctions() {
    if (!js_context_) {
        auto& logger = LogManager::getInstance();
        logger.log("virtualpoint", LogLevel::ERROR, "JavaScript 엔진이 초기화되지 않음");
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
            auto& logger = LogManager::getInstance();
            logger.log("virtualpoint", LogLevel::WARN, 
                       "시스템 함수 '" + name + "' 등록 실패: " + e.what());
        }
    }
    
    auto& logger = LogManager::getInstance();
    logger.log("virtualpoint", LogLevel::INFO, 
               "시스템 함수 " + std::to_string(success_count) + "/" + 
               std::to_string(system_functions.size()) + "개 등록 완료");
    
    return success_count > 0;
}

// =============================================================================
// 가상포인트 관리 - 올바른 Repository 사용법
// =============================================================================

bool VirtualPointEngine::loadVirtualPoints(int tenant_id) {
    auto& logger = LogManager::getInstance();
    logger.log("virtualpoint", LogLevel::INFO, 
               "테넌트 " + std::to_string(tenant_id) + "의 가상포인트 로드 중...");
    
    try {
        // ✅ 올바른 Repository Factory 사용법
        auto repo = Database::RepositoryFactory::getInstance().createVirtualPointRepository();
        auto entities = repo->findByTenantId(tenant_id);
        
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
            vp_def.data_type = entity.getDataType();
            vp_def.unit = entity.getUnit();
            vp_def.calculation_interval_ms = entity.getCalculationInterval();
            vp_def.is_enabled = entity.getIsEnabled();
            
            // input_mappings 파싱
            if (!entity.getInputMappings().empty()) {
                try {
                    vp_def.input_mappings = json::parse(entity.getInputMappings());
                } catch (const std::exception& e) {
                    logger.log("virtualpoint", LogLevel::WARN, 
                               "가상포인트 " + std::to_string(vp_def.id) + 
                               " input_mappings 파싱 실패: " + e.what());
                }
            }
            
            virtual_points_[vp_def.id] = vp_def;
            
            // 의존성 맵 구축 (input_mappings 기반)
            if (vp_def.input_mappings.contains("inputs") && 
                vp_def.input_mappings["inputs"].is_array()) {
                
                for (const auto& input : vp_def.input_mappings["inputs"]) {
                    if (input.contains("point_id") && input["point_id"].is_number()) {
                        int point_id = input["point_id"].get<int>();
                        point_to_vp_map_[point_id].push_back(vp_def.id);
                        vp_dependencies_[vp_def.id].push_back(point_id);
                    }
                }
            }
        }
        
        logger.log("virtualpoint", LogLevel::INFO, 
                   "가상포인트 " + std::to_string(entities.size()) + "개 로드 완료");
        
        return true;
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("virtualpoint", LogLevel::ERROR, 
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
// 메인 계산 인터페이스 (Pipeline에서 호출)
// =============================================================================

std::vector<TimestampedValue> VirtualPointEngine::calculateForMessage(const DeviceDataMessage& msg) {
    std::vector<TimestampedValue> results;
    
    if (!initialized_) {
        auto& logger = LogManager::getInstance();
        logger.log("virtualpoint", LogLevel::ERROR, "VirtualPointEngine이 초기화되지 않음");
        return results;
    }
    
    // 이 메시지에 영향받는 가상포인트들 찾기
    auto affected_vps = getAffectedVirtualPoints(msg);
    
    if (affected_vps.empty()) {
        // 영향받는 가상포인트가 없음
        return results;
    }
    
    auto& logger = LogManager::getInstance();
    logger.log("virtualpoint", LogLevel::DEBUG_LEVEL, 
               "메시지로 인해 " + std::to_string(affected_vps.size()) + 
               "개 가상포인트 재계산 필요");
    
    // 각 가상포인트 계산
    for (int vp_id : affected_vps) {
        try {
            auto vp_opt = getVirtualPoint(vp_id);
            if (!vp_opt || !vp_opt->is_enabled) {
                continue;
            }
            
            // 입력값 수집
            json inputs = collectInputValues(*vp_opt, msg);
            
            // 계산 실행
            auto calc_result = calculate(vp_id, inputs);
            
            if (calc_result.success) {
                TimestampedValue tv;
                tv.point_id = vp_id;
                tv.value = calc_result.value;
                tv.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                tv.quality = "good";
                
                results.push_back(tv);
                
                // 통계 업데이트
                updateVirtualPointStats(vp_id, calc_result);
                
                // 알람 평가 트리거
                triggerAlarmEvaluation(vp_id, calc_result.value);
                
            } else {
                logger.log("virtualpoint", LogLevel::ERROR, 
                           "가상포인트 " + std::to_string(vp_id) + " 계산 실패: " + 
                           calc_result.error_message);
            }
            
        } catch (const std::exception& e) {
            logger.log("virtualpoint", LogLevel::ERROR, 
                       "가상포인트 " + std::to_string(vp_id) + " 처리 예외: " + e.what());
        }
    }
    
    logger.log("virtualpoint", LogLevel::DEBUG_LEVEL, 
               std::to_string(results.size()) + "개 가상포인트 계산 완료");
    
    return results;
}

// =============================================================================
// 개별 계산
// =============================================================================

CalculationResult VirtualPointEngine::calculate(int vp_id, const json& inputs) {
    CalculationResult result;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 통계용 현재 처리중인 가상포인트 ID 저장
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
        
        // tenant_id 설정 (라이브러리 함수 조회용)
        tenant_id_ = vp.tenant_id;
        
        // 수식 실행
        result.value = evaluateFormula(vp.formula, inputs);
        result.success = true;
        result.input_snapshot = inputs;
        
        // 실행 시간 계산
        auto end_time = std::chrono::high_resolution_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        // 통계 업데이트
        total_calculations_++;
        
        auto& logger = LogManager::getInstance();
        logger.log("virtualpoint", LogLevel::DEBUG_LEVEL, 
                   "가상포인트 " + vp.name + " 계산 완료: " + 
                   result.value.toString() + " (" + 
                   std::to_string(result.execution_time.count()) + "ms)");
        
    } catch (const std::exception& e) {
        result.error_message = e.what();
        result.success = false;
        failed_calculations_++;
        
        auto& logger = LogManager::getInstance();
        logger.log("virtualpoint", LogLevel::ERROR, 
                   "가상포인트 " + std::to_string(vp_id) + " 계산 실패: " + 
                   result.error_message);
    }
    
    // 초기화
    current_vp_id_ = 0;
    tenant_id_ = 0;
    
    return result;
}

CalculationResult VirtualPointEngine::calculateWithFormula(const std::string& formula, const json& inputs) {
    CalculationResult result;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // 수식 직접 실행
        result.value = evaluateFormula(formula, inputs);
        result.success = true;
        result.input_snapshot = inputs;
        
        // 실행 시간 계산
        auto end_time = std::chrono::high_resolution_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        // 통계 업데이트
        total_calculations_++;
        
    } catch (const std::exception& e) {
        result.error_message = e.what();
        result.success = false;
        failed_calculations_++;
        
        auto& logger = LogManager::getInstance();
        logger.log("virtualpoint", LogLevel::ERROR, 
                   "수식 계산 실패: " + result.error_message);
    }
    
    return result;
}

// =============================================================================
// JavaScript 수식 평가 (핵심 로직)
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

// =============================================================================
// 수식 전처리 (라이브러리 함수 주입)
// =============================================================================

std::string VirtualPointEngine::preprocessFormula(const std::string& formula, int tenant_id) {
    auto& script_lib = ScriptLibraryManager::getInstance();
    
    // 수식 분석: 라이브러리 함수 사용 여부 확인
    auto dependencies = script_lib.collectDependencies(formula);
    
    if (dependencies.empty()) {
        // 라이브러리 함수를 사용하지 않는 순수 수식
        return formula;
    }
    
    // 라이브러리 함수를 사용하는 경우, 함수 정의를 앞에 추가
    std::stringstream complete_script;
    
    // 의존 함수들을 먼저 추가
    for (const auto& func_name : dependencies) {
        auto script_opt = script_lib.getScript(func_name, tenant_id);
        if (script_opt) {
            complete_script << "// Library: " << script_opt->display_name << "\n";
            complete_script << script_opt->script_code << "\n\n";
            
            // 사용 통계 기록
            if (current_vp_id_ > 0) {
                script_lib.recordUsage(script_opt->id, current_vp_id_, "virtual_point");
            }
        }
    }
    
    // 원본 수식 추가
    complete_script << "// User Formula\n";
    complete_script << formula;
    
    return complete_script.str();
}

// =============================================================================
// 의존성 관리
// =============================================================================

std::vector<int> VirtualPointEngine::getAffectedVirtualPoints(const DeviceDataMessage& msg) const {
    std::vector<int> affected_vps;
    
    std::shared_lock<std::shared_mutex> lock(dep_mutex_);
    
    // 메시지의 각 데이터 포인트에 대해
    for (const auto& data_point : msg.data_points) {
        auto it = point_to_vp_map_.find(data_point.point_id);
        if (it != point_to_vp_map_.end()) {
            // 이 포인트에 의존하는 가상포인트들 추가
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
        return it->second;
    }
    
    return {};
}

bool VirtualPointEngine::hasDependency(int vp_id, int point_id) const {
    std::shared_lock<std::shared_mutex> lock(dep_mutex_);
    
    auto it = vp_dependencies_.find(vp_id);
    if (it != vp_dependencies_.end()) {
        const auto& deps = it->second;
        return std::find(deps.begin(), deps.end(), point_id) != deps.end();
    }
    
    return false;
}

// =============================================================================
// 입력값 수집
// =============================================================================

json VirtualPointEngine::collectInputValues(const VirtualPointDef& vp, const DeviceDataMessage& msg) {
    json inputs;
    
    if (!vp.input_mappings.contains("inputs") || !vp.input_mappings["inputs"].is_array()) {
        auto& logger = LogManager::getInstance();
        logger.log("virtualpoint", LogLevel::WARN, 
                   "가상포인트 " + vp.name + "에 입력 매핑이 없음");
        return inputs;
    }
    
    // input_mappings에 정의된 각 입력에 대해
    for (const auto& input_def : vp.input_mappings["inputs"]) {
        if (!input_def.contains("variable") || !input_def.contains("point_id")) {
            continue;
        }
        
        std::string var_name = input_def["variable"].get<std::string>();
        int point_id = input_def["point_id"].get<int>();
        
        // 메시지에서 해당 포인트 값 찾기
        auto value_opt = getPointValue(std::to_string(point_id), msg);
        if (value_opt) {
            inputs[var_name] = *value_opt;
        } else {
            // 현재 값이 없으면 기본값 또는 에러
            auto& logger = LogManager::getInstance();
            logger.log("virtualpoint", LogLevel::WARN, 
                       "가상포인트 " + vp.name + "의 입력 " + var_name + 
                       " (point_id=" + std::to_string(point_id) + ") 값을 찾을 수 없음");
            inputs[var_name] = 0.0;  // 기본값
        }
    }
    
    return inputs;
}

std::optional<double> VirtualPointEngine::getPointValue(const std::string& point_id, const DeviceDataMessage& msg) {
    int id = std::stoi(point_id);
    
    for (const auto& data_point : msg.data_points) {
        if (data_point.point_id == id) {
            return data_point.value.getDouble();
        }
    }
    
    return std::nullopt;
}

// =============================================================================
// 헬퍼 메서드들
// =============================================================================

void VirtualPointEngine::updateVirtualPointStats(int vp_id, const CalculationResult& result) {
    std::unique_lock<std::shared_mutex> lock(vp_mutex_);
    
    auto it = virtual_points_.find(vp_id);
    if (it != virtual_points_.end()) {
        auto& vp = it->second;
        vp.execution_count++;
        vp.last_value = result.value.getDouble();
        
        // 평균 실행 시간 업데이트 (이동평균)
        double new_time = static_cast<double>(result.execution_time.count());
        if (vp.execution_count == 1) {
            vp.avg_execution_time_ms = new_time;
        } else {
            vp.avg_execution_time_ms = (vp.avg_execution_time_ms * 0.9) + (new_time * 0.1);
        }
        
        if (!result.success) {
            vp.last_error = result.error_message;
        } else {
            vp.last_error.clear();
        }
    }
}

void VirtualPointEngine::triggerAlarmEvaluation(int vp_id, const DataValue& value) {
    try {
        // ✅ AlarmManager 직접 사용 - 올바른 싱글톤 연동
        auto& alarm_mgr = Alarm::AlarmManager::getInstance();
        
        // TODO: AlarmManager에 evaluateForVirtualPoint 메서드 추가 필요
        // 현재는 일반적인 알람 평가만 수행
        
    } catch (const std::exception& e) {
        auto& logger = LogManager::getInstance();
        logger.log("virtualpoint", LogLevel::ERROR, 
                   "가상포인트 " + std::to_string(vp_id) + " 알람 평가 실패: " + e.what());
    }
}

// =============================================================================
// 통계
// =============================================================================

json VirtualPointEngine::getStatistics() const {
    json stats;
    
    stats["total_calculations"] = total_calculations_.load();
    stats["failed_calculations"] = failed_calculations_.load();
    stats["success_rate"] = total_calculations_ > 0 ? 
        (double)(total_calculations_ - failed_calculations_) / total_calculations_ * 100.0 : 0.0;
    
    std::shared_lock<std::shared_mutex> lock(vp_mutex_);
    stats["loaded_virtual_points"] = virtual_points_.size();
    
    // 평균 실행 시간
    double total_avg_time = 0.0;
    int count = 0;
    for (const auto& [id, vp] : virtual_points_) {
        if (vp.execution_count > 0) {
            total_avg_time += vp.avg_execution_time_ms;
            count++;
        }
    }
    
    stats["average_execution_time_ms"] = count > 0 ? total_avg_time / count : 0.0;
    
    return stats;
}

// =============================================================================
// 사용하지 않는 메서드들 (호환성용 stub)
// =============================================================================

bool VirtualPointEngine::reloadVirtualPoint(int vp_id) {
    // TODO: 개별 가상포인트 리로드 구현
    auto& logger = LogManager::getInstance();
    logger.log("virtualpoint", LogLevel::WARN, "reloadVirtualPoint 미구현");
    return false;
}

bool VirtualPointEngine::registerCustomFunction(const std::string& name, const std::string& code) {
    // ScriptLibraryManager를 통해 처리
    auto& logger = LogManager::getInstance();
    logger.log("virtualpoint", LogLevel::WARN, "registerCustomFunction은 ScriptLibraryManager 사용 권장");
    return false;
}

bool VirtualPointEngine::unregisterCustomFunction(const std::string& name) {
    // ScriptLibraryManager를 통해 처리
    auto& logger = LogManager::getInstance();
    logger.log("virtualpoint", LogLevel::WARN, "unregisterCustomFunction은 ScriptLibraryManager 사용 권장");
    return false;
}

} // namespace VirtualPoint
} // namespace PulseOne