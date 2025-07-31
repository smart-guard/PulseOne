/**
 * @file CurrentValueEntity.cpp
 * @brief PulseOne CurrentValueEntity 구현 (DataPointEntity 패턴 100% 적용)
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 DataPointEntity 패턴 완전 적용:
 * - 헤더에서는 선언만, CPP에서 Repository 호출
 * - Repository include는 CPP에서만 (순환 참조 방지)
 * - BaseEntity 순수 가상 함수 구현만 포함
 * - entityToParams 등은 Repository로 이동
 */

#include "Database/Entities/CurrentValueEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/CurrentValueRepository.h"

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 구현 (CPP에서 구현하여 중복 제거)
// =============================================================================

CurrentValueEntity::CurrentValueEntity() 
    : BaseEntity<CurrentValueEntity>()
    , point_id_(0)
    , value_(0.0)
    , raw_value_(0.0)
    , string_value_("")
    , quality_(PulseOne::Enums::DataQuality::GOOD)
    , timestamp_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now())
{
    // 기본 생성자
}

CurrentValueEntity::CurrentValueEntity(int point_id) 
    : BaseEntity<CurrentValueEntity>()
    , point_id_(point_id)
    , value_(0.0)
    , raw_value_(0.0)
    , string_value_("")
    , quality_(PulseOne::Enums::DataQuality::GOOD)
    , timestamp_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now())
{
    // ID로 생성하면서 자동 로드
    loadFromDatabase();
}

CurrentValueEntity::CurrentValueEntity(int point_id, double value) 
    : BaseEntity<CurrentValueEntity>()
    , point_id_(point_id)
    , value_(value)
    , raw_value_(value)  // 기본적으로 같은 값
    , string_value_("")
    , quality_(PulseOne::Enums::DataQuality::GOOD)
    , timestamp_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now())
{
    markModified();  // 새 값이므로 수정 상태로 마킹
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
            // 로드된 데이터로 현재 인스턴스 업데이트
            *this = loaded_entity.value();
            markSaved();  // BaseEntity 패턴
            
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
        
        // Repository의 save 메서드 호출 (upsert)
        bool success = repo->save(*this);
        
        if (success) {
            markSaved();      // BaseEntity 패턴
            markSaved();      // BaseEntity 패턴
            
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
        
        // Repository의 update 메서드 호출
        bool success = repo->update(*this);
        
        if (success) {
            markSaved();      // BaseEntity 패턴
            markSaved();      // BaseEntity 패턴
            
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
            markDeleted();  // BaseEntity 패턴
            
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

} // namespace Entities
} // namespace Database
} // namespace PulseOne