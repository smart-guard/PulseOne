/**
 * @file AlarmOccurrenceEntity.cpp
 * @brief PulseOne AlarmOccurrenceEntity 구현 - 실제 DB 스키마 완전 호환
 * @author PulseOne Development Team
 * @date 2025-08-26
 * 
 * 🔧 수정사항:
 * - device_id 타입 수정: string → optional<int>
 * - JSON 직렬화/역직렬화 수정
 * - 모든 필드 타입 정확히 매핑
 */

#include "Database/Entities/AlarmOccurrenceEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/AlarmOccurrenceRepository.h"
#include "Alarm/AlarmTypes.h"
#include <algorithm>

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 구현 (타입 수정됨)
// =============================================================================

AlarmOccurrenceEntity::AlarmOccurrenceEntity() 
    : BaseEntity<AlarmOccurrenceEntity>()
    , rule_id_(0)
    , tenant_id_(0)
    , occurrence_time_(std::chrono::system_clock::now())
    , trigger_value_("")
    , trigger_condition_("")
    , alarm_message_("")
    , severity_(AlarmSeverity::MEDIUM)
    , state_(AlarmState::ACTIVE)
    , acknowledged_time_(std::nullopt)
    , acknowledged_by_(std::nullopt)
    , acknowledge_comment_("")
    , cleared_time_(std::nullopt)
    , cleared_value_("")
    , clear_comment_("")
    , cleared_by_(std::nullopt)
    , notification_sent_(false)
    , notification_time_(std::nullopt)
    , notification_count_(0)
    , notification_result_("")
    , context_data_("{}")
    , source_name_("")
    , location_("")
    , created_at_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now())
    , device_id_(std::nullopt)  // 🔧 수정: optional<int>로 초기화
    , point_id_(std::nullopt)
    , category_(std::nullopt)
    , tags_() {
}

AlarmOccurrenceEntity::AlarmOccurrenceEntity(int occurrence_id) 
    : AlarmOccurrenceEntity() {
    setId(occurrence_id);
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현
// =============================================================================

bool AlarmOccurrenceEntity::loadFromDatabase() {
    if (getId() <= 0) {
        LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::LOG_ERROR,
                                    "loadFromDatabase - Invalid occurrence ID: " + std::to_string(getId()));
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
                LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::INFO,
                                            "loadFromDatabase - Loaded occurrence: " + std::to_string(getId()));
                return true;
            }
        }
        
        LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::WARN,
                                    "loadFromDatabase - Occurrence not found: " + std::to_string(getId()));
        return false;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::LOG_ERROR,
                                    "loadFromDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool AlarmOccurrenceEntity::saveToDatabase() {
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmOccurrenceRepository();
        if (repo) {
            updateTimestamp();
            
            bool success = false;
            if (getId() <= 0) {
                AlarmOccurrenceEntity mutable_copy = *this;
                success = repo->save(mutable_copy);
                if (success) {
                    setId(mutable_copy.getId());
                }
            } else {
                success = repo->update(*this);
            }
            
            if (success) {
                markSaved();
                LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::INFO,
                                            "saveToDatabase - Saved occurrence: " + std::to_string(getId()));
                return true;
            }
        }
        
        LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::LOG_ERROR,
                                    "saveToDatabase - Repository operation failed");
        return false;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::LOG_ERROR,
                                    "saveToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool AlarmOccurrenceEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::LOG_ERROR,
                                    "deleteFromDatabase - Invalid occurrence ID: " + std::to_string(getId()));
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmOccurrenceRepository();
        if (repo) {
            bool success = repo->deleteById(getId());
            if (success) {
                markDeleted();
                LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::INFO,
                                            "deleteFromDatabase - Deleted occurrence: " + std::to_string(getId()));
                return true;
            }
        }
        
        LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::LOG_ERROR,
                                    "deleteFromDatabase - Repository operation failed");
        return false;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::LOG_ERROR,
                                    "deleteFromDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool AlarmOccurrenceEntity::updateToDatabase() {
    if (getId() <= 0) {
        LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::LOG_ERROR,
                                    "updateToDatabase - Invalid occurrence ID: " + std::to_string(getId()));
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getAlarmOccurrenceRepository();
        if (repo) {
            updateTimestamp();
            
            bool success = repo->update(*this);
            if (success) {
                markSaved();
                LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::INFO,
                                            "updateToDatabase - Updated occurrence: " + std::to_string(getId()));
                return true;
            }
        }
        
        LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::LOG_ERROR,
                                    "updateToDatabase - Repository operation failed");
        return false;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::LOG_ERROR,
                                    "updateToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

// =============================================================================
// JSON 직렬화/역직렬화 (타입 수정됨)
// =============================================================================

json AlarmOccurrenceEntity::toJson() const {
    json j;
    
    try {
        // 기본 식별자
        j["id"] = getId();
        j["rule_id"] = rule_id_;
        j["tenant_id"] = tenant_id_;
        
        // 발생 정보
        j["occurrence_time"] = timestampToString(occurrence_time_);
        j["trigger_value"] = trigger_value_;
        j["trigger_condition"] = trigger_condition_;
        j["alarm_message"] = alarm_message_;
        j["severity"] = PulseOne::Alarm::severityToString(severity_);
        j["state"] = PulseOne::Alarm::stateToString(state_);
        
        // Acknowledge 정보
        j["acknowledged_time"] = acknowledged_time_.has_value() ? 
                                timestampToString(acknowledged_time_.value()) : "";
        j["acknowledged_by"] = acknowledged_by_.has_value() ? 
                              acknowledged_by_.value() : -1;
        j["acknowledge_comment"] = acknowledge_comment_;
        
        // Clear 정보
        j["cleared_time"] = cleared_time_.has_value() ? 
                           timestampToString(cleared_time_.value()) : "";
        j["cleared_value"] = cleared_value_;
        j["clear_comment"] = clear_comment_;
        j["cleared_by"] = cleared_by_.has_value() ? cleared_by_.value() : -1;
        
        // 알림 정보
        j["notification_sent"] = notification_sent_;
        j["notification_time"] = notification_time_.has_value() ? 
                                timestampToString(notification_time_.value()) : "";
        j["notification_count"] = notification_count_;
        j["notification_result"] = notification_result_;
        
        // 컨텍스트 정보
        j["context_data"] = context_data_;
        j["source_name"] = source_name_;
        j["location"] = location_;
        
        // 시간 정보
        j["created_at"] = timestampToString(created_at_);
        j["updated_at"] = timestampToString(updated_at_);
        
        // 🔧 수정: 디바이스/포인트 정보 (정수형 처리)
        j["device_id"] = device_id_.has_value() ? device_id_.value() : -1;
        j["point_id"] = point_id_.has_value() ? point_id_.value() : -1;
        
        // 분류 시스템
        j["category"] = category_.has_value() ? category_.value() : "";
        j["tags"] = tags_;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::LOG_ERROR,
                                    "toJson failed: " + std::string(e.what()));
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
        if (j.contains("occurrence_time")) {
            std::string time_str = j["occurrence_time"].get<std::string>();
            if (!time_str.empty()) {
                occurrence_time_ = stringToTimestamp(time_str);
            }
        }
        if (j.contains("trigger_value")) trigger_value_ = j["trigger_value"].get<std::string>();
        if (j.contains("trigger_condition")) trigger_condition_ = j["trigger_condition"].get<std::string>();
        if (j.contains("alarm_message")) alarm_message_ = j["alarm_message"].get<std::string>();
        if (j.contains("severity")) {
            severity_ = PulseOne::Alarm::stringToSeverity(j["severity"].get<std::string>());
        }
        if (j.contains("state")) {
            state_ = PulseOne::Alarm::stringToState(j["state"].get<std::string>());
        }
        
        // Acknowledge 정보
        if (j.contains("acknowledged_time")) {
            std::string time_str = j["acknowledged_time"].get<std::string>();
            if (!time_str.empty()) {
                acknowledged_time_ = stringToTimestamp(time_str);
            }
        }
        if (j.contains("acknowledged_by")) {
            int user_id = j["acknowledged_by"].get<int>();
            if (user_id > 0) {
                acknowledged_by_ = user_id;
            }
        }
        if (j.contains("acknowledge_comment")) acknowledge_comment_ = j["acknowledge_comment"].get<std::string>();
        
        // Clear 정보
        if (j.contains("cleared_time")) {
            std::string time_str = j["cleared_time"].get<std::string>();
            if (!time_str.empty()) {
                cleared_time_ = stringToTimestamp(time_str);
            }
        }
        if (j.contains("cleared_value")) cleared_value_ = j["cleared_value"].get<std::string>();
        if (j.contains("clear_comment")) clear_comment_ = j["clear_comment"].get<std::string>();
        if (j.contains("cleared_by")) {
            int user_id = j["cleared_by"].get<int>();
            if (user_id > 0) {
                cleared_by_ = user_id;
            }
        }
        
        // 알림 정보
        if (j.contains("notification_sent")) notification_sent_ = j["notification_sent"].get<bool>();
        if (j.contains("notification_time")) {
            std::string time_str = j["notification_time"].get<std::string>();
            if (!time_str.empty()) {
                notification_time_ = stringToTimestamp(time_str);
            }
        }
        if (j.contains("notification_count")) notification_count_ = j["notification_count"].get<int>();
        if (j.contains("notification_result")) notification_result_ = j["notification_result"].get<std::string>();
        
        // 컨텍스트 정보
        if (j.contains("context_data")) context_data_ = j["context_data"].get<std::string>();
        if (j.contains("source_name")) source_name_ = j["source_name"].get<std::string>();
        if (j.contains("location")) location_ = j["location"].get<std::string>();
        
        // 시간 정보
        if (j.contains("created_at")) {
            std::string time_str = j["created_at"].get<std::string>();
            if (!time_str.empty()) {
                created_at_ = stringToTimestamp(time_str);
            }
        }
        if (j.contains("updated_at")) {
            std::string time_str = j["updated_at"].get<std::string>();
            if (!time_str.empty()) {
                updated_at_ = stringToTimestamp(time_str);
            }
        }
        
        // 🔧 수정: 디바이스/포인트 정보 (정수형 처리)
        if (j.contains("device_id")) {
            int device_id = j["device_id"].get<int>();
            if (device_id > 0) {
                device_id_ = device_id;
            }
        }
        if (j.contains("point_id")) {
            int point_id = j["point_id"].get<int>();
            if (point_id > 0) {
                point_id_ = point_id;
            }
        }
        
        // 분류 시스템
        if (j.contains("category")) {
            std::string cat = j["category"].get<std::string>();
            if (!cat.empty()) {
                category_ = cat;
            }
        }
        if (j.contains("tags") && j["tags"].is_array()) {
            tags_.clear();
            for (const auto& tag : j["tags"]) {
                if (tag.is_string()) {
                    tags_.push_back(tag.get<std::string>());
                }
            }
        }
        
        markModified();
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::LOG_ERROR,
                                    "fromJson failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 유효성 검사
// =============================================================================

bool AlarmOccurrenceEntity::isValid() const {
    return rule_id_ > 0 && 
           tenant_id_ > 0 && 
           !alarm_message_.empty();
}

// =============================================================================
// 비즈니스 로직 메서드들
// =============================================================================

bool AlarmOccurrenceEntity::acknowledge(int user_id, const std::string& comment) {
    try {
        if (state_ == AlarmState::ACKNOWLEDGED) {
            return true; // 이미 인지됨
        }
        
        acknowledged_time_ = std::chrono::system_clock::now();
        acknowledged_by_ = user_id;
        acknowledge_comment_ = comment;
        state_ = AlarmState::ACKNOWLEDGED;
        updateTimestamp();
        
        bool success = updateToDatabase();
        
        if (success) {
            LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::INFO,
                                        "acknowledge - Occurrence " + std::to_string(getId()) + 
                                        " acknowledged by user " + std::to_string(user_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::LOG_ERROR,
                                    "acknowledge failed: " + std::string(e.what()));
        return false;
    }
}

bool AlarmOccurrenceEntity::clear(int user_id, const std::string& cleared_value, const std::string& comment) {
    try {
        if (state_ == AlarmState::CLEARED) {
            return true; // 이미 해제됨
        }
        
        cleared_time_ = std::chrono::system_clock::now();
        cleared_value_ = cleared_value;
        clear_comment_ = comment;
        cleared_by_ = user_id;
        state_ = AlarmState::CLEARED;
        updateTimestamp();
        
        bool success = updateToDatabase();
        
        if (success) {
            LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::INFO,
                                        "clear - Occurrence " + std::to_string(getId()) + 
                                        " cleared by user " + std::to_string(user_id));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::LOG_ERROR,
                                    "clear failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 태그 관리 메서드들
// =============================================================================

void AlarmOccurrenceEntity::addTag(const std::string& tag) {
    if (tag.empty()) return;
    
    auto it = std::find(tags_.begin(), tags_.end(), tag);
    if (it == tags_.end()) {
        tags_.push_back(tag);
        updateTimestamp();
        markModified();
        
        LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::DEBUG,
                                    "addTag - Added tag '" + tag + "' to occurrence " + std::to_string(getId()));
    }
}

void AlarmOccurrenceEntity::removeTag(const std::string& tag) {
    auto it = std::find(tags_.begin(), tags_.end(), tag);
    if (it != tags_.end()) {
        tags_.erase(it);
        updateTimestamp();
        markModified();
        
        LogManager::getInstance().log("AlarmOccurrenceEntity", LogLevel::DEBUG,
                                    "removeTag - Removed tag '" + tag + "' from occurrence " + std::to_string(getId()));
    }
}

bool AlarmOccurrenceEntity::hasTag(const std::string& tag) const {
    return std::find(tags_.begin(), tags_.end(), tag) != tags_.end();
}

// =============================================================================
// toString 메서드
// =============================================================================

std::string AlarmOccurrenceEntity::toString() const {
    std::ostringstream ss;
    ss << "AlarmOccurrence[id=" << getId()
       << ", rule_id=" << rule_id_
       << ", tenant_id=" << tenant_id_
       << ", device_id=" << (device_id_.has_value() ? std::to_string(device_id_.value()) : "null")
       << ", point_id=" << (point_id_.has_value() ? std::to_string(point_id_.value()) : "null")
       << ", severity=" << PulseOne::Alarm::severityToString(severity_)
       << ", state=" << PulseOne::Alarm::stateToString(state_)
       << ", message=\"" << alarm_message_ << "\""
       << ", category=" << (category_.has_value() ? category_.value() : "null")
       << ", tags_count=" << tags_.size()
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

void AlarmOccurrenceEntity::updateTimestamp() {
    updated_at_ = std::chrono::system_clock::now();
    markModified();
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne