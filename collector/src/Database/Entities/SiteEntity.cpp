/**
 * @file SiteEntity.cpp
 * @brief PulseOne SiteEntity 구현 (DeviceEntity 패턴 100% 적용)
 * @author PulseOne Development Team
 * @date 2025-07-31
 * 
 * 🎯 DeviceEntity 패턴 완전 적용:
 * - 헤더에서는 선언만, CPP에서 Repository 호출
 * - Repository include는 CPP에서만 (순환 참조 방지)
 * - BaseEntity 순수 가상 함수 구현만 포함
 * - 모든 비즈니스 로직은 Repository로 위임
 */

#include "Database/Entities/SiteEntity.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/SiteRepository.h"

namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 구현 (CPP에서 구현하여 중복 제거)
// =============================================================================

SiteEntity::SiteEntity() 
    : BaseEntity<SiteEntity>()
    , tenant_id_(0)
    , parent_site_id_(0)
    , name_("")
    , code_("")
    , site_type_(SiteType::FACTORY)
    , description_("")
    , location_("")
    , timezone_("UTC")
    , address_("")
    , city_("")
    , country_("")
    , latitude_(0.0)
    , longitude_(0.0)
    , hierarchy_level_(1)
    , hierarchy_path_("")
    , is_active_(true)
    , contact_name_("")
    , contact_email_("")
    , contact_phone_("")
    , created_at_(std::chrono::system_clock::now())
    , updated_at_(std::chrono::system_clock::now()) {
}

SiteEntity::SiteEntity(int site_id) 
    : SiteEntity() {  // 위임 생성자 사용
    setId(site_id);
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (Repository 활용)
// =============================================================================

bool SiteEntity::loadFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("SiteEntity::loadFromDatabase - Invalid site ID: " + std::to_string(getId()));
        }
        markError();
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getSiteRepository();
        if (repo) {
            auto loaded = repo->findById(getId());
            if (loaded.has_value()) {
                *this = loaded.value();
                markSaved();
                if (logger_) {
                    logger_->Info("SiteEntity - Loaded site: " + name_);
                }
                return true;
            }
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("SiteEntity::loadFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool SiteEntity::saveToDatabase() {
    if (!isValid()) {
        if (logger_) {
            logger_->Error("SiteEntity::saveToDatabase - Invalid site data");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getSiteRepository();
        if (repo) {
            bool success = repo->save(*this);
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("SiteEntity - Saved site: " + name_);
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("SiteEntity::saveToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool SiteEntity::deleteFromDatabase() {
    if (getId() <= 0) {
        if (logger_) {
            logger_->Error("SiteEntity::deleteFromDatabase - Invalid site ID");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getSiteRepository();
        if (repo) {
            bool success = repo->deleteById(getId());
            if (success) {
                markDeleted();
                if (logger_) {
                    logger_->Info("SiteEntity - Deleted site: " + name_);
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("SiteEntity::deleteFromDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

bool SiteEntity::updateToDatabase() {
    if (getId() <= 0 || !isValid()) {
        if (logger_) {
            logger_->Error("SiteEntity::updateToDatabase - Invalid site data or ID");
        }
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getSiteRepository();
        if (repo) {
            bool success = repo->update(*this);
            if (success) {
                markSaved();
                if (logger_) {
                    logger_->Info("SiteEntity - Updated site: " + name_);
                }
            }
            return success;
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("SiteEntity::updateToDatabase failed: " + std::string(e.what()));
        }
        markError();
        return false;
    }
}

// =============================================================================
// 비즈니스 로직 메서드들 (Repository 위임)
// =============================================================================

bool SiteEntity::hasChildSites() {
    if (getId() <= 0) {
        return false;
    }
    
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getSiteRepository();
        if (repo) {
            return repo->hasChildSites(getId());
        }
        return false;
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("SiteEntity::hasChildSites failed: " + std::string(e.what()));
        }
        return false;
    }
}

void SiteEntity::updateHierarchyPath() {
    try {
        auto& factory = RepositoryFactory::getInstance();
        auto repo = factory.getSiteRepository();
        if (repo) {
            repo->updateHierarchyPath(*this);
            markModified();
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("SiteEntity::updateHierarchyPath failed: " + std::string(e.what()));
        }
    }
}

json SiteEntity::extractConfiguration() const {
    json config = {
        {"basic", {
            {"name", name_},
            {"code", code_},
            {"type", siteTypeToString(site_type_)},
            {"description", description_}
        }},
        {"hierarchy", {
            {"level", hierarchy_level_},
            {"path", hierarchy_path_},
            {"parent_id", parent_site_id_},
            {"is_root", isRootSite()}
        }},
        {"location", {
            {"address", address_},
            {"city", city_},
            {"country", country_},
            {"timezone", timezone_}
        }},
        {"contact", {
            {"name", contact_name_},
            {"email", contact_email_},
            {"phone", contact_phone_}
        }}
    };
    
    return config;
}

json SiteEntity::getLocationInfo() const {
    json location;
    location["site_id"] = getId();
    location["address"] = address_;
    location["city"] = city_;
    location["country"] = country_;
    location["timezone"] = timezone_;
    location["has_gps"] = hasGpsCoordinates();
    
    if (hasGpsCoordinates()) {
        location["gps"] = {
            {"latitude", latitude_},
            {"longitude", longitude_}
        };
    }
    
    return location;
}

json SiteEntity::getHierarchyInfo() const {
    json hierarchy;
    hierarchy["site_id"] = getId();
    hierarchy["level"] = hierarchy_level_;
    hierarchy["path"] = hierarchy_path_;
    hierarchy["parent_id"] = parent_site_id_;
    hierarchy["is_root"] = isRootSite();
    // hasChildSites()는 DB 조회가 필요하므로 const 메서드에서 제외
    hierarchy["note"] = "Use hasChildSites() method to check for children";
    
    return hierarchy;
}

// =============================================================================
// 정적 유틸리티 메서드들
// =============================================================================

std::string SiteEntity::siteTypeToString(SiteType type) {
    switch (type) {
        case SiteType::COMPANY: return "COMPANY";
        case SiteType::FACTORY: return "FACTORY";
        case SiteType::OFFICE: return "OFFICE";
        case SiteType::BUILDING: return "BUILDING";
        case SiteType::FLOOR: return "FLOOR";
        case SiteType::LINE: return "LINE";
        case SiteType::AREA: return "AREA";
        case SiteType::ZONE: return "ZONE";
        default: return "FACTORY";
    }
}

SiteEntity::SiteType SiteEntity::stringToSiteType(const std::string& type_str) {
    if (type_str == "COMPANY") return SiteType::COMPANY;
    if (type_str == "FACTORY") return SiteType::FACTORY;
    if (type_str == "OFFICE") return SiteType::OFFICE;
    if (type_str == "BUILDING") return SiteType::BUILDING;
    if (type_str == "FLOOR") return SiteType::FLOOR;
    if (type_str == "LINE") return SiteType::LINE;
    if (type_str == "AREA") return SiteType::AREA;
    if (type_str == "ZONE") return SiteType::ZONE;
    return SiteType::FACTORY;  // 기본값
}

} // namespace Entities
} // namespace Database
} // namespace PulseOne