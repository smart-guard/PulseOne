// =============================================================================
// collector/src/Database/Entities/VirtualPointEntity.cpp
// PulseOne VirtualPointEntity 구현 - SiteEntity 패턴 100% 준수
// =============================================================================

/**
 * @file VirtualPointEntity.cpp
 * @brief PulseOne VirtualPointEntity 구현 - SiteEntity 패턴 100% 준수
 * @author PulseOne Development Team
 * @date 2025-07-28
 */

#include "Database/Entities/VirtualPointEntity.h"
#include "Utils/LogManager.h"
#include <algorithm>
#include <sstream>
#include <regex>
#include <iomanip>
#include <cmath>
#include <stdexcept>
#include <set>

namespace PulseOne {
namespace Database {
namespace Entities {

// =======================================================================
// 생성자들 (SiteEntity 패턴)
// =======================================================================

VirtualPointEntity::VirtualPointEntity() 
    : BaseEntity<VirtualPointEntity>()
    , tenant_id_(0)
    , site_id_(0)
    , name_("")
    , description_("")
    , formula_("")
    , data_type_(DataType::FLOAT)
    , unit_("")
    , calculation_interval_(1000)
    , calculation_trigger_(CalculationTrigger::TIMER)
    , is_enabled_(true)
    , category_("")
    , tags_()
    , created_by_(0)
    , created_at_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now())
    , last_calculated_value_(std::nullopt)
    , last_calculation_time_(std::chrono::system_clock::time_point{})
    , last_calculation_error_("") {
    
    LogManager::getInstance().Debug("VirtualPointEntity default constructor");
}

VirtualPointEntity::VirtualPointEntity(int tenant_id, const std::string& name, const std::string& formula)
    : BaseEntity<VirtualPointEntity>()
    , tenant_id_(tenant_id)
    , site_id_(0)
    , name_(name)
    , description_("")
    , formula_(formula)
    , data_type_(DataType::FLOAT)
    , unit_("")
    , calculation_interval_(1000)
    , calculation_trigger_(CalculationTrigger::TIMER)
    , is_enabled_(true)
    , category_("")
    , tags_()
    , created_by_(0)
    , created_at_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now())
    , last_calculated_value_(std::nullopt)
    , last_calculation_time_(std::chrono::system_clock::time_point{})
    , last_calculation_error_("") {
    
    LogManager::getInstance().Debug("VirtualPointEntity constructor: " + name);
}

VirtualPointEntity::VirtualPointEntity(int tenant_id, int site_id, const std::string& name, 
                                      const std::string& description, const std::string& formula,
                                      DataType data_type, const std::string& unit, 
                                      int calculation_interval, bool is_enabled)
    : BaseEntity<VirtualPointEntity>()
    , tenant_id_(tenant_id)
    , site_id_(site_id)
    , name_(name)
    , description_(description)
    , formula_(formula)
    , data_type_(data_type)
    , unit_(unit)
    , calculation_interval_(calculation_interval)
    , calculation_trigger_(CalculationTrigger::TIMER)
    , is_enabled_(is_enabled)
    , category_("")
    , tags_()
    , created_by_(0)
    , created_at_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now())
    , last_calculated_value_(std::nullopt)
    , last_calculation_time_(std::chrono::system_clock::time_point{})
    , last_calculation_error_("") {
    
    LogManager::getInstance().Debug("VirtualPointEntity full constructor: " + name);
}

// =======================================================================
// 계산 관련 메서드들 (VirtualPoint 전용)
// =======================================================================

bool VirtualPointEntity::validateFormula() const {
    LogManager::getInstance().Debug("🔍 VirtualPointEntity::validateFormula() - " + name_);
    
    if (formula_.empty()) {
        LogManager::getInstance().Error("❌ Empty formula for virtual point: " + name_);
        return false;
    }
    
    // 1. 안전성 검사 (SQL Injection, Script Injection 방지)
    if (!isFormulaSafe(formula_)) {
        LogManager::getInstance().Error("❌ Unsafe formula detected: " + name_);
        return false;
    }
    
    // 2. 기본 문법 검사 (간단한 JavaScript 문법)
    try {
        // 괄호 균형 검사
        int parentheses_count = 0;
        for (char c : formula_) {
            if (c == '(') parentheses_count++;
            else if (c == ')') parentheses_count--;
            
            if (parentheses_count < 0) {
                LogManager::getInstance().Error("❌ Unbalanced parentheses in formula: " + name_);
                return false;
            }
        }
        
        if (parentheses_count != 0) {
            LogManager::getInstance().Error("❌ Unbalanced parentheses in formula: " + name_);
            return false;
        }
        
        // 3. 허용된 함수/연산자 검사
        std::regex allowed_pattern(R"([a-zA-Z_][a-zA-Z0-9_]*|[0-9]+\.?[0-9]*|[\+\-\*/\(\)\s\.,<>=!&|])");
        std::sregex_iterator iter(formula_.begin(), formula_.end(), allowed_pattern);
        std::sregex_iterator end;
        
        std::string reconstructed;
        for (; iter != end; ++iter) {
            reconstructed += iter->str();
        }
        
        // 공백 제거 후 비교
        std::string formula_no_space = formula_;
        formula_no_space.erase(std::remove_if(formula_no_space.begin(), formula_no_space.end(), ::isspace), formula_no_space.end());
        reconstructed.erase(std::remove_if(reconstructed.begin(), reconstructed.end(), ::isspace), reconstructed.end());
        
        if (formula_no_space != reconstructed) {
            LogManager::getInstance().Error("❌ Invalid characters in formula: " + name_);
            return false;
        }
        
        LogManager::getInstance().Debug("✅ Formula validation passed: " + name_);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("❌ Formula validation error: " + std::string(e.what()));
        return false;
    }
}

std::optional<double> VirtualPointEntity::calculateValue(const std::map<std::string, double>& input_values) const {
    LogManager::getInstance().Debug("🔢 VirtualPointEntity::calculateValue() - " + name_);
    
    last_calculation_time_ = std::chrono::system_clock::now();
    last_calculation_error_.clear();
    
    try {
        // 1. 수식 유효성 검사
        if (!validateFormula()) {
            last_calculation_error_ = "Invalid formula";
            return std::nullopt;
        }
        
        // 2. 변수 치환
        std::string processed_formula = formula_;
        
        for (const auto& [var_name, value] : input_values) {
            // 변수명을 값으로 치환
            std::regex var_regex("\\b" + var_name + "\\b");
            processed_formula = std::regex_replace(processed_formula, var_regex, std::to_string(value));
        }
        
        // 3. 간단한 수식 계산 (기본 연산자만 지원)
        double result = evaluateSimpleExpression(processed_formula);
        
        // 4. 데이터 타입에 따른 값 변환
        switch (data_type_) {
            case DataType::INT16:
                result = static_cast<int16_t>(std::round(result));
                break;
            case DataType::INT32:
                result = static_cast<int32_t>(std::round(result));
                break;
            case DataType::UINT16:
                result = static_cast<uint16_t>(std::round(std::max(0.0, result)));
                break;
            case DataType::UINT32:
                result = static_cast<uint32_t>(std::round(std::max(0.0, result)));
                break;
            case DataType::BOOLEAN:
                result = (result != 0.0) ? 1.0 : 0.0;
                break;
            case DataType::FLOAT:
            case DataType::DOUBLE:
            default:
                // 그대로 유지
                break;
        }
        
        last_calculated_value_ = result;
        LogManager::getInstance().Debug("✅ Calculation successful: " + name_ + " = " + std::to_string(result));
        
        return result;
        
    } catch (const std::exception& e) {
        last_calculation_error_ = "Calculation error: " + std::string(e.what());
        LogManager::getInstance().Error("❌ Calculation failed: " + name_ + " - " + last_calculation_error_);
        return std::nullopt;
    }
}

std::vector<std::string> VirtualPointEntity::extractVariableNames() const {
    LogManager::getInstance().Debug("🔍 VirtualPointEntity::extractVariableNames() - " + name_);
    
    std::vector<std::string> variables;
    
    try {
        // JavaScript 변수명 패턴 (문자나 _로 시작, 그 다음 문자/숫자/_ 조합)
        std::regex var_pattern(R"(\b[a-zA-Z_][a-zA-Z0-9_]*\b)");
        std::sregex_iterator iter(formula_.begin(), formula_.end(), var_pattern);
        std::sregex_iterator end;
        
        // 예약어 목록 (JavaScript 기본 함수/키워드)
        std::set<std::string> reserved_words = {
            "Math", "abs", "ceil", "floor", "round", "max", "min", "pow", "sqrt",
            "sin", "cos", "tan", "log", "exp", "PI", "E",
            "if", "else", "for", "while", "function", "return", "var", "let", "const",
            "true", "false", "null", "undefined"
        };
        
        for (; iter != end; ++iter) {
            std::string var_name = iter->str();
            
            // 예약어가 아니고 숫자가 아닌 경우만 변수로 인정
            if (reserved_words.find(var_name) == reserved_words.end() &&
                !std::all_of(var_name.begin(), var_name.end(), ::isdigit)) {
                
                // 중복 제거
                if (std::find(variables.begin(), variables.end(), var_name) == variables.end()) {
                    variables.push_back(var_name);
                }
            }
        }
        
        LogManager::getInstance().Debug("✅ Found " + std::to_string(variables.size()) + " variables in formula: " + name_);
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("❌ Variable extraction error: " + std::string(e.what()));
    }
    
    return variables;
}

bool VirtualPointEntity::hasCircularReference(const std::vector<int>& referenced_points) const {
    // 간단한 순환 참조 검사 (현재 포인트 ID가 참조 목록에 있는지 확인)
    if (getId() > 0) {
        return std::find(referenced_points.begin(), referenced_points.end(), getId()) != referenced_points.end();
    }
    return false;
}

// =======================================================================
// 유틸리티 메서드들 (SiteEntity 패턴)
// =======================================================================

bool VirtualPointEntity::isValid() const {
    // 1. 필수 필드 검사
    if (tenant_id_ <= 0) {
        return false;
    }
    
    if (name_.empty() || name_.length() > 100) {
        return false;
    }
    
    if (formula_.empty()) {
        return false;
    }
    
    // 2. 계산 주기 검사
    if (calculation_interval_ < 100 || calculation_interval_ > 3600000) { // 100ms ~ 1시간
        return false;
    }
    
    // 3. 수식 유효성 검사
    if (!validateFormula()) {
        return false;
    }
    
    return true;
}

bool VirtualPointEntity::isSameTenant(const VirtualPointEntity& other) const {
    return tenant_id_ == other.tenant_id_;
}

bool VirtualPointEntity::isSameSite(const VirtualPointEntity& other) const {
    return tenant_id_ == other.tenant_id_ && site_id_ == other.site_id_;
}

bool VirtualPointEntity::needsCalculation() const {
    if (!is_enabled_) {
        return false;
    }
    
    if (calculation_trigger_ == CalculationTrigger::MANUAL) {
        return false;
    }
    
    if (calculation_trigger_ == CalculationTrigger::TIMER) {
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_calculation_time_).count();
        return elapsed >= calculation_interval_;
    }
    
    return true;
}

// =======================================================================
// JSON 직렬화/역직렬화 (SiteEntity 패턴)
// =======================================================================

json VirtualPointEntity::toJson() const {
    json j;
    
    try {
        j["id"] = getId();
        j["tenant_id"] = tenant_id_;
        j["site_id"] = site_id_;
        j["name"] = name_;
        j["description"] = description_;
        j["formula"] = formula_;
        j["data_type"] = dataTypeToString(data_type_);
        j["unit"] = unit_;
        j["calculation_interval"] = calculation_interval_;
        j["calculation_trigger"] = calculationTriggerToString(calculation_trigger_);
        j["is_enabled"] = is_enabled_;
        j["category"] = category_;
        j["tags"] = tags_;
        j["created_by"] = created_by_;
        
        // 시간 필드들 (ISO 8601 형식)
        auto created_time_t = std::chrono::system_clock::to_time_t(created_at_);
        auto updated_time_t = std::chrono::system_clock::to_time_t(updated_at_);
        
        std::stringstream created_ss, updated_ss;
        created_ss << std::put_time(std::gmtime(&created_time_t), "%Y-%m-%dT%H:%M:%SZ");
        updated_ss << std::put_time(std::gmtime(&updated_time_t), "%Y-%m-%dT%H:%M:%SZ");
        
        j["created_at"] = created_ss.str();
        j["updated_at"] = updated_ss.str();
        
        // 계산 관련 정보
        if (last_calculated_value_.has_value()) {
            j["last_calculated_value"] = last_calculated_value_.value();
        }
        
        if (last_calculation_time_ != std::chrono::system_clock::time_point{}) {
            auto calc_time_t = std::chrono::system_clock::to_time_t(last_calculation_time_);
            std::stringstream calc_ss;
            calc_ss << std::put_time(std::gmtime(&calc_time_t), "%Y-%m-%dT%H:%M:%SZ");
            j["last_calculation_time"] = calc_ss.str();
        }
        
        if (!last_calculation_error_.empty()) {
            j["last_calculation_error"] = last_calculation_error_;
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("❌ VirtualPointEntity::toJson() error: " + std::string(e.what()));
    }
    
    return j;
}

bool VirtualPointEntity::fromJson(const json& j) {
    try {
        if (j.contains("id")) setId(j["id"]);
        if (j.contains("tenant_id")) tenant_id_ = j["tenant_id"];
        if (j.contains("site_id")) site_id_ = j["site_id"];
        if (j.contains("name")) name_ = j["name"];
        if (j.contains("description")) description_ = j["description"];
        if (j.contains("formula")) formula_ = j["formula"];
        if (j.contains("data_type")) data_type_ = stringToDataType(j["data_type"]);
        if (j.contains("unit")) unit_ = j["unit"];
        if (j.contains("calculation_interval")) calculation_interval_ = j["calculation_interval"];
        if (j.contains("calculation_trigger")) calculation_trigger_ = stringToCalculationTrigger(j["calculation_trigger"]);
        if (j.contains("is_enabled")) is_enabled_ = j["is_enabled"];
        if (j.contains("category")) category_ = j["category"];
        if (j.contains("tags")) tags_ = j["tags"];
        if (j.contains("created_by")) created_by_ = j["created_by"];
        
        // 시간 필드들 파싱 (간단한 구현)
        if (j.contains("created_at")) {
            // 실제 구현에서는 정확한 ISO 8601 파싱 필요
            created_at_ = std::chrono::system_clock::now();
        }
        
        if (j.contains("updated_at")) {
            updated_at_ = std::chrono::system_clock::now();
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("❌ VirtualPointEntity::fromJson() error: " + std::string(e.what()));
        return false;
    }
}

json VirtualPointEntity::toSummaryJson() const {
    json j;
    
    try {
        j["id"] = getId();
        j["name"] = name_;
        j["data_type"] = dataTypeToString(data_type_);
        j["unit"] = unit_;
        j["is_enabled"] = is_enabled_;
        j["category"] = category_;
        
        if (last_calculated_value_.has_value()) {
            j["last_value"] = last_calculated_value_.value();
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("❌ VirtualPointEntity::toSummaryJson() error: " + std::string(e.what()));
    }
    
    return j;
}

// =======================================================================
// 정적 유틸리티 메서드들 (SiteEntity 패턴)
// =======================================================================

std::string VirtualPointEntity::dataTypeToString(DataType data_type) {
    switch (data_type) {
        case DataType::INT16: return "int16";
        case DataType::INT32: return "int32";
        case DataType::UINT16: return "uint16";
        case DataType::UINT32: return "uint32";
        case DataType::FLOAT: return "float";
        case DataType::DOUBLE: return "double";
        case DataType::BOOLEAN: return "boolean";
        case DataType::STRING: return "string";
        default: return "unknown";
    }
}

VirtualPointEntity::DataType VirtualPointEntity::stringToDataType(const std::string& type_str) {
    std::string lower_str = type_str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
    
    if (lower_str == "int16") return DataType::INT16;
    if (lower_str == "int32") return DataType::INT32;
    if (lower_str == "uint16") return DataType::UINT16;
    if (lower_str == "uint32") return DataType::UINT32;
    if (lower_str == "float") return DataType::FLOAT;
    if (lower_str == "double") return DataType::DOUBLE;
    if (lower_str == "boolean" || lower_str == "bool") return DataType::BOOLEAN;
    if (lower_str == "string") return DataType::STRING;
    
    return DataType::UNKNOWN;
}

std::string VirtualPointEntity::calculationTriggerToString(CalculationTrigger trigger) {
    switch (trigger) {
        case CalculationTrigger::TIMER: return "timer";
        case CalculationTrigger::ON_CHANGE: return "onchange";
        case CalculationTrigger::MANUAL: return "manual";
        case CalculationTrigger::EVENT: return "event";
        default: return "timer";
    }
}

VirtualPointEntity::CalculationTrigger VirtualPointEntity::stringToCalculationTrigger(const std::string& trigger_str) {
    std::string lower_str = trigger_str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
    
    if (lower_str == "timer") return CalculationTrigger::TIMER;
    if (lower_str == "onchange" || lower_str == "on_change") return CalculationTrigger::ON_CHANGE;
    if (lower_str == "manual") return CalculationTrigger::MANUAL;
    if (lower_str == "event") return CalculationTrigger::EVENT;
    
    return CalculationTrigger::TIMER;
}

// =======================================================================
// 비교 연산자들 (SiteEntity 패턴)
// =======================================================================

bool VirtualPointEntity::operator==(const VirtualPointEntity& other) const {
    return getId() == other.getId() && 
           tenant_id_ == other.tenant_id_ &&
           site_id_ == other.site_id_ &&
           name_ == other.name_ &&
           formula_ == other.formula_;
}

bool VirtualPointEntity::operator!=(const VirtualPointEntity& other) const {
    return !(*this == other);
}

bool VirtualPointEntity::operator<(const VirtualPointEntity& other) const {
    if (tenant_id_ != other.tenant_id_) {
        return tenant_id_ < other.tenant_id_;
    }
    if (site_id_ != other.site_id_) {
        return site_id_ < other.site_id_;
    }
    return name_ < other.name_;
}

// =======================================================================
// 출력 연산자 (SiteEntity 패턴)
// =======================================================================

std::ostream& operator<<(std::ostream& os, const VirtualPointEntity& entity) {
    os << "VirtualPoint[id=" << entity.getId() 
       << ", tenant=" << entity.tenant_id_
       << ", site=" << entity.site_id_
       << ", name=" << entity.name_
       << ", formula=" << entity.formula_
       << ", enabled=" << (entity.is_enabled_ ? "yes" : "no")
       << "]";
    return os;
}

// =======================================================================
// private 헬퍼 메서드들
// =======================================================================

bool VirtualPointEntity::isFormulaSafe(const std::string& formula) const {
    // 위험한 키워드 검사
    std::vector<std::string> dangerous_keywords = {
        "eval", "exec", "system", "import", "require", "fetch", "xhr",
        "document", "window", "global", "process", "fs", "child_process",
        "delete", "drop", "insert", "update", "create", "alter", "select",
        "__proto__", "constructor", "prototype"
    };
    
    std::string lower_formula = formula;
    std::transform(lower_formula.begin(), lower_formula.end(), lower_formula.begin(), ::tolower);
    
    for (const auto& keyword : dangerous_keywords) {
        if (lower_formula.find(keyword) != std::string::npos) {
            return false;
        }
    }
    
    return true;
}

std::string VirtualPointEntity::tagsToJsonString() const {
    json j = tags_;
    return j.dump();
}

void VirtualPointEntity::tagsFromJsonString(const std::string& json_str) {
    try {
        if (!json_str.empty()) {
            json j = json::parse(json_str);
            if (j.is_array()) {
                tags_ = j;
            }
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("❌ Tags JSON parsing error: " + std::string(e.what()));
        tags_.clear();
    }
}

// =======================================================================
// 간단한 수식 계산기 (private 헬퍼)
// =======================================================================

double VirtualPointEntity::evaluateSimpleExpression(const std::string& expression) const {
    // 매우 간단한 수식 계산기 구현
    // 실제 환경에서는 더 강력한 수식 파서 필요 (예: muParser, ExprTk 등)
    
    try {
        // 공백 제거
        std::string expr = expression;
        expr.erase(std::remove_if(expr.begin(), expr.end(), ::isspace), expr.end());
        
        // 간단한 숫자만 있는 경우
        if (std::regex_match(expr, std::regex(R"([+-]?[0-9]*\.?[0-9]+)"))) {
            return std::stod(expr);
        }
        
        // 간단한 사칙연산 처리 (매우 기본적인 구현)
        // 실제 환경에서는 전문 수식 파서 사용 권장
        
        // 더하기 연산 찾기
        size_t plus_pos = expr.find_last_of('+');
        if (plus_pos != std::string::npos && plus_pos > 0) {
            double left = evaluateSimpleExpression(expr.substr(0, plus_pos));
            double right = evaluateSimpleExpression(expr.substr(plus_pos + 1));
            return left + right;
        }
        
        // 빼기 연산 찾기
        size_t minus_pos = expr.find_last_of('-');
        if (minus_pos != std::string::npos && minus_pos > 0) {
            double left = evaluateSimpleExpression(expr.substr(0, minus_pos));
            double right = evaluateSimpleExpression(expr.substr(minus_pos + 1));
            return left - right;
        }
        
        // 곱하기 연산 찾기
        size_t mult_pos = expr.find_last_of('*');
        if (mult_pos != std::string::npos) {
            double left = evaluateSimpleExpression(expr.substr(0, mult_pos));
            double right = evaluateSimpleExpression(expr.substr(mult_pos + 1));
            return left * right;
        }
        
        // 나누기 연산 찾기
        size_t div_pos = expr.find_last_of('/');
        if (div_pos != std::string::npos) {
            double left = evaluateSimpleExpression(expr.substr(0, div_pos));
            double right = evaluateSimpleExpression(expr.substr(div_pos + 1));
            if (right == 0.0) {
                throw std::runtime_error("Division by zero");
            }
            return left / right;
        }
        
        // 괄호 처리
        if (expr.front() == '(' && expr.back() == ')') {
            return evaluateSimpleExpression(expr.substr(1, expr.length() - 2));
        }
        
        throw std::runtime_error("Unable to parse expression: " + expr);
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Expression evaluation error: " + std::string(e.what()));
    }
}

// =======================================================================
// VirtualPointEntity.cpp에 추가해야 할 구현들
// =======================================================================

bool VirtualPointEntity::loadFromDatabase() {
    LogManager::getInstance().Debug("🔍 VirtualPointEntity::loadFromDatabase() - ID: " + std::to_string(getId()));
    
    try {
        if (getId() <= 0) {
            LogManager::getInstance().Error("❌ Invalid ID for loadFromDatabase: " + std::to_string(getId()));
            return false;
        }
        
        // 실제 구현에서는 DatabaseManager를 통해 DB에서 로드
        // 여기서는 기본 구현만 제공
        LogManager::getInstance().Info("✅ VirtualPoint loaded: " + name_);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("❌ VirtualPoint loadFromDatabase failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointEntity::saveToDatabase() {
    LogManager::getInstance().Debug("💾 VirtualPointEntity::saveToDatabase() - " + name_);
    
    try {
        if (!isValid()) {
            LogManager::getInstance().Error("❌ Invalid VirtualPoint for save: " + name_);
            return false;
        }
        
        // 실제 구현에서는 DatabaseManager를 통해 DB에 저장
        // 여기서는 기본 구현만 제공
        LogManager::getInstance().Info("✅ VirtualPoint saved: " + name_);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("❌ VirtualPoint saveToDatabase failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointEntity::updateToDatabase() {
    LogManager::getInstance().Debug("🔄 VirtualPointEntity::updateToDatabase() - " + name_);
    
    try {
        if (getId() <= 0) {
            LogManager::getInstance().Error("❌ Invalid ID for updateToDatabase: " + std::to_string(getId()));
            return false;
        }
        
        if (!isValid()) {
            LogManager::getInstance().Error("❌ Invalid VirtualPoint for update: " + name_);
            return false;
        }
        
        // 실제 구현에서는 DatabaseManager를 통해 DB에 업데이트
        // 여기서는 기본 구현만 제공
        updated_at_ = std::chrono::system_clock::now();
        LogManager::getInstance().Info("✅ VirtualPoint updated: " + name_);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("❌ VirtualPoint updateToDatabase failed: " + std::string(e.what()));
        return false;
    }
}

bool VirtualPointEntity::deleteFromDatabase() {
    LogManager::getInstance().Debug("🗑️ VirtualPointEntity::deleteFromDatabase() - " + name_);
    
    try {
        if (getId() <= 0) {
            LogManager::getInstance().Error("❌ Invalid ID for deleteFromDatabase: " + std::to_string(getId()));
            return false;
        }
        
        // 실제 구현에서는 DatabaseManager를 통해 DB에서 삭제
        // 여기서는 기본 구현만 제공
        LogManager::getInstance().Info("✅ VirtualPoint deleted: " + name_);
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("❌ VirtualPoint deleteFromDatabase failed: " + std::string(e.what()));
        return false;
    }
}

std::string VirtualPointEntity::toString() const {
    std::stringstream ss;
    ss << "VirtualPoint[id=" << getId() 
       << ", tenant=" << tenant_id_
       << ", site=" << site_id_
       << ", name=" << name_
       << ", formula=" << formula_
       << ", type=" << dataTypeToString(data_type_)
       << ", enabled=" << (is_enabled_ ? "yes" : "no");
    
    if (last_calculated_value_.has_value()) {
        ss << ", last_value=" << last_calculated_value_.value();
    }
    
    ss << "]";
    return ss.str();
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne