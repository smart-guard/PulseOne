// =============================================================================
// collector/src/Database/Entities/AlarmOccurrenceEntity.cpp
// PulseOne AlarmOccurrenceEntity 구현 - BaseEntity 패턴 100% 준수
// =============================================================================

/**
 * @file AlarmOccurrenceEntity.cpp
 * @brief PulseOne 알람 발생 이력 엔티티 구현
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
// BaseEntity 순수 가상 함수 구현 (필수!)
// =============================================================================

bool AlarmOccurrenceEntity::loadFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::loadFromDatabase - Invalid occurrence ID: " + std::to_string(getId()));
        }
        return false;
    }
    
    try {
        // 🔥 임시로 직접 SQL 실행 (Repository 추가 전까지)
        std::string query = "SELECT * FROM " + getTableName() + " WHERE id = " + std::to_string(getId());
        
        auto results = executeUnifiedQuery(query);
        if (results.empty()) {
            if (logger_) {
                logger_->Warn("AlarmOccurrenceEntity::loadFromDatabase - No data found for ID: " + std::to_string(getId()));
            }
            return false;
        }
        
        const auto& row = results[0];
        
        // 데이터 매핑
        rule_id_ = std::stoi(getValueOrDefault(row, "rule_id", "0"));
        tenant_id_ = std::stoi(getValueOrDefault(row, "tenant_id", "0"));
        trigger_value_ = getValueOrDefault(row, "trigger_value", "");
        trigger_condition_ = getValueOrDefault(row, "trigger_condition", "");
        alarm_message_ = getValueOrDefault(row, "alarm_message", "");
        severity_ = stringToSeverity(getValueOrDefault(row, "severity", "medium"));
        state_ = stringToState(getValueOrDefault(row, "state", "active"));
        
        // Optional 필드들
        auto ack_time_str = getValueOrDefault(row, "acknowledged_time", "");
        if (!ack_time_str.empty() && ack_time_str != "NULL") {
            acknowledged_time_ = stringToTimestamp(ack_time_str);
        }
        
        auto ack_by_str = getValueOrDefault(row, "acknowledged_by", "");
        if (!ack_by_str.empty() && ack_by_str != "NULL") {
            acknowledged_by_ = std::stoi(ack_by_str);
        }
        
        acknowledge_comment_ = getValueOrDefault(row, "acknowledge_comment", "");
        
        auto cleared_time_str = getValueOrDefault(row, "cleared_time", "");
        if (!cleared_time_str.empty() && cleared_time_str != "NULL") {
            cleared_time_ = stringToTimestamp(cleared_time_str);
        }
        
        cleared_value_ = getValueOrDefault(row, "cleared_value", "");
        clear_comment_ = getValueOrDefault(row, "clear_comment", "");
        
        notification_sent_ = (getValueOrDefault(row, "notification_sent", "0") == "1");
        
        auto notif_time_str = getValueOrDefault(row, "notification_time", "");
        if (!notif_time_str.empty() && notif_time_str != "NULL") {
            notification_time_ = stringToTimestamp(notif_time_str);
        }
        
        notification_count_ = std::stoi(getValueOrDefault(row, "notification_count", "0"));
        notification_result_ = getValueOrDefault(row, "notification_result", "");
        context_data_ = getValueOrDefault(row, "context_data", "");
        source_name_ = getValueOrDefault(row, "source_name", "");
        location_ = getValueOrDefault(row, "location", "");
        
        // 시간 필드 처리
        auto occ_time_str = getValueOrDefault(row, "occurrence_time", "");
        if (!occ_time_str.empty()) {
            occurrence_time_ = stringToTimestamp(occ_time_str);
        }
        
        markLoaded();  // BaseEntity 패턴
        
        if (logger_) {
            logger_->Info("AlarmOccurrenceEntity::loadFromDatabase - Loaded occurrence: " + std::to_string(getId()));
        }
        
        return true;
        
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
            logger_->Error("AlarmOccurrenceEntity::saveToDatabase - Invalid occurrence data");
        }
        return false;
    }
    
    try {
        // INSERT 쿼리 생성
        std::ostringstream query;
        query << "INSERT INTO " << getTableName() << " (";
        query << "rule_id, tenant_id, occurrence_time, trigger_value, trigger_condition, ";
        query << "alarm_message, severity, state, acknowledge_comment, cleared_value, ";
        query << "clear_comment, notification_sent, notification_count, notification_result, ";
        query << "context_data, source_name, location";
        
        // Optional 필드들 조건부 추가
        if (acknowledged_time_.has_value()) query << ", acknowledged_time";
        if (acknowledged_by_.has_value()) query << ", acknowledged_by";
        if (cleared_time_.has_value()) query << ", cleared_time";
        if (notification_time_.has_value()) query << ", notification_time";
        
        query << ") VALUES (";
        query << rule_id_ << ", " << tenant_id_ << ", ";
        query << "'" << timestampToString(occurrence_time_) << "', ";
        query << "'" << trigger_value_ << "', ";
        query << "'" << trigger_condition_ << "', ";
        query << "'" << alarm_message_ << "', ";
        query << "'" << severityToString(severity_) << "', ";
        query << "'" << stateToString(state_) << "', ";
        query << "'" << acknowledge_comment_ << "', ";
        query << "'" << cleared_value_ << "', ";
        query << "'" << clear_comment_ << "', ";
        query << (notification_sent_ ? 1 : 0) << ", ";
        query << notification_count_ << ", ";
        query << "'" << notification_result_ << "', ";
        query << "'" << context_data_ << "', ";
        query << "'" << source_name_ << "', ";
        query << "'" << location_ << "'";
        
        // Optional 필드들 값 추가
        if (acknowledged_time_.has_value()) {
            query << ", '" << timestampToString(acknowledged_time_.value()) << "'";
        }
        if (acknowledged_by_.has_value()) {
            query << ", " << acknowledged_by_.value();
        }
        if (cleared_time_.has_value()) {
            query << ", '" << timestampToString(cleared_time_.value()) << "'";
        }
        if (notification_time_.has_value()) {
            query << ", '" << timestampToString(notification_time_.value()) << "'";
        }
        
        query << ")";
        
        bool success = executeUnifiedNonQuery(query.str());
        
        if (success) {
            // 생성된 ID 가져오기
            auto last_id = getLastInsertId();
            if (last_id > 0) {
                setId(static_cast<int>(last_id));
            }
            
            markSaved();  // BaseEntity 패턴
            
            if (logger_) {
                logger_->Info("AlarmOccurrenceEntity::saveToDatabase - Saved occurrence: " + std::to_string(getId()));
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
        std::string query = "DELETE FROM " + getTableName() + " WHERE id = " + std::to_string(getId());
        
        bool success = executeUnifiedNonQuery(query);
        
        if (success) {
            markDeleted();  // BaseEntity 패턴
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
        std::ostringstream query;
        query << "UPDATE " << getTableName() << " SET ";
        query << "rule_id = " << rule_id_ << ", ";
        query << "tenant_id = " << tenant_id_ << ", ";
        query << "occurrence_time = '" << timestampToString(occurrence_time_) << "', ";
        query << "trigger_value = '" << trigger_value_ << "', ";
        query << "trigger_condition = '" << trigger_condition_ << "', ";
        query << "alarm_message = '" << alarm_message_ << "', ";
        query << "severity = '" << severityToString(severity_) << "', ";
        query << "state = '" << stateToString(state_) << "', ";
        query << "acknowledge_comment = '" << acknowledge_comment_ << "', ";
        query << "cleared_value = '" << cleared_value_ << "', ";
        query << "clear_comment = '" << clear_comment_ << "', ";
        query << "notification_sent = " << (notification_sent_ ? 1 : 0) << ", ";
        query << "notification_count = " << notification_count_ << ", ";
        query << "notification_result = '" << notification_result_ << "', ";
        query << "context_data = '" << context_data_ << "', ";
        query << "source_name = '" << source_name_ << "', ";
        query << "location = '" << location_ << "'";
        
        // Optional 필드들
        if (acknowledged_time_.has_value()) {
            query << ", acknowledged_time = '" << timestampToString(acknowledged_time_.value()) << "'";
        } else {
            query << ", acknowledged_time = NULL";
        }
        
        if (acknowledged_by_.has_value()) {
            query << ", acknowledged_by = " << acknowledged_by_.value();
        } else {
            query << ", acknowledged_by = NULL";
        }
        
        if (cleared_time_.has_value()) {
            query << ", cleared_time = '" << timestampToString(cleared_time_.value()) << "'";
        } else {
            query << ", cleared_time = NULL";
        }
        
        if (notification_time_.has_value()) {
            query << ", notification_time = '" << timestampToString(notification_time_.value()) << "'";
        } else {
            query << ", notification_time = NULL";
        }
        
        query << " WHERE id = " << getId();
        
        bool success = executeUnifiedNonQuery(query.str());
        
        if (success) {
            markSaved();  // BaseEntity 패턴
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