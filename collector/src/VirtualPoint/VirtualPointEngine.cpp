// =============================================================================
// VirtualPointEngine.cpp 수정 부분 - 기존 코드 유지하면서 라이브러리 통합
// =============================================================================

// 기존 include 유지
#include "VirtualPoint/VirtualPointEngine.h"
#include "VirtualPoint/ScriptLibraryManager.h"  // 🔥 추가
#include "Database/Repositories/VirtualPointRepository.h"
#include "Pipeline/PipelineManager.h"
#include "Alarm/AlarmManager.h"

namespace PulseOne {
namespace VirtualPoint {

// =============================================================================
// 🔥 새로운 메서드: 수식 전처리 (라이브러리 함수 감지 및 주입)
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
            script_lib.recordUsage(script_opt->id, current_vp_id_, "virtual_point");
        }
    }
    
    // 원본 수식 추가
    complete_script << "// User Formula\n";
    complete_script << formula;
    
    return complete_script.str();
}

// =============================================================================
// 기존 registerSystemFunctions() 메서드 확장
// =============================================================================
bool VirtualPointEngine::registerSystemFunctions() {
    if (!js_context_) return false;
    
    // 🔥 스크립트 라이브러리 초기화
    auto& script_lib = ScriptLibraryManager::getInstance();
    if (!script_lib.isInitialized()) {
        script_lib.initialize(db_manager_);
    }
    
    // 기존 시스템 함수들 (유지)
    std::string system_functions = R"(
        // 기본 수학 함수들 (기존 코드 유지)
        function avg(...values) {
            if (values.length === 0) return 0;
            return values.reduce((a, b) => a + b, 0) / values.length;
        }
        
        function max(...values) {
            if (values.length === 0) return 0;
            return Math.max(...values);
        }
        
        function min(...values) {
            if (values.length === 0) return 0;
            return Math.min(...values);
        }
        
        // ... 기존 함수들 유지 ...
    )";
    
    // 기존 함수 등록
    JSValue result = JS_Eval(js_context_, 
                            system_functions.c_str(), 
                            system_functions.length(),
                            "<system>", 
                            JS_EVAL_TYPE_GLOBAL);
    
    bool success = !JS_IsException(result);
    JS_FreeValue(js_context_, result);
    
    // 🔥 라이브러리에서 추가 시스템 함수 로드
    if (success) {
        auto library_scripts = script_lib.getScriptsByCategory("function");
        
        for (const auto& script : library_scripts) {
            if (script.is_system) {
                // 라이브러리 함수 등록
                JSValue lib_result = JS_Eval(js_context_, 
                                            script.script_code.c_str(),
                                            script.script_code.length(),
                                            script.name.c_str(),
                                            JS_EVAL_TYPE_GLOBAL);
                
                if (!JS_IsException(lib_result)) {
                    logger_.Log(Utils::LogLevel::DEBUG, 
                               "✅ 라이브러리 함수 등록: " + script.name);
                } else {
                    logger_.Log(Utils::LogLevel::WARNING, 
                               "라이브러리 함수 '" + script.name + "' 등록 실패");
                }
                
                JS_FreeValue(js_context_, lib_result);
            }
        }
        
        logger_.Log(Utils::LogLevel::INFO, 
                   "라이브러리에서 " + std::to_string(library_scripts.size()) + "개 함수 추가 로드");
    }
    
    return success;
}

// =============================================================================
// 기존 evaluateFormula() 메서드 수정 - 전처리 추가
// =============================================================================
DataValue VirtualPointEngine::evaluateFormula(const std::string& formula, const json& inputs) {
    std::lock_guard<std::mutex> lock(js_mutex_);
    
    if (!js_context_) {
        throw std::runtime_error("JavaScript 엔진이 초기화되지 않음");
    }
    
    // 🔥 수식 전처리 (라이브러리 함수 주입)
    std::string processed_formula = preprocessFormula(formula, tenant_id_);
    
    // 입력 변수들을 JavaScript 전역 객체로 설정 (기존 코드 유지)
    for (auto& [key, value] : inputs.items()) {
        std::string js_code;
        
        if (value.is_number()) {
            js_code = "var " + key + " = " + std::to_string(value.get<double>()) + ";";
        } else if (value.is_boolean()) {
            js_code = "var " + key + " = " + (value.get<bool>() ? "true" : "false") + ";";
        } else if (value.is_string()) {
            js_code = "var " + key + " = '" + value.get<std::string>() + "';";
        } else {
            js_code = "var " + key + " = null;";
        }
        
        JSValue var_result = JS_Eval(js_context_, js_code.c_str(), js_code.length(), 
                                     "<input>", JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(js_context_, var_result);
    }
    
    // 🔥 전처리된 수식 실행 (라이브러리 함수 포함)
    JSValue eval_result = JS_Eval(js_context_, processed_formula.c_str(), processed_formula.length(),
                                  "<formula>", JS_EVAL_TYPE_GLOBAL);
    
    // ... 나머지 기존 코드 유지 (에러 처리, 결과 변환 등) ...
    
    if (JS_IsException(eval_result)) {
        // 예외 정보 추출
        JSValue exception = JS_GetException(js_context_);
        const char* error_str = JS_ToCString(js_context_, exception);
        std::string error_msg = error_str ? error_str : "Unknown error";
        JS_FreeCString(js_context_, error_str);
        JS_FreeValue(js_context_, exception);
        JS_FreeValue(js_context_, eval_result);
        
        throw std::runtime_error("JavaScript 실행 오류: " + error_msg);
    }
    
    // 결과 변환 (기존 코드 유지)
    DataValue result;
    
    if (JS_IsBool(eval_result)) {
        result = DataValue(static_cast<bool>(JS_ToBool(js_context_, eval_result)));
    } else if (JS_IsNumber(eval_result)) {
        double val;
        JS_ToFloat64(js_context_, &val, eval_result);
        result = DataValue(val);
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
// 🔥 새로운 public 메서드: 수식과 함께 직접 계산 (외부에서 사용)
// =============================================================================
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
        result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // 통계 업데이트
        total_calculations_++;
        
    } catch (const std::exception& e) {
        result.error_message = e.what();
        failed_calculations_++;
        
        // 에러 로깅
        logger_.Log(Utils::LogLevel::ERROR, 
                   "수식 계산 실패: " + result.error_message);
    }
    
    return result;
}

// =============================================================================
// 🔥 새로운 메서드: 템플릿 기반 가상포인트 생성
// =============================================================================
bool VirtualPointEngine::createVirtualPointFromTemplate(
    int template_id, 
    const json& variables, 
    const std::string& vp_name,
    int tenant_id) {
    
    auto& script_lib = ScriptLibraryManager::getInstance();
    
    // 템플릿에서 코드 생성
    std::string generated_code = script_lib.generateFromTemplate(template_id, variables);
    
    if (generated_code.empty()) {
        logger_.Log(Utils::LogLevel::ERROR, "템플릿 기반 코드 생성 실패");
        return false;
    }
    
    // VirtualPointEntity 생성
    Database::Entities::VirtualPointEntity vp_entity;
    vp_entity.setTenantId(tenant_id);
    vp_entity.setName(vp_name);
    vp_entity.setDescription("Generated from template ID " + std::to_string(template_id));
    vp_entity.setFormula(generated_code);
    vp_entity.setIsEnabled(true);
    
    // input_mappings 생성 (템플릿 변수 기반)
    json input_mappings;
    input_mappings["inputs"] = json::array();
    
    for (auto& [var_name, var_value] : variables.items()) {
        if (var_value.is_string() && var_value.get<std::string>().find("point_") == 0) {
            // point_XXX 형식이면 데이터포인트 매핑
            json mapping;
            mapping["variable"] = var_name;
            mapping["point_id"] = std::stoi(var_value.get<std::string>().substr(6));
            input_mappings["inputs"].push_back(mapping);
        }
    }
    
    vp_entity.setInputMappings(input_mappings.dump());
    
    // DB에 저장
    auto repo = std::make_unique<Database::Repositories::VirtualPointRepository>();
    if (repo->save(vp_entity)) {
        // 메모리에 로드
        loadVirtualPoints(tenant_id);
        
        logger_.Log(Utils::LogLevel::INFO, 
                   "템플릿 기반 가상포인트 '" + vp_name + "' 생성 완료 (ID: " + 
                   std::to_string(vp_entity.getId()) + ")");
        return true;
    }
    
    return false;
}

// =============================================================================
// 🔥 기존 calculate() 메서드에 tenant_id 추가
// =============================================================================
CalculationResult VirtualPointEngine::calculate(int vp_id, const json& inputs) {
    CalculationResult result;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 🔥 현재 처리중인 가상포인트 ID 저장 (통계용)
    current_vp_id_ = vp_id;
    
    auto vp_opt = getVirtualPoint(vp_id);
    if (!vp_opt) {
        result.error_message = "가상포인트 ID " + std::to_string(vp_id) + " 찾을 수 없음";
        current_vp_id_ = 0;
        return result;
    }
    
    const auto& vp = *vp_opt;
    
    // 🔥 tenant_id 설정 (라이브러리 함수 조회용)
    tenant_id_ = vp.tenant_id;
    
    try {
        // JavaScript 수식 실행 (이제 라이브러리 함수도 자동 포함)
        result.value = evaluateFormula(vp.formula, inputs);
        result.success = true;
        result.input_snapshot = inputs;
        
        // ... 나머지 기존 코드 유지 ...
        
    } catch (const std::exception& e) {
        // ... 기존 에러 처리 코드 유지 ...
    }
    
    // 🔥 현재 처리중인 가상포인트 ID 초기화
    current_vp_id_ = 0;
    tenant_id_ = 0;
    
    return result;
}

// =============================================================================
// 🔥 VirtualPointEngine.h에 추가할 멤버 변수와 메서드
// =============================================================================
/*
class VirtualPointEngine {
private:
    // 기존 멤버 변수들...
    
    // 🔥 추가 멤버 변수
    int current_vp_id_ = 0;        // 현재 처리중인 가상포인트 ID
    int tenant_id_ = 0;             // 현재 tenant ID
    
public:
    // 🔥 추가 public 메서드
    CalculationResult calculateWithFormula(const std::string& formula, const json& inputs);
    bool createVirtualPointFromTemplate(int template_id, const json& variables, 
                                       const std::string& vp_name, int tenant_id = 0);
    
private:
    // 🔥 추가 private 메서드
    std::string preprocessFormula(const std::string& formula, int tenant_id);
};
*/

} // namespace VirtualPoint
} // namespace PulseOne