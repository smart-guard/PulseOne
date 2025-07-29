// =============================================================================
// collector/src/Database/Entities/SiteEntity.cpp
// PulseOne 사이트 엔티티 구현 - TenantEntity/UserEntity 패턴 100% 준수
// =============================================================================

#include "Database/Entities/SiteEntity.h"
#include "Common/Utils.h"
#include "Common/Constants.h"
#include <sstream>
#include <iomanip>
#include <algorithm>


namespace PulseOne {
namespace Database {
namespace Entities {

// =============================================================================
// 생성자 및 소멸자 (TenantEntity 패턴)
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
    , contact_phone_("") {
}

SiteEntity::SiteEntity(int site_id) 
    : BaseEntity<SiteEntity>(site_id)
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
    , contact_phone_("") {
}

// =============================================================================
// BaseEntity 순수 가상 함수 구현 (TenantEntity 패턴)
// =============================================================================

bool SiteEntity::loadFromDatabase() {
    if (id_ <= 0) {
        logger_->Error("SiteEntity::loadFromDatabase - Invalid site ID: " + std::to_string(id_));
        markError();
        return false;
    }
    
    try {
        std::string query = "SELECT * FROM " + getTableName() + " WHERE id = " + std::to_string(id_);
        
        // 🔥 TenantEntity와 동일한 방식으로 executeUnifiedQuery 사용
        auto results = executeUnifiedQuery(query);
        
        if (results.empty()) {
            logger_->Warn("SiteEntity::loadFromDatabase - Site not found: " + std::to_string(id_));
            return false;
        }
        
        // 첫 번째 행을 엔티티로 변환
        bool success = mapRowToEntity(results[0]);
        
        if (success) {
            markSaved();  // TenantEntity 패턴
            logger_->Info("SiteEntity::loadFromDatabase - Loaded site: " + name_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteEntity::loadFromDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool SiteEntity::saveToDatabase() {
    if (!isValid()) {
        logger_->Error("SiteEntity::saveToDatabase - Invalid site data");
        return false;
    }
    
    try {
        std::string sql = buildInsertSQL();  // TenantEntity 패턴
        
        bool success = executeUnifiedNonQuery(sql);
        
        if (success) {
            // SQLite인 경우 마지막 INSERT ID 조회
            std::string db_type = config_manager_->getOrDefault("DATABASE_TYPE", "SQLITE");
            if (db_type == "SQLITE") {
                auto results = executeUnifiedQuery("SELECT last_insert_rowid() as id");
                if (!results.empty()) {
                    id_ = std::stoi(results[0]["id"]);
                }
            }
            
            markSaved();  // TenantEntity 패턴
            logger_->Info("SiteEntity::saveToDatabase - Saved site: " + name_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteEntity::saveToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool SiteEntity::updateToDatabase() {
    if (id_ <= 0 || !isValid()) {
        logger_->Error("SiteEntity::updateToDatabase - Invalid site data or ID");
        return false;
    }
    
    try {
        std::string sql = buildUpdateSQL();  // TenantEntity 패턴
        
        bool success = executeUnifiedNonQuery(sql);
        
        if (success) {
            markSaved();  // TenantEntity 패턴
            logger_->Info("SiteEntity::updateToDatabase - Updated site: " + name_);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteEntity::updateToDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool SiteEntity::deleteFromDatabase() {
    if (id_ <= 0) {
        logger_->Error("SiteEntity::deleteFromDatabase - Invalid site ID");
        return false;
    }
    
    try {
        std::string query = "DELETE FROM " + getTableName() + " WHERE id = " + std::to_string(id_);
        
        bool success = executeUnifiedNonQuery(query);
        
        if (success) {
            markDeleted();  // TenantEntity 패턴
            logger_->Info("SiteEntity::deleteFromDatabase - Deleted site ID: " + std::to_string(id_));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteEntity::deleteFromDatabase failed: " + std::string(e.what()));
        markError();
        return false;
    }
}

bool SiteEntity::isValid() const {
    // 기본 유효성 검사 (BaseEntity)
    if (!BaseEntity<SiteEntity>::isValid()) {
        return false;
    }
    
    // 사이트 전용 유효성 검사
    if (tenant_id_ <= 0) {
        return false;
    }
    
    if (name_.empty() || name_.length() > 100) {
        return false;
    }
    
    if (code_.empty() || code_.length() > 20) {
        return false;
    }
    
    // GPS 좌표 유효성 검사 (설정된 경우만)
    if (hasGpsCoordinates()) {
        if (latitude_ < -90.0 || latitude_ > 90.0) {
            return false;
        }
        if (longitude_ < -180.0 || longitude_ > 180.0) {
            return false;
        }
    }
    
    return true;
}

// =============================================================================
// JSON 직렬화/역직렬화 (TenantEntity 패턴)
// =============================================================================

json SiteEntity::toJson() const {
    json j;
    j["id"] = id_;
    j["tenant_id"] = tenant_id_;
    j["parent_site_id"] = parent_site_id_;
    j["name"] = name_;
    j["code"] = code_;
    j["site_type"] = siteTypeToString(site_type_);
    j["description"] = description_;
    j["location"] = location_;
    j["timezone"] = timezone_;
    j["address"] = address_;
    j["city"] = city_;
    j["country"] = country_;
    j["latitude"] = latitude_;
    j["longitude"] = longitude_;
    j["hierarchy_level"] = hierarchy_level_;
    j["hierarchy_path"] = hierarchy_path_;
    j["is_active"] = is_active_;
    j["contact_name"] = contact_name_;
    j["contact_email"] = contact_email_;
    j["contact_phone"] = contact_phone_;
    j["created_at"] = PulseOne::Utils::TimestampToISOString(created_at_);
    j["updated_at"] = PulseOne::Utils::TimestampToISOString(updated_at_);
    
    return j;
}

bool SiteEntity::fromJson(const json& data) {
    try {
        if (data.contains("id")) id_ = data["id"];
        if (data.contains("tenant_id")) tenant_id_ = data["tenant_id"];
        if (data.contains("parent_site_id")) parent_site_id_ = data["parent_site_id"];
        if (data.contains("name")) name_ = data["name"];
        if (data.contains("code")) code_ = data["code"];
        if (data.contains("site_type")) site_type_ = stringToSiteType(data["site_type"]);
        if (data.contains("description")) description_ = data["description"];
        if (data.contains("location")) location_ = data["location"];
        if (data.contains("timezone")) timezone_ = data["timezone"];
        if (data.contains("address")) address_ = data["address"];
        if (data.contains("city")) city_ = data["city"];
        if (data.contains("country")) country_ = data["country"];
        if (data.contains("latitude")) latitude_ = data["latitude"];
        if (data.contains("longitude")) longitude_ = data["longitude"];
        if (data.contains("hierarchy_level")) hierarchy_level_ = data["hierarchy_level"];
        if (data.contains("hierarchy_path")) hierarchy_path_ = data["hierarchy_path"];
        if (data.contains("is_active")) is_active_ = data["is_active"];
        if (data.contains("contact_name")) contact_name_ = data["contact_name"];
        if (data.contains("contact_email")) contact_email_ = data["contact_email"];
        if (data.contains("contact_phone")) contact_phone_ = data["contact_phone"];
        
        markModified();
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteEntity::fromJson failed: " + std::string(e.what()));
        return false;
    }
}

std::string SiteEntity::toString() const {
    std::ostringstream oss;
    oss << "SiteEntity[id=" << id_ 
        << ", tenant_id=" << tenant_id_
        << ", name=" << name_ 
        << ", code=" << code_
        << ", type=" << siteTypeToString(site_type_)
        << ", location=" << location_
        << ", active=" << (is_active_ ? "true" : "false") << "]";
    return oss.str();
}

// =============================================================================
// 비즈니스 로직 메서드들 (TenantEntity 패턴)
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

void SiteEntity::updateHierarchyPath() {
    if (parent_site_id_ <= 0) {
        // 루트 사이트
        hierarchy_path_ = "/" + std::to_string(id_);
        hierarchy_level_ = 1;
    } else {
        // 하위 사이트 - 상위 사이트 정보 필요
        // 실제 구현에서는 상위 사이트를 조회해서 경로를 구성
        hierarchy_path_ = "/" + std::to_string(parent_site_id_) + "/" + std::to_string(id_);
        hierarchy_level_ = 2;  // 단순화된 구현
    }
    markModified();
}

bool SiteEntity::hasChildSites() {
    if (id_ <= 0) {
        return false;
    }
    
    try {
        std::string query = "SELECT COUNT(*) as count FROM " + getTableName() + 
                           " WHERE parent_site_id = " + std::to_string(id_);
        
        auto results = executeUnifiedQuery(query);
        
        if (!results.empty()) {
            int count = std::stoi(results[0]["count"]);
            return count > 0;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteEntity::hasChildSites failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 고급 기능 (TenantEntity 패턴)
// =============================================================================

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
    location["site_id"] = id_;
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
    hierarchy["site_id"] = id_;
    hierarchy["level"] = hierarchy_level_;
    hierarchy["path"] = hierarchy_path_;
    hierarchy["parent_id"] = parent_site_id_;
    hierarchy["is_root"] = isRootSite();
    // DB 조회가 필요한 hasChildSites() 제거 - const 메서드에서는 DB 조회 안함 (TenantEntity 패턴)
    hierarchy["note"] = "Use hasChildSites() method to check for children";
    
    return hierarchy;
}

// =============================================================================
// private 헬퍼 메서드들 (TenantEntity 패턴)
// =============================================================================

bool SiteEntity::mapRowToEntity(const std::map<std::string, std::string>& row) {
    try {
        // 필수 필드들
        id_ = std::stoi(row.at("id"));
        tenant_id_ = std::stoi(row.at("tenant_id"));
        name_ = row.at("name");
        code_ = row.at("code");
        site_type_ = stringToSiteType(row.at("site_type"));
        
        // 선택적 필드들 (기본값 처리)
        auto it = row.find("parent_site_id");
        parent_site_id_ = (it != row.end() && !it->second.empty()) ? std::stoi(it->second) : 0;
        
        it = row.find("description");
        description_ = (it != row.end()) ? it->second : "";
        
        it = row.find("location");
        location_ = (it != row.end()) ? it->second : "";
        
        it = row.find("timezone");
        timezone_ = (it != row.end()) ? it->second : "UTC";
        
        it = row.find("address");
        address_ = (it != row.end()) ? it->second : "";
        
        it = row.find("city");
        city_ = (it != row.end()) ? it->second : "";
        
        it = row.find("country");
        country_ = (it != row.end()) ? it->second : "";
        
        it = row.find("latitude");
        latitude_ = (it != row.end() && !it->second.empty()) ? std::stod(it->second) : 0.0;
        
        it = row.find("longitude");
        longitude_ = (it != row.end() && !it->second.empty()) ? std::stod(it->second) : 0.0;
        
        it = row.find("hierarchy_level");
        hierarchy_level_ = (it != row.end() && !it->second.empty()) ? std::stoi(it->second) : 1;
        
        it = row.find("hierarchy_path");
        hierarchy_path_ = (it != row.end()) ? it->second : "";
        
        it = row.find("is_active");
        is_active_ = (it != row.end()) ? (it->second == "1" || it->second == "true") : true;
        
        it = row.find("contact_name");
        contact_name_ = (it != row.end()) ? it->second : "";
        
        it = row.find("contact_email");
        contact_email_ = (it != row.end()) ? it->second : "";
        
        it = row.find("contact_phone");
        contact_phone_ = (it != row.end()) ? it->second : "";
        
        // 타임스탬프 처리
        it = row.find("created_at");
        if (it != row.end() && !it->second.empty()) {
            created_at_ = PulseOne::Utils::ParseTimestampFromString(it->second);
        }
        
        it = row.find("updated_at");
        if (it != row.end() && !it->second.empty()) {
            updated_at_ = PulseOne::Utils::ParseTimestampFromString(it->second);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("SiteEntity::mapRowToEntity failed: " + std::string(e.what()));
        return false;
    }
}

std::string SiteEntity::buildInsertSQL() const {
    std::ostringstream sql;
    sql << "INSERT INTO " << getTableName() << " ("
        << "tenant_id, parent_site_id, name, code, site_type, description, "
        << "location, timezone, address, city, country, latitude, longitude, "
        << "hierarchy_level, hierarchy_path, is_active, contact_name, "
        << "contact_email, contact_phone, created_at, updated_at"
        << ") VALUES ("
        << tenant_id_ << ", "
        << (parent_site_id_ > 0 ? std::to_string(parent_site_id_) : "NULL") << ", "
        << "'" << name_ << "', "
        << "'" << code_ << "', "
        << "'" << siteTypeToString(site_type_) << "', "
        << "'" << description_ << "', "
        << "'" << location_ << "', "
        << "'" << timezone_ << "', "
        << "'" << address_ << "', "
        << "'" << city_ << "', "
        << "'" << country_ << "', "
        << latitude_ << ", "
        << longitude_ << ", "
        << hierarchy_level_ << ", "
        << "'" << hierarchy_path_ << "', "
        << (is_active_ ? 1 : 0) << ", "
        << "'" << contact_name_ << "', "
        << "'" << contact_email_ << "', "
        << "'" << contact_phone_ << "', "
        << "'" << PulseOne::Utils::TimestampToDBString(created_at_) << "', "
        << "'" << PulseOne::Utils::TimestampToDBString(updated_at_) << "'"
        << ")";
        
    return sql.str();
}

std::string SiteEntity::buildUpdateSQL() const {
    std::ostringstream sql;
    sql << "UPDATE " << getTableName() << " SET "
        << "tenant_id = " << tenant_id_ << ", "
        << "parent_site_id = " << (parent_site_id_ > 0 ? std::to_string(parent_site_id_) : "NULL") << ", "
        << "name = '" << name_ << "', "
        << "code = '" << code_ << "', "
        << "site_type = '" << siteTypeToString(site_type_) << "', "
        << "description = '" << description_ << "', "
        << "location = '" << location_ << "', "
        << "timezone = '" << timezone_ << "', "
        << "address = '" << address_ << "', "
        << "city = '" << city_ << "', "
        << "country = '" << country_ << "', "
        << "latitude = " << latitude_ << ", "
        << "longitude = " << longitude_ << ", "
        << "hierarchy_level = " << hierarchy_level_ << ", "
        << "hierarchy_path = '" << hierarchy_path_ << "', "
        << "is_active = " << (is_active_ ? 1 : 0) << ", "
        << "contact_name = '" << contact_name_ << "', "
        << "contact_email = '" << contact_email_ << "', "
        << "contact_phone = '" << contact_phone_ << "', "
        << "updated_at = '" << PulseOne::Utils::TimestampToDBString(updated_at_) << "' "
        << "WHERE id = " << id_;
        
    return sql.str();
}



} // namespace Entities
} // namespace Database
} // namespace PulseOne