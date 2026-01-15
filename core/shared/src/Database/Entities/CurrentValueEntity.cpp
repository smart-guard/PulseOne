/**
 * @file CurrentValueEntity.cpp
 * @brief PulseOne CurrentValueEntity 구현 - 새 JSON 스키마 완전 호환
 * @author PulseOne Development Team
 * @date 2025-08-07
 * 
 * 🎯 새 current_values 테이블 스키마 완전 반영:
 * - JSON 기반 current_value, raw_value 처리
 * - 다양한 타임스탬프들 (value, quality, log, read, write)
 * - 통계 카운터들 (read_count, write_count, error_count)
 * - DataVariant 타입 지원 완전 구현
 */

#include "Database/Entities/CurrentValueEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/CurrentValueRepository.h"
#include "Common/Utils.h"

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 구현 (새 스키마 필드들로 초기화)
// =============================================================================

CurrentValueEntity::CurrentValueEntity() 
    : BaseEntity<CurrentValueEntity>()
    , point_id_(0)
    , current_value_(R"({"value": 0.0})")           // 🔥 JSON 기본값
    , raw_value_(R"({"value": 0.0})")               // 🔥 JSON 기본값
    , value_type_("double")                          // 🔥 기본 타입
    , quality_code_(PulseOne::Enums::DataQuality::GOOD)
    , quality_("good")                               // 🔥 텍스트 품질
    , value_timestamp_(std::chrono::system_clock::now())     // 🔥 값 타임스탬프
    , quality_timestamp_(std::chrono::system_clock::now())   // 🔥 품질 타임스탬프
    , last_log_time_(std::chrono::system_clock::now())       // 🔥 로그 타임스탬프
    , last_read_time_(std::chrono::system_clock::now())      // 🔥 읽기 타임스탬프
    , last_write_time_(std::chrono::system_clock::now())     // 🔥 쓰기 타임스탬프
    , updated_at_(std::chrono::system_clock::now())
    , read_count_(0)                                 // 🔥 통계 카운터
    , write_count_(0)                                // 🔥 통계 카운터
    , error_count_(0)                                // 🔥 통계 카운터
{
    // 기본 생성자 - 모든 새 필드들 초기화
}

CurrentValueEntity::CurrentValueEntity(int point_id) 
    : BaseEntity<CurrentValueEntity>()
    , point_id_(point_id)
    , current_value_(R"({"value": 0.0})")
    , raw_value_(R"({"value": 0.0})")
    , value_type_("double")
    , quality_code_(PulseOne::Enums::DataQuality::NOT_CONNECTED)  // 초기 상태
    , quality_("not_connected")
    , value_timestamp_(std::chrono::system_clock::now())
    , quality_timestamp_(std::chrono::system_clock::now())
    , last_log_time_(std::chrono::system_clock::now())
    , last_read_time_(std::chrono::system_clock::now())
    , last_write_time_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now())
    , read_count_(0)
    , write_count_(0)
    , error_count_(0)
{
    // ID로 생성하면서 자동 로드 시도
    loadFromDatabase();
}

CurrentValueEntity::CurrentValueEntity(int point_id, const PulseOne::BasicTypes::DataVariant& value) 
    : BaseEntity<CurrentValueEntity>()
    , point_id_(point_id)
    , current_value_(R"({"value": 0.0})")  // 임시, 아래에서 설정됨
    , raw_value_(R"({"value": 0.0})")
    , value_type_("double")                 // 임시, 아래에서 설정됨
    , quality_code_(PulseOne::Enums::DataQuality::GOOD)
    , quality_("good")
    , value_timestamp_(std::chrono::system_clock::now())
    , quality_timestamp_(std::chrono::system_clock::now())
    , last_log_time_(std::chrono::system_clock::now())
    , last_read_time_(std::chrono::system_clock::now())
    , last_write_time_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now())
    , read_count_(0)
    , write_count_(0)
    , error_count_(0)
{
    // DataVariant 값을 JSON으로 설정
    setCurrentValueFromVariant(value);
    setRawValue(current_value_);  // 기본적으로 같은 값
    markModified();
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (Repository 활용)
// =============================================================================

bool CurrentValueEntity::loadFromDatabase() {
    if (point_id_ <= 0) {
        if (logger_) {
            logger_->Error("CurrentValueEntity::loadFromDatabase - Invalid point_id: " + std::to_string(point_id_));
        }
        return false;
    }
    
    try {
        auto& factory = PulseOne::Database::RepositoryFactory::getInstance();
        auto repo = factory.getCurrentValueRepository();
        
        if (!repo) {
            if (logger_) {
                logger_->Error("CurrentValueEntity::loadFromDatabase - Repository not available");
            }
            return false;
        }
        
        auto loaded_entity = repo->findById(point_id_);
        if (loaded_entity.has_value()) {
            // 🔥 새 스키마 모든 필드들 복사
            *this = loaded_entity.value();
            markSaved();
            
            if (logger_) {
                logger_->Debug("CurrentValueEntity::loadFromDatabase - Loaded current value for point_id: " + std::to_string(point_id_));
            }
            return true;
        } else {
            if (logger_) {
                logger_->Debug("CurrentValueEntity::loadFromDatabase - No current value found for point_id: " + std::to_string(point_id_));
            }
            return false;
        }
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("CurrentValueEntity::loadFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool CurrentValueEntity::saveToDatabase() {
    if (!isValid()) {
        if (logger_) {
            logger_->Error("CurrentValueEntity::saveToDatabase - Invalid current value data");
        }
        return false;
    }
    
    try {
        auto& factory = PulseOne::Database::RepositoryFactory::getInstance();
        auto repo = factory.getCurrentValueRepository();
        
        if (!repo) {
            if (logger_) {
                logger_->Error("CurrentValueEntity::saveToDatabase - Repository not available");
            }
            return false;
        }
        
        // 🔥 저장 전 updated_at 갱신
        updated_at_ = std::chrono::system_clock::now();
        markModified();
        
        // Repository의 save 메서드 호출 (upsert)
        bool success = repo->save(*this);
        
        if (success) {
            markSaved();
            
            if (logger_) {
                logger_->Info("CurrentValueEntity::saveToDatabase - Saved current value for point_id: " + std::to_string(point_id_));
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("CurrentValueEntity::saveToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool CurrentValueEntity::updateToDatabase() {
    if (point_id_ <= 0 || !isValid()) {
        if (logger_) {
            logger_->Error("CurrentValueEntity::updateToDatabase - Invalid current value data or point_id");
        }
        return false;
    }
    
    try {
        auto& factory = PulseOne::Database::RepositoryFactory::getInstance();
        auto repo = factory.getCurrentValueRepository();
        
        if (!repo) {
            if (logger_) {
                logger_->Error("CurrentValueEntity::updateToDatabase - Repository not available");
            }
            return false;
        }
        
        // 🔥 업데이트 전 updated_at 갱신
        updated_at_ = std::chrono::system_clock::now();
        markModified();
        
        // Repository의 update 메서드 호출
        bool success = repo->update(*this);
        
        if (success) {
            markSaved();
            
            if (logger_) {
                logger_->Info("CurrentValueEntity::updateToDatabase - Updated current value for point_id: " + std::to_string(point_id_));
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("CurrentValueEntity::updateToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool CurrentValueEntity::deleteFromDatabase() {
    if (point_id_ <= 0) {
        if (logger_) {
            logger_->Error("CurrentValueEntity::deleteFromDatabase - Invalid point_id");
        }
        return false;
    }
    
    try {
        auto& factory = PulseOne::Database::RepositoryFactory::getInstance();
        auto repo = factory.getCurrentValueRepository();
        
        if (!repo) {
            if (logger_) {
                logger_->Error("CurrentValueEntity::deleteFromDatabase - Repository not available");
            }
            return false;
        }
        
        bool success = repo->deleteById(point_id_);
        
        if (success) {
            markDeleted();
            
            if (logger_) {
                logger_->Info("CurrentValueEntity::deleteFromDatabase - Deleted current value for point_id: " + std::to_string(point_id_));
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("CurrentValueEntity::deleteFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

// =============================================================================
// 🔥 추가 구현 메서드들 (새 스키마 활용)
// =============================================================================

void CurrentValueEntity::updateValue(const PulseOne::BasicTypes::DataVariant& new_value, 
                                    PulseOne::Enums::DataQuality new_quality) {
    // 이전 품질과 다르면 품질 타임스탬프 업데이트
    if (quality_code_ != new_quality) {
        quality_timestamp_ = std::chrono::system_clock::now();
        quality_code_ = new_quality;
        quality_ = PulseOne::Utils::DataQualityToString(new_quality);
    }
    
    // 값 업데이트
    setCurrentValueFromVariant(new_value);
    
    // 마킹
    markModified();
}

void CurrentValueEntity::updateValueWithRaw(const PulseOne::BasicTypes::DataVariant& current_val,
                                           const PulseOne::BasicTypes::DataVariant& raw_val,
                                           PulseOne::Enums::DataQuality new_quality) {
    // 품질 업데이트
    if (quality_code_ != new_quality) {
        quality_timestamp_ = std::chrono::system_clock::now();
        quality_code_ = new_quality;
        quality_ = PulseOne::Utils::DataQualityToString(new_quality);
    }
    
    // 현재값 설정
    setCurrentValueFromVariant(current_val);
    
    // 원시값 설정 (별도 처리)
    json raw_json;
    std::visit([&](auto&& arg) {
        raw_json["value"] = arg;
    }, raw_val);
    raw_value_ = raw_json.dump();
    
    // 값 타임스탬프 업데이트
    value_timestamp_ = std::chrono::system_clock::now();
    updated_at_ = value_timestamp_;
    
    markModified();
}

bool CurrentValueEntity::shouldLog(int log_interval_ms, double deadband) const {
    // 시간 기반 로깅 체크
    if (log_interval_ms > 0) {
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_log_time_);
        if (elapsed.count() < log_interval_ms) {
            return false;  // 아직 로깅 간격이 안됨
        }
    }
    
    // 데드밴드 체크 (단순화 - 실제로는 이전 로그값과 비교해야 함)
    if (deadband > 0.0 && hasGoodQuality()) {
        // 실제 구현에서는 이전 로그된 값과 비교
        // 여기서는 단순화
        return true;
    }
    
    return true;
}

void CurrentValueEntity::onRead() {
    incrementReadCount();
    
    if (logger_) {
        logger_->Debug("CurrentValueEntity::onRead - point_id: " + std::to_string(point_id_) + 
                      ", read_count: " + std::to_string(read_count_));
    }
}

void CurrentValueEntity::onWrite() {
    incrementWriteCount();
    
    if (logger_) {
        logger_->Debug("CurrentValueEntity::onWrite - point_id: " + std::to_string(point_id_) + 
                      ", write_count: " + std::to_string(write_count_));
    }
}

void CurrentValueEntity::onError() {
    incrementErrorCount();
    
    // 에러 시 품질을 BAD로 설정
    if (quality_code_ != PulseOne::Enums::DataQuality::BAD) {
        quality_code_ = PulseOne::Enums::DataQuality::BAD;
        quality_ = "bad";
        quality_timestamp_ = std::chrono::system_clock::now();
    }
    
    updated_at_ = std::chrono::system_clock::now();
    markModified();
    
    if (logger_) {
        logger_->Warn("CurrentValueEntity::onError - point_id: " + std::to_string(point_id_) + 
                     ", error_count: " + std::to_string(error_count_));
    }
}

double CurrentValueEntity::getNumericValue() const {
    try {
        auto variant = getCurrentValueAsVariant();
        
        // DataVariant에서 숫자값 추출
        return std::visit([](auto&& arg) -> double {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_arithmetic_v<T>) {
                return static_cast<double>(arg);
            } else if constexpr (std::is_same_v<T, std::string>) {
                try {
                    return std::stod(arg);
                } catch (...) {
                    return 0.0;
                }
            } else {
                return 0.0;
            }
        }, variant);
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("CurrentValueEntity::getNumericValue failed: " + std::string(e.what()));
        }
        return 0.0;
    }
}

std::string CurrentValueEntity::getStringValue() const {
    try {
        auto variant = getCurrentValueAsVariant();
        
        // DataVariant에서 문자열값 추출
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return arg;
            } else if constexpr (std::is_same_v<T, bool>) {
                return arg ? "true" : "false";
            } else if constexpr (std::is_arithmetic_v<T>) {
                return std::to_string(arg);
            } else {
                return "";
            }
        }, variant);
        
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("CurrentValueEntity::getStringValue failed: " + std::string(e.what()));
        }
        return "";
    }
}

void CurrentValueEntity::resetStatistics() {
    read_count_ = 0;
    write_count_ = 0;
    error_count_ = 0;
    last_read_time_ = std::chrono::system_clock::now();
    last_write_time_ = std::chrono::system_clock::now();
    updated_at_ = std::chrono::system_clock::now();
    markModified();
    
    if (logger_) {
        logger_->Info("CurrentValueEntity::resetStatistics - point_id: " + std::to_string(point_id_));
    }
}

json CurrentValueEntity::getFullStatus() const {
    json status = getStatistics();  // 기본 통계
    
    // 추가 상태 정보
    status["point_id"] = point_id_;
    status["current_value"] = current_value_;
    status["value_type"] = value_type_;
    status["quality"] = quality_;
    status["is_stale"] = isStale();
    status["has_good_quality"] = hasGoodQuality();
    status["value_timestamp"] = PulseOne::Utils::TimestampToDBString(value_timestamp_);
    status["quality_timestamp"] = PulseOne::Utils::TimestampToDBString(quality_timestamp_);
    
    return status;
}

// =============================================================================
// 🔥 타입 불일치 수정 및 편의 메서드 구현
// =============================================================================

double CurrentValueEntity::getValue() const {
    return getNumericValue();
}

void CurrentValueEntity::setValue(double value) {
    updateValue(value, quality_code_);
}

void CurrentValueEntity::setTimestamp(const std::chrono::system_clock::time_point& timestamp) {
    value_timestamp_ = timestamp;
    updated_at_ = timestamp;
    markModified();
}

std::chrono::system_clock::time_point CurrentValueEntity::getTimestamp() const {
    return value_timestamp_;
}

std::shared_ptr<Repositories::CurrentValueRepository> CurrentValueEntity::getRepository() const {
    return RepositoryFactory::getInstance().getCurrentValueRepository();
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne