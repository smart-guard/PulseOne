// =============================================================================
// collector/src/Database/Entities/AlarmOccurrenceEntity.cpp
// PulseOne AlarmOccurrenceEntity 구현 - DataPointEntity 패턴 100% 준수
// =============================================================================

/**
 * @file AlarmOccurrenceEntity.cpp
 * @brief PulseOne 알람 발생 이력 엔티티 구현 - DataPointEntity 패턴 완성
 * @author PulseOne Development Team
 * @date 2025-08-10
 */

#include "Database/Entities/AlarmOccurrenceEntity.h"
#include "Database/RepositoryFactory.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 및 소멸자
// =============================================================================

AlarmOccurrenceEntity::AlarmOccurrenceEntity()
    : BaseEntity<AlarmOccurrenceEntity>()
    , rule_id_(0)
    , tenant_id_(0)
    , occurrence_time_(std::chrono::system_clock::now())
    , trigger_value_("")
    , trigger_condition_("")
    , alarm_message_("")
    , severity_(Severity::MEDIUM)
    , state_(State::ACTIVE)
    , acknowledge_comment_("")
    , cleared_value_("")
    , clear_comment_("")
    , notification_sent_(false)
    , notification_count_(0)
    , notification_result_("")
    , context_data_("")
    , source_name_("")
    , location_("") {
}

AlarmOccurrenceEntity::AlarmOccurrenceEntity(int occurrence_id) 
    : AlarmOccurrenceEntity() {  // 위임 생성자 사용
    setId(occurrence_id);
}

AlarmOccurrenceEntity::AlarmOccurrenceEntity(int rule_id, int tenant_id, const std::string& trigger_value, 
                                           const std::string& alarm_message, Severity severity)
    : AlarmOccurrenceEntity() {  // 위임 생성자 사용
    
    rule_id_ = rule_id;
    tenant_id_ = tenant_id;
    trigger_value_ = trigger_value;
    alarm_message_ = alarm_message;
    severity_ = severity;
    occurrence_time_ = std::chrono::system_clock::now();
    state_ = State::ACTIVE;
    
    markModified();  // BaseEntity 패턴: markModified() 사용
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (DataPointEntity 패턴 100% 적용)
// =============================================================================

bool AlarmOccurrenceEntity::loadFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::loadFromDatabase - Invalid occurrence ID: " + std::to_string(getId()));
        }
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmOccurrenceRepository();
        
        if (!repo) {
            if (logger_) {
                logger_->Error("AlarmOccurrenceEntity::loadFromDatabase - AlarmOccurrenceRepository not available");
            }
            markError();
            return false;
        }
        
        auto loaded = repo->findById(getId());
        if (loaded.has_value()) {
            // 로드된 데이터를 현재 객체에 복사
            *this = loaded.value();
            markSaved();
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceEntity::loadFromDatabase - Loaded alarm occurrence: " + std::to_string(getId()));
            }
            return true;
        } else {
            if (logger_) {
                logger_->Warn("AlarmOccurrenceEntity::loadFromDatabase - Alarm occurrence not found: " + std::to_string(getId()));
            }
            return false;
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::loadFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool AlarmOccurrenceEntity::saveToDatabase() {
    if (!isValid()) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::saveToDatabase - Invalid alarm occurrence data");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmOccurrenceRepository();
        
        if (!repo) {
            if (logger_) {
                logger_->Error("AlarmOccurrenceEntity::saveToDatabase - AlarmOccurrenceRepository not available");
            }
            return false;
        }
        
        // Repository의 save 메서드 호출 (Entity를 참조로 전달)
        AlarmOccurrenceEntity mutable_copy = *this;
        bool success = repo->save(mutable_copy);
        
        if (success) {
            // 저장 성공 시 ID와 상태 업데이트
            if (getId() <= 0) {
                setId(mutable_copy.getId());
            }
            markSaved();
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceEntity::saveToDatabase - Saved alarm occurrence: " + std::to_string(getId()));
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::saveToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool AlarmOccurrenceEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::deleteFromDatabase - Invalid occurrence ID: " + std::to_string(getId()));
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmOccurrenceRepository();
        
        if (!repo) {
            if (logger_) {
                logger_->Error("AlarmOccurrenceEntity::deleteFromDatabase - AlarmOccurrenceRepository not available");
            }
            return false;
        }
        
        bool success = repo->deleteById(getId());
        
        if (success) {
            markDeleted();
            if (logger_) {
                logger_->Info("AlarmOccurrenceEntity::deleteFromDatabase - Deleted occurrence: " + std::to_string(getId()));
            }
            setId(-1); // 삭제 후 ID 리셋
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::deleteFromDatabase failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool AlarmOccurrenceEntity::updateToDatabase() {
    if (getId() <= 0 || !isValid()) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::updateToDatabase - Invalid occurrence data or ID");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmOccurrenceRepository();
        
        if (!repo) {
            if (logger_) {
                logger_->Error("AlarmOccurrenceEntity::updateToDatabase - AlarmOccurrenceRepository not available");
            }
            return false;
        }
        
        bool success = repo->update(*this);
        
        if (success) {
            markSaved();
            if (logger_) {
                logger_->Info("AlarmOccurrenceEntity::updateToDatabase - Updated occurrence: " + std::to_string(getId()));
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::updateToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

// =============================================================================
// JSON 직렬화/역직렬화 (BaseEntity 패턴 - json 타입 사용)
// =============================================================================

json AlarmOccurrenceEntity::toJson() const {
    json j;
    
    j["id"] = getId();
    j["rule_id"] = rule_id_;
    j["tenant_id"] = tenant_id_;
    j["occurrence_time"] = timestampToString(occurrence_time_);
    j["trigger_value"] = trigger_value_;
    j["trigger_condition"] = trigger_condition_;
    j["alarm_message"] = alarm_message_;
    j["severity"] = severityToString(severity_);
    j["state"] = stateToString(state_);
    
    // Optional 필드들
    if (acknowledged_time_.has_value()) {
        j["acknowledged_time"] = timestampToString(acknowledged_time_.value());
    } else {
        j["acknowledged_time"] = nullptr;
    }
    
    if (acknowledged_by_.has_value()) {
        j["acknowledged_by"] = acknowledged_by_.value();
    } else {
        j["acknowledged_by"] = nullptr;
    }
    
    j["acknowledge_comment"] = acknowledge_comment_;
    
    if (cleared_time_.has_value()) {
        j["cleared_time"] = timestampToString(cleared_time_.value());
    } else {
        j["cleared_time"] = nullptr;
    }
    
    j["cleared_value"] = cleared_value_;
    j["clear_comment"] = clear_comment_;
    j["notification_sent"] = notification_sent_;
    
    if (notification_time_.has_value()) {
        j["notification_time"] = timestampToString(notification_time_.value());
    } else {
        j["notification_time"] = nullptr;
    }
    
    j["notification_count"] = notification_count_;
    j["notification_result"] = notification_result_;
    j["context_data"] = context_data_;
    j["source_name"] = source_name_;
    j["location"] = location_;
    
    return j;
}

bool AlarmOccurrenceEntity::fromJson(const json& data) {
    try {
        if (data.contains("id")) {
            setId(data["id"].get<int>());
        }
        if (data.contains("rule_id")) {
            rule_id_ = data["rule_id"].get<int>();
        }
        if (data.contains("tenant_id")) {
            tenant_id_ = data["tenant_id"].get<int>();
        }
        if (data.contains("occurrence_time")) {
            auto time_str = data["occurrence_time"].get<std::string>();
            occurrence_time_ = stringToTimestamp(time_str);
        }
        if (data.contains("trigger_value")) {
            trigger_value_ = data["trigger_value"].get<std::string>();
        }
        if (data.contains("trigger_condition")) {
            trigger_condition_ = data["trigger_condition"].get<std::string>();
        }
        if (data.contains("alarm_message")) {
            alarm_message_ = data["alarm_message"].get<std::string>();
        }
        if (data.contains("severity")) {
            severity_ = stringToSeverity(data["severity"].get<std::string>());
        }
        if (data.contains("state")) {
            state_ = stringToState(data["state"].get<std::string>());
        }
        
        // Optional 필드들
        if (data.contains("acknowledged_time") && !data["acknowledged_time"].is_null()) {
            acknowledged_time_ = stringToTimestamp(data["acknowledged_time"].get<std::string>());
        }
        if (data.contains("acknowledged_by") && !data["acknowledged_by"].is_null()) {
            acknowledged_by_ = data["acknowledged_by"].get<int>();
        }
        if (data.contains("acknowledge_comment")) {
            acknowledge_comment_ = data["acknowledge_comment"].get<std::string>();
        }
        if (data.contains("cleared_time") && !data["cleared_time"].is_null()) {
            cleared_time_ = stringToTimestamp(data["cleared_time"].get<std::string>());
        }
        if (data.contains("cleared_value")) {
            cleared_value_ = data["cleared_value"].get<std::string>();
        }
        if (data.contains("clear_comment")) {
            clear_comment_ = data["clear_comment"].get<std::string>();
        }
        if (data.contains("notification_sent")) {
            notification_sent_ = data["notification_sent"].get<bool>();
        }
        if (data.contains("notification_time") && !data["notification_time"].is_null()) {
            notification_time_ = stringToTimestamp(data["notification_time"].get<std::string>());
        }
        if (data.contains("notification_count")) {
            notification_count_ = data["notification_count"].get<int>();
        }
        if (data.contains("notification_result")) {
            notification_result_ = data["notification_result"].get<std::string>();
        }
        if (data.contains("context_data")) {
            context_data_ = data["context_data"].get<std::string>();
        }
        if (data.contains("source_name")) {
            source_name_ = data["source_name"].get<std::string>();
        }
        if (data.contains("location")) {
            location_ = data["location"].get<std::string>();
        }
        
        markModified();  // BaseEntity 패턴
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::fromJson failed: " + std::string(e.what()));
        }
        return false;
    }
}

std::string AlarmOccurrenceEntity::toString() const {
    std::ostringstream ss;
    ss << "AlarmOccurrence[id=" << getId()
       << ", rule_id=" << rule_id_
       << ", tenant_id=" << tenant_id_
       << ", severity=" << severityToString(severity_)
       << ", state=" << stateToString(state_)
       << ", message=\"" << alarm_message_ << "\""
       << ", elapsed=" << getElapsedSeconds() << "s]";
    return ss.str();
}

bool AlarmOccurrenceEntity::isValid() const {
    // 기본 유효성 검사
    if (rule_id_ <= 0) return false;
    if (tenant_id_ <= 0) return false;
    if (alarm_message_.empty()) return false;
    
    return true;
}

// =============================================================================
// 비즈니스 로직 메서드들
// =============================================================================

bool AlarmOccurrenceEntity::acknowledge(int user_id, const std::string& comment) {
    try {
        if (state_ == State::ACKNOWLEDGED) {
            return true; // 이미 인지됨
        }
        
        acknowledged_time_ = std::chrono::system_clock::now();
        acknowledged_by_ = user_id;
        acknowledge_comment_ = comment;
        state_ = State::ACKNOWLEDGED;
        markModified();
        
        // 데이터베이스에 즉시 반영
        bool success = updateToDatabase();
        
        if (success && logger_) {
            logger_->Info("AlarmOccurrenceEntity::acknowledge - Occurrence " + std::to_string(getId()) + " acknowledged by user " + std::to_string(user_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::acknowledge failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool AlarmOccurrenceEntity::clear(const std::string& cleared_value, const std::string& comment) {
    try {
        if (state_ == State::CLEARED) {
            return true; // 이미 해제됨
        }
        
        cleared_time_ = std::chrono::system_clock::now();
        cleared_value_ = cleared_value;
        clear_comment_ = comment;
        state_ = State::CLEARED;
        markModified();
        
        // 데이터베이스에 즉시 반영
        bool success = updateToDatabase();
        
        if (success && logger_) {
            logger_->Info("AlarmOccurrenceEntity::clear - Occurrence " + std::to_string(getId()) + " cleared");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::clear failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool AlarmOccurrenceEntity::changeState(State new_state) {
    try {
        if (state_ == new_state) {
            return true; // 이미 해당 상태
        }
        
        State old_state = state_;
        state_ = new_state;
        markModified();
        
        // 데이터베이스에 즉시 반영
        bool success = updateToDatabase();
        
        if (success && logger_) {
            logger_->Info("AlarmOccurrenceEntity::changeState - Occurrence " + std::to_string(getId()) + 
                         " state changed: " + stateToString(old_state) + " → " + stateToString(new_state));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::changeState failed: " + std::string(e.what()));
        }
        return false;
    }
}

void AlarmOccurrenceEntity::markNotificationSent(const std::string& result_json) {
    notification_sent_ = true;
    notification_time_ = std::chrono::system_clock::now();
    notification_result_ = result_json;
    markModified();
    
    if (logger_) {
        logger_->Debug("AlarmOccurrenceEntity::markNotificationSent - Occurrence " + std::to_string(getId()));
    }
}

void AlarmOccurrenceEntity::incrementNotificationCount() {
    notification_count_++;
    markModified();
    
    if (logger_) {
        logger_->Debug("AlarmOccurrenceEntity::incrementNotificationCount - Occurrence " + std::to_string(getId()) + 
                      " count: " + std::to_string(notification_count_));
    }
}

long long AlarmOccurrenceEntity::getElapsedSeconds() const {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - occurrence_time_);
    return duration.count();
}

// =============================================================================
// 정적 유틸리티 메서드들
// =============================================================================

std::string AlarmOccurrenceEntity::severityToString(Severity severity) {
    switch (severity) {
        case Severity::LOW: return "low";
        case Severity::MEDIUM: return "medium";
        case Severity::HIGH: return "high";
        case Severity::CRITICAL: return "critical";
        default: return "medium";
    }
}

AlarmOccurrenceEntity::Severity AlarmOccurrenceEntity::stringToSeverity(const std::string& str) {
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
    
    if (lower_str == "low") return Severity::LOW;
    if (lower_str == "medium") return Severity::MEDIUM;
    if (lower_str == "high") return Severity::HIGH;
    if (lower_str == "critical") return Severity::CRITICAL;
    
    return Severity::MEDIUM; // 기본값
}

std::string AlarmOccurrenceEntity::stateToString(State state) {
    switch (state) {
        case State::ACTIVE: return "active";
        case State::ACKNOWLEDGED: return "acknowledged";
        case State::CLEARED: return "cleared";
        default: return "active";
    }
}

AlarmOccurrenceEntity::State AlarmOccurrenceEntity::stringToState(const std::string& str) {
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
    
    if (lower_str == "active") return State::ACTIVE;
    if (lower_str == "acknowledged") return State::ACKNOWLEDGED;
    if (lower_str == "cleared") return State::CLEARED;
    
    return State::ACTIVE; // 기본값
}

// =============================================================================
// 헬퍼 메서드들
// =============================================================================

std::string AlarmOccurrenceEntity::timestampToString(const std::chrono::system_clock::time_point& tp) const {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::chrono::system_clock::time_point AlarmOccurrenceEntity::stringToTimestamp(const std::string& str) const {
    std::tm tm = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    
    auto time_t = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time_t);
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne