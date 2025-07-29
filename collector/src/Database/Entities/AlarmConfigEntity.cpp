// =============================================================================
// collector/src/Database/Entities/AlarmConfigEntity.cpp
// PulseOne 알람설정 엔티티 구현 - DeviceEntity 패턴 100% 준수
// =============================================================================

#include "Database/Entities/AlarmConfigEntity.h"
#include "Common/Constants.h"
#include <sstream>
#include <iomanip>
#include <algorithm>


namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

AlarmConfigEntity::AlarmConfigEntity() 
    : BaseEntity<AlarmConfigEntity>()
    , tenant_id_(0)
    , site_id_(0)
    , data_point_id_(0)
    , virtual_point_id_(0)
    , alarm_name_("")
    , description_("")
    , severity_(Severity::MEDIUM)
    , condition_type_(ConditionType::GREATER_THAN)
    , threshold_value_(0.0)
    , high_limit_(100.0)
    , low_limit_(0.0)
    , timeout_seconds_(30)
    , is_enabled_(true)
    , auto_acknowledge_(false)
    , delay_seconds_(0)
    , message_template_("Alarm: {{ALARM_NAME}} - Value: {{CURRENT_VALUE}}") {
}

AlarmConfigEntity::AlarmConfigEntity(int alarm_id) 
    : BaseEntity<AlarmConfigEntity>(alarm_id)
    , tenant_id_(0)
    , site_id_(0)
    , data_point_id_(0)
    , virtual_point_id_(0)
    , alarm_name_("")
    , description_("")
    , severity_(Severity::MEDIUM)
    , condition_type_(ConditionType::GREATER_THAN)
    , threshold_value_(0.0)
    , high_limit_(100.0)
    , low_limit_(0.0)
    , timeout_seconds_(30)
    , is_enabled_(true)
    , auto_acknowledge_(false)
    , delay_seconds_(0)
    , message_template_("Alarm: {{ALARM_NAME}} - Value: {{CURRENT_VALUE}}") {
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (DeviceEntity 패턴)
// =============================================================================

bool AlarmConfigEntity::loadFromDatabase() {
    if (id_ <= 0) {
        logger_->Error("AlarmConfigEntity::loadFromDatabase - Invalid alarm config ID: " + std::to_string(id_));
        markError();
        return false;
    }
    
    try {
        std::string query = "SELECT * FROM " + getTableName() + " WHERE id = " + std::to_string(id_);
        
        // 🔥 DeviceEntity와 동일한 방식으로 executeUnifiedQuery 사용
        auto results = executeUnifiedQuery(query);
        
        if (results.empty()) {
            logger_->Warn("AlarmConfigEntity::loadFromDatabase - Alarm config not found: " + std::to_string(id_));
            return false;
        }
        
        // 첫 번째 행을 엔티티로 변환
        bool success = mapRowToEntity(results[0]);
        
        if (success) {
            markSaved();  // DeviceEntity 패턴
            logger_->Info("AlarmConfigEntity::loadFromDatabase - Loaded alarm config: " + alarm_name_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigEntity::loadFromDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool AlarmConfigEntity::saveToDatabase() {
    if (!isValid()) {
        logger_->Error("AlarmConfigEntity::saveToDatabase - Invalid alarm config data");
        return false;
    }
    
    try {
        std::string sql = buildInsertSQL();  // DeviceEntity 패턴
        
        bool success = executeUnifiedNonQuery(sql);
        
        if (success) {
            // SQLite인 경우 마지막 INSERT ID 조회
            std::string db_type = config_manager_->getOrDefault("DATABASE_TYPE", "SQLITE");
            if (db_type == "SQLITE") {
                auto results = executeUnifiedQuery("SELECT last_insert_rowid() as id");
                if (!results.empty()) {
                    id_ = std::stoi(results[0]["id"]);
                }
            }
            
            markSaved();  // DeviceEntity 패턴
            logger_->Info("AlarmConfigEntity::saveToDatabase - Saved alarm config: " + alarm_name_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigEntity::saveToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool AlarmConfigEntity::updateToDatabase() {
    if (id_ <= 0 || !isValid()) {
        logger_->Error("AlarmConfigEntity::updateToDatabase - Invalid alarm config data or ID");
        return false;
    }
    
    try {
        std::string sql = buildUpdateSQL();  // DeviceEntity 패턴
        
        bool success = executeUnifiedNonQuery(sql);
        
        if (success) {
            markSaved();  // DeviceEntity 패턴
            logger_->Info("AlarmConfigEntity::updateToDatabase - Updated alarm config: " + alarm_name_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigEntity::updateToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool AlarmConfigEntity::deleteFromDatabase() {
    if (id_ <= 0) {
        logger_->Error("AlarmConfigEntity::deleteFromDatabase - Invalid alarm config ID");
        return false;
    }
    
    try {
        std::string sql = "DELETE FROM " + getTableName() + " WHERE id = " + std::to_string(id_);
        
        bool success = executeUnifiedNonQuery(sql);
        
        if (success) {
            markDeleted();  // DeviceEntity 패턴
            logger_->Info("AlarmConfigEntity::deleteFromDatabase - Deleted alarm config: " + alarm_name_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigEntity::deleteFromDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool AlarmConfigEntity::isValid() const {
    // 기본 유효성 검사
    if (alarm_name_.empty()) {
        return false;
    }
    
    if (tenant_id_ <= 0) {
        return false;
    }
    
    // 데이터포인트 또는 가상포인트 중 하나는 있어야 함
    if (data_point_id_ <= 0 && virtual_point_id_ <= 0) {
        return false;
    }
    
    // 타임아웃과 지연시간 검사
    if (timeout_seconds_ < 0 || delay_seconds_ < 0) {
        return false;
    }
    
    // 범위 조건일 때 상한값과 하한값 검사
    if (condition_type_ == ConditionType::BETWEEN || condition_type_ == ConditionType::OUT_OF_RANGE) {
        if (low_limit_ >= high_limit_) {
            return false;
        }
    }
    
    return true;
}

// =============================================================================
// JSON 직렬화 (DataPointEntity 패턴)
// =============================================================================

json AlarmConfigEntity::toJson() const {
    json j;
    
    try {
        // 기본 정보
        j["id"] = id_;
        j["tenant_id"] = tenant_id_;
        j["site_id"] = site_id_;
        j["data_point_id"] = data_point_id_;
        j["virtual_point_id"] = virtual_point_id_;
        
        // 알람 설정
        j["alarm_name"] = alarm_name_;
        j["description"] = description_;
        j["severity"] = severityToString(severity_);
        j["condition_type"] = conditionTypeToString(condition_type_);
        
        // 임계값 설정
        j["threshold_value"] = threshold_value_;
        j["high_limit"] = high_limit_;
        j["low_limit"] = low_limit_;
        j["timeout_seconds"] = timeout_seconds_;
        
        // 제어 설정
        j["is_enabled"] = is_enabled_;
        j["auto_acknowledge"] = auto_acknowledge_;
        j["delay_seconds"] = delay_seconds_;
        j["message_template"] = message_template_;
        
        // 시간 정보 (DeviceEntity 패턴)
        j["created_at"] = timestampToString(created_at_);
        j["updated_at"] = timestampToString(updated_at_);
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigEntity::toJson failed: " + std::string(e.what()));
    }
    
    return j;
}

bool AlarmConfigEntity::fromJson(const json& data) {
    try {
        if (data.contains("id")) id_ = data["id"];
        if (data.contains("tenant_id")) tenant_id_ = data["tenant_id"];
        if (data.contains("site_id")) site_id_ = data["site_id"];
        if (data.contains("data_point_id")) data_point_id_ = data["data_point_id"];
        if (data.contains("virtual_point_id")) virtual_point_id_ = data["virtual_point_id"];
        if (data.contains("alarm_name")) alarm_name_ = data["alarm_name"];
        if (data.contains("description")) description_ = data["description"];
        if (data.contains("severity")) severity_ = stringToSeverity(data["severity"]);
        if (data.contains("condition_type")) condition_type_ = stringToConditionType(data["condition_type"]);
        if (data.contains("threshold_value")) threshold_value_ = data["threshold_value"];
        if (data.contains("high_limit")) high_limit_ = data["high_limit"];
        if (data.contains("low_limit")) low_limit_ = data["low_limit"];
        if (data.contains("timeout_seconds")) timeout_seconds_ = data["timeout_seconds"];
        if (data.contains("is_enabled")) is_enabled_ = data["is_enabled"];
        if (data.contains("auto_acknowledge")) auto_acknowledge_ = data["auto_acknowledge"];
        if (data.contains("delay_seconds")) delay_seconds_ = data["delay_seconds"];
        if (data.contains("message_template")) message_template_ = data["message_template"];
        
        markModified();
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigEntity::fromJson failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

std::string AlarmConfigEntity::toString() const {
    std::stringstream ss;
    ss << "AlarmConfigEntity{";
    ss << "id=" << id_;
    ss << ", name='" << alarm_name_ << "'";
    ss << ", severity='" << severityToString(severity_) << "'";
    ss << ", condition='" << conditionTypeToString(condition_type_) << "'";
    ss << ", threshold=" << threshold_value_;
    ss << ", enabled=" << (is_enabled_ ? "true" : "false");
    ss << "}";
    return ss.str();
}

// =============================================================================
// 비즈니스 로직 메서드들
// =============================================================================

bool AlarmConfigEntity::evaluateCondition(double value) const {
    if (!is_enabled_) {
        return false;
    }
    
    switch (condition_type_) {
        case ConditionType::GREATER_THAN:
            return value > threshold_value_;
        
        case ConditionType::LESS_THAN:
            return value < threshold_value_;
        
        case ConditionType::EQUAL:
            return std::abs(value - threshold_value_) < 0.001;  // 부동소수점 허용 오차
        
        case ConditionType::NOT_EQUAL:
            return std::abs(value - threshold_value_) >= 0.001;
        
        case ConditionType::BETWEEN:
            return value >= low_limit_ && value <= high_limit_;
        
        case ConditionType::OUT_OF_RANGE:
            return value < low_limit_ || value > high_limit_;
        
        case ConditionType::RATE_OF_CHANGE:
            // 변화율 계산은 이전 값과의 비교가 필요하므로 여기서는 임계값 비교만
            return std::abs(value) > threshold_value_;
        
        default:
            return false;
    }
}

std::string AlarmConfigEntity::generateAlarmMessage(double current_value) const {
    return replaceTemplateVariables(message_template_, current_value);
}

int AlarmConfigEntity::getSeverityLevel() const {
    switch (severity_) {
        case Severity::LOW: return 0;
        case Severity::MEDIUM: return 1;
        case Severity::HIGH: return 2;
        case Severity::CRITICAL: return 3;
        default: return 1;
    }
}

std::string AlarmConfigEntity::severityToString(Severity severity) {
    switch (severity) {
        case Severity::LOW: return "LOW";
        case Severity::MEDIUM: return "MEDIUM";
        case Severity::HIGH: return "HIGH";
        case Severity::CRITICAL: return "CRITICAL";
        default: return "MEDIUM";
    }
}

AlarmConfigEntity::Severity AlarmConfigEntity::stringToSeverity(const std::string& severity_str) {
    if (severity_str == "LOW") return Severity::LOW;
    if (severity_str == "MEDIUM") return Severity::MEDIUM;
    if (severity_str == "HIGH") return Severity::HIGH;
    if (severity_str == "CRITICAL") return Severity::CRITICAL;
    return Severity::MEDIUM;  // 기본값
}

std::string AlarmConfigEntity::conditionTypeToString(ConditionType condition) {
    switch (condition) {
        case ConditionType::GREATER_THAN: return "GREATER_THAN";
        case ConditionType::LESS_THAN: return "LESS_THAN";
        case ConditionType::EQUAL: return "EQUAL";
        case ConditionType::NOT_EQUAL: return "NOT_EQUAL";
        case ConditionType::BETWEEN: return "BETWEEN";
        case ConditionType::OUT_OF_RANGE: return "OUT_OF_RANGE";
        case ConditionType::RATE_OF_CHANGE: return "RATE_OF_CHANGE";
        default: return "GREATER_THAN";
    }
}

AlarmConfigEntity::ConditionType AlarmConfigEntity::stringToConditionType(const std::string& condition_str) {
    if (condition_str == "GREATER_THAN") return ConditionType::GREATER_THAN;
    if (condition_str == "LESS_THAN") return ConditionType::LESS_THAN;
    if (condition_str == "EQUAL") return ConditionType::EQUAL;
    if (condition_str == "NOT_EQUAL") return ConditionType::NOT_EQUAL;
    if (condition_str == "BETWEEN") return ConditionType::BETWEEN;
    if (condition_str == "OUT_OF_RANGE") return ConditionType::OUT_OF_RANGE;
    if (condition_str == "RATE_OF_CHANGE") return ConditionType::RATE_OF_CHANGE;
    return ConditionType::GREATER_THAN;  // 기본값
}

// =============================================================================
// 고급 기능 (DeviceEntity 패턴)
// =============================================================================

json AlarmConfigEntity::extractConfiguration() const {
    json config = {
        {"basic", {
            {"alarm_name", alarm_name_},
            {"description", description_},
            {"severity", severityToString(severity_)},
            {"is_enabled", is_enabled_}
        }},
        {"condition", {
            {"type", conditionTypeToString(condition_type_)},
            {"threshold_value", threshold_value_},
            {"high_limit", high_limit_},
            {"low_limit", low_limit_}
        }},
        {"timing", {
            {"timeout_seconds", timeout_seconds_},
            {"delay_seconds", delay_seconds_},
            {"auto_acknowledge", auto_acknowledge_}
        }},
        {"target", {
            {"tenant_id", tenant_id_},
            {"site_id", site_id_},
            {"data_point_id", data_point_id_},
            {"virtual_point_id", virtual_point_id_}
        }}
    };
    
    return config;
}

json AlarmConfigEntity::getEvaluationContext() const {
    json context;
    context["alarm_id"] = id_;
    context["alarm_name"] = alarm_name_;
    context["condition_type"] = conditionTypeToString(condition_type_);
    context["threshold_value"] = threshold_value_;
    context["high_limit"] = high_limit_;
    context["low_limit"] = low_limit_;
    context["is_enabled"] = is_enabled_;
    context["severity_level"] = getSeverityLevel();
    
    return context;
}

json AlarmConfigEntity::getAlarmInfo() const {
    json info;
    info["alarm_id"] = id_;
    info["alarm_name"] = alarm_name_;
    info["description"] = description_;
    info["severity"] = severityToString(severity_);
    info["tenant_id"] = tenant_id_;
    info["data_point_id"] = data_point_id_;
    info["virtual_point_id"] = virtual_point_id_;
    info["is_enabled"] = is_enabled_;
    
    return info;
}

// =============================================================================
// 내부 헬퍼 메서드들 (DeviceEntity 패턴)
// =============================================================================

bool AlarmConfigEntity::mapRowToEntity(const std::map<std::string, std::string>& row) {
    try {
        if (row.count("id")) id_ = std::stoi(row.at("id"));
        if (row.count("tenant_id")) tenant_id_ = std::stoi(row.at("tenant_id"));
        if (row.count("site_id")) site_id_ = std::stoi(row.at("site_id"));
        if (row.count("data_point_id")) data_point_id_ = std::stoi(row.at("data_point_id"));
        if (row.count("virtual_point_id")) virtual_point_id_ = std::stoi(row.at("virtual_point_id"));
        if (row.count("alarm_name")) alarm_name_ = row.at("alarm_name");
        if (row.count("description")) description_ = row.at("description");
        if (row.count("severity")) severity_ = stringToSeverity(row.at("severity"));
        if (row.count("condition_type")) condition_type_ = stringToConditionType(row.at("condition_type"));
        if (row.count("threshold_value")) threshold_value_ = std::stod(row.at("threshold_value"));
        if (row.count("high_limit")) high_limit_ = std::stod(row.at("high_limit"));
        if (row.count("low_limit")) low_limit_ = std::stod(row.at("low_limit"));
        if (row.count("timeout_seconds")) timeout_seconds_ = std::stoi(row.at("timeout_seconds"));
        if (row.count("is_enabled")) is_enabled_ = (row.at("is_enabled") == "1" || row.at("is_enabled") == "true");
        if (row.count("auto_acknowledge")) auto_acknowledge_ = (row.at("auto_acknowledge") == "1" || row.at("auto_acknowledge") == "true");
        if (row.count("delay_seconds")) delay_seconds_ = std::stoi(row.at("delay_seconds"));
        if (row.count("message_template")) message_template_ = row.at("message_template");
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("AlarmConfigEntity::mapRowToEntity failed: " + std::string(e.what()));
        return false;
    }
}

std::string AlarmConfigEntity::buildInsertSQL() const {
    std::stringstream ss;
    ss << "INSERT INTO " << getTableName() << " ";
    ss << "(tenant_id, site_id, data_point_id, virtual_point_id, alarm_name, description, ";
    ss << "severity, condition_type, threshold_value, high_limit, low_limit, timeout_seconds, ";
    ss << "is_enabled, auto_acknowledge, delay_seconds, message_template, created_at, updated_at) ";
    ss << "VALUES (";
    ss << tenant_id_ << ", ";
    ss << site_id_ << ", ";
    ss << data_point_id_ << ", ";
    ss << virtual_point_id_ << ", '";
    ss << alarm_name_ << "', '";
    ss << description_ << "', '";
    ss << severityToString(severity_) << "', '";
    ss << conditionTypeToString(condition_type_) << "', ";
    ss << threshold_value_ << ", ";
    ss << high_limit_ << ", ";
    ss << low_limit_ << ", ";
    ss << timeout_seconds_ << ", ";
    ss << (is_enabled_ ? 1 : 0) << ", ";
    ss << (auto_acknowledge_ ? 1 : 0) << ", ";
    ss << delay_seconds_ << ", '";
    ss << message_template_ << "', '";
    ss << timestampToString(created_at_) << "', '";
    ss << timestampToString(updated_at_) << "')";
    
    return ss.str();
}

std::string AlarmConfigEntity::buildUpdateSQL() const {
    std::stringstream ss;
    ss << "UPDATE " << getTableName() << " SET ";
    ss << "tenant_id = " << tenant_id_ << ", ";
    ss << "site_id = " << site_id_ << ", ";
    ss << "data_point_id = " << data_point_id_ << ", ";
    ss << "virtual_point_id = " << virtual_point_id_ << ", ";
    ss << "alarm_name = '" << alarm_name_ << "', ";
    ss << "description = '" << description_ << "', ";
    ss << "severity = '" << severityToString(severity_) << "', ";
    ss << "condition_type = '" << conditionTypeToString(condition_type_) << "', ";
    ss << "threshold_value = " << threshold_value_ << ", ";
    ss << "high_limit = " << high_limit_ << ", ";
    ss << "low_limit = " << low_limit_ << ", ";
    ss << "timeout_seconds = " << timeout_seconds_ << ", ";
    ss << "is_enabled = " << (is_enabled_ ? 1 : 0) << ", ";
    ss << "auto_acknowledge = " << (auto_acknowledge_ ? 1 : 0) << ", ";
    ss << "delay_seconds = " << delay_seconds_ << ", ";
    ss << "message_template = '" << message_template_ << "', ";
    ss << "updated_at = '" << timestampToString(updated_at_) << "' ";
    ss << "WHERE id = " << id_;
    
    return ss.str();
}

std::string AlarmConfigEntity::replaceTemplateVariables(const std::string& template_str, double value) const {
    std::string result = template_str;
    
    // 변수 치환
    size_t pos = 0;
    while ((pos = result.find("{{ALARM_NAME}}", pos)) != std::string::npos) {
        result.replace(pos, 13, alarm_name_);
        pos += alarm_name_.length();
    }
    
    pos = 0;
    while ((pos = result.find("{{CURRENT_VALUE}}", pos)) != std::string::npos) {
        result.replace(pos, 17, std::to_string(value));
        pos += std::to_string(value).length();
    }
    
    pos = 0;
    while ((pos = result.find("{{THRESHOLD}}", pos)) != std::string::npos) {
        result.replace(pos, 13, std::to_string(threshold_value_));
        pos += std::to_string(threshold_value_).length();
    }
    
    pos = 0;
    while ((pos = result.find("{{SEVERITY}}", pos)) != std::string::npos) {
        result.replace(pos, 12, severityToString(severity_));
        pos += severityToString(severity_).length();
    }
    
    return result;
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne