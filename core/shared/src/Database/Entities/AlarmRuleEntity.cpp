// =============================================================================
// collector/src/Database/Entities/AlarmRuleEntity.cpp
// PulseOne AlarmRuleEntity 구현 - 스키마 완전 동기화 버전
// =============================================================================

/**
 * @file AlarmRuleEntity.cpp
 * @brief PulseOne AlarmRuleEntity 구현 - DB 스키마와 완전 동기화
 * @author PulseOne Development Team
 * @date 2025-08-26
 * 
 * 스키마 동기화 완료:
 * - 새 필드들 초기화 추가
 * - JSON 직렬화/역직렬화 확장
 * - 비즈니스 로직 메서드 업데이트
 */

#include "Database/Entities/AlarmRuleEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/AlarmRuleRepository.h"
#include "Alarm/AlarmTypes.h"
#include "Logging/LogManager.h"

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 구현 - 모든 필드 초기화 (새 필드 포함)
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
    , severity_(AlarmSeverity::MEDIUM)
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
    , created_by_(0)
    // 새로 추가된 필드들 초기화
    , template_id_(std::nullopt)
    , rule_group_("")
    , created_by_template_(false)
    , last_template_update_(std::nullopt)
    , escalation_enabled_(false)
    , escalation_max_level_(3)
    , escalation_rules_("{}")
    , category_("")
    , tags_("[]") {
    
    LogManager::getInstance().Debug("AlarmRuleEntity 기본 생성자 호출 - 모든 필드 초기화 완료");
}

AlarmRuleEntity::AlarmRuleEntity(int alarm_id) 
    : AlarmRuleEntity() {  // 위임 생성자 사용
    setId(alarm_id);
    LogManager::getInstance().Debug("AlarmRuleEntity ID 생성자 호출 - ID: " + std::to_string(alarm_id));
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (Repository 활용)
// =============================================================================

bool AlarmRuleEntity::loadFromDatabase() {
    if (getId() <= 0) {
        LogManager::getInstance().Error("AlarmRuleEntity::loadFromDatabase - Invalid alarm rule ID: " + std::to_string(getId()));
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
                LogManager::getInstance().Info("AlarmRuleEntity::loadFromDatabase - Loaded alarm rule: " + name_ + 
                                             " (category: " + category_ + ")");
                return true;
            }
        }
        
        LogManager::getInstance().Warn("AlarmRuleEntity::loadFromDatabase - Alarm rule not found: " + std::to_string(getId()));
        return false;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("AlarmRuleEntity::loadFromDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool AlarmRuleEntity::saveToDatabase() {
    try {
        // 저장 전 타임스탬프 업데이트
        auto now = std::chrono::system_clock::now();
        if (getId() <= 0) {
            created_at_ = now;
        }
        updated_at_ = now;
        
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmRuleRepository();
        if (repo) {
            bool success = false;
            if (getId() <= 0) {
                success = repo->save(*this);
            } else {
                success = repo->update(*this);
            }
            
            if (success) {
                markSaved();
                LogManager::getInstance().Info("AlarmRuleEntity::saveToDatabase - Saved alarm rule: " + name_ + 
                                             " (category: " + category_ + ", escalation: " + 
                                             (escalation_enabled_ ? "enabled" : "disabled") + ")");
            }
            return success;
        }
        
        LogManager::getInstance().Error("AlarmRuleEntity::saveToDatabase - Repository not available");
        return false;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("AlarmRuleEntity::saveToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool AlarmRuleEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        LogManager::getInstance().Error("AlarmRuleEntity::deleteFromDatabase - Invalid alarm rule ID: " + std::to_string(getId()));
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmRuleRepository();
        if (repo) {
            bool success = repo->deleteById(getId());
            if (success) {
                LogManager::getInstance().Info("AlarmRuleEntity::deleteFromDatabase - Deleted alarm rule: " + name_ + 
                                             " (ID: " + std::to_string(getId()) + ")");
                setId(-1); // 삭제 후 ID 리셋
            }
            return success;
        }
        
        LogManager::getInstance().Error("AlarmRuleEntity::deleteFromDatabase - Repository not available");
        return false;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("AlarmRuleEntity::deleteFromDatabase failed: " + std::string(e.what()));
        return false;
    }
}

bool AlarmRuleEntity::updateToDatabase() {
    if (getId() <= 0) {
        LogManager::getInstance().Error("AlarmRuleEntity::updateToDatabase - Invalid alarm rule ID: " + std::to_string(getId()));
        return false;
    }
    
    try {
        // 업데이트 타임스탬프 갱신
        updated_at_ = std::chrono::system_clock::now();
        
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmRuleRepository();
        if (repo) {
            bool success = repo->update(*this);
            if (success) {
                markSaved();
                LogManager::getInstance().Info("AlarmRuleEntity::updateToDatabase - Updated alarm rule: " + name_ + 
                                             " (category: " + category_ + ")");
            }
            return success;
        }
        
        LogManager::getInstance().Error("AlarmRuleEntity::updateToDatabase - Repository not available");
        return false;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("AlarmRuleEntity::updateToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

// =============================================================================
// 비즈니스 로직 메서드 구현 - 새 필드 활용
// =============================================================================

std::string AlarmRuleEntity::generateMessage(double value, const std::string& unit) const {
    if (!message_template_.empty()) {
        return interpolateTemplate(message_template_, value, unit);
    }
    
    // 기본 메시지 생성 - 카테고리 정보 포함
    std::ostringstream oss;
    oss << "ALARM: " << name_;
    
    // 카테고리 정보 추가
    if (!category_.empty()) {
        oss << " [" << category_ << "]";
    }
    
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
    
    oss << " - Severity: " << PulseOne::Alarm::severityToString(severity_);
    
    // 에스컬레이션 정보 추가
    if (escalation_enabled_) {
        oss << " - Escalation: Level 1/" << escalation_max_level_;
    }
    
    return oss.str();
}

std::string AlarmRuleEntity::generateDigitalMessage(bool state) const {
    if (!message_template_.empty()) {
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
    
    // 기본 메시지 생성 - 카테고리 정보 포함
    std::ostringstream oss;
    oss << "ALARM: " << name_;
    
    if (!category_.empty()) {
        oss << " [" << category_ << "]";
    }
    
    oss << " - Digital State: " << (state ? "TRUE" : "FALSE");
    oss << " - Trigger: " << PulseOne::Alarm::digitalTriggerToString(trigger_condition_);
    oss << " - Severity: " << PulseOne::Alarm::severityToString(severity_);
    
    if (escalation_enabled_) {
        oss << " - Escalation: Enabled";
    }
    
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
        auto rules = json::parse(suppression_rules_);
        auto context = json::parse(context_json);
        
        // 시간 기반 억제 체크
        if (rules.contains("time_based")) {
            auto time_rules = rules["time_based"];
            if (time_rules.is_array()) {
                for (const auto& time_rule : time_rules) {
                    if (time_rule.contains("start") && time_rule.contains("end")) {
                        // 현재 시간이 억제 시간 범위에 있는지 체크
                        auto now = std::chrono::system_clock::now();
                        auto time_t = std::chrono::system_clock::to_time_t(now);
                        struct tm* tm_info = std::localtime(&time_t);
                        
                        int current_hour = tm_info->tm_hour;
                        // 간단한 시간 범위 체크 구현
                        if (time_rule.contains("start") && time_rule.contains("end")) {
                            std::string start_str = time_rule["start"];
                            std::string end_str = time_rule["end"];
                            
                            // "HH:MM" 형식 파싱 후 비교 (상세 구현 필요)
                            LogManager::getInstance().Debug("Time-based suppression check: " + start_str + " - " + end_str);
                        }
                    }
                }
            }
        }
        
        // 조건 기반 억제 체크
        if (rules.contains("condition_based")) {
            auto condition_rules = rules["condition_based"];
            if (condition_rules.is_array()) {
                for (const auto& condition_rule : condition_rules) {
                    if (condition_rule.contains("point_id") && condition_rule.contains("condition")) {
                        // 조건 기반 억제 로직 (상세 구현 필요)
                        LogManager::getInstance().Debug("Condition-based suppression check");
                    }
                }
            }
        }
        
        return false; // 기본적으로 억제하지 않음
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Warn("AlarmRuleEntity::checkSuppressionRules - JSON parsing failed: " + std::string(e.what()));
        return false;
    }
}

int AlarmRuleEntity::getSeverityLevel() const {
    return static_cast<int>(severity_);
}

// =============================================================================
// 내부 헬퍼 메서드 구현 - 새 필드 지원
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
    
    // {{CATEGORY}} 치환 (새로 추가)
    pos = result.find("{{CATEGORY}}");
    if (pos != std::string::npos) {
        result.replace(pos, 12, category_.empty() ? "General" : category_);
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
        result.replace(pos, 12, PulseOne::Alarm::severityToString(severity_));
    }
    
    // {{TIMESTAMP}} 치환
    pos = result.find("{{TIMESTAMP}}");
    if (pos != std::string::npos) {
        result.replace(pos, 13, timestampToString(std::chrono::system_clock::now()));
    }
    
    // {{ESCALATION_LEVEL}} 치환 (새로 추가)
    pos = result.find("{{ESCALATION_LEVEL}}");
    if (pos != std::string::npos) {
        std::string escalation_info = escalation_enabled_ ? 
            ("1/" + std::to_string(escalation_max_level_)) : "Disabled";
        result.replace(pos, 20, escalation_info);
    }
    
    // {{RULE_GROUP}} 치환 (새로 추가)
    pos = result.find("{{RULE_GROUP}}");
    if (pos != std::string::npos) {
        result.replace(pos, 14, rule_group_.empty() ? "Default" : rule_group_);
    }
    
    // {{TAGS}} 치환 (새로 추가)
    pos = result.find("{{TAGS}}");
    if (pos != std::string::npos) {
        std::string tags_str = "None";
        if (!tags_.empty() && tags_ != "[]") {
            try {
                auto tags_array = json::parse(tags_);
                if (tags_array.is_array() && !tags_array.empty()) {
                    std::ostringstream tags_oss;
                    for (size_t i = 0; i < tags_array.size(); ++i) {
                        if (i > 0) tags_oss << ", ";
                        tags_oss << tags_array[i].get<std::string>();
                    }
                    tags_str = tags_oss.str();
                }
            } catch (...) {
                // JSON 파싱 실패 시 기본값 사용
            }
        }
        result.replace(pos, 8, tags_str);
    }
    
    return result;
}

// =============================================================================
// 새로 추가된 비즈니스 로직 메서드들
// =============================================================================

bool AlarmRuleEntity::hasEscalationRules() const {
    if (!escalation_enabled_ || escalation_rules_.empty() || escalation_rules_ == "{}") {
        return false;
    }
    
    try {
        auto rules = json::parse(escalation_rules_);
        return rules.contains("levels") && rules["levels"].is_array() && !rules["levels"].empty();
    } catch (...) {
        return false;
    }
}

std::vector<std::string> AlarmRuleEntity::getTagsList() const {
    std::vector<std::string> result;
    
    if (tags_.empty() || tags_ == "[]") {
        return result;
    }
    
    try {
        auto tags_array = json::parse(tags_);
        if (tags_array.is_array()) {
            for (const auto& tag : tags_array) {
                if (tag.is_string()) {
                    result.push_back(tag.get<std::string>());
                }
            }
        }
    } catch (const std::exception& e) {
        LogManager::getInstance().Warn("AlarmRuleEntity::getTagsList - JSON parsing failed: " + std::string(e.what()));
    }
    
    return result;
}

void AlarmRuleEntity::addTag(const std::string& tag) {
    if (tag.empty()) return;
    
    auto current_tags = getTagsList();
    
    // 중복 체크
    for (const auto& existing_tag : current_tags) {
        if (existing_tag == tag) {
            return; // 이미 존재함
        }
    }
    
    // 태그 추가
    current_tags.push_back(tag);
    
    // JSON 배열로 다시 변환
    json tags_array = json::array();
    for (const auto& t : current_tags) {
        tags_array.push_back(t);
    }
    
    tags_ = tags_array.dump();
    markModified();
    
    LogManager::getInstance().Debug("AlarmRuleEntity::addTag - Added tag: " + tag + " to rule: " + name_);
}

void AlarmRuleEntity::removeTag(const std::string& tag) {
    if (tag.empty()) return;
    
    auto current_tags = getTagsList();
    auto it = std::find(current_tags.begin(), current_tags.end(), tag);
    
    if (it != current_tags.end()) {
        current_tags.erase(it);
        
        // JSON 배열로 다시 변환
        json tags_array = json::array();
        for (const auto& t : current_tags) {
            tags_array.push_back(t);
        }
        
        tags_ = tags_array.dump();
        markModified();
        
        LogManager::getInstance().Debug("AlarmRuleEntity::removeTag - Removed tag: " + tag + " from rule: " + name_);
    }
}

bool AlarmRuleEntity::hasTag(const std::string& tag) const {
    auto current_tags = getTagsList();
    return std::find(current_tags.begin(), current_tags.end(), tag) != current_tags.end();
}

std::string AlarmRuleEntity::getCategoryDisplayName() const {
    if (category_.empty()) {
        return "General";
    }
    
    // 카테고리 표시명 매핑
    static const std::map<std::string, std::string> category_display_names = {
        {"temperature", "Temperature"},
        {"pressure", "Pressure"}, 
        {"flow", "Flow"},
        {"level", "Level"},
        {"vibration", "Vibration"},
        {"electrical", "Electrical"},
        {"safety", "Safety"},
        {"process", "Process"},
        {"system", "System"},
        {"custom", "Custom"},
        {"general", "General"}
    };
    
    auto it = category_display_names.find(category_);
    return (it != category_display_names.end()) ? it->second : category_;
}

std::shared_ptr<Repositories::AlarmRuleRepository> AlarmRuleEntity::getRepository() const {
    return RepositoryFactory::getInstance().getAlarmRuleRepository();
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne