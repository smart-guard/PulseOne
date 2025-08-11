// =============================================================================
// collector/include/VirtualPoint/VirtualPointTypes.h
// PulseOne 가상포인트 시스템 - 공통 타입 정의
// =============================================================================

/**
 * @file VirtualPointTypes.h
 * @brief PulseOne 가상포인트 시스템 공통 타입 정의
 * @author PulseOne Development Team
 * @date 2025-08-11
 * 
 * 🎯 공통 타입 정의:
 * - ScriptCategory: 스크립트 카테고리 열거형
 * - ScriptReturnType: 스크립트 반환 타입 열거형
 * - 변환 함수들 포함
 * - AlarmTypes.h와 동일한 패턴 적용
 */

#ifndef VIRTUAL_POINT_TYPES_H
#define VIRTUAL_POINT_TYPES_H

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace VirtualPoint {

// =============================================================================
// 🎯 열거형 타입들
// =============================================================================

/**
 * @brief 스크립트 카테고리
 */
enum class ScriptCategory : uint8_t {
    FUNCTION = 0,       // 함수형 스크립트
    FORMULA = 1,        // 수식 스크립트  
    TEMPLATE = 2,       // 템플릿 스크립트
    CUSTOM = 3          // 사용자 정의 스크립트
};

/**
 * @brief 스크립트 반환 타입
 */
enum class ScriptReturnType : uint8_t {
    FLOAT = 0,          // 실수형
    STRING = 1,         // 문자열
    BOOLEAN = 2,        // 불린형
    OBJECT = 3          // 객체형 (JSON)
};

/**
 * @brief 가상포인트 상태
 */
enum class VirtualPointState : uint8_t {
    INACTIVE = 0,       // 비활성
    ACTIVE = 1,         // 활성
    ERROR = 2,          // 오류
    DISABLED = 3        // 비활성화
};

/**
 * @brief 실행 타입 (VirtualPointEntity에서 사용)
 */
enum class ExecutionType : uint8_t {
    JAVASCRIPT = 0,     // JavaScript 스크립트
    FORMULA = 1,        // 수식
    AGGREGATE = 2,      // 집계 함수
    REFERENCE = 3       // 참조
};

/**
 * @brief 에러 처리 방식 (VirtualPointEntity에서 사용)
 */
enum class ErrorHandling : uint8_t {
    RETURN_NULL = 0,    // NULL 반환
    RETURN_LAST = 1,    // 마지막 값 반환
    RETURN_ZERO = 2,    // 0 반환
    RETURN_DEFAULT = 3  // 기본값 반환
};

/**
 * @brief 의존성 타입 (VirtualPointDependencyEntity에서 사용)
 */
enum class DependsOnType : uint8_t {
    DATA_POINT = 0,     // 데이터 포인트 의존
    VIRTUAL_POINT = 1   // 가상포인트 의존
};

/**
 * @brief 계산 트리거 타입
 */
enum class CalculationTrigger : uint8_t {
    ON_CHANGE = 0,      // 입력값 변경시
    PERIODIC = 1,       // 주기적
    ON_DEMAND = 2,      // 요청시
    EVENT_DRIVEN = 3    // 이벤트 기반
};

/**
 * @brief 스크립트 실행 상태
 */
enum class ExecutionStatus : uint8_t {
    SUCCESS = 0,        // 성공
    RUNTIME_ERROR = 1,  // 런타임 오류
    TIMEOUT = 2,        // 타임아웃
    SYNTAX_ERROR = 3,   // 구문 오류
    DEPENDENCY_ERROR = 4 // 의존성 오류
};

// =============================================================================
// 🎯 구조체들
// =============================================================================

/**
 * @brief 스크립트 파라미터 정의
 */
struct ScriptParameter {
    std::string name;
    ScriptReturnType type = ScriptReturnType::FLOAT;
    std::string description;
    nlohmann::json default_value;
    bool required = true;
    std::optional<double> min_value;
    std::optional<double> max_value;
    std::vector<std::string> allowed_values;
};

/**
 * @brief 스크립트 메타데이터
 */
struct ScriptMetadata {
    int id = 0;
    std::string name;
    std::string display_name;
    std::string description;
    ScriptCategory category = ScriptCategory::CUSTOM;
    ScriptReturnType return_type = ScriptReturnType::FLOAT;
    std::vector<ScriptParameter> parameters;
    std::vector<std::string> tags;
    std::string version = "1.0.0";
    std::string author;
    bool is_system = false;
    int usage_count = 0;
    double rating = 0.0;
};

/**
 * @brief 계산 컨텍스트
 */
struct CalculationContext {
    int virtual_point_id = 0;
    int tenant_id = 0;
    nlohmann::json input_values;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, nlohmann::json> variables;
    std::string formula;
    std::chrono::milliseconds timeout{5000};
};

/**
 * @brief 실행 결과
 */
struct ExecutionResult {
    ExecutionStatus status = ExecutionStatus::SUCCESS;
    nlohmann::json result;
    std::string error_message;
    std::chrono::microseconds execution_time{0};
    std::map<std::string, nlohmann::json> debug_info;
    
    bool isSuccess() const { return status == ExecutionStatus::SUCCESS; }
    bool hasError() const { return status != ExecutionStatus::SUCCESS; }
};

/**
 * @brief 가상포인트 통계
 */
struct VirtualPointStatistics {
    int total_points = 0;
    int active_points = 0;
    int error_points = 0;
    int total_calculations = 0;
    int successful_calculations = 0;
    int failed_calculations = 0;
    double avg_execution_time_ms = 0.0;
    std::chrono::system_clock::time_point last_calculation;
    std::map<ScriptCategory, int> points_by_category;
    std::map<ExecutionStatus, int> calculations_by_status;
};

// =============================================================================
// 🎯 타입 변환 헬퍼 함수들 (네임스페이스 내부!)
// =============================================================================

// ScriptCategory 변환
inline std::string scriptCategoryToString(ScriptCategory category) {
    switch (category) {
        case ScriptCategory::FUNCTION: return "FUNCTION";
        case ScriptCategory::FORMULA: return "FORMULA";
        case ScriptCategory::TEMPLATE: return "TEMPLATE";
        case ScriptCategory::CUSTOM: return "CUSTOM";
        default: return "CUSTOM";
    }
}

inline ScriptCategory stringToScriptCategory(const std::string& str) {
    if (str == "FUNCTION" || str == "function") return ScriptCategory::FUNCTION;
    if (str == "FORMULA" || str == "formula") return ScriptCategory::FORMULA;
    if (str == "TEMPLATE" || str == "template") return ScriptCategory::TEMPLATE;
    if (str == "CUSTOM" || str == "custom") return ScriptCategory::CUSTOM;
    return ScriptCategory::CUSTOM;
}

// ScriptReturnType 변환
inline std::string scriptReturnTypeToString(ScriptReturnType type) {
    switch (type) {
        case ScriptReturnType::FLOAT: return "FLOAT";
        case ScriptReturnType::STRING: return "STRING";
        case ScriptReturnType::BOOLEAN: return "BOOLEAN";
        case ScriptReturnType::OBJECT: return "OBJECT";
        default: return "FLOAT";
    }
}

inline ScriptReturnType stringToScriptReturnType(const std::string& str) {
    if (str == "FLOAT" || str == "float") return ScriptReturnType::FLOAT;
    if (str == "STRING" || str == "string") return ScriptReturnType::STRING;
    if (str == "BOOLEAN" || str == "boolean") return ScriptReturnType::BOOLEAN;
    if (str == "OBJECT" || str == "object") return ScriptReturnType::OBJECT;
    return ScriptReturnType::FLOAT;
}

// ExecutionType 변환
inline std::string executionTypeToString(ExecutionType type) {
    switch (type) {
        case ExecutionType::JAVASCRIPT: return "JAVASCRIPT";
        case ExecutionType::FORMULA: return "FORMULA";
        case ExecutionType::AGGREGATE: return "AGGREGATE";
        case ExecutionType::REFERENCE: return "REFERENCE";
        default: return "JAVASCRIPT";
    }
}

inline ExecutionType stringToExecutionType(const std::string& str) {
    if (str == "JAVASCRIPT" || str == "javascript") return ExecutionType::JAVASCRIPT;
    if (str == "FORMULA" || str == "formula") return ExecutionType::FORMULA;
    if (str == "AGGREGATE" || str == "aggregate") return ExecutionType::AGGREGATE;
    if (str == "REFERENCE" || str == "reference") return ExecutionType::REFERENCE;
    return ExecutionType::JAVASCRIPT;
}

// ErrorHandling 변환
inline std::string errorHandlingToString(ErrorHandling handling) {
    switch (handling) {
        case ErrorHandling::RETURN_NULL: return "RETURN_NULL";
        case ErrorHandling::RETURN_LAST: return "RETURN_LAST";
        case ErrorHandling::RETURN_ZERO: return "RETURN_ZERO";
        case ErrorHandling::RETURN_DEFAULT: return "RETURN_DEFAULT";
        default: return "RETURN_NULL";
    }
}

inline ErrorHandling stringToErrorHandling(const std::string& str) {
    if (str == "RETURN_NULL" || str == "return_null") return ErrorHandling::RETURN_NULL;
    if (str == "RETURN_LAST" || str == "return_last") return ErrorHandling::RETURN_LAST;
    if (str == "RETURN_ZERO" || str == "return_zero") return ErrorHandling::RETURN_ZERO;
    if (str == "RETURN_DEFAULT" || str == "return_default") return ErrorHandling::RETURN_DEFAULT;
    return ErrorHandling::RETURN_NULL;
}

// DependsOnType 변환
inline std::string dependsOnTypeToString(DependsOnType type) {
    switch (type) {
        case DependsOnType::DATA_POINT: return "DATA_POINT";
        case DependsOnType::VIRTUAL_POINT: return "VIRTUAL_POINT";
        default: return "DATA_POINT";
    }
}

inline DependsOnType stringToDependsOnType(const std::string& str) {
    if (str == "DATA_POINT" || str == "data_point") return DependsOnType::DATA_POINT;
    if (str == "VIRTUAL_POINT" || str == "virtual_point") return DependsOnType::VIRTUAL_POINT;
    return DependsOnType::DATA_POINT;
}

// VirtualPointState 변환
inline std::string virtualPointStateToString(VirtualPointState state) {
    switch (state) {
        case VirtualPointState::INACTIVE: return "INACTIVE";
        case VirtualPointState::ACTIVE: return "ACTIVE";
        case VirtualPointState::ERROR: return "ERROR";
        case VirtualPointState::DISABLED: return "DISABLED";
        default: return "INACTIVE";
    }
}

inline VirtualPointState stringToVirtualPointState(const std::string& str) {
    if (str == "INACTIVE") return VirtualPointState::INACTIVE;
    if (str == "ACTIVE") return VirtualPointState::ACTIVE;
    if (str == "ERROR") return VirtualPointState::ERROR;
    if (str == "DISABLED") return VirtualPointState::DISABLED;
    return VirtualPointState::INACTIVE;
}

// CalculationTrigger 변환
inline std::string calculationTriggerToString(CalculationTrigger trigger) {
    switch (trigger) {
        case CalculationTrigger::ON_CHANGE: return "ON_CHANGE";
        case CalculationTrigger::PERIODIC: return "PERIODIC";
        case CalculationTrigger::ON_DEMAND: return "ON_DEMAND";
        case CalculationTrigger::EVENT_DRIVEN: return "EVENT_DRIVEN";
        default: return "ON_CHANGE";
    }
}

inline CalculationTrigger stringToCalculationTrigger(const std::string& str) {
    if (str == "ON_CHANGE") return CalculationTrigger::ON_CHANGE;
    if (str == "PERIODIC") return CalculationTrigger::PERIODIC;
    if (str == "ON_DEMAND") return CalculationTrigger::ON_DEMAND;
    if (str == "EVENT_DRIVEN") return CalculationTrigger::EVENT_DRIVEN;
    return CalculationTrigger::ON_CHANGE;
}

// ExecutionStatus 변환
inline std::string executionStatusToString(ExecutionStatus status) {
    switch (status) {
        case ExecutionStatus::SUCCESS: return "SUCCESS";
        case ExecutionStatus::RUNTIME_ERROR: return "RUNTIME_ERROR";
        case ExecutionStatus::TIMEOUT: return "TIMEOUT";
        case ExecutionStatus::SYNTAX_ERROR: return "SYNTAX_ERROR";
        case ExecutionStatus::DEPENDENCY_ERROR: return "DEPENDENCY_ERROR";
        default: return "SUCCESS";
    }
}

inline ExecutionStatus stringToExecutionStatus(const std::string& str) {
    if (str == "SUCCESS") return ExecutionStatus::SUCCESS;
    if (str == "RUNTIME_ERROR") return ExecutionStatus::RUNTIME_ERROR;
    if (str == "TIMEOUT") return ExecutionStatus::TIMEOUT;
    if (str == "SYNTAX_ERROR") return ExecutionStatus::SYNTAX_ERROR;
    if (str == "DEPENDENCY_ERROR") return ExecutionStatus::DEPENDENCY_ERROR;
    return ExecutionStatus::SUCCESS;
}

// =============================================================================
// 🎯 소문자 변환 헬퍼 함수들 (API/JSON 호환용)
// =============================================================================

// 소문자 변환 함수들
inline std::string scriptCategoryToLowerString(ScriptCategory category) {
    switch (category) {
        case ScriptCategory::FUNCTION: return "function";
        case ScriptCategory::FORMULA: return "formula";
        case ScriptCategory::TEMPLATE: return "template";
        case ScriptCategory::CUSTOM: return "custom";
        default: return "custom";
    }
}

inline std::string scriptReturnTypeToLowerString(ScriptReturnType type) {
    switch (type) {
        case ScriptReturnType::FLOAT: return "float";
        case ScriptReturnType::STRING: return "string";
        case ScriptReturnType::BOOLEAN: return "boolean";
        case ScriptReturnType::OBJECT: return "object";
        default: return "float";
    }
}

// 소문자 변환 함수들
inline std::string scriptCategoryToLowerString(ScriptCategory category) {
    switch (category) {
        case ScriptCategory::FUNCTION: return "function";
        case ScriptCategory::FORMULA: return "formula";
        case ScriptCategory::TEMPLATE: return "template";
        case ScriptCategory::CUSTOM: return "custom";
        default: return "custom";
    }
}

inline std::string scriptReturnTypeToLowerString(ScriptReturnType type) {
    switch (type) {
        case ScriptReturnType::FLOAT: return "float";
        case ScriptReturnType::STRING: return "string";
        case ScriptReturnType::BOOLEAN: return "boolean";
        case ScriptReturnType::OBJECT: return "object";
        default: return "float";
    }
}

inline std::string executionTypeToLowerString(ExecutionType type) {
    switch (type) {
        case ExecutionType::JAVASCRIPT: return "javascript";
        case ExecutionType::FORMULA: return "formula";
        case ExecutionType::AGGREGATE: return "aggregate";
        case ExecutionType::REFERENCE: return "reference";
        default: return "javascript";
    }
}

inline std::string errorHandlingToLowerString(ErrorHandling handling) {
    switch (handling) {
        case ErrorHandling::RETURN_NULL: return "return_null";
        case ErrorHandling::RETURN_LAST: return "return_last";
        case ErrorHandling::RETURN_ZERO: return "return_zero";
        case ErrorHandling::RETURN_DEFAULT: return "return_default";
        default: return "return_null";
    }
}

inline std::string virtualPointStateToLowerString(VirtualPointState state) {
    switch (state) {
        case VirtualPointState::INACTIVE: return "inactive";
        case VirtualPointState::ACTIVE: return "active";
        case VirtualPointState::ERROR: return "error";
        case VirtualPointState::DISABLED: return "disabled";
        default: return "inactive";
    }
}

} // namespace VirtualPoint
} // namespace PulseOne

#endif // VIRTUAL_POINT_TYPES_H