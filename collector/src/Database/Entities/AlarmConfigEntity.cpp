// =============================================================================
// collector/src/Database/Entities/AlarmConfigEntity.cpp
// PulseOne 알람설정 엔티티 구현 - DeviceEntity 패턴 100% 적용
// =============================================================================

/**
 * @file AlarmConfigEntity.cpp
 * @brief PulseOne AlarmConfigEntity 구현 - DeviceEntity 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 DeviceEntity 패턴 완전 적용:
 * - 헤더에서는 선언만, CPP에서 Repository 호출
 * - Repository include는 CPP에서만 (순환 참조 방지)
 * - BaseEntity 순수 가상 함수 구현만 포함
 * - Repository Factory 패턴 사용
 */

#include "Database/Entities/AlarmConfigEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/AlarmConfigRepository.h"

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 구현 (CPP에서 구현하여 중복 제거)
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
    , message_template_("Alarm: {{ALARM_NAME}} - Value: {{CURRENT_VALUE}}")
    , created_at_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now()) {
}

AlarmConfigEntity::AlarmConfigEntity(int alarm_id) 
    : AlarmConfigEntity() {  // 위임 생성자 사용
    setId(alarm_id);
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (DeviceEntity 패턴)
// =============================================================================

bool AlarmConfigEntity::loadFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("AlarmConfigEntity::loadFromDatabase - Invalid alarm config ID: " + std::to_string(getId()));
        }
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmConfigRepository();
        if (repo) {
            auto loaded = repo->findById(getId());
            if (loaded.has_value()) {
                *this = loaded.value();
                markSaved();
                if (logger_) {
                    logger_->Info("AlarmConfigEntity - Loaded alarm config: " + alarm_name_);
                }
                return true;
            }
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmConfigEntity::loadFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool AlarmConfigEntity::saveToDatabase() {
    if (!isValid()) {
        if (logger_) {
            logger_->Error("AlarmConfigEntity::saveToDatabase - Invalid alarm config data");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmConfigRepository();
        if (repo) {
            bool success = repo->save(*this);
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("AlarmConfigEntity - Saved alarm config: " + alarm_name_);
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmConfigEntity::saveToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool AlarmConfigEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("AlarmConfigEntity::deleteFromDatabase - Invalid alarm config ID");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmConfigRepository();
        if (repo) {
            bool success = repo->deleteById(getId());
            if (success) {
                markDeleted();
                if (logger_) {
                    logger_->Info("AlarmConfigEntity - Deleted alarm config: " + alarm_name_);
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmConfigEntity::deleteFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool AlarmConfigEntity::updateToDatabase() {
    if (getId() <= 0 || !isValid()) {
        if (logger_) {
            logger_->Error("AlarmConfigEntity::updateToDatabase - Invalid alarm config data or ID");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmConfigRepository();
        if (repo) {
            bool success = repo->update(*this);
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("AlarmConfigEntity - Updated alarm config: " + alarm_name_);
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmConfigEntity::updateToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

// =============================================================================
// 🔥 DeviceEntity 패턴 추가: timestampToString 메서드
// =============================================================================

std::string AlarmConfigEntity::timestampToString(const std::chrono::system_clock::time_point& timestamp) const {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// =============================================================================
// 유효성 검사
// =============================================================================

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
// JSON 직렬화 (DeviceEntity 패턴)
// =============================================================================

json AlarmConfigEntity::toJson() const {
    json j;
    
    try {
        // 기본 정보
        j["id"] = getId();
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
        
        // 🔥 DeviceEntity 패턴 적용: timestampToString 사용
        j["created_at"] = timestampToString(created_at_);
        j["updated_at"] = timestampToString(updated_at_);
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmConfigEntity::toJson failed: " + std::string(e.what()));
        }
    }
    
    return j;
}

bool AlarmConfigEntity::fromJson(const json& data) {
    try {
        if (data.contains("id")) setId(data["id"]);
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
        if (logger_) {
            logger_->Error("AlarmConfigEntity::fromJson failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

std::string AlarmConfigEntity::toString() const {
    std::stringstream ss;
    ss << "AlarmConfigEntity{";
    ss << "id=" << getId();
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
    context["alarm_id"] = getId();
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
    info["alarm_id"] = getId();
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
// private 헬퍼 메서드들
// =============================================================================

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