// =============================================================================
// collector/src/Database/Entities/AlarmOccurrenceEntity.cpp
// PulseOne AlarmOccurrenceEntity 구현 - AlarmRuleEntity 패턴 100% 적용
// =============================================================================

#include "Database/Entities/AlarmOccurrenceEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/AlarmOccurrenceRepository.h"

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 구현 (AlarmRuleEntity 패턴)
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
    , acknowledged_time_(std::nullopt)
    , acknowledged_by_(std::nullopt)
    , acknowledge_comment_("")
    , cleared_time_(std::nullopt)
    , cleared_value_("")
    , clear_comment_("")
    , notification_sent_(false)
    , notification_time_(std::nullopt)
    , notification_count_(0)
    , notification_result_("")
    , context_data_("{}")
    , source_name_("")
    , location_("") {
}

AlarmOccurrenceEntity::AlarmOccurrenceEntity(int occurrence_id) 
    : AlarmOccurrenceEntity() {  // 위임 생성자 사용
    setId(occurrence_id);
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (AlarmRuleEntity 패턴)
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
                AlarmOccurrenceEntity mutable_copy = *this;
                success = repo->save(mutable_copy);
                if (success) {
                    setId(mutable_copy.getId());  // 새로 생성된 ID 설정
                }
            } else {
                success = repo->update(*this);
            }
            
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("AlarmOccurrenceEntity::saveToDatabase - Saved occurrence: " + std::to_string(getId()));
                }
                return true;
            }
        }
        
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::saveToDatabase - Repository operation failed");
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
                markDeleted();
                if (logger_) {
                    logger_->Info("AlarmOccurrenceEntity::deleteFromDatabase - Deleted occurrence: " + std::to_string(getId()));
                }
                return true;
            }
        }
        
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::deleteFromDatabase - Repository operation failed");
        }
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::deleteFromDatabase failed: " + std::string(e.what()));
        }
        markError();
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
                return true;
            }
        }
        
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::updateToDatabase - Repository operation failed");
        }
        return false;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::updateToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

// =============================================================================
// JSON 직렬화/역직렬화 (AlarmRuleEntity 패턴)
// =============================================================================

json AlarmOccurrenceEntity::toJson() const {
    json j;
    
    try {
        // 기본 식별자
        j["id"] = getId();
        j["rule_id"] = rule_id_;
        j["tenant_id"] = tenant_id_;
        
        // 발생 정보
        j["occurrence_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            occurrence_time_.time_since_epoch()).count();
        j["trigger_value"] = trigger_value_;
        j["trigger_condition"] = trigger_condition_;
        j["alarm_message"] = alarm_message_;
        j["severity"] = severityToString(severity_);
        j["state"] = stateToString(state_);
        
        // Optional 필드들
        if (acknowledged_time_.has_value()) {
            j["acknowledged_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                acknowledged_time_.value().time_since_epoch()).count();
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
            j["cleared_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                cleared_time_.value().time_since_epoch()).count();
        } else {
            j["cleared_time"] = nullptr;
        }
        j["cleared_value"] = cleared_value_;
        j["clear_comment"] = clear_comment_;
        
        // 알림 정보
        j["notification_sent"] = notification_sent_;
        if (notification_time_.has_value()) {
            j["notification_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                notification_time_.value().time_since_epoch()).count();
        } else {
            j["notification_time"] = nullptr;
        }
        j["notification_count"] = notification_count_;
        j["notification_result"] = notification_result_;
        
        // 컨텍스트 정보
        j["context_data"] = context_data_;
        j["source_name"] = source_name_;
        j["location"] = location_;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::toJson failed: " + std::string(e.what()));
        }
    }
    
    return j;
}

bool AlarmOccurrenceEntity::fromJson(const json& j) {
    try {
        // 기본 식별자
        if (j.contains("id")) setId(j["id"].get<int>());
        if (j.contains("rule_id")) rule_id_ = j["rule_id"].get<int>();
        if (j.contains("tenant_id")) tenant_id_ = j["tenant_id"].get<int>();
        
        // 발생 정보
        if (j.contains("occurrence_time") && !j["occurrence_time"].is_null()) {
            auto ms = j["occurrence_time"].get<int64_t>();
            occurrence_time_ = std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
        }
        if (j.contains("trigger_value")) trigger_value_ = j["trigger_value"].get<std::string>();
        if (j.contains("trigger_condition")) trigger_condition_ = j["trigger_condition"].get<std::string>();
        if (j.contains("alarm_message")) alarm_message_ = j["alarm_message"].get<std::string>();
        if (j.contains("severity")) severity_ = stringToSeverity(j["severity"].get<std::string>());
        if (j.contains("state")) state_ = stringToState(j["state"].get<std::string>());
        
        // Optional 필드들
        if (j.contains("acknowledged_time") && !j["acknowledged_time"].is_null()) {
            auto ms = j["acknowledged_time"].get<int64_t>();
            acknowledged_time_ = std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
        }
        if (j.contains("acknowledged_by") && !j["acknowledged_by"].is_null()) {
            acknowledged_by_ = j["acknowledged_by"].get<int>();
        }
        if (j.contains("acknowledge_comment")) acknowledge_comment_ = j["acknowledge_comment"].get<std::string>();
        
        if (j.contains("cleared_time") && !j["cleared_time"].is_null()) {
            auto ms = j["cleared_time"].get<int64_t>();
            cleared_time_ = std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
        }
        if (j.contains("cleared_value")) cleared_value_ = j["cleared_value"].get<std::string>();
        if (j.contains("clear_comment")) clear_comment_ = j["clear_comment"].get<std::string>();
        
        // 알림 정보
        if (j.contains("notification_sent")) notification_sent_ = j["notification_sent"].get<bool>();
        if (j.contains("notification_time") && !j["notification_time"].is_null()) {
            auto ms = j["notification_time"].get<int64_t>();
            notification_time_ = std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
        }
        if (j.contains("notification_count")) notification_count_ = j["notification_count"].get<int>();
        if (j.contains("notification_result")) notification_result_ = j["notification_result"].get<std::string>();
        
        // 컨텍스트 정보
        if (j.contains("context_data")) context_data_ = j["context_data"].get<std::string>();
        if (j.contains("source_name")) source_name_ = j["source_name"].get<std::string>();
        if (j.contains("location")) location_ = j["location"].get<std::string>();
        
        markModified();
        return true;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("AlarmOccurrenceEntity::fromJson failed: " + std::string(e.what()));
        }
        return false;
    }
}

// =============================================================================
// 유효성 검사
// =============================================================================

bool AlarmOccurrenceEntity::isValid() const {
    return getId() > 0 && 
           rule_id_ > 0 && 
           tenant_id_ > 0 && 
           !alarm_message_.empty();
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
            logger_->Info("AlarmOccurrenceEntity::acknowledge - Occurrence " + 
                         std::to_string(getId()) + " acknowledged by user " + std::to_string(user_id));
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

// =============================================================================
// 문자열 변환 메서드들 (static - AlarmRuleEntity 패턴)
// =============================================================================

std::string AlarmOccurrenceEntity::severityToString(Severity severity) {
    switch (severity) {
        case Severity::CRITICAL: return "critical";
        case Severity::HIGH: return "high";
        case Severity::MEDIUM: return "medium";
        case Severity::LOW: return "low";
        case Severity::INFO: return "info";
        default: return "medium";
    }
}

AlarmOccurrenceEntity::Severity AlarmOccurrenceEntity::stringToSeverity(const std::string& str) {
    if (str == "critical") return Severity::CRITICAL;
    if (str == "high") return Severity::HIGH;
    if (str == "medium") return Severity::MEDIUM;
    if (str == "low") return Severity::LOW;
    if (str == "info") return Severity::INFO;
    return Severity::MEDIUM; // 기본값
}

std::string AlarmOccurrenceEntity::stateToString(State state) {
    switch (state) {
        case State::ACTIVE: return "active";
        case State::ACKNOWLEDGED: return "acknowledged";
        case State::CLEARED: return "cleared";
        case State::SUPPRESSED: return "suppressed";
        default: return "active";
    }
}

AlarmOccurrenceEntity::State AlarmOccurrenceEntity::stringToState(const std::string& str) {
    if (str == "active") return State::ACTIVE;
    if (str == "acknowledged") return State::ACKNOWLEDGED;
    if (str == "cleared") return State::CLEARED;
    if (str == "suppressed") return State::SUPPRESSED;
    return State::ACTIVE; // 기본값
}

// =============================================================================
// toString 메서드
// =============================================================================

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

// =============================================================================
// 내부 헬퍼 메서드들
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