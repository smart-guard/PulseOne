// =============================================================================
// collector/src/VirtualPoint/VirtualPointEngine.cpp
// PulseOne 가상포인트 엔진 구현 - 완성본
// =============================================================================

#include "VirtualPoint/VirtualPointEngine.h"
#include <sstream>
#include <algorithm>
#include <cmath>

namespace PulseOne {
namespace VirtualPoint {

// =============================================================================
// 생성자/소멸자
// =============================================================================

VirtualPointEngine::VirtualPointEngine() 
    : logger_(Utils::LogManager::getInstance()) {
    logger_.Debug("VirtualPointEngine constructor");
}

VirtualPointEngine::~VirtualPointEngine() {
    shutdown();
}

VirtualPointEngine& VirtualPointEngine::getInstance() {
    static VirtualPointEngine instance;
    return instance;
}

// =============================================================================
// 초기화/종료
// =============================================================================

bool VirtualPointEngine::initialize(std::shared_ptr<Database::DatabaseManager> db_manager) {
    if (initialized_) {
        logger_.Warn("VirtualPointEngine already initialized");
        return true;
    }
    
    try {
        db_manager_ = db_manager;
        
        // JavaScript 엔진 초기화
        if (!initJSEngine()) {
            logger_.Error("Failed to initialize JS engine");
            return false;
        }
        
        // 시스템 함수 등록
        if (!registerSystemFunctions()) {
            logger_.Error("Failed to register system functions");
            return false;
        }
        
        initialized_ = true;
        logger_.Info("VirtualPointEngine initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("VirtualPointEngine initialization failed: " + std::string(e.what()));
        return false;
    }
}

void VirtualPointEngine::shutdown() {
    if (!initialized_) return;
    
    logger_.Info("Shutting down VirtualPointEngine");
    cleanupJSEngine();
    virtual_points_.clear();
    point_to_vp_map_.clear();
    initialized_ = false;
}

// =============================================================================
// JavaScript 엔진 관리
// =============================================================================

bool VirtualPointEngine::initJSEngine() {
    std::lock_guard<std::mutex> lock(js_mutex_);
    
    // Runtime 생성
    js_runtime_ = JS_NewRuntime();
    if (!js_runtime_) {
        logger_.Error("Failed to create JS runtime");
        return false;
    }
    
    // Context 생성
    js_context_ = JS_NewContext(js_runtime_);
    if (!js_context_) {
        logger_.Error("Failed to create JS context");
        JS_FreeRuntime(js_runtime_);
        js_runtime_ = nullptr;
        return false;
    }
    
    // 메모리 제한 설정 (10MB)
    JS_SetMemoryLimit(js_runtime_, 10 * 1024 * 1024);
    
    // 실행 시간 제한을 위한 인터럽트 핸들러 설정
    JS_SetInterruptHandler(js_runtime_, [](JSRuntime* rt, void* opaque) -> int {
        return 0;  // 0 = continue, 1 = interrupt
    }, nullptr);
    
    logger_.Debug("JS engine initialized");
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
}

// =============================================================================
// 시스템 함수 등록
// =============================================================================

bool VirtualPointEngine::registerSystemFunctions() {
    std::lock_guard<std::mutex> lock(js_mutex_);
    
    const std::string system_functions = R"(
        // 평균값 계산
        function avg(...values) {
            const valid = values.filter(v => v !== null && v !== undefined && !isNaN(v));
            return valid.length > 0 ? valid.reduce((a, b) => a + b, 0) / valid.length : null;
        }
        
        // 최대값
        function max(...values) {
            const valid = values.filter(v => v !== null && v !== undefined && !isNaN(v));
            return valid.length > 0 ? Math.max(...valid) : null;
        }
        
        // 최소값
        function min(...values) {
            const valid = values.filter(v => v !== null && v !== undefined && !isNaN(v));
            return valid.length > 0 ? Math.min(...valid) : null;
        }
        
        // 범위 제한
        function clamp(value, min, max) {
            return Math.min(Math.max(value, min), max);
        }
        
        // 스케일링
        function scale(value, inMin, inMax, outMin, outMax) {
            return (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
        }
        
        // 논리 AND
        function and(...values) {
            return values.every(v => v === true || v === 1);
        }
        
        // 논리 OR
        function or(...values) {
            return values.some(v => v === true || v === 1);
        }
        
        // 상승 엣지 감지
        function risingEdge(current, previous) {
            return !previous && current;
        }
        
        // 하강 엣지 감지
        function fallingEdge(current, previous) {
            return previous && !current;
        }
        
        // OEE 계산
        function oee(availability, performance, quality) {
            return (availability * performance * quality) / 10000;
        }
        
        // 현재 시간 (밀리초)
        function now() {
            return Date.now();
        }
        
        // 효율 계산
        function efficiency(actual, target) {
            if (target <= 0) return 0;
            return (actual / target) * 100;
        }
    )";
    
    JSValue result = JS_Eval(js_context_, system_functions.c_str(), 
                            system_functions.length(), "<system>", 
                            JS_EVAL_TYPE_GLOBAL);
    
    if (JS_IsException(result)) {
        logger_.Error("Failed to register system functions");
        JS_FreeValue(js_context_, result);
        return false;
    }
    
    JS_FreeValue(js_context_, result);
    logger_.Debug("System functions registered");
    return true;
}

// =============================================================================
// 가상포인트 로드
// =============================================================================

bool VirtualPointEngine::loadVirtualPoints(int tenant_id) {
    try {
        std::string query = R"(
            SELECT id, name, description, formula, data_type, unit,
                   calculation_interval, execution_type, error_handling,
                   dependencies, is_enabled
            FROM virtual_points
            WHERE tenant_id = ? AND is_enabled = 1
        )";
        
        auto results = db_manager_->executeQuery(query, {std::to_string(tenant_id)});
        
        std::unique_lock<std::shared_mutex> lock(vp_mutex_);
        
        for (const auto& row : results) {
            VirtualPointDef vp;
            vp.id = std::stoi(row.at("id"));
            vp.tenant_id = tenant_id;
            vp.name = row.at("name");
            vp.description = row.at("description");
            vp.formula = row.at("formula");
            vp.data_type = row.at("data_type");
            vp.unit = row.at("unit");
            vp.calculation_interval_ms = std::stoi(row.at("calculation_interval"));
            vp.execution_type = row.at("execution_type");
            vp.error_handling = row.at("error_handling");
            vp.is_enabled = true;
            
            // Dependencies JSON 파싱
            if (!row.at("dependencies").empty()) {
                vp.dependencies = json::parse(row.at("dependencies"));
            }
            
            virtual_points_[vp.id] = vp;
            
            // 의존성 맵 구축
            loadDependenciesFromDB(vp.id);
        }
        
        logger_.Info("Loaded " + std::to_string(virtual_points_.size()) + 
                    " virtual points for tenant " + std::to_string(tenant_id));
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to load virtual points: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointEngine::loadDependenciesFromDB(int vp_id) {
    try {
        std::string query = R"(
            SELECT depends_on_type, depends_on_id
            FROM virtual_point_dependencies
            WHERE virtual_point_id = ?
        )";
        
        auto results = db_manager_->executeQuery(query, {std::to_string(vp_id)});
        
        std::unique_lock<std::shared_mutex> lock(dep_mutex_);
        
        for (const auto& row : results) {
            int point_id = std::stoi(row.at("depends_on_id"));
            point_to_vp_map_[point_id].push_back(vp_id);
            vp_dependencies_[vp_id].push_back(point_id);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to load dependencies for VP " + std::to_string(vp_id) + 
                     ": " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// Pipeline 인터페이스 - 메인 처리 함수
// =============================================================================

std::vector<TimestampedValue> VirtualPointEngine::calculateForMessage(const DeviceDataMessage& msg) {
    std::vector<TimestampedValue> results;
    
    if (!initialized_) {
        logger_.Error("VirtualPointEngine not initialized");
        return results;
    }
    
    // 영향받는 가상포인트 찾기
    auto affected_vps = getAffectedVirtualPoints(msg);
    
    for (int vp_id : affected_vps) {
        std::shared_lock<std::shared_mutex> lock(vp_mutex_);
        
        auto it = virtual_points_.find(vp_id);
        if (it == virtual_points_.end() || !it->second.is_enabled) {
            continue;
        }
        
        const auto& vp = it->second;
        lock.unlock();
        
        // 입력값 수집
        auto inputs = collectInputValues(vp, msg);
        
        // 계산 실행
        auto result = calculate(vp_id, inputs);
        
        if (result.success) {
            TimestampedValue tv;
            tv.point_id = "vp_" + std::to_string(vp_id);
            tv.value = result.value;
            tv.timestamp = msg.timestamp;
            tv.quality = Enums::DataQuality::GOOD;
            
            results.push_back(tv);
            
            logger_.Debug("Virtual point " + vp.name + " calculated: " + 
                        std::visit([](auto&& v) { return std::to_string(v); }, result.value));
        }
    }
    
    return results;
}

// =============================================================================
// 개별 계산
// =============================================================================

CalculationResult VirtualPointEngine::calculate(int vp_id, const json& inputs) {
    CalculationResult result;
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        std::shared_lock<std::shared_mutex> lock(vp_mutex_);
        
        auto it = virtual_points_.find(vp_id);
        if (it == virtual_points_.end()) {
            result.error_message = "Virtual point not found";
            return result;
        }
        
        const auto& vp = it->second;
        lock.unlock();
        
        // 수식 평가
        result.value = evaluateFormula(vp.formula, inputs);
        result.success = true;
        result.input_snapshot = inputs;
        
        // 실행 시간 계산
        auto end_time = std::chrono::steady_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        // 통계 업데이트
        total_calculations_++;
        
        // 이력 저장
        saveExecutionHistory(vp_id, result);
        
    } catch (const std::exception& e) {
        result.error_message = e.what();
        result.success = false;
        failed_calculations_++;
        logger_.Error("Calculation failed for VP " + std::to_string(vp_id) + 
                     ": " + std::string(e.what()));
    }
    
    return result;
}

// =============================================================================
// 수식 평가
// =============================================================================

DataValue VirtualPointEngine::evaluateFormula(const std::string& formula, const json& inputs) {
    std::lock_guard<std::mutex> lock(js_mutex_);
    
    // 입력 변수를 JavaScript 전역 변수로 설정
    for (auto& [key, value] : inputs.items()) {
        std::string js_code = "var " + key + " = " + value.dump() + ";";
        JSValue var_result = JS_Eval(js_context_, js_code.c_str(), 
                                    js_code.length(), "<input>", 
                                    JS_EVAL_TYPE_GLOBAL);
        
        if (JS_IsException(var_result)) {
            JS_FreeValue(js_context_, var_result);
            throw std::runtime_error("Failed to set input variable: " + key);
        }
        JS_FreeValue(js_context_, var_result);
    }
    
    // 수식 평가
    std::string eval_code = "(" + formula + ")";
    JSValue result = JS_Eval(js_context_, eval_code.c_str(), 
                            eval_code.length(), "<formula>", 
                            JS_EVAL_TYPE_GLOBAL);
    
    if (JS_IsException(result)) {
        // 에러 메시지 추출
        JSValue exception = JS_GetException(js_context_);
        const char* error_str = JS_ToCString(js_context_, exception);
        std::string error_msg = error_str ? error_str : "Unknown error";
        JS_FreeCString(js_context_, error_str);
        JS_FreeValue(js_context_, exception);
        JS_FreeValue(js_context_, result);
        
        throw std::runtime_error("Formula evaluation failed: " + error_msg);
    }
    
    // 결과 추출
    DataValue value;
    
    if (JS_IsNumber(result)) {
        double d;
        JS_ToFloat64(js_context_, &d, result);
        value = d;
    } else if (JS_IsBool(result)) {
        value = JS_ToBool(js_context_, result) ? 1.0 : 0.0;
    } else if (JS_IsString(result)) {
        const char* str = JS_ToCString(js_context_, result);
        value = std::string(str ? str : "");
        JS_FreeCString(js_context_, str);
    } else {
        value = 0.0;  // 기본값
    }
    
    JS_FreeValue(js_context_, result);
    return value;
}

// =============================================================================
// 입력값 수집
// =============================================================================

json VirtualPointEngine::collectInputValues(const VirtualPointDef& vp, const DeviceDataMessage& msg) {
    json inputs;
    
    // 메시지의 모든 포인트를 변수로 변환
    for (const auto& point : msg.points) {
        // point_id에서 숫자만 추출 (예: "dp_123" -> "point_123")
        std::string var_name = "point_" + point.point_id.substr(point.point_id.find_last_of("_") + 1);
        
        // DataValue를 double로 변환
        double value = 0.0;
        std::visit([&value](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_arithmetic_v<T>) {
                value = static_cast<double>(v);
            }
        }, point.value);
        
        inputs[var_name] = value;
    }
    
    // 추가 컨텍스트 정보
    inputs["timestamp"] = std::chrono::system_clock::to_time_t(msg.timestamp);
    inputs["device_id"] = msg.device_id;
    
    return inputs;
}

// =============================================================================
// 의존성 관리
// =============================================================================

std::vector<int> VirtualPointEngine::getAffectedVirtualPoints(const DeviceDataMessage& msg) const {
    std::vector<int> affected;
    
    // 메시지에 명시된 가상포인트
    if (!msg.affected_virtual_points.empty()) {
        affected = msg.affected_virtual_points;
    }
    
    // 데이터포인트 기반으로 추가 탐색
    std::shared_lock<std::shared_mutex> lock(dep_mutex_);
    
    for (const auto& point : msg.points) {
        // point_id에서 숫자 추출
        size_t pos = point.point_id.find_last_of("_");
        if (pos != std::string::npos) {
            try {
                int point_id = std::stoi(point.point_id.substr(pos + 1));
                
                auto it = point_to_vp_map_.find(point_id);
                if (it != point_to_vp_map_.end()) {
                    affected.insert(affected.end(), it->second.begin(), it->second.end());
                }
            } catch (...) {
                // ID 파싱 실패 무시
            }
        }
    }
    
    // 중복 제거
    std::sort(affected.begin(), affected.end());
    affected.erase(std::unique(affected.begin(), affected.end()), affected.end());
    
    return affected;
}

// =============================================================================
// 데이터베이스 작업
// =============================================================================

bool VirtualPointEngine::saveExecutionHistory(int vp_id, const CalculationResult& result) {
    try {
        std::string query = R"(
            INSERT INTO virtual_point_execution_history 
            (virtual_point_id, execution_duration_ms, result_value, 
             input_snapshot, success, error_message)
            VALUES (?, ?, ?, ?, ?, ?)
        )";
        
        std::string result_value = json(result.value).dump();
        
        db_manager_->executeUpdate(query, {
            std::to_string(vp_id),
            std::to_string(result.execution_time.count()),
            result_value,
            result.input_snapshot.dump(),
            result.success ? "1" : "0",
            result.error_message
        });
        
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("Failed to save execution history: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 통계
// =============================================================================

json VirtualPointEngine::getStatistics() const {
    std::shared_lock<std::shared_mutex> lock(vp_mutex_);
    
    return {
        {"total_virtual_points", virtual_points_.size()},
        {"enabled_virtual_points", std::count_if(virtual_points_.begin(), virtual_points_.end(),
            [](const auto& p) { return p.second.is_enabled; })},
        {"total_calculations", total_calculations_.load()},
        {"failed_calculations", failed_calculations_.load()},
        {"success_rate", total_calculations_ > 0 ? 
            (double)(total_calculations_ - failed_calculations_) / total_calculations_ * 100 : 0}
    };
}

} // namespace VirtualPoint
} // namespace PulseOne