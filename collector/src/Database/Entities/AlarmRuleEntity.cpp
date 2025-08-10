// =============================================================================
// collector/src/Database/Entities/AlarmRuleEntity.cpp
// PulseOne AlarmRuleEntity 구현 - DeviceEntity/DataPointEntity 패턴 100% 적용
// =============================================================================

/**
 * @file AlarmRuleEntity.cpp
 * @brief PulseOne AlarmRuleEntity 구현 - DeviceEntity/DataPointEntity 패턴 100% 적용
 * @author PulseOne Development Team
 * @date 2025-08-10
 * 
 * 🎯 DeviceEntity/DataPointEntity 패턴 완전 적용:
 * - 헤더에서는 선언만, CPP에서 Repository 호출
 * - Repository include는 CPP에서만 (순환 참조 방지)
 * - BaseEntity 순수 가상 함수 구현만 포함
 * - RepositoryFactory 패턴 사용
 */

#include "Database/Entities/AlarmRuleEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/AlarmRuleRepository.h"

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 구현 (CPP에서 구현하여 중복 제거)
// =============================================================================

AlarmRuleEntity::AlarmRuleEntity() 
    : BaseEntity<AlarmRuleEntity>()
    , tenant_id_(0)
    , name_("")
    , description_("")
    , target_type_(TargetType::DATA_POINT)
    , target_id_(std::nullopt)
    , target_group_("")
    , alarm_type_(AlarmType::ANALOG)
    , high_high_limit_(std::nullopt)
    , high_limit_(std::nullopt)
    , low_limit_(std::nullopt)
    , low_low_limit_(std::nullopt)
    , deadband_(0.0)
    , rate_of_change_(0.0)
    , trigger_condition_(DigitalTrigger::ON_CHANGE)
    , condition_script_("")
    , message_script_("")
    , message_config_("{}")
    , message_template_("")
    , severity_(Severity::MEDIUM)
    , priority_(100)
    , auto_acknowledge_(false)
    , acknowledge_timeout_min_(0)
    , auto_clear_(true)
    , suppression_rules_("{}")
    , notification_enabled_(true)
    , notification_delay_sec_(0)
    , notification_repeat_interval_min_(0)
    , notification_channels_("[]")
    , notification_recipients_("[]")
    , is_enabled_(true)
    , is_latched_(false)
    , created_at_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now())
    , created_by_(0) {
}

AlarmRuleEntity::AlarmRuleEntity(int alarm_id) 
    : AlarmRuleEntity() {  // 위임 생성자 사용
    setId(alarm_id);
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (Repository 활용)
// =============================================================================

bool AlarmRuleEntity::loadFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("AlarmRuleEntity::loadFromDatabase - Invalid alarm rule ID: " + std::to_string(getId()));
        }
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmRuleRepository();
        if (repo) {
            auto loaded = repo->findById(getId());
            if (loaded.has_value()) {
                *this = loaded.value();
                markSaved();
                if (logger_) {
                    logger_->Info("AlarmRuleEntity::loadFromDatabase - Loaded alarm rule: " + name_);
                }
                return true;
            }
        }
        
        if (logger_) {
            logger_->Warn("AlarmRuleEntity::loadFromDatabase - Alarm rule not found: " + std::to_string(getId()));
        }
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleEntity::loadFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool AlarmRuleEntity::saveToDatabase() {
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmRuleRepository();
        if (repo) {
            // 새 엔티티인 경우 save, 기존 엔티티인 경우 update
            bool success = false;
            if (getId() <= 0) {
                success = repo->save(*this);
            } else {
                success = repo->update(*this);
            }
            
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("AlarmRuleEntity::saveToDatabase - Saved alarm rule: " + name_);
                }
            }
            return success;
        }
        
        if (logger_) {
            logger_->Error("AlarmRuleEntity::saveToDatabase - Repository not available");
        }
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleEntity::saveToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool AlarmRuleEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("AlarmRuleEntity::deleteFromDatabase - Invalid alarm rule ID: " + std::to_string(getId()));
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmRuleRepository();
        if (repo) {
            bool success = repo->deleteById(getId());
            if (success) {
                if (logger_) {
                    logger_->Info("AlarmRuleEntity::deleteFromDatabase - Deleted alarm rule: " + name_);
                }
                setId(-1); // 삭제 후 ID 리셋
            }
            return success;
        }
        
        if (logger_) {
            logger_->Error("AlarmRuleEntity::deleteFromDatabase - Repository not available");
        }
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleEntity::deleteFromDatabase failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool AlarmRuleEntity::updateToDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("AlarmRuleEntity::updateToDatabase - Invalid alarm rule ID: " + std::to_string(getId()));
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmRuleRepository();
        if (repo) {
            bool success = repo->update(*this);
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("AlarmRuleEntity::updateToDatabase - Updated alarm rule: " + name_);
                }
            }
            return success;
        }
        
        if (logger_) {
            logger_->Error("AlarmRuleEntity::updateToDatabase - Repository not available");
        }
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmRuleEntity::updateToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

// =============================================================================
// 비즈니스 로직 메서드 구현
// =============================================================================

std::string AlarmRuleEntity::generateMessage(double value, const std::string& unit) const {
    if (!message_template_.empty()) {
        // 메시지 템플릿 사용
        return interpolateTemplate(message_template_, value, unit);
    }
    
    // 기본 메시지 생성
    std::ostringstream oss;
    oss << "ALARM: " << name_;
    oss << " - Current Value: " << std::fixed << std::setprecision(2) << value;
    if (!unit.empty()) {
        oss << " " << unit;
    }
    
    // 알람 조건에 따른 추가 정보
    if (alarm_type_ == AlarmType::ANALOG) {
        if (high_high_limit_.has_value() && value >= high_high_limit_.value()) {
            oss << " (HIGH-HIGH LIMIT: " << high_high_limit_.value() << ")";
        } else if (high_limit_.has_value() && value >= high_limit_.value()) {
            oss << " (HIGH LIMIT: " << high_limit_.value() << ")";
        } else if (low_low_limit_.has_value() && value <= low_low_limit_.value()) {
            oss << " (LOW-LOW LIMIT: " << low_low_limit_.value() << ")";
        } else if (low_limit_.has_value() && value <= low_limit_.value()) {
            oss << " (LOW LIMIT: " << low_limit_.value() << ")";
        }
    }
    
    oss << " - Severity: " << severityToString(severity_);
    
    return oss.str();
}

std::string AlarmRuleEntity::generateDigitalMessage(bool state) const {
    if (!message_template_.empty()) {
        // 메시지 템플릿에서 {{VALUE}} 치환
        std::string msg = message_template_;
        std::string value_str = state ? "TRUE" : "FALSE";
        
        size_t pos = msg.find("{{VALUE}}");
        if (pos != std::string::npos) {
            msg.replace(pos, 9, value_str);
        }
        pos = msg.find("{{CURRENT_VALUE}}");
        if (pos != std::string::npos) {
            msg.replace(pos, 17, value_str);
        }
        
        return msg;
    }
    
    // 기본 메시지 생성
    std::ostringstream oss;
    oss << "ALARM: " << name_;
    oss << " - Digital State: " << (state ? "TRUE" : "FALSE");
    oss << " - Trigger: " << digitalTriggerToString(trigger_condition_);
    oss << " - Severity: " << severityToString(severity_);
    
    return oss.str();
}

bool AlarmRuleEntity::isInAlarmState(double value) const {
    if (alarm_type_ != AlarmType::ANALOG) {
        return false;
    }
    
    // HIGH-HIGH 체크
    if (high_high_limit_.has_value() && value >= high_high_limit_.value()) {
        return true;
    }
    
    // HIGH 체크
    if (high_limit_.has_value() && value >= high_limit_.value()) {
        return true;
    }
    
    // LOW-LOW 체크
    if (low_low_limit_.has_value() && value <= low_low_limit_.value()) {
        return true;
    }
    
    // LOW 체크
    if (low_limit_.has_value() && value <= low_limit_.value()) {
        return true;
    }
    
    return false;
}

bool AlarmRuleEntity::checkSuppressionRules(const std::string& context_json) const {
    if (suppression_rules_.empty() || suppression_rules_ == "{}") {
        return false; // 억제 규칙 없음
    }
    
    try {
        // JSON 파싱하여 억제 규칙 체크
        auto rules = json::parse(suppression_rules_);
        auto context = json::parse(context_json);
        
        // 시간 기반 억제 체크
        if (rules.contains("time_based")) {
            auto time_rules = rules["time_based"];
            if (time_rules.contains("start_time") && time_rules.contains("end_time")) {
                // 현재 시간이 억제 시간 범위에 있는지 체크
                // 구현 필요
            }
        }
        
        // 조건 기반 억제 체크
        if (rules.contains("condition_based")) {
            auto condition_rules = rules["condition_based"];
            // 조건 기반 억제 로직 구현 필요
        }
        
        return false; // 기본적으로 억제하지 않음
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Warn("AlarmRuleEntity::checkSuppressionRules - JSON parsing failed: " + std::string(e.what()));
        }
        return false;
    }
}

int AlarmRuleEntity::getSeverityLevel() const {
    return static_cast<int>(severity_);
}

// =============================================================================
// 헬퍼 메서드 구현 (enum ↔ string 변환)
// =============================================================================

std::string AlarmRuleEntity::alarmTypeToString(AlarmType type) {
    switch (type) {
        case AlarmType::ANALOG: return "analog";
        case AlarmType::DIGITAL: return "digital";
        case AlarmType::SCRIPT: return "script";
        default: return "analog";
    }
}

AlarmRuleEntity::AlarmType AlarmRuleEntity::stringToAlarmType(const std::string& str) {
    if (str == "analog") return AlarmType::ANALOG;
    if (str == "digital") return AlarmType::DIGITAL;
    if (str == "script") return AlarmType::SCRIPT;
    return AlarmType::ANALOG; // 기본값
}

std::string AlarmRuleEntity::severityToString(Severity severity) {
    switch (severity) {
        case Severity::CRITICAL: return "critical";
        case Severity::HIGH: return "high";
        case Severity::MEDIUM: return "medium";
        case Severity::LOW: return "low";
        case Severity::INFO: return "info";
        default: return "medium";
    }
}

AlarmRuleEntity::Severity AlarmRuleEntity::stringToSeverity(const std::string& str) {
    if (str == "critical") return Severity::CRITICAL;
    if (str == "high") return Severity::HIGH;
    if (str == "medium") return Severity::MEDIUM;
    if (str == "low") return Severity::LOW;
    if (str == "info") return Severity::INFO;
    return Severity::MEDIUM; // 기본값
}

std::string AlarmRuleEntity::digitalTriggerToString(DigitalTrigger trigger) {
    switch (trigger) {
        case DigitalTrigger::ON_TRUE: return "on_true";
        case DigitalTrigger::ON_FALSE: return "on_false";
        case DigitalTrigger::ON_CHANGE: return "on_change";
        case DigitalTrigger::ON_RISING: return "on_rising";
        case DigitalTrigger::ON_FALLING: return "on_falling";
        default: return "on_change";
    }
}

AlarmRuleEntity::DigitalTrigger AlarmRuleEntity::stringToDigitalTrigger(const std::string& str) {
    if (str == "on_true") return DigitalTrigger::ON_TRUE;
    if (str == "on_false") return DigitalTrigger::ON_FALSE;
    if (str == "on_change") return DigitalTrigger::ON_CHANGE;
    if (str == "on_rising") return DigitalTrigger::ON_RISING;
    if (str == "on_falling") return DigitalTrigger::ON_FALLING;
    return DigitalTrigger::ON_CHANGE; // 기본값
}

std::string AlarmRuleEntity::targetTypeToString(TargetType type) {
    switch (type) {
        case TargetType::DATA_POINT: return "data_point";
        case TargetType::VIRTUAL_POINT: return "virtual_point";
        case TargetType::GROUP: return "group";
        default: return "data_point";
    }
}

AlarmRuleEntity::TargetType AlarmRuleEntity::stringToTargetType(const std::string& str) {
    if (str == "data_point") return TargetType::DATA_POINT;
    if (str == "virtual_point") return TargetType::VIRTUAL_POINT;
    if (str == "group") return TargetType::GROUP;
    return TargetType::DATA_POINT; // 기본값
}

// =============================================================================
// 내부 헬퍼 메서드 구현
// =============================================================================

std::string AlarmRuleEntity::timestampToString(const std::chrono::system_clock::time_point& tp) const {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string AlarmRuleEntity::interpolateTemplate(const std::string& tmpl, double value, const std::string& unit) const {
    std::string result = tmpl;
    
    // {{ALARM_NAME}} 치환
    size_t pos = result.find("{{ALARM_NAME}}");
    if (pos != std::string::npos) {
        result.replace(pos, 14, name_);
    }
    
    // {{CURRENT_VALUE}} 치환
    std::ostringstream value_oss;
    value_oss << std::fixed << std::setprecision(2) << value;
    std::string value_str = value_oss.str();
    
    pos = result.find("{{CURRENT_VALUE}}");
    if (pos != std::string::npos) {
        result.replace(pos, 17, value_str);
    }
    
    pos = result.find("{{VALUE}}");
    if (pos != std::string::npos) {
        result.replace(pos, 9, value_str);
    }
    
    // {{UNIT}} 치환
    pos = result.find("{{UNIT}}");
    if (pos != std::string::npos) {
        result.replace(pos, 8, unit);
    }
    
    // {{SEVERITY}} 치환
    pos = result.find("{{SEVERITY}}");
    if (pos != std::string::npos) {
        result.replace(pos, 12, severityToString(severity_));
    }
    
    // {{TIMESTAMP}} 치환
    pos = result.find("{{TIMESTAMP}}");
    if (pos != std::string::npos) {
        result.replace(pos, 13, timestampToString(std::chrono::system_clock::now()));
    }
    
    return result;
}



} // namespace Entities
} // namespace Database
} // namespace PulseOne