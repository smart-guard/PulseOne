// =============================================================================
// collector/src/Database/Entities/AlarmOccurrenceEntity.cpp
// PulseOne AlarmOccurrenceEntity 구현 - BaseEntity 패턴 적용
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
#include <regex>
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
    , location_("")
    , logger_(nullptr)
    , config_manager_(nullptr) {
    
    initializeDependencies();
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
    
    markDirty();  // 새로 생성된 엔티티이므로 dirty 마크
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (Repository 활용)
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
        if (repo) {
            auto loaded = repo->findById(getId());
            if (loaded.has_value()) {
                *this = loaded.value();
                markSaved();
                if (logger_) {
                    logger_->Info("AlarmOccurrenceEntity::loadFromDatabase - Loaded occurrence: " + std::to_string(getId()));
                }
                return true;
            }
        }
        
        if (logger_) {
            logger_->Warn("AlarmOccurrenceEntity::loadFromDatabase - Occurrence not found: " + std::to_string(getId()));
        }
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::loadFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool AlarmOccurrenceEntity::saveToDatabase() {
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmOccurrenceRepository();
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
                    logger_->Info("AlarmOccurrenceEntity::saveToDatabase - Saved occurrence: " + std::to_string(getId()));
                }
            }
            return success;
        }
        
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::saveToDatabase - Repository not available");
        }
        return false;
        
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
        if (repo) {
            bool success = repo->deleteById(getId());
            if (success) {
                if (logger_) {
                    logger_->Info("AlarmOccurrenceEntity::deleteFromDatabase - Deleted occurrence: " + std::to_string(getId()));
                }
                setId(-1); // 삭제 후 ID 리셋
            }
            return success;
        }
        
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::deleteFromDatabase - Repository not available");
        }
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::deleteFromDatabase failed: " + std::string(e.what()));
        }
        return false;
    }
}

bool AlarmOccurrenceEntity::updateToDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::updateToDatabase - Invalid occurrence ID: " + std::to_string(getId()));
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmOccurrenceRepository();
        if (repo) {
            bool success = repo->update(*this);
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("AlarmOccurrenceEntity::updateToDatabase - Updated occurrence: " + std::to_string(getId()));
                }
            }
            return success;
        }
        
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::updateToDatabase - Repository not available");
        }
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::updateToDatabase failed: " + std::string(e.what()));
        }
        return false;
    }
}

// =============================================================================
// 열거형 변환 유틸리티 메서드들
// =============================================================================

std::string AlarmOccurrenceEntity::stateToString(State state) {
    switch (state) {
        case State::ACTIVE:       return "active";
        case State::ACKNOWLEDGED: return "acknowledged";
        case State::CLEARED:      return "cleared";
        case State::SUPPRESSED:   return "suppressed";
        case State::SHELVED:      return "shelved";
        default:                  return "unknown";
    }
}

AlarmOccurrenceEntity::State AlarmOccurrenceEntity::stringToState(const std::string& state_str) {
    std::string lower_state = state_str;
    std::transform(lower_state.begin(), lower_state.end(), lower_state.begin(), ::tolower);
    
    if (lower_state == "active")       return State::ACTIVE;
    if (lower_state == "acknowledged") return State::ACKNOWLEDGED;
    if (lower_state == "cleared")      return State::CLEARED;
    if (lower_state == "suppressed")   return State::SUPPRESSED;
    if (lower_state == "shelved")      return State::SHELVED;
    
    return State::ACTIVE; // 기본값
}

std::vector<std::string> AlarmOccurrenceEntity::getAllStateStrings() {
    return {"active", "acknowledged", "cleared", "suppressed", "shelved"};
}

std::string AlarmOccurrenceEntity::severityToString(Severity severity) {
    switch (severity) {
        case Severity::CRITICAL: return "critical";
        case Severity::HIGH:     return "high";
        case Severity::MEDIUM:   return "medium";
        case Severity::LOW:      return "low";
        case Severity::INFO:     return "info";
        default:                 return "medium";
    }
}

AlarmOccurrenceEntity::Severity AlarmOccurrenceEntity::stringToSeverity(const std::string& severity_str) {
    std::string lower_severity = severity_str;
    std::transform(lower_severity.begin(), lower_severity.end(), lower_severity.begin(), ::tolower);
    
    if (lower_severity == "critical") return Severity::CRITICAL;
    if (lower_severity == "high")     return Severity::HIGH;
    if (lower_severity == "medium")   return Severity::MEDIUM;
    if (lower_severity == "low")      return Severity::LOW;
    if (lower_severity == "info")     return Severity::INFO;
    
    return Severity::MEDIUM; // 기본값
}

std::vector<std::string> AlarmOccurrenceEntity::getAllSeverityStrings() {
    return {"critical", "high", "medium", "low", "info"};
}

// =============================================================================
// 비즈니스 로직 메서드들
// =============================================================================

bool AlarmOccurrenceEntity::acknowledge(int user_id, const std::string& comment) {
    try {
        if (state_ != State::ACTIVE) {
            if (logger_) {
                logger_->Warn("AlarmOccurrenceEntity::acknowledge - Occurrence not in active state: " + stateToString(state_));
            }
            return false;
        }
        
        state_ = State::ACKNOWLEDGED;
        acknowledged_time_ = std::chrono::system_clock::now();
        acknowledged_by_ = user_id;
        acknowledge_comment_ = comment;
        
        markDirty();
        
        // 데이터베이스에 즉시 반영
        bool success = updateToDatabase();
        
        if (success && logger_) {
            logger_->Info("AlarmOccurrenceEntity::acknowledge - Occurrence " + std::to_string(getId()) + 
                         " acknowledged by user " + std::to_string(user_id));
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
            if (logger_) {
                logger_->Warn("AlarmOccurrenceEntity::clear - Occurrence already cleared");
            }
            return true; // 이미 클리어된 상태
        }
        
        state_ = State::CLEARED;
        cleared_time_ = std::chrono::system_clock::now();
        cleared_value_ = cleared_value;
        clear_comment_ = comment;
        
        markDirty();
        
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
        markDirty();
        
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
    markDirty();
    
    if (logger_) {
        logger_->Debug("AlarmOccurrenceEntity::markNotificationSent - Occurrence " + std::to_string(getId()));
    }
}

void AlarmOccurrenceEntity::incrementNotificationCount() {
    notification_count_++;
    markDirty();
    
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

long long AlarmOccurrenceEntity::getStateDurationSeconds() const {
    auto now = std::chrono::system_clock::now();
    
    switch (state_) {
        case State::ACKNOWLEDGED:
            if (acknowledged_time_.has_value()) {
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - acknowledged_time_.value());
                return duration.count();
            }
            break;
        case State::CLEARED:
            if (cleared_time_.has_value()) {
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - cleared_time_.value());
                return duration.count();
            }
            break;
        default:
            break;
    }
    
    // 기본적으로는 발생 시점부터의 경과 시간
    return getElapsedSeconds();
}

// =============================================================================
// JSON 직렬화/역직렬화
// =============================================================================

std::string AlarmOccurrenceEntity::toJson() const {
    std::ostringstream json;
    json << "{";
    json << "\"id\":" << getId() << ",";
    json << "\"rule_id\":" << rule_id_ << ",";
    json << "\"tenant_id\":" << tenant_id_ << ",";
    json << "\"occurrence_time\":\"" << timestampToString(occurrence_time_) << "\",";
    json << "\"trigger_value\":\"" << trigger_value_ << "\",";
    json << "\"trigger_condition\":\"" << trigger_condition_ << "\",";
    json << "\"alarm_message\":\"" << alarm_message_ << "\",";
    json << "\"severity\":\"" << severityToString(severity_) << "\",";
    json << "\"state\":\"" << stateToString(state_) << "\",";
    
    if (acknowledged_time_.has_value()) {
        json << "\"acknowledged_time\":\"" << timestampToString(acknowledged_time_.value()) << "\",";
    }
    if (acknowledged_by_.has_value()) {
        json << "\"acknowledged_by\":" << acknowledged_by_.value() << ",";
    }
    json << "\"acknowledge_comment\":\"" << acknowledge_comment_ << "\",";
    
    if (cleared_time_.has_value()) {
        json << "\"cleared_time\":\"" << timestampToString(cleared_time_.value()) << "\",";
    }
    json << "\"cleared_value\":\"" << cleared_value_ << "\",";
    json << "\"clear_comment\":\"" << clear_comment_ << "\",";
    
    json << "\"notification_sent\":" << (notification_sent_ ? "true" : "false") << ",";
    if (notification_time_.has_value()) {
        json << "\"notification_time\":\"" << timestampToString(notification_time_.value()) << "\",";
    }
    json << "\"notification_count\":" << notification_count_ << ",";
    json << "\"notification_result\":\"" << notification_result_ << "\",";
    
    json << "\"context_data\":\"" << context_data_ << "\",";
    json << "\"source_name\":\"" << source_name_ << "\",";
    json << "\"location\":\"" << location_ << "\"";
    json << "}";
    
    return json.str();
}

bool AlarmOccurrenceEntity::fromJson(const std::string& json_str) {
    try {
        // 간단한 JSON 파싱 (실제로는 더 정교한 JSON 라이브러리 사용 권장)
        auto data = parseJsonToMap(json_str);
        
        if (data.find("id") != data.end()) {
            setId(std::stoi(data.at("id")));
        }
        if (data.find("rule_id") != data.end()) {
            rule_id_ = std::stoi(data.at("rule_id"));
        }
        if (data.find("tenant_id") != data.end()) {
            tenant_id_ = std::stoi(data.at("tenant_id"));
        }
        if (data.find("trigger_value") != data.end()) {
            trigger_value_ = data.at("trigger_value");
        }
        if (data.find("alarm_message") != data.end()) {
            alarm_message_ = data.at("alarm_message");
        }
        if (data.find("severity") != data.end()) {
            severity_ = stringToSeverity(data.at("severity"));
        }
        if (data.find("state") != data.end()) {
            state_ = stringToState(data.at("state"));
        }
        
        // 추가 필드들도 필요에 따라 파싱...
        
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::fromJson failed: " + std::string(e.what()));
        }
        return false;
    }
}

std::string AlarmOccurrenceEntity::toSummaryJson() const {
    std::ostringstream json;
    json << "{";
    json << "\"id\":" << getId() << ",";
    json << "\"rule_id\":" << rule_id_ << ",";
    json << "\"alarm_message\":\"" << alarm_message_ << "\",";
    json << "\"severity\":\"" << severityToString(severity_) << "\",";
    json << "\"state\":\"" << stateToString(state_) << "\",";
    json << "\"elapsed_seconds\":" << getElapsedSeconds();
    json << "}";
    return json.str();
}

// =============================================================================
// 유틸리티 메서드들
// =============================================================================

std::string AlarmOccurrenceEntity::getDisplayString() const {
    std::ostringstream ss;
    ss << "[" << severityToString(severity_) << "] ";
    ss << alarm_message_;
    ss << " (" << stateToString(state_) << ")";
    ss << " - " << getElapsedSeconds() << "s ago";
    return ss.str();
}

std::string AlarmOccurrenceEntity::getLogString() const {
    std::ostringstream ss;
    ss << "AlarmOccurrence{id=" << getId();
    ss << ", rule_id=" << rule_id_;
    ss << ", severity=" << severityToString(severity_);
    ss << ", state=" << stateToString(state_);
    ss << ", elapsed=" << getElapsedSeconds() << "s}";
    return ss.str();
}

bool AlarmOccurrenceEntity::isValid() const {
    if (rule_id_ <= 0) return false;
    if (tenant_id_ <= 0) return false;
    if (alarm_message_.empty()) return false;
    return true;
}

// =============================================================================
// 내부 헬퍼 메서드들
// =============================================================================

void AlarmOccurrenceEntity::initializeDependencies() const {
    if (!logger_) {
        logger_ = &LogManager::getInstance();
    }
    if (!config_manager_) {
        config_manager_ = &ConfigManager::getInstance();
    }
}

std::string AlarmOccurrenceEntity::timestampToString(const std::chrono::system_clock::time_point& tp) const {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::chrono::system_clock::time_point AlarmOccurrenceEntity::stringToTimestamp(const std::string& timestamp_str) const {
    // 간단한 ISO 8601 파싱 (실제로는 더 정교한 파싱 필요)
    std::tm tm = {};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::map<std::string, std::string> AlarmOccurrenceEntity::parseJsonToMap(const std::string& json_str) const {
    std::map<std::string, std::string> result;
    
    // 매우 간단한 JSON 파싱 (실제로는 JSON 라이브러리 사용 권장)
    std::regex field_regex(R"("([^"]+)"\s*:\s*"?([^",}]+)"?)");
    std::sregex_iterator iter(json_str.begin(), json_str.end(), field_regex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        std::smatch match = *iter;
        result[match[1].str()] = match[2].str();
    }
    
    return result;
}

std::string AlarmOccurrenceEntity::mapToJson(const std::map<std::string, std::string>& data) const {
    std::ostringstream json;
    json << "{";
    
    bool first = true;
    for (const auto& pair : data) {
        if (!first) json << ",";
        json << "\"" << pair.first << "\":\"" << pair.second << "\"";
        first = false;
    }
    
    json << "}";
    return json.str();
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne